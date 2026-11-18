#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless Object35 carrier-child contract: room 10/1 places two SPECIAL
# id-53 (object 0x35) sequence actors; the param0 == 0 one's Object35Create
# fires sub_0805A50C, which natively runs EntityCreateInRange(0x49, 0xCF,
# sub_0805A5BC). The port must append the inert carrier child through the
# shared slot recycler — one extra id-53 record linked to its parent —
# and keep it for the room's lifetime (the bx-lr update never deletes).
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_10_room_001/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate object35 child skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate object35 child skipped: $bin missing" >&2
    exit 77
fi

out=$("$bin" --simulate 300 --area 10 --room 1 --spawn 128 160 --mute 2>/dev/null)
printf '%s\n' "$out"

# Two placed id-53 records plus the param0 == 0 carrier child: three
# records, all spawned (flags & 0xF spawns unconditionally, the child is
# born spawned like the native EntityCreateInRange record).
line=$(printf '%s\n' "$out" | sed -n 's/^  id 53: //p')
if [ "$line" != "3 record(s), 3 spawned" ]; then
    echo "FAIL: Object35 carrier child missing (id 53: ${line:-<none>})" >&2
    exit 1
fi
count=$(printf '%s\n' "$out" | sed -n 's/^Aria simulate: \([0-9][0-9]*\) entities,.*/\1/p')
if [ "$count" != 3 ]; then
    echo "FAIL: expected 3 room entities (2 placed + 1 carrier), got ${count:-<none>}" >&2
    exit 1
fi

# The child is inert and never deleted: a longer run keeps the same
# high-water mark instead of growing the pool.
long_out=$("$bin" --simulate 2000 --area 10 --room 1 --spawn 128 160 --mute 2>/dev/null)
long_count=$(printf '%s\n' "$long_out" | sed -n 's/^Aria simulate: \([0-9][0-9]*\) entities,.*/\1/p')
if [ "$long_count" != "$count" ]; then
    echo "FAIL: entity pool high-water mark grew ($count -> $long_count)" >&2
    exit 1
fi
echo "aria simulate object35 child: OK (id 53: $line, high-water mark $count)"
