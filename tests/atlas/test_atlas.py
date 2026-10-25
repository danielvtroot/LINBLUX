#!/usr/bin/env python3
"""Integration tests for the LINBLUX ATLAS CLI.

These tests use a controlled fixture and never touch the repository index. The
temporary clone lives under ``.atlas/tests`` (gitignored) so no test data can be
mistaken for project source.
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


class AtlasCliTest(unittest.TestCase):
    def setUp(self) -> None:
        cache = ROOT / ".atlas"
        cache.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(prefix="atlas-test-", dir=cache)
        self.root = Path(self.tmp.name) / "repo"
        shutil.copytree(FIXTURE, self.root)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def invoke(self, *args: str, timeout: int = 60) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(ATLAS), "--root", str(self.root), *args],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=timeout,
        )

    def json(self, *args: str, expect: int = 0) -> dict:
        proc = self.invoke(*args, "--json")
        self.assertEqual(
            proc.returncode,
            expect,
            f"command failed: {args}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}",
        )
        try:
            return json.loads(proc.stdout)
        except json.JSONDecodeError as exc:
            self.fail(f"invalid JSON for {args}: {exc}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}")

    def init(self) -> dict:
        return self.json("init", "--force")["data"]

    def test_init_status_verify_and_json_contract(self) -> None:
        data = self.init()
        self.assertTrue(data["ok"])
        self.assertEqual(data["indexed_files"], 8)
        self.assertGreaterEqual(data["counts"]["symbols"], 5)
        self.assertGreaterEqual(data["counts"]["cmake_targets"], 3)
        self.assertGreaterEqual(data["counts"]["tests"], 2)
        self.assertGreater(data["database_bytes"], 0)

        status = self.json("status")
        self.assertTrue(status["ok"])
        self.assertEqual(status["data"]["counts"]["files"], 8)
        self.assertIn("semantic_cpp", status["data"]["tools"])

        verify = self.json("verify")
        self.assertTrue(verify["data"]["ok"])
        checks = {item["name"]: item["ok"] for item in verify["data"]["checks"]}
        self.assertTrue(checks["content-hashes"])
        self.assertTrue(checks["query-smoke"])

    def test_locate_symbol_text_and_tests(self) -> None:
        self.init()
        located = self.json("locate", "widget set value", "--limit", "5")
        candidates = located["data"]["candidates"]
        self.assertTrue(candidates)
        self.assertIn("core", {item["module"] for item in candidates})
        self.assertTrue(any(
            "Widget" in symbol["qualified"]
            for item in candidates
            for symbol in item.get("symbols", [])
        ))
        self.assertIn("score_breakdown", candidates[0]["files"][0])

        symbol = self.json("symbol", "Widget")
        definitions = symbol["data"]["definitions"]
        self.assertTrue(definitions)
        self.assertTrue(all(item["origin"] == "regex-cpp" for item in definitions))
        self.assertTrue(all(item["confidence"] == "medium" for item in definitions))

        text = self.json("text", "setValue")
        self.assertTrue(text["data"]["results"])
        self.assertTrue(any(item["path"].endswith("Widget.h") for item in text["data"]["results"]))

        tests = self.json("tests-for", "src/core/Widget.h")
        self.assertEqual([item["name"] for item in tests["data"]["tests"]], ["widget_test"])

    def test_impact_maps_consumers(self) -> None:
        self.init()
        result = self.json("impact", "src/core/Widget.h", "--depth", "1")["data"]
        self.assertEqual(result["resolved_file"], "src/core/Widget.h")
        affected = set(result["affected_files"])
        self.assertIn("src/core/Widget.cpp", affected)
        self.assertIn("src/app/Service.h", affected)
        self.assertIn("tests/widget_test.cpp", affected)
        self.assertTrue(any(test["name"] == "widget_test" for test in result["tests"]))
        self.assertTrue(result["public_interface"])

    def test_incremental_add_modify_delete_and_verify(self) -> None:
        first = self.init()
        gadget = self.root / "src" / "core" / "Gadget.h"
        gadget.write_text(
            "#pragma once\nnamespace fixture {\nclass Gadget {\npublic:\n    int id() const;\n};\n}\n",
            encoding="utf-8",
        )
        added = self.json("update")["data"]
        self.assertEqual(added["added"], 1)
        self.assertEqual(added["deleted"], 0)
        gadget_symbol = self.json("symbol", "Gadget")
        self.assertTrue(gadget_symbol["data"]["definitions"])
        self.assertTrue(self.json("verify")["data"]["ok"])

        widget = self.root / "src" / "core" / "Widget.h"
        widget.write_text(widget.read_text(encoding="utf-8").replace(
            "void setValue(int value);",
            "void setValue(int value);\n    void reset();",
        ), encoding="utf-8")
        changed = self.json("update")["data"]
        self.assertEqual(changed["updated"], 1)
        self.assertEqual(changed["added"], 0)
        self.assertTrue(self.json("symbol", "reset")["data"]["definitions"])
        self.assertTrue(self.json("verify")["data"]["ok"])

        gadget.unlink()
        removed = self.json("update")["data"]
        self.assertEqual(removed["deleted"], 1)
        status = self.json("status")["data"]
        self.assertEqual(status["counts"]["files"], first["counts"]["files"])
        self.assertEqual(self.json("symbol", "Gadget")["data"]["definitions"], [])
        self.assertTrue(self.json("verify")["data"]["ok"])

    def test_mtime_only_change_refreshes_status_metadata(self) -> None:
        self.init()
        target = self.root / "src" / "app" / "Service.h"
        stat = target.stat()
        os.utime(target, ns=(stat.st_atime_ns, stat.st_mtime_ns + 1_000_000_000))
        updated = self.json("update")["data"]
        self.assertEqual(updated["updated"], 0)
        self.assertEqual(updated["unchanged"], 8)
        status = self.json("status")["data"]
        self.assertEqual(status["stale_estimate"], 0)
        self.assertTrue(status["ok"])

    def test_ignored_build_and_third_party_directories(self) -> None:
        self.init()
        status = self.json("status")["data"]
        self.assertEqual(status["counts"]["files"], 8)
        for term in ("generated", "vendored"):
            result = self.json("text", term)["data"]
            self.assertFalse(result["results"], f"ignored files leaked through {term}")
            self.assertFalse(any(
                "build/" in item["path"] or "third_party/" in item["path"]
                for item in result["results"]
            ))

    def test_corrupt_index_recovers(self) -> None:
        self.init()
        for path in (self.root / ".atlas").glob("index.sqlite*"):
            path.unlink()
        (self.root / ".atlas" / "index.sqlite").write_bytes(b"this is not sqlite")
        recovered = self.json("update")["data"]
        self.assertEqual(recovered["indexed_files"], 8)
        self.assertTrue(self.json("verify")["data"]["ok"])
        self.assertIn("core", {item["module"] for item in self.json("locate", "widget")["data"]["candidates"]})

    def test_limits_are_deterministic(self) -> None:
        self.init()
        first = self.json("locate", "widget", "--limit", "1")["data"]
        second = self.json("locate", "widget", "--limit", "1")["data"]
        self.assertLessEqual(len(first["candidates"]), 1)
        self.assertEqual(
            [(item["module"], item["score"]) for item in first["candidates"]],
            [(item["module"], item["score"]) for item in second["candidates"]],
        )

    def test_symlink_outside_root_is_rejected(self) -> None:
        self.init()
        outside = self.root.parent / "outside.cpp"
        outside.write_text(
            "namespace fixture { class Leaked { public: int value() const; }; }\n",
            encoding="utf-8",
        )
        link = self.root / "src" / "Leaked.cpp"
        os.symlink(outside, link)
        updated = self.json("update")["data"]
        self.assertEqual(updated["added"], 0)
        self.assertEqual(self.json("symbol", "Leaked")["data"]["definitions"], [])
        self.assertEqual(self.json("status")["data"]["counts"]["files"], 8)

    def test_invalid_paths_are_rejected(self) -> None:
        self.init()
        outside = self.json("tests-for", "../outside", expect=2)
        self.assertFalse(outside["ok"])
        self.assertIn("fora da raiz", outside["error"])
        missing = self.json("symbol", "DoesNotExist")
        self.assertEqual(missing["data"]["definitions"], [])

    def test_doctor_reports_only_nonfatal_fixture_issues(self) -> None:
        self.init()
        report = self.json("doctor")["data"]
        severities = {issue["severity"] for issue in report["issues"]}
        self.assertNotIn("alta", severities)
        # The fixture intentionally has no manual contracts yet.
        self.assertIn("contracts-missing", {issue["code"] for issue in report["issues"]})


if __name__ == "__main__":
    unittest.main(verbosity=2)
