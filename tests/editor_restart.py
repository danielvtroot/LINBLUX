"""Isolated real-application save/exit/restart test under Xvfb (no user files)."""
import ctypes
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def wait_for(predicate, process, seconds=15):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError(f"app exited early: {process.returncode}")
        result = predicate()
        if result:
            return result
        time.sleep(0.05)
    raise AssertionError("timed out waiting for application state")


def xdo(*args):
    return subprocess.run(["xdotool", *map(str, args)], capture_output=True,
                          text=True, timeout=5, check=True).stdout.strip()


# Send the normal WM_DELETE_WINDOW request; GLFW must perform normal shutdown.
# Kept here instead of introducing an Xlib dependency into the editor.
class ClientData(ctypes.Union):
    _fields_ = [("b", ctypes.c_char * 20), ("s", ctypes.c_short * 10),
                ("l", ctypes.c_long * 5)]


class ClientMessage(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong),
                ("send_event", ctypes.c_int), ("display", ctypes.c_void_p),
                ("window", ctypes.c_ulong), ("message_type", ctypes.c_ulong),
                ("format", ctypes.c_int), ("data", ClientData)]


class XEvent(ctypes.Union):
    _fields_ = [("client", ClientMessage), ("pad", ctypes.c_long * 24)]


def close_window(window):
    xlib = ctypes.CDLL("libX11.so.6")
    xlib.XOpenDisplay.argtypes = [ctypes.c_char_p]
    xlib.XOpenDisplay.restype = ctypes.c_void_p
    xlib.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    xlib.XInternAtom.restype = ctypes.c_ulong
    xlib.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                              ctypes.c_long, ctypes.POINTER(XEvent)]
    xlib.XFlush.argtypes = xlib.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = xlib.XOpenDisplay(None)
    assert display
    event = XEvent()
    event.client.type = 33  # ClientMessage
    event.client.display = display
    event.client.window = int(window)
    event.client.message_type = xlib.XInternAtom(display, b"WM_PROTOCOLS", 0)
    event.client.format = 32
    event.client.data.l[0] = xlib.XInternAtom(display, b"WM_DELETE_WINDOW", 0)
    assert xlib.XSendEvent(display, int(window), 0, 0, ctypes.byref(event))
    xlib.XFlush(display)
    xlib.XCloseDisplay(display)


def equivalent(a, b):
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return math.isclose(a, b, abs_tol=0.0001, rel_tol=0.00001)
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(equivalent(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(equivalent(x, y) for x, y in zip(a, b))
    return a == b


def main():
    binary = str(Path(sys.argv[1]).resolve())
    root = Path(tempfile.mkdtemp(prefix="loblox-restart-"))
    environment = dict(os.environ, XDG_DATA_HOME=str(root / "data"),
                       XDG_STATE_HOME=str(root / "state"), LOBLOX_TRACE_INPUT="1",
                       LIBGL_ALWAYS_SOFTWARE="1")
    log = root / "state/loblox/loblox.jsonl"

    def events():
        return [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []

    def run(project, save_path, expected=None, benchmark=False):
        before = len(events())
        started = time.monotonic()
        with (root / f"run-{before}.log").open("w") as output:
            arguments = [binary] if project is None else [binary, "--project", str(project)]
            process = subprocess.Popen(arguments, cwd=root,
                                       env=environment, stdout=output, stderr=output)
            try:
                def find_window():
                    result = subprocess.run(["xdotool", "search", "--pid", str(process.pid),
                                             "--name", "^Loblox Studio$"],
                                            capture_output=True, text=True, timeout=2)
                    return result.stdout.strip().splitlines()[-1] if result.returncode == 0 else None
                window = wait_for(find_window, process)
                wait_for(lambda: any(e["event"] == "startup_ready"
                                      for e in events()[before:]), process)
                startup = time.monotonic() - started
                xdo("windowfocus", "--sync", window)
                time.sleep(0.3)
                if benchmark:
                    status = Path(f"/proc/{process.pid}/status").read_text()
                    rss = next(s for s in status.splitlines() if s.startswith("VmRSS:"))
                    def ticks():
                        fields = Path(f"/proc/{process.pid}/stat").read_text().split()
                        return int(fields[13]) + int(fields[14])
                    initial = ticks()
                    cpu_started = time.monotonic()
                    time.sleep(2)
                    cpu = (ticks() - initial) / os.sysconf("SC_CLK_TCK") / (time.monotonic() - cpu_started)
                    print(f"Actual app: startup to ready event {startup:.3f}s; {rss}; CPU {cpu * 100:.1f}% of one core (llvmpipe)")
                previous = save_path.stat().st_mtime_ns if save_path.exists() else 0
                xdo("keydown", "ctrl", "keydown", "s")
                try:
                    wait_for(lambda: save_path.exists() and save_path.stat().st_mtime_ns != previous, process)
                finally:
                    xdo("keyup", "s", "keyup", "ctrl")
                actual = json.loads(save_path.read_text())
                if expected is not None:
                    assert equivalent(expected, actual), "scene changed across process restart"
                close_window(window)
                assert process.wait(timeout=10) == 0, "shutdown failed"
                return actual
            finally:
                if process.poll() is None:
                    process.terminate()
                    process.wait(timeout=5)

    # Explicit missing path must preserve the default scene and not overwrite
    # the failed startup file when saving normally.
    missing = root / "missing.json"
    default = root / "data/loblox/projects/default.loblox.json"
    # Normal startup is now Home; it must not silently open/rewrite an old
    # default project. Explicit legacy projects retain their original contract.
    with (root / "home.log").open("w") as output:
        process = subprocess.Popen([binary], cwd=root, env=environment, stdout=output, stderr=output)
        try:
            window = wait_for(lambda: subprocess.run(["xdotool", "search", "--pid", str(process.pid), "--name", "^Loblox Studio$"], capture_output=True,text=True).stdout.strip(), process)
            wait_for(lambda: any(e["event"] == "startup_ready" for e in events()), process)
            assert not default.exists(), "Home must not create a legacy default file"
            close_window(window.splitlines()[-1]);assert process.wait(timeout=10) == 0
        finally:
            if process.poll() is None: process.terminate();process.wait(timeout=5)
    assert not any(e["event"] == "startup_project_failed" for e in events()), "fresh startup must not report a missing default as an error"
    document = run(missing, default, benchmark=True)
    assert not missing.exists()
    assert {o["class"] for o in document["objects"]} >= {"Baseplate", "SpawnLocation"}
    # Keep the fixture in the writer's preorder: Workspace descendants before
    # additional root services. Compare the complete saved document strictly.
    insert_at = next((i for i, o in enumerate(document["objects"])
                      if o["parent"] == 0 and o["class"] not in ("Workspace", "Lighting")),
                     len(document["objects"]))
    for index in range(2):
        document["objects"].insert(insert_at + index, dict(id=20 + index, parent=1, name=f"RestartPart{index}",
            **{"class": "Part"}, shape="Block", position=[8 * index, 12, -3],
            rotation=[15, 30, 45], size=[8, 2, 4], color=[70, 160, 90, 255],
            material="Wood", anchored=True, canCollide=bool(index)))
    document["nextId"] = 30
    project = root / "edited.json"
    project.write_text(json.dumps(document))
    document = run(project, project, document)
    # Persist deletion and ensure nextId never reuses the deleted ID.
    document["objects"] = [o for o in document["objects"] if o["id"] != 20]
    project.write_text(json.dumps(document))
    run(project, project, document)
    assert any(e["event"] == "startup_project_failed" for e in events())
    assert sum(e["event"] == "startup_project_loaded" for e in events()) == 2
    assert sum(e["event"] == "save_end" and e.get("result") == "ok" for e in events()) == 3
    print(f"Actual app: Home plus 3 editor launches, save/exit/restart, IDs/transforms/properties/deletion OK; artifacts: {root}")
    if len(sys.argv) > 2:
        game_project = root / "animated-r6.json"
        subprocess.run([str(Path(sys.argv[2]).resolve()), "--write-fixture", str(game_project)],
                       env=environment, check=True, timeout=15)
        game = json.loads(game_project.read_text())
        assert game["version"] == 2 and len(game["game"]["rigs"][0]["bodies"]) == 6
        game = run(game_project, game_project, game)
        game = run(game_project, game_project, game)
        assert game["game"]["player"]["walkSpeed"] == 18
        assert any(k["time"] == .25 for k in game["game"]["animations"][0]["tracks"][3])
        print("Actual app: 2 additional launches; custom R6, edited Walk keyframes and player settings preserved")


if __name__ == "__main__":
    main()
