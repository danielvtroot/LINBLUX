"""LINBLUX automatic checkpoint backup.

The tool keeps an isolated git repository outside the project tree and pushes
only an explicit allowlist of files to the ``auto-backup`` branch.
"""

__all__ = ["config", "selection", "state", "store", "runner"]
