"""Conservative change discovery for TURBO.

The worktree is the source of truth for FAST/IMPACT.  Build, cache and
assistant directories are deliberately ignored here so TURBO never treats its
own logs as source changes.
"""

from __future__ import annotations

import hashlib
import os
import subprocess
from pathlib import Path

from . import config


class ChangeError(RuntimeError):
    pass


def _git(root: Path, *args: str, timeout: int = 30) -> bytes:
    try:
        result = subprocess.run(
            ["git", "-C", str(root), *args],
            capture_output=True,
            timeout=timeout,
            check=False,
        )
    except (OSError, subprocess.SubprocessError) as exc:
        raise ChangeError(f"falha ao executar git: {exc}") from exc
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).decode("utf-8", "replace").strip()
        raise ChangeError(f"git {' '.join(args)} falhou: {detail}")
    return result.stdout


def _split_z(raw: bytes) -> list[str]:
    return [part.decode("utf-8", "surrogateescape") for part in raw.split(b"\0") if part]


def head(root: Path) -> str | None:
    try:
        return _git(root, "rev-parse", "HEAD").decode("ascii", "replace").strip() or None
    except ChangeError:
        return None


def status_entries(root: Path) -> dict[str, str]:
    """Return path -> porcelain status, including staged and untracked files."""
    raw = _git(root, "status", "--porcelain=v1", "-z", "--untracked-files=all")
    parts = _split_z(raw)
    entries: dict[str, str] = {}
    index = 0
    while index < len(parts):
        value = parts[index]
        if len(value) < 4:
            index += 1
            continue
        status = value[:2]
        path = value[3:]
        entries[path] = status
        # Renames/copies carry the original path in the next NUL field.
        if status[0] in {"R", "C"} and index + 1 < len(parts):
            index += 1
        index += 1
    return entries


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as handle:
            for chunk in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(chunk)
    except OSError:
        return "unreadable"
    return digest.hexdigest()


def current_files(root: Path) -> list[dict]:
    changed = _git(root, "diff", "--name-only", "-z", "HEAD")
    untracked = _git(root, "ls-files", "--others", "--exclude-standard", "-z")
    statuses = status_entries(root)
    paths = sorted({*_split_z(changed), *_split_z(untracked)})
    result = []
    for rel in paths:
        normalized = rel.replace(os.sep, "/")
        if config.ignored_change(normalized):
            continue
        path = (root / normalized).resolve()
        try:
            path.relative_to(root.resolve())
        except ValueError:
            continue
        exists = path.is_file()
        result.append(
            {
                "path": normalized,
                "status": statuses.get(normalized, "??" if exists else "D "),
                "exists": exists,
                "module": config.module_for_path(normalized),
                "sha256": file_sha256(path) if exists else None,
                "bytes": path.stat().st_size if exists else 0,
            }
        )
    return result


def summarize(root: Path, files: list[dict] | None = None) -> dict:
    files = current_files(root) if files is None else files
    modules = sorted({item["module"] for item in files})
    docs = [item for item in files if config.is_doc_path(item["path"])]
    code = [item for item in files if not config.is_doc_path(item["path"])]
    return {
        "head": head(root),
        "path": str(root),
        "files": files,
        "count": len(files),
        "modules": modules,
        "docs": [item["path"] for item in docs],
        "code": [item["path"] for item in code],
        "docs_only": bool(files) and not code,
        "clean": not files,
    }


def fingerprint(root: Path, extra: dict | None = None) -> str:
    """Hash the worktree state that can change a validation result."""
    summary = summarize(root)
    payload = {
        "head": summary["head"],
        "files": [
            {
                "path": item["path"],
                "exists": item["exists"],
                "sha256": item["sha256"],
            }
            for item in summary["files"]
        ],
        "extra": extra or {},
    }
    import json

    return hashlib.sha256(json.dumps(payload, sort_keys=True).encode("utf-8")).hexdigest()
