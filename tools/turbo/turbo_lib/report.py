"""Compact, agent-friendly rendering of TURBO results."""

from __future__ import annotations

from . import config

STATUS_PT = {
    "PASSED": "PASSOU AGORA",
    "PASSED_NOW": "PASSOU AGORA",
    "PASSED_PARTIAL": "PASSOU PARCIALMENTE — COBERTURA DESCONHECIDA",
    "REUSED": "RESULTADO REUTILIZADO",
    "FAILED": "FALHOU",
    "ERROR": "ERRO DO EXECUTOR",
    "TIMEOUT": "TIMEOUT",
    "NOT_RUN": "NÃO EXECUTADO",
    "UNKNOWN": "COBERTURA DESCONHECIDA",
    "NO_TESTS": "COBERTURA DESCONHECIDA",
    "BUILD_FAILED": "FALHA DE COMPILAÇÃO",
    "CONFIG_FAILED": "FALHA DE CONFIGURAÇÃO",
    "DOCS_OK": "DOCUMENTAÇÃO OK",
    "ASSET_OK": "RECURSO OK",
    "BUSY": "OCUPADO — OUTRO TURBO EM EXECUÇÃO",
    "RUNNING": "EM ANDAMENTO",
    "SKIPPED": "IGNORADO",
}


def status_label(status: str) -> str:
    return STATUS_PT.get(status, status)


def _short(value: str, limit: int = 1200) -> str:
    value = (value or "").strip()
    if len(value) <= limit:
        return value
    return value[:limit] + "\n[... truncado; veja o log completo]"


def format_plan(plan: dict) -> str:
    from .selection import explain

    return "LINBLUX ATLAS TURBO — PLANO\n" + explain(plan)


def format_result(result: dict, *, detail: bool = False) -> str:
    lines = ["LINBLUX ATLAS TURBO", ""]
    lines.append(f"Modo: {str(result.get('mode', '?')).upper()}")
    lines.append(f"Status: {status_label(result.get('status', '?'))}")
    if result.get("run_id"):
        lines.append(f"Execução: {result['run_id']}")
    if result.get("changed_count") is not None:
        lines.append(f"Arquivos alterados: {result.get('changed_count')}")
    plan = result.get("plan") or {}
    if plan:
        lines.append(f"Cobertura: {plan.get('coverage', '?')}")
    build = result.get("build") or {}
    if build:
        build_label = (
            "REUTILIZADA (não executada agora)"
            if result.get("reused")
            else status_label(build.get("status", "?"))
        )
        lines.append(
            f"Compilação: {build_label}"
            + (f" ({build.get('duration_seconds', 0):.1f}s)" if build.get("duration_seconds") else "")
        )
    test = result.get("test") or {}
    counts = test.get("counts") or {}
    if counts:
        lines.append("Testes (histórico reutilizado):" if result.get("reused") else "Testes:")
        lines.append(f"  executados: {counts.get('executed', 0)}")
        lines.append(f"  aprovados: {counts.get('passed', 0)}")
        lines.append(f"  falhas: {counts.get('failed', 0)}")
        lines.append(f"  timeout: {counts.get('timeout', 0)}")
        lines.append(f"  não executados: {counts.get('not_run', 0)}")
    elif result.get("status") == "REUSED":
        lines.append("Testes: resultado reutilizado, nenhum teste iniciado agora")
    docs = result.get("docs") or result.get("assets") or {}
    if docs:
        lines.append(f"Documentação/JSON: {status_label(docs.get('status', '?'))} "
                     f"({len(docs.get('checked', []))} arquivo(s))")
    failure_candidates = []
    for name, item in sorted((test.get("tests") or {}).items()):
        if item.get("status") in {"FAILED", "ERROR", "TIMEOUT"}:
            failure_candidates.append((name, item))
    if failure_candidates:
        name, item = failure_candidates[0]
        lines.append("")
        lines.append(f"Falha: {name}")
        lines.append(f"Detalhe: {_short(item.get('detail') or test.get('excerpt') or result.get('message', ''))}")
    elif result.get("status") in {"BUILD_FAILED", "CONFIG_FAILED", "ERROR"}:
        lines.append("")
        lines.append("Falha:")
        lines.append(_short(result.get("excerpt") or result.get("message") or "consulte o log"))
    elif result.get("status") == "REUSED":
        lines.append("")
        lines.append("RESULTADO REUTILIZADO — não foi executado agora.")
        lines.append("Use --no-cache para exigir execução real.")
    elif result.get("status") in {"NOT_RUN", "UNKNOWN", "NO_TESTS"}:
        lines.append("")
        lines.append("Nenhuma aprovação foi emitida. " + (result.get("message") or "consulte o plano."))
    logs = []
    for key in ("build", "test", "docs"):
        block = result.get(key) or {}
        for log in (block.get("log"), (block.get("command") or {}).get("log")):
            if log and log not in logs:
                logs.append(log)
    if logs:
        lines.append("")
        lines.append("Log completo:")
        for log in logs[:4]:
            lines.append(f"  {log}")
    if result.get("next_action"):
        lines.append("")
        lines.append(f"Próxima ação: {result['next_action']}")
    if result.get("duration_seconds") is not None and result["status"] != "REUSED":
        lines.append("")
        lines.append(f"Duração total: {result['duration_seconds']:.1f}s")
    if detail:
        lines.append("")
        lines.append("Plano:")
        lines.append(format_plan(plan) if plan else "(sem plano)")
    return "\n".join(lines).strip() + "\n"


def format_inventory(inventory: dict) -> str:
    from .inventory import summary

    info = summary(inventory)
    lines = [
        "LINBLUX ATLAS TURBO — INVENTÁRIO",
        f"Testes: {info['tests']} | gráficos: {info['graphics']} | headless: {info['headless']} | "
        f"lentos: {info['slow']} | reutilizáveis: {info['reusable']}",
        f"Estimativa da suíte completa pelo histórico: {info['estimated_full_seconds']:.1f}s",
        "",
        "Testes registrados:",
    ]
    for test in inventory.get("tests", []):
        labels = ",".join(test.get("labels", []))
        duration = test.get("duration_seconds") or 0
        target = test.get("target") or "-"
        lines.append(f"- {test['name']}: {duration:.2f}s; target={target}; labels={labels}")
    if inventory.get("missing_tests"):
        lines.append("")
        lines.append("Condicionais não registrados: " + ", ".join(inventory["missing_tests"]))
    if inventory.get("warnings"):
        lines.append("")
        for warning in inventory["warnings"]:
            lines.append(f"AVISO: {warning}")
    return "\n".join(lines) + "\n"


def format_state(state: dict | None, latest: dict | None) -> str:
    lines = ["LINBLUX ATLAS TURBO — ESTADO"]
    if state:
        lines.append(f"Estado: {status_label(state.get('status', '?'))}")
        lines.append(f"Run: {state.get('run_id', '?')}")
        lines.append(f"Modo: {state.get('mode', '?')}")
        if state.get("log"):
            lines.append(f"Log: {state['log']}")
        if state.get("message"):
            lines.append(state["message"])
    else:
        lines.append("Nenhuma execução em andamento registrada.")
    if latest:
        lines.append("")
        lines.append(f"Último resultado: {status_label(latest.get('status', '?'))} "
                     f"({latest.get('run_id', '?')})")
    return "\n".join(lines) + "\n"
