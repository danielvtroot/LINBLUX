"""Compact, evidence-first diagnosis (ATLAS 2.0).

``diagnose`` composes ``locate`` + ``symbol`` + ``tests-for`` + ``impact`` into a
short answer for an agent: likely files with a line, why they matched, ranked
hypotheses (explicitly hypotheses), test suggestions and the next inspection.

Nothing here edits code, runs tests or executes indexed content.
"""

from __future__ import annotations

import json
import sqlite3
from pathlib import Path

from .graph import impact, tests_for
from .search import locate

MAX_SCAN_LINES = 4000


def _line_text(root: Path, rel: str, line: int | None) -> str:
    if not line or line < 1:
        return ""
    path = root / rel
    try:
        with path.open(encoding="utf-8", errors="replace") as handle:
            for index, text in enumerate(handle, 1):
                if index == line:
                    return text.strip()[:200]
                if index > MAX_SCAN_LINES:
                    break
    except OSError:
        return ""
    return ""


def _first_match_line(root: Path, rel: str, tokens: list[str]) -> int | None:
    if not tokens:
        return None
    path = root / rel
    lowered = [token for token in tokens if len(token) >= 3][:6]
    if not lowered:
        return None
    try:
        with path.open(encoding="utf-8", errors="replace") as handle:
            for index, text in enumerate(handle, 1):
                low = text.lower()
                if any(token in low for token in lowered):
                    return index
                if index > MAX_SCAN_LINES:
                    break
    except OSError:
        return None
    return None


def _symbol_of(conn: sqlite3.Connection, rel: str, line: int | None) -> dict | None:
    if not line:
        return None
    row = conn.execute(
        "SELECT s.qualified,s.kind,s.line FROM symbols s JOIN files f ON f.id=s.file_id "
        "WHERE f.path=? AND s.line<=? ORDER BY s.line DESC LIMIT 1",
        (rel, line),
    ).fetchone()
    return dict(row) if row else None


def diagnose(
    root: Path,
    conn: sqlite3.Connection,
    query: str,
    *,
    max_files: int = 5,
    max_tests: int = 3,
) -> dict:
    max_files = max(1, min(int(max_files), 10))
    max_tests = max(1, min(int(max_tests), 8))
    located = locate(root, conn, query, limit=max(max_files, 5))
    tokens = located.get("tokens", [])
    files = located.get("files", [])[:max_files]

    candidates = []
    for item in files:
        rel = item["path"]
        line = item.get("line")
        if line is None and item.get("textual"):
            line = _first_match_line(root, rel, tokens)
        symbol = _symbol_of(conn, rel, line)
        snippet = _line_text(root, rel, line) or (item.get("evidence") or [""])[0]
        candidates.append({
            "path": rel,
            "line": line,
            "symbol": symbol["qualified"] if symbol else None,
            "symbol_kind": symbol["kind"] if symbol else None,
            "module": item.get("module"),
            "score": item.get("score"),
            "textual": item.get("textual"),
            "inferred": item.get("inferred"),
            "match_types": item.get("match_types", []),
            "evidence": item.get("evidence", [])[:2],
            "snippet": snippet[:160],
        })

    tests: list[dict] = []
    for candidate in candidates[:2]:
        for test in tests_for(conn, candidate["path"]):
            if len(tests) >= max_tests:
                break
            if any(existing["name"] == test["name"] for existing in tests):
                continue
            tests.append({
                "name": test["name"],
                "target": test.get("target"),
                "confidence": test.get("confidence"),
                "reason": (test.get("reasons") or [""])[0],
            })
        if len(tests) >= max_tests:
            break

    hypotheses = []
    if candidates:
        top = candidates[0]
        why = ", ".join(top["match_types"]) or "correspondência fraca"
        hypotheses.append({
            "text": (
                f"o comportamento descrito é implementado em "
                f"{top['symbol'] or top['path']}"
            ),
            "confidence": "média" if top["textual"] and not top["inferred"] else "baixa",
            "evidence": [f"{top['path']}:{top['line']}" if top["line"] else top["path"], f"evidência: {why}"],
        })
    if len(candidates) > 1:
        second = candidates[1]
        hypotheses.append({
            "text": f"alternativa: {second['path']} participa do fluxo",
            "confidence": "baixa",
            "evidence": [f"{second['path']}:{second['line']}" if second["line"] else second["path"]],
        })

    risks = []
    if candidates:
        impacted = impact(root, conn, candidates[0]["path"], depth=1, limit=20)
        if impacted.get("public_interface"):
            risks.append("cabeçalho público: consumidores precisam recompilar")
        if impacted.get("affected_count"):
            risks.append(f"{impacted['affected_count']} arquivo(s) afetados por include/target")
        if not tests:
            risks.append("nenhum teste mapeado: cobertura desconhecida")

    next_step = "confirmar no código antes de editar"
    if candidates and candidates[0]["line"]:
        next_step = (
            f"abrir {candidates[0]['path']}:{candidates[0]['line']} e confirmar a hipótese"
        )
    elif candidates:
        next_step = f"abrir {candidates[0]['path']} e localizar o símbolo relacionado"

    result = {
        "query": query,
        "confidence": located.get("confidence", "baixa"),
        "files": candidates,
        "tests": tests,
        "hypotheses": hypotheses,
        "risks": risks,
        "next": next_step,
        "limitations": [
            "hipótese derivada de ranking textual/estrutural; não é causa confirmada",
            "confirme a linha indicada antes de editar",
        ],
    }
    result["chars"] = len(json.dumps(result, ensure_ascii=False))
    return result


def fix_plan(
    root: Path,
    conn: sqlite3.Connection,
    query: str,
    *,
    max_files: int = 3,
    max_tests: int = 3,
) -> dict:
    """Short, read-only plan for an agent. Never edits or executes anything."""
    diagnosis = diagnose(root, conn, query, max_files=max_files, max_tests=max_tests)
    steps = []
    for index, candidate in enumerate(diagnosis["files"][:max_files], 1):
        where = f"{candidate['path']}:{candidate['line']}" if candidate["line"] else candidate["path"]
        steps.append({
            "step": index,
            "action": "inspecionar",
            "where": where,
            "why": candidate["snippet"] or (candidate["evidence"] or [""])[0],
            "done_when": "comportamento reproduzido ou causa descartada",
        })
    if diagnosis["tests"]:
        names = [test["name"] for test in diagnosis["tests"]]
        steps.append({
            "step": len(steps) + 1,
            "action": "validar",
            "where": "tools/turbo",
            "why": "testes estruturais associados: " + ", ".join(names),
            "done_when": "teste focado passa e nenhum mandatory regrediu",
        })
    result = {
        "query": query,
        "plan": steps,
        "hypotheses": diagnosis["hypotheses"],
        "risks": diagnosis["risks"],
        "next": diagnosis["next"],
        "edits_code": False,
        "limitations": [
            "plano somente leitura; nenhuma edição ou execução automática",
            "passos são hipóteses até confirmação no código",
        ],
    }
    result["chars"] = len(json.dumps(result, ensure_ascii=False))
    return result
