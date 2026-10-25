"""Real CMake/CTest/ATLAS inventory used by TURBO.

No test is classified as fast or smoke merely because its name is short.  The
classification is derived from the CTest command/environment/properties, the
measured CTestCostData durations and the ATLAS test/source/target index.
"""

from __future__ import annotations

import json
import os
import sqlite3
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

from . import config

# Explicit module ownership for the tests that exist in this repository.  The
# fallback below still classifies new tests from names/commands.
TEST_MODULES = {
    "linblux_player_help": ("player",),
    "linblux_player_validate": ("player", "serialization"),
    "linblux_experiences": ("experiences", "serialization"),
    "linblux_studio": ("editor", "game"),
    "linblux_game": ("game", "scene", "physics"),
    "linblux_tests": ("core", "scene", "physics", "scripting", "serialization"),
    "linblux_manipulation": ("editor", "scene", "physics"),
    "linblux_spawn": ("game", "scene", "objects"),
    "linblux_startup_no_display": ("app", "core"),
    "linblux_help": ("app",),
    "linblux_decal_render": ("rendering", "game"),
    "linblux_rig_image": ("editor", "rendering", "rig"),
    "linblux_gizmo_visual": ("editor", "scene", "rendering"),
    "linblux_experience_ui": ("experiences", "ui", "app"),
    "linblux_rml_ui": ("ui", "editor"),
    "linblux_rig_decals_ui": ("editor", "rendering", "rig"),
    "linblux_mouse_lock": ("editor", "ui", "game"),
    "linblux_manipulation_ui": ("editor", "scene", "ui"),
    "linblux_rig_input": ("editor", "rig", "ui"),
    "linblux_visual_studio": ("editor", "rendering", "ui"),
    "linblux_sky_capture": ("rendering", "scene"),
    "linblux_graphics_quality": ("rendering", "ui", "game"),
    "linblux_graphics_quality_1080": ("rendering", "ui", "game"),
    "linblux_studio_ui": ("editor", "ui"),
    "linblux_game_ui": ("game", "ui"),
    "linblux_editor_input": ("editor", "ui"),
    "linblux_experience_startup": ("experiences", "app", "ui"),
    "linblux_editor_restart": ("editor", "app", "serialization"),
    "linblux_video": ("media",),
    "linblux_home_video": ("media", "ui", "app"),
    "linblux_defaults": ("core", "experiences", "serialization"),
    "linblux_engine_default_lifecycle": ("core", "experiences", "serialization"),
    "linblux_engine_default_restart": ("core", "experiences", "serialization", "app"),
    "linblux_atlas": ("atlas", "tooling"),
}


def _run(command: list[str], cwd: Path, timeout: int = 60) -> subprocess.CompletedProcess:
    return subprocess.run(
        command,
        cwd=str(cwd),
        text=True,
        capture_output=True,
        timeout=timeout,
        check=False,
    )


def _atlas_python(root: Path) -> Path:
    return root / "tools" / "atlas" / "atlas.py"


def ensure_atlas(root: Path, force: bool = False) -> dict:
    """Make sure ATLAS is usable and current, without writing outside .atlas."""
    atlas = _atlas_python(root)
    warning = None
    if not atlas.is_file():
        return {"ok": False, "updated": False, "warning": "tools/atlas/atlas.py ausente"}
    db = root / ".atlas" / "index.sqlite"
    should_update = force or not db.is_file()
    stale = None
    if not should_update:
        try:
            status = _run([sys.executable, str(atlas), "status", "--json"], root, timeout=30)
            payload = json.loads(status.stdout or "{}")
            data = payload.get("data") or {}
            stale = data.get("stale_estimate")
            should_update = not data.get("compatible", True) or bool(stale)
        except (OSError, ValueError, subprocess.SubprocessError):
            should_update = True
    if should_update:
        try:
            result = _run([sys.executable, str(atlas), "update"], root, timeout=180)
        except (OSError, subprocess.SubprocessError) as exc:
            return {"ok": False, "updated": False, "warning": f"ATLAS update falhou: {exc}"}
        if result.returncode != 0:
            warning = (result.stderr or result.stdout).strip()[:500] or "ATLAS update falhou"
            return {"ok": False, "updated": True, "warning": warning}
        try:
            status = _run([sys.executable, str(atlas), "status", "--json"], root, timeout=30)
            payload = json.loads(status.stdout or "{}")
            data = payload.get("data") or {}
            return {
                "ok": bool(data.get("exists")) and bool(data.get("compatible", True)),
                "updated": True,
                "stale_estimate": data.get("stale_estimate"),
                "counts": data.get("counts"),
                "warning": warning,
            }
        except (OSError, ValueError, subprocess.SubprocessError):
            return {"ok": True, "updated": True, "warning": warning}
    return {"ok": True, "updated": False, "stale_estimate": stale}


def atlas_snapshot(root: Path, ensure: bool = True) -> dict:
    info = ensure_atlas(root) if ensure else {"ok": True, "updated": False}
    db = root / ".atlas" / "index.sqlite"
    snapshot = {
        "info": info,
        "database": str(db),
        "tests": [],
        "targets": [],
        "target_sources": [],
        "test_files": [],
        "files": [],
        "relations": [],
        "warning": info.get("warning"),
    }
    if not db.is_file():
        snapshot["warning"] = "ATLAS sem banco; seleção degradada"
        return snapshot
    try:
        uri = f"file:{db}?mode=ro"
        conn = sqlite3.connect(uri, uri=True)
        conn.row_factory = sqlite3.Row
        try:
            snapshot["tests"] = [dict(row) for row in conn.execute("SELECT * FROM tests ORDER BY name")]
            snapshot["targets"] = [dict(row) for row in conn.execute("SELECT * FROM cmake_targets ORDER BY name")]
            snapshot["target_sources"] = [
                dict(row) for row in conn.execute("SELECT * FROM target_sources ORDER BY target_id, kind, path")
            ]
            snapshot["test_files"] = [
                dict(row) for row in conn.execute("SELECT * FROM test_files ORDER BY test_id, path")
            ]
            snapshot["files"] = [
                dict(row) for row in conn.execute("SELECT id, path, module, area, language, size, line_count FROM files")
            ]
            snapshot["relations"] = [
                dict(row)
                for row in conn.execute(
                    "SELECT r.source_file, r.target, r.kind, r.confidence, f.path AS source_path "
                    "FROM relations r JOIN files f ON f.id = r.source_file"
                )
            ]
        finally:
            conn.close()
    except (OSError, sqlite3.Error) as exc:
        snapshot["warning"] = f"ATLAS indisponível: {exc}"
    return snapshot


def parse_cost_data(build_dir: Path) -> dict[str, dict]:
    path = build_dir / "Testing" / "Temporary" / "CTestCostData.txt"
    result: dict[str, dict] = {}
    if not path.is_file():
        return result
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return result
    for line in lines:
        fields = line.split()
        if len(fields) < 3 or fields[0].startswith("#"):
            continue
        name = fields[0]
        try:
            runs = int(fields[1])
            cost = float(fields[2])
        except ValueError:
            continue
        result[name] = {"runs": runs, "cost_seconds": cost, "evidence": str(path)}
    return result


def show_only_tests(build_dir: Path, timeout: int = 60) -> tuple[list[dict], str | None]:
    if not build_dir.is_dir():
        return [], f"diretório de build ausente: {build_dir}"
    try:
        result = _run(
            ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"],
            build_dir,
            timeout=timeout,
        )
    except (OSError, subprocess.SubprocessError) as exc:
        return [], f"ctest indisponível: {exc}"
    raw = result.stdout or ""
    try:
        payload = json.loads(raw)
    except ValueError:
        # CTest can prepend an informational line in some releases.
        start = raw.find("{")
        try:
            payload = json.loads(raw[start:]) if start >= 0 else {}
        except ValueError:
            detail = (result.stderr or raw).strip()[:500]
            return [], f"JSON do ctest inválido: {detail}"
    tests = []
    for item in payload.get("tests", []):
        props = {}
        for prop in item.get("properties", []):
            props[prop.get("name")] = prop.get("value")
        command = item.get("command") or []
        if not command and props.get("COMMAND"):
            command = props["COMMAND"]
        env = props.get("ENVIRONMENT") or []
        if isinstance(env, str):
            env = [env]
        tests.append(
            {
                "name": item.get("name"),
                "command": command,
                "command_text": " ".join(str(part) for part in command),
                "environment": env,
                "properties": props,
                "timeout": props.get("TIMEOUT"),
                "labels": props.get("LABELS") or [],
                "run_serial": props.get("RUN_SERIAL"),
                "processors": props.get("PROCESSORS"),
            }
        )
    return tests, None


def _normalize_labels(value) -> list[str]:
    if not value:
        return []
    if isinstance(value, str):
        value = [value]
    result = []
    for item in value:
        result.extend(str(item).split(";"))
    return sorted({item.strip() for item in result if item.strip()})


def _infer_modules(name: str) -> list[str]:
    if name in TEST_MODULES:
        return list(TEST_MODULES[name])
    tokens = set(name.replace("linblux_", "").split("_"))
    modules = []
    for module, hints in config.MODULE_TEST_HINTS.items():
        if tokens.intersection(hints):
            modules.append(module)
    return modules or ["core"]


def classify_test(name: str, ctest: dict, atlas_test: dict | None, cost: dict | None) -> dict:
    command_text = ctest.get("command_text", "")
    environment = " ".join(str(item) for item in ctest.get("environment", []))
    labels = set(_normalize_labels(ctest.get("labels")))
    lower = name.lower()
    graphics = any(marker in command_text or marker in environment for marker in config.GRAPHICS_MARKERS)
    graphics = graphics or any(
        token in lower
        for token in (
            "_ui",
            "visual",
            "graphics",
            "gizmo",
            "render",
            "sky_capture",
            "decal_render",
            "rig_image",
            "mouse_lock",
            "home_video",
        )
    )
    video = "video" in lower and "home_video" not in lower
    smoke = any(token in lower for token in ("_help", "_validate", "startup_no_display"))
    integration = graphics or video or "integration" in labels
    if graphics:
        labels.update({"graphics", "integration", "display"})
    if video:
        labels.update({"video", "integration", "headless"})
    if smoke:
        labels.add("smoke")
    if not graphics and not video:
        labels.add("headless")
    if not graphics and not video and not smoke:
        labels.add("unit")
    if "unit" in labels and "integration" not in labels:
        labels.add("headless")
    duration = float((cost or {}).get("cost_seconds") or 0)
    if duration >= 10:
        labels.add("slow")
    if graphics and duration >= 10:
        labels.add("slow")
    modules = _infer_modules(name)
    labels.update(module for module in modules if module not in {"tooling"})
    timeout = ctest.get("timeout") or (atlas_test or {}).get("timeout") or config.DEFAULT_TEST_TIMEOUT
    reusable = (
        not graphics
        and not video
        and bool(labels.intersection({"unit", "smoke"}))
        and duration < 60
    )
    target = (atlas_test or {}).get("target")
    source_path = (atlas_test or {}).get("source_path")
    env_pairs = {}
    for item in ctest.get("environment", []):
        if "=" in str(item):
            key, value = str(item).split("=", 1)
            env_pairs[key] = value
    return {
        "name": name,
        "target": target,
        "source_path": source_path,
        "modules": modules,
        "labels": sorted(labels),
        "ctest_labels": _normalize_labels(ctest.get("labels")),
        "graphics": graphics,
        "headless": not graphics,
        "video": video,
        "smoke": smoke,
        "integration": integration,
        "slow": "slow" in labels,
        "reusable": reusable,
        "duration_seconds": duration,
        "runs": int((cost or {}).get("runs") or 0),
        "timeout_seconds": float(timeout) if timeout else config.DEFAULT_TEST_TIMEOUT,
        "command": ctest.get("command", []),
        "command_text": command_text,
        "environment": env_pairs,
        "labels_source": "ctest+inventory",
        "evidence": {
            "ctest": "command/environment/properties",
            "cost": (cost or {}).get("evidence"),
            "atlas": bool(atlas_test),
        },
    }


def load_inventory(
    root: Path,
    build_dir: Path,
    *,
    ensure: bool = True,
    ctest_timeout: int = 60,
) -> dict:
    raw_tests, ctest_warning = show_only_tests(build_dir, timeout=ctest_timeout)
    costs = parse_cost_data(build_dir)
    atlas = atlas_snapshot(root, ensure=ensure)
    atlas_tests = {row["name"]: row for row in atlas.get("tests", [])}
    tests = []
    for raw in raw_tests:
        name = raw["name"]
        tests.append(classify_test(name, raw, atlas_tests.get(name), costs.get(name)))
    # Preserve conditional tests known to ATLAS but not registered in this
    # build.  They are NOT runnable, but TURBO must not silently hide them.
    missing = []
    registered = {item["name"] for item in tests}
    for name in atlas_tests:
        if name not in registered:
            missing.append(name)
    targets = atlas.get("targets", [])
    source_by_target = defaultdict(list)
    deps_by_target = defaultdict(list)
    for row in atlas.get("target_sources", []):
        target = next((t["name"] for t in targets if t["id"] == row["target_id"]), None)
        if not target:
            continue
        if row["kind"] == "source":
            source_by_target[target].append(row["path"])
        elif row["kind"] == "dependency":
            deps_by_target[target].append(row["path"])
    test_files = defaultdict(list)
    test_by_id = {row["id"]: row["name"] for row in atlas.get("tests", [])}
    for row in atlas.get("test_files", []):
        name = test_by_id.get(row["test_id"])
        if name:
            test_files[name].append(row["path"])
    warnings = []
    if ctest_warning:
        warnings.append(ctest_warning)
    if atlas.get("warning"):
        warnings.append(atlas["warning"])
    if missing:
        warnings.append(f"{len(missing)} teste(s) do ATLAS não registrados neste build")
    return {
        "root": str(root),
        "build_dir": str(build_dir),
        "tests": sorted(tests, key=lambda item: item["name"]),
        "missing_tests": missing,
        "targets": targets,
        "source_by_target": dict(source_by_target),
        "deps_by_target": dict(deps_by_target),
        "test_files": dict(test_files),
        "atlas_files": atlas.get("files", []),
        "atlas_relations": atlas.get("relations", []),
        "atlas_info": atlas.get("info", {}),
        "warnings": warnings,
        "generated_at": __import__("datetime").datetime.now(
            __import__("datetime").timezone.utc
        ).isoformat(timespec="seconds"),
        "cost_data": costs,
    }


def summary(inventory: dict) -> dict:
    tests = inventory.get("tests", [])
    return {
        "tests": len(tests),
        "graphics": sum(1 for test in tests if test["graphics"]),
        "headless": sum(1 for test in tests if test["headless"]),
        "slow": sum(1 for test in tests if test["slow"]),
        "reusable": sum(1 for test in tests if test["reusable"]),
        "estimated_full_seconds": round(sum(test["duration_seconds"] for test in tests), 3),
        "missing": len(inventory.get("missing_tests", [])),
        "warnings": inventory.get("warnings", []),
        "generated_at": inventory.get("generated_at"),
    }
