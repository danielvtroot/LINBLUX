"""Checkpoint runner: monitor changes, commit locally, push when possible."""

from __future__ import annotations

import fcntl
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path

from .config import BackupConfig
from .selection import Selection, select
from .state import BackupState, parse_time, utc_now
from .store import PushResult, Store, StoreError, SyncResult


class Logger:
    def __init__(self, quiet: bool = False) -> None:
        self.quiet = quiet

    def __call__(self, message: str) -> None:
        if self.quiet:
            return
        stamp = utc_now()
        print(f"{stamp} {message}", file=sys.stderr, flush=True)


@dataclass
class CycleResult:
    checked_at: str
    changed: list[str]
    checkpoint: str | None
    pushed: str | None
    pending: bool
    error: str | None

    def as_dict(self) -> dict:
        return {
            "checked_at": self.checked_at,
            "changed": self.changed,
            "checkpoint": self.checkpoint,
            "pushed": self.pushed,
            "pending": self.pending,
            "error": self.error,
        }


def default_state_dir() -> Path:
    override = os.environ.get("LINBLUX_BACKUP_STATE")
    if override:
        return Path(override).expanduser()
    xdg = os.environ.get("XDG_STATE_HOME")
    base = Path(xdg).expanduser() if xdg else Path.home() / ".local" / "state"
    return base / "linblux-backup"


class BackupRunner:
    def __init__(
        self,
        project: Path,
        config: BackupConfig,
        state_dir: Path | None = None,
        remote_url: str | None = None,
        logger: Logger | None = None,
    ) -> None:
        self.project = Path(project).resolve()
        self.config = config
        self.state_dir = Path(state_dir).resolve() if state_dir else default_state_dir()
        self.state_path = self.state_dir / "state.json"
        self.lock_path = self.state_dir / "backup.lock"
        self.log = logger or Logger()
        self.state = BackupState.load(self.state_path)
        self.remote_url = remote_url or self._resolve_remote()
        self.store = Store(self.state_dir, self.remote_url, config.branch, config.push_timeout_seconds)
        self._lock_handle = None

    # -- setup ----------------------------------------------------------
    def _resolve_remote(self) -> str:
        candidate = self.config.remote
        if "://" in candidate or candidate.startswith("git@") or candidate.startswith("/"):
            return candidate
        result = subprocess.run(
            ["git", "-C", str(self.project), "remote", "get-url", candidate],
            capture_output=True, check=False, timeout=30,
        )
        if result.returncode != 0 or not result.stdout.strip():
            raise StoreError(f"remote git '{candidate}' não encontrado no projeto")
        return result.stdout.decode().strip()

    def acquire_lock(self) -> bool:
        self.state_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
        handle = self.lock_path.open("a+")
        try:
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            handle.close()
            return False
        handle.seek(0)
        handle.truncate()
        handle.write(f"{os.getpid()} {utc_now()}\n")
        handle.flush()
        self._lock_handle = handle
        return True

    def daemon_running(self) -> bool:
        if not self.lock_path.exists():
            return False
        with self.lock_path.open("a+") as handle:
            try:
                fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            except OSError:
                return True
            fcntl.flock(handle.fileno(), fcntl.LOCK_UN)
            return False

    # -- work -----------------------------------------------------------
    def collect(self) -> Selection:
        return select(self.project, self.config)

    def due(self, now: datetime) -> bool:
        last = parse_time((self.state.last_checkpoint or {}).get("time"))
        if last is None:
            return True
        return (now - last).total_seconds() >= self.config.checkpoint_interval_seconds

    def _message(self, changes: SyncResult, checkpoint_time: str) -> str:
        paths = [*changes.added, *changes.modified, *changes.removed]
        listed = ", ".join(paths[:20])
        if len(paths) > 20:
            listed += f", … (+{len(paths) - 20})"
        return (
            f"backup: checkpoint {checkpoint_time} ({len(paths)} arquivos)\n\n"
            f"Projeto: {self.project.name}\n"
            f"Alterações: {changes.summary()}\n"
            f"Arquivos: {listed}\n"
        )

    def checkpoint(self, selection: Selection, changes: SyncResult, now: datetime) -> str | None:
        self.store.ensure()
        self.store.apply(selection, changes)
        commit = self.store.commit(changes, self._message(changes, utc_now()))
        if commit is None:
            return None
        self.state.last_checkpoint = {
            "hash": commit,
            "time": utc_now(),
            "added": len(changes.added),
            "modified": len(changes.modified),
            "removed": len(changes.removed),
            "files": len(selection.files),
        }
        self.state.manifest = {item.relative: item.digest for item in selection.files}
        self.state.skipped = selection.skipped
        self.state.save(self.state_path)
        self.log(f"checkpoint {commit[:12]} ({len(selection.files)} arquivos protegidos)")
        return commit

    def push(self, force: bool = False) -> PushResult:
        previous_attempt = parse_time(self.state.last_attempt)
        now = datetime.now(timezone.utc)
        if (
            not force
            and self.state.last_error is not None
            and previous_attempt is not None
            and (now - previous_attempt).total_seconds() < self.config.push_retry_seconds
        ):
            return PushResult(False, None, self.state.last_error)
        self.state.last_attempt = utc_now()
        result = self.store.push()
        if result.ok:
            self.state.last_push = {"hash": result.remote_hash, "time": utc_now(), "branch": self.config.branch}
            self.state.last_error = None
            self.log(f"push confirmado {result.remote_hash[:12]} → {self.config.branch}")
        else:
            self.state.last_error = result.error
            self.log(f"push pendente: {result.error}")
        self.state.save(self.state_path)
        return result

    def unprotected(self, selection: Selection) -> list[str]:
        changes = self.store.diff(selection)
        return sorted({*changes.added, *changes.modified, *changes.removed})

    def run_once(self, force: bool = False) -> CycleResult:
        now = datetime.now(timezone.utc)
        selection = self.collect()
        self.store.ensure()
        changes = self.store.diff(selection)
        checkpoint: str | None = None
        error: str | None = None
        if changes.any and (force or self.due(now)):
            try:
                checkpoint = self.checkpoint(selection, changes, now)
            except StoreError as failure:
                error = str(failure)
                self.state.last_error = error
                self.state.save(self.state_path)

        pushed: str | None = None
        if checkpoint is not None or self._unpushed():
            result = self.push(force=force or checkpoint is not None)
            if result.ok:
                pushed = result.remote_hash
            else:
                error = error or result.error

        head = self.store.head()
        last_push = (self.state.last_push or {}).get("hash")
        pending = bool(head) and head != last_push
        return CycleResult(
            checked_at=utc_now(),
            changed=self.unprotected(selection),
            checkpoint=checkpoint,
            pushed=pushed,
            pending=pending,
            error=error,
        )

    def _unpushed(self) -> bool:
        head = self.store.head()
        if not head:
            return False
        last_push = (self.state.last_push or {}).get("hash")
        return head != last_push

    def run_forever(self, stop_after: float | None = None) -> int:
        if not self.acquire_lock():
            self.log("outro daemon de backup já está ativo; encerrando")
            return 0
        started = time.monotonic()
        self.log(
            f"monitorando {self.project} → {self.config.branch} em {self.remote_url} "
            f"(checkpoint a cada {self.config.checkpoint_interval_seconds:.0f}s)"
        )
        while True:
            try:
                result = self.run_once()
                if result.error:
                    self.log(f"aviso: {result.error}")
            except (StoreError, OSError) as failure:
                self.state.last_error = str(failure)
                self.state.save(self.state_path)
                self.log(f"erro: {failure}")
            if stop_after is not None and time.monotonic() - started >= stop_after:
                return 0
            time.sleep(self.config.poll_interval_seconds)


def status_snapshot(runner: BackupRunner) -> dict:
    selection = runner.collect()
    head = runner.store.head()
    last_push = runner.state.last_push
    if head:
        changes = runner.store.diff(selection)
    else:
        changes = SyncResult([item.relative for item in selection.files], [], [])
    uncommitted = sorted({*changes.added, *changes.modified, *changes.removed})
    return {
        "project": str(runner.project),
        "branch": runner.config.branch,
        "remote": runner.remote_url,
        "running": runner.daemon_running(),
        "last_checkpoint": runner.state.last_checkpoint,
        "last_push": last_push,
        "pending_push": bool(head) and head != (last_push or {}).get("hash"),
        "local_head": head,
        "changes": {
            "added": changes.added,
            "modified": changes.modified,
            "removed": changes.removed,
        },
        "unprotected": uncommitted,
        "skipped": selection.skipped,
        "last_error": runner.state.last_error,
        "protected_files": len(selection.files) - len(
            [item for item in selection.files if item.relative in set(uncommitted)]
        ),
        "interval_seconds": runner.config.checkpoint_interval_seconds,
    }
