#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Water buoyancy contract: room 0/5's pool (bit-3 cells, y 400-447) must make
# Soma float at head-at-surface depth (~world y 425, mGBA-measured) instead of
# sinking to the pool floor.  Spawned deep, he has to rise; held at the
# surface he must not fall through to the bottom.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_00_room_005/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate water float skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate water float skipped: $bin missing" >&2
    exit 77
fi

# Deep spawn inside the pool on the right side (clear of the merman at the
# left edge): native buoyancy lifts Soma back to the float depth within a few
# dozen frames; without it he rests on the floor near y=448.
out=$("$bin" --simulate 500 --area 0 --room 5 --spawn 360 440 --mute 2>/dev/null)
printf '%s\n' "$out"

y=$(printf '%s\n' "$out" | sed -n 's/.*Soma [0-9][0-9]*,\([0-9][0-9]*\) hp.*/\1/p')
if [ -z "$y" ]; then
    echo "FAIL: could not read final Soma position" >&2
    exit 1
fi
# Pool floor top is ~448; the float depth is ~425 (mGBA-measured).  Require
# him well off the bottom so a slow residual sink cannot pass.
if [ "$y" -ge 432 ]; then
    echo "FAIL: Soma sank toward the pool floor (final y=$y)" >&2
    exit 1
fi
echo "aria simulate water float: OK (final y=$y, floated off the floor)"
