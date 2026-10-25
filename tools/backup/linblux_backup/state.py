"""Durable backup state (JSON, atomic writes, owner-only permissions)."""

from __future__ import annotations

import json
import os
import tempfile
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_time(value: str | None) -> datetime | None:
    if not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None


@dataclass
class BackupState:
    last_checkpoint: dict | None = None
    last_push: dict | None = None
    last_attempt: str | None = None
    last_error: str | None = None
    manifest: dict[str, str] = field(default_factory=dict)
    skipped: list[dict] = field(default_factory=list)

    @classmethod
    def load(cls, path: Path) -> "BackupState":
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (FileNotFoundError, OSError, ValueError):
            return cls()
        if not isinstance(data, dict):
            return cls()
        return cls(
            last_checkpoint=data.get("last_checkpoint"),
            last_push=data.get("last_push"),
            last_attempt=data.get("last_attempt"),
            last_error=data.get("last_error"),
            manifest=dict(data.get("manifest") or {}),
            skipped=list(data.get("skipped") or []),
        )

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
        payload = json.dumps(self.to_dict(), indent=2, sort_keys=True) + "\n"
        handle, temporary = tempfile.mkstemp(dir=str(path.parent), prefix=".state-", suffix=".tmp")
        try:
            with os.fdopen(handle, "w", encoding="utf-8") as stream:
                stream.write(payload)
                stream.flush()
                os.fsync(stream.fileno())
            os.chmod(temporary, 0o600)
            os.replace(temporary, path)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)

    def to_dict(self) -> dict:
        return {
            "version": 1,
            "last_checkpoint": self.last_checkpoint,
            "last_push": self.last_push,
            "last_attempt": self.last_attempt,
            "last_error": self.last_error,
            "manifest": self.manifest,
            "skipped": self.skipped,
        }
