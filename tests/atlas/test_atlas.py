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


class AtlasV2RegressionTest(unittest.TestCase):
    """Regressions for the ATLAS 2.0 fixes (audit B1-B6, B14, B17)."""

    def setUp(self) -> None:
        cache = ROOT / ".atlas"
        cache.mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(prefix="atlas-v2-", dir=cache)
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
        return json.loads(proc.stdout)

    def init(self) -> dict:
        return self.json("init", "--force")["data"]

    def test_generated_directories_are_not_indexed(self) -> None:
        for relative in (".turbo/logs/debug.py", "prototype/mock.cpp", "node_modules/pkg/index.js"):
            target = self.root / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("ZZGENERATEDZZ\n", encoding="utf-8")
        self.init()
        self.assertEqual(self.json("status")["data"]["counts"]["files"], 8)
        self.assertEqual(self.json("text", "ZZGENERATEDZZ")["data"]["results"], [])

    def test_second_update_does_not_reread_files(self) -> None:
        self.init()
        again = self.json("update")["data"]
        self.assertEqual(again["bytes_read"], 0)
        self.assertEqual(again["added"], 0)
        self.assertEqual(again["updated"], 0)
        self.assertEqual(again["unchanged"], 8)

    def test_edit_with_preserved_mtime_is_detected(self) -> None:
        self.init()
        target = self.root / "src" / "app" / "Service.h"
        before = target.stat()
        text = target.read_text(encoding="utf-8").replace("int read() const;", "int load() const;")
        target.write_text(text, encoding="utf-8")
        os.utime(target, ns=(before.st_atime_ns, before.st_mtime_ns))
        self.assertEqual(target.stat().st_size, before.st_size)
        updated = self.json("update")["data"]
        self.assertEqual(updated["updated"], 1)
        self.assertGreater(updated["bytes_read"], 0)

    def test_locate_ranking_is_stable_across_limits(self) -> None:
        self.init()
        tops = []
        for limit in (1, 3, 8, 20):
            files = self.json("locate", "widget set value", "--limit", str(limit))["data"]["files"]
            self.assertTrue(files, f"sem candidatos em limit={limit}")
            tops.append(files[0]["path"])
        self.assertEqual(len(set(tops)), 1, tops)

    def test_locate_reports_line_and_match_types(self) -> None:
        self.init()
        top = self.json("locate", "widget set value")["data"]["files"][0]
        self.assertTrue(top["line"])
        self.assertTrue(top["match_types"])

    def test_tests_for_library_source_uses_include_evidence(self) -> None:
        self.init()
        tests = self.json("tests-for", "src/core/Widget.cpp")["data"]["tests"]
        names = [item["name"] for item in tests]
        self.assertIn("widget_test", names, names)
        widget = next(item for item in tests if item["name"] == "widget_test")
        self.assertEqual(widget["confidence"], "confirmed")
        self.assertTrue(
            any(reason.startswith("include:") for reason in widget["reasons"]),
            widget["reasons"],
        )

    def test_impact_reaches_dependent_test_source(self) -> None:
        self.init()
        data = self.json("impact", "src/core/Widget.cpp")["data"]
        self.assertIn("tests/widget_test.cpp", data["affected_files"])

    def test_diagnose_brief_stays_short_and_json_is_valid(self) -> None:
        self.init()
        proc = self.invoke("diagnose", "widget set value")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertLessEqual(len(proc.stdout), 1200)
        self.assertIn("DIAGNÓSTICO", proc.stdout)
        payload = self.json("diagnose", "widget set value", "--brief")
        self.assertTrue(payload["ok"])
        self.assertIn("files", payload["data"])

    def test_missing_target_uses_uniform_error_envelope(self) -> None:
        self.init()
        proc = self.invoke("impact", "naoexiste.cpp", "--json")
        self.assertNotEqual(proc.returncode, 0)
        payload = json.loads(proc.stdout)
        self.assertFalse(payload["ok"])
        self.assertTrue(payload.get("error"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
