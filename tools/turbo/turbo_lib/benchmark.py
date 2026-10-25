"""Measurement records for TURBO.  No synthetic speed-up percentages."""

from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

from . import cache, config, inventory as inventory_mod


def save(root: Path, payload: dict) -> Path:
    directory = config.turbo_dir(root) / "benchmarks"
    directory.mkdir(parents=True, exist_ok=True)
    name = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + f"-{__import__('os').getpid()}.json"
    path = directory / name
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True), encoding="utf-8")
    return path


def base_payload(inventory: dict, plan: dict | None = None) -> dict:
    info = inventory_mod.summary(inventory)
    return {
        "generated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "inventory": info,
        "plan": {
            "mode": (plan or {}).get("mode"),
            "test_count": (plan or {}).get("test_count", 0),
            "graphics_count": (plan or {}).get("graphics_count", 0),
            "estimated_seconds": (plan or {}).get("estimated_seconds", 0),
            "build_targets": (plan or {}).get("build_targets", []),
        },
        "build_signature": cache.build_signature(Path(inventory["build_dir"])),
        "note": "tempos medidos nesta máquina; estimativas do histórico CTest não são execuções",
    }
