"""Tests for the LINBLUX automatic checkpoint backup.

Run with:
    python3 -m unittest discover -s tests/backup -p 'test_*.py'
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "backup"))

from linblux_backup.config import BackupConfig  # noqa: E402
from linblux_backup.runner import BackupRunner, Logger, status_snapshot  # noqa: E402

# Fixtures build the PEM marker at runtime so this file never contains a
# literal private-key block (the backup secret scan would skip it).
PEM_MARKER = "-----BEGIN " + "PRIVATE KEY-----"

GIT_ENV = {
    "GIT_AUTHOR_NAME": "Test",
    "GIT_AUTHOR_EMAIL": "test@example.invalid",
    "GIT_COMMITTER_NAME": "Test",
    "GIT_COMMITTER_EMAIL": "test@example.invalid",
    "GIT_TERMINAL_PROMPT": "0",
    "LC_ALL": "C",
}


def git(*args: str, cwd: Path | None = None, check: bool = True) -> subprocess.CompletedProcess:
    import os

    env = os.environ.copy()
    env.update(GIT_ENV)
    result = subprocess.run(
        ["git", *args], cwd=str(cwd) if cwd else None,
        capture_output=True, text=True, env=env, check=False,
    )
    if check and result.returncode != 0:
        raise AssertionError(f"git {' '.join(args)} falhou: {result.stderr}")
    return result


def make_config(**overrides) -> BackupConfig:
    values = dict(
        branch="auto-backup",
        remote="origin",
        include=("src/**", "docs/**", "CMakeLists.txt"),
        exclude=("build/**", "**/*.log", "**/.env", "**/*.key", "**/*.o"),
        max_file_bytes=2048,
        max_total_bytes=1_000_000,
        checkpoint_interval_seconds=0,
        poll_interval_seconds=0.1,
        push_retry_seconds=0,
        push_timeout_seconds=20,
        secret_scan=True,
    )
    values.update(overrides)
    return BackupConfig(**values)


class BackupTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = Path(tempfile.mkdtemp(prefix="linblux-backup-test-"))
        self.project = self.temporary / "project"
        self.origin = self.temporary / "origin.git"
        self.state = self.temporary / "state"
        self.project.mkdir()
        (self.project / "src").mkdir()
        (self.project / "docs").mkdir()
        (self.project / "build").mkdir()
        (self.project / "src" / "main.cpp").write_text("int main() { return 0; }\n")
        (self.project / "docs" / "guide.md").write_text("# Guide\n")
        (self.project / "CMakeLists.txt").write_text("project(demo)\n")
        (self.project / "build" / "artifact.o").write_text("binary-ish\n")
        (self.project / "debug.log").write_text("noise\n")
        (self.project / "src" / "notes.log").write_text("noise\n")
        git("init", "-q", "-b", "main", cwd=self.project)
        git("add", ".", cwd=self.project)
        git("commit", "-q", "-m", "initial", cwd=self.project)
        git("init", "-q", "--bare", str(self.origin))
        git("remote", "add", "origin", str(self.origin), cwd=self.project)

    def tearDown(self) -> None:
        shutil.rmtree(self.temporary, ignore_errors=True)

    def runner(self, config: BackupConfig | None = None, remote_url: str | None = None) -> BackupRunner:
        return BackupRunner(
            self.project,
            config or make_config(),
            self.state,
            remote_url=remote_url,
            logger=Logger(quiet=True),
        )

    def clone_backup(self, destination: Path) -> Path:
        git("clone", "-q", "--branch", "auto-backup", str(self.origin), str(destination))
        return destination


class CheckpointTests(BackupTestCase):
    def test_checkpoint_pushes_allowlist_and_never_touches_project_git_state(self) -> None:
        before_head = git("rev-parse", "HEAD", cwd=self.project).stdout.strip()
        index_path = self.project / ".git" / "index"
        before_index = index_path.stat().st_mtime_ns

        result = self.runner().run_once()

        self.assertIsNotNone(result.checkpoint)
        self.assertEqual(result.pushed, result.checkpoint)
        self.assertFalse(result.pending)
        self.assertEqual(git("rev-parse", "--abbrev-ref", "HEAD", cwd=self.project).stdout.strip(), "main")
        self.assertEqual(git("rev-parse", "HEAD", cwd=self.project).stdout.strip(), before_head)
        self.assertEqual(index_path.stat().st_mtime_ns, before_index)

        clone = self.clone_backup(self.temporary / "clone")
        self.assertTrue((clone / "src" / "main.cpp").is_file())
        self.assertTrue((clone / "docs" / "guide.md").is_file())
        self.assertTrue((clone / "CMakeLists.txt").is_file())
        self.assertFalse((clone / "build" / "artifact.o").exists())
        self.assertFalse((clone / "debug.log").exists())
        self.assertFalse((clone / "src" / "notes.log").exists())
        self.assertEqual(
            git("rev-parse", "HEAD", cwd=clone).stdout.strip(), result.checkpoint
        )

    def test_limits_and_secret_scan_skip_unsafe_files(self) -> None:
        (self.project / "src" / "big.cpp").write_text("x" * 4096)
        (self.project / "src" / ".env").write_text("TOKEN=abc\n")
        (self.project / "src" / "server.key").write_text(PEM_MARKER + "\nabc\n")
        (self.project / "src" / "credentials.txt").write_text(PEM_MARKER + "\nzzz\n")
        runner = self.runner()
        result = runner.run_once()

        clone = self.clone_backup(self.temporary / "clone")
        for relative in ("src/big.cpp", "src/.env", "src/server.key", "src/credentials.txt"):
            self.assertFalse((clone / relative).exists(), relative)
        skipped = {item["path"] for item in runner.state.skipped}
        self.assertIn("src/big.cpp", skipped)
        self.assertIn("src/credentials.txt", skipped)
        self.assertIsNotNone(result.checkpoint)

    def test_checkpoint_waits_for_the_configured_interval(self) -> None:
        from datetime import datetime, timedelta, timezone

        config = make_config(checkpoint_interval_seconds=120)
        first = self.runner(config).run_once()
        self.assertIsNotNone(first.checkpoint)

        (self.project / "src" / "main.cpp").write_text("int main() { return 3; }\n")
        too_soon = self.runner(config).run_once()
        self.assertIsNone(too_soon.checkpoint, "checkpoint não pode repetir antes de 2 minutos")
        self.assertIn("src/main.cpp", too_soon.changed)

        runner = self.runner(config)
        stale = (datetime.now(timezone.utc) - timedelta(seconds=121)).replace(microsecond=0)
        runner.state.last_checkpoint["time"] = stale.isoformat().replace("+00:00", "Z")
        runner.state.save(runner.state_path)
        after_interval = self.runner(config).run_once()
        self.assertIsNotNone(after_interval.checkpoint)
        self.assertNotEqual(after_interval.checkpoint, first.checkpoint)

    def test_second_checkpoint_updates_and_removes_files(self) -> None:
        runner = self.runner()
        first = runner.run_once()
        (self.project / "src" / "main.cpp").write_text("int main() { return 1; }\n")
        (self.project / "src" / "extra.cpp").write_text("// extra\n")
        (self.project / "docs" / "guide.md").unlink()
        second = runner.run_once()

        self.assertNotEqual(first.checkpoint, second.checkpoint)
        clone = self.clone_backup(self.temporary / "clone")
        self.assertIn("return 1", (clone / "src" / "main.cpp").read_text())
        self.assertTrue((clone / "src" / "extra.cpp").is_file())
        self.assertFalse((clone / "docs" / "guide.md").exists())


class OfflineAndRecoveryTests(BackupTestCase):
    def test_offline_checkpoint_stays_local_then_pushes(self) -> None:
        offline = self.runner(remote_url=str(self.temporary / "missing.git"))
        result = offline.run_once()

        self.assertIsNotNone(result.checkpoint)
        self.assertIsNone(result.pushed)
        self.assertTrue(result.pending)
        self.assertIsNotNone(result.error)
        self.assertTrue((self.state / "work" / "src" / "main.cpp").is_file())

        online = self.runner()
        resumed = online.run_once()
        self.assertEqual(resumed.pushed, result.checkpoint)
        self.assertFalse(resumed.pending)
        remote = git(
            "ls-remote", "--heads", str(self.origin), "auto-backup"
        ).stdout.split()[0]
        self.assertEqual(remote, result.checkpoint)

    def test_rejected_push_reconciles_without_rewriting_remote_history(self) -> None:
        runner = self.runner()
        runner.run_once()

        other = self.temporary / "other-agent"
        git("clone", "-q", "--branch", "auto-backup", str(self.origin), str(other))
        (other / "src" / "other-agent.cpp").write_text("// other agent\n")
        git("add", "src/other-agent.cpp", cwd=other)
        git("commit", "-q", "-m", "outro agente", cwd=other)
        git("push", "-q", "origin", "auto-backup", cwd=other)
        remote_before = git("rev-parse", "HEAD", cwd=other).stdout.strip()

        (self.project / "src" / "main.cpp").write_text("int main() { return 2; }\n")
        result = self.runner().run_once()

        self.assertIsNotNone(result.pushed)
        history = git("log", "--format=%s", "auto-backup", cwd=self.origin).stdout
        self.assertIn("outro agente", history)
        self.assertIn("checkpoint", history)
        ancestor = git("merge-base", "--is-ancestor", remote_before, result.pushed, cwd=self.origin, check=False)
        self.assertEqual(ancestor.returncode, 0, "histórico remoto foi preservado")
        clone = self.clone_backup(self.temporary / "clone")
        self.assertTrue((clone / "src" / "other-agent.cpp").is_file())

    def test_clean_clone_restores_every_protected_file(self) -> None:
        runner = self.runner()
        result = runner.run_once()
        clone = self.clone_backup(self.temporary / "recovery")

        checked = 0
        for source in (self.project / "src", self.project / "docs"):
            for path in source.rglob("*"):
                if path.is_file() and not path.name.endswith(".log"):
                    restored = clone / path.relative_to(self.project)
                    self.assertTrue(restored.is_file(), str(restored))
                    self.assertEqual(restored.read_bytes(), path.read_bytes())
                    checked += 1
        self.assertGreaterEqual(checked, 2)
        self.assertTrue((clone / "CMakeLists.txt").is_file())
        self.assertIsNotNone(result.checkpoint)

    def test_cli_verify_confirms_restoration(self) -> None:
        self.runner().run_once()
        completed = subprocess.run(
            [sys.executable, str(ROOT / "tools" / "backup" / "backup.py"), "verify",
             "--project", str(self.project), "--state-dir", str(self.state), "--json"],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr + completed.stdout)
        report = json.loads(completed.stdout)
        self.assertTrue(report["ok"])
        self.assertGreater(report["checked"], 0)
        self.assertIsNotNone(report["checkpoint"])


class StatusTests(BackupTestCase):
    def test_status_reports_checkpoint_push_and_unprotected_changes(self) -> None:
        runner = self.runner()
        result = runner.run_once()
        (self.project / "src" / "main.cpp").write_text("// changed\n")
        (self.project / "src" / "new.cpp").write_text("// new\n")

        snapshot = status_snapshot(self.runner())
        self.assertEqual(snapshot["last_checkpoint"]["hash"], result.checkpoint)
        self.assertEqual(snapshot["last_push"]["hash"], result.checkpoint)
        self.assertFalse(snapshot["pending_push"])
        self.assertIn("src/main.cpp", snapshot["unprotected"])
        self.assertIn("src/new.cpp", snapshot["unprotected"])
        self.assertIn("src/new.cpp", snapshot["changes"]["added"])

    def test_cli_status_json_has_required_fields(self) -> None:
        self.runner().run_once()
        completed = subprocess.run(
            [sys.executable, str(ROOT / "tools" / "backup" / "backup.py"), "status",
             "--project", str(self.project), "--state-dir", str(self.state), "--json"],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        payload = json.loads(completed.stdout)
        for key in ("last_checkpoint", "last_push", "pending_push", "unprotected", "branch"):
            self.assertIn(key, payload)
        self.assertFalse(payload["unprotected"])

    def test_daemon_lock_is_exclusive(self) -> None:
        first = self.runner()
        second = self.runner()
        self.assertTrue(first.acquire_lock())
        try:
            self.assertFalse(second.acquire_lock())
        finally:
            first._lock_handle.close()


if __name__ == "__main__":
    unittest.main()
