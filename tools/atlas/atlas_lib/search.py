"""Deterministic, explainable search and symbol lookup."""

from __future__ import annotations

import re
import sqlite3
from pathlib import Path

from .config import CONTRACT_MODULES, STOPWORDS, glossary, normalize_text

MAX_LIMIT = 50


def stem_token(token: str) -> str:
    """Small, deterministic Portuguese plural normaliser (not a stemmer)."""
    base = token
    if base.endswith("oes") and len(base) > 4:
        base = base[:-3] + "ao"
    elif base.endswith("aes") and len(base) > 4:
        base = base[:-3] + "ao"
    elif base.endswith("ais") and len(base) > 4:
        base = base[:-2] + "l"
    elif base.endswith("eis") and len(base) > 4:
        base = base[:-2] + "l"
    elif base.endswith("ns") and len(base) > 4:
        base = base[:-2] + "m"
    elif base.endswith("s") and len(base) > 4:
        base = base[:-1]
    return base


def query_tokens(query: str) -> list[str]:
    normalized = normalize_text(query)
    words = re.findall(r"[a-z0-9_][a-z0-9_.:/-]*", normalized)
    tokens = []
    for word in words:
        word = word.strip("._:/-")
        if len(word) < 2 or word in STOPWORDS:
            continue
        for value in (word, stem_token(word)):
            if len(value) >= 2 and value not in STOPWORDS and value not in tokens:
                tokens.append(value)
    return tokens[:16]


AREA_WEIGHTS = {
    "source": 1.0,
    "test": 0.72,
    "docs": 0.42,
    "asset": 0.40,
    "tool": 0.35,
    "script": 0.55,
    "config": 0.5,
    "agent-config": 0.35,
    "package": 0.4,
}

# Fixed FTS candidate pool. It must not depend on ``--limit``: the audit showed
# the top-1 changing when only the display limit changed (audit B3 / S1, S4, S7).
CANDIDATE_POOL = 240
# A candidate that exists only because of the glossary cannot score above a
# candidate with real textual evidence (audit B4).
INFERRED_SCORE_CAP = 6.0



def _fts_quote(token: str) -> str:
    return '"' + token.replace('"', '""') + '"'


def _fts_match(tokens: list[str]) -> str:
    return " OR ".join(_fts_quote(token) for token in tokens)


def _hint_map(root: Path) -> dict[str, dict]:
    data = glossary(root)
    raw = data.get("terms", data)
    hints = {}
    if isinstance(raw, dict):
        for key, value in raw.items():
            if isinstance(value, dict):
                hints[normalize_text(key)] = value
    return hints


def _hint_score(module: str, path: str, token: str, hints: dict[str, dict]) -> tuple[float, list[str]]:
    hint = hints.get(token)
    if not hint:
        return 0.0, []
    score = 0.0
    evidence = []
    modules = {normalize_text(item) for item in hint.get("modules", [])}
    paths = [normalize_text(item) for item in hint.get("paths", [])]
    if module in modules:
        score += 7.0
        evidence.append(f"glossário: {token} → módulo {module}")
    for prefix in paths:
        if normalize_text(path).startswith(prefix):
            score += 3.0
            evidence.append(f"glossário: {token} → caminho {prefix}")
            break
    return score, evidence


def _symbol_hits(
    conn: sqlite3.Connection,
    file_id: int,
    tokens: list[str],
    extra_symbols: list[str] | None = None,
    limit: int = 3,
) -> tuple[float, list[dict]]:
    rows = conn.execute(
        "SELECT name,qualified,kind,line FROM symbols WHERE file_id=? ORDER BY line", (file_id,)
    ).fetchall()
    expected = {normalize_text(item) for item in (extra_symbols or [])}
    scored = []
    for row in rows:
        name = normalize_text(row["name"])
        qualified = normalize_text(row["qualified"])
        value = 0.0
        for token in tokens:
            if name == token or qualified == token:
                value = max(value, 10.0)
            elif name.startswith(token) or qualified.startswith(token):
                value = max(value, 6.0)
            elif token in name or token in qualified:
                value = max(value, 3.5)
        for symbol in expected:
            if name == symbol or qualified == symbol or qualified.endswith("::" + symbol):
                value = max(value, 8.0)
            elif name.startswith(symbol) or qualified.startswith(symbol):
                value = max(value, 4.0)
        if value:
            scored.append((value, dict(row)))
    scored.sort(key=lambda item: (-item[0], item[1]["line"]))
    return sum(item[0] for item in scored[:limit]), [item[1] for item in scored[:limit]]


def _hint_symbols_for(module: str, tokens: list[str], hints: dict[str, dict]) -> list[str]:
    symbols: list[str] = []
    for token in tokens:
        hint = hints.get(token)
        if not hint:
            continue
        if module in {normalize_text(item) for item in hint.get("modules", [])}:
            for symbol in hint.get("symbols", []):
                if symbol not in symbols:
                    symbols.append(symbol)
    return symbols[:12]


def _module_symbols(
    conn: sqlite3.Connection,
    module: str,
    tokens: list[str],
    hint_symbols: list[str],
    limit: int = 4,
) -> list[dict]:
    names = []
    for item in tokens + hint_symbols:
        value = normalize_text(item)
        if value and value not in names:
            names.append(value)
    if not names:
        return []
    clauses = []
    params: list = [module]
    for name in names[:20]:
        clauses.append("(s.name LIKE ? COLLATE NOCASE OR s.qualified LIKE ? COLLATE NOCASE)")
        params.extend([f"%{name}%", f"%{name}%"])
    sql = (
        "SELECT DISTINCT s.name,s.qualified,s.kind,s.line,f.path "
        "FROM symbols s JOIN files f ON f.id=s.file_id "
        "WHERE f.module=? AND (" + " OR ".join(clauses) + ") "
        "ORDER BY s.name,s.line LIMIT 60"
    )
    try:
        rows = [dict(row) for row in conn.execute(sql, params)]
    except sqlite3.OperationalError:
        return []
    expected = {normalize_text(item) for item in hint_symbols}
    scored = []
    for row in rows:
        name = normalize_text(row["name"])
        qualified = normalize_text(row["qualified"])
        value = 0.0
        for token in tokens:
            if name == token or qualified == token:
                value = max(value, 10.0)
            elif name.startswith(token) or qualified.startswith(token):
                value = max(value, 6.0)
            elif token in name or token in qualified:
                value = max(value, 3.5)
        if name in expected or qualified in expected or any(qualified.endswith("::" + item) for item in expected):
            value = max(value, 9.0)
        if value:
            scored.append((value, row))
    scored.sort(key=lambda item: (-item[0], item[1]["path"], item[1]["line"]))
    result = []
    seen = set()
    for _, row in scored:
        key = (row["qualified"], row["path"], row["line"])
        if key in seen:
            continue
        seen.add(key)
        result.append(row)
        if len(result) >= limit:
            break
    return result


def _module_rows(conn: sqlite3.Connection, module: str, hint_symbols: list[str]) -> list[dict]:
    rows: list[dict] = []
    if hint_symbols:
        clauses = []
        params: list = [module]
        for symbol in hint_symbols[:8]:
            clauses.append("(s.name=? COLLATE NOCASE OR s.qualified LIKE ? COLLATE NOCASE)")
            params.extend([symbol, "%::" + symbol])
        try:
            rows = [dict(row) for row in conn.execute(
                "SELECT DISTINCT f.id,f.path,f.module,f.area,f.language,0.0 AS rank,'' AS snippet "
                "FROM files f JOIN symbols s ON s.file_id=f.id "
                "WHERE f.module=? AND (" + " OR ".join(clauses) + ") LIMIT 3",
                params,
            )]
        except sqlite3.OperationalError:
            rows = []
    if not rows:
        rows = [dict(row) for row in conn.execute(
            "SELECT f.id,f.path,f.module,f.area,f.language,0.0 AS rank,'' AS snippet,"
            " COUNT(s.id) AS symbol_count FROM files f LEFT JOIN symbols s ON s.file_id=f.id "
            "WHERE f.module=? GROUP BY f.id ORDER BY symbol_count DESC,f.path LIMIT 2",
            (module,),
        )]
    for row in rows:
        row["hint_only"] = True
    return rows


def _synthesize_hint_rows(
    conn: sqlite3.Connection,
    tokens: list[str],
    hints: dict[str, dict],
    existing_modules: set[str],
    module_filter: str | None,
    existing_paths: set[str] | None = None,
) -> list[dict]:
    """Add files whose *symbols* the glossary points to, even when the module
    already appeared through a weaker textual match (audit S4/S8)."""
    added = []
    existing_paths = existing_paths or set()
    for token in tokens:
        hint = hints.get(token)
        if not hint:
            continue
        for module in hint.get("modules", []):
            module = normalize_text(module)
            if module_filter and module != normalize_text(module_filter):
                continue
            if not module:
                continue
            hint_symbols = hint.get("symbols", [])
            rows = _module_rows(conn, module, hint_symbols)
            for row in rows:
                if row["path"] in existing_paths:
                    continue
                row["hint_only"] = True
                row["hint_symbols"] = hint_symbols
                added.append(row)
                existing_paths.add(row["path"])
            if rows:
                existing_modules.add(module)
    return added



def _test_hits(conn: sqlite3.Connection, path: str) -> list[dict]:
    rows = conn.execute(
        "SELECT DISTINCT t.name,t.command,t.source_path,t.timeout "
        "FROM tests t JOIN test_files tf ON tf.test_id=t.id WHERE tf.path=? ORDER BY t.name",
        (path,),
    ).fetchall()
    return [dict(row) for row in rows]


def _candidate_rows(conn: sqlite3.Connection, tokens: list[str], module: str | None, limit: int) -> list[dict]:
    if not tokens:
        return []
    match = _fts_match(tokens)
    sql = (
        "SELECT f.id,f.path,f.module,f.area,f.language,"
        " bm25(file_fts, 2.5, 1.5, 1.0) AS rank,"
        " snippet(file_fts, 2, '[[', ']]', '…', 18) AS snippet "
        "FROM file_fts JOIN files f ON f.id=file_fts.rowid "
        "WHERE file_fts MATCH ?"
    )
    params: list = [match]
    if module:
        sql += " AND f.module=?"
        params.append(module)
    sql += " ORDER BY rank LIMIT ?"
    params.append(limit)
    try:
        rows = [dict(row) for row in conn.execute(sql, params)]
    except sqlite3.OperationalError:
        rows = []
    if rows:
        return rows
    # Fallback for substrings that FTS tokenisation cannot match.
    clauses = []
    params = []
    for token in tokens:
        clauses.append("(lower(f.path) LIKE ?)")
        params.append(f"%{token}%")
    if module:
        clauses.append("f.module=?")
        params.append(module)
    sql = "SELECT f.id,f.path,f.module,f.area,f.language,0.0 AS rank,'' AS snippet FROM files f"
    sql += " WHERE " + " OR ".join(clauses) + " LIMIT ?"
    params.append(limit)
    return [dict(row) for row in conn.execute(sql, params)]


def _module_contract(root: Path, module: str) -> str | None:
    path = root / "docs/atlas/module-contracts" / f"{module}.md"
    return path.relative_to(root).as_posix() if path.is_file() else None


def _confidence(score: float, gap: float) -> str:
    if score >= 20 and gap >= 4:
        return "alta"
    if score >= 9:
        return "parcial"
    return "baixa"


def _match_types(item: dict) -> list[str]:
    """Which kinds of evidence produced this candidate (explainable output)."""
    breakdown = item["score_breakdown"]
    kinds: list[str] = []
    if breakdown.get("symbols", 0) > 0:
        kinds.append("symbol")
    if breakdown.get("fts", 0) > 0:
        kinds.append("code")
    if breakdown.get("path", 0) > 0:
        kinds.append("path")
    if breakdown.get("glossary", 0) > 0:
        kinds.append("glossary")
    if breakdown.get("tests", 0) > 0:
        kinds.append("test")
    if item["area"] == "docs":
        kinds.append("docs")
    return kinds


def _public_file(item: dict) -> dict:
    return {
        "path": item["path"],
        "module": item["module"],
        "area": item["area"],
        "score": item["score"],
        "line": item.get("line"),
        "textual": item["textual"],
        "inferred": item["inferred"],
        "match_types": item["match_types"],
        "score_breakdown": item["score_breakdown"],
        "symbols": item["symbols"][:2],
        "tests": item["tests"][:3],
        "evidence": item["evidence"][:3],
    }


def overview(root: Path, conn: sqlite3.Connection, limit: int = 12) -> dict:
    rows = conn.execute(
        "SELECT module,COUNT(*) AS files,SUM(line_count) AS lines FROM files "
        "GROUP BY module ORDER BY files DESC,module"
    ).fetchall()
    modules = [dict(row) for row in rows][:limit]
    return {
        "query": "",
        "tokens": [],
        "candidates": [],
        "modules": modules,
        "confidence": "baixa",
        "limitations": [
            "consulta vazia: exibindo inventário de módulos",
            "use: atlas.py locate \"sintoma ou termo\"",
        ],
    }


def locate(root: Path, conn: sqlite3.Connection, query: str, limit: int = 8, module: str | None = None) -> dict:
    limit = max(1, min(limit, MAX_LIMIT))
    tokens = query_tokens(query)
    if not tokens:
        return overview(root, conn, limit)
    hints = _hint_map(root)
    rows = _candidate_rows(conn, tokens, module, CANDIDATE_POOL)
    existing_modules = {row["module"] for row in rows}
    rows.extend(
        _synthesize_hint_rows(
            conn, tokens, hints, existing_modules, module,
            {row["path"] for row in rows},
        )
    )
    if not rows:
        return {
            "query": query,
            "tokens": tokens,
            "candidates": [],
            "confidence": "baixa",
            "limitations": ["nenhum arquivo indexado corresponde aos termos"],
            "next": ["verifique o índice com: atlas.py status", "tente termos do domínio: camera, rig, salvar, fisica"],
        }

    candidates = []
    seen_paths = set()
    for row in rows:
        path = row["path"]
        if path in seen_paths:
            continue
        seen_paths.add(path)
        module_name = row["module"]
        score = max(0.0, -float(row["rank"] or 0.0))
        evidence = []
        breakdown = {"fts": round(score, 3)}
        normalized_path = normalize_text(path)
        path_score = 0.0
        for token in tokens:
            if token in normalized_path:
                path_score += 4.0
                evidence.append(f"caminho contém '{token}': {path}")
        score += path_score
        breakdown["path"] = round(path_score, 3)

        hint_score = 0.0
        for token in tokens:
            value, why = _hint_score(module_name, path, token, hints)
            hint_score += value
            evidence.extend(why)
        hint_score = min(hint_score, 18.0)
        score += hint_score
        breakdown["glossary"] = round(hint_score, 3)

        hint_symbols = _hint_symbols_for(module_name, tokens, hints)
        symbol_score, symbols = _symbol_hits(conn, row["id"], tokens, hint_symbols)
        score += symbol_score
        breakdown["symbols"] = round(symbol_score, 3)
        for symbol in symbols[:2]:
            evidence.append(
                f"símbolo {symbol['qualified']} ({path}:{symbol['line']}, {symbol['kind']})"
            )

        if row.get("hint_only"):
            inferred = 5.0
            score += inferred
            breakdown["inferred"] = inferred
            evidence.append("módulo sugerido pelo glossário; sem correspondência textual forte")

        tests = _test_hits(conn, path)
        test_score = min(len(tests) * 1.5, 4.5)
        score += test_score
        breakdown["tests"] = round(test_score, 3)
        for test in tests[:2]:
            evidence.append(f"teste associado: {test['name']}")

        snippet = (row.get("snippet") or "").replace("\n", " ")
        if snippet:
            evidence.append("trecho: " + snippet[:180])
        if not evidence:
            evidence.append(f"correspondência textual em {path}")

        inferred = bool(row.get("hint_only")) and not (
            breakdown["fts"] or path_score or symbol_score
        )
        if inferred:
            score = min(score, INFERRED_SCORE_CAP)

        area_weight = AREA_WEIGHTS.get(row["area"], 0.5)
        score *= area_weight
        breakdown["area_weight"] = area_weight
        evidence = list(dict.fromkeys(evidence))
        candidates.append({
            "path": path,
            "module": module_name,
            "area": row["area"],
            "language": row["language"],
            "score": round(score, 3),
            "line": symbols[0]["line"] if symbols else None,
            "score_breakdown": breakdown,
            "evidence": evidence[:6],
            "symbols": symbols[:4],
            "tests": [test["name"] for test in tests[:4]],
            "inferred": inferred,
        })

    # Textual evidence always outranks glossary-only inference, even when the
    # inferred candidate has a higher raw score. ``--limit`` only truncates the
    # ordering below; it never reorders it (audit B3/B4).
    for item in candidates:
        breakdown = item["score_breakdown"]
        item["textual"] = bool(
            breakdown.get("fts", 0) > 0
            or breakdown.get("path", 0) > 0
            or breakdown.get("symbols", 0) > 0
        )
        item["match_types"] = _match_types(item)

    candidates.sort(
        key=lambda item: (
            0 if item["textual"] else 1,
            -item["score"],
            item["module"],
            item["path"],
        )
    )

    ranked_files = candidates[: max(limit * 3, 12)]

    grouped: dict[str, dict] = {}
    module_order: list[str] = []
    for candidate in ranked_files:
        module_name = candidate["module"]
        group = grouped.get(module_name)
        if group is None:
            group = {
                "module": module_name,
                "score": candidate["score"],
                "files": [],
                "symbols": [],
                "tests": [],
                "evidence": [],
                "inferred": False,
            }
            grouped[module_name] = group
            module_order.append(module_name)
        if len(group["files"]) >= 3:
            continue
        group["files"].append(candidate)
        group["inferred"] = group["inferred"] or candidate["inferred"]
        for symbol in candidate["symbols"]:
            if len(group["symbols"]) < 4:
                group["symbols"].append({
                    "qualified": symbol["qualified"], "kind": symbol["kind"],
                    "path": candidate["path"], "line": symbol["line"],
                })
        for test in candidate["tests"]:
            if test not in group["tests"] and len(group["tests"]) < 4:
                group["tests"].append(test)
        if candidate["evidence"]:
            group["evidence"].append(candidate["evidence"][0])

    modules = [grouped[name] for name in module_order][:limit]
    for group in modules:
        module_name = group["module"]
        hint_symbols = _hint_symbols_for(module_name, tokens, hints)
        existing = {
            (symbol["qualified"], symbol["path"], symbol["line"])
            for symbol in group["symbols"]
        }
        for symbol in _module_symbols(conn, module_name, tokens, hint_symbols):
            key = (symbol["qualified"], symbol["path"], symbol["line"])
            if key in existing:
                continue
            if len(group["symbols"]) >= 4:
                break
            group["symbols"].append({
                "qualified": symbol["qualified"], "kind": symbol["kind"],
                "path": symbol["path"], "line": symbol["line"],
            })
            existing.add(key)
        contract = _module_contract(root, module_name)
        if contract:
            group["contract"] = contract

    files = [_public_file(item) for item in ranked_files[: max(limit, 5)]]
    top = files[0]["score"] if files else 0.0
    gap = top - (files[1]["score"] if len(files) > 1 else 0.0)
    confidence = _confidence(top, gap)
    next_steps = []
    for candidate in modules[:3]:
        contract = _module_contract(root, candidate["module"])
        if contract:
            next_steps.append(f"consultar contrato {contract}")
        for test in candidate["tests"][:1]:
            next_steps.append(f"executar teste relacionado: ctest -R '^{re.escape(test)}$'")
    next_steps.append("aprofundar com: atlas.py impact <arquivo> e atlas.py symbol <símbolo>")
    return {
        "query": query,
        "tokens": tokens,
        "confidence": confidence,
        "candidates": modules,
        "files": files,
        "next": next_steps[:8],
        "limitations": [
            "ranking textual/regex; não é análise semântica completa de C++",
            "referências são textuais, não chamadas comprovadas",
        ],
        "total_files_considered": len(rows),
    }


def symbol(conn: sqlite3.Connection, name: str, root: Path, limit: int = 20) -> dict:
    limit = max(1, min(limit, MAX_LIMIT))
    exact = conn.execute(
        "SELECT s.name,s.qualified,s.kind,s.line,s.end_line,s.signature,s.origin,s.confidence,f.path,f.module "
        "FROM symbols s JOIN files f ON f.id=s.file_id "
        "WHERE s.name=? COLLATE NOCASE OR s.qualified=? COLLATE NOCASE "
        "ORDER BY f.path,s.line LIMIT ?",
        (name, name, limit),
    ).fetchall()
    rows = [dict(row) for row in exact]
    if not rows:
        pattern = f"%{name}%"
        rows = [dict(row) for row in conn.execute(
            "SELECT s.name,s.qualified,s.kind,s.line,s.end_line,s.signature,s.origin,s.confidence,f.path,f.module "
            "FROM symbols s JOIN files f ON f.id=s.file_id "
            "WHERE s.name LIKE ? COLLATE NOCASE OR s.qualified LIKE ? COLLATE NOCASE "
            "ORDER BY CASE WHEN s.name LIKE ? COLLATE NOCASE THEN 0 ELSE 1 END,f.path,s.line LIMIT ?",
            (pattern, pattern, name + "%", limit),
        )]
    definitions = [row for row in rows if row["line"]]
    references = []
    token = re.split(r"::|\.", name)[-1]
    if token and len(token) >= 3:
        try:
            candidates = conn.execute(
                "SELECT f.path FROM file_fts JOIN files f ON f.id=file_fts.rowid "
                "WHERE file_fts MATCH ? ORDER BY bm25(file_fts,2.5,1.5,1.0) LIMIT 20",
                (_fts_quote(normalize_text(token)),),
            ).fetchall()
        except sqlite3.OperationalError:
            candidates = []
        definitions_by_line = {(row["path"], row["line"]) for row in rows}
        pattern = re.compile(r"\b" + re.escape(token) + r"\b")
        for candidate in candidates:
            path = candidate["path"]
            try:
                text = (root / path).read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            for index, line in enumerate(text.splitlines(), 1):
                if pattern.search(line) and (path, index) not in definitions_by_line:
                    references.append({"path": path, "line": index, "text": line.strip()[:180]})
                    if len(references) >= limit:
                        break
            if len(references) >= limit:
                break
        # Files that define the symbol but were not in FTS candidates still show
        # their own textual mentions after the definition line.
    return {
        "name": name,
        "definitions": definitions,
        "references": references,
        "reference_kind": "textual",
        "limitations": [
            "não há distinção semântica entre sobrecargas e namespaces homônimos",
            "referências textuais não provam chamada em tempo de execução",
        ],
    }


def text_search(conn: sqlite3.Connection, query: str, limit: int = 10) -> dict:
    limit = max(1, min(limit, MAX_LIMIT))
    tokens = query_tokens(query)
    if not tokens:
        return {"query": query, "results": [], "limitations": ["consulta sem termos úteis"]}
    try:
        rows = conn.execute(
            "SELECT f.path,f.module,f.area,"
            " bm25(file_fts,2.5,1.5,1.0) AS rank,"
            " snippet(file_fts,2,'[[',']]','…',28) AS snippet "
            "FROM file_fts JOIN files f ON f.id=file_fts.rowid "
            "WHERE file_fts MATCH ? ORDER BY rank LIMIT ?",
            (_fts_match(tokens), limit),
        ).fetchall()
    except sqlite3.OperationalError:
        rows = []
    return {
        "query": query,
        "tokens": tokens,
        "results": [dict(row) for row in rows],
        "limitations": ["correspondência textual; não é busca semântica"],
    }
