"""CLI for the LINBLUX automatic checkpoint backup."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from .config import BackupConfig, load_config
from .runner import BackupRunner, Logger, default_state_dir, status_snapshot
from .selection import sha256_file
from .store import Store, StoreError


def _add_common(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--project", default=os.environ.get("LINBLUX_BACKUP_PROJECT", os.getcwd()),
                        help="raiz do projeto monitorado")
    parser.add_argument("--config", help="arquivo de configuração alternativo")
    parser.add_argument("--state-dir", help="diretório de estado isolado (fora do projeto)")
    parser.add_argument("--json", action="store_true", help="saída estruturada")
    parser.add_argument("--quiet", action="store_true", help="sem mensagens de log")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="backup",
        description="Checkpoints automáticos do LINBLUX em um branch separado (auto-backup).",
    )
    subcommands = parser.add_subparsers(dest="command", required=True)

    run = subcommands.add_parser("run", help="monitora o projeto e cria checkpoints")
    _add_common(run)
    run.add_argument("--once", action="store_true", help="executa um único ciclo")
    run.add_argument("--stop-after", type=float, help="encerra após N segundos (teste do daemon)")

    status = subcommands.add_parser("status", help="último checkpoint, último push e pendências")
    _add_common(status)

    init = subcommands.add_parser("init", help="prepara o repositório isolado, sem enviar")
    _add_common(init)

    verify = subcommands.add_parser("verify", help="clona o branch remoto e valida a restauração")
    _add_common(verify)
    service = subcommands.add_parser("service", help="instala/inicia serviço do usuário (systemd ou cron)")
    service.add_argument("action", choices=("install", "start"))
    service.add_argument("--backend", choices=("auto", "systemd", "cron"), default="auto")
    _add_common(service)
    return parser


def _runner(args) -> BackupRunner:
    project = Path(args.project).expanduser().resolve()
    if not project.is_dir():
        raise SystemExit(f"projeto não encontrado: {project}")
    config = load_config(Path(args.config).expanduser() if args.config else None)
    state_dir = Path(args.state_dir).expanduser() if args.state_dir else default_state_dir()
    return BackupRunner(project, config, state_dir, logger=Logger(quiet=args.quiet))


def _print_status(snapshot: dict) -> None:
    last_checkpoint = snapshot["last_checkpoint"] or {}
    last_push = snapshot["last_push"] or {}
    print(f"LINBLUX backup — branch {snapshot['branch']}")
    print(f"Projeto:                {snapshot['project']}")
    print(f"Remoto:                 {snapshot['remote']}")
    print(f"Monitor:                {'ativo' if snapshot['running'] else 'parado'}")
    if last_checkpoint:
        print(
            f"Último checkpoint local: {last_checkpoint['hash'][:12]} em {last_checkpoint['time']} "
            f"({last_checkpoint.get('files', 0)} arquivos)"
        )
    else:
        print("Último checkpoint local: nenhum")
    if last_push:
        print(f"Último push confirmado:  {last_push['hash'][:12]} em {last_push['time']}")
    else:
        print("Último push confirmado:  nenhum")
    print(f"Pendente de envio:       {'sim' if snapshot['pending_push'] else 'não'}")
    print(f"Arquivos locais cobertos: {snapshot['protected_files']}")
    print(f"Arquivos remotos cobertos: {snapshot['remote_protected_files']}")
    print("Escopo: allowlist; ignorados/dados privados não estão incluídos.")
    changes = snapshot.get("changes") or {}
    unprotected = snapshot["unprotected"]
    print(f"Alterações não protegidas: {len(unprotected)}")
    for relative in unprotected[:25]:
        marker = "A" if relative in set(changes.get("added", [])) else (
            "D" if relative in set(changes.get("removed", [])) else "M"
        )
        print(f"  {marker} {relative}")
    if len(unprotected) > 25:
        print(f"  … (+{len(unprotected) - 25})")
    if snapshot["skipped"]:
        print(f"Ignorados por política:  {len(snapshot['skipped'])}")
        for item in snapshot["skipped"][:10]:
            print(f"  {item['path']} ({item['reason']})")
    print(f"Intervalo de checkpoint: {snapshot['interval_seconds']:.0f}s")
    if snapshot["last_error"]:
        print(f"Último problema:         {snapshot['last_error']}")


def _command_status(runner: BackupRunner, as_json: bool) -> int:
    snapshot = status_snapshot(runner)
    if as_json:
        print(json.dumps(snapshot, indent=2, sort_keys=True))
    else:
        _print_status(snapshot)
    return 0


def _command_run(runner: BackupRunner, args) -> int:
    if args.once:
        if not runner.acquire_lock():
            print("outro daemon de backup já está ativo; ciclo não executado", file=sys.stderr)
            return 1
        try:
            result = runner.run_once(force=False)
        finally:
            runner._lock_handle.close()
            runner._lock_handle = None
        if args.json:
            print(json.dumps(result.as_dict(), indent=2, sort_keys=True))
        else:
            print(f"Checkpoint: {result.checkpoint or 'nenhum'}")
            print(f"Push:       {result.pushed or ('pendente' if result.pending else 'sincronizado')}")
            print(f"Pendências: {len(result.changed)}")
            if result.error:
                print(f"Problema:   {result.error}")
        return 0 if result.error is None else 1
    return runner.run_forever(stop_after=args.stop_after)


def _command_init(runner: BackupRunner, as_json: bool) -> int:
    if not runner.acquire_lock():
        print("outro daemon de backup já está ativo", file=sys.stderr)
        return 1
    try:
        runner.store.ensure()
        info = {
            "state_dir": str(runner.state_dir),
            "work_repo": str(runner.store.work),
            "branch": runner.config.branch,
            "remote": runner.remote_url,
            "head": runner.store.head(),
        }
    finally:
        if runner._lock_handle is not None:
            runner._lock_handle.close()
    print(json.dumps(info, indent=2, sort_keys=True) if as_json else "\n".join(f"{k}: {v}" for k, v in info.items()))
    return 0


def _command_verify(runner: BackupRunner, config: BackupConfig, as_json: bool) -> int:
    selection = runner.collect()
    report: dict = {"checkpoint": None, "checked": 0, "missing": [], "mismatched": [], "ok": False}
    with tempfile.TemporaryDirectory(prefix="linblux-backup-verify-") as temporary:
        clone = Path(temporary) / "clone"
        cloned = subprocess.run(
            ["git", "clone", "--quiet", "--depth", "1", "--branch", config.branch,
             runner.remote_url, str(clone)],
            capture_output=True, timeout=180, check=False,
        )
        if cloned.returncode != 0:
            report["error"] = cloned.stderr.decode(errors="replace").strip().splitlines()[-1:] or ["clone falhou"]
            if as_json:
                print(json.dumps(report, indent=2, sort_keys=True))
            else:
                print(f"Falha ao clonar {config.branch}: {report['error']}")
            return 1
        head = subprocess.run(["git", "-C", str(clone), "rev-parse", "HEAD"],
                              capture_output=True, text=True, check=False)
        report["checkpoint"] = head.stdout.strip() or None
        for item in selection.files:
            restored = clone / item.relative
            if not restored.is_file():
                report["missing"].append(item.relative)
                continue
            if sha256_file(restored) != item.digest:
                report["mismatched"].append(item.relative)
        report["checked"] = len(selection.files)
        report["ok"] = not report["missing"] and not report["mismatched"]
    if as_json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(f"Checkpoint remoto {report['checkpoint'] or '?'} restaurado por clonagem limpa")
        print(f"Arquivos verificados: {report['checked']}")
        print(f"Restauração íntegra:  {'sim' if report['ok'] else 'não'}")
        for relative in report["missing"][:20]:
            print(f"  ausente: {relative}")
        for relative in report["mismatched"][:20]:
            print(f"  divergente: {relative}")
    return 0 if report["ok"] else 1


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        runner = _runner(args)
    except StoreError as failure:
        print(f"erro: {failure}", file=sys.stderr)
        return 1
    except SystemExit as failure:
        print(f"erro: {failure}", file=sys.stderr)
        return 1

    if args.command == "status":
        return _command_status(runner, args.json)
    if args.command == "init":
        return _command_init(runner, args.json)
    if args.command == "verify":
        return _command_verify(runner, runner.config, args.json)
    if args.command == "service":
        from . import service
        try:
            result = service.install(runner, args.backend, args.config) if args.action == "install" else service.start(runner, args.config)
        except (StoreError, OSError, subprocess.TimeoutExpired) as failure:
            print(f"Serviço não confirmado: {failure}", file=sys.stderr)
            return 1
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 0
    return _command_run(runner, args)
