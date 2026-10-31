#!/usr/bin/env python3
"""ATLAS 2.1 I/O-mode tests: ``normal``, ``hdd-safe`` and ``hdd-critical``.

Everything runs against a throwaway copy of the small fixture under ``.atlas``
(gitignored). The tests assert *observed* behaviour: whether the cache directory
or the index file changed, not just what the tool claims in its output.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ATLAS = ROOT / "tools" / "atlas" / "atlas.py"
FIXTURE = Path(__file__).resolve().parent / "fixture"


class IoModeTest(unittest.TestCase):
    def setUp(self) -> None:
        cache = ROOT / ".atlas"
        cache.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(prefix="atlas-io-", dir=cache)
        self.root = Path(self.tmp.name) / "repo"
        shutil.copytree(FIXTURE, self.root)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    # ------------------------------------------------------------------ helpers
    def _env(self, mode: str | None) -> dict:
        env = dict(os.environ)
        env.pop("ATLAS_IO_MODE", None)
        if mode:
            env["ATLAS_IO_MODE"] = mode
        return env

    def run_atlas(
        self, *args: str, mode: str | None = None, expect: int = 0
    ) -> subprocess.CompletedProcess[str]:
        proc = subprocess.run(
            [sys.executable, str(ATLAS), "--root", str(self.root), *args],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=90,
            env=self._env(mode),
        )
        self.assertEqual(
            proc.returncode,
            expect,
            f"mode={mode} args={args}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}",
        )
        return proc

    def json_atlas(self, *args: str, mode: str | None = None, expect: int = 0) -> dict:
        proc = self.run_atlas(*args, "--json", mode=mode, expect=expect)
        try:
            return json.loads(proc.stdout)
        except json.JSONDecodeError as exc:  # pragma: no cover - diagnostic aid
            self.fail(f"JSON inválido para {args} (mode={mode}): {exc}\n{proc.stdout}")

    def init(self) -> dict:
        return self.json_atlas("init", "--force")["data"]

    def snapshot(self) -> dict | None:
        cache = self.root / ".atlas"
        if not cache.is_dir():
            return None
        return {
            path.name: (path.stat().st_size, path.stat().st_mtime_ns)
            for path in sorted(cache.iterdir())
            if path.is_file()
        }

    # ------------------------------------------------------------------- tests
    def test_critical_refuses_to_create_an_index(self) -> None:
        proc = self.run_atlas("init", mode="hdd-critical", expect=2)
        self.assertIn("hdd-critical", proc.stderr)
        self.assertIn("bloqueada", proc.stderr)
        self.assertFalse((self.root / ".atlas").exists())

    def test_critical_query_without_index_does_not_scan(self) -> None:
        proc = self.run_atlas("diagnose", "widget value", mode="hdd-critical", expect=2)
        self.assertIn("indice ausente", proc.stderr)
        self.assertFalse((self.root / ".atlas").exists())

    def test_critical_reads_never_touch_the_index(self) -> None:
        self.init()
        before = self.snapshot()
        for args in (
            ("locate", "widget set value"),
            ("symbol", "Widget"),
            ("tests-for", "src/core/Widget.cpp"),
            ("impact", "src/core/Widget.cpp"),
            ("diagnose", "widget set value", "--brief"),
            ("status",),
            ("health",),
        ):
            self.run_atlas(*args, mode="hdd-critical")
        self.assertEqual(self.snapshot(), before, "hdd-critical wrote to .atlas")

    def test_safe_queries_are_read_only(self) -> None:
        self.init()
        before = self.snapshot()
        for args in (
            ("locate", "widget set value"),
            ("diagnose", "widget set value", "--brief"),
            ("health", "--brief"),
        ):
            payload = self.json_atlas(*args, mode="hdd-safe")
            self.assertEqual(payload["data"].get("io_mode"), "hdd-safe")
        self.assertEqual(self.snapshot(), before, "hdd-safe query wrote to .atlas")
        status = self.json_atlas("status", mode="hdd-safe")["data"]
        self.assertTrue(status["staleness_checked"])
        self.assertEqual(status["stale_estimate"], 0)
        self.assertEqual(self.snapshot(), before, "hdd-safe status wrote to .atlas")

    def test_safe_update_is_explicit_and_idempotent(self) -> None:
        self.init()
        updated = self.json_atlas("update", mode="hdd-safe")["data"]
        self.assertEqual(updated["io_mode"], "hdd-safe")
        self.assertEqual(updated["added"], 0)
        self.assertEqual(updated["unchanged"], 8)
        before = self.snapshot()
        again = self.json_atlas("update", mode="hdd-safe")["data"]
        self.assertTrue(again.get("noop"))
        self.assertEqual(again["bytes_read"], 0)
        self.assertEqual(self.snapshot(), before, "no-op update rewrote the index")

    def test_stale_file_is_reported_without_writing(self) -> None:
        self.init()
        target = self.root / "src" / "core" / "Widget.h"
        target.write_text(
            target.read_text(encoding="utf-8").replace(
                "void setValue(int value);", "void setValue(int value);\n    void reset();"
            ),
            encoding="utf-8",
        )
        before = self.snapshot()
        status = self.json_atlas("status", mode="hdd-safe")["data"]
        self.assertGreaterEqual(status["stale_estimate"], 1)
        self.assertEqual(self.snapshot(), before, "safe status wrote while stale")

        health = self.json_atlas("health", mode="hdd-critical")["data"]
        self.assertFalse(health["index"]["staleness_checked"])
        self.assertTrue(
            any("desatualizado" in warning for warning in health["warnings"]),
            health["warnings"],
        )
        self.assertEqual(self.snapshot(), before, "critical health wrote while stale")

        fixed = self.json_atlas("update", mode="hdd-safe")["data"]
        self.assertEqual(fixed["updated"], 1)
        self.assertTrue(self.json_atlas("verify", mode="normal")["data"]["ok"])

    def test_critical_blocks_integrity_reports_and_bench(self) -> None:
        self.init()
        before = self.snapshot()
        for command in ("verify", "doctor", "docs", "bench"):
            proc = self.run_atlas(command, mode="hdd-critical", expect=2)
            self.assertIn("hdd-critical", proc.stderr)
        self.assertEqual(self.snapshot(), before, "blocked command still wrote")

    def test_invalid_mode_is_rejected(self) -> None:
        proc = self.run_atlas("status", mode="turbo", expect=2)
        self.assertIn("inválido", proc.stderr)
        self.assertIn("hdd-critical", proc.stderr)
        bad = self.run_atlas("--io-mode", "safe-ish", "status", expect=2)
        self.assertIn("invalid choice", bad.stderr)

    def test_concurrent_readers_do_not_corrupt_the_index(self) -> None:
        self.init()
        command = [sys.executable, str(ATLAS), "--root", str(self.root), "locate", "widget"]
        env = self._env("hdd-critical")
        first = subprocess.Popen(command, cwd=ROOT, env=env, stdout=subprocess.DEVNULL,
                                 stderr=subprocess.PIPE, text=True)
        second = subprocess.Popen(command, cwd=ROOT, env=env, stdout=subprocess.DEVNULL,
                                  stderr=subprocess.PIPE, text=True)
        errors = []
        for proc in (first, second):
            _, err = proc.communicate(timeout=90)
            if proc.returncode != 0:
                errors.append(err)
        self.assertFalse(errors, errors)
        self.assertTrue(self.json_atlas("verify")["data"]["ok"])

    def test_health_json_is_small_and_parsable_in_every_mode(self) -> None:
        self.init()
        for mode in ("normal", "hdd-safe", "hdd-critical"):
            payload = self.json_atlas("health", "--brief", mode=mode)["data"]
            self.assertEqual(payload["io_mode"], mode)
            self.assertTrue(payload["index"]["exists"])
            self.assertIn("available_mib", payload["memory"])
        text = self.run_atlas("health", "--brief", mode="hdd-critical").stdout
        self.assertLessEqual(len(text), 800, text)

    def test_normal_mode_remains_writable_and_compatible(self) -> None:
        self.init()
        located = self.json_atlas("locate", "widget set value")["data"]
        self.assertTrue(located["candidates"])
        gadget = self.root / "src" / "core" / "Gadget.h"
        gadget.write_text("#pragma once\nnamespace fixture { class Gadget {}; }\n", encoding="utf-8")
        added = self.json_atlas("update")["data"]
        self.assertEqual(added["added"], 1)
        self.assertEqual(added["io_mode"], "normal")


if __name__ == "__main__":
    unittest.main(verbosity=2)
