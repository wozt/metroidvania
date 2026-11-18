#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless continuous-play contract: walk from room 0/0 through 0/3, fight
# through the real weapon/enemy path, then continue into room 0/5.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_00_room_000/room.tsv
assets/extracted/aria/rooms/runtime/area_00_room_003/room.tsv
assets/extracted/aria/rooms/runtime/area_00_room_005/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv
assets/extracted/aria/sprites/weapons/runtime/runtime_index.tsv
assets/extracted/aria/metadata/enemies.tsv
assets/extracted/aria/metadata/weapons.tsv
assets/extracted/aria/metadata/weapon_frames.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate route skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate route skipped: $bin missing" >&2
    exit 77
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/aria-route.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM
script="$tmp_dir/route.txt"
printf 'hold 16 220\n' > "$script"
i=0
while [ "$i" -lt 120 ]; do
    printf 'tap 2 1\nhold 16 8\n' >> "$script"
    i=$((i + 1))
done
printf 'hold 16 1100\n' >> "$script"

out=$("$bin" --simulate 2200 --simulate-script "$script" \
      --area 0 --room 0 --spawn 400 680 --weapon 1 --atk 9999 --mute 2>/dev/null)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "Room 0/0 -> 0/3" || {
    echo "FAIL: opening room transition did not occur" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "Room 0/3 -> 0/5" || {
    echo "FAIL: post-combat room transition did not occur" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "Experience 1 (next level in 83)" || {
    echo "FAIL: first native zombie XP reward was not observed" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "Experience 2 (next level in 82)" || {
    echo "FAIL: second native zombie XP reward was not observed" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q \
    "Aria simulate: 2200 frames, room 0/5, .* room changes 2" || {
    echo "FAIL: route did not finish in room 0/5 after two transitions" >&2
    exit 1
}
echo "aria simulate route: OK (0/0 -> 0/3 -> 0/5 with combat and XP)"
