"""SQLite storage for the ATLAS index."""

from __future__ import annotations

import json
import sqlite3
from contextlib import contextmanager
from pathlib import Path

from . import INDEX_SCHEMA_VERSION
from .config import CACHE_DIR, db_path

SCHEMA = """
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS meta (
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS files (
  id INTEGER PRIMARY KEY,
  path TEXT NOT NULL UNIQUE,
  module TEXT NOT NULL,
  area TEXT NOT NULL,
  language TEXT NOT NULL,
  size INTEGER NOT NULL,
  mtime_ns INTEGER NOT NULL,
  ctime_ns INTEGER NOT NULL DEFAULT 0,
  sha256 TEXT NOT NULL,
  line_count INTEGER NOT NULL,
  indexed_at TEXT NOT NULL,
  is_external INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS files_module ON files(module);
CREATE INDEX IF NOT EXISTS files_path ON files(path);

CREATE TABLE IF NOT EXISTS symbols (
  id INTEGER PRIMARY KEY,
  file_id INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  name TEXT NOT NULL,
  qualified TEXT NOT NULL,
  kind TEXT NOT NULL,
  line INTEGER NOT NULL,
  end_line INTEGER,
  signature TEXT,
  origin TEXT NOT NULL,
  confidence TEXT NOT NULL,
  is_definition INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS symbols_name ON symbols(name COLLATE NOCASE);
CREATE INDEX IF NOT EXISTS symbols_qualified ON symbols(qualified COLLATE NOCASE);
CREATE INDEX IF NOT EXISTS symbols_file ON symbols(file_id);

CREATE TABLE IF NOT EXISTS relations (
  id INTEGER PRIMARY KEY,
  source_file INTEGER NOT NULL REFERENCES files(id) ON DELETE CASCADE,
  target TEXT NOT NULL,
  kind TEXT NOT NULL,
  confidence TEXT NOT NULL,
  evidence TEXT
);
CREATE INDEX IF NOT EXISTS relations_source ON relations(source_file);
CREATE INDEX IF NOT EXISTS relations_target ON relations(target);

CREATE TABLE IF NOT EXISTS cmake_targets (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  type TEXT NOT NULL,
  file_id INTEGER REFERENCES files(id) ON DELETE SET NULL,
  evidence TEXT
);

CREATE TABLE IF NOT EXISTS target_sources (
  id INTEGER PRIMARY KEY,
  target_id INTEGER NOT NULL REFERENCES cmake_targets(id) ON DELETE CASCADE,
  path TEXT NOT NULL,
  kind TEXT NOT NULL,
  confidence TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS target_sources_path ON target_sources(path);

CREATE TABLE IF NOT EXISTS tests (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  command TEXT NOT NULL,
  target TEXT,
  source_path TEXT,
  timeout REAL,
  labels TEXT,
  origin TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS test_files (
  id INTEGER PRIMARY KEY,
  test_id INTEGER NOT NULL REFERENCES tests(id) ON DELETE CASCADE,
  path TEXT NOT NULL,
  evidence TEXT
);
CREATE INDEX IF NOT EXISTS test_files_path ON test_files(path);

CREATE TABLE IF NOT EXISTS external_deps (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  kind TEXT NOT NULL,
  evidence TEXT,
  UNIQUE(name, kind)
);

CREATE VIRTUAL TABLE IF NOT EXISTS file_fts USING fts5(
  path,
  module,
  content,
  tokenize = 'unicode61 remove_diacritics 2'
);
"""


def new_connection(path: Path) -> sqlite3.Connection:
    path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(path), timeout=30)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("PRAGMA synchronous=NORMAL")
    conn.execute("PRAGMA foreign_keys=ON")
    conn.execute("PRAGMA busy_timeout=30000")
    return conn


def open_db(root: Path, create: bool = False) -> sqlite3.Connection:
    path = db_path(root)
    if not create and not path.is_file():
        raise FileNotFoundError(
            f"indice ausente em {path}; execute: python3 tools/atlas/atlas.py init"
        )
    conn = new_connection(path)
    if create:
        conn.executescript(SCHEMA)
        # Never overwrite the version of an existing database: index_repository
        # needs the stored value to detect an incompatible schema and rebuild.
        if get_meta(conn, "schema_version") is None:
            set_meta(conn, "schema_version", str(INDEX_SCHEMA_VERSION))
        conn.commit()
    return conn


def set_meta(conn: sqlite3.Connection, key: str, value: str) -> None:
    conn.execute(
        "INSERT INTO meta(key,value) VALUES(?,?) "
        "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
        (key, value),
    )


def get_meta(conn: sqlite3.Connection, key: str, default: str | None = None) -> str | None:
    row = conn.execute("SELECT value FROM meta WHERE key=?", (key,)).fetchone()
    return row["value"] if row else default


def meta_dict(conn: sqlite3.Connection) -> dict[str, str]:
    return {row["key"]: row["value"] for row in conn.execute("SELECT key,value FROM meta")}


def schema_version(conn: sqlite3.Connection) -> int:
    raw = get_meta(conn, "schema_version", "0")
    try:
        return int(raw or 0)
    except ValueError:
        return 0


def index_is_compatible(conn: sqlite3.Connection) -> bool:
    return schema_version(conn) == INDEX_SCHEMA_VERSION


def table_counts(conn: sqlite3.Connection) -> dict[str, int]:
    names = ("files", "symbols", "relations", "cmake_targets", "tests", "external_deps")
    return {
        name: conn.execute(f"SELECT COUNT(*) AS n FROM {name}").fetchone()["n"]
        for name in names
    }


def snapshot_files(conn: sqlite3.Connection) -> dict[str, dict]:
    return {
        row["path"]: dict(row)
        for row in conn.execute(
            "SELECT id,path,size,mtime_ns,ctime_ns,sha256,module,language FROM files"
        )
    }


def update_meta_stats(conn: sqlite3.Connection, counts: dict[str, int]) -> None:
    set_meta(conn, "counts", json.dumps(counts, sort_keys=True))


def index_status(conn: sqlite3.Connection) -> dict:
    meta = meta_dict(conn)
    counts = table_counts(conn)
    db_file = Path(conn.execute("PRAGMA database_list").fetchone()["file"])
    size = db_file.stat().st_size if db_file and db_file.exists() else 0
    return {
        "database": str(db_file),
        "database_bytes": size,
        "schema_version": schema_version(conn),
        "compatible": index_is_compatible(conn),
        "meta": meta,
        "counts": counts,
    }


@contextmanager
def transaction(conn: sqlite3.Connection):
    conn.commit()
    conn.execute("BEGIN IMMEDIATE")
    try:
        yield conn
        conn.commit()
    except Exception:
        conn.rollback()
        raise


def corrupt_or_missing_reason(conn: sqlite3.Connection) -> str | None:
    try:
        row = conn.execute("PRAGMA integrity_check").fetchone()
        if not row or row[0] != "ok":
            return f"integrity_check: {row[0] if row else 'sem resposta'}"
        if not index_is_compatible(conn):
            return (
                f"versao de indice incompativel: {schema_version(conn)} "
                f"(esperado {INDEX_SCHEMA_VERSION})"
            )
        conn.execute("SELECT COUNT(*) FROM files").fetchone()
        conn.execute("INSERT INTO file_fts(file_fts) VALUES('integrity-check')")
    except sqlite3.DatabaseError as exc:
        return str(exc)
    return None


def remove_index(root: Path) -> None:
    for suffix in ("", "-wal", "-shm"):
        path = Path(str(db_path(root)) + suffix)
        try:
            path.unlink()
        except FileNotFoundError:
            pass


def ensure_cache_dir(root: Path) -> Path:
    path = root / CACHE_DIR
    path.mkdir(parents=True, exist_ok=True)
    return path
