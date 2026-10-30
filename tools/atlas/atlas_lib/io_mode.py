"""I/O policy for the ATLAS (2.1): ``normal``, ``hdd-safe`` and ``hdd-critical``.

The policy is a small immutable value object resolved once per process from
``--io-mode`` or ``ATLAS_IO_MODE`` and threaded into the only two places that can
touch the disk: the SQLite connection factory (read-only opens) and the indexer.

Resolving a policy reads nothing but the environment, so asking for a mode never
walks the project tree. Nothing here can start an index build on its own.
"""

from __future__ import annotations

import os
from dataclasses import dataclass

MODES = ("normal", "hdd-safe", "hdd-critical")
ENV_VAR = "ATLAS_IO_MODE"

# Short aliases so an agent can type ``ATLAS_IO_MODE=safe`` without guessing.
ALIASES = {"safe": "hdd-safe", "critical": "hdd-critical", "hdd": "hdd-safe"}

# Commands that only read the index. They are allowed in every mode and are the
# only ones that open SQLite read-only connections.
READ_COMMANDS = frozenset(
    {
        "status",
        "health",
        "locate",
        "symbol",
        "text",
        "impact",
        "trace",
        "tests-for",
        "diagnose",
        "fix-plan",
        "eval",
    }
)

# Commands whose main effect is a write to the index or to the repository.
WRITE_COMMANDS = frozenset({"init", "update", "docs"})

# Commands that read every source file (SHA-256 / integrity checks).
INTEGRITY_COMMANDS = frozenset({"doctor", "verify"})


@dataclass(frozen=True)
class IoPolicy:
    """What the current process is allowed to do to the disk."""

    name: str
    read_only: bool
    allow_index_write: bool
    allow_integrity: bool
    allow_bench: bool
    allow_reports: bool
    check_staleness: bool
    note: str

    def blocked_reason(self, command: str) -> str | None:
        """Return why ``command`` is refused in this mode, or ``None``."""
        if command in WRITE_COMMANDS and not self.allow_index_write:
            return (
                f"modo {self.name}: escrita no índice bloqueada "
                f"({command}); peça autorização e execute com ATLAS_IO_MODE=normal"
            )
        if command == "docs" and not self.allow_reports:
            return f"modo {self.name}: geração de relatórios bloqueada (docs)"
        if command in INTEGRITY_COMMANDS and not self.allow_integrity:
            return (
                f"modo {self.name}: verificação de integridade/hashes bloqueada "
                f"({command}); use ATLAS_IO_MODE=normal quando houver autorização"
            )
        if command == "bench" and not self.allow_bench:
            return f"modo {self.name}: benchmark bloqueado para não estressar o disco"
        return None

    def as_dict(self) -> dict:
        return {
            "mode": self.name,
            "read_only": self.read_only,
            "index_write": self.allow_index_write,
            "integrity_checks": self.allow_integrity,
            "bench": self.allow_bench,
            "reports": self.allow_reports,
            "staleness_check": self.check_staleness,
        }


NORMAL = IoPolicy(
    name="normal",
    read_only=False,
    allow_index_write=True,
    allow_integrity=True,
    allow_bench=True,
    allow_reports=True,
    check_staleness=True,
    note="",
)

HDD_SAFE = IoPolicy(
    name="hdd-safe",
    read_only=True,
    allow_index_write=True,
    allow_integrity=True,
    allow_bench=True,
    allow_reports=True,
    check_staleness=True,
    note=(
        "consultas abrem o índice somente leitura; atualização só quando pedida "
        "explicitamente (update/init)"
    ),
)

HDD_CRITICAL = IoPolicy(
    name="hdd-critical",
    read_only=True,
    allow_index_write=False,
    allow_integrity=False,
    allow_bench=False,
    allow_reports=False,
    check_staleness=False,
    note=(
        "somente leitura: nenhuma indexação, verificação de hash ou relatório; "
        "o índice pode estar desatualizado e isso não é verificado"
    ),
)

_POLICIES = {policy.name: policy for policy in (NORMAL, HDD_SAFE, HDD_CRITICAL)}


def resolve(value: str | None = None, env: dict | None = None) -> IoPolicy:
    """Resolve the mode from an explicit value, then the environment."""
    raw = (value or (env if env is not None else os.environ).get(ENV_VAR) or "").strip()
    if not raw:
        return NORMAL
    key = raw.lower()
    key = ALIASES.get(key, key)
    if key not in _POLICIES:
        raise ValueError(
            f"modo de I/O inválido: {raw} (use {' ou '.join(MODES)}, "
            f"via --io-mode ou {ENV_VAR})"
        )
    return _POLICIES[key]
