#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless save-room contract: activate Object1C, confirm the save, then
# verify that its post-save animation eventually releases player movement.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"
fixture_bin="${FUSION_AOS_SAVE_FIXTURE:-build/fusion_aos_save_tests}"

need="assets/extracted/aria/rooms/runtime/area_00_room_014/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate save room skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate save room skipped: $bin missing" >&2
    exit 77
fi
if [ ! -x "$fixture_bin" ]; then
    echo "aria simulate save room skipped: $fixture_bin missing" >&2
    exit 77
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/aria-save-room.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM
script="$tmp_dir/save.txt"
save="$tmp_dir/aria-slot-1.sav"
"$fixture_bin" --write-runtime-fixture "$save"
cat > "$script" <<'EOF'
wait 60
hold 64 20
tap 1 1
wait 10
tap 1 1
hold 16 300
EOF

out=$("$bin" --simulate 430 --simulate-script "$script" \
      --save "$save" --mute 2>/dev/null)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "SaveData_SaveSlotToSram -> 0" || {
    echo "FAIL: save-room confirmation did not commit" >&2
    exit 1
}
expected="level 2, experience 120, next 84, gold 7890, equipment 12/5/40, souls 2/3/18"
printf '%s\n' "$out" | grep -q "$expected" || {
    echo "FAIL: loaded persistent state was not present before the save" >&2
    exit 1
}
final_x=$(printf '%s\n' "$out" | sed -n 's/.*Soma \([0-9][0-9]*\),[0-9-]* hp.*/\1/p' | tail -1)
if [ -z "$final_x" ] || [ "$final_x" -le 120 ]; then
    echo "FAIL: post-save Object1C lock held Soma at the save statue" >&2
    exit 1
fi
# Room 0/14 is a one-screen save room whose transition table only
# covers the left edge: holding RIGHT walks Soma into the dead right
# edge (sub_08010350's native scan is unbounded — the port clamps the
# position like a wall instead of dying in take_exit's "No transition"
# path, which used to abort the whole simulate run). The run must
# reach its last frame and must not print the fatal diagnostic.
if printf '%s\n' "$out" | grep -q "No transition"; then
    echo "FAIL: dead room edge reached take_exit's fatal path" >&2
    exit 1
fi
printf '%s\n' "$out" | grep -q "Aria simulate: 430 frames" || {
    echo "FAIL: simulate run aborted before its last frame" >&2
    exit 1
}
# Object1C's dormant state 99 re-asserts +0xA074 |= 2 every frame
# natively (_08054D68); the diagnostic prints on mask changes only,
# so the whole save sequence must stay a handful of lines, not one
# per frame.
a074_lines=$(printf '%s\n' "$out" | grep -c "Object1C 1: +0xA074" || true)
if [ "$a074_lines" -gt 8 ]; then
    echo "FAIL: +0xA074 report spammed $a074_lines lines" >&2
    exit 1
fi

# Start a second process from the file Object1C just committed. This catches
# the former headless split where --save restored only room/position/HP/moves
# and silently reset XP, gold, equipment and souls before frame one.
restart=$("$bin" --save "$save" --simulate 1 --mute 2>/dev/null)
printf '%s\n' "$restart"
printf '%s\n' "$restart" | grep -q "room 0/14" || {
    echo "FAIL: restarted process did not resume the saved room" >&2
    exit 1
}
printf '%s\n' "$restart" | grep -q "$expected" || {
    echo "FAIL: restarted process lost persistent progression" >&2
    exit 1
}
echo "aria simulate save room: OK (save committed, restarted with progression, movement resumed at x=$final_x)"
