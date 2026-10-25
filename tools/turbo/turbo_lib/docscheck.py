"""Cheap validation for documentation and JSON-only changes."""

from __future__ import annotations

import json
import re
from pathlib import Path

from . import config

LINK_RE = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")


def _check_markdown(path: Path, root: Path) -> list[str]:
    errors = []
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return [f"{path}: não foi possível ler ({exc})"]
    for raw in LINK_RE.findall(text):
        target = raw.strip().split("#", 1)[0].split("?", 1)[0]
        if not target or target.startswith(("http://", "https://", "mailto:", "data:")):
            continue
        if target.startswith("/"):
            resolved = root / target.lstrip("/")
        else:
            resolved = (path.parent / target).resolve()
        try:
            resolved.relative_to(root.resolve())
        except ValueError:
            errors.append(f"{path}: link sai do repositório: {raw}")
            continue
        if not resolved.exists():
            errors.append(f"{path}: link interno inexistente: {raw}")
    return errors


def check_changed(root: Path, changed: list[dict]) -> dict:
    errors = []
    checked = []
    for item in changed:
        rel = item["path"]
        path = root / rel
        if not path.is_file():
            continue
        checked.append(rel)
        if rel.lower().endswith((".md", ".markdown")):
            errors.extend(_check_markdown(path, root))
        elif rel.lower().endswith(".json"):
            try:
                json.loads(path.read_text(encoding="utf-8"))
            except (OSError, ValueError) as exc:
                errors.append(f"{rel}: JSON inválido: {exc}")
    return {
        "status": "FAILED" if errors else "DOCS_OK",
        "checked": checked,
        "errors": errors[:20],
        "error_count": len(errors),
    }
