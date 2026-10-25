"""Persistent, conservative validation history for TURBO."""

from __future__ import annotations

import hashlib
import json
import os
import time
from datetime import datetime, timezone
from pathlib import Path

from . import config

PASSING = {"PASSED", "PASSED_NOW", "DOCS_OK"}
REUSABLE_STATUSES = {"PASSED", "PASSED_NOW", "DOCS_OK"}


class BusyError(RuntimeError):
    """Another TURBO process owns the repository run lock."""


def _now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def new_run_id() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") + f"-{os.getpid()}"


def history_dir(root: Path) -> Path:
    return config.turbo_dir(root) / "history"


def logs_dir(root: Path) -> Path:
    return config.turbo_dir(root) / "logs"


def state_path(root: Path) -> Path:
    return config.turbo_dir(root) / "state.json"


def latest_path(root: Path, mode: str | None = None) -> Path:
    return config.turbo_dir(root) / (f"last-{mode}.json" if mode else "last.json")


def build_signature(build_dir: Path) -> dict:
    cache = build_dir / "CMakeCache.txt"
    keys = (
        "CMAKE_GENERATOR:",
        "CMAKE_BUILD_TYPE:",
        "CMAKE_CXX_COMPILER:",
        "CMAKE_CXX_FLAGS:",
        "CMAKE_CXX_COMPILER_LAUNCHER:",
        "LINBLUX_ENABLE_VIDEO:",
    )
    values = {}
    if cache.is_file():
        try:
            for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
                for key in keys:
                    if line.startswith(key):
                        values[key] = line
                        break
        except OSError:
            pass
    payload = json.dumps(values, sort_keys=True).encode("utf-8")
    return {
        "hash": hashlib.sha256(payload).hexdigest(),
        "cache": str(cache),
        "values": values,
    }


def _atomic_write(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_suffix(path.suffix + f".tmp-{os.getpid()}")
    temp.write_text(json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=True), encoding="utf-8")
    os.replace(temp, path)


def save_result(root: Path, result: dict, plan: dict, *, keep_limit: int = config.HISTORY_LIMIT) -> Path:
    history_dir(root).mkdir(parents=True, exist_ok=True)
    _atomic_write(history_dir(root) / f"{result['run_id']}.json", result)
    _atomic_write(latest_path(root), result)
    _atomic_write(latest_path(root, result.get("mode")), result)
    # Prune old history by name (timestamp-prefixed), never touching logs.
    try:
        files = sorted(history_dir(root).glob("*.json"))
        for old in files[:-keep_limit]:
            old.unlink(missing_ok=True)
    except OSError:
        pass
    return history_dir(root) / f"{result['run_id']}.json"


def load_result(root: Path, mode: str | None = None) -> dict | None:
    for candidate in (latest_path(root, mode), latest_path(root)) if mode else (latest_path(root),):
        if not candidate.is_file():
            continue
        try:
            return json.loads(candidate.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
    return None


def load_history(root: Path, limit: int = 20) -> list[dict]:
    directory = history_dir(root)
    if not directory.is_dir():
        return []
    items = []
    for path in sorted(directory.glob("*.json"), reverse=True)[:limit]:
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            continue
        data["_path"] = str(path)
        items.append(data)
    return items


def write_state(root: Path, state: dict) -> None:
    _atomic_write(state_path(root), state)


def read_state(root: Path) -> dict | None:
    path = state_path(root)
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def can_reuse(
    last: dict | None,
    *,
    mode: str,
    fingerprint: str,
    selected: list[str],
    plan: dict,
) -> tuple[bool, str]:
    if not last:
        return False, "sem histórico"
    if last.get("status") not in REUSABLE_STATUSES:
        return False, f"último resultado não aprovado ({last.get('status')})"
    if last.get("mode") != mode:
        return False, "modo diferente"
    if last.get("fingerprint") != fingerprint:
        return False, "código/configuração mudou"
    if last.get("selected_names") != sorted(selected):
        return False, "seleção de testes diferente"
    if last.get("coverage") != "known":
        return False, "cobertura anterior não era conhecida"
    if last.get("timed_out"):
        return False, "execução anterior teve timeout"
    tests = plan.get("selected_tests", [])
    volatile = [test["name"] for test in tests if not test.get("reusable", False)]
    if volatile:
        return False, "há teste não determinístico/gráfico/vídeo: " + ", ".join(volatile[:4])
    return True, "mesmos arquivos, configuração, seleção e ferramentas"


def reused_result(last: dict, *, fingerprint: str, run_id: str) -> dict:
    result = json.loads(json.dumps(last))
    result["run_id"] = run_id
    result["status"] = "REUSED"
    result["reused"] = True
    result["reused_from"] = last.get("run_id")
    result["reused_at"] = _now()
    result["started_at"] = _now()
    result["finished_at"] = _now()
    result["duration_seconds"] = 0
    result["fingerprint"] = fingerprint
    result["message"] = "RESULTADO REUTILIZADO — não foi executado agora"
    for name, test in result.get("tests", {}).items():
        if test.get("status") == "PASSED":
            test["status_now"] = "REUSED"
            test["status"] = "REUSED"
    return result


class RunLock:
    """A process lock for build/test operations, distinct from the ATLAS cache."""

    def __init__(self, root: Path):
        self.path = config.turbo_dir(root) / "run.lock"
        self.handle = None

    def __enter__(self):
        import fcntl

        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.handle = self.path.open("a+", encoding="utf-8")
        try:
            fcntl.flock(self.handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as exc:
            self.handle.close()
            self.handle = None
            raise BusyError("outro TURBO está compilando/testando neste repositório") from exc
        self.handle.seek(0)
        self.handle.truncate()
        self.handle.write(f"pid={os.getpid()} started={_now()}\n")
        self.handle.flush()
        return self

    def __exit__(self, exc_type, exc, tb):
        if self.handle:
            try:
                import fcntl

                fcntl.flock(self.handle.fileno(), fcntl.LOCK_UN)
            except OSError:
                pass
            self.handle.close()
            self.handle = None
        return False


def age_seconds(timestamp: str | None) -> float | None:
    if not timestamp:
        return None
    try:
        parsed = datetime.fromisoformat(timestamp)
        if parsed.tzinfo is None:
            parsed = parsed.replace(tzinfo=timezone.utc)
        return max(0.0, (datetime.now(timezone.utc) - parsed).total_seconds())
    except ValueError:
        return None
