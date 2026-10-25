#!/usr/bin/env python3
"""LINBLUX automatic checkpoint backup.

Uso:
    python3 tools/backup/backup.py run
    python3 tools/backup/backup.py run --once
    python3 tools/backup/backup.py status
    python3 tools/backup/backup.py verify
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from linblux_backup.cli import main  # noqa: E402

if __name__ == "__main__":
    raise SystemExit(main())
