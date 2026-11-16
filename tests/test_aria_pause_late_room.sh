#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless --capture contract for the START pause gate in a later
# room: room 0/19 spawns a passive Object00 door (SPECIAL id 0)
# during the run. Native Object00Create calls sub_0800C5A8(0) only
# inside the Soma-near branch, so a far spawn must leave
# unk_A074_1 alone and the GameModeInGameUpdate case-1 START edge
# must still open the pause hub. Before the fix the port reported 0
# on every door creation and START stayed dead outside the first
# rooms.
# Needs the extracted runtime rooms (fusion_rebuild output,
# gitignored); exits 77 — the CTest SKIP_RETURN_CODE — when they or
# the binary are absent. SDL runs on the dummy video driver, so no
# X server is required.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_00_room_019/room.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria pause late-room skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria pause late-room skipped: $bin missing" >&2
    exit 77
fi

# wait for spawn/settle (the door's spawn-due window opens around
# frame 68), tap START (AOS_KEY_START = 1<<3 = 8) for two frames,
# then let the open fade run before the capture frame.
script=$(mktemp "${TMPDIR:-/tmp}/aria_pause_late_room.XXXXXX")
trap 'rm -f "$script" "$script.bmp"' EXIT
cat > "$script" <<'EOF'
wait 120
tap 8 2
wait 90
EOF

out=$(SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
      "$bin" --area 0 --room 19 --capture "$script.bmp" 400 0 \
      --capture-script "$script" --mute 2>&1)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "Pause hub opened" || {
    echo "FAIL: START did not open the pause hub in room 0/19" >&2
    exit 1
}
[ -s "$script.bmp" ] || {
    echo "FAIL: capture BMP missing or empty" >&2
    exit 1
}
echo "aria pause late-room: OK (room 0/19 pause hub opened)"
