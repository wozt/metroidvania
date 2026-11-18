#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless save-room contract: activate Object1C, confirm the save, then
# verify that its post-save animation eventually releases player movement.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

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

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/aria-save-room.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM
script="$tmp_dir/save.txt"
cat > "$script" <<'EOF'
wait 60
hold 64 20
tap 1 1
wait 10
tap 1 1
hold 16 300
EOF

out=$("$bin" --simulate 430 --simulate-script "$script" \
      --area 0 --room 14 --spawn 120 80 --mute 2>/dev/null)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "SaveData_SaveSlotToSram -> 0" || {
    echo "FAIL: save-room confirmation did not commit" >&2
    exit 1
}
final_x=$(printf '%s\n' "$out" | sed -n 's/.*Soma \([0-9][0-9]*\),[0-9-]* hp.*/\1/p' | tail -1)
if [ -z "$final_x" ] || [ "$final_x" -le 120 ]; then
    echo "FAIL: post-save Object1C lock held Soma at the save statue" >&2
    exit 1
fi
echo "aria simulate save room: OK (save committed and movement resumed at x=$final_x)"
