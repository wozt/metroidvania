#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless kill-to-level integration contract. A real weapon animation and
# hitbox kill room 5/1's Cockatrice (enemy 46), whose native metadata awards
# 120 XP. New-game Soma crosses the 84-XP level-2 threshold and retains the
# exact reward remainder. Private extracted inputs are never committed; this
# test skips when fusion_rebuild has not produced them.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_05_room_001/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv
assets/extracted/aria/sprites/weapons/runtime/runtime_index.tsv
assets/extracted/aria/metadata/weapons.tsv
assets/extracted/aria/metadata/weapon_frames.tsv
assets/extracted/aria/metadata/enemies.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate XP skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate XP skipped: $bin missing" >&2
    exit 77
fi

# Spawn 32 pixels left of the native Cockatrice record and press B. The high
# diagnostic ATK shortens the fight but the ordinary Soma -> weapon entity ->
# enemy collision -> update_entities reward path remains intact.
out=$("$bin" --simulate 30 --simulate-hold 2 \
      --area 5 --room 1 --spawn 288 223 --weapon 1 --atk 9999 --mute \
      2>/dev/null)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "cockatrice hit: .* killed" || {
    echo "FAIL: Cockatrice was not killed through the weapon path" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "Experience 120 (next level in 84)" || {
    echo "FAIL: exact native Cockatrice XP reward was not reported" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "Level up: level 2, max HP 332" || {
    echo "FAIL: kill did not trigger the native level-up result" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q \
    "Aria simulate: level 2, experience 120, next 84" || {
    echo "FAIL: final progress summary does not retain the exact reward" >&2
    exit 1
}
echo "aria simulate XP: OK (Cockatrice 120 XP -> level 2)"
