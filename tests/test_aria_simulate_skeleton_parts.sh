#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Skeleton integration contract: EnemySkeletonCreate's separate head and
# held-bone entities must stay linked to their body instead of deleting
# themselves on their first update with a null parent.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_00_room_006/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate Skeleton parts skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate Skeleton parts skipped: $bin missing" >&2
    exit 77
fi

out=$(AOS_SIM_DUMP_ENTS=1 "$bin" --simulate 5 --area 0 --room 6 \
      --spawn 112 191 --mute 2>/dev/null)
printf '%s\n' "$out"

# Roles 32/33 are AOS_ROLE_SKELETON_HEAD/AOS_ROLE_SKELETON_HELD. Each
# native room-0/6 Skeleton creates both; after five frames all five pairs
# must still have a body slot and remain live.
heads=$(printf '%s\n' "$out" | grep -c 'id=2 .*role=32 parent=[0-9][0-9]* removed=0')
held=$(printf '%s\n' "$out" | grep -c 'id=2 .*role=33 parent=[0-9][0-9]* removed=0')
if [ "$heads" != 5 ] || [ "$held" != 5 ]; then
    echo "FAIL: Skeleton child linkage is incomplete (heads=$heads held=$held)" >&2
    exit 1
fi
echo "aria simulate Skeleton parts: OK (5 linked heads, 5 linked held bones)"
