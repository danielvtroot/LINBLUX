"""Configuration and small environment helpers for ATLAS TURBO."""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

TURBO_VERSION = "0.1.0"
TURBO_SCHEMA_VERSION = 1
TURBO_DIR_NAME = ".turbo"
DEFAULT_BUILD_DIR = "build-linblux"
DEFAULT_JOBS = 2
DEFAULT_TEST_TIMEOUT = 300
# IMPACT is risk-based, not a disguised FULL.  Four graphical integrations keep
# a normal editor change near a couple of minutes on software rendering; pass
# --max-graphics with a larger value when the change requires more coverage.
IMPACT_DEFAULT_GRAPHICS = 4
BUILD_TIMEOUT = 1800
MAX_EXCERPT_CHARS = 4000
MAX_REPORT_CHARS = 9000
HISTORY_LIMIT = 120
IGNORED_CHANGE_PREFIXES = (
    ".turbo/",
    ".atlas/",
    "build/",
    "build-",
    "build-no-video/",
    "cache/",
    ".cache/",
    "__pycache__/",
)
GRAPHICS_MARKERS = (
    "xvfb-run",
    "x11",
    "opengl",
    "framebuffer",
    "LIBGL_ALWAYS_SOFTWARE",
)
MODULE_TEST_HINTS = {
    "app": ("app", "startup", "help", "restart"),
    "core": ("core", "tests", "defaults"),
    "editor": ("studio", "editor", "gizmo", "manipulation", "input"),
    "experiences": ("experience", "defaults", "startup"),
    "game": ("game", "spawn", "rig", "camera"),
    "media": ("video", "home_video"),
    "objects": ("tests", "spawn", "manipulation"),
    "physics": ("tests", "manipulation", "spawn"),
    "player": ("player",),
    "rendering": ("render", "visual", "graphics", "gizmo", "rig", "sky", "decal", "camera"),
    "scene": ("tests", "spawn", "manipulation"),
    "scripting": ("tests", "experiences"),
    "serialization": ("tests", "defaults", "experience", "restart"),
    "ui": ("ui", "rml", "studio", "game", "experience"),
    "ui-assets": ("ui", "rml", "studio"),
    "assets-defaults": ("defaults", "experience", "startup"),
    "assets-templates": ("experience", "studio"),
    "assets-rigs": ("rig",),
    "atlas": ("atlas",),
    "docs": (),
    "tests": ("tests",),
    "build": (),
    "root": ("startup", "help"),
}
MODULE_HINTS = {
    "src/app/": "app",
    "src/core/": "core",
    "src/editor/": "editor",
    "src/experiences/": "experiences",
    "src/game/": "game",
    "src/media/": "media",
    "src/objects/": "objects",
    "src/physics/": "physics",
    "src/player/": "player",
    "src/rendering/": "rendering",
    "src/scene/": "scene",
    "src/scripting/": "scripting",
    "src/serialization/": "serialization",
    "src/ui/": "ui",
    "src/main.cpp": "app",
    "tests/": "tests",
    "docs/": "docs",
    "tools/": "tooling",
    "assets/ui/": "ui-assets",
    "assets/defaults/": "assets-defaults",
    "assets/templates/": "assets-templates",
    "assets/rigs/": "assets-rigs",
}
ASSET_TEST_RULES = (
    ("assets/ui/", ("linblux_rml_ui", "linblux_studio_ui")),
    ("assets/defaults/", ("linblux_defaults", "linblux_experiences")),
    ("assets/templates/", ("linblux_experience_ui", "linblux_experiences")),
    ("assets/rigs/", ("linblux_rig_image", "linblux_rig_input")),
    ("midia/", ("linblux_video", "linblux_home_video")),
)
DOC_SUFFIXES = (".md", ".markdown", ".txt", ".rst")


def repo_root(value: str | Path | None = None) -> Path:
    root = Path(value or ".").expanduser().resolve()
    if not root.is_dir():
        raise ValueError(f"raiz invalida ou inexistente: {root}")
    return root


def turbo_dir(root: Path) -> Path:
    return root / TURBO_DIR_NAME


def default_build_dir(root: Path) -> Path:
    override = os.environ.get("LINBLUX_BUILD_DIR")
    return (root / (override or DEFAULT_BUILD_DIR)).resolve()


def safe_jobs(value: int | None = None) -> int:
    if value is not None:
        return max(1, min(int(value), 16))
    env = os.environ.get("LINBLUX_TURBO_JOBS")
    if env and env.isdigit():
        return max(1, min(int(env), 16))
    # Keep memory headroom: one compiler per two CPUs, at most four.
    cpus = os.cpu_count() or 2
    return max(1, min(4, cpus // 2))


def tool_versions() -> dict:
    tools = {
        "python": sys.version.split()[0],
        "cmake": _first_line(["cmake", "--version"]),
        "ctest": _first_line(["ctest", "--version"]),
        "ninja": _which("ninja"),
        "ccache": _which("ccache"),
        "clangd": _which("clangd"),
        "rg": _which("rg"),
        "ctags": _which("ctags"),
        "xvfb-run": _which("xvfb-run"),
        "git": _which("git"),
        "make": _which("make"),
    }
    return tools


def _which(name: str) -> str | None:
    return shutil.which(name)


def _first_line(command: list[str]) -> str | None:
    try:
        result = subprocess.run(
            command,
            text=True,
            capture_output=True,
            timeout=10,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return None
    output = (result.stdout or result.stderr).strip().splitlines()
    return output[0].strip() if output else None


def is_doc_path(rel: str) -> bool:
    # CMakeLists.txt ends in .txt but is build configuration, not documentation.
    if rel == "CMakeLists.txt" or rel == "CMakePresets.json":
        return False
    return rel.startswith("docs/") or rel.lower().endswith(DOC_SUFFIXES)


def module_for_path(rel: str) -> str:
    for prefix, module in MODULE_HINTS.items():
        if rel == prefix or rel.startswith(prefix):
            return module
    return "root"


def ignored_change(rel: str) -> bool:
    normalized = rel.replace(os.sep, "/")
    if normalized.startswith(IGNORED_CHANGE_PREFIXES):
        return True
    parts = normalized.split("/")
    return any(part in {"build", "__pycache__", ".atlas", ".turbo"} for part in parts)


def environment_status() -> dict:
    """Return facts that affect the validation strategy."""
    tools = tool_versions()
    memory_kib = None
    try:
        for line in Path("/proc/meminfo").read_text(encoding="utf-8").splitlines():
            if line.startswith("MemAvailable:"):
                memory_kib = int(line.split()[1])
                break
    except (OSError, ValueError, IndexError):
        pass
    return {
        "tools": tools,
        "cpus": os.cpu_count(),
        "memory_available_kib": memory_kib,
        "default_jobs": safe_jobs(),
        "generator": None,
        "ccache": tools.get("ccache"),
        "ninja": tools.get("ninja"),
    }
