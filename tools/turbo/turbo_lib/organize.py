"""Evidence-based structural audit.  It never moves or deletes files."""

from __future__ import annotations

import hashlib
import os
import re
import subprocess
from collections import defaultdict
from pathlib import Path

from . import config

TRACKED_TEXT_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".lua",
    ".py",
    ".md",
    ".json",
    ".txt",
    ".cmake",
    ".sh",
    ".yml",
    ".yaml",
    ".toml",
    ".xml",
}
GENERATED_MARKERS = (
    "__pycache__/",
    "/__pycache__/",
    ".pyc",
    "build/",
    "build-",
    "Testing/",
    ".atlas/",
    ".turbo/",
)
ABSOLUTE_RE = re.compile(r"(?<![\w.])/(?:home|tmp|Users)/[^\s\"'<>]+")
MAX_SCAN_FILE = 2 * 1024 * 1024


def _git_files(root: Path) -> list[str]:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            capture_output=True,
            timeout=30,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return []
    if result.returncode != 0:
        return []
    return [part.decode("utf-8", "surrogateescape") for part in result.stdout.split(b"\0") if part]


def _text_file(path: Path) -> bool:
    if path.suffix.lower() not in TRACKED_TEXT_SUFFIXES:
        return False
    try:
        if path.stat().st_size > MAX_SCAN_FILE:
            return False
        chunk = path.read_bytes()[:4096]
    except OSError:
        return False
    return b"\0" not in chunk


def _hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _module_sizes(inventory: dict | None) -> list[dict]:
    if not inventory:
        return []
    by_module = defaultdict(lambda: {"files": 0, "lines": 0})
    for row in inventory.get("atlas_files", []):
        module = row.get("module") or "root"
        by_module[module]["files"] += 1
        by_module[module]["lines"] += int(row.get("line_count") or 0)
    return sorted(
        ({"module": module, **values} for module, values in by_module.items()),
        key=lambda item: item["lines"],
        reverse=True,
    )


def audit(root: Path, inventory: dict | None = None) -> dict:
    tracked = _git_files(root)
    tracked_set = set(tracked)
    generated = []
    for rel in tracked:
        normalized = rel.replace(os.sep, "/")
        if any(marker in normalized for marker in GENERATED_MARKERS):
            generated.append(rel)
    # Duplicate candidates are only reported for tracked, reasonably-sized
    # text files.  Byte-identical duplicates are evidence, not proof of no use.
    by_hash: dict[str, list[str]] = defaultdict(list)
    scanned = 0
    for rel in tracked:
        if not rel.startswith(("src/", "tests/", "assets/", "docs/", "scripts/", "tools/")):
            continue
        path = root / rel
        if not path.is_file() or not _text_file(path):
            continue
        try:
            by_hash[_hash(path)].append(rel)
        except OSError:
            continue
        scanned += 1
        if scanned >= 5000:
            break
    duplicates = [
        sorted(group)
        for group in by_hash.values()
        if len(group) > 1 and not all(item.startswith("tests/") and "fixture" in item for item in group)
    ]
    duplicates.sort(key=lambda group: (-len(group), group[0]))
    absolute_paths = []
    scan_prefixes = ("src/", "tests/", "tools/", "scripts/")
    scan_names = {"CMakeLists.txt", "CMakePresets.json"}
    for rel in tracked:
        if not (rel in scan_names or rel.startswith(scan_prefixes)):
            continue
        path = root / rel
        if not _text_file(path):
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for match in ABSOLUTE_RE.findall(text):
            absolute_paths.append({"path": rel, "reference": match})
            if len(absolute_paths) >= 30:
                break
        if len(absolute_paths) >= 30:
            break
    modules = _module_sizes(inventory)
    large = [item for item in modules if item["lines"] >= 2500]
    missing_tests = (inventory or {}).get("missing_tests", [])
    root_dirs = sorted(
        item.name for item in root.iterdir() if item.is_dir() and not item.name.startswith(".git")
    )
    findings = []
    if generated:
        findings.append(
            {
                "severity": "high" if any(item.startswith("build") for item in generated) else "medium",
                "kind": "generated-files-tracked",
                "evidence": generated[:20],
                "recommendation": "remover do Git somente após confirmar que são gerados; adicionar ao .gitignore",
            }
        )
    if duplicates:
        findings.append(
            {
                "severity": "low",
                "kind": "duplicate-candidates",
                "evidence": duplicates[:10],
                "recommendation": "revisar uso caso a caso; conteúdo igual não prova ausência de consumidores",
            }
        )
    if absolute_paths:
        findings.append(
            {
                "severity": "medium",
                "kind": "absolute-paths",
                "evidence": absolute_paths[:15],
                "recommendation": "usar caminhos relativos à raiz ou variáveis de runtime",
            }
        )
    if large:
        findings.append(
            {
                "severity": "low",
                "kind": "large-modules",
                "evidence": large[:8],
                "recommendation": "considerar divisão somente após análise de responsabilidades e testes",
            }
        )
    if missing_tests:
        findings.append(
            {
                "severity": "low",
                "kind": "conditional-tests",
                "evidence": missing_tests[:12],
                "recommendation": "testes condicionais não estão registrados neste build; não os trate como executados",
            }
        )
    return {
        "root": str(root),
        "tracked_files": len(tracked_set),
        "root_dirs": root_dirs,
        "scanned_text_files": scanned,
        "generated_tracked": generated[:50],
        "duplicate_candidates": duplicates[:20],
        "absolute_paths": absolute_paths[:30],
        "large_modules": large[:12],
        "missing_tests": missing_tests,
        "findings": findings,
        "apply_supported": False,
        "note": "auditoria somente leitura; nenhum arquivo foi movido ou removido",
    }
