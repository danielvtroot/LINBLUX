"""Configuration loading for the LINBLUX automatic backup."""

from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path

TOOL_DIR = Path(__file__).resolve().parent.parent
DEFAULT_CONFIG_PATH = TOOL_DIR / "config.json"


@dataclass(frozen=True)
class BackupConfig:
    """Explicit backup policy. Nothing outside ``include`` is ever protected."""

    branch: str
    remote: str
    include: tuple[str, ...]
    exclude: tuple[str, ...]
    max_file_bytes: int
    max_total_bytes: int
    checkpoint_interval_seconds: float
    poll_interval_seconds: float
    push_retry_seconds: float
    push_timeout_seconds: float
    secret_scan: bool

    def to_dict(self) -> dict:
        return {
            "branch": self.branch,
            "remote": self.remote,
            "include": list(self.include),
            "exclude": list(self.exclude),
            "max_file_bytes": self.max_file_bytes,
            "max_total_bytes": self.max_total_bytes,
            "checkpoint_interval_seconds": self.checkpoint_interval_seconds,
            "poll_interval_seconds": self.poll_interval_seconds,
            "push_retry_seconds": self.push_retry_seconds,
            "push_timeout_seconds": self.push_timeout_seconds,
            "secret_scan": self.secret_scan,
        }


def user_config_path() -> Path | None:
    """Optional per-user override, never stored inside the project."""
    override = os.environ.get("LINBLUX_BACKUP_CONFIG")
    if override:
        return Path(override).expanduser()
    xdg = os.environ.get("XDG_CONFIG_HOME")
    base = Path(xdg).expanduser() if xdg else Path.home() / ".config"
    return base / "linblux" / "backup.json"


def _read_json(path: Path) -> dict:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return {}
    except (OSError, ValueError) as error:
        raise SystemExit(f"configuração inválida em {path}: {error}") from error
    if not isinstance(data, dict):
        raise SystemExit(f"configuração inválida em {path}: esperado um objeto JSON")
    return data


def load_config(path: Path | None = None) -> BackupConfig:
    """Base config from the repository, merged with an optional user override."""
    merged = _read_json(path or DEFAULT_CONFIG_PATH)
    override_path = user_config_path()
    if override_path is not None and override_path.exists():
        merged = {**merged, **_read_json(override_path)}

    base = _read_json(DEFAULT_CONFIG_PATH)
    def value(key):
        return merged.get(key, base[key])

    include = value("include")
    exclude = value("exclude")
    if not isinstance(include, list) or not include:
        raise SystemExit("configuração inválida: 'include' deve ser uma lista não vazia")
    if not isinstance(exclude, list):
        raise SystemExit("configuração inválida: 'exclude' deve ser uma lista")

    config = BackupConfig(
        branch=str(value("branch")),
        remote=str(value("remote")),
        include=tuple(str(item) for item in include),
        exclude=tuple(str(item) for item in exclude),
        max_file_bytes=int(value("max_file_bytes")),
        max_total_bytes=int(value("max_total_bytes")),
        checkpoint_interval_seconds=float(value("checkpoint_interval_seconds")),
        poll_interval_seconds=float(value("poll_interval_seconds")),
        push_retry_seconds=float(value("push_retry_seconds")),
        push_timeout_seconds=float(value("push_timeout_seconds")),
        secret_scan=bool(value("secret_scan")),
    )
    if (config.max_file_bytes <= 0 or config.max_total_bytes < config.max_file_bytes or
            min(config.checkpoint_interval_seconds, config.poll_interval_seconds,
                config.push_retry_seconds, config.push_timeout_seconds) <= 0):
        raise SystemExit("configuração inválida: limites e intervalos devem ser positivos")
    return config
