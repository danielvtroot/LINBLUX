"""Configuration, language detection, ignore rules and module mapping."""

from __future__ import annotations

import json
import unicodedata
from pathlib import Path

from . import INDEX_SCHEMA_VERSION

CACHE_DIR = ".atlas"
DB_NAME = "index.sqlite"
CONFIG_NAME = "config.json"
# Single source of truth for the on-disk schema lives in atlas_lib/__init__.py.
INDEX_VERSION = INDEX_SCHEMA_VERSION
MAX_FILE_BYTES = 2 * 1024 * 1024
MAX_FTS_CHARS = 400_000

SOURCE_EXTENSIONS = {
    ".c": "c",
    ".cc": "cpp",
    ".cpp": "cpp",
    ".cxx": "cpp",
    ".h": "cpp",
    ".hh": "cpp",
    ".hpp": "cpp",
    ".hxx": "cpp",
    ".glsl": "glsl",
    ".vert": "glsl",
    ".frag": "glsl",
    ".lua": "lua",
    ".py": "python",
    ".sh": "shell",
    ".rml": "rml",
    ".rcss": "css",
    ".css": "css",
    ".md": "markdown",
    ".json": "json",
    ".cmake": "cmake",
    ".xml": "xml",
    ".desktop": "desktop",
}

TEXT_NAMES = {
    "CMakeLists.txt": "cmake",
    "AGENTS.md": "markdown",
    "README.md": "markdown",
    "CONTRIBUTING.md": "markdown",
    "LICENSE": "text",
}

SEARCHABLE_JSON_PREFIXES = (
    "assets/ui/",
    "assets/templates/",
    "assets/defaults/",
    "docs/",
    "tools/atlas/data/",
)

# Prefixes never indexed. The ATLAS fixture is copied into an isolated root by
# tests and must not leak fake targets into the real inventory. ``prototype/`` is
# an obsolete stub that no CMake target references (audit ATM-2.0, B1).
IGNORED_PREFIXES = (
    "tests/atlas/fixture/",
    "prototype/",
)

IGNORED_DIRS = {
    ".git",
    ".atlas",
    ".turbo",
    ".cache",
    "cache",
    "build",
    "build-linblux",
    "build-no-video",
    "Testing",
    "__pycache__",
    ".freebuff",
    ".codex",
    ".agents",
    "referencias",
    "projects",
    "third_party",
    "_deps",
    "node_modules",
    ".venv",
    "venv",
    "dist",
    "backups",
}

IGNORED_NAMES = {
    ".env",
    ".env.local",
    "compile_commands.json",
    "loblox_imgui.ini",
}

IGNORED_SUFFIXES = {
    ".png",
    ".jpg",
    ".jpeg",
    ".webp",
    ".gif",
    ".bmp",
    ".ico",
    ".mp4",
    ".webm",
    ".avi",
    ".mov",
    ".wav",
    ".ogg",
    ".mp3",
    ".zip",
    ".gz",
    ".xz",
    ".7z",
    ".lo",
    ".o",
    ".a",
    ".so",
    ".pyc",
    ".sqlite",
    ".sqlite3",
    ".xwd",
    ".core",
    ".pem",
    ".key",
}

# Modules whose contracts are expected in docs/atlas/module-contracts.
CONTRACT_MODULES = (
    "app",
    "core",
    "editor",
    "experiences",
    "game",
    "media",
    "objects",
    "physics",
    "player",
    "rendering",
    "scene",
    "scripting",
    "serialization",
    "ui",
)


def repo_root(path: str | Path | None = None) -> Path:
    """Resolve a project root and validate that it is a directory."""
    root = Path(path or ".").expanduser().resolve()
    if not root.is_dir():
        raise ValueError(f"raiz invalida ou inexistente: {root}")
    return root


def cache_dir(root: Path) -> Path:
    return root / CACHE_DIR


def db_path(root: Path) -> Path:
    return cache_dir(root) / DB_NAME


def load_user_config(root: Path) -> dict:
    """Optional local overrides. Never required for operation."""
    path = cache_dir(root) / CONFIG_NAME
    if not path.is_file():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def detect_language(path: Path) -> str | None:
    if path.name in TEXT_NAMES:
        return TEXT_NAMES[path.name]
    return SOURCE_EXTENSIONS.get(path.suffix.lower())


def should_index(rel: str, path: Path, user_config: dict | None = None) -> bool:
    """Return True only for project-owned, text-like files."""
    parts = Path(rel).parts
    if not parts:
        return False
    if parts[0] in IGNORED_DIRS:
        return False
    if any(part in IGNORED_DIRS or part.startswith("build-") for part in parts):
        return False
    if any(rel.startswith(prefix) for prefix in IGNORED_PREFIXES):
        return False
    if path.name in IGNORED_NAMES or path.name.startswith(".env."):
        return False
    if path.suffix.lower() in IGNORED_SUFFIXES:
        return False
    if path.name.endswith((".tmp", ".log", ".bak", ".orig")):
        return False
    language = detect_language(path)
    if language is None:
        return False
    if language == "json" and not rel.startswith(SEARCHABLE_JSON_PREFIXES):
        return False
    if language == "desktop" and not rel.startswith("packaging/"):
        return False
    if language == "xml" and not rel.startswith(("packaging/", "docs/")):
        return False
    if user_config:
        for excluded in user_config.get("exclude", []):
            if rel.startswith(str(excluded)):
                return False
    try:
        if path.stat().st_size > MAX_FILE_BYTES:
            return False
    except OSError:
        return False
    return True


def module_for(rel: str, language: str) -> str:
    parts = Path(rel).parts
    if not parts:
        return "root"
    if parts[0] == "src" and len(parts) > 2:
        return parts[1]
    if parts[0] == "tests":
        return "tests"
    if parts[0] == "tools" and len(parts) > 1 and parts[1] == "atlas":
        return "atlas"
    if parts[0] == "scripts":
        return "scripts"
    if parts[0] == "docs":
        return "docs"
    if parts[0] == "assets" and len(parts) > 1:
        return "ui-assets" if parts[1] == "ui" else f"assets-{parts[1]}"
    if parts[0] == "packaging":
        return "packaging"
    if rel == "CMakeLists.txt" or language == "cmake":
        return "build"
    if parts[0] in {".opencode"}:
        return "agents"
    return "root"


def area_for(rel: str) -> str:
    if rel.startswith("src/"):
        return "source"
    if rel.startswith("tests/"):
        return "test"
    if rel.startswith("docs/"):
        return "docs"
    if rel.startswith("assets/"):
        return "asset"
    if rel.startswith("tools/"):
        return "tool"
    if rel.startswith("scripts/"):
        return "script"
    if rel.startswith("packaging/"):
        return "package"
    if rel.startswith(".opencode/"):
        return "agent-config"
    return "config"


def is_external(rel: str) -> bool:
    return Path(rel).parts[0] in {"third_party"} if Path(rel).parts else False


def normalize_text(value: str) -> str:
    value = unicodedata.normalize("NFKD", value)
    value = "".join(ch for ch in value if not unicodedata.combining(ch))
    return value.lower()


def glossary(root: Path) -> dict:
    path = root / "tools/atlas/data/glossary.json"
    if not path.is_file():
        return {}
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


STOPWORDS = {
    "a", "ao", "aos", "as", "com", "como", "da", "das", "de", "do", "dos",
    "e", "em", "entre", "era", "essa", "esse", "esta", "este", "eu", "foi",
    "ha", "isso", "isto", "ja", "mais", "mas", "me", "mesmo", "meu", "na",
    "nao", "nas", "no", "nos", "o", "os", "ou", "para", "pela", "pelo",
    "por", "qual", "que", "se", "sem", "ser", "seu", "sua", "tem", "um",
    "uma", "voce", "the", "an", "and", "are", "at", "be", "by", "for",
    "from", "how", "in", "is", "it", "my", "not", "of", "on", "or", "that",
    "the", "this", "to", "was", "what", "when", "where", "why", "with",
}
