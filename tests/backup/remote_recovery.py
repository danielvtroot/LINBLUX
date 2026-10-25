"""Explicit online proof; never runs as part of the offline unittest suite.

Run under TURBO supervision after the monitor has pushed a permitted canary.
Reads the source/index, clones into a temporary directory, never restores over it.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/backup"))
from linblux_backup.selection import read_snapshot


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--state-dir", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    args = parser.parse_args()
    before = json.loads(args.baseline.read_text())
    state = json.loads((args.state_dir / "state.json").read_text())
    checkpoint = state["last_push"]["hash"]
    assert checkpoint == state["last_checkpoint"]["hash"], "checkpoint sem push confirmado"
    env = {**os.environ, "GIT_TERMINAL_PROMPT": "0", "GIT_OPTIONAL_LOCKS": "0"}
    checked = 0
    with tempfile.TemporaryDirectory(prefix="linblux-github-recovery-") as temporary:
        clone = Path(temporary) / "restored"
        subprocess.run(["git", "clone", "--quiet", "--depth", "1", "--branch", "auto-backup",
                        "https://github.com/danielvtroot/LINBLUX.git", str(clone)],
                       env=env, check=True, timeout=45, capture_output=True)
        actual = subprocess.check_output(["git", "-C", str(clone), "rev-parse", "HEAD"], env=env, text=True).strip()
        assert actual == checkpoint, "o remoto mudou; repita com um checkpoint estável"
        for relative, expected in state["manifest"].items():
            data, _ = read_snapshot(clone, relative, 4 * 1024 * 1024)
            assert hashlib.sha256(data).hexdigest() == expected, "divergência: " + relative
            checked += 1
        canary, _ = read_snapshot(clone, "docs/backup/recovery-canary.txt", 4096)
        canary_hash = hashlib.sha256(canary).hexdigest()
        assert canary_hash == before["canary_sha256"], "marcador não recuperado"
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, env=env, text=True).strip()
    index = hashlib.sha256((ROOT / ".git/index").read_bytes()).hexdigest()
    assert head == before["main_before"], "main foi alterada pelo monitor"
    assert index == before["index_before"], "índice do desenvolvedor foi alterado"
    report = {"ok": True, "remote_checkpoint": checkpoint, "files_restored_and_hashed": checked,
              "canary_sha256": canary_hash, "main_unchanged": True, "index_unchanged": True,
              "temporary_clean_clone": True}
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
