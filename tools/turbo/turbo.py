#!/usr/bin/env python3
"""LINBLUX ATLAS TURBO command-line interface (offline, Python stdlib)."""

from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))

from turbo_lib.cli import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
