#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless --simulate room-transition contract for fusion_aria_runtime:
# Soma spawned in room 0/0 walks right through the native exit into
# room 0/3 (the same edge/exit-cell trigger the SDL loop runs through
# take_exit). Needs the extracted runtime rooms and sprite/object
# libraries (fusion_rebuild output, gitignored); exits 77 — the CTest
# SKIP_RETURN_CODE — when they or the binary are absent.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_00_room_000/room.tsv
assets/extracted/aria/rooms/runtime/area_00_room_003/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate transition skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate transition skipped: $bin missing" >&2
    exit 77
fi

# Spawn on the walkable floor band of room 0/0 (2x3 screens) and hold
# RIGHT; the room 0/0 -> 0/3 exit sits on the right edge of screen row
# 2, so y must be inside that band (512..720).
out=$("$bin" --simulate 1200 --simulate-hold 16 \
      --area 0 --room 0 --spawn 400 680 --mute 2>/dev/null)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "Room 0/0 -> 0/3" || {
    echo "FAIL: no room 0/0 -> 0/3 transition" >&2
    exit 1
}
printf '%s\n' "$out" | grep -Eq "room changes [1-9][0-9]*" || {
    echo "FAIL: summary reports no room change" >&2
    exit 1
}
final=$(printf '%s\n' "$out" | sed -n 's/.*frames, room \([0-9]*\/[0-9]*\),.*/\1/p' | head -1)
if [ "$final" = "0/0" ]; then
    echo "FAIL: still in room 0/0 after the walk" >&2
    exit 1
fi
echo "aria simulate transition: OK (final room $final)"
