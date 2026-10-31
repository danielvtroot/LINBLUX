"""Command-line orchestration for LINBLUX ATLAS TURBO."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path

from . import (
    benchmark as benchmark_mod,
    cache,
    changes as changes_mod,
    config,
    docscheck,
    inventory as inventory_mod,
    organize as organize_mod,
    report,
    runner,
    selection,
)

GLOBAL_FLAGS = {"--json", "--quiet", "--no-atlas-update"}
GLOBAL_VALUES = {"--root", "--build-dir", "--jobs"}


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def find_root(explicit: str | None = None) -> Path:
    if explicit:
        return config.repo_root(explicit)
    current = Path.cwd().resolve()
    for candidate in (current, *current.parents):
        if (candidate / "CMakeLists.txt").is_file() and (candidate / "tools" / "atlas" / "atlas.py").is_file():
            return candidate
    return config.repo_root()


def normalize_argv(argv: list[str]) -> list[str]:
    """Allow global flags after the subcommand without changing their meaning."""
    front: list[str] = []
    rest: list[str] = []
    index = 0
    while index < len(argv):
        arg = argv[index]
        if arg in GLOBAL_FLAGS:
            front.append(arg)
            index += 1
        elif arg in GLOBAL_VALUES and index + 1 < len(argv):
            front.extend([arg, argv[index + 1]])
            index += 2
        elif any(arg.startswith(value + "=") for value in GLOBAL_VALUES):
            front.append(arg)
            index += 1
        else:
            rest.append(arg)
            index += 1
    return front + rest


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="turbo.py",
        description="LINBLUX ATLAS TURBO: compilação incremental e validação explicável.",
    )
    parser.add_argument("--root", help="raiz do repositório (padrão: detectar a partir do cwd)")
    parser.add_argument("--build-dir", help="diretório de build (padrão: build-linblux)")
    parser.add_argument("--json", action="store_true", help="emitir envelope JSON")
    parser.add_argument("--quiet", action="store_true", help="silenciar texto resumido")
    parser.add_argument("--jobs", type=int, help="paralelismo seguro (padrão: derivado de CPU/memória)")
    parser.add_argument("--no-atlas-update", action="store_true", help="não atualizar o índice ATLAS")
    parser.add_argument("--version", action="version", version=f"ATLAS TURBO {config.TURBO_VERSION}")
    sub = parser.add_subparsers(dest="command", required=True)

    p_doctor = sub.add_parser("doctor", help="verificar ambiente, build, CTest e ATLAS")

    p_inventory = sub.add_parser("inventory", help="listar testes reais, labels, custos e targets")

    p_build = sub.add_parser("build", help="compilar targets relevantes")
    p_build.add_argument("--target", default="auto", help="auto, all ou lista separada por vírgulas")
    p_build.add_argument("--configure", action="store_true", help="reexecutar cmake -S -B antes do build")

    p_test = sub.add_parser("test", help="selecionar, compilar e executar testes")
    p_test.add_argument("--mode", choices=("fast", "impact", "full"), default="fast")
    p_test.add_argument("--changed", action="store_true", help="usar as alterações atuais (padrão no FAST)")
    p_test.add_argument("--plan", action="store_true", help="mostrar o plano sem compilar nem testar")
    p_test.add_argument("--no-build", action="store_true", help="não compilar; executar apenas os testes selecionados")
    p_test.add_argument("--no-cache", action="store_true", help="ignorar histórico reutilizável")
    p_test.add_argument(
        "--budget",
        "--timeout",
        dest="budget",
        type=_positive_seconds,
        help="orçamento global dos testes em segundos (padrão por modo; --timeout é alias)",
    )
    p_test.add_argument(
        "--per-test-timeout",
        type=_positive_seconds,
        help="limite individual de cada teste em segundos (padrão: gráficos 180s, headless 90s)",
    )
    p_test.add_argument("--max-graphics", type=int, default=None, help="limite de testes gráficos no IMPACT")

    p_report = sub.add_parser("report", help="mostrar o último relatório compacto")
    p_report.add_argument("--mode", choices=("fast", "impact", "full"), help="relatório do modo")
    p_report.add_argument("--detail", action="store_true", help="incluir o plano completo")

    p_status = sub.add_parser("status", help="estado consultável da execução atual/última")

    p_bench = sub.add_parser("benchmark", help="medir build/testes reais sem inventar percentuais")
    p_bench.add_argument("--run-fast", action="store_true", help="executar o plano FAST e medir tempos reais")

    p_org = sub.add_parser("organize", help="auditar estrutura e duplicatas sem mover arquivos")
    p_org.add_argument("--dry-run", action="store_true", default=True, help="somente auditoria (padrão)")
    p_org.add_argument("--apply", action="store_true", help="desabilitado: exige revisão humana")
    return parser


def _emit(args, command: str, data: dict, ok: bool, message: str | None = None, exit_code: int = 0) -> int:
    if args.json:
        print(
            json.dumps(
                {
                    "ok": ok,
                    "command": command,
                    "message": message,
                    "data": data,
                },
                ensure_ascii=False,
                indent=2,
                sort_keys=True,
            )
        )
    elif not args.quiet:
        print(message if message else json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True))
    return exit_code


def _paths(args) -> tuple[Path, Path]:
    root = find_root(args.root)
    build = Path(args.build_dir).expanduser() if args.build_dir else config.default_build_dir(root)
    if not build.is_absolute():
        build = (root / build).resolve()
    return root, build


def _jobs(args) -> int:
    return config.safe_jobs(args.jobs)


def _positive_seconds(value: str) -> float:
    try:
        seconds = float(value)
    except (TypeError, ValueError) as exc:
        raise argparse.ArgumentTypeError("use um número de segundos") from exc
    if seconds <= 0:
        raise argparse.ArgumentTypeError("o tempo deve ser maior que zero")
    return seconds


def _test_budget(args, mode: str) -> float:
    if args.budget is not None:
        return float(args.budget)
    return float(config.MODE_TEST_BUDGETS.get(mode, 300))


def _per_test_caps(args) -> dict[str, float]:
    if args.per_test_timeout is not None:
        cap = float(args.per_test_timeout)
        return {"graphics": cap, "headless": cap}
    return {
        "graphics": float(config.GRAPHICS_TEST_TIMEOUT),
        "headless": float(config.HEADLESS_TEST_TIMEOUT),
    }


def _apply_test_budget(plan: dict, args, mode: str) -> tuple[float, dict[str, float]]:
    budget = _test_budget(args, mode)
    caps = _per_test_caps(args)
    plan["budget_seconds"] = budget
    plan["per_test_timeout_seconds"] = caps
    estimated = float(plan.get("estimated_seconds") or 0)
    if estimated > budget:
        plan.setdefault("warnings", []).append(
            f"orçamento de {budget:.0f}s é menor que a estimativa histórica de {estimated:.0f}s; "
            "testes adicionais podem ficar pendentes"
        )
    return budget, caps


def _inventory(args, root: Path, build: Path) -> dict:
    return inventory_mod.load_inventory(
        root,
        build,
        ensure=not args.no_atlas_update,
    )


def _configure_if_absent(root: Path, build: Path, log_dir: Path, timeout: float = 600) -> dict | None:
    if (build / "CMakeCache.txt").is_file():
        return None
    log_dir.mkdir(parents=True, exist_ok=True)
    return runner.run_command(
        ["cmake", "-S", str(root), "-B", str(build)],
        cwd=root,
        log_path=log_dir / "configure.log",
        timeout=timeout,
        name="cmake-configure",
    )


def _exit_for_status(status: str) -> int:
    if status in {"PASSED", "PASSED_NOW", "REUSED", "DOCS_OK", "ASSET_OK", "SKIPPED"}:
        return 0
    if status == "TIMEOUT":
        return 124
    if status in {"PASSED_PARTIAL", "BUDGET_EXHAUSTED", "UNKNOWN", "NO_TESTS", "NOT_RUN"}:
        return 2
    if status == "BUSY":
        return 3
    return 1


def _base_result(mode: str, root: Path, build: Path, run_id: str, changes: dict, plan: dict) -> dict:
    return {
        "schema_version": config.TURBO_SCHEMA_VERSION,
        "tool": "linblux-atlas-turbo",
        "version": config.TURBO_VERSION,
        "run_id": run_id,
        "mode": mode,
        "status": "RUNNING",
        "started_at": _utc_now(),
        "root": str(root),
        "build_dir": str(build),
        "changed_count": changes.get("count", 0),
        "changed": [item["path"] for item in changes.get("files", [])],
        "changed_files": changes.get("files", []),
        "plan": plan,
        "coverage": plan.get("coverage"),
        "selected_names": plan.get("selected_names", []),
        "fingerprint": None,
        "reused": False,
        "timed_out": False,
    }


def _test_outcome(test: dict) -> str:
    if not test:
        return "NOT_RUN"
    counts = test.get("counts") or {}
    if counts.get("failed"):
        return "FAILED"
    if test.get("status") == "BUDGET_EXHAUSTED":
        return "BUDGET_EXHAUSTED"
    if test.get("status") == "TIMEOUT" or counts.get("timeout"):
        return "TIMEOUT"
    if test.get("budget_exhausted"):
        return "BUDGET_EXHAUSTED"
    if counts.get("not_run") or test.get("missing"):
        return "UNKNOWN"
    if counts.get("executed", 0) < counts.get("selected", 0):
        return "UNKNOWN"
    if test.get("status") == "PASSED":
        return "PASSED"
    return test.get("status") or "UNKNOWN"


def _execute_plan(
    root: Path,
    build: Path,
    plan: dict,
    mode: str,
    *,
    jobs: int,
    budget_seconds: float,
    per_test_caps: dict[str, float],
    no_build: bool,
    run_id: str,
) -> tuple[dict, dict, Path]:
    log_dir = cache.logs_dir(root) / run_id
    log_dir.mkdir(parents=True, exist_ok=True)
    build_result = {
        "status": "SKIPPED",
        "reason": "compilação desabilitada com --no-build",
        "commands": [],
        "targets": plan.get("build_targets", []),
        "duration_seconds": 0,
        "log": None,
    }
    if not no_build:
        build_result = runner.build(
            root,
            build,
            plan.get("build_targets", []),
            jobs=jobs,
            log_dir=log_dir,
            timeout=config.BUILD_TIMEOUT,
            configure=bool(plan.get("configure_required")),
        )
        if build_result.get("status") != "PASSED":
            first = next(
                (item for item in build_result.get("commands", []) if item.get("status") != "PASSED"),
                None,
            )
            build_result["status"] = (
                "CONFIG_FAILED"
                if first and first.get("name") == "cmake-configure"
                else "BUILD_FAILED"
            )
            return build_result, {}, log_dir
    selected = plan.get("selected_names", [])
    if not selected:
        return build_result, {}, log_dir
    test_result = runner.run_supervised_tests(
        root,
        build,
        plan.get("selected_tests", []),
        log_dir=log_dir,
        budget_seconds=budget_seconds,
        graphics_timeout=per_test_caps["graphics"],
        headless_timeout=per_test_caps["headless"],
    )
    return build_result, test_result, log_dir


def _next_action(status: str, mode: str, coverage: str) -> str:
    if status == "PASSED":
        if coverage != "known":
            return "Cobertura desconhecida; execute o modo IMPACT ou FULL antes de concluir."
        if mode == "fast":
            return "FAST aprovado agora; execute IMPACT antes da entrega."
        if mode == "impact":
            return "IMPACT aprovado; FULL continua necessário para mudanças estruturais/release."
        return "Validação completa aprovada nesta execução."
    if status == "PASSED_PARTIAL":
        return "Execução parcial: amplie a validação (IMPACT/FULL) antes de concluir."
    if status == "BUDGET_EXHAUSTED":
        return (
            "Orçamento esgotado: os testes pendentes NÃO foram aprovados. Se forem necessários, "
            "repita apenas os pendentes com --budget/--per-test-timeout; caso contrário, registre a cobertura parcial."
        )
    if status == "REUSED":
        return "Resultado reutilizado; use --no-cache para exigir uma execução real."
    if status == "TIMEOUT":
        return "Há teste travado ou acima do limite; consulte o log, corrija a causa e repita apenas o teste afetado."
    if status in {"UNKNOWN", "NO_TESTS", "NOT_RUN"}:
        return "Nenhuma aprovação completa foi emitida; amplie a validação ou execute FULL."
    return "Corrija a primeira falha indicada e repita; o log completo está no caminho acima."


def _run_test(args) -> int:
    root, build = _paths(args)
    mode = args.mode
    if args.changed and mode == "fast":
        mode = "fast"
    run_id = cache.new_run_id()
    started = time.monotonic()
    changes = changes_mod.summarize(root)

    if not changes.get("files"):
        plan = {
            "mode": mode,
            "changed_count": 0,
            "changed": [],
            "docs": [],
            "docs_only": False,
            "changed_modules": [],
            "affected_targets": [],
            "build_targets": [],
            "selected_tests": [],
            "selected_names": [],
            "filtered": {},
            "coverage": "none",
            "coverage_unknown": True,
            "unknown_paths": [],
            "structural": False,
            "configure_required": False,
            "estimated_seconds": 0,
            "graphics_count": 0,
            "slow_count": 0,
            "warnings": [],
            "test_count": 0,
            "inventory_warnings": [],
        }
        result = _base_result(mode, root, build, run_id, changes, plan)
        result.update(
            {
                "status": "NOT_RUN",
                "message": "nenhuma alteração detectada; nada foi executado",
                "finished_at": _utc_now(),
                "duration_seconds": round(time.monotonic() - started, 3),
                "next_action": "Altere o código e repita o FAST.",
            }
        )
        cache.save_result(root, result, plan)
        text = report.format_result(result)
        return _emit(args, "test", result, True, text, 0)

    # Documentation-only changes do not need a C++ build or a graphical test.
    if changes.get("docs_only"):
        plan = {
            "mode": mode,
            "changed_count": changes["count"],
            "changed": [item["path"] for item in changes["files"]],
            "docs": changes.get("docs", []),
            "docs_only": True,
            "changed_modules": changes.get("modules", []),
            "affected_targets": [],
            "build_targets": [],
            "selected_tests": [],
            "selected_names": [],
            "filtered": {},
            "coverage": "docs",
            "coverage_unknown": False,
            "unknown_paths": [],
            "structural": False,
            "configure_required": False,
            "estimated_seconds": 0,
            "graphics_count": 0,
            "slow_count": 0,
            "warnings": [],
            "test_count": 0,
            "inventory_warnings": [],
        }
        result = _base_result(mode, root, build, run_id, changes, plan)
        docs = docscheck.check_changed(root, changes["files"])
        result["docs"] = docs
        result["build"] = {"status": "SKIPPED", "reason": "mudança somente de documentação/JSON"}
        result["test"] = {"status": "NOT_RUN", "reason": "documentação não exige recompilação"}
        status = "DOCS_OK" if docs["status"] == "DOCS_OK" else "FAILED"
        result["status"] = status
        result["finished_at"] = _utc_now()
        result["duration_seconds"] = round(time.monotonic() - started, 3)
        result["next_action"] = _next_action(status, mode, "docs")
        result["message"] = "validação de links/JSON concluída" if status == "DOCS_OK" else "há erros de documentação"
        cache.save_result(root, result, plan)
        return _emit(args, "test", result, status == "DOCS_OK", report.format_result(result), _exit_for_status(status))

    log_dir = cache.logs_dir(root) / run_id
    initial = _configure_if_absent(root, build, log_dir)
    if initial and initial["status"] != "PASSED":
        plan = {
            "mode": mode,
            "changed_count": changes["count"],
            "changed": [item["path"] for item in changes["files"]],
            "docs": changes.get("docs", []),
            "docs_only": False,
            "changed_modules": changes.get("modules", []),
            "affected_targets": [],
            "build_targets": [],
            "selected_tests": [],
            "selected_names": [],
            "filtered": {},
            "coverage": "unknown",
            "coverage_unknown": True,
            "unknown_paths": [],
            "structural": False,
            "configure_required": True,
            "estimated_seconds": 0,
            "graphics_count": 0,
            "slow_count": 0,
            "warnings": ["configuração CMake inicial falhou"],
            "test_count": 0,
            "inventory_warnings": [],
        }
        result = _base_result(mode, root, build, run_id, changes, plan)
        result.update(
            {
                "status": "CONFIG_FAILED",
                "build": {"status": "CONFIG_FAILED", "command": initial, "log": initial.get("log")},
                "test": {"status": "NOT_RUN"},
                "excerpt": initial.get("excerpt"),
                "message": "configuração CMake falhou antes do inventário",
                "finished_at": _utc_now(),
                "duration_seconds": round(time.monotonic() - started, 3),
                "next_action": "Corrija a configuração CMake e repita.",
            }
        )
        cache.save_result(root, result, plan)
        return _emit(args, "test", result, False, report.format_result(result), 1)

    inventory = _inventory(args, root, build)
    max_graphics = args.max_graphics
    if mode == "impact" and max_graphics is None:
        max_graphics = config.IMPACT_DEFAULT_GRAPHICS
    plan = selection.build_plan(
        inventory,
        changes,
        mode,
        root=root,
        max_graphics=max_graphics,
    )
    budget_seconds, per_test_caps = _apply_test_budget(plan, args, mode)
    if args.plan:
        text = report.format_plan(plan)
        return _emit(args, "test", {"plan": plan, "changes": changes}, True, text, 0)

    # Validate documentation and JSON resources even when C++ tests also run.
    # A malformed locale or asset must fail before an approval is emitted.
    asset_paths = [
        item for item in changes["files"]
        if item["path"].lower().endswith((".md", ".markdown", ".json"))
    ]
    asset_check = docscheck.check_changed(root, asset_paths) if asset_paths else None
    if asset_check and asset_check["status"] != "DOCS_OK":
        result = _base_result(mode, root, build, run_id, changes, plan)
        result.update(
            {
                "status": "FAILED",
                "assets": asset_check,
                "build": {"status": "SKIPPED", "reason": "validação de recurso falhou"},
                "test": {"status": "NOT_RUN", "reason": "validação de recurso falhou"},
                "message": "há erros em documentação/JSON; compilação não iniciada",
                "finished_at": _utc_now(),
                "duration_seconds": round(time.monotonic() - started, 3),
                "next_action": "Corrija os links ou o JSON e repita o FAST.",
            }
        )
        cache.save_result(root, result, plan)
        return _emit(args, "test", result, False, report.format_result(result), 1)
    plan["asset_validation"] = asset_check
    only_validated_files = all(
        item["path"].startswith(("assets/", "midia/", "docs/"))
        or item["path"].lower().endswith((".md", ".markdown", ".json"))
        for item in changes["files"]
    )
    if only_validated_files and plan.get("test_count", 0) == 0 and asset_check:
        result = _base_result(mode, root, build, run_id, changes, plan)
        result.update(
            {
                "status": "ASSET_OK",
                "assets": asset_check,
                "build": {"status": "SKIPPED", "reason": "recurso validado sem compilação"},
                "test": {"status": "NOT_RUN", "reason": "nenhum teste de engine associado ao recurso"},
                "message": "recurso/documentação válidos; nenhum teste de engine foi associado",
                "coverage": "assets",
                "finished_at": _utc_now(),
                "duration_seconds": round(time.monotonic() - started, 3),
                "next_action": "Se o recurso for usado pela UI, execute IMPACT para incluir testes gráficos.",
            }
        )
        cache.save_result(root, result, plan)
        return _emit(args, "test", result, True, report.format_result(result), 0)

    extra = {
        "mode": mode,
        "selected": plan["selected_names"],
        "coverage": plan["coverage"],
        "build": cache.build_signature(build),
        "tools": {key: value for key, value in config.tool_versions().items() if value},
    }
    fingerprint = changes_mod.fingerprint(root, extra)
    if not args.no_cache:
        last = cache.load_result(root, mode)
        reusable, reason = cache.can_reuse(
            last,
            mode=mode,
            fingerprint=fingerprint,
            selected=plan["selected_names"],
            plan=plan,
        )
        if reusable:
            result = cache.reused_result(last, fingerprint=fingerprint, run_id=run_id)
            result["plan"] = plan
            result["changed"] = [item["path"] for item in changes["files"]]
            result["changed_count"] = changes["count"]
            result["message"] = f"RESULTADO REUTILIZADO ({reason}); não executado agora"
            result["next_action"] = _next_action("REUSED", mode, plan["coverage"])
            cache.save_result(root, result, plan)
            return _emit(args, "test", result, True, report.format_result(result), 0)

    result = _base_result(mode, root, build, run_id, changes, plan)
    result["fingerprint"] = fingerprint
    result["timeout_seconds"] = budget_seconds
    result["budget_seconds"] = budget_seconds
    result["per_test_timeout_seconds"] = per_test_caps
    try:
        with cache.RunLock(root):
            cache.write_state(
                root,
                {
                    "status": "RUNNING",
                    "run_id": run_id,
                    "mode": mode,
                    "pid": os.getpid(),
                    "started_at": result["started_at"],
                    "log": str(log_dir / "tests.log"),
                    "message": "compilação/testes em execução",
                },
            )
            build_result, test_result, log_dir = _execute_plan(
                root,
                build,
                plan,
                mode,
                jobs=_jobs(args),
                budget_seconds=budget_seconds,
                per_test_caps=per_test_caps,
                no_build=args.no_build,
                run_id=run_id,
            )
    except cache.BusyError as exc:
        result.update(
            {
                "status": "BUSY",
                "message": str(exc),
                "finished_at": _utc_now(),
                "duration_seconds": round(time.monotonic() - started, 3),
                "next_action": "Aguarde a execução atual; consulte `status` ou use o resultado existente.",
            }
        )
        # Do not overwrite the running process's state or history.
        return _emit(args, "test", result, False, report.format_result(result), 3)
    except Exception as exc:
        result.update(
            {
                "status": "ERROR",
                "message": f"executor falhou: {exc}",
                "excerpt": traceback.format_exc(limit=8),
                "finished_at": _utc_now(),
                "duration_seconds": round(time.monotonic() - started, 3),
                "next_action": "Corrija o executor; nenhum resultado foi aprovado.",
            }
        )
        cache.write_state(root, {**result, "pid": os.getpid()})
        return _emit(args, "test", result, False, report.format_result(result), 1)

    result["build"] = build_result
    result["test"] = test_result
    if build_result.get("status") == "CONFIG_FAILED":
        status = "CONFIG_FAILED"
        result["excerpt"] = build_result.get("excerpt") or build_result.get("reason")
    elif build_result.get("status") == "BUILD_FAILED":
        status = "BUILD_FAILED"
        result["excerpt"] = build_result.get("excerpt") or build_result.get("reason")
    else:
        status = _test_outcome(test_result) if plan.get("selected_names") else "NO_TESTS"
        if status == "PASSED" and plan.get("coverage") != "known":
            status = "PASSED_PARTIAL"
        if status in {"BUDGET_EXHAUSTED", "TIMEOUT"}:
            plan["coverage"] = "partial"
    result["status"] = status
    result["coverage"] = plan.get("coverage")
    result["finished_at"] = _utc_now()
    result["duration_seconds"] = round(time.monotonic() - started, 3)
    result["timed_out"] = bool(test_result.get("timed_out")) or status == "TIMEOUT"
    result["budget_exhausted"] = bool(test_result.get("budget_exhausted")) or status == "BUDGET_EXHAUSTED"
    result["pending"] = test_result.get("pending", [])
    result["next_action"] = _next_action(status, mode, plan.get("coverage", "none"))
    if status == "PASSED":
        result["message"] = "testes selecionados passaram agora; escopo do modo FAST/IMPACT/FULL"
    elif status == "PASSED_PARTIAL":
        result["message"] = "testes selecionados passaram, mas a cobertura é limitada/desconhecida"
    elif status == "BUDGET_EXHAUSTED":
        pending = test_result.get("pending", [])
        required = test_result.get("pending_required", [])
        result["message"] = (
            f"orçamento de testes esgotado: {len(pending)} teste(s) pendente(s)"
            + (f", {len(required)} obrigatório(s)" if required else "")
            + "; cobertura incompleta, sem aprovação plena"
        )
    elif status == "REUSED":
        result["message"] = "resultado reutilizado"
    elif status == "NO_TESTS":
        result["message"] = "nenhum teste pôde ser associado com segurança"
    elif status == "UNKNOWN":
        result["message"] = "execução incompleta ou cobertura desconhecida; sem aprovação plena"
    elif status == "TIMEOUT":
        result["message"] = "timeout individual/global: processo e grupo encerrados; sem aprovação plena"
    else:
        result["message"] = "há falhas; consulte o primeiro erro e o log completo"
    cache.save_result(root, result, plan)
    cache.write_state(root, {**result, "pid": os.getpid()})
    exit_code = _exit_for_status(status)
    ok = status in {"PASSED", "REUSED", "DOCS_OK", "ASSET_OK"}
    return _emit(args, "test", result, ok, report.format_result(result), exit_code)


def _run_build(args) -> int:
    root, build = _paths(args)
    run_id = cache.new_run_id()
    log_dir = cache.logs_dir(root) / run_id
    log_dir.mkdir(parents=True, exist_ok=True)
    changes = changes_mod.summarize(root)
    target_arg = (args.target or "auto").strip()
    plan = None
    if target_arg == "auto":
        if not changes["files"]:
            data = {
                "run_id": run_id,
                "status": "NOT_RUN",
                "targets": [],
                "message": "nenhuma alteração detectada; nada para compilar",
            }
            return _emit(args, "build", data, True, "Nenhuma alteração; build não iniciado.", 0)
        inventory = _inventory(args, root, build)
        plan = selection.build_plan(inventory, changes, "fast", root=root)
        targets = plan.get("build_targets", [])
    elif target_arg == "all":
        targets = ["all"]
    else:
        targets = [part.strip() for part in target_arg.split(",") if part.strip()]
    if not targets:
        data = {"run_id": run_id, "status": "NOT_RUN", "targets": [], "plan": plan}
        return _emit(args, "build", data, True, "Nenhum target necessário.", 2)
    try:
        with cache.RunLock(root):
            result = runner.build(
                root,
                build,
                targets,
                jobs=_jobs(args),
                log_dir=log_dir,
                timeout=config.BUILD_TIMEOUT,
                configure=bool(args.configure or (plan and plan.get("configure_required"))),
            )
    except Exception as exc:
        data = {"run_id": run_id, "status": "ERROR", "targets": targets, "message": str(exc)}
        return _emit(args, "build", data, False, f"TURBO build: erro: {exc}", 1)
    data = {"run_id": run_id, "status": result.get("status"), "targets": targets, "result": result, "plan": plan}
    if result.get("status") == "PASSED":
        text = (
            f"LINBLUX ATLAS TURBO — BUILD\nStatus: OK\nTargets: {', '.join(targets)}\n"
            f"Duração: {result.get('duration_seconds', 0):.2f}s\nLog: {result.get('log') or log_dir / 'build.log'}"
        )
    else:
        text = report.format_result(
            {
                "mode": "build",
                "status": result.get("status", "FAILED"),
                "build": result,
                "plan": plan or {},
                "excerpt": result.get("excerpt"),
            }
        )
    return _emit(args, "build", data, result.get("status") == "PASSED", text, 0 if result.get("status") == "PASSED" else 1)


def _run_inventory(args) -> int:
    root, build = _paths(args)
    inventory = _inventory(args, root, build)
    data = {
        "root": str(root),
        "build_dir": str(build),
        "summary": inventory_mod.summary(inventory),
        "tests": inventory.get("tests", []),
        "missing_tests": inventory.get("missing_tests", []),
        "warnings": inventory.get("warnings", []),
        "atlas_info": inventory.get("atlas_info", {}),
    }
    text = report.format_inventory(inventory)
    return _emit(args, "inventory", data, True, text, 0)


def _run_doctor(args) -> int:
    root, build = _paths(args)
    env = config.environment_status()
    issues = []
    cache_file = build / "CMakeCache.txt"
    if not build.is_dir():
        issues.append({"severity": "high", "message": f"diretório de build ausente: {build}"})
    elif not cache_file.is_file():
        issues.append({"severity": "high", "message": "CMakeCache.txt ausente; execute cmake -S . -B <build>"})
    generator = None
    if cache_file.is_file():
        try:
            for line in cache_file.read_text(encoding="utf-8", errors="replace").splitlines():
                if line.startswith("CMAKE_GENERATOR:"):
                    generator = line.split("=", 1)[-1]
                    break
        except OSError:
            pass
    if not env["tools"].get("cmake"):
        issues.append({"severity": "high", "message": "cmake ausente"})
    if not env["tools"].get("ctest"):
        issues.append({"severity": "high", "message": "ctest ausente"})
    if not env["tools"].get("xvfb-run"):
        issues.append({"severity": "medium", "message": "xvfb-run ausente; testes gráficos não podem ser executados"})
    if not env["tools"].get("ninja"):
        issues.append({"severity": "low", "message": "ninja ausente; usando o gerador existente"})
    if not env["tools"].get("ccache"):
        issues.append({"severity": "low", "message": "ccache ausente; compilação incremental sem cache de compilador"})
    memory = env.get("memory_available_kib")
    if memory is not None and memory < 1024 * 1024:
        issues.append({"severity": "medium", "message": "memória disponível abaixo de 1 GiB; reduza --jobs"})
    try:
        usage = shutil.disk_usage("/tmp")
        if usage.free < 256 * 1024 * 1024:
            issues.append({"severity": "high", "message": "menos de 256 MiB livres em /tmp; testes podem falhar"})
    except OSError:
        pass
    inventory = None
    if build.is_dir():
        try:
            inventory = _inventory(args, root, build)
        except Exception as exc:
            issues.append({"severity": "medium", "message": f"inventário indisponível: {exc}"})
    if inventory:
        if not inventory.get("tests"):
            issues.append({"severity": "high", "message": "nenhum teste CTest registrado no build atual"})
        elif not any(test.get("ctest_labels") for test in inventory["tests"]):
            issues.append(
                {"severity": "medium", "message": "labels TURBO ainda não registrados; reexecute o CMake"}
            )
        for warning in inventory.get("warnings", []):
            issues.append({"severity": "low", "message": warning})
        if inventory.get("atlas_info", {}).get("warning"):
            issues.append({"severity": "medium", "message": inventory["atlas_info"]["warning"]})
    high = sum(1 for issue in issues if issue["severity"] == "high")
    medium = sum(1 for issue in issues if issue["severity"] == "medium")
    data = {
        "root": str(root),
        "build_dir": str(build),
        "environment": env,
        "generator": generator,
        "build_exists": build.is_dir(),
        "cmake_cache": cache_file.is_file(),
        "inventory": inventory_mod.summary(inventory) if inventory else None,
        "issues": issues,
        "high": high,
        "medium": medium,
        "low": sum(1 for issue in issues if issue["severity"] == "low"),
    }
    lines = [
        "LINBLUX ATLAS TURBO — DOCTOR",
        f"Raiz: {root}",
        f"Build: {build}",
        f"Gerador: {generator or 'desconhecido'}",
        f"Jobs seguros: {env['default_jobs']}",
        f"Testes: {(inventory_mod.summary(inventory)['tests'] if inventory else 0)}",
        f"Resumo: alta={high} média={medium} baixa={data['low']}",
    ]
    for issue in issues:
        lines.append(f"- [{issue['severity']}] {issue['message']}")
    if not issues:
        lines.append("Nenhum problema detectado.")
    return _emit(args, "doctor", data, high == 0, "\n".join(lines), 0 if high == 0 else 1)


def _run_report(args) -> int:
    root, _build = _paths(args)
    result = cache.load_result(root, args.mode)
    if not result:
        return _emit(args, "report", {"result": None}, True, "Nenhum relatório TURBO disponível.", 0)
    text = report.format_result(result, detail=args.detail)
    return _emit(args, "report", result, True, text, 0)


def _run_status(args) -> int:
    root, _build = _paths(args)
    state = cache.read_state(root)
    if state and state.get("pid"):
        try:
            os.kill(int(state["pid"]), 0)
        except (OSError, ValueError):
            state = {**state, "status": "STALE", "message": "processo registrado não está mais ativo"}
    latest = cache.load_result(root)
    text = report.format_state(state, latest)
    return _emit(args, "status", {"state": state, "latest": latest}, True, text, 0)


def _run_benchmark(args) -> int:
    root, build = _paths(args)
    started = time.monotonic()
    inventory = _inventory(args, root, build)
    inventory_seconds = time.monotonic() - started
    changes = changes_mod.summarize(root)
    plan = selection.build_plan(inventory, changes, "fast", root=root)
    payload = benchmark_mod.base_payload(inventory, plan)
    payload.update(
        {
            "run_id": cache.new_run_id(),
            "inventory_seconds": round(inventory_seconds, 4),
            "changed_count": changes.get("count", 0),
            "changes": [item["path"] for item in changes.get("files", [])],
            "run_fast": bool(args.run_fast),
        }
    )
    if args.run_fast:
        run_id = payload["run_id"]
        try:
            with cache.RunLock(root):
                build_result, test_result, log_dir = _execute_plan(
                    root,
                    build,
                    plan,
                    "fast",
                    jobs=_jobs(args),
                    budget_seconds=float(config.MODE_TEST_BUDGETS["fast"]),
                    per_test_caps={
                        "graphics": float(config.GRAPHICS_TEST_TIMEOUT),
                        "headless": float(config.HEADLESS_TEST_TIMEOUT),
                    },
                    no_build=False,
                    run_id=run_id,
                )
            payload["build"] = build_result
            payload["test"] = test_result
            payload["log_dir"] = str(log_dir)
            payload["measured_seconds"] = {
                "build": build_result.get("duration_seconds", 0),
                "tests": test_result.get("duration_seconds", 0),
                "total": round(time.monotonic() - started, 3),
            }
            payload["output_chars_estimate"] = len(
                report.format_result(
                    {
                        "mode": "fast",
                        "status": _test_outcome(test_result),
                        "plan": plan,
                        "build": build_result,
                        "test": test_result,
                    }
                )
            )
            benchmark_mod.save(root, payload)
            text = (
                "LINBLUX ATLAS TURBO — BENCHMARK\n"
                f"Inventário: {inventory_seconds:.3f}s\n"
                f"Build: {build_result.get('duration_seconds', 0):.2f}s\n"
                f"Testes FAST selecionados: {plan.get('test_count', 0)} "
                f"({plan.get('estimated_seconds', 0):.2f}s estimados)\n"
                f"Testes executados: {test_result.get('counts', {}).get('executed', 0)}; "
                f"passaram: {test_result.get('counts', {}).get('passed', 0)}\n"
                f"Suíte completa estimada pelo CTest: "
                f"{inventory_mod.summary(inventory)['estimated_full_seconds']:.1f}s\n"
                f"Log: {log_dir}\n"
                f"Registro: .turbo/benchmarks/{payload['run_id']}.json"
            )
            return _emit(args, "benchmark", payload, True, text, 0)
        except Exception as exc:
            payload["error"] = str(exc)
            benchmark_mod.save(root, payload)
            return _emit(args, "benchmark", payload, False, f"TURBO benchmark falhou: {exc}", 1)
    benchmark_mod.save(root, payload)
    text = (
        "LINBLUX ATLAS TURBO — BENCHMARK (estimativas)\n"
        f"Inventário: {inventory_seconds:.3f}s\n"
        f"Testes registrados: {inventory_mod.summary(inventory)['tests']}\n"
        f"Suíte completa estimada pelo CTest: "
        f"{inventory_mod.summary(inventory)['estimated_full_seconds']:.1f}s\n"
        f"Plano FAST: {plan.get('test_count', 0)} teste(s), "
        f"{plan.get('estimated_seconds', 0):.2f}s estimados\n"
        "Use --run-fast para medir a execução real."
    )
    return _emit(args, "benchmark", payload, True, text, 0)


def _run_organize(args) -> int:
    root, build = _paths(args)
    if args.apply:
        data = {
            "status": "REFUSED",
            "message": "migração automática desabilitada; use --dry-run e revise as evidências",
        }
        return _emit(args, "organize", data, False, data["message"], 1)
    inventory = _inventory(args, root, build) if build.is_dir() else None
    audit = organize_mod.audit(root, inventory)
    lines = [
        "LINBLUX ATLAS TURBO — ORGANIZAÇÃO (dry-run)",
        f"Arquivos rastreados: {audit['tracked_files']}",
        f"Achados: {len(audit['findings'])}",
    ]
    for finding in audit["findings"]:
        lines.append(f"- [{finding['severity']}] {finding['kind']}: {finding['recommendation']}")
        for evidence in finding["evidence"][:5]:
            lines.append(f"  {evidence}")
    lines.append("Nenhum arquivo foi movido ou removido.")
    return _emit(args, "organize", audit, True, "\n".join(lines), 0)


def _hdd_critical_refusal(parsed) -> str | None:
    """``hdd-critical`` means: do not add I/O. Compiling/running tests is I/O.

    ``test --plan`` stays allowed because it only reads the inventory. The
    refusal is explicit instead of silently skipping work, so an agent never
    mistakes an untested change for a tested one.
    """
    mode = (os.environ.get("ATLAS_IO_MODE") or "").strip().lower()
    if mode != "hdd-critical":
        return None
    if parsed.command not in {"build", "test", "benchmark"}:
        return None
    if parsed.command == "test" and getattr(parsed, "plan", False):
        return None
    hint = "ATLAS_IO_MODE=hdd-safe" if parsed.command == "test" else "ATLAS_IO_MODE=normal"
    return (
        f"modo hdd-critical: {parsed.command} compila/executa testes e escreve em "
        f"disco; peça autorização e use {hint}. `test --plan` continua permitido."
    )


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    parser = _parser()
    parsed = None
    try:
        parsed = parser.parse_args(normalize_argv(argv))
        refused = _hdd_critical_refusal(parsed)
        if refused:
            return _emit(
                parsed, parsed.command,
                {"status": "REFUSED", "message": refused}, False, refused, 1,
            )
        if parsed.command == "doctor":
            return _run_doctor(parsed)
        if parsed.command == "inventory":
            return _run_inventory(parsed)
        if parsed.command == "build":
            return _run_build(parsed)
        if parsed.command == "test":
            return _run_test(parsed)
        if parsed.command == "report":
            return _run_report(parsed)
        if parsed.command == "status":
            return _run_status(parsed)
        if parsed.command == "benchmark":
            return _run_benchmark(parsed)
        if parsed.command == "organize":
            return _run_organize(parsed)
        parser.error("comando desconhecido")
    except KeyboardInterrupt:
        print("TURBO interrompido pelo usuário.", file=sys.stderr)
        return 130
    except Exception as exc:
        if parsed is not None and getattr(parsed, "json", False):
            print(
                json.dumps(
                    {
                        "ok": False,
                        "command": getattr(parsed, "command", "unknown"),
                        "message": str(exc),
                        "data": {},
                    },
                    ensure_ascii=False,
                )
            )
        else:
            print(f"TURBO erro: {exc}", file=sys.stderr)
        if os.environ.get("TURBO_DEBUG"):
            traceback.print_exc()
        return 1
    return 1
