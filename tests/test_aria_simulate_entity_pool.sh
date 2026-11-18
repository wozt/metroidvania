#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless entity-pool contract: room 0/3's zombie spawner creates and removes
# short-lived zombies and gore.  Native EntityCreateInRange reuses deleted
# slots; the host must not grow the fixed room table without bound.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_00_room_003/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate entity pool skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate entity pool skipped: $bin missing" >&2
    exit 77
fi

out=$(AOS_SIM_DUMP_ENTS=1 "$bin" --simulate 500 --area 0 --room 3 \
      --spawn 128 191 --mute 2>/dev/null)
printf '%s\n' "$out"

# The room has eight static records and the native zombie producer allows at
# most eight live children.  A larger count indicates deleted slots leaked.
printf '%s\n' "$out" | grep -q "Aria simulate: 16 entities," || {
    echo "FAIL: deleted zombie slots were not reused" >&2
    exit 1
}
echo "aria simulate entity pool: OK (deleted slots reused)"
