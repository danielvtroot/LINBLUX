"""Tests for LINBLUX ATLAS TURBO itself.

These tests are intentionally stdlib-only.  They exercise real processes and
real CMake failures in temporary directories, but never run the graphical
LINBLUX suite or modify repository files.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
TURBO_PATH = ROOT / "tools" / "turbo"
if str(TURBO_PATH) not in sys.path:
    sys.path.insert(0, str(TURBO_PATH))

from turbo_lib import cache, changes, config, docscheck, inventory, organize, report, runner, selection  # noqa: E402


def fake_test(
    name: str,
    target: str,
    modules: tuple[str, ...],
    *,
    graphics: bool = False,
    slow: bool = False,
    video: bool = False,
    duration: float = 0.0,
    ctest_labels: tuple[str, ...] = (),
) -> dict:
    labels = set(ctest_labels)
    labels.update(modules)
    labels.add("graphics" if graphics else "headless")
    if graphics:
        labels.update({"graphics", "integration"})
    if video:
        labels.update({"video", "integration"})
    if slow:
        labels.add("slow")
    if not graphics and not video and not slow:
        labels.add("unit")
    return {
        "name": name,
        "target": target,
        "source_path": f"tests/{name}.cpp",
        "modules": list(modules),
        "labels": sorted(labels),
        "ctest_labels": list(ctest_labels),
        "graphics": graphics,
        "headless": not graphics,
        "video": video,
        "smoke": False,
        "integration": graphics or video,
        "slow": slow,
        "reusable": not graphics and not video and not slow,
        "duration_seconds": duration,
        "runs": 1,
        "timeout_seconds": 300,
        "command": [],
        "command_text": "",
        "environment": {},
        "labels_source": "test",
        "evidence": {},
    }


def fake_inventory(tests: list[dict], **kwargs) -> dict:
    return {
        "tests": tests,
        "missing_tests": [],
        "targets": [],
        "source_by_target": kwargs.get("source_by_target", {}),
        "deps_by_target": kwargs.get("deps_by_target", {}),
        "test_files": kwargs.get("test_files", {}),
        "atlas_files": kwargs.get("atlas_files", []),
        "atlas_relations": kwargs.get("atlas_relations", []),
        "atlas_info": {},
        "warnings": [],
        "build_dir": "/tmp/build",
    }


def fake_changes(path: str, module: str = "core") -> dict:
    return {
        "files": [{"path": path, "status": "M ", "exists": True, "module": module, "sha256": "a", "bytes": 1}],
        "count": 1,
        "modules": [module],
        "docs": [],
        "code": [path],
        "docs_only": False,
        "clean": False,
    }


class InventoryTests(unittest.TestCase):
    def test_classification_uses_command_and_cost(self):
        graphics = inventory.classify_test(
            "linblux_ui_demo",
            {
                "command_text": "xvfb-run ./demo",
                "environment": ["LIBGL_ALWAYS_SOFTWARE=1"],
                "labels": [],
                "timeout": 90,
            },
            None,
            {"cost_seconds": 12.5, "runs": 2},
        )
        self.assertTrue(graphics["graphics"])
        self.assertTrue(graphics["headless"] is False)
        self.assertIn("graphics", graphics["labels"])
        self.assertIn("slow", graphics["labels"])
        self.assertEqual(graphics["timeout_seconds"], 90)

        smoke = inventory.classify_test(
            "linblux_help",
            {"command_text": "./linblux --help", "environment": [], "labels": [], "timeout": None},
            None,
            None,
        )
        self.assertTrue(smoke["headless"])
        self.assertTrue(smoke["smoke"])
        self.assertIn("smoke", smoke["labels"])
        self.assertFalse(smoke["graphics"])

        video = inventory.classify_test(
            "linblux_video",
            {"command_text": "./video file.mp4", "environment": [], "labels": [], "timeout": 40},
            None,
            {"cost_seconds": 18.0, "runs": 4},
        )
        self.assertTrue(video["video"])
        self.assertIn("video", video["labels"])

    def test_missing_build_is_reported_not_invented(self):
        with tempfile.TemporaryDirectory() as temp:
            data = inventory.load_inventory(Path(temp), Path(temp) / "missing", ensure=False)
            self.assertEqual(data["tests"], [])
            self.assertTrue(data["warnings"])
            summary = inventory.summary(data)
            self.assertEqual(summary["tests"], 0)
            self.assertEqual(summary["estimated_full_seconds"], 0)


class SelectionTests(unittest.TestCase):
    def test_fast_excludes_graphics_and_keeps_headless_consumers(self):
        unit = fake_test("unit", "unit_tests", ("core",))
        graphics = fake_test("graphics", "graphics_tests", ("editor",), graphics=True)
        inventory_data = fake_inventory(
            [unit, graphics],
            source_by_target={
                "linblux_engine": ["src/core/value.cpp"],
                "linblux_studio": ["src/editor/value.cpp"],
                "unit_tests": ["tests/unit.cpp"],
                "graphics_tests": ["tests/graphics.cpp"],
            },
            deps_by_target={
                "unit_tests": ["linblux_engine"],
                "graphics_tests": ["linblux_engine", "linblux_studio"],
            },
            atlas_files=[
                {"path": "src/core/value.cpp", "module": "core"},
                {"path": "src/editor/value.cpp", "module": "editor"},
            ],
        )
        plan = selection.build_plan(inventory_data, fake_changes("src/core/value.cpp"), "fast")
        self.assertIn("unit", plan["selected_names"])
        self.assertNotIn("graphics", plan["selected_names"])
        self.assertIn("graphics", plan["filtered"])
        self.assertEqual(plan["graphics_count"], 0)
        self.assertIn("linblux_engine", plan["build_targets"])

    def test_impact_includes_selected_graphics_by_module(self):
        graphics = fake_test("editor_ui", "editor_ui_tests", ("editor",), graphics=True)
        unit = fake_test("editor_unit", "editor_tests", ("editor",))
        inventory_data = fake_inventory(
            [graphics, unit],
            source_by_target={"linblux_studio": ["src/editor/value.cpp"]},
            deps_by_target={"editor_ui_tests": ["linblux_studio"], "editor_tests": ["linblux_studio"]},
            atlas_files=[{"path": "src/editor/value.cpp", "module": "editor"}],
        )
        plan = selection.build_plan(inventory_data, fake_changes("src/editor/value.cpp", "editor"), "impact")
        self.assertIn("editor_ui", plan["selected_names"])
        self.assertIn("editor_unit", plan["selected_names"])
        self.assertEqual(plan["graphics_count"], 1)

    def test_no_tests_never_means_all_passed(self):
        inventory_data = fake_inventory(
            [],
            source_by_target={"linblux_engine": ["src/core/new.cpp"]},
            atlas_files=[{"path": "src/core/new.cpp", "module": "core"}],
        )
        plan = selection.build_plan(inventory_data, fake_changes("src/core/new.cpp"), "fast")
        self.assertEqual(plan["test_count"], 0)
        self.assertNotEqual(plan["coverage"], "known")
        self.assertTrue(plan["warnings"])

    def test_header_consumers_widen_targets(self):
        unit = fake_test("editor_unit", "editor_tests", ("editor",))
        inventory_data = fake_inventory(
            [unit],
            source_by_target={"linblux_studio": ["src/editor/value.cpp"]},
            deps_by_target={"editor_tests": ["linblux_studio"]},
            atlas_files=[
                {"path": "src/core/api.h", "module": "core"},
                {"path": "src/editor/value.cpp", "module": "editor"},
            ],
            atlas_relations=[
                {"source_path": "src/editor/value.cpp", "target": "src/core/api.h", "kind": "include"},
                {"source_path": "src/editor/a.cpp", "target": "src/core/api.h", "kind": "include"},
                {"source_path": "src/editor/b.cpp", "target": "src/core/api.h", "kind": "include"},
                {"source_path": "src/editor/c.cpp", "target": "src/core/api.h", "kind": "include"},
                {"source_path": "src/editor/d.cpp", "target": "src/core/api.h", "kind": "include"},
            ],
        )
        change = fake_changes("src/core/api.h", "core")
        change["files"][0]["path"] = "src/core/api.h"
        plan = selection.build_plan(inventory_data, change, "impact")
        self.assertIn("linblux_studio", plan["affected_targets"])
        self.assertIn("editor_unit", plan["selected_names"])
        self.assertTrue(any("header" in warning for warning in plan["warnings"]))

    def test_cmake_change_is_structural_and_requires_full_for_fast_certainty(self):
        unit = fake_test("unit", "unit_tests", ("core",))
        graphics = fake_test("graphics", "graphics_tests", ("editor",), graphics=True)
        inventory_data = fake_inventory([unit, graphics])
        change = fake_changes("CMakeLists.txt", "build")
        plan = selection.build_plan(inventory_data, change, "fast")
        self.assertTrue(plan["structural"])
        self.assertTrue(plan["configure_required"])
        self.assertNotEqual(plan["coverage"], "known")
        full = selection.build_plan(inventory_data, change, "full")
        self.assertEqual(full["test_count"], 2)
        self.assertIn("all", full["build_targets"])

    def test_full_selects_every_registered_test_even_unrelated(self):
        unit = fake_test("unit", "unit_tests", ("core",))
        unrelated_graphics = fake_test("graphics", "graphics_tests", ("editor",), graphics=True)
        inventory_data = fake_inventory([unit, unrelated_graphics])
        change = fake_changes("src/core/value.cpp", "core")
        fast = selection.build_plan(inventory_data, change, "fast")
        full = selection.build_plan(inventory_data, change, "full")
        self.assertEqual(fast["selected_names"], ["unit"])
        self.assertEqual(full["test_count"], 2)
        self.assertEqual(full["selected_names"], ["graphics", "unit"])
        self.assertEqual(full["graphics_count"], 1)
        self.assertEqual(full["coverage"], "known")
        self.assertEqual(full["build_targets"], ["all"])

    def test_plan_is_json_serializable(self):
        unit = fake_test("unit", "unit_tests", ("core",))
        inventory_data = fake_inventory([unit])
        plan = selection.build_plan(inventory_data, fake_changes("src/core/x.cpp"), "fast")
        json.dumps(plan)
        self.assertIn("Cobertura", selection.explain(plan))

    def test_direct_regression_is_required_and_module_guess_is_additional(self):
        direct = fake_test("direct", "direct_tests", ("core",))
        broad = fake_test("broad", "broad_tests", ("core",))
        inventory_data = fake_inventory(
            [broad, direct],
            source_by_target={
                "direct_tests": ["tests/direct.cpp"],
                "broad_tests": ["tests/broad.cpp"],
                "linblux_engine": ["src/core/value.cpp"],
            },
            deps_by_target={
                "direct_tests": ["linblux_engine"],
                "broad_tests": ["linblux_engine"],
            },
            atlas_files=[
                {"path": "tests/direct.cpp", "module": "core"},
                {"path": "src/core/value.cpp", "module": "core"},
            ],
        )
        plan = selection.build_plan(inventory_data, fake_changes("tests/direct.cpp", "core"), "fast")
        self.assertEqual(plan["required_names"], ["direct"])
        self.assertEqual(plan["recommended_names"], ["broad"])
        self.assertEqual(plan["required_count"], 1)
        self.assertEqual(plan["recommended_count"], 1)
        self.assertIn("[obrigatório]", selection.explain(plan))
        self.assertIn("[adicional]", selection.explain(plan))

    def test_full_selection_is_all_required(self):
        unit = fake_test("unit", "unit_tests", ("core",))
        graphics = fake_test("graphics", "graphics_tests", ("editor",), graphics=True)
        inventory_data = fake_inventory([unit, graphics])
        full = selection.build_plan(inventory_data, fake_changes("src/core/value.cpp", "core"), "full")
        self.assertEqual(full["required_count"], 2)
        self.assertEqual(full["recommended_count"], 0)


    def test_atlas_include_evidence_is_additive(self):
        consumer = fake_test("consumer", "consumer_tests", ("editor",))
        inventory_data = fake_inventory(
            [consumer],
            source_by_target={
                "linblux_studio": ["src/editor/value.cpp"],
                "consumer_tests": ["tests/consumer.cpp"],
            },
            deps_by_target={"consumer_tests": ["linblux_studio"]},
            test_files={"consumer": ["tests/consumer.cpp"]},
            atlas_files=[
                {"path": "src/editor/value.h", "module": "editor"},
                {"path": "src/editor/value.cpp", "module": "editor"},
                {"path": "tests/consumer.cpp", "module": "editor"},
            ],
            atlas_relations=[
                {"source_path": "tests/consumer.cpp", "target": "src/editor/value.h", "kind": "include"},
            ],
        )
        change = fake_changes("src/editor/value.cpp", "editor")
        legacy = selection.build_plan({**inventory_data, "atlas_relations": []}, change, "impact")
        plan = selection.build_plan(inventory_data, change, "impact")
        self.assertTrue(set(legacy["selected_names"]) <= set(plan["selected_names"]))
        legacy_reasons = {test["name"]: set(test["reasons"]) for test in legacy["selected_tests"]}
        for test in plan["selected_tests"]:
            self.assertTrue(
                legacy_reasons.get(test["name"], set()) <= set(test["reasons"]),
                test["name"],
            )
        self.assertIn("consumer", plan["selected_names"])
        self.assertEqual(plan["atlas_evidence"], "applied")
        entry = next(test for test in plan["selected_tests"] if test["name"] == "consumer")
        self.assertEqual(entry["evidence"], "direct")
        self.assertTrue(any("include direto do ATLAS" in reason for reason in entry["reasons"]))
        self.assertIn("consumer", plan["required_names"])

    def test_atlas_include_evidence_marks_transitive(self):
        deep = fake_test("deep", "deep_tests", ("editor",))
        inventory_data = fake_inventory(
            [deep],
            source_by_target={"deep_tests": ["tests/deep.cpp"]},
            deps_by_target={"deep_tests": []},
            test_files={"deep": ["tests/deep.cpp"]},
            atlas_files=[
                {"path": "src/editor/value.h", "module": "editor"},
                {"path": "src/editor/mid.h", "module": "editor"},
                {"path": "tests/deep.cpp", "module": "editor"},
            ],
            atlas_relations=[
                {"source_path": "src/editor/mid.h", "target": "src/editor/value.h", "kind": "include"},
                {"source_path": "tests/deep.cpp", "target": "src/editor/mid.h", "kind": "include"},
            ],
        )
        plan = selection.build_plan(inventory_data, fake_changes("src/editor/value.h", "editor"), "impact")
        self.assertIn("deep", plan["selected_names"])
        entry = next(test for test in plan["selected_tests"] if test["name"] == "deep")
        self.assertEqual(entry["evidence"], "transitive")
        self.assertTrue(any("include transitivo do ATLAS" in reason for reason in entry["reasons"]))

    def test_missing_atlas_graph_keeps_target_selection(self):
        unit = fake_test("unit", "unit_tests", ("core",))
        inventory_data = fake_inventory(
            [unit],
            source_by_target={
                "linblux_engine": ["src/core/value.cpp"],
                "unit_tests": ["tests/unit.cpp"],
            },
            deps_by_target={"unit_tests": ["linblux_engine"]},
            atlas_files=[{"path": "src/core/value.cpp", "module": "core"}],
        )
        inventory_data.pop("atlas_relations")
        plan = selection.build_plan(inventory_data, fake_changes("src/core/value.cpp"), "fast")
        self.assertIn("unit", plan["selected_names"])
        self.assertEqual(plan["atlas_evidence"], "unavailable")
        self.assertTrue(any("ATLAS" in warning for warning in plan["warnings"]))


class CacheTests(unittest.TestCase):
    def _last(self, **updates):
        base = {
            "status": "PASSED",
            "mode": "fast",
            "fingerprint": "abc",
            "selected_names": ["unit"],
            "coverage": "known",
            "timed_out": False,
            "run_id": "old",
        }
        base.update(updates)
        return base

    def test_reuse_rejects_failure_and_unknown_coverage(self):
        plan = {"selected_tests": [{"name": "unit", "reusable": True}], "coverage": "known"}
        ok, _ = cache.can_reuse(
            self._last(status="FAILED"), mode="fast", fingerprint="abc", selected=["unit"], plan=plan
        )
        self.assertFalse(ok)
        ok, _ = cache.can_reuse(
            self._last(coverage="baseline"), mode="fast", fingerprint="abc", selected=["unit"], plan=plan
        )
        self.assertFalse(ok)

    def test_reuse_rejects_volatile_and_changed_inputs(self):
        plan = {"selected_tests": [{"name": "ui", "reusable": False}], "coverage": "known"}
        ok, reason = cache.can_reuse(
            self._last(selected_names=["ui"]), mode="fast", fingerprint="abc", selected=["ui"], plan=plan
        )
        self.assertFalse(ok)
        self.assertIn("determin", reason)
        ok, _ = cache.can_reuse(
            self._last(), mode="fast", fingerprint="different", selected=["unit"], plan={
                "selected_tests": [{"name": "unit", "reusable": True}], "coverage": "known"
            }
        )
        self.assertFalse(ok)

    def test_reused_result_is_not_passed_now(self):
        old = self._last()
        old["tests"] = {"unit": {"status": "PASSED"}}
        old["build"] = {"status": "PASSED", "duration_seconds": 1.2}
        reused = cache.reused_result(old, fingerprint="abc", run_id="new")
        self.assertEqual(reused["status"], "REUSED")
        self.assertNotEqual(reused["status"], "PASSED")
        self.assertEqual(reused["tests"]["unit"]["status"], "REUSED")
        text = report.format_result(reused)
        self.assertIn("REUTILIZADO", text)
        self.assertIn("REUTILIZADA", text)
        self.assertNotIn("PASSOU AGORA", text)

    def test_partial_coverage_label_is_not_always_unknown(self):
        limited = {
            "status": "PASSED_PARTIAL",
            "plan": {"coverage": "partial"},
            "test": {"counts": {"executed": 6, "passed": 6}},
        }
        unknown = {
            "status": "PASSED_PARTIAL",
            "plan": {"coverage": "unknown"},
            "test": {"counts": {"executed": 6, "passed": 6}},
        }
        self.assertIn("COBERTURA LIMITADA", report.format_result(limited))
        self.assertNotIn("COBERTURA DESCONHECIDA", report.format_result(limited))
        self.assertIn("COBERTURA DESCONHECIDA", report.format_result(unknown))

    def test_fingerprint_changes_with_worktree_content(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            subprocess.run(["git", "init", "-q"], cwd=root, check=True)
            subprocess.run(["git", "config", "user.email", "turbo@example.invalid"], cwd=root, check=True)
            subprocess.run(["git", "config", "user.name", "Turbo Test"], cwd=root, check=True)
            (root / "value.txt").write_text("one\n", encoding="utf-8")
            subprocess.run(["git", "add", "value.txt"], cwd=root, check=True)
            subprocess.run(["git", "commit", "-qm", "initial"], cwd=root, check=True)
            first = changes.fingerprint(root)
            (root / "value.txt").write_text("two\n", encoding="utf-8")
            second = changes.fingerprint(root)
            self.assertNotEqual(first, second)

    def test_run_lock_releases(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with cache.RunLock(root):
                with self.assertRaises(cache.BusyError):
                    with cache.RunLock(root):
                        pass
            with cache.RunLock(root):
                pass


class RunnerTests(unittest.TestCase):
    def test_timeout_kills_child_group(self):
        with tempfile.TemporaryDirectory() as temp:
            temp_path = Path(temp)
            log = temp_path / "timeout.log"
            child_code = (
                "import subprocess,sys,time; "
                "p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)']); "
                "print(p.pid, flush=True); time.sleep(30)"
            )
            result = runner.run_command(
                [sys.executable, "-c", child_code],
                cwd=temp_path,
                log_path=log,
                timeout=0.8,
                name="timeout-test",
            )
            self.assertEqual(result["status"], "TIMEOUT")
            self.assertTrue(result["timed_out"])
            text = log.read_text(encoding="utf-8", errors="replace")
            self.assertIn("TURBO TIMEOUT", text)
            child_pid = None
            for line in text.splitlines():
                if line.isdigit():
                    child_pid = int(line)
                    break
            self.assertIsNotNone(child_pid)
            time.sleep(0.3)
            alive = True
            try:
                os.kill(child_pid, 0)
            except OSError:
                alive = False
            if alive:
                try:
                    os.kill(child_pid, 9)
                except OSError:
                    pass
            self.assertFalse(alive, "processo filho sobreviveu ao timeout")

    def test_exit_code_is_preserved(self):
        with tempfile.TemporaryDirectory() as temp:
            result = runner.run_command(
                ["sh", "-c", "exit 7"],
                cwd=Path(temp),
                log_path=Path(temp) / "exit.log",
                timeout=5,
                name="exit-test",
            )
            self.assertEqual(result["exit_code"], 7)
            self.assertEqual(result["status"], "FAILED")

    def test_excerpt_is_bounded(self):
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / "long.log"
            log.write_text(("ok\n" * 1000) + "error: sentinel\n" + ("tail\n" * 1000), encoding="utf-8")
            text = runner.excerpt(log, max_chars=300)
            self.assertLessEqual(len(text), 400)
            self.assertIn("sentinel", text)

    def test_supervised_tests_required_first_with_individual_logs(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            calls: list[str] = []

            def fake_ctest(_root, _build, selected, *, jobs, log_dir, timeout):
                name = selected[0]
                calls.append(name)
                log_dir.mkdir(parents=True, exist_ok=True)
                log = log_dir / "tests.log"
                log.write_text(f"{name} complete\n", encoding="utf-8")
                return {
                    "status": "PASSED",
                    "timed_out": False,
                    "duration_seconds": 0.01,
                    "log": str(log),
                    "junit": None,
                    "tests": {
                        name: {
                            "name": name,
                            "status": "PASSED",
                            "duration_seconds": 0.01,
                            "detail": None,
                        }
                    },
                }

            tests = [
                {
                    "name": "recommended",
                    "graphics": False,
                    "priority": "recommended",
                    "duration_seconds": 1,
                    "timeout_seconds": 5,
                },
                {
                    "name": "required",
                    "graphics": True,
                    "priority": "required",
                    "duration_seconds": 2,
                    "timeout_seconds": 5,
                },
            ]
            with mock.patch.object(runner, "run_ctest", side_effect=fake_ctest):
                result = runner.run_supervised_tests(
                    root,
                    root / "build",
                    tests,
                    log_dir=root / "logs",
                    budget_seconds=10,
                    graphics_timeout=5,
                    headless_timeout=5,
                )
            self.assertEqual(calls, ["required", "recommended"])
            self.assertEqual(result["status"], "PASSED")
            self.assertEqual(result["counts"]["passed"], 2)
            self.assertEqual(result["counts"]["budget_skipped"], 0)
            self.assertTrue((root / "logs" / "tests.log").is_file())
            self.assertTrue((root / "logs" / "tests" / "required" / "tests.log").is_file())
            self.assertIn("budget: 10.0s", (root / "logs" / "tests.log").read_text(encoding="utf-8"))

    def test_budget_exhaustion_is_pending_not_passed(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            tests = [
                {
                    "name": "required",
                    "graphics": True,
                    "priority": "required",
                    "duration_seconds": 2,
                    "timeout_seconds": 5,
                },
                {
                    "name": "recommended",
                    "graphics": False,
                    "priority": "recommended",
                    "duration_seconds": 1,
                    "timeout_seconds": 5,
                },
            ]
            with mock.patch.object(runner, "run_ctest", side_effect=AssertionError("must not run")):
                result = runner.run_supervised_tests(
                    root,
                    root / "build",
                    tests,
                    log_dir=root / "logs",
                    budget_seconds=0.1,
                    graphics_timeout=5,
                    headless_timeout=5,
                )
            self.assertEqual(result["status"], "BUDGET_EXHAUSTED")
            self.assertEqual(result["counts"]["budget_skipped"], 2)
            self.assertEqual(result["pending_required"], ["required"])
            self.assertFalse(result["tests"])
            self.assertEqual(result["exit_code"], 2)
            text = report.format_result(
                {
                    "status": "BUDGET_EXHAUSTED",
                    "plan": {"coverage": "partial", "budget_seconds": 0.1},
                    "test": result,
                }
            )
            self.assertIn("ORÇAMENTO ESGOTADO", text)
            self.assertIn("NÃO aprovados", text)

    def test_global_budget_interruption_is_distinct_from_test_timeout(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)

            def fake_ctest(_root, _build, selected, *, jobs, log_dir, timeout):
                name = selected[0]
                time.sleep(0.15)
                log_dir.mkdir(parents=True, exist_ok=True)
                log = log_dir / "tests.log"
                log.write_text("killed by budget\n", encoding="utf-8")
                return {
                    "status": "TIMEOUT",
                    "timed_out": True,
                    "duration_seconds": 0.15,
                    "log": str(log),
                    "junit": None,
                    "tests": {
                        name: {
                            "name": name,
                            "status": "TIMEOUT",
                            "duration_seconds": 0.15,
                            "detail": "timeout",
                        }
                    },
                }

            tests = [
                {
                    "name": "graphics",
                    "graphics": True,
                    "priority": "required",
                    "duration_seconds": 2,
                    "timeout_seconds": 5,
                }
            ]
            with mock.patch.object(runner, "run_ctest", side_effect=fake_ctest):
                result = runner.run_supervised_tests(
                    root,
                    root / "build",
                    tests,
                    log_dir=root / "logs",
                    budget_seconds=1.5,
                    graphics_timeout=5,
                    headless_timeout=5,
                )
            self.assertEqual(result["status"], "BUDGET_EXHAUSTED")
            self.assertEqual(result["counts"]["budget_interrupted"], 1)
            self.assertEqual(result["budget_interruptions"][0]["name"], "graphics")
            self.assertEqual(result["exit_code"], 2)

    def test_per_test_timeout_is_not_reported_as_budget(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)

            def fake_ctest(_root, _build, selected, *, jobs, log_dir, timeout):
                name = selected[0]
                time.sleep(0.15)
                log_dir.mkdir(parents=True, exist_ok=True)
                log = log_dir / "tests.log"
                log.write_text("killed by individual cap\n", encoding="utf-8")
                return {
                    "status": "TIMEOUT",
                    "timed_out": True,
                    "duration_seconds": 0.15,
                    "log": str(log),
                    "junit": None,
                    "tests": {
                        name: {
                            "name": name,
                            "status": "TIMEOUT",
                            "duration_seconds": 0.15,
                            "detail": "timeout individual",
                        }
                    },
                }

            tests = [
                {
                    "name": "headless",
                    "graphics": False,
                    "priority": "required",
                    "duration_seconds": 0.1,
                    "timeout_seconds": 5,
                }
            ]
            with mock.patch.object(runner, "run_ctest", side_effect=fake_ctest):
                result = runner.run_supervised_tests(
                    root,
                    root / "build",
                    tests,
                    log_dir=root / "logs",
                    budget_seconds=20,
                    graphics_timeout=5,
                    headless_timeout=0.05,
                )
            self.assertEqual(result["status"], "TIMEOUT")
            self.assertEqual(result["counts"]["timeout"], 1)
            self.assertEqual(result["counts"]["budget_interrupted"], 0)
            self.assertFalse(result["budget_exhausted"])
            self.assertEqual(result["exit_code"], 124)

    def test_cmake_configure_failure_is_reported_with_log(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\nproject(turbo_bad)\nthis_is_not_cmake()\n",
                encoding="utf-8",
            )
            result = runner.build(
                root,
                root / "build",
                [],
                jobs=1,
                log_dir=root / "logs",
                configure=True,
            )
            self.assertEqual(result["status"], "FAILED")
            self.assertTrue((root / "logs" / "configure.log").is_file())
            self.assertTrue(result["commands"])

    def test_compile_failure_keeps_log(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.20)\nproject(turbo_compile)\n"
                "add_executable(bad main.cpp)\n",
                encoding="utf-8",
            )
            (root / "main.cpp").write_text("this is not valid C++\n", encoding="utf-8")
            result = runner.build(
                root,
                root / "build",
                ["bad"],
                jobs=1,
                log_dir=root / "logs",
                configure=True,
            )
            self.assertEqual(result["status"], "FAILED")
            self.assertTrue((root / "logs" / "build.log").is_file())


class DocsAndOrganizeTests(unittest.TestCase):
    def test_docs_and_json_validation(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "docs").mkdir()
            (root / "docs" / "ok.md").write_text("[x](ok.md)\n", encoding="utf-8")
            (root / "docs" / "missing.md").write_text("[x](absent.md)\n", encoding="utf-8")
            (root / "data.json").write_text('{"ok": true}\n', encoding="utf-8")
            good = docscheck.check_changed(root, [{"path": "docs/ok.md"}, {"path": "data.json"}])
            self.assertEqual(good["status"], "DOCS_OK")
            bad = docscheck.check_changed(root, [{"path": "docs/missing.md"}])
            self.assertEqual(bad["status"], "FAILED")
            (root / "data.json").write_text("{bad", encoding="utf-8")
            invalid = docscheck.check_changed(root, [{"path": "data.json"}])
            self.assertEqual(invalid["status"], "FAILED")

    def test_organize_is_read_only(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            subprocess.run(["git", "init", "-q"], cwd=root, check=True)
            subprocess.run(["git", "config", "user.email", "turbo@example.invalid"], cwd=root, check=True)
            subprocess.run(["git", "config", "user.name", "Turbo Test"], cwd=root, check=True)
            (root / "src").mkdir()
            (root / "src" / "a.cpp").write_text("int a;\n", encoding="utf-8")
            (root / "src" / "b.cpp").write_text("int a;\n", encoding="utf-8")
            (root / "build.log").write_text("generated\n", encoding="utf-8")
            subprocess.run(["git", "add", "."], cwd=root, check=True)
            subprocess.run(["git", "commit", "-qm", "fixture"], cwd=root, check=True)
            before = sorted(path.name for path in (root / "src").iterdir())
            audit = organize.audit(root)
            after = sorted(path.name for path in (root / "src").iterdir())
            self.assertEqual(before, after)
            self.assertFalse(audit["apply_supported"])
            self.assertTrue(any(item["kind"] == "duplicate-candidates" for item in audit["findings"]))


class ReportTests(unittest.TestCase):
    def test_failure_shows_first_error_with_location(self):
        detail = (
            "In file included from src/a.cpp:1:\n"
            "src/core/value.cpp:42:7: error: 'value_' was not declared in this scope\n"
            "src/core/value.cpp:44:7: error: expected ';' before '}' token\n"
            "make[2]: *** [CMakeFiles/x.dir/build.make:76: CMakeFiles/x.dir/src/core/value.cpp.o] Error 1\n"
        )
        text = report.format_result(
            {
                "status": "BUILD_FAILED",
                "excerpt": detail,
                "build": {"status": "FAILED", "log": "/tmp/build.log"},
            }
        )
        self.assertIn("Tipo: compilação", text)
        self.assertIn("Provável causa: 'value_' was not declared in this scope", text)
        self.assertIn("Arquivo: src/core/value.cpp:42:7", text)
        self.assertIn("/tmp/build.log", text)
        self.assertIn("Próxima ação:", text)
        self.assertNotIn("expected ';' before", text.split("Provável causa")[1].split("\n")[0])

    def test_failure_without_pattern_keeps_detail(self):
        text = report.format_result(
            {
                "status": "BUILD_FAILED",
                "excerpt": "algo deu errado sem formato reconhecido",
                "build": {"status": "FAILED"},
            }
        )
        self.assertIn("Tipo: não identificado", text)
        self.assertIn("algo deu errado", text)

    def test_test_failure_is_labelled_with_target(self):
        text = report.format_result(
            {
                "status": "FAILED",
                "test": {
                    "counts": {"selected": 1, "executed": 1, "failed": 1},
                    "tests": {
                        "widget_test": {
                            "status": "FAILED",
                            "detail": "Assertion 'value == 7' failed at src/widget.cpp:12",
                            "log": "/tmp/t.log",
                            "target": "widget_tests",
                        }
                    },
                },
            }
        )
        self.assertIn("Tipo: teste", text)
        self.assertIn("Target: widget_tests", text)
        self.assertIn("/tmp/t.log", text)

    def test_excerpt_prefers_first_error(self):
        with tempfile.TemporaryDirectory() as temp:
            log = Path(temp) / "cascade.log"
            log.write_text("error: root cause here\n" + "error: consequence\n" * 50, encoding="utf-8")
            text = runner.excerpt(log, max_chars=200)
            self.assertIn("root cause here", text)
            self.assertIn("primeiras", text)


class CliContractTests(unittest.TestCase):
    def test_cli_json_contract(self):
        result = subprocess.run(
            [sys.executable, str(TURBO_PATH / "turbo.py"), "doctor", "--json", "--no-atlas-update"],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=30,
        )
        self.assertIn(result.returncode, (0, 1))
        payload = json.loads(result.stdout)
        self.assertIn("ok", payload)
        self.assertEqual(payload["command"], "doctor")
        self.assertIn("data", payload)

    def test_cli_help_lists_required_modes(self):
        result = subprocess.run(
            [sys.executable, str(TURBO_PATH / "turbo.py"), "test", "--help"],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=10,
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("--mode", result.stdout)
        self.assertIn("fast", result.stdout)
        self.assertIn("impact", result.stdout)
        self.assertIn("full", result.stdout)
        self.assertIn("--budget", result.stdout)
        self.assertIn("--per-test-timeout", result.stdout)
        self.assertIn("--timeout", result.stdout)


if __name__ == "__main__":
    unittest.main()
