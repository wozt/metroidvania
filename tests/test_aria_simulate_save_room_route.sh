#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless save-room route contract: after the Object1C save sequence
# releases its input lock, Soma exits room 0/36 through the left edge
# into 0/5, crosses back into 0/36 through 0/5's right edge, then
# walks into 0/36's dead right edge — a position outside the room
# bounds that matches no transition-table entry (sub_08010350's
# native scan is unbounded; the port clamps the position like a
# wall). The route must complete every simulated frame, must not
# print take_exit's fatal "No transition" diagnostic, and must
# produce the three real transitions in order. Needs the extracted
# runtime rooms and sprite/object libraries (fusion_rebuild output,
# gitignored); exits 77 — the CTest SKIP_RETURN_CODE — when absent.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"
fixture_bin="${FUSION_AOS_SAVE_FIXTURE:-build/fusion_aos_save_tests}"

need="assets/extracted/aria/rooms/runtime/area_00_room_036/room.tsv
assets/extracted/aria/rooms/runtime/area_00_room_005/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate save-room route skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate save-room route skipped: $bin missing" >&2
    exit 77
fi
if [ ! -x "$fixture_bin" ]; then
    echo "aria simulate save-room route skipped: $fixture_bin missing" >&2
    exit 77
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/aria-save-route.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM
script="$tmp_dir/route.txt"
save="$tmp_dir/aria-slot-1.sav"
"$fixture_bin" --write-runtime-fixture "$save"
cat > "$script" <<'EOF'
wait 60
hold 64 20
tap 1 1
wait 10
tap 1 1
wait 400
hold 32 500
hold 16 900
hold 32 500
EOF

out=$("$bin" --simulate 2600 --simulate-script "$script" \
      --save "$save" --area 0 --room 36 --spawn 120 80 --mute 2>/dev/null)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "SaveData_SaveSlotToSram -> 0" || {
    echo "FAIL: save-room confirmation did not commit" >&2
    exit 1
}
if printf '%s\n' "$out" | grep -q "No transition"; then
    echo "FAIL: dead room edge reached take_exit's fatal path" >&2
    exit 1
fi
printf '%s\n' "$out" | grep -q "Aria simulate: 2600 frames" || {
    echo "FAIL: simulate run aborted before its last frame" >&2
    exit 1
}
# 0/36 -> 0/5 on the save-room exit, 0/5 -> 0/36 on the return edge,
# then 0/36 -> 0/5 again after the dead-edge clamp let LEFT carry
# Soma back out — the oscillation the bug report described, now
# bounded by real exits only.
for want in "Room 0/36 -> 0/5" "Room 0/5 -> 0/36"; do
    printf '%s\n' "$out" | grep -q "$want" || {
        echo "FAIL: missing transition $want" >&2
        exit 1
    }
done
second_exit=$(printf '%s\n' "$out" | grep -c "Room 0/36 -> 0/5" || true)
if [ "$second_exit" -lt 2 ]; then
    echo "FAIL: save room did not re-exit after the dead-edge clamp" >&2
    exit 1
fi
echo "aria simulate save-room route: OK (save, exit, return, dead edge, re-exit)"
