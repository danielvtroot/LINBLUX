#!/usr/bin/env python3
"""TURBO must not start heavy I/O in ``ATLAS_IO_MODE=hdd-critical``.

``test --plan`` remains allowed (read-only inventory); ``build`` and a real
``test`` run are refused with an actionable message instead of being silently
skipped, so a missing test run is never mistaken for approval.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TURBO = ROOT / "tools" / "turbo" / "turbo.py"


class HddCriticalGuardTest(unittest.TestCase):
    def invoke(self, *args: str, mode: str = "hdd-critical") -> subprocess.CompletedProcess[str]:
        env = dict(os.environ, ATLAS_IO_MODE=mode)
        return subprocess.run(
            [sys.executable, str(TURBO), *args],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=90,
            env=env,
        )

    def test_build_is_refused(self) -> None:
        proc = self.invoke("build", "--json")
        self.assertEqual(proc.returncode, 1, proc.stderr)
        payload = json.loads(proc.stdout)
        self.assertFalse(payload["ok"])
        self.assertEqual(payload["data"]["status"], "REFUSED")
        self.assertIn("hdd-critical", payload["message"])
        self.assertIn("hdd-safe", self.invoke("test", "--json").stdout)

    def test_test_run_is_refused_but_plan_is_allowed(self) -> None:
        refused = self.invoke("test", "--json")
        self.assertEqual(refused.returncode, 1, refused.stderr)
        self.assertIn("REFUSED", refused.stdout)
        plan = self.invoke("test", "--plan")
        self.assertNotIn("REFUSED", plan.stdout)
        self.assertIn("PLANO", plan.stdout.upper())

    def test_normal_mode_still_plans(self) -> None:
        proc = self.invoke("test", "--plan", mode="normal")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("PLANO", proc.stdout.upper())


if __name__ == "__main__":
    unittest.main(verbosity=2)
