#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# First boss route integration: approach Object02 from inside room 0/9,
# cross its native exit, then prove Creaking Skull and its thirteen still-frame
# bone records are available in room 0/10.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"
index=assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv

for f in \
    assets/extracted/aria/rooms/runtime/area_00_room_009/room.tsv \
    assets/extracted/aria/rooms/runtime/area_00_room_010/room.tsv \
    assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv \
    "$index"; do
    if [ ! -f "$f" ]; then
        echo "aria first boss room skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria first boss room skipped: $bin missing" >&2
    exit 77
fi

# The boss children select native +0x65 still-frame ids 0..11 and 20.
for frame in 0 1 2 3 4 5 6 7 8 9 10 11 20; do
    grep -q "^Enemy/creaking_skull/frame_${frame}[[:space:]]" "$index" || {
        echo "FAIL: Creaking Skull still frame $frame is absent" >&2
        exit 1
    }
done

# Start far enough from x=504 that Object02 takes its ordinary outside
# create path, opens as Soma approaches and exposes the room transition.
out=$("$bin" --simulate 300 --simulate-hold 16 \
      --area 0 --room 9 --spawn 400 1440 --mute 2>/dev/null)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "Room 0/9 -> 0/10" || {
    echo "FAIL: Object02 did not admit Soma to room 0/10" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "frames, room 0/10," || {
    echo "FAIL: simulation did not remain in the first boss room" >&2
    exit 1
}
alive=$(printf '%s\n' "$out" |
    sed -n 's/.*SPECIAL [0-9][0-9]*\/[0-9][0-9]* spawned, \([0-9][0-9]*\) enemies alive.*/\1/p')
if [ -z "$alive" ] || [ "$alive" -lt 14 ]; then
    echo "FAIL: Creaking Skull body and thirteen bones are not alive" >&2
    exit 1
fi

echo "aria first boss room: OK ($alive boss entities alive)"
