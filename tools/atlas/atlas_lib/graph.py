"""Dependency graph queries: impact, tests-for and trace."""

from __future__ import annotations

import re
import sqlite3
from pathlib import Path

from .config import normalize_text
from .search import locate, symbol

MAX_VISIT = 120
# A test source shared by more than this many tests is a harness, not a source
# of a specific test (audit B16: tests/x11_focus.cpp used by 20 UI tests).
SHARED_HARNESS_THRESHOLD = 3

_TEST_COLUMNS = "t.name,t.command,t.target,t.source_path,t.timeout,t.origin"
# Target-chain evidence is structural: every test linking the library would
# match. It is reported as ``inferred`` and capped so it cannot flood the answer
# with 60 tests that merely link the same static library (audit B5).
CHAIN_TEST_LIMIT = 8
MODULE_FALLBACK_LIMIT = 8
# A header included by more than this many test sources is infrastructure
# (Diagnostics.h, Part.h via common helpers); it cannot justify a specific test.
GENERIC_HEADER_FANOUT = 4


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


def _target_closure(
    conn: sqlite3.Connection, targets: set[str], limit: int = MAX_VISIT
) -> dict[str, str]:
    """Transitive dependents of ``targets`` -> the target they were reached from.

    Walks target -> dependent (reverse of ``target_link_libraries``), so an
    INTERFACE library in the middle no longer stops the walk (audit B6).
    """
    reached: dict[str, str] = {}
    frontier = sorted(targets)
    while frontier and len(reached) < limit:
        current = frontier.pop(0)
        for dependent in _dependent_targets(conn, current):
            if dependent in targets or dependent in reached:
                continue
            reached[dependent] = current
            frontier.append(dependent)
    return reached


def _shared_source_counts(conn: sqlite3.Connection) -> dict[str, int]:
    return {
        row["source_path"]: row["n"]
        for row in conn.execute(
            "SELECT source_path,COUNT(*) AS n FROM tests "
            "WHERE source_path IS NOT NULL GROUP BY source_path"
        )
    }


def _include_closure(
    conn: sqlite3.Connection, path: str, depth: int = 3, limit: int = 80
) -> set[str]:
    """Indexed headers reachable from ``path`` (structural, depth-limited)."""
    seen = {path}
    frontier = [path]
    for _ in range(depth):
        nxt: list[str] = []
        for current in frontier:
            rows = conn.execute(
                "SELECT DISTINCT r.target FROM relations r JOIN files f ON f.id=r.source_file "
                "WHERE f.path=? AND r.kind='include'",
                (current,),
            ).fetchall()
            for row in rows:
                target = row["target"]
                if target in seen:
                    continue
                if conn.execute("SELECT 1 FROM files WHERE path=?", (target,)).fetchone() is None:
                    continue
                seen.add(target)
                nxt.append(target)
                if len(seen) >= limit:
                    return seen
        frontier = nxt
        if not frontier:
            break
    return seen


def _tests_in_targets(conn: sqlite3.Connection, targets: list[str]) -> list[dict]:
    if not targets:
        return []
    placeholders = ",".join("?" for _ in targets)
    return [
        dict(row)
        for row in conn.execute(
            f"SELECT DISTINCT {_TEST_COLUMNS} FROM tests t "
            f"WHERE t.target IN ({placeholders}) ORDER BY t.name",
            tuple(sorted(targets)),
        )
    ]


def tests_for(conn: sqlite3.Connection, path: str) -> list[dict]:
    """Tests structurally associated with a file, with the reason for each.

    Evidence order: test source, include (transitive over indexed includes),
    same target, target chain, module fallback. Nothing here proves that a test
    *executes* the code; the reason string is part of the answer.
    """
    shared = _shared_source_counts(conn)
    found: dict[str, dict] = {}
    rank = {"confirmed": 0, "harness": 1, "inferred": 2}

    def add(rows: list[dict], reason: str, confidence: str) -> None:
        for row in rows:
            item = found.get(row["name"])
            if item is None:
                item = dict(row)
                item["reasons"] = []
                item["confidence"] = confidence
                found[row["name"]] = item
            elif rank.get(confidence, 3) < rank.get(item["confidence"], 3):
                item["confidence"] = confidence
            if reason not in item["reasons"]:
                item["reasons"].append(reason)

    direct = [
        dict(row)
        for row in conn.execute(
            f"SELECT DISTINCT {_TEST_COLUMNS} FROM tests t "
            "JOIN test_files tf ON tf.test_id=t.id WHERE tf.path=? ORDER BY t.name",
            (path,),
        )
    ]
    for row in direct:
        count = shared.get(row["source_path"] or "", 0)
        if count > SHARED_HARNESS_THRESHOLD:
            add(
                [row],
                f"harness compartilhado por {count} testes: {row['source_path']}",
                "harness",
            )
        else:
            add([row], f"fonte do teste: {path}", "confirmed")

    # Include evidence: a test source that includes this file (or one of its
    # headers) is coupled at compile time. Headers included by many test sources
    # are infrastructure and cannot justify a specific test (audit B5).
    fanout = {
        row["header"]: row["n"]
        for row in conn.execute(
            "SELECT r.target AS header,COUNT(DISTINCT r.source_file) AS n FROM relations r "
            "JOIN files f ON f.id=r.source_file "
            "WHERE r.kind='include' AND f.path LIKE 'tests/%' GROUP BY r.target"
        )
    }
    closure = sorted(_include_closure(conn, path))
    if closure:
        placeholders = ",".join("?" for _ in closure)
        rows = [
            dict(row)
            for row in conn.execute(
                f"SELECT DISTINCT {_TEST_COLUMNS}, consumer.path AS consumer, r.target AS header "
                "FROM tests t JOIN test_files tf ON tf.test_id=t.id "
                "JOIN files consumer ON consumer.path=tf.path "
                "JOIN relations r ON r.source_file=consumer.id "
                f"WHERE r.kind='include' AND r.target IN ({placeholders}) "
                "ORDER BY t.name, consumer.path",
                tuple(closure),
            )
        ]
        specific = [row for row in rows if fanout.get(row["header"], 0) <= GENERIC_HEADER_FANOUT]
        grouped: dict[str, dict] = {}
        for row in specific:
            entry = grouped.setdefault(row["name"], {"base": row, "pairs": []})
            pair = (row["consumer"], row["header"], fanout.get(row["header"], 0))
            if pair not in entry["pairs"]:
                entry["pairs"].append(pair)
        for name, entry in grouped.items():
            consumer, header, count = sorted(entry["pairs"], key=lambda item: (item[2], item[0]))[0]
            base = {key: entry["base"][key] for key in (
                "name", "command", "target", "source_path", "timeout", "origin"
            )}
            add([base], f"include: {consumer} inclui {header} ({count} fonte(s) de teste)", "confirmed")

    own_targets = set(_targets_for_file(conn, path))
    if own_targets:
        own_rows = _tests_in_targets(conn, sorted(own_targets))
        if own_rows:
            add(own_rows, f"mesmo target: {', '.join(sorted(own_targets))}", "confirmed")

    # Structural fallback: every test that links the library through CMake. It
    # is real dependency information but says nothing about which test exercises
    # this file, so it stays inferred and capped.
    if own_targets:
        chain_targets = sorted(_target_closure(conn, own_targets))
        chain_rows = _tests_in_targets(conn, chain_targets)[:CHAIN_TEST_LIMIT]
        if chain_rows:
            chain = ", ".join(sorted(own_targets))
            add(
                chain_rows,
                f"cadeia de targets a partir de {chain} (estrutural; não prova execução)",
                "inferred",
            )

    if not found:
        rows = [
            dict(row)
            for row in conn.execute(
                f"SELECT DISTINCT {_TEST_COLUMNS} FROM tests t "
                "JOIN test_files tf ON tf.test_id=t.id JOIN files f ON f.path=tf.path "
                "WHERE f.module=(SELECT module FROM files WHERE path=?) "
                "AND tf.path LIKE 'tests/%' ORDER BY t.name LIMIT ?",
                (path, MODULE_FALLBACK_LIMIT),
            )
        ]
        add(rows, "mesmo módulo do arquivo (aproximação por diretório)", "inferred")

    return sorted(
        found.values(),
        key=lambda item: (rank.get(item["confidence"], 3), item["name"]),
    )


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
            for parent in _reverse_includes(conn, current):
                if parent["path"] not in seen:
                    seen.add(parent["path"])
                    next_frontier.add(parent["path"])
                    nodes.append({
                        "path": parent["path"], "module": parent["module"],
                        "relation": parent["kind"], "confidence": parent["confidence"],
                        "via": current,
                    })
            if len(seen) >= MAX_VISIT:
                break
        if nodes:
            levels.append({"depth": level + 1, "nodes": nodes[:limit]})
        frontier = next_frontier
        if not frontier or len(seen) >= MAX_VISIT:
            break

    # Target graph: file -> its libraries -> every target that links them
    # (transitively) -> their sources. This is what makes a .cpp of a static
    # library report real consumers instead of zero (audit B6).
    own_targets = set(_targets_for_file(conn, path))
    closure = _target_closure(conn, own_targets) if own_targets else {}
    target_nodes: list[dict] = []
    for name in sorted(closure):
        for source in _target_files(conn, name):
            if source in seen:
                continue
            seen.add(source)
            target_nodes.append({
                "path": source,
                "module": "build",
                "relation": f"target {name} depende de {closure[name]}",
                "confidence": "confirmed",
                "via": closure[name],
            })
            if len(target_nodes) >= limit:
                break
        if len(target_nodes) >= limit:
            break
    if target_nodes:
        levels.append({"depth": len(levels) + 1, "nodes": target_nodes, "kind": "target"})

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
    if own_targets and len(closure) > 8:
        risks.append(
            f"biblioteca ligada a {len(closure)} targets: recompilação ampla; prefira validar por target de teste"
        )
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
        "targets": [
            {"name": name, "via": via, "type": next(
                (r["type"] for r in conn.execute(
                    "SELECT type FROM cmake_targets WHERE name=?", (name,)
                )),
                "unknown",
            )}
            for name, via in sorted(closure.items())
        ][:20],
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
    ranked = located.get("files") or []
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
                "line": file.get("line"),
                "symbols": [dict(item) for item in symbols],
                "includes": [dict(item) for item in includes][:8],
                "cmake_targets": targets,
                "tests": [test["name"] for test in tests_for(conn, row["path"])][:6],
            })
    if not flow:
        flow = [
            {
                "module": item.get("module", ""),
                "path": item.get("path", ""),
                "symbols": [], "includes": [], "cmake_targets": [],
                "tests": item.get("tests", []),
            }
            for item in ranked[:3]
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
