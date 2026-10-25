"""Real executable: receive .lo under another HOME, Play/Stop, no authoring writes."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time

sys.dont_write_bytecode = True
from editor_restart import close_window

binary = Path(sys.argv[1]).resolve()
fixture_run = subprocess.run([str(Path(sys.argv[2]).resolve())], capture_output=True,
                             text=True, check=True, timeout=30)
fixture = Path(re.search(r"fixture: (.+)", fixture_run.stdout).group(1))
root = Path(tempfile.mkdtemp(prefix="loblox-experience-startup-"))
home = root / "receiver-home"
home.mkdir()
environment = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(home / "data"),
                   XDG_CONFIG_HOME=str(home / "config"), XDG_STATE_HOME=str(home / "state"),
                   LIBGL_ALWAYS_SOFTWARE="1", LP_NUM_THREADS="2")
trace = home / "state/linblux/linblux.jsonl"

def xdo(*arguments):
    return subprocess.run(["xdotool", *map(str, arguments)], capture_output=True,
                          text=True, check=True, timeout=5).stdout.strip()

def events():
    return [json.loads(line) for line in trace.read_text().splitlines()] if trace.exists() else []

with (root / "application.log").open("w") as output:
    process = subprocess.Popen([str(binary), "--project", str(fixture / "Shared.lo")],
                               cwd=root, env=environment, stdout=output, stderr=output)
    def wait(predicate, timeout=20):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            assert process.poll() is None, f"Application exited: {process.returncode}"
            result = predicate()
            if result:
                return result
            time.sleep(.1)
        raise AssertionError(f"Timed out; artifacts: {root}")
    try:
        def visible_window():
            result = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid",
                                     str(process.pid), "--name", "^LINBLUX Studio$"],
                                    capture_output=True, text=True, timeout=2)
            return result.stdout.strip().splitlines()[-1] if result.returncode == 0 else None
        window = wait(visible_window)
        wait(lambda: any(e.get("event") == "startup_ready" for e in events()))
        xdo("windowmove", window, 0, 0)
        xdo("windowfocus", "--sync", window)
        # Initial font/viewport uploads occur after startup_ready. Input must
        # target a painted, mapped window, in its own coordinate system.
        time.sleep(2)
        manifests = list((home / "data/linblux/experiences").glob("*/experience.json"))
        assert len(manifests) == 1
        manifest = json.loads(manifests[0].read_text())
        place = manifests[0].parent / manifest["mainPlace"]
        original = place.read_bytes()
        assert (manifests[0].parent / manifest["faceImage"]).is_file()
        assert str(fixture).encode() not in original
        subprocess.run(["scrot", "-o", "-u", str(root / "received-studio.png")], check=True)
        xdo("mousemove", "--window", window, 1170, 16)
        xdo("mousedown", 1); time.sleep(.4); xdo("mouseup", 1)
        wait(lambda: any(e.get("subsystem") == "runtime" and e.get("event") == "start" for e in events()))
        time.sleep(.8)
        subprocess.run(["scrot", "-o", "-u", str(root / "received-play.png")], check=True)
        xdo("keydown", "Shift_L"); xdo("keydown", "F5"); time.sleep(.4)
        xdo("keyup", "F5"); xdo("keyup", "Shift_L")
        wait(lambda: any(e.get("subsystem") == "runtime" and e.get("event") == "stop" for e in events()))
        assert place.read_bytes() == original, "Play/Stop wrote to the authored place"
        close_window(window)
        assert process.wait(timeout=15) == 0
        print(f"Actual binary: .lo import in another HOME, Play/Stop, portable face, unchanged authored place; captures: {root}")
    finally:
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=10)
