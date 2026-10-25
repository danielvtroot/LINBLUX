"""Dependency graph queries: impact, tests-for and trace."""

from __future__ import annotations

import re
import sqlite3
from pathlib import Path

from .config import normalize_text
from .search import locate, symbol

MAX_VISIT = 120


def safe_project_path(root: Path, value: str) -> str | None:
    """Resolve a user path only when it stays inside the project root."""
    raw = Path(value).expanduser()
    try:
        resolved = (raw if raw.is_absolute() else root / raw).resolve()
    except OSError:
        return None
    try:
        return resolved.relative_to(root).as_posix()
    except ValueError:
        return None


def _file_row(conn: sqlite3.Connection, path: str):
    return conn.execute("SELECT * FROM files WHERE path=?", (path,)).fetchone()


def _reverse_includes(conn: sqlite3.Connection, path: str) -> list[dict]:
    rows = conn.execute(
        "SELECT DISTINCT f.path,f.module,r.kind,r.confidence FROM relations r "
        "JOIN files f ON f.id=r.source_file WHERE r.target=? ORDER BY f.path",
        (path,),
    ).fetchall()
    return [dict(row) for row in rows]


def _targets_for_file(conn: sqlite3.Connection, path: str) -> list[str]:
    rows = conn.execute(
        "SELECT DISTINCT t.name FROM target_sources ts JOIN cmake_targets t ON t.id=ts.target_id "
        "WHERE ts.path=? AND ts.kind='source' ORDER BY t.name",
        (path,),
    ).fetchall()
    return [row["name"] for row in rows]


def _dependent_targets(conn: sqlite3.Connection, target: str) -> list[str]:
    rows = conn.execute(
        "SELECT DISTINCT t.name FROM target_sources ts JOIN cmake_targets t ON t.id=ts.target_id "
        "WHERE ts.kind='dependency' AND ts.path=? ORDER BY t.name",
        (target,),
    ).fetchall()
    return [row["name"] for row in rows]


def _target_files(conn: sqlite3.Connection, target: str) -> list[str]:
    rows = conn.execute(
        "SELECT DISTINCT ts.path FROM target_sources ts JOIN cmake_targets t ON t.id=ts.target_id "
        "WHERE t.name=? AND ts.kind='source' ORDER BY ts.path",
        (target,),
    ).fetchall()
    return [row["path"] for row in rows]


def tests_for(conn: sqlite3.Connection, path: str) -> list[dict]:
    rows = conn.execute(
        "SELECT DISTINCT t.name,t.command,t.target,t.source_path,t.timeout,t.origin "
        "FROM tests t JOIN test_files tf ON tf.test_id=t.id WHERE tf.path=? ORDER BY t.name",
        (path,),
    ).fetchall()
    found = {row["name"]: dict(row) for row in rows}
    # A test that includes the header is also relevant even when CMake maps only
    # the main test translation unit.
    for parent in _reverse_includes(conn, path):
        parent_rows = conn.execute(
            "SELECT DISTINCT t.name,t.command,t.target,t.source_path,t.timeout,t.origin "
            "FROM tests t JOIN test_files tf ON tf.test_id=t.id WHERE tf.path=? ORDER BY t.name",
            (parent["path"],),
        ).fetchall()
        for row in parent_rows:
            found.setdefault(row["name"], dict(row))
    for target in _targets_for_file(conn, path):
        for row in conn.execute(
            "SELECT DISTINCT t.name,t.command,t.target,t.source_path,t.timeout,t.origin "
            "FROM tests t WHERE t.target=? ORDER BY t.name",
            (target,),
        ).fetchall():
            found.setdefault(row["name"], dict(row))
    return sorted(found.values(), key=lambda item: item["name"])


def _symbol_file(conn: sqlite3.Connection, value: str) -> str | None:
    row = conn.execute(
        "SELECT f.path FROM symbols s JOIN files f ON f.id=s.file_id "
        "WHERE s.name=? COLLATE NOCASE OR s.qualified=? COLLATE NOCASE "
        "ORDER BY CASE WHEN s.name=? COLLATE NOCASE THEN 0 ELSE 1 END, f.path LIMIT 1",
        (value, value, value),
    ).fetchone()
    return row["path"] if row else None


def impact(root: Path, conn: sqlite3.Connection, target: str, depth: int = 2, limit: int = 30) -> dict:
    depth = max(1, min(int(depth), 5))
    limit = max(1, min(int(limit), 80))
    path = safe_project_path(root, target)
    if path is None or not (root / path).is_file():
        path = _symbol_file(conn, target)
    if path is None:
        return {
            "input": target,
            "error": "arquivo ou símbolo não encontrado",
            "limitations": ["informe um caminho relativo dentro do projeto ou um símbolo definido"],
        }
    row = _file_row(conn, path)
    if row is None:
        path = _symbol_file(conn, target)
        row = _file_row(conn, path) if path else None
    if row is None:
        return {"input": target, "error": "arquivo fora do índice", "limitations": ["execute atlas.py update"]}

    levels: list[dict] = []
    seen = {path}
    frontier = {path}
    for level in range(depth):
        next_frontier: set[str] = set()
        nodes = []
        for current in sorted(frontier):
            parents = _reverse_includes(conn, current)
            for parent in parents:
                if parent["path"] not in seen:
                    seen.add(parent["path"])
                    next_frontier.add(parent["path"])
                    nodes.append({
                        "path": parent["path"], "module": parent["module"],
                        "relation": parent["kind"], "confidence": parent["confidence"],
                        "via": current,
                    })
            for target_name in _targets_for_file(conn, current):
                for dependent in _dependent_targets(conn, target_name):
                    for source in _target_files(conn, dependent):
                        if source not in seen:
                            seen.add(source)
                            next_frontier.add(source)
                            nodes.append({
                                "path": source, "module": "build",
                                "relation": f"target {dependent} depende de {target_name}",
                                "confidence": "confirmed", "via": current,
                            })
            if len(seen) >= MAX_VISIT:
                break
        if nodes:
            levels.append({"depth": level + 1, "nodes": nodes[:limit]})
        frontier = next_frontier
        if not frontier or len(seen) >= MAX_VISIT:
            break

    module = row["module"]
    related_tests = tests_for(conn, path)
    public_interface = path.endswith((".h", ".hpp", ".hh"))
    risks = []
    if public_interface:
        risks.append("cabeçalho: interface de compilação pode afetar todos os consumidores")
    if module in {"serialization", "game", "experiences", "scene"}:
        risks.append("módulo ligado a dados persistidos: verifique compatibilidade de JSON/.lo e testes de ciclo de vida")
    if module in {"ui", "editor"}:
        risks.append("módulo de UI: valide também capturas e testes de interação sob Xvfb")
    if module in {"physics", "scripting"}:
        risks.append("módulo de execução: valide runtime, Lua e sessão de Play")
    if not related_tests:
        risks.append("nenhum teste mapeado diretamente; procure testes de integração do módulo")

    contracts = []
    contract = root / "docs/atlas/module-contracts" / f"{module}.md"
    if contract.is_file():
        contracts.append(contract.relative_to(root).as_posix())
    return {
        "input": target,
        "resolved_file": path,
        "module": module,
        "public_interface": public_interface,
        "levels": levels,
        "affected_files": sorted(seen - {path})[:limit],
        "affected_count": len(seen) - 1,
        "tests": related_tests[:12],
        "risks": risks,
        "documentation": contracts,
        "limitations": [
            "dependências de include e CMake são estruturais; chamadas dinâmicas/Lua/eventos podem não aparecer",
            "profundidade limitada e sem execução automática de testes",
        ],
    }


def trace(root: Path, conn: sqlite3.Connection, query: str, limit: int = 6) -> dict:
    located = locate(root, conn, query, limit=min(limit, 8))
    flow = []
    for candidate in located.get("candidates", [])[:3]:
        files = candidate.get("files", [])
        for file in files[:2]:
            row = _file_row(conn, file["path"])
            if not row:
                continue
            symbols = conn.execute(
                "SELECT name,qualified,kind,line FROM symbols WHERE file_id=? ORDER BY line LIMIT 8",
                (row["id"],),
            ).fetchall()
            includes = conn.execute(
                "SELECT target,kind,confidence FROM relations WHERE source_file=? AND kind='include' ORDER BY target",
                (row["id"],),
            ).fetchall()
            targets = _targets_for_file(conn, row["path"])
            flow.append({
                "module": candidate["module"],
                "path": row["path"],
                "symbols": [dict(item) for item in symbols],
                "includes": [dict(item) for item in includes][:8],
                "cmake_targets": targets,
                "tests": [test["name"] for test in tests_for(conn, row["path"])][:6],
            })
    if not flow:
        flow = [
            {"module": item["module"], "path": item["files"][0]["path"] if item.get("files") else "",
             "symbols": item.get("symbols", []), "includes": [], "cmake_targets": [], "tests": item.get("tests", [])}
            for item in located.get("candidates", [])[:3]
        ]
    return {
        "query": query,
        "confidence": located.get("confidence"),
        "flow": flow,
        "next": located.get("next", []),
        "limitations": located.get("limitations", []),
    }


def symbols_in_file(conn: sqlite3.Connection, path: str, limit: int = 40) -> list[dict]:
    rows = conn.execute(
        "SELECT name,qualified,kind,line,end_line,signature FROM symbols s "
        "JOIN files f ON f.id=s.file_id WHERE f.path=? ORDER BY s.line LIMIT ?",
        (path, limit),
    ).fetchall()
    return [dict(row) for row in rows]
