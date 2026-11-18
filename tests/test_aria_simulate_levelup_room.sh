#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless kill-to-level regression for a later room: room 0/3's zombie
# spawner children pay 1 native XP each through the ordinary
# Soma -> weapon entity -> enemy collision -> update_entities reward
# path, and crossing the 84-XP level-2 threshold must arm the
# sub_080455BC level-up display state exactly like room 0/0 does.
# The deterministic LCG (aos_random starts at state 0) keeps the
# scripted sweep reproducible; the diagnostic ATK only shortens each
# fight.  Private extracted inputs are never committed; this test
# skips when fusion_rebuild has not produced them.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_00_room_003/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv
assets/extracted/aria/sprites/weapons/runtime/runtime_index.tsv
assets/extracted/aria/metadata/weapons.tsv
assets/extracted/aria/metadata/weapon_frames.tsv
assets/extracted/aria/metadata/enemies.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate level-up room skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate level-up room skipped: $bin missing" >&2
    exit 77
fi

# Sweep the 5-screen room: hold RIGHT, slash, wait, hold LEFT, slash,
# wait.  360 cycles cover ~72k frames — comfortably past the kills
# the zombie spawner's recycle cadence needs for the 84-XP threshold.
script=$(mktemp "${TMPDIR:-/tmp}/aria_levelup_room.XXXXXX")
trap 'rm -f "$script"' EXIT
for i in $(seq 360); do
    printf 'hold 16 40\ntap 2 10\nwait 50\nhold 32 40\ntap 2 10\nwait 50\n'
done > "$script"

out=$("$bin" --simulate 74000 --simulate-script "$script" \
      --area 0 --room 3 --spawn 640 200 \
      --atk 9999 --def 9999 --hp 9999 --mute 2>/dev/null)
printf '%s\n' "$out" | grep "Aria simulate:" || true
printf '%s\n' "$out" | grep -q "Level up: level 2" || {
    echo "FAIL: room 0/3 zombie kills never triggered the level-up" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "Aria simulate: level 2," || {
    echo "FAIL: final progress summary does not show level 2" >&2
    exit 1
}
echo "aria simulate level-up room: OK (room 0/3 zombies -> level 2)"
