#!/usr/bin/env bash
# Private compositor: never sends XTest events to the user's desktop.
set -euo pipefail
if [ "$(id -u)" -eq 0 ]; then
    printf 'Execute como usuario comum; sway headless nao deve rodar como root.\n' >&2
    exit 77
fi
for dependency in sway swaymsg Xwayland xdotool python3; do
    command -v "$dependency" >/dev/null || { printf 'Ausente: %s\n' "$dependency" >&2; exit 77; }
done
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
input_binary="$(realpath "${1:-./build/loblox_editor_input_tests}")"
editor_binary="$(realpath "${2:-./build/loblox}")"
sway_test_dir="$(mktemp -d /tmp/loblox-sway-XXXXXX)"
mkdir -m 700 "$sway_test_dir/runtime"
export XDG_RUNTIME_DIR="$sway_test_dir/runtime"
export WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_LIBINPUT_NO_DEVICES=1
export LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS="${LP_NUM_THREADS:-2}"
unset DISPLAY WAYLAND_DISPLAY SWAYSOCK
sway -c "$script_dir/headless-sway.conf" -d >"$sway_test_dir/sway.log" 2>&1 &
sway_test_pid=$!
trap 'kill "$sway_test_pid" 2>/dev/null || true; wait "$sway_test_pid" 2>/dev/null || true' EXIT
for attempt in {1..100}; do
    kill -0 "$sway_test_pid" 2>/dev/null || { tail -n 30 "$sway_test_dir/sway.log"; exit 1; }
    sway_test_display="$(sed -n 's/.*Starting Xwayland on \(:[0-9][0-9]*\).*/\1/p' "$sway_test_dir/sway.log" | tail -n 1)"
    if [ -n "$sway_test_display" ]; then
        export DISPLAY="$sway_test_display"
        if xdotool getdisplaygeometry >/dev/null 2>&1; then break; fi
    fi
    sleep 0.1
done
test -n "${DISPLAY:-}" || { tail -n 30 "$sway_test_dir/sway.log"; exit 1; }
printf 'Sway privado: %s; DISPLAY=%s\n' "$sway_test_dir" "$DISPLAY"
"$input_binary"
python3 "$script_dir/editor_restart.py" "$editor_binary"
