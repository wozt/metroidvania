#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless warp-discovery persistence contract: a fixture save carrying
# unk_60.unk_378 bits loads into the runtime's AosMapWarp mask, the
# Object1C save commit rewrites the file at schema v16 with the same
# mask, and a second process loading that file restores it again —
# the save -> quit -> load round trip the portable schema gained in
# v16. Needs the extracted runtime room and sprite/object libraries
# (fusion_rebuild output, gitignored); exits 77 — the CTest
# SKIP_RETURN_CODE — when absent.
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
        echo "aria simulate warp save skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ] || [ ! -x "$fixture_bin" ]; then
    echo "aria simulate warp save skipped: runtime or fixture binary missing" >&2
    exit 77
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/aria-warp-save.XXXXXX")
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
printf '%s\n' "$out" | grep -q "warp_discovered a5" || {
    echo "FAIL: fixture warp mask was not restored into the runtime" >&2
    exit 1
}
head -1 "$save" | grep -q "metroidvania-aos-save-v16" || {
    echo "FAIL: committed save is not schema v16" >&2
    exit 1
}
grep -q "warp_discovered=a5" "$save" || {
    echo "FAIL: committed save lost the warp discovery mask" >&2
    exit 1
}

# Process restart: a fresh simulate loads the committed file and must
# report the same mask.
out2=$("$bin" --simulate 60 --save "$save" --mute 2>/dev/null)
printf '%s\n' "$out2" | grep -q "warp_discovered a5" || {
    echo "FAIL: warp mask did not survive save/load across processes" >&2
    exit 1
}
echo "aria simulate warp save: OK (load a5, commit v16, reload a5)"
