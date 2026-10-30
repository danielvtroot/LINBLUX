"""Health checks, status and index verification."""

from __future__ import annotations

import hashlib
import json
import shutil
import sqlite3
import time
from datetime import datetime, timezone
from pathlib import Path

from . import io_mode
from .config import CONTRACT_MODULES, INDEX_VERSION, cache_dir, db_path, glossary
from .db import (
    corrupt_or_missing_reason,
    get_meta,
    index_is_compatible,
    index_status,
    meta_dict,
    open_db,
    schema_version,
    table_counts,
)
from .indexer import collect_files, read_text_file
from .search import locate


def _quick_stale(root: Path, conn: sqlite3.Connection) -> tuple[int, int, int]:
    files, _ = collect_files(root)
    rows = {
        row["path"]: row
        for row in conn.execute("SELECT path,size,mtime_ns,ctime_ns FROM files")
    }
    changed = 0
    missing = 0
    for rel, path in files.items():
        row = rows.get(rel)
        if row is None:
            changed += 1
            continue
        try:
            stat = path.stat()
        except OSError:
            missing += 1
            continue
        if (
            stat.st_size != row["size"]
            or stat.st_mtime_ns != row["mtime_ns"]
            or stat.st_ctime_ns != int(row["ctime_ns"] or 0)
        ):
            changed += 1
    removed = sum(1 for rel in rows if rel not in files)
    return changed, missing, removed


def status(root: Path, policy=None) -> dict:
    path = db_path(root)
    if not path.is_file():
        return {
            "ok": False,
            "exists": False,
            "database": str(path),
            "next": "python3 tools/atlas/atlas.py init",
            "tools": _tools(),
        }
    started = time.monotonic()
    conn = open_db(root, policy=policy)
    base = index_status(conn)
    checked = policy.check_staleness if policy is not None else True
    if checked:
        changed, missing, removed = _quick_stale(root, conn)
    else:
        changed = missing = removed = 0
    external = [
        dict(row) for row in conn.execute(
            "SELECT name,kind FROM external_deps ORDER BY name LIMIT 30"
        )
    ]
    counts = table_counts(conn)
    result = {
        "ok": base["compatible"] and changed == 0 and removed == 0,
        "exists": True,
        **base,
        "stale_estimate": changed,
        "staleness_checked": checked,
        "missing_files": missing,
        "removed_files": removed,
        "external_deps": external,
        "tools": _tools(),
        "elapsed_seconds": round(time.monotonic() - started, 3),
        "next": (
            "defasagem não verificada neste modo; confirme manualmente"
            if not checked
            else ("python3 tools/atlas/atlas.py update" if changed or removed else "índice atualizado")
        ),
        "counts": counts,
        "limitations": ["a estimativa usa tamanho/mtime; verify recalcula SHA-256"],
    }
    conn.close()
    return result


def _tools() -> dict:
    import sqlite3 as sql
    return {
        "sqlite": sql.sqlite_version,
        "fts5": _fts5_available(),
        "clangd": shutil.which("clangd"),
        "rg": shutil.which("rg"),
        "ctags": shutil.which("ctags"),
        "python": "stdlib",
        "semantic_cpp": "indisponível (sem clangd/rg/ctags); extração regex marcada como medium",
    }


def _fts5_available() -> bool:
    try:
        conn = sqlite3.connect(":memory:")
        conn.execute("CREATE VIRTUAL TABLE t USING fts5(x)")
        return True
    except sqlite3.DatabaseError:
        return False


def doctor(root: Path, strict: bool = False, policy=None) -> dict:
    issues: list[dict] = []
    path = db_path(root)
    if not path.is_file():
        return {
            "ok": False,
            "issues": [{"severity": "alta", "code": "index-missing",
                        "message": "índice ausente", "fix": "python3 tools/atlas/atlas.py init"}],
            "summary": {"alta": 1, "media": 0, "baixa": 0},
        }
    # FTS “integrity-check” needs a write-capable handle; hdd-critical blocks
    # this command in the CLI, so the handle is only opened when allowed.
    conn = open_db(root) if policy is None or policy.allow_integrity else open_db(root, policy=policy)
    reason = corrupt_or_missing_reason(conn)
    if reason:
        issues.append({
            "severity": "alta", "code": "index-corrupt",
            "message": f"índice inválido: {reason}",
            "fix": "python3 tools/atlas/atlas.py init --force",
        })
        conn.close()
        return {
            "ok": False,
            "issues": issues,
            "summary": {"alta": 1, "media": 0, "baixa": 0},
        }

    changed, missing, removed = _quick_stale(root, conn)
    if changed:
        issues.append({
            "severity": "media", "code": "index-stale",
            "message": f"{changed} arquivo(s) novo(s) ou alterado(s) sem indexar",
            "fix": "python3 tools/atlas/atlas.py update",
        })
    if removed:
        issues.append({
            "severity": "media", "code": "index-removed",
            "message": f"{removed} arquivo(s) removido(s) ainda no índice",
            "fix": "python3 tools/atlas/atlas.py update",
        })
    if schema_version(conn) != INDEX_VERSION:
        issues.append({
            "severity": "alta", "code": "schema-mismatch",
            "message": f"schema {schema_version(conn)} != {INDEX_VERSION}",
            "fix": "python3 tools/atlas/atlas.py init --force",
        })

    files = {row["path"] for row in conn.execute("SELECT path FROM files")}
    target_paths = {row["path"] for row in conn.execute("SELECT path FROM target_sources WHERE kind='source'")}
    external_target_paths = {path for path in target_paths if path.startswith("third_party/")}
    missing_sources = sorted(
        path for path in target_paths
        if path not in files and path not in external_target_paths
    )
    if missing_sources:
        issues.append({
            "severity": "media", "code": "cmake-source-missing",
            "message": f"{len(missing_sources)} fonte(s) CMake não indexada(s)",
            "examples": missing_sources[:8],
            "fix": "verifique CMake e execute update",
        })
    if external_target_paths:
        issues.append({
            "severity": "baixa", "code": "external-target-sources",
            "message": f"{len(external_target_paths)} fonte(s) externas (third_party) inventariadas, não indexadas como código próprio",
            "examples": sorted(external_target_paths)[:8],
            "fix": "comportamento esperado; use o inventário de dependências",
        })
    source_cpp = sorted(path for path in files if path.startswith("src/") and path.endswith((".cpp", ".cc", ".cxx")))
    untargeted = [path for path in source_cpp if path not in target_paths]
    if untargeted:
        issues.append({
            "severity": "baixa", "code": "source-without-target",
            "message": f"{len(untargeted)} .cpp de src/ sem target CMake direto",
            "examples": untargeted[:8],
            "fix": "confirme se é arquivo novo ou código órfão",
        })
    if conn.execute("SELECT COUNT(*) AS n FROM cmake_targets").fetchone()["n"] == 0:
        issues.append({
            "severity": "alta", "code": "cmake-graph-empty",
            "message": "nenhum target CMake indexado",
            "fix": "verifique CMakeLists.txt e execute init --force",
        })
    test_count = conn.execute("SELECT COUNT(*) AS n FROM tests").fetchone()["n"]
    conditional = conn.execute("SELECT COUNT(*) AS n FROM tests WHERE origin LIKE '%conditional%'").fetchone()["n"]
    if test_count == 0:
        issues.append({
            "severity": "media", "code": "tests-empty",
            "message": "nenhum teste CMake indexado",
            "fix": "execute update e verifique add_test no CMakeLists",
        })
    elif conditional:
        issues.append({
            "severity": "baixa", "code": "tests-conditional",
            "message": f"{conditional} teste(s) condicional(is) podem não estar registrados no build atual",
            "fix": "confirme com ctest --test-dir build -N",
        })
    unresolved = conn.execute(
        "SELECT COUNT(*) AS n FROM relations WHERE confidence!='confirmed' AND kind IN ('include','lua-require')"
    ).fetchone()["n"]
    if unresolved:
        examples = [
            row["target"] for row in conn.execute(
                "SELECT DISTINCT target FROM relations WHERE confidence!='confirmed' "
                "AND kind IN ('include','lua-require') ORDER BY target LIMIT 8"
            )
        ]
        issues.append({
            "severity": "baixa", "code": "includes-unresolved",
            "message": f"{unresolved} include(s)/require(s) sem caminho confirmado",
            "examples": examples,
            "fix": "pode ser header de sistema/third_party; confirme antes de tratar como erro",
        })

    duplicate = conn.execute(
        "SELECT qualified,kind,COUNT(DISTINCT file_id) AS files FROM symbols "
        "WHERE is_definition=1 GROUP BY qualified,kind HAVING files>1 ORDER BY qualified LIMIT 8"
    ).fetchall()
    if duplicate:
        issues.append({
            "severity": "baixa", "code": "symbols-homonymous",
            "message": f"{len(duplicate)} nome(s) definido(s) em mais de um arquivo",
            "examples": [f"{row['qualified']} ({row['kind']})" for row in duplicate],
            "fix": "normal para sobrecargas/namespaces; confirme com atlas.py symbol",
        })

    missing_contracts = [
        module for module in CONTRACT_MODULES
        if module in {row["module"] for row in conn.execute("SELECT DISTINCT module FROM files")}
        and not (root / "docs/atlas/module-contracts" / f"{module}.md").is_file()
    ]
    if missing_contracts:
        issues.append({
            "severity": "baixa", "code": "contracts-missing",
            "message": f"{len(missing_contracts)} módulo(s) sem contrato manual",
            "examples": missing_contracts,
            "fix": "adicione docs/atlas/module-contracts/<módulo>.md",
        })
    if not (root / "AGENTS.md").is_file():
        issues.append({
            "severity": "baixa", "code": "agents-missing",
            "message": "AGENTS.md ausente",
            "fix": "restaure a orientação universal de agentes",
        })
    if not glossary(root):
        issues.append({
            "severity": "baixa", "code": "glossary-missing",
            "message": "glossário do ATLAS ausente",
            "fix": "restaure tools/atlas/data/glossary.json",
        })

    summary = {"alta": 0, "media": 0, "baixa": 0}
    for issue in issues:
        summary[issue["severity"]] = summary.get(issue["severity"], 0) + 1
    conn.close()
    return {
        "ok": summary.get("alta", 0) == 0 and (not strict or not issues),
        "issues": issues,
        "summary": summary,
        "strict": strict,
        "limitations": ["diagnósticos estruturais não substituem compilação nem testes"],
    }


def verify(root: Path, policy=None) -> dict:
    path = db_path(root)
    checks: list[dict] = []
    if not path.is_file():
        return {
            "ok": False,
            "checks": [{"name": "index", "ok": False, "detail": "índice ausente"}],
        }
    # Same as doctor: the FTS integrity-check needs a writable handle.
    conn = open_db(root) if policy is None or policy.allow_integrity else open_db(root, policy=policy)
    try:
        try:
            integrity = conn.execute("PRAGMA integrity_check").fetchone()[0]
            checks.append({"name": "sqlite-integrity", "ok": integrity == "ok", "detail": integrity})
        except sqlite3.DatabaseError as exc:
            checks.append({"name": "sqlite-integrity", "ok": False, "detail": str(exc)})
        try:
            conn.execute("INSERT INTO file_fts(file_fts) VALUES('integrity-check')")
            checks.append({"name": "fts-integrity", "ok": True, "detail": "FTS5 íntegro"})
        except sqlite3.DatabaseError as exc:
            checks.append({"name": "fts-integrity", "ok": False, "detail": str(exc)})
        compatible = schema_version(conn) == INDEX_VERSION
        checks.append({
            "name": "schema-version", "ok": compatible,
            "detail": f"{schema_version(conn)} (esperado {INDEX_VERSION})",
        })
        files, _ = collect_files(root)
        rows = {row["path"]: row for row in conn.execute("SELECT path,sha256,size,mtime_ns FROM files")}
        actual = 0
        changed = []
        for rel, file_path in files.items():
            read = read_text_file(file_path)
            if read is None:
                continue
            actual += 1
            digest = hashlib.sha256(read[1]).hexdigest()
            row = rows.get(rel)
            if row is None or row["sha256"] != digest:
                changed.append(rel)
        removed = sorted(rel for rel in rows if rel not in files)
        checks.append({
            "name": "content-hashes", "ok": not changed and not removed,
            "detail": f"{actual} arquivos verificados; {len(changed)} alterado(s); {len(removed)} removido(s)",
            "examples": (changed + removed)[:8],
        })
        checks.append({
            "name": "file-count", "ok": len(rows) == actual and actual > 0,
            "detail": f"índice={len(rows)}, disco={actual}",
        })
        counts = table_counts(conn)
        checks.append({
            "name": "symbols-present", "ok": counts["symbols"] > 0,
            "detail": f"{counts['symbols']} símbolos, {counts['relations']} relações",
        })
        checks.append({
            "name": "cmake-graph", "ok": counts["cmake_targets"] > 0 and counts["tests"] > 0,
            "detail": f"{counts['cmake_targets']} targets, {counts['tests']} testes",
        })
        sample = conn.execute(
            "SELECT s.name FROM symbols s JOIN files f ON f.id=s.file_id "
            "WHERE f.area='source' AND s.kind IN ('class','struct','interface') "
            "AND length(s.name)>=4 ORDER BY s.id LIMIT 1"
        ).fetchone()
        if sample is None:
            sample = conn.execute(
                "SELECT s.name FROM symbols s JOIN files f ON f.id=s.file_id "
                "WHERE f.area='source' AND length(s.name)>=4 ORDER BY s.id LIMIT 1"
            ).fetchone()
        smoke_query = sample["name"] if sample else "test"
        located = locate(root, conn, smoke_query, limit=3)
        checks.append({
            "name": "query-smoke", "ok": bool(located.get("candidates")),
            "detail": f"{len(located.get('candidates', []))} módulo(s) retornado(s) para '{smoke_query}'",
        })
    finally:
        conn.close()
    return {
        "ok": all(check["ok"] for check in checks),
        "checks": checks,
        "limitations": ["verify valida o índice, não a compilação ou os testes da engine"],
    }


def _meminfo() -> dict[str, int]:
    """Read /proc/meminfo (a few KiB, no disk access)."""
    values: dict[str, int] = {}
    try:
        with Path("/proc/meminfo").open(encoding="utf-8") as handle:
            for line in handle:
                key, _, rest = line.partition(":")
                parts = rest.split()
                if parts and parts[0].isdigit():
                    values[key] = int(parts[0])
    except OSError:
        return {}
    return values


def _swap_used_mib() -> int | None:
    try:
        lines = Path("/proc/swaps").read_text(encoding="utf-8").splitlines()[1:]
    except OSError:
        return None
    used = 0
    for line in lines:
        columns = line.split()
        if len(columns) >= 4 and columns[3].isdigit():
            used += int(columns[3])
    return used // 1024


def health(root: Path, policy=None) -> dict:
    """Operational health of the ATLAS itself: mode, index and free memory.

    Never walks the project tree in a mode that cannot check staleness, never
    runs SMART/sector scans and never writes. A missing index is reported, not
    rebuilt.
    """
    policy = policy or io_mode.resolve()
    path = db_path(root)
    warnings: list[str] = []
    index: dict = {
        "exists": path.is_file(),
        "database": str(path),
        "staleness_checked": False,
    }
    if index["exists"]:
        try:
            stat = path.stat()
            index["bytes"] = stat.st_size
            index["modified_at"] = datetime.fromtimestamp(
                stat.st_mtime, timezone.utc
            ).isoformat(timespec="seconds")
            conn = open_db(root, policy=policy)
            try:
                meta = meta_dict(conn)
                index["updated_at"] = meta.get("updated_at")
                index["schema_version"] = schema_version(conn)
                index["compatible"] = index_is_compatible(conn)
                index["files"] = conn.execute("SELECT COUNT(*) AS n FROM files").fetchone()["n"]
                if policy.check_staleness:
                    changed, missing, removed = _quick_stale(root, conn)
                    index.update({
                        "staleness_checked": True,
                        "stale_estimate": changed,
                        "missing_files": missing,
                        "removed_files": removed,
                    })
                    if changed or removed:
                        warnings.append(
                            f"índice desatualizado: {changed} alterado(s), {removed} removido(s); "
                            "execute update quando autorizado"
                        )
                if not index["compatible"]:
                    warnings.append(
                        f"schema {index['schema_version']} incompatível; consultas podem falhar"
                    )
            finally:
                conn.close()
        except (OSError, sqlite3.DatabaseError, FileNotFoundError) as exc:
            index["error"] = str(exc)
            warnings.append(f"índice ilegível: {exc}")
    else:
        warnings.append("índice ausente; nenhuma varredura automática será iniciada")

    if not policy.check_staleness:
        warnings.append(
            f"modo {policy.name}: defasagem não verificada; o índice pode estar desatualizado"
        )

    mem = _meminfo()
    total_mib = mem.get("MemTotal", 0) // 1024
    available_mib = mem.get("MemAvailable", 0) // 1024
    swap_mib = _swap_used_mib()
    cache_enabled = bool(
        policy.allow_index_write and available_mib >= 512 and available_mib >= total_mib // 8
    )
    budget_mib = min(32, max(8, available_mib // 64)) if cache_enabled else 0
    if swap_mib and swap_mib > 256:
        warnings.append(
            f"swap em uso ({swap_mib} MiB): evite caches grandes e testes pesados"
        )

    return {
        "ok": not index.get("error") and (index["exists"] or not policy.read_only),
        "io_mode": policy.name,
        "read_only": policy.read_only,
        "policy": policy.as_dict(),
        "note": policy.note,
        "index": index,
        "memory": {
            "total_mib": total_mib,
            "available_mib": available_mib,
            "swap_used_mib": swap_mib,
        },
        "cache": {"enabled": cache_enabled, "budget_mib": budget_mib},
        "warnings": warnings,
        "limitations": [
            "health não é diagnóstico de hardware: não lê SMART nem testa setores",
            "leituras de páginas não residentes podem ocorrer em qualquer modo",
        ],
    }
