"""Generated documentation, benchmarks and real-question evaluation."""

from __future__ import annotations

import json
import resource
import shutil
import tempfile
import time
from pathlib import Path

from .config import CONTRACT_MODULES, should_index
from .db import open_db, table_counts
from .indexer import index_repository
from .search import locate


def write_generated_docs(root: Path) -> dict:
    conn = open_db(root)
    counts = table_counts(conn)
    modules = [dict(row) for row in conn.execute(
        "SELECT module,COUNT(*) AS files,SUM(line_count) AS lines FROM files "
        "GROUP BY module ORDER BY files DESC,module"
    )]
    targets = [dict(row) for row in conn.execute(
        "SELECT name,type FROM cmake_targets ORDER BY name"
    )]
    tests = [dict(row) for row in conn.execute(
        "SELECT name,command,target,source_path,origin FROM tests ORDER BY name"
    )]
    external = [dict(row) for row in conn.execute(
        "SELECT name,kind FROM external_deps ORDER BY name,kind"
    )]
    symbol_counts = [dict(row) for row in conn.execute(
        "SELECT kind,COUNT(*) AS n FROM symbols GROUP BY kind ORDER BY n DESC,kind"
    )]
    conn.close()
    out_dir = root / "docs/atlas/generated"
    out_dir.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Inventário gerado pelo ATLAS",
        "",
        "> Arquivo regenerável. Não edite manualmente; execute `python3 tools/atlas/atlas.py docs`.",
        "",
        "## Contagens",
        "",
    ]
    for key, value in counts.items():
        lines.append(f"- {key}: {value}")
    lines += ["", "## Módulos", "", "| Módulo | Arquivos | Linhas |", "| --- | ---: | ---: |"]
    lines += [f"| {row['module']} | {row['files']} | {row['lines']} |" for row in modules]
    lines += ["", "## Símbolos por tipo", ""]
    lines += [f"- {row['kind']}: {row['n']}" for row in symbol_counts]
    lines += ["", "## Targets CMake", ""]
    lines += [f"- `{row['name']}` ({row['type']})" for row in targets]
    lines += ["", "## Testes declarados no CMake", ""]
    for row in tests:
        source = f" — `{row['source_path']}`" if row["source_path"] else ""
        note = " (condicional)" if "conditional" in (row["origin"] or "") else ""
        lines.append(f"- `{row['name']}`{note}{source}")
    lines += ["", "## Dependências externas inventariadas", ""]
    lines += [f"- {row['name']} ({row['kind']})" for row in external]
    lines += ["", "## Contratos manuais esperados", ""]
    for module in CONTRACT_MODULES:
        path = root / "docs/atlas/module-contracts" / f"{module}.md"
        lines.append(f"- [{'x' if path.is_file() else ' '}] `{module}`")
    path = out_dir / "inventory.md"
    content = "\n".join(lines) + "\n"
    changed = True
    if path.is_file():
        try:
            changed = path.read_text(encoding="utf-8") != content
        except OSError:
            changed = True
    if changed:
        path.write_text(content, encoding="utf-8")
    return {"path": path.relative_to(root).as_posix(), "counts": counts, "changed": changed}


def _copy_indexable(source: Path, target: Path) -> int:
    copied = 0
    for path in source.rglob("*"):
        if not path.is_file():
            continue
        try:
            rel = path.relative_to(source)
        except ValueError:
            continue
        if not should_index(rel.as_posix(), path):
            continue
        destination = target / rel
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, destination)
        copied += 1
    return copied


def benchmark(root: Path, iterations: int = 5) -> dict:
    iterations = max(1, min(iterations, 20))
    work = Path(tempfile.mkdtemp(prefix="atlas-bench-", dir=root / ".atlas"))
    try:
        copied = _copy_indexable(root, work)
        start = time.perf_counter()
        first = index_repository(work, force=True, quiet=True)
        initial_ms = (time.perf_counter() - start) * 1000
        editable = sorted(
            path for path in work.rglob("*")
            if path.is_file() and path.suffix in {".cpp", ".h", ".py", ".md"}
        )
        if not editable:
            raise RuntimeError("nenhum arquivo editável para o benchmark")
        editable[0].write_text(editable[0].read_text(encoding="utf-8") + "\n// atlas bench\n", encoding="utf-8")
        start = time.perf_counter()
        index_repository(work, quiet=True)
        one_ms = (time.perf_counter() - start) * 1000
        for path in editable[1:6]:
            path.write_text(path.read_text(encoding="utf-8") + "\n// atlas bench multi\n", encoding="utf-8")
        start = time.perf_counter()
        index_repository(work, quiet=True)
        five_ms = (time.perf_counter() - start) * 1000
        conn = open_db(work)
        counts = table_counts(conn)
        db_bytes = sum(
            path.stat().st_size
            for path in (work / ".atlas").glob("index.sqlite*")
            if path.is_file()
        )
        queries = _eval_questions(root) or [{"query": "camera personagem", "expect_modules": []}]
        samples = []
        start_all = time.perf_counter()
        for _ in range(iterations):
            for item in queries:
                started = time.perf_counter()
                result = locate(work, conn, item["query"], limit=5)
                samples.append({
                    "query": item["query"],
                    "ms": (time.perf_counter() - started) * 1000,
                    "candidates": len(result.get("candidates", [])),
                })
        total_ms = (time.perf_counter() - start_all) * 1000
        conn.close()
        peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        real_conn = open_db(root)
        real_result = locate(root, real_conn, "camera personagem", limit=5)
        baseline_chars = 0
        for candidate in real_result.get("candidates", [])[:3]:
            for file in candidate.get("files", [])[:2]:
                try:
                    baseline_chars += (root / file["path"]).stat().st_size
                except OSError:
                    pass
        real_conn.close()
        atlas_chars = len(json.dumps(real_result, ensure_ascii=False))
        return {
            "ok": True,
            "copied_files": copied,
            "query_index": "cópia temporária completa dos arquivos indexáveis",
            "initial_index_ms": round(initial_ms, 2),
            "update_one_file_ms": round(one_ms, 2),
            "update_five_files_ms": round(five_ms, 2),
            "query_count": len(samples),
            "query_average_ms": round(total_ms / max(len(samples), 1), 2),
            "query_max_ms": round(max(item["ms"] for item in samples), 2),
            "index_bytes": db_bytes,
            "peak_rss_kib": peak,
            "counts": counts,
            "first_index": first,
            "token_economy": {
                "method": "caracteres da saída JSON do locate vs soma dos arquivos candidatos",
                "atlas_output_chars": atlas_chars,
                "candidate_file_chars": baseline_chars,
                "ratio": round(atlas_chars / baseline_chars, 4) if baseline_chars else None,
            },
            "limitations": [
                "medição local em Xvfb/CPU não representa GPU",
                "caracteres não são tokens de um modelo específico",
            ],
        }
    finally:
        shutil.rmtree(work, ignore_errors=True)


def _eval_questions(root: Path) -> list[dict]:
    path = root / "tools/atlas/data/eval_questions.json"
    if not path.is_file():
        return []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return []
    return [item for item in data if isinstance(item, dict) and item.get("query")]


def evaluate(root: Path) -> dict:
    questions = _eval_questions(root)
    if not questions:
        return {"ok": False, "error": "tools/atlas/data/eval_questions.json ausente ou vazio"}
    conn = open_db(root)
    rows = []
    top1 = top3 = module_any = symbol_hits = 0
    file_hits = {1: 0, 3: 0, 5: 0}
    file_mrr = 0.0
    file_evaluated = 0
    for item in questions:
        result = locate(root, conn, item["query"], limit=10)
        candidates = result.get("candidates", [])
        modules = [candidate["module"] for candidate in candidates]
        symbols = []
        for candidate in candidates:
            symbols.extend(symbol["qualified"] for symbol in candidate.get("symbols", []))
        expected = set(item.get("expect_modules", []))
        expected_symbols = set(item.get("expect_symbols", []))
        rank = next((index + 1 for index, module in enumerate(modules) if module in expected), None)

        def symbol_matches(expected_name: str, value: str) -> bool:
            return (
                value == expected_name
                or value.endswith("::" + expected_name)
                or value.split("::")[-1] == expected_name
            )

        symbol_rank = next(
            (
                index + 1
                for index, value in enumerate(symbols)
                if any(symbol_matches(expected_name, value) for expected_name in expected_symbols)
            ),
            None,
        )
        if rank == 1:
            top1 += 1
        if rank and rank <= 3:
            top3 += 1
        if rank:
            module_any += 1
        if symbol_rank:
            symbol_hits += 1
        expected_files = set(item.get("expect_files", []))
        returned_files = [entry["path"] for entry in result.get("files", [])]
        file_rank = next(
            (index + 1 for index, path in enumerate(returned_files) if path in expected_files),
            None,
        )
        if expected_files:
            file_evaluated += 1
            for cutoff in file_hits:
                if file_rank and file_rank <= cutoff:
                    file_hits[cutoff] += 1
            if file_rank:
                file_mrr += 1.0 / file_rank
        rows.append({
            "id": item.get("id"),
            "query": item["query"],
            "expected_modules": sorted(expected),
            "returned_modules": modules,
            "module_rank": rank,
            "expected_symbols": sorted(expected_symbols),
            "symbol_rank": symbol_rank,
            "expected_files": sorted(expected_files),
            "returned_files": returned_files[:5],
            "file_rank": file_rank,
            "confidence": result.get("confidence"),
        })
    conn.close()
    total = len(questions)
    file_total = file_evaluated or 0
    return {
        "ok": True,
        "total": total,
        "top1": top1,
        "top3": top3,
        "module_hits": module_any,
        "symbol_hits": symbol_hits,
        "top1_rate": round(top1 / total, 4),
        "top3_rate": round(top3 / total, 4),
        "module_hit_rate": round(module_any / total, 4),
        "symbol_hit_rate": round(symbol_hits / total, 4),
        "file_evaluated": file_total,
        "file_r1": file_hits[1],
        "file_r3": file_hits[3],
        "file_r5": file_hits[5],
        "file_r1_rate": round(file_hits[1] / file_total, 4) if file_total else 0.0,
        "file_r3_rate": round(file_hits[3] / file_total, 4) if file_total else 0.0,
        "file_r5_rate": round(file_hits[5] / file_total, 4) if file_total else 0.0,
        "file_mrr": round(file_mrr / file_total, 4) if file_total else 0.0,
        "questions": rows,
        "method": (
            "ranking determinístico do locate; módulo e arquivo esperados definidos "
            "manualmente e verificados contra o índice"
        ),
        "limitations": [
            "amostra mantida manualmente; não representa precisão universal",
            "Recall/MRR de arquivo só conta perguntas com expect_files",
            "os arquivos esperados são relacionados verificados, não causas comprovadas",
        ],
    }
