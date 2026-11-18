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

# Entity count is the pool's high-water mark, not the number of live zombies:
# zombie drips and puddles use native entity slots too. Object07 now consumes
# the native animation-seed RNG draws, so the deterministic high-water mark is
# 19 rather than the old 16. Run far past every child lifetime and require the
# mark to stay fixed; a leaking append-only pool keeps growing.
count=$(printf '%s\n' "$out" | sed -n 's/^Aria simulate: \([0-9][0-9]*\) entities,.*/\1/p')
long_out=$("$bin" --simulate 2000 --area 0 --room 3 \
           --spawn 128 191 --mute 2>/dev/null)
long_count=$(printf '%s\n' "$long_out" | sed -n 's/^Aria simulate: \([0-9][0-9]*\) entities,.*/\1/p')
if [ "$count" != 19 ] || [ "$long_count" != "$count" ]; then
    echo "FAIL: entity pool high-water mark grew ($count -> $long_count)" >&2
    exit 1
fi
echo "aria simulate entity pool: OK (high-water mark $count, deleted slots reused)"
