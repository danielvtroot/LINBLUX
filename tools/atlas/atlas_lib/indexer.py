"""File discovery, hashing, language extraction and incremental indexing."""

from __future__ import annotations

import hashlib
import json
import os
import re
import shlex
import time
from datetime import datetime, timezone
from pathlib import Path

from . import INDEX_SCHEMA_VERSION, __version__
from .config import (
    IGNORED_DIRS,
    INDEX_VERSION,
    MAX_FTS_CHARS,
    area_for,
    detect_language,
    glossary,
    is_external,
    load_user_config,
    module_for,
    should_index,
)
from .db import (
    ensure_cache_dir,
    get_meta,
    open_db,
    remove_index,
    set_meta,
    snapshot_files,
    table_counts,
    transaction,
    update_meta_stats,
)

CPP_KEYWORDS = {
    "if", "for", "while", "switch", "catch", "return", "else", "do", "case",
    "sizeof", "new", "delete", "throw", "typedef", "using", "namespace",
    "class", "struct", "enum", "union", "template", "operator", "static_assert",
    "const", "volatile", "static", "inline", "virtual", "explicit", "friend",
    "extern", "public", "private", "protected", "constexpr", "consteval",
}

CPP_TYPE_WORDS = {
    "void", "bool", "char", "short", "int", "long", "float", "double", "auto",
    "unsigned", "signed", "size_t", "std", "string", "vector", "optional",
    "unique_ptr", "shared_ptr", "weak_ptr", "pair", "tuple", "array", "map",
    "unordered_map", "set", "unordered_set", "function", "span", "string_view",
}

CPP_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE)
CPP_NAMESPACE_RE = re.compile(r"\bnamespace\s+([A-Za-z_]\w*(?:::\w+)*)\s*\{")
CPP_CLASS_RE = re.compile(
    r"(?m)^[ \t]*(class|struct|enum(?:\s+class)?)\s+([A-Za-z_]\w*)"
    r"(?:\s*:\s*([^;{\n]+))?\s*\{"
)
CPP_FUNCTION_RE = re.compile(
    r"(?m)^[ \t]*(?!(?:if|for|while|switch|catch|return|else|do|case|sizeof|new|delete|throw|#)\b)"
    r"((?:[\w:<>~,\*&\s]+?))\s+"
    r"(~?[A-Za-z_]\w*(?:::\w+)*)\s*"
    r"\(([^;{}]*)\)\s*(?:const\b\s*)?(?:noexcept(?:\s*\([^)]*\))?\s*)?"
    r"(?:override\b\s*)?(?:final\b\s*)?(?:->\s*[\w:<>,\*&\s]+?)?\s*([{;])"
)
CPP_CTOR_RE = re.compile(
    r"(?m)^[ \t]*(?:explicit\s+)?(~?[A-Za-z_]\w*(?:::\w+)*)\s*"
    r"\(([^;{}]*)\)\s*(?::[^{;]*)?\s*([{;])"
)
CPP_USING_RE = re.compile(r"(?m)^[ \t]*using\s+([A-Za-z_]\w*)\s*=")
CPP_TYPEDEF_RE = re.compile(r"(?m)^[ \t]*typedef\s+[^;]+?\s+([A-Za-z_]\w*)\s*;")
LUA_FUNCTION_RE = re.compile(r"(?m)^[ \t]*(?:local\s+)?function\s+([\w.:]+)\s*\(")
LUA_ASSIGN_RE = re.compile(r"(?m)^[ \t]*([A-Za-z_][\w.:]*)\s*=\s*function\s*\(")
PY_FUNCTION_RE = re.compile(r"(?m)^[ \t]*(?:async\s+)?def\s+([A-Za-z_]\w*)\s*\(")
PY_CLASS_RE = re.compile(r"(?m)^[ \t]*class\s+([A-Za-z_]\w*)")
RML_ID_RE = re.compile(r"<([a-z][\w-]*)\b[^>]*\bid=\"([^\"]+)\"", re.IGNORECASE)
LUA_REQUIRE_RE = re.compile(r"require\s*\(?\s*[\"']([^\"']+)[\"']")


def _line(text: str, position: int) -> int:
    return text.count("\n", 0, position) + 1


def _blank_cpp(text: str) -> str:
    """Blank comments and string/char literals while preserving line breaks."""
    out = list(text)
    i = 0
    n = len(text)
    state = "code"
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if ch == "/" and nxt == "/":
                state = "line_comment"
                out[i] = out[i + 1] = " "
                i += 2
                continue
            if ch == "/" and nxt == "*":
                state = "block_comment"
                out[i] = out[i + 1] = " "
                i += 2
                continue
            if ch == '"':
                state = "string"
                out[i] = " "
            elif ch == "'":
                state = "char"
                out[i] = " "
        elif state == "line_comment":
            if ch == "\n":
                state = "code"
            else:
                out[i] = " "
        elif state == "block_comment":
            if ch == "*" and nxt == "/":
                out[i] = out[i + 1] = " "
                i += 2
                state = "code"
                continue
            if ch != "\n":
                out[i] = " "
        elif state in {"string", "char"}:
            if ch == "\\":
                if i + 1 < n:
                    if text[i + 1] != "\n":
                        out[i + 1] = " "
                    i += 2
                    continue
            if (state == "string" and ch == '"') or (state == "char" and ch == "'"):
                state = "code"
            if ch != "\n":
                out[i] = " "
        i += 1
    return "".join(out)


def _matching_brace(text: str, open_pos: int) -> int:
    depth = 0
    for pos in range(open_pos, len(text)):
        if text[pos] == "{":
            depth += 1
        elif text[pos] == "}":
            depth -= 1
            if depth == 0:
                return pos
    return len(text) - 1


def _namespace_intervals(text: str) -> list[tuple[int, int, str]]:
    intervals = []
    for match in CPP_NAMESPACE_RE.finditer(text):
        open_pos = text.find("{", match.start(), match.end())
        if open_pos < 0:
            continue
        end = _matching_brace(text, open_pos)
        intervals.append((match.start(), end, match.group(1)))
    return intervals


def _class_intervals(text: str) -> list[tuple[int, int, str, str]]:
    intervals = []
    for match in CPP_CLASS_RE.finditer(text):
        open_pos = text.find("{", match.start(), match.end())
        if open_pos < 0:
            continue
        end = _matching_brace(text, open_pos)
        kind = match.group(1).replace(" ", "-")
        intervals.append((match.start(), end, match.group(2), kind))
    return intervals


def _namespace_path(intervals: list[tuple[int, int, str]], pos: int) -> str:
    """Full namespace of a position, including nested and ``a::b`` namespaces.

    ``_enclosing`` returns only the innermost interval, which loses ``loblox``
    in ``namespace loblox { namespace editor { ... } }``.
    """
    merged: list[str] = []
    for item in sorted(intervals):
        if not (item[0] <= pos <= item[1]):
            continue
        segments = [segment for segment in item[2].split("::") if segment]
        if merged and segments and merged[-1] == segments[0]:
            merged.extend(segments[1:])
        else:
            merged.extend(segments)
    return "::".join(merged)


def _enclosing(intervals, pos: int, name_index: int = 2) -> str:
    best = None
    for item in intervals:
        start, end = item[0], item[1]
        if start <= pos <= end and (best is None or start > best[0]):
            best = item
    if best is None:
        return ""
    return best[name_index]


def _is_cpp_function(return_type: str, name: str) -> bool:
    if name in CPP_KEYWORDS:
        return False
    words = re.findall(r"[A-Za-z_]\w*", return_type)
    if not words:
        return False
    if words[0] in {"if", "for", "while", "switch", "catch", "return", "else", "do"}:
        return False
    if len(words) == 1 and words[0] not in CPP_TYPE_WORDS:
        # Unknown one-word return types are still common (Foo makeFoo()). Accept
        # them only when the source line also has a brace or a known qualifier.
        return True
    return True


def _extract_cpp_functions(text: str, clean: str, classes: list, add, allow_declarations: bool = True) -> None:
    """Fast line-oriented extraction.

    A full C++ parser is not available in this environment. This scanner is
    deliberately conservative: it recognises signatures that start on one line
    and may continue for a few lines. Results are labelled regex-cpp/medium and
    must not be treated as a semantic AST.
    """
    clean_lines = clean.splitlines()
    line_offsets = [0]
    for line in text.splitlines(keepends=True):
        line_offsets.append(line_offsets[-1] + len(line))
    class_names = {item[2] for item in classes}
    name_re = re.compile(r"(~?[A-Za-z_]\w*(?:::\w+)*)\s*$")
    control = {"if", "for", "while", "switch", "catch", "return", "else", "do", "case", "default", "break", "continue", "goto", "sizeof", "new", "delete", "throw"}
    allowed_prefix = {
        "static", "inline", "virtual", "explicit", "friend", "extern", "constexpr",
        "consteval", "const", "volatile", "unsigned", "signed", "struct", "class", "enum",
    }
    i = 0
    while i < len(clean_lines):
        stripped = clean_lines[i].strip()
        if not stripped or stripped.startswith(("#", "//", "/*", "*", "using ", "typedef ", "namespace ", "template")):
            i += 1
            continue
        if "(" not in stripped:
            i += 1
            continue
        candidate = stripped
        end_line_index = i
        guard = 0
        while (
            end_line_index + 1 < len(clean_lines)
            and candidate.count("(") > candidate.count(")")
            and guard < 8
        ):
            end_line_index += 1
            candidate += " " + clean_lines[end_line_index].strip()
            guard += 1
        paren = candidate.find("(")
        if paren <= 0:
            i = end_line_index + 1
            continue
        pre = candidate[:paren].strip()
        match = name_re.search(pre)
        if not match:
            i = end_line_index + 1
            continue
        name = match.group(1)
        prefix = pre[:match.start()].strip()
        short = name.split("::")[-1].lstrip("~")
        prefix_words = re.findall(r"[A-Za-z_]\w*", prefix)
        if name in CPP_KEYWORDS or short in CPP_KEYWORDS:
            i = end_line_index + 1
            continue
        if any(ch in prefix for ch in (";", "{", "}")):
            i = end_line_index + 1
            continue
        if prefix_words and prefix_words[0] in control:
            i = end_line_index + 1
            continue
        if not prefix_words:
            if short not in class_names:
                i = end_line_index + 1
                continue
            kind = "constructor"
        else:
            if "=" in prefix:
                i = end_line_index + 1
                continue
            if prefix_words[0] not in allowed_prefix and not (
                prefix_words[0] in CPP_TYPE_WORDS
                or prefix_words[0][:1].isupper()
                or ":" in prefix
                or "*" in prefix
                or "&" in prefix
                or "<" in prefix
            ):
                i = end_line_index + 1
                continue
            kind = "function"
        close = candidate.find(")", paren + 1)
        if close < 0:
            i = end_line_index + 1
            continue
        rest = candidate[close + 1:]
        first_brace = rest.find("{")
        first_semicolon = rest.find(";")
        first_colon = rest.find(":")
        if first_brace >= 0 and (first_semicolon < 0 or first_brace < first_semicolon):
            term = "{"
        elif first_semicolon >= 0:
            term = ";"
        elif first_colon >= 0:
            term = "{"
        else:
            term = ""
        if not term:
            i = end_line_index + 1
            continue
        definition = term == "{"
        if not definition and not allow_declarations:
            i = end_line_index + 1
            continue
        start_pos = line_offsets[min(i, len(line_offsets) - 1)]
        if definition:
            end_offset = line_offsets[min(end_line_index + 1, len(line_offsets) - 1)]
            open_pos = clean.find("{", start_pos, end_offset + 1)
            if open_pos < 0:
                open_pos = clean.find("{", start_pos)
        else:
            open_pos = -1
        end_pos = _matching_brace(clean, open_pos) if open_pos >= 0 else start_pos
        add(name, kind, start_pos, end_pos, candidate[:500], definition)
        i = end_line_index + 1


def parse_cpp(text: str, allow_declarations: bool = True) -> dict:
    clean = _blank_cpp(text)
    namespaces = _namespace_intervals(clean)
    classes = _class_intervals(clean)
    symbols = []
    seen = set()

    def add(name: str, kind: str, pos: int, end: int, signature: str, definition: bool):
        key = (name, kind, pos)
        if key in seen:
            return
        seen.add(key)
        namespace = _namespace_path(namespaces, pos)
        owner = _enclosing(classes, pos, 2)
        if owner == name:
            owner = ""
        if owner and owner != name and kind == "function":
            kind = "method"
        parts = [part for part in (namespace, owner) if part]
        if name.startswith("::"):
            qualified = name.lstrip(":")
        elif "::" in name:
            # Out-of-class definition (``Class::method`` or
            # ``loblox::Class::method``): the owner is already inside the name,
            # so only prepend the namespace when it is not already there.
            prefix = "::".join(parts)
            if prefix and name.startswith(prefix + "::"):
                qualified = name
            else:
                qualified = "::".join(parts + [name]) if parts else name
            if kind == "function":
                kind = "method"
        else:
            qualified = "::".join(parts + [name]) if parts else name
        symbols.append({
            "name": name.split("::")[-1],
            "qualified": qualified,
            "kind": kind,
            "line": _line(text, pos),
            "end_line": _line(text, end) if definition else None,
            "signature": re.sub(r"\s+", " ", signature).strip()[:240],
            "origin": "regex-cpp",
            "confidence": "medium",
            "is_definition": 1 if definition else 0,
        })

    for match in CPP_CLASS_RE.finditer(clean):
        open_pos = clean.find("{", match.start(), match.end())
        end = _matching_brace(clean, open_pos)
        body = clean[open_pos:end + 1]
        kind = match.group(1).replace(" ", "-")
        if kind in {"class", "struct"} and re.search(r"\bvirtual\b[^;{}]*=\s*0\s*;", body):
            kind = "interface"
        add(match.group(2), kind, match.start(), end, text[match.start():match.start() + 200], True)

    _extract_cpp_functions(text, clean, classes, add, allow_declarations)

    for regex, kind in ((CPP_USING_RE, "using"), (CPP_TYPEDEF_RE, "typedef")):
        for match in regex.finditer(clean):
            add(match.group(1), kind, match.start(), match.start(), text[match.start():match.start() + 180], True)

    relations = []
    for match in CPP_INCLUDE_RE.finditer(text):
        relations.append({
            "target": match.group(1),
            "kind": "include",
            "confidence": "textual",
            "evidence": f"linha {_line(text, match.start())}",
        })
    return {"symbols": symbols, "relations": relations}


def parse_lua(text: str) -> dict:
    symbols = []
    for regex in (LUA_FUNCTION_RE, LUA_ASSIGN_RE):
        for match in regex.finditer(text):
            name = match.group(1)
            symbols.append({
                "name": name.split(".")[-1].split(":")[-1],
                "qualified": name,
                "kind": "function",
                "line": _line(text, match.start()),
                "end_line": None,
                "signature": text[match.start():text.find("\n", match.start())][:220],
                "origin": "regex-lua",
                "confidence": "medium",
                "is_definition": 1,
            })
    relations = [{
        "target": match.group(1), "kind": "lua-require",
        "confidence": "textual", "evidence": f"linha {_line(text, match.start())}",
    } for match in LUA_REQUIRE_RE.finditer(text)]
    return {"symbols": symbols, "relations": relations}


def parse_python(text: str) -> dict:
    symbols = []
    for regex, kind in ((PY_CLASS_RE, "class"), (PY_FUNCTION_RE, "function")):
        for match in regex.finditer(text):
            symbols.append({
                "name": match.group(1),
                "qualified": match.group(1),
                "kind": kind,
                "line": _line(text, match.start()),
                "end_line": None,
                "signature": text[match.start():text.find("\n", match.start())][:220],
                "origin": "regex-python",
                "confidence": "medium",
                "is_definition": 1,
            })
    return {"symbols": symbols, "relations": []}


def parse_rml(text: str) -> dict:
    symbols = []
    for match in RML_ID_RE.finditer(text):
        tag, name = match.group(1), match.group(2)
        symbols.append({
            "name": name,
            "qualified": f"ui:{name}",
            "kind": "ui-element",
            "line": _line(text, match.start()),
            "end_line": None,
            "signature": f"<{tag} id=\"{name}\">",
            "origin": "regex-rml",
            "confidence": "high",
            "is_definition": 1,
        })
    return {"symbols": symbols, "relations": []}


def parse_markdown(text: str) -> dict:
    symbols = []
    for match in re.finditer(r"(?m)^(#{1,2})\s+(.+?)\s*$", text):
        symbols.append({
            "name": match.group(2).strip()[:120],
            "qualified": match.group(2).strip()[:120],
            "kind": "heading",
            "line": _line(text, match.start()),
            "end_line": None,
            "signature": match.group(0)[:180],
            "origin": "regex-markdown",
            "confidence": "high",
            "is_definition": 1,
        })
    return {"symbols": symbols, "relations": []}


def parse_text(rel: str, language: str, text: str) -> dict:
    if language == "cpp":
        allow_declarations = Path(rel).suffix.lower() in {".h", ".hh", ".hpp", ".hxx"}
        return parse_cpp(text, allow_declarations=allow_declarations)
    if language == "lua":
        return parse_lua(text)
    if language == "python":
        return parse_python(text)
    if language == "rml":
        return parse_rml(text)
    if language == "markdown":
        return parse_markdown(text)
    return {"symbols": [], "relations": []}


def read_text_file(path: Path) -> tuple[str, bytes] | None:
    try:
        raw = path.read_bytes()
    except OSError:
        return None
    if b"\x00" in raw[:4096]:
        return None
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        text = raw.decode("utf-8", "replace")
    return text, raw


def collect_files(root: Path) -> tuple[dict[str, Path], int]:
    config = load_user_config(root)
    root_resolved = root.resolve()
    files: dict[str, Path] = {}
    skipped = 0
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(
            name for name in dirnames
            if name not in IGNORED_DIRS and not name.startswith("build-")
        )
        base = Path(dirpath)
        for filename in sorted(filenames):
            path = base / filename
            try:
                if path.is_symlink():
                    path.resolve().relative_to(root_resolved)
            except (OSError, ValueError):
                skipped += 1
                continue
            try:
                rel = path.relative_to(root).as_posix()
            except ValueError:
                skipped += 1
                continue
            if should_index(rel, path, config):
                files[rel] = path
            else:
                skipped += 1
    return files, skipped


def _sha256(raw: bytes) -> str:
    return hashlib.sha256(raw).hexdigest()


def _insert_symbols(conn, file_id: int, symbols: list[dict]) -> None:
    for symbol in symbols:
        conn.execute(
            "INSERT INTO symbols(file_id,name,qualified,kind,line,end_line,signature,origin,confidence,is_definition)"
            " VALUES(?,?,?,?,?,?,?,?,?,?)",
            (
                file_id, symbol["name"], symbol["qualified"], symbol["kind"],
                symbol["line"], symbol.get("end_line"), symbol.get("signature"),
                symbol["origin"], symbol["confidence"], symbol["is_definition"],
            ),
        )


def _resolve_include(source: str, target: str, all_paths: set[str]) -> tuple[str, str]:
    target = target.replace("\\", "/")
    if target in all_paths:
        return target, "confirmed"
    source_dir = Path(source).parent.as_posix()
    candidate = (Path(source_dir) / target).as_posix()
    candidate = os.path.normpath(candidate).replace("\\", "/")
    if candidate in all_paths:
        return candidate, "confirmed"
    matches = [path for path in all_paths if path.endswith("/" + target) or path == target]
    if len(matches) == 1:
        return matches[0], "confirmed"
    # Common project include roots: src/ and tests/.
    for prefix in ("src/", "tests/", "third_party/", ""):
        candidate = os.path.normpath(prefix + target).replace("\\", "/")
        if candidate in all_paths:
            return candidate, "confirmed"
    return target, "inferred"


def _cmake_args(text: str) -> list[str]:
    try:
        return shlex.split(text, posix=True)
    except ValueError:
        return re.findall(r"[^\s]+", text)


def _normalize_cmake_path(value: str, cmake_dir: str) -> str | None:
    value = value.strip().strip('"')
    if not value or value.startswith("$<"):
        return None
    if any(token in value for token in ("${", "$ENV", "$<")):
        for prefix in ("${CMAKE_CURRENT_SOURCE_DIR}/", "${PROJECT_SOURCE_DIR}/", "${CMAKE_SOURCE_DIR}/"):
            if value.startswith(prefix):
                value = value[len(prefix):]
                break
        else:
            return None
    path = Path(value)
    if path.is_absolute():
        try:
            return path.resolve().relative_to(Path(cmake_dir).resolve().parent if Path(cmake_dir).name == "build" else Path.cwd()).as_posix()
        except (ValueError, OSError):
            pass
    joined = os.path.normpath(os.path.join(cmake_dir, value)).replace("\\", "/")
    return joined


def parse_cmake(text: str, cmake_dir: str) -> dict:
    clean = re.sub(r"(?m)#.*$", "", text)
    commands = []
    for match in re.finditer(r"(?m)^[ \t]*([A-Za-z_]\w*)[ \t]*\(", clean):
        name = match.group(1).lower()
        open_pos = clean.find("(", match.start(), match.end())
        depth = 0
        end = len(clean)
        quote = False
        escaped = False
        for pos in range(open_pos, len(clean)):
            ch = clean[pos]
            if escaped:
                escaped = False
                continue
            if ch == "\\":
                escaped = True
                continue
            if ch == '"':
                quote = not quote
            elif not quote:
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                    if depth == 0:
                        end = pos
                        break
        args = _cmake_args(clean[open_pos + 1:end])
        commands.append((name, args))

    targets: dict[str, dict] = {}
    tests: list[dict] = []
    external: list[dict] = []
    conditional = 0
    for name, args in commands:
        if name == "if":
            conditional += 1
            continue
        if name == "endif":
            conditional = max(0, conditional - 1)
            continue
        if name in {"add_library", "add_executable"} and args:
            target = args[0]
            kind = name.removeprefix("add_").lower()
            rest = args[1:]
            if "ALIAS" in rest:
                # add_library(alias ALIAS real): tests link the alias name while
                # the sources belong to the real target. Without this edge the
                # target graph stops and a whole module reports zero consumers
                # (audit B6: linblux_editor ALIAS linblux_studio).
                alias_at = rest.index("ALIAS")
                real = rest[alias_at + 1] if alias_at + 1 < len(rest) else ""
                entry = targets.setdefault(
                    target, {"name": target, "type": "alias", "sources": [], "deps": []}
                )
                entry["type"] = "alias"
                if real and real not in entry["deps"]:
                    entry["deps"].append(real)
                continue
            declared_type = kind
            if kind == "library" and rest and rest[0] in {
                "STATIC", "SHARED", "MODULE", "INTERFACE", "OBJECT"
            }:
                declared_type = rest[0].lower()
            if rest and rest[0] in {"STATIC", "SHARED", "MODULE", "INTERFACE", "OBJECT"}:
                rest = rest[1:]
            sources = []
            for value in rest:
                if value in {"ALIAS", "IMPORTED", "GLOBAL"}:
                    continue
                normalized = _normalize_cmake_path(value, cmake_dir)
                if normalized and Path(normalized).suffix:
                    sources.append(normalized)
            targets.setdefault(target, {"name": target, "type": declared_type, "sources": [], "deps": []})
            entry = targets[target]
            if entry["type"] == "unknown":
                entry["type"] = declared_type
            for source in sources:
                if source not in entry["sources"]:
                    entry["sources"].append(source)
        elif name == "target_sources" and args:
            target = args[0]
            entry = targets.setdefault(target, {"name": target, "type": "unknown", "sources": [], "deps": []})
            for value in args[1:]:
                if value in {"PUBLIC", "PRIVATE", "INTERFACE", "FILE_SET", "TYPE", "BASE_DIRS", "FILES"}:
                    continue
                normalized = _normalize_cmake_path(value, cmake_dir)
                if normalized and Path(normalized).suffix and normalized not in entry["sources"]:
                    entry["sources"].append(normalized)
        elif name in {"target_link_libraries", "add_dependencies"} and args:
            target = args[0]
            rest = [arg for arg in args[1:] if arg not in {"PUBLIC", "PRIVATE", "INTERFACE"} and not arg.startswith("$<")]
            targets.setdefault(target, {"name": target, "type": "unknown", "sources": [], "deps": []})
            targets[target]["deps"].extend(rest)
        elif name == "add_test":
            if "NAME" in args and "COMMAND" in args:
                i = args.index("NAME")
                j = args.index("COMMAND")
                test_name = args[i + 1] if i + 1 < len(args) else "?"
                command = args[j + 1:] if j + 1 < len(args) else []
                tests.append({
                    "name": test_name,
                    "command": " ".join(command),
                    "tokens": command,
                    "conditional": conditional > 0,
                })
        elif name == "set_tests_properties":
            if "PROPERTIES" in args:
                i = args.index("PROPERTIES")
                names = args[:i]
                props = args[i + 1:]
                props_map = dict(zip(props[0::2], props[1::2]))
                for test in tests:
                    if test["name"] in names:
                        if "TIMEOUT" in props_map:
                            try:
                                test["timeout"] = float(props_map["TIMEOUT"])
                            except ValueError:
                                pass
                        if "LABELS" in props_map:
                            test["labels"] = props_map["LABELS"]
        elif name in {"pkg_check_modules", "find_package"} and args:
            external.append({"name": args[0], "kind": "cmake-" + name, "evidence": f"{name}({args[0]})"})
        elif name == "fetchcontent_declare" and args:
            external.append({"name": args[0], "kind": "fetchcontent", "evidence": f"FetchContent_Declare({args[0]})"})

    # Resolve test sources after all targets are known.
    for test in tests:
        source = None
        target = None
        for token in test["tokens"]:
            match = re.search(r"\$<TARGET_FILE:([^>]+)>", token)
            if match:
                target = match.group(1)
                break
            if token in targets:
                target = token
                break
        if target is None:
            first = test["tokens"][0] if test["tokens"] else ""
            if first in targets:
                target = first
        if target and target in targets:
            sources = [path for path in targets[target]["sources"] if path in {"src", "tests"} or path.endswith((".cpp", ".c", ".cc"))]
            if sources:
                source = sources[-1]
        if source is None:
            for token in test["tokens"]:
                if token.endswith((".py", ".sh")) and "/" in token:
                    source = token.lstrip("./")
                    break
        test["target"] = target
        test["source_path"] = source
    return {"targets": targets, "tests": tests, "external": external}


def _rebuild_graph(conn, root: Path, files: dict[str, Path]) -> tuple[int, int]:
    """Rebuild targets/tests/deps from CMakeLists.txt files on disk."""
    conn.execute("DELETE FROM cmake_targets")
    conn.execute("DELETE FROM tests")
    conn.execute("DELETE FROM external_deps")
    target_count = 0
    test_count = 0
    external_rows: set[tuple[str, str]] = set()
    for rel, path in files.items():
        if path.name != "CMakeLists.txt":
            continue
        try:
            cmake_text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        row = conn.execute("SELECT id FROM files WHERE path=?", (rel,)).fetchone()
        file_id = row["id"] if row else None
        graph = parse_cmake(cmake_text, str(path.parent.relative_to(root)))
        for target_name, target in graph["targets"].items():
            cur = conn.execute(
                "INSERT INTO cmake_targets(name,type,file_id,evidence) VALUES(?,?,?,?)",
                (target_name, target["type"], file_id, f"CMakeLists.txt:{rel}"),
            )
            target_id = cur.lastrowid
            target_count += 1
            for source in target["sources"]:
                conn.execute(
                    "INSERT INTO target_sources(target_id,path,kind,confidence) VALUES(?,?,?,?)",
                    (target_id, source, "source", "confirmed"),
                )
            for dep in target["deps"]:
                conn.execute(
                    "INSERT INTO target_sources(target_id,path,kind,confidence) VALUES(?,?,?,?)",
                    (target_id, dep, "dependency", "confirmed"),
                )
        for test in graph["tests"]:
            cur = conn.execute(
                "INSERT INTO tests(name,command,target,source_path,timeout,labels,origin) VALUES(?,?,?,?,?,?,?)",
                (
                    test["name"], test["command"], test.get("target"),
                    test.get("source_path"), test.get("timeout"), test.get("labels"),
                    "cmake" + (":conditional" if test.get("conditional") else ""),
                ),
            )
            test_id = cur.lastrowid
            test_count += 1
            source = test.get("source_path")
            if source:
                conn.execute(
                    "INSERT INTO test_files(test_id,path,evidence) VALUES(?,?,?)",
                    (test_id, source, "cmake test source"),
                )
            if test.get("target") in graph["targets"]:
                for source in graph["targets"][test["target"]]["sources"]:
                    conn.execute(
                        "INSERT INTO test_files(test_id,path,evidence) VALUES(?,?,?)",
                        (test_id, source, "target source"),
                    )
        for item in graph["external"]:
            external_rows.add((item["name"], item["kind"]))

    third_party = root / "third_party"
    if third_party.is_dir():
        for child in sorted(third_party.iterdir()):
            if child.is_dir():
                external_rows.add((child.name, "vendored"))
    for name, kind in sorted(external_rows):
        conn.execute(
            "INSERT OR IGNORE INTO external_deps(name,kind,evidence) VALUES(?,?,?)",
            (name, kind, "CMake/third_party inventory"),
        )
    return target_count, test_count


def index_repository(
    root: Path, force: bool = False, quiet: bool = False, rehash: bool = False
) -> dict:
    started = time.monotonic()
    cache = ensure_cache_dir(root)
    if force:
        remove_index(root)
    try:
        conn = open_db(root, create=True)
        if not get_meta(conn, "schema_version") or get_meta(conn, "schema_version") != str(INDEX_SCHEMA_VERSION):
            conn.close()
            remove_index(root)
            conn = open_db(root, create=True)
    except Exception:
        remove_index(root)
        conn = open_db(root, create=True)

    files, skipped = collect_files(root)
    existing = snapshot_files(conn)
    all_paths = set(files)
    now = datetime.now(timezone.utc).isoformat(timespec="seconds")
    stats = {
        "added": 0, "updated": 0, "deleted": 0, "unchanged": 0,
        "skipped": skipped, "symbols": 0, "relations": 0,
        "bytes_read": 0, "indexed_files": len(files), "rehash": bool(rehash),
    }
    parsed: dict[str, dict] = {}
    hashes: dict[str, str] = {}
    mtimes: dict[str, int] = {}
    ctimes: dict[str, int] = {}
    sizes: dict[str, int] = {}
    touched: dict[str, tuple[int, int]] = {}

    # Fast path: unchanged metadata (size + mtime + ctime) skips the file read
    # and the SHA-256 entirely. ``rehash=True`` (update --full) forces hashing.
    for rel, path in files.items():
        prior = existing.get(rel)
        try:
            stat = path.stat()
        except OSError:
            continue
        mtimes[rel] = stat.st_mtime_ns
        ctimes[rel] = stat.st_ctime_ns
        sizes[rel] = stat.st_size
        if (
            prior
            and not rehash
            and prior["size"] == stat.st_size
            and prior["mtime_ns"] == stat.st_mtime_ns
            and int(prior.get("ctime_ns") or 0) == stat.st_ctime_ns
        ):
            stats["unchanged"] += 1
            continue
        read = read_text_file(path)
        if read is None:
            continue
        text, raw = read
        digest = _sha256(raw)
        hashes[rel] = digest
        stats["bytes_read"] += len(raw)
        if prior and prior["sha256"] == digest:
            stats["unchanged"] += 1
            if (
                prior["mtime_ns"] != stat.st_mtime_ns
                or int(prior.get("ctime_ns") or 0) != stat.st_ctime_ns
            ):
                touched[rel] = (stat.st_mtime_ns, stat.st_ctime_ns)
            continue
        language = detect_language(path) or "text"
        parsed[rel] = parse_text(rel, language, text)
        parsed[rel]["text"] = text
        if prior:
            stats["updated"] += 1
        else:
            stats["added"] += 1

    deleted = [rel for rel in existing if rel not in files]
    stats["deleted"] = len(deleted)

    cmake_files = [(rel, path) for rel, path in files.items() if path.name == "CMakeLists.txt"]
    cmake_changed = any(rel in parsed for rel, _ in cmake_files) or any(
        Path(rel).name == "CMakeLists.txt" for rel in deleted
    )
    graph_rows = conn.execute("SELECT COUNT(*) AS n FROM cmake_targets").fetchone()["n"]
    test_rows = conn.execute("SELECT COUNT(*) AS n FROM tests").fetchone()["n"]
    rebuild_graph = bool(
        force or cmake_changed or graph_rows == 0 or test_rows == 0 or not existing
    )

    if not (parsed or deleted or touched or rebuild_graph):
        counts = table_counts(conn)
        elapsed = time.monotonic() - started
        db_files = [cache / "index.sqlite", cache / "index.sqlite-wal", cache / "index.sqlite-shm"]
        stats.update({
            "noop": True,
            "elapsed_seconds": round(elapsed, 3),
            "symbols_changed": 0,
            "relations_changed": 0,
            "symbols": counts["symbols"],
            "relations": counts["relations"],
            "counts": counts,
            "database_bytes": sum(path.stat().st_size for path in db_files if path.exists()),
        })
        conn.close()
        if not quiet:
            print(
                f"ATLAS: índice atualizado ({stats['indexed_files']} arquivos, "
                f"nada a fazer) em {elapsed:.3f}s"
            )
        return stats

    with transaction(conn):
        for rel in deleted:
            conn.execute("DELETE FROM files WHERE path=?", (rel,))
        # Clean FTS rows whose file row no longer exists (rows deleted above).
        conn.execute("DELETE FROM file_fts WHERE rowid NOT IN (SELECT id FROM files)")

        # Same content, different metadata: refresh the fast-path columns so
        # status/doctor do not report a stale file that needs no reparse.
        conn.executemany(
            "UPDATE files SET mtime_ns=?, ctime_ns=? WHERE path=?",
            [(mtime_ns, ctime_ns, rel) for rel, (mtime_ns, ctime_ns) in touched.items()],
        )

        for rel, payload in parsed.items():
            path = files[rel]
            language = detect_language(path) or "text"
            text = payload["text"]
            if rel in existing:
                file_id = existing[rel]["id"]
                conn.execute(
                    "UPDATE files SET module=?,area=?,language=?,size=?,mtime_ns=?,ctime_ns=?,sha256=?,line_count=?,indexed_at=?,is_external=? WHERE id=?",
                    (
                        module_for(rel, language), area_for(rel), language, sizes[rel],
                        mtimes[rel], ctimes[rel], hashes[rel], text.count("\n") + 1, now,
                        1 if is_external(rel) else 0, file_id,
                    ),
                )
                conn.execute("DELETE FROM symbols WHERE file_id=?", (file_id,))
                conn.execute("DELETE FROM relations WHERE source_file=?", (file_id,))
            else:
                cur = conn.execute(
                    "INSERT INTO files(path,module,area,language,size,mtime_ns,ctime_ns,sha256,line_count,indexed_at,is_external)"
                    " VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                    (
                        rel, module_for(rel, language), area_for(rel), language, sizes[rel],
                        mtimes[rel], ctimes[rel], hashes[rel], text.count("\n") + 1, now,
                        1 if is_external(rel) else 0,
                    ),
                )
                file_id = cur.lastrowid
            symbols = payload["symbols"]
            _insert_symbols(conn, file_id, symbols)
            stats["symbols"] += len(symbols)
            for relation in payload["relations"]:
                target, confidence = _resolve_include(rel, relation["target"], all_paths)
                conn.execute(
                    "INSERT INTO relations(source_file,target,kind,confidence,evidence) VALUES(?,?,?,?,?)",
                    (file_id, target, relation["kind"], confidence, relation.get("evidence")),
                )
                stats["relations"] += 1
            context = f"{rel}\n{text[:MAX_FTS_CHARS]}"
            conn.execute("DELETE FROM file_fts WHERE rowid=?", (file_id,))
            conn.execute(
                "INSERT INTO file_fts(rowid,path,module,content) VALUES(?,?,?,?)",
                (file_id, rel, module_for(rel, language), context),
            )

        # CMake/tests graph: rebuilt only when CMakeLists.txt changed, on first
        # index, or when the tables are empty. A pure source edit must not pay
        # for re-parsing and re-inserting the whole build graph (audit B19).
        if rebuild_graph:
            target_count, test_count = _rebuild_graph(conn, root, files)
        else:
            target_count = conn.execute(
                "SELECT COUNT(*) AS n FROM cmake_targets"
            ).fetchone()["n"]
            test_count = conn.execute("SELECT COUNT(*) AS n FROM tests").fetchone()["n"]

        # Newly added files can make an earlier unresolved include resolvable.
        for relation in conn.execute(
            "SELECT r.id,r.target,f.path AS source_path FROM relations r "
            "JOIN files f ON f.id=r.source_file "
            "WHERE r.confidence!='confirmed' AND r.kind IN ('include','lua-require')"
        ).fetchall():
            target, confidence = _resolve_include(relation["source_path"], relation["target"], all_paths)
            if confidence == "confirmed":
                conn.execute(
                    "UPDATE relations SET target=?,confidence=? WHERE id=?",
                    (target, confidence, relation["id"]),
                )

        counts = table_counts(conn)
        update_meta_stats(conn, counts)
        set_meta(conn, "atlas_version", __version__)
        set_meta(conn, "index_version", str(INDEX_VERSION))
        set_meta(conn, "updated_at", now)
        set_meta(conn, "file_count", str(len(files)))
        set_meta(conn, "glossary_terms", str(len(glossary(root).get("terms", {}))))
        set_meta(conn, "target_count", str(target_count))
        set_meta(conn, "test_count", str(test_count))

    conn.execute("PRAGMA optimize")
    try:
        conn.execute("PRAGMA wal_checkpoint(TRUNCATE)")
    except Exception:
        pass
    conn.commit()
    counts = table_counts(conn)
    elapsed = time.monotonic() - started
    db_files = [cache / "index.sqlite", cache / "index.sqlite-wal", cache / "index.sqlite-shm"]
    stats.update({
        "elapsed_seconds": round(elapsed, 3),
        "symbols_changed": stats["symbols"],
        "relations_changed": stats["relations"],
        "symbols": counts["symbols"],
        "relations": counts["relations"],
        "counts": counts,
        "database_bytes": sum(path.stat().st_size for path in db_files if path.exists()),
    })
    conn.close()
    if not quiet:
        print(
            f"ATLAS: {stats['indexed_files']} arquivos, {stats['added']} novos, "
            f"{stats['updated']} alterados, {stats['deleted']} removidos, "
            f"{stats['symbols']} símbolos em {elapsed:.2f}s"
        )
    return stats
