"""Isolated git store for checkpoint commits.

The store is a standalone repository under the backup state directory, with its
own index and branch. The developer's repository is only read (``git ls-files``)
and never receives commits or index writes.

Pushes are plain ``git push`` of ``HEAD:refs/heads/<branch>``. A rejected push
is reconciled with ``fetch`` + ``rebase``; ``--force`` is never used.
"""

from __future__ import annotations

import os
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path

from .selection import Selection, sha256_file


@dataclass
class SyncResult:
    added: list[str]
    modified: list[str]
    removed: list[str]

    @property
    def any(self) -> bool:
        return bool(self.added or self.modified or self.removed)

    def summary(self) -> str:
        return f"+{len(self.added)} ~{len(self.modified)} -{len(self.removed)}"


@dataclass
class PushResult:
    ok: bool
    remote_hash: str | None
    error: str | None


class StoreError(RuntimeError):
    pass


class Store:
    AUTHOR_NAME = "LINBLUX Auto Backup"
    AUTHOR_EMAIL = "auto-backup@linblux.invalid"

    def __init__(self, state_dir: Path, remote_url: str, branch: str, push_timeout: float = 60.0) -> None:
        if branch in ("main", "master") or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_/-]*", branch) or ".." in branch:
            raise StoreError("branch de backup inválido ou reservado ao desenvolvimento")
        self.state_dir = Path(state_dir)
        self.work = self.state_dir / "work"
        self.remote_url = remote_url
        self.branch = branch
        self.push_timeout = push_timeout

    # -- git plumbing ---------------------------------------------------
    def _env(self) -> dict[str, str]:
        env = os.environ.copy()
        for name in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_PREFIX"):
            env.pop(name, None)
        env["GIT_TERMINAL_PROMPT"] = "0"
        env["GIT_ASKPASS"] = "/bin/false"
        env["GIT_OPTIONAL_LOCKS"] = "0"
        env["LC_ALL"] = "C"
        return env

    def _git(self, *args: str, timeout: float = 60.0) -> subprocess.CompletedProcess:
        try:
            return subprocess.run(
                ["git", "-C", str(self.work), *args],
                capture_output=True,
                env=self._env(),
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            raise StoreError(f"git {args[0]} excedeu {timeout:.0f}s") from error

    def _identity(self) -> list[str]:
        return [
            "-c", f"user.name={self.AUTHOR_NAME}",
            "-c", f"user.email={self.AUTHOR_EMAIL}",
            "-c", "commit.gpgsign=false",
            "-c", "core.hooksPath=/dev/null",
        ]

    # -- lifecycle ------------------------------------------------------
    def exists(self) -> bool:
        return (self.work / ".git").exists()

    def ensure(self) -> None:
        self.state_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
        if self.work.is_symlink() or (self.work / ".git").is_symlink() or (self.work / ".git").is_file():
            raise StoreError("repositório de backup deve ser independente, sem links/worktrees")
        if not self.exists():
            subprocess.run(
                ["git", "init", "-q", f"--initial-branch={self.branch}", str(self.work)],
                capture_output=True, check=True, env=self._env(),
            )
        else:
            current = self._git("symbolic-ref", "--quiet", "--short", "HEAD")
            if current.returncode == 0 and current.stdout.decode().strip() != self.branch:
                raise StoreError("branch inesperado no repositório isolado; histórico preservado")
        remotes = self._git("remote").stdout.decode().split()
        if "origin" not in remotes:
            self._git("remote", "add", "origin", self.remote_url)
        else:
            existing = self._git("remote", "get-url", "origin").stdout.decode().strip()
            if existing != self.remote_url:
                self._git("remote", "set-url", "origin", self.remote_url)
        self._git("config", "push.default", "nothing")
        self._git("config", "advice.detachedHead", "false")
        self._git("config", "core.hooksPath", "/dev/null")
        # A fresh machine must extend the existing backup history, not create
        # an unrelated root and then fail its first non-forced push.
        if not self.head() and self.remote_head() and self._fetch_remote():
            checked = self._git("checkout", "-q", "-B", self.branch, f"origin/{self.branch}")
            if checked.returncode:
                raise StoreError("não foi possível abrir o histórico remoto do backup")

    def head(self) -> str | None:
        if not self.exists():
            return None
        result = self._git("rev-parse", "--verify", "HEAD")
        return result.stdout.decode().strip() if result.returncode == 0 else None

    # -- content --------------------------------------------------------
    def tracked(self) -> set[str]:
        if not self.exists():
            return set()
        result = self._git("ls-files", "-z")
        if result.returncode != 0:
            return set()
        return {name for name in result.stdout.decode("utf-8", "surrogateescape").split("\0") if name}

    def diff(self, selection: Selection) -> SyncResult:
        wanted = selection.by_relative()
        current = self.tracked()
        added: list[str] = []
        modified: list[str] = []
        for relative, item in wanted.items():
            if relative not in current:
                added.append(relative)
                continue
            target = self.work / relative
            if target.resolve().is_relative_to(self.work) is False:
                raise StoreError(f"link externo recusado no repositório isolado: {relative}")
            if not target.is_file() or sha256_file(target) != item.digest:
                modified.append(relative)
        removed = sorted(current - set(wanted))
        return SyncResult(sorted(added), sorted(modified), removed)

    def apply(self, selection: Selection, changes: SyncResult) -> None:
        for relative in [*changes.added, *changes.modified]:
            item = selection.by_relative()[relative]
            target = self.work / relative
            if target.parent.resolve() != target.parent:
                raise StoreError(f"diretório com link recusado: {relative}")
            target.parent.mkdir(parents=True, exist_ok=True)
            # Never follow a symlink inherited from an existing remote tree.
            if target.parent.resolve() != target.parent or target.is_symlink():
                raise StoreError(f"link recusado no repositório isolado: {relative}")
            target.write_bytes(item.data)
            target.chmod(0o755 if item.mode & 0o111 else 0o644)
        for relative in changes.removed:
            target = self.work / relative
            if target.parent.resolve() != target.parent:
                raise StoreError(f"remoção por diretório com link recusada: {relative}")
            if target.exists():
                target.unlink()
            parent = target.parent
            while parent != self.work:
                try:
                    parent.rmdir()
                except OSError:
                    break
                parent = parent.parent

    def commit(self, changes: SyncResult, message: str) -> str | None:
        paths = [*changes.added, *changes.modified, *changes.removed]
        if not paths:
            return self.head()
        staged = self._git("add", "--", *paths)
        if staged.returncode != 0:
            raise StoreError(staged.stderr.decode(errors="replace").strip())
        committed = self._git(*self._identity(), "commit", "-q", "-m", message)
        if committed.returncode != 0:
            output = (committed.stdout + committed.stderr).decode(errors="replace").strip()
            if "nothing to commit" in output:
                return self.head()
            raise StoreError(output)
        return self.head()

    # -- remote ---------------------------------------------------------
    def remote_head(self) -> str | None:
        result = self._git("ls-remote", "origin", f"refs/heads/{self.branch}", timeout=45)
        if result.returncode != 0:
            return None
        for line in result.stdout.decode(errors="replace").splitlines():
            parts = line.split()
            if len(parts) == 2:
                return parts[0]
        return None

    def _fetch_remote(self) -> bool:
        fetched = self._git(
            "fetch", "--quiet", "origin", f"+refs/heads/{self.branch}:refs/remotes/origin/{self.branch}",
            timeout=max(self.push_timeout, 60.0),
        )
        return fetched.returncode == 0

    def _rebase_onto_remote(self) -> str | None:
        if not self._fetch_remote():
            return "não foi possível buscar o branch remoto"
        rebased = self._git("rebase", "-q", f"origin/{self.branch}")
        if rebased.returncode != 0:
            self._git("rebase", "--abort")
            return "checkpoint local divergiu do branch remoto"
        return None

    def push(self) -> PushResult:
        """Push without force. Reconcile a rejected push at most once."""
        refspec = f"HEAD:refs/heads/{self.branch}"
        pushed = self._git("push", "--porcelain", "origin", refspec, timeout=self.push_timeout)
        if pushed.returncode != 0:
            output = (pushed.stdout + pushed.stderr).decode(errors="replace").strip()
            rejected = any(
                marker in output
                for marker in ("non-fast-forward", "fetch first", "rejected", "stale info")
            )
            if not rejected:
                return PushResult(False, None, output.splitlines()[-1] if output else "push falhou")
            conflict = self._rebase_onto_remote()
            if conflict:
                return PushResult(False, None, conflict)
            pushed = self._git("push", "--porcelain", "origin", refspec, timeout=self.push_timeout)
            if pushed.returncode != 0:
                output = (pushed.stdout + pushed.stderr).decode(errors="replace").strip()
                return PushResult(False, None, output.splitlines()[-1] if output else "push falhou")

        local = self.head()
        remote = self.remote_head()
        if remote is None:
            return PushResult(False, None, "push enviado, confirmação remota indisponível")
        if local and remote != local:
            return PushResult(False, None, f"remoto em {remote[:12]}, esperado {local[:12]}")
        return PushResult(True, remote, None)
