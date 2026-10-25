"""Explicit, auditable file selection for the LINBLUX automatic backup.

Only files matched by ``include`` and not matched by ``exclude`` are read.
Candidates come from git's own index/untracked listing, so ``.gitignore`` is
respected without ever writing to the developer's index.
"""

from __future__ import annotations

import hashlib
import os
import re
import stat
import subprocess
from dataclasses import dataclass, field
from pathlib import Path

from .config import BackupConfig

# High-confidence credential shapes only. Generic words such as "token" are
# ignored on purpose to avoid skipping legitimate source files.
SECRET_PATTERNS = (
    re.compile(rb"-----BEGIN (?:[A-Z]+ )*PRIVATE KEY-----"),
    re.compile(rb"\bAKIA[0-9A-Z]{16}\b"),
    re.compile(rb"\bASIA[0-9A-Z]{16}\b"),
    re.compile(rb"\bgh[pousr]_[A-Za-z0-9]{20,}\b"),
    re.compile(rb"\bgithub_pat_[A-Za-z0-9_]{20,}\b"),
    re.compile(rb"\bsk-(?:proj-|svcacct-)?[A-Za-z0-9_-]{20,}\b"),
    re.compile(rb"\bxox[baprs]-[A-Za-z0-9-]{10,}\b"),
    re.compile(rb"\bAIza[0-9A-Za-z_-]{35}\b"),
)


def _glob_to_regex(pattern: str) -> re.Pattern[str]:
    """Gitignore-like glob: ``*`` stays inside a segment, ``**`` spans dirs."""
    if "/" not in pattern:
        pattern = "**/" + pattern
    out: list[str] = []
    index = 0
    while index < len(pattern):
        char = pattern[index]
        if char == "*":
            if pattern.startswith("**", index):
                index += 2
                if pattern.startswith("/", index):
                    index += 1
                    out.append(r"(?:.*/)?")
                else:
                    out.append(r".*")
                continue
            out.append(r"[^/]*")
        elif char == "?":
            out.append(r"[^/]")
        else:
            out.append(re.escape(char))
        index += 1
    return re.compile("^" + "".join(out) + "$")


class Matcher:
    """Ordered matcher; a leading ``!`` in ``exclude`` re-includes a path."""

    def __init__(self, include: tuple[str, ...], exclude: tuple[str, ...]) -> None:
        self._include = tuple(_glob_to_regex(item) for item in include)
        self._exclude = tuple(
            (item.startswith("!"), _glob_to_regex(item[1:] if item.startswith("!") else item))
            for item in exclude
        )

    def included(self, relative: str) -> bool:
        if not any(pattern.match(relative) for pattern in self._include):
            return False
        included = True
        for negated, pattern in self._exclude:
            if pattern.match(relative):
                # A normal exclude pattern drops the file; a "!" pattern puts it back.
                included = negated
        return included


@dataclass(frozen=True)
class SelectedFile:
    relative: str
    path: Path
    size: int
    digest: str
    data: bytes
    mode: int


@dataclass
class Selection:
    files: list[SelectedFile] = field(default_factory=list)
    skipped: list[dict] = field(default_factory=list)
    total_bytes: int = 0

    def by_relative(self) -> dict[str, SelectedFile]:
        return {item.relative: item for item in self.files}


def _git(project: Path, *args: str, timeout: float = 30.0) -> subprocess.CompletedProcess:
    env = os.environ.copy()
    for key in ("GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_PREFIX"):
        env.pop(key, None)
    env["GIT_OPTIONAL_LOCKS"] = "0"  # never refresh/lock the developer index
    env["GIT_TERMINAL_PROMPT"] = "0"
    env["LC_ALL"] = "C"
    return subprocess.run(
        ["git", "-C", str(project), *args],
        capture_output=True,
        env=env,
        timeout=timeout,
        check=False,
    )


def candidate_paths(project: Path) -> list[str]:
    """Tracked files plus untracked files that git does not ignore."""
    result = _git(project, "ls-files", "-z", "--cached", "--others", "--exclude-standard")
    if result.returncode != 0:
        return _walk_paths(project)
    names = result.stdout.decode("utf-8", "surrogateescape").split("\0")
    return sorted({name for name in names if name})


def _walk_paths(project: Path) -> list[str]:
    """Fallback for a tree without git metadata (never writes anything)."""
    skip = {".git", "build", "build-linblux", "build-no-video", "cache", ".cache",
            ".atlas", ".turbo", "Testing", "__pycache__"}
    found: list[str] = []
    for root, directories, files in os.walk(project):
        directories[:] = [name for name in directories if name not in skip and not name.startswith("build")]
        for name in files:
            found.append(os.path.relpath(os.path.join(root, name), project))
    return sorted(found)


def sha256_file(path: Path, chunk: int = 1 << 20) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        while True:
            block = handle.read(chunk)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def _looks_secret(data: bytes) -> bool:
    return any(pattern.search(data) for pattern in SECRET_PATTERNS)


def read_snapshot(project: Path, relative: str, limit: int) -> tuple[bytes, int]:
    """Open every path component without following symlinks; bound the read.

    The exact bytes scanned are later committed, even if another editor replaces
    the source after selection. No second, unchecked copy from the working tree.
    """
    parts = Path(relative).parts
    if not parts or Path(relative).is_absolute() or any(p in ("..", ".git") for p in parts):
        raise OSError("caminho fora da política")
    directory = os.open(project, os.O_RDONLY | os.O_DIRECTORY)
    try:
        for part in parts[:-1]:
            child = os.open(part, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW, dir_fd=directory)
            os.close(directory)
            directory = child
        descriptor = os.open(parts[-1], os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK, dir_fd=directory)
        with os.fdopen(descriptor, "rb") as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode):
                raise OSError("não é arquivo regular")
            if info.st_size > limit:
                raise ValueError("acima do limite de tamanho")
            data = stream.read(limit + 1)
            if len(data) > limit:
                raise ValueError("acima do limite de tamanho")
            return data, info.st_mode & 0o777
    finally:
        os.close(directory)


def select(project: Path, config: BackupConfig) -> Selection:
    """Resolve the exact set of files this checkpoint would protect."""
    matcher = Matcher(config.include, config.exclude)
    selection = Selection()
    for relative in candidate_paths(project):
        if not matcher.included(relative):
            continue
        absolute = project / relative
        try:
            data, mode = read_snapshot(project, relative, config.max_file_bytes)
        except ValueError as error:
            selection.skipped.append({"path": relative, "reason": str(error)})
            continue
        except OSError:
            selection.skipped.append({"path": relative, "reason": "ausente, link ou leitura recusada"})
            continue
        if selection.total_bytes + len(data) > config.max_total_bytes:
            selection.skipped.append({"path": relative, "reason": "limite total de backup atingido"})
            continue
        if config.secret_scan and _looks_secret(data):
            selection.skipped.append({"path": relative, "reason": "credencial detectada"})
            continue
        selection.files.append(
            SelectedFile(relative, absolute, len(data), hashlib.sha256(data).hexdigest(), data, mode)
        )
        selection.total_bytes += len(data)
    return selection
