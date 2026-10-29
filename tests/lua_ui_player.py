"""Real Player binary: a UI example validates headless and is painted on screen."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from PIL import Image

player = Path(sys.argv[1]).resolve()
source_root = Path(__file__).resolve().parent.parent
factory = source_root / "assets/defaults/factory/world.loblox.json"
example = source_root / "assets/examples/lua/ui_hud_vida.lua"
root = Path(tempfile.mkdtemp(prefix="linblux-ui-player-"))
project = root / "world.loblox.json"

world = json.loads(factory.read_text())
script = next(item for item in world["objects"] if item["class"] == "Script")
script["source"] = example.read_text()
# The example draws inside Play only; a plain Part keeps the scene valid.
project.write_text(json.dumps(world))
environment = dict(os.environ, HOME=str(root / "home"), XDG_DATA_HOME=str(root / "data"),
                   XDG_CONFIG_HOME=str(root / "config"), XDG_STATE_HOME=str(root / "state"),
                   LIBGL_ALWAYS_SOFTWARE="1", LP_NUM_THREADS="2")
for directory in ("home", "data", "config", "state"):
    (root / directory).mkdir()

validate = subprocess.run([str(player), "--validate", str(project)], capture_output=True,
                          text=True, timeout=60)
if validate.returncode != 0 or "experiência validada" not in validate.stdout:
    raise SystemExit(f"UI example failed validation:\n{validate.stdout}\n{validate.stderr}")

if shutil.which("xdotool") is None or shutil.which("scrot") is None:
    print("Player UI: validation OK; window capture skipped (xdotool/scrot ausente)")
    raise SystemExit(0)

log = (root / "player.log").open("w")
process = subprocess.Popen([str(player), "--project", str(project), "--frames", "900"],
                           cwd=root, env=environment, stdout=log, stderr=log)
try:
    window = None
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline and not window:
        assert process.poll() is None, f"Player exited early: {(root / 'player.log').read_text()}"
        found = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(process.pid),
                                "--name", "^LINBLUX Player$"], capture_output=True, text=True)
        window = found.stdout.strip().splitlines()[-1] if found.returncode == 0 else None
        time.sleep(.2)
    assert window, "Player window not found"
    time.sleep(3)
    shot = root / "player.png"
    subprocess.run(["scrot", "-o", "-u", str(shot)], check=True)
finally:
    if process.poll() is None:
        process.terminate()
    try:
        process.wait(timeout=20)
    except subprocess.TimeoutExpired:
        process.kill()
    log.close()

image = Image.open(shot).convert("RGB")
green = sum(1 for pixel in image.getdata()
            if abs(pixel[0] - 76) < 30 and abs(pixel[1] - 217) < 30 and abs(pixel[2] - 100) < 40)
# The example requests alpha E6, so the panel must be composited with the
# sky underneath, not equal to its unblended #0F0F13 source color. Compare an
# interior strip (away from text/bar/native menu) to same-height sky samples.
alpha = 230 / 255
panel = 0
for y in range(58, 74):
    sky = image.getpixel((330, y))
    expected = [round(c * alpha + sky[i] * (1-alpha)) for i,c in enumerate((15,15,19))]
    panel += sum(all(abs(image.getpixel((x,y))[i]-expected[i]) <= 3 for i in range(3)) for x in range(128,310))
log_text = (root / "player.log").read_text()
assert "Falha" not in log_text, log_text
assert green > 120, f"health bar not painted by the Player (green pixels: {green})"
assert panel > .95 * (310-128) * (74-58), f"UI panel not painted by the Player (panel pixels: {panel})"
print(f"Player UI: example validated headless and painted ({green} bar / {panel} panel pixels); {root}")
