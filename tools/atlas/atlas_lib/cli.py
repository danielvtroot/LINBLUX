"""Command-line interface for LINBLUX ATLAS."""

from __future__ import annotations

import argparse
import json
import sqlite3
import sys
from pathlib import Path

from . import __version__
from .config import repo_root
from .db import open_db
from .diagnose import diagnose, fix_plan
from .diagnostics import doctor, status, verify
from .graph import impact, tests_for, trace
from .indexer import index_repository
from .report import benchmark, evaluate, write_generated_docs
from .search import locate, symbol, text_search


def _normalize_argv(argv: list[str]) -> list[str]:
    """Allow --json/--root/--quiet before or after the subcommand."""
    globals_: list[str] = []
    rest: list[str] = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg in {"--json", "-j", "--quiet", "-q", "--brief", "--detail"}:
            globals_.append(arg)
            i += 1
        elif arg == "--root" and i + 1 < len(argv):
            globals_.append("--root")
            globals_.append(argv[i + 1])
            i += 2
        elif arg.startswith("--root="):
            globals_.append(arg)
            i += 1
        else:
            rest.append(arg)
            i += 1
    return globals_ + rest


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="atlas.py",
        description="LINBLUX ATLAS — inteligência estrutural local, incremental e explicável.",
    )
    parser.add_argument("--root", default=".", help="raiz do projeto (padrão: diretório atual)")
    parser.add_argument("--json", "-j", action="store_true", help="saída JSON estruturada")
    parser.add_argument("--quiet", "-q", action="store_true", help="silencia mensagens auxiliares")
    parser.add_argument("--brief", action="store_true", help="saída curta, pensada para agentes de IA")
    parser.add_argument("--detail", action="store_true", help="saída detalhada (padrão nos comandos de consulta)")
    parser.add_argument("--version", action="version", version=f"LINBLUX ATLAS {__version__}")
    sub = parser.add_subparsers(dest="command", required=True)

    init = sub.add_parser("init", help="cria/recria o índice completo")
    init.add_argument("--force", action="store_true", help="remove o cache antes de indexar")

    update_p = sub.add_parser("update", help="atualiza o índice incrementalmente")
    update_p.add_argument(
        "--full",
        action="store_true",
        help="recalcula SHA-256 de todos os arquivos (ignora o atalho de metadados)",
    )

    sub.add_parser("status", help="estado do índice e ferramentas")

    locate_p = sub.add_parser("locate", help="localiza áreas para um sintoma ou termo")
    locate_p.add_argument("query", nargs="+")
    locate_p.add_argument("--limit", type=int, default=8)
    locate_p.add_argument("--module")

    symbol_p = sub.add_parser("symbol", help="busca definições e referências textuais")
    symbol_p.add_argument("name")
    symbol_p.add_argument("--limit", type=int, default=20)

    text_p = sub.add_parser("text", help="pesquisa textual por tokens no índice")
    text_p.add_argument("query", nargs="+")
    text_p.add_argument("--limit", type=int, default=10)

    impact_p = sub.add_parser("impact", help="estima consumidores e testes afetados")
    impact_p.add_argument("target", help="caminho relativo ou símbolo")
    impact_p.add_argument("--depth", type=int, default=2)
    impact_p.add_argument("--limit", type=int, default=30)

    trace_p = sub.add_parser("trace", help="mostra fluxo provável para um tema")
    trace_p.add_argument("query", nargs="+")
    trace_p.add_argument("--limit", type=int, default=6)

    tests_p = sub.add_parser("tests-for", help="testes associados a um arquivo")
    tests_p.add_argument("path")
    tests_p.add_argument("--limit", type=int, default=20)

    diagnose_p = sub.add_parser(
        "diagnose", help="diagnóstico curto: arquivos, linhas, hipóteses e testes"
    )
    diagnose_p.add_argument("query", nargs="+")
    diagnose_p.add_argument("--max-files", type=int, default=5)
    diagnose_p.add_argument("--max-tests", type=int, default=3)
    diagnose_p.add_argument("--max-chars", type=int, default=1100)
    diagnose_p.add_argument("--detail", action="store_true", help="saída completa, sem limite")

    plan_p = sub.add_parser("fix-plan", help="plano de correção somente leitura")
    plan_p.add_argument("query", nargs="+")
    plan_p.add_argument("--max-files", type=int, default=3)
    plan_p.add_argument("--max-tests", type=int, default=3)
    plan_p.add_argument("--max-chars", type=int, default=1100)
    plan_p.add_argument("--detail", action="store_true", help="saída completa, sem limite")

    doctor_p = sub.add_parser("doctor", help="diagnostica inconsistências estruturais")
    doctor_p.add_argument("--strict", action="store_true")

    sub.add_parser("verify", help="verifica integridade, hashes e consultas")

    sub.add_parser("docs", help="regenera o inventário em docs/atlas/generated")

    bench_p = sub.add_parser("bench", help="mede indexação, atualização e consultas")
    bench_p.add_argument("--iterations", type=int, default=5)

    sub.add_parser("eval", help="mede o ranking em perguntas reais verificadas")

    return parser


def _clip(text: str, limit: int, *, detail: bool = False) -> str:
    """Deterministic line-boundary truncation with an explicit omission marker.

    Agents get a short answer by default; ``--detail`` returns everything and
    ``--max-chars`` tunes the budget without ever producing invalid JSON.
    """
    if detail or limit <= 0 or len(text) <= limit:
        return text
    marker = "… (omitido; use --detail para a saída completa)\n"
    budget = max(0, limit - len(marker))
    kept = text[:budget]
    cut = kept.rsplit("\n", 1)[0] if "\n" in kept else kept
    return cut + "\n" + marker


def _brief_payload(command: str, data: dict) -> dict:
    """Few fields, deterministic truncation, never an invalid partial object."""
    if command in {"locate", "diagnose", "fix-plan"}:
        files = [
            {
                key: item.get(key)
                for key in ("path", "line", "module", "score", "symbol", "match_types", "textual", "inferred")
            }
            for item in (data.get("files") or [])[:5]
        ]
        tests = data.get("tests") or []
        names = [item if isinstance(item, str) else item.get("name") for item in tests][:3]
        payload = {
            "query": data.get("query"),
            "confidence": data.get("confidence"),
            "files": files,
            "tests": [name for name in names if name],
            "next": data.get("next") if isinstance(data.get("next"), str) else (data.get("next") or [])[:2],
        }
        if command == "fix-plan":
            payload["plan"] = (data.get("plan") or [])[:3]
        if data.get("hypotheses"):
            payload["hypotheses"] = data["hypotheses"][:2]
        return payload
    if command == "impact":
        return {
            "resolved_file": data.get("resolved_file"),
            "module": data.get("module"),
            "affected_count": data.get("affected_count"),
            "files": (data.get("affected_files") or [])[:5],
            "tests": [test.get("name") for test in (data.get("tests") or [])][:5],
            "risks": (data.get("risks") or [])[:2],
        }
    if command == "tests-for":
        return {
            "path": data.get("path"),
            "tests": [
                {key: test.get(key) for key in ("name", "target", "confidence", "reasons")}
                for test in (data.get("tests") or [])[:8]
            ],
        }
    return data


def _brief_text(command: str, data: dict) -> str:
    lines: list[str] = []
    if command in {"locate", "diagnose", "fix-plan"}:
        lines.append(f"ATLAS {command.upper()} — confiança {data.get('confidence', '?')}")
        for item in data.get("files") or []:
            where = f"{item['path']}:{item['line']}" if item.get("line") else item["path"]
            symbol = f" ({item['symbol']})" if item.get("symbol") else ""
            why = ", ".join(item.get("match_types") or []) or ("inferido" if item.get("inferred") else "?")
            lines.append(f"- {where}{symbol} [{why}]")
        if data.get("tests"):
            lines.append("testes: " + ", ".join(str(name) for name in data["tests"] if name))
        if data.get("hypotheses"):
            lines.append("hipótese: " + str(data["hypotheses"][0].get("text", "")))
        for step in data.get("plan") or []:
            lines.append(f"{step.get('step')}. {step.get('action')} {step.get('where')}")
        next_step = data.get("next")
        if isinstance(next_step, str):
            lines.append("próximo: " + next_step)
        elif next_step:
            lines.append("próximo: " + "; ".join(str(item) for item in next_step[:2]))
        return "\n".join(lines) + "\n"
    if command == "impact":
        lines.append(f"ATLAS IMPACT — {data.get('resolved_file')}")
        lines.append(f"afetados: {data.get('affected_count')}")
        for path in data.get("files") or []:
            lines.append(f"- {path}")
        if data.get("tests"):
            lines.append("testes: " + ", ".join(data["tests"]))
        return "\n".join(lines) + "\n"
    if command == "tests-for":
        lines.append(f"ATLAS TESTS-FOR — {data.get('path')}")
        for test in data.get("tests") or []:
            lines.append(f"- {test['name']} [{test.get('confidence')}] {test.get('target') or ''}")
        return "\n".join(lines) + "\n"
    return _text_render(command, data)


def _text_render(command: str, data: dict) -> str:
    lines: list[str] = []
    if command in {"init", "update"}:
        lines.append("LINBLUX ATLAS — ÍNDICE")
        if data.get("noop"):
            lines.append(
                f"Nada a fazer: {data.get('indexed_files', 0)} arquivos já indexados "
                f"({data.get('elapsed_seconds', 0)}s)"
            )
            return "\n".join(lines) + "\n"
        lines.append(
            f"Arquivos: {data.get('indexed_files', 0)}  novos: {data.get('added', 0)}  "
            f"alterados: {data.get('updated', 0)}  removidos: {data.get('deleted', 0)}"
        )
        lines.append(
            f"Símbolos: {data.get('symbols', 0)}  relações: {data.get('relations', 0)}  "
            f"tempo: {data.get('elapsed_seconds', 0)}s  banco: {data.get('database_bytes', 0)} bytes"
        )
        counts = data.get("counts", {})
        if counts:
            lines.append("Contagens: " + ", ".join(f"{k}={v}" for k, v in counts.items()))
        return "\n".join(lines) + "\n"
    if command == "locate":
        lines.append("LINBLUX ATLAS — LOCALIZAÇÃO")
        lines.append("")
        lines.append(f"Problema: {data.get('query','')}")
        lines.append(f"Confiança: {data.get('confidence','baixa')}")
        if data.get("tokens"):
            lines.append("Termos: " + ", ".join(data["tokens"]))
        if data.get("files"):
            lines += ["", "Arquivos prováveis:"]
            for item in data["files"][:8]:
                where = f"{item['path']}:{item['line']}" if item.get("line") else item["path"]
                why = ", ".join(item.get("match_types") or []) or "sem evidência textual"
                lines.append(f"- {where}  (score {item['score']}, {why})")
        lines += ["", "Áreas candidatas:"]
        for index, candidate in enumerate(data.get("candidates", []), 1):
            lines.append(f"{index}. {candidate['module']} — pontuação {candidate['score']:.1f}")
            for file in candidate.get("files", [])[:2]:
                lines.append(f"   - {file['path']} (score {file['score']:.1f})")
            for symbol in candidate.get("symbols", [])[:3]:
                lines.append(f"   símbolo: {symbol['qualified']} ({symbol['path']}:{symbol['line']})")
            for test in candidate.get("tests", [])[:2]:
                lines.append(f"   teste: {test}")
        evidence = []
        for candidate in data.get("candidates", []):
            evidence.extend(candidate.get("evidence", []))
        if evidence:
            lines += ["", "Evidências:"]
            for item in evidence[:10]:
                lines.append(f"- {item}")
        if data.get("next"):
            lines += ["", "Próxima investigação:"]
            lines += [f"- {item}" for item in data["next"][:8]]
        if data.get("limitations"):
            lines += ["", "Limitações:"]
            lines += [f"- {item}" for item in data["limitations"]]
        if data.get("modules"):
            lines += ["", "Módulos disponíveis:"]
            for item in data["modules"]:
                lines.append(f"- {item['module']}: {item['files']} arquivo(s)")
        return "\n".join(lines) + "\n"
    if command == "symbol":
        lines.append(f"LINBLUX ATLAS — SÍMBOLO {data.get('name','')}")
        lines.append("")
        definitions = data.get("definitions", [])
        if definitions:
            lines.append("Definições/declarações:")
            for item in definitions:
                definition = "def" if item.get("end_line") else "decl"
                lines.append(
                    f"- {item['qualified']} [{item['kind']}/{definition}] {item['path']}:{item['line']} "
                    f"({item['confidence']}, {item['origin']})"
                )
        else:
            lines.append("Nenhum símbolo indexado com esse nome.")
        references = data.get("references", [])
        if references:
            lines += ["", "Referências textuais:"]
            for item in references[:20]:
                lines.append(f"- {item['path']}:{item['line']} — {item['text'][:140]}")
        if data.get("limitations"):
            lines += ["", "Limitações:"] + [f"- {item}" for item in data["limitations"]]
        return "\n".join(lines) + "\n"
    if command == "text":
        lines.append(f"LINBLUX ATLAS — TEXTO: {data.get('query','')}")
        for item in data.get("results", []):
            lines.append(f"- {item['path']} ({item['module']})")
            if item.get("snippet"):
                lines.append("  " + item["snippet"].replace("\n", " ")[:220])
        if not data.get("results"):
            lines.append("Nenhum resultado.")
        return "\n".join(lines) + "\n"
    if command == "impact":
        lines.append("LINBLUX ATLAS — IMPACTO")
        lines.append("")
        if data.get("error"):
            lines.append("Erro: " + data["error"])
            return "\n".join(lines) + "\n"
        lines.append(f"Arquivo: {data.get('resolved_file')}")
        lines.append(f"Módulo: {data.get('module')}  |  interface pública: {data.get('public_interface')}")
        lines.append(f"Arquivos afetados: {data.get('affected_count', 0)}")
        for level in data.get("levels", []):
            lines += ["", f"Profundidade {level['depth']}:"]
            for node in level["nodes"][:20]:
                lines.append(f"- {node['path']} [{node['module']}] via {node['relation']} ({node['confidence']})")
        tests = data.get("tests", [])
        if tests:
            lines += ["", "Testes relacionados:"]
            for test in tests:
                timeout = f", timeout {test['timeout']}s" if test.get("timeout") else ""
                lines.append(f"- {test['name']} ({test.get('origin','?')}{timeout})")
        if data.get("risks"):
            lines += ["", "Riscos:"] + [f"- {item}" for item in data["risks"]]
        if data.get("documentation"):
            lines += ["", "Documentação:"] + [f"- {item}" for item in data["documentation"]]
        if data.get("limitations"):
            lines += ["", "Limitações:"] + [f"- {item}" for item in data["limitations"]]
        return "\n".join(lines) + "\n"
    if command == "trace":
        lines.append(f"LINBLUX ATLAS — FLUXO: {data.get('query','')}")
        lines.append(f"Confiança: {data.get('confidence','baixa')}")
        for item in data.get("flow", []):
            lines += ["", f"Módulo: {item['module']}  Arquivo: {item['path']}"]
            for symbol in item.get("symbols", [])[:8]:
                lines.append(f"  símbolo: {symbol['qualified']} ({symbol['kind']}, linha {symbol['line']})")
            for include in item.get("includes", [])[:6]:
                lines.append(f"  include: {include['target']} ({include['confidence']})")
            if item.get("cmake_targets"):
                lines.append("  targets: " + ", ".join(item["cmake_targets"]))
            if item.get("tests"):
                lines.append("  testes: " + ", ".join(item["tests"]))
        if data.get("next"):
            lines += ["", "Próxima investigação:"] + [f"- {item}" for item in data["next"]]
        return "\n".join(lines) + "\n"
    if command == "diagnose":
        lines.append("LINBLUX ATLAS — DIAGNÓSTICO")
        lines.append(f"Problema: {data.get('query','')}")
        lines.append(f"Confiança: {data.get('confidence','baixa')} (hipótese, não causa confirmada)")
        lines += ["", "Arquivos prováveis:"]
        for item in data.get("files", [])[:4]:
            where = f"{item['path']}:{item['line']}" if item.get("line") else item["path"]
            symbol = f" — {item['symbol']}" if item.get("symbol") else ""
            lines.append(f"- {where}{symbol} ({', '.join(item.get('match_types') or [])})")
            if item.get("snippet"):
                lines.append(f"  {item['snippet'][:100]}")
        hypotheses = data.get("hypotheses") or []
        if hypotheses:
            lines += ["", "Hipótese (não confirmada):"]
            lines.append(f"- [{hypotheses[0]['confidence']}] {hypotheses[0]['text']}")
        if data.get("tests"):
            lines += ["", "Testes sugeridos:"]
            for test in data["tests"][:3]:
                reason = (test.get("reason") or "")[:90]
                lines.append(f"- {test['name']} [{test.get('confidence')}] {reason}")
        if data.get("risks"):
            lines += ["", "Riscos:"] + [f"- {item}" for item in data["risks"][:3]]
        if data.get("next"):
            lines += ["", f"Próximo passo: {data['next']}"]
        lines.append("Limitação: ranking textual/estrutural; confirme o código antes de editar")
        return "\n".join(lines) + "\n"
    if command == "fix-plan":
        lines.append("LINBLUX ATLAS — PLANO DE CORREÇÃO (somente leitura)")
        lines.append("")
        for step in data.get("plan", []):
            lines.append(f"{step['step']}. {step['action']}: {step['where']}")
            lines.append(f"   por quê: {step['why']}")
            lines.append(f"   pronto quando: {step['done_when']}")
        if data.get("next"):
            lines += ["", f"Próximo passo: {data['next']}"]
        return "\n".join(lines) + "\n"
    if command == "tests-for":
        lines.append(f"LINBLUX ATLAS — TESTES PARA {data.get('path','')}")
        for test in data.get("tests", []):
            reason = (test.get("reasons") or [""])[0]
            lines.append(
                f"- {test['name']} [{test.get('confidence','?')}]: {reason}"
            )
        if not data.get("tests"):
            lines.append("Nenhum teste mapeado.")
        return "\n".join(lines) + "\n"
    if command == "status":
        lines.append("LINBLUX ATLAS — STATUS")
        lines.append(f"Banco: {data.get('database')}")
        lines.append(f"Existe: {data.get('exists')}  Compatível: {data.get('compatible')}")
        if data.get("counts"):
            lines.append("Contagens: " + ", ".join(f"{k}={v}" for k, v in data["counts"].items()))
        lines.append(f"Alterações pendentes (estimativa): {data.get('stale_estimate', 0)}")
        tools = data.get("tools", {})
        lines.append(f"SQLite/FTS5: {tools.get('sqlite')} / {tools.get('fts5')}")
        lines.append(f"C++ semântico: {tools.get('semantic_cpp')}")
        if data.get("external_deps"):
            lines.append("Dependências externas: " + ", ".join(item["name"] for item in data["external_deps"][:12]))
        if data.get("next"):
            lines.append("Próximo: " + data["next"])
        return "\n".join(lines) + "\n"
    if command == "doctor":
        lines.append("LINBLUX ATLAS — DOCTOR")
        summary = data.get("summary", {})
        lines.append(f"Resumo: alta={summary.get('alta',0)} media={summary.get('media',0)} baixa={summary.get('baixa',0)}")
        for issue in data.get("issues", []):
            lines.append(f"- [{issue['severity']}] {issue['code']}: {issue['message']}")
            if issue.get("examples"):
                lines.append("  exemplos: " + ", ".join(map(str, issue["examples"][:6])))
            if issue.get("fix"):
                lines.append("  correção: " + issue["fix"])
        if not data.get("issues"):
            lines.append("Nenhuma inconsistência detectada.")
        return "\n".join(lines) + "\n"
    if command == "verify":
        lines.append("LINBLUX ATLAS — VERIFY")
        lines.append(f"Resultado: {'OK' if data.get('ok') else 'FALHOU'}")
        for check in data.get("checks", []):
            lines.append(f"- [{'ok' if check['ok'] else 'falha'}] {check['name']}: {check.get('detail','')}")
        if data.get("limitations"):
            lines += ["Limitações:"] + [f"- {item}" for item in data["limitations"]]
        return "\n".join(lines) + "\n"
    if command == "docs":
        return f"Inventário gerado: {data.get('path')}\n"
    if command == "bench":
        lines.append("LINBLUX ATLAS — BENCH")
        for key in (
            "copied_files", "initial_index_ms", "update_one_file_ms", "update_five_files_ms",
            "query_count", "query_average_ms", "query_max_ms", "index_bytes", "peak_rss_kib",
        ):
            lines.append(f"- {key}: {data.get(key)}")
        economy = data.get("token_economy", {})
        lines.append(f"- caracteres ATLAS/arquivos: {economy.get('atlas_output_chars')}/{economy.get('candidate_file_chars')} (ratio {economy.get('ratio')})")
        return "\n".join(lines) + "\n"
    if command == "eval":
        lines.append("LINBLUX ATLAS — EVAL")
        lines.append(f"Perguntas: {data.get('total')}  top1: {data.get('top1')}  top3: {data.get('top3')}  módulo: {data.get('module_hits')}  símbolo: {data.get('symbol_hits')}")
        lines.append(f"Taxas: top1={data.get('top1_rate')} top3={data.get('top3_rate')} módulo={data.get('module_hit_rate')} símbolo={data.get('symbol_hit_rate')}")
        for item in data.get("questions", []):
            lines.append(f"- {item['id']}: {item['query']} → {item['returned_modules']} (rank {item['module_rank']})")
        return "\n".join(lines) + "\n"
    return json.dumps(data, ensure_ascii=False, indent=2) + "\n"


def main(argv: list[str] | None = None) -> int:
    argv = _normalize_argv(list(sys.argv[1:] if argv is None else argv))
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        root = repo_root(args.root)
        command = args.command
        if command == "init":
            data = index_repository(root, force=True, quiet=args.quiet or args.json)
            data["ok"] = True
        elif command == "update":
            data = index_repository(
                root, force=False, quiet=args.quiet or args.json, rehash=args.full
            )
            data["ok"] = True
        elif command == "status":
            data = status(root)
        elif command == "locate":
            conn = open_db(root)
            data = locate(root, conn, " ".join(args.query), limit=args.limit, module=args.module)
            conn.close()
        elif command == "symbol":
            conn = open_db(root)
            data = symbol(conn, args.name, root, limit=args.limit)
            conn.close()
        elif command == "text":
            conn = open_db(root)
            data = text_search(conn, " ".join(args.query), limit=args.limit)
            conn.close()
        elif command == "impact":
            conn = open_db(root)
            data = impact(root, conn, args.target, depth=args.depth, limit=args.limit)
            conn.close()
        elif command == "trace":
            conn = open_db(root)
            data = trace(root, conn, " ".join(args.query), limit=args.limit)
            conn.close()
        elif command == "tests-for":
            from .graph import safe_project_path
            path = safe_project_path(root, args.path)
            if path is None:
                raise ValueError("caminho fora da raiz do projeto")
            conn = open_db(root)
            tests = tests_for(conn, path)[: max(1, args.limit)]
            conn.close()
            data = {"path": path, "tests": tests}
        elif command == "diagnose":
            conn = open_db(root)
            data = diagnose(
                root, conn, " ".join(args.query),
                max_files=args.max_files, max_tests=args.max_tests,
            )
            conn.close()
        elif command == "fix-plan":
            conn = open_db(root)
            data = fix_plan(
                root, conn, " ".join(args.query),
                max_files=args.max_files, max_tests=args.max_tests,
            )
            conn.close()
        elif command == "doctor":
            data = doctor(root, strict=args.strict)
        elif command == "verify":
            data = verify(root)
        elif command == "docs":
            data = write_generated_docs(root)
            data["ok"] = True
        elif command == "bench":
            data = benchmark(root, iterations=args.iterations)
        elif command == "eval":
            data = evaluate(root)
        else:
            parser.error(f"comando desconhecido: {command}")
            return 2
    except (ValueError, FileNotFoundError, sqlite3.DatabaseError, RuntimeError, OSError) as exc:
        if args.json:
            print(json.dumps({"ok": False, "command": args.command, "error": str(exc)}, ensure_ascii=False, indent=2))
        else:
            print(f"ATLAS: {exc}", file=sys.stderr)
        return 2

    if args.brief:
        data = _brief_payload(args.command, data)

    if args.json:
        payload = {"ok": bool(data.get("ok", not data.get("error"))), "command": args.command, "data": data}
        # Uniform error envelope: every command reports failure the same way.
        if data.get("error"):
            payload["error"] = data["error"]
        print(json.dumps(payload, ensure_ascii=False, indent=2))
    elif args.brief:
        text = _brief_text(args.command, data)
        if args.command in {"diagnose", "fix-plan"}:
            text = _clip(text, getattr(args, "max_chars", 1100), detail=args.detail)
        print(text, end="")
    else:
        text = _text_render(args.command, data)
        if args.command in {"diagnose", "fix-plan"}:
            text = _clip(text, getattr(args, "max_chars", 1100), detail=args.detail)
        print(text, end="")

    if args.command == "verify" and not data.get("ok"):
        return 1
    if args.command == "doctor" and args.strict and not data.get("ok"):
        return 1
    if data.get("error"):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
