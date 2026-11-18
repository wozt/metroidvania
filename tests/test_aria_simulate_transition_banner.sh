#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Headless room-transition load-delay contract: the fixed
# AOS_ROOM_TRANSITION_LOAD_FRAMES blanked span stands in for
# sub_0800F9EC's streaming only when the destination room will arm
# the sub_0800FFCC area-name banner (sub_080104EC's unk_65 == 2
# sub_0800FCB8 pick in [0, 0xC]).  Room 0/0 -> 0/3 banners and must
# keep the ~9-frame delay; the follow-on 0/3 -> 0/5 swap does not
# and must commit on the first stage-1 frame.  The banner pick needs
# the ROM's castle map cells, so this test skips (77) without --rom
# data, like the other private-input tests.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"
rom="${ARIA_ROM:-roms/Castlevania - Aria of Sorrow (USA).gba}"

need="assets/extracted/aria/rooms/runtime/area_00_room_000/room.tsv
assets/extracted/aria/rooms/runtime/area_00_room_003/room.tsv
assets/extracted/aria/rooms/runtime/area_00_room_005/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv
assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate banner transition skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -f "$rom" ]; then
    echo "aria simulate banner transition skipped: $rom missing" >&2
    exit 77
fi
if [ ! -x "$bin" ]; then
    echo "aria simulate banner transition skipped: $bin missing" >&2
    exit 77
fi

# Same walk-right route as test_aria_simulate_transition.sh: room 0/0's
# right edge leads to 0/3 (a banner room) and holding RIGHT carries on
# into 0/5 (no banner pick).
out=$("$bin" --simulate 3000 --simulate-hold 16 \
      --area 0 --room 0 --spawn 400 680 --mute --rom "$rom" 2>/dev/null)
printf '%s\n' "$out"

# Pair each "transition begin frame N" with the next "transition
# commit frame M -> area/room" and report "area/room gap".
gaps=$(printf '%s\n' "$out" | awk '
    /transition begin frame/ { begin = $6; next }
    /transition commit frame/ {
        split($8, a, "/");
        printf "%d/%d %d\n", a[1], a[2], $6 - begin;
    }')
printf '%s\n' "$gaps"

banner_gap=$(printf '%s\n' "$gaps" | awk '$1 == "0/3" { print $2 }')
plain_gap=$(printf '%s\n' "$gaps" | awk '$1 == "0/5" { print $2 }')

[ -n "$banner_gap" ] || { echo "FAIL: no 0/0 -> 0/3 transition" >&2; exit 1; }
[ -n "$plain_gap" ] || { echo "FAIL: no 0/3 -> 0/5 transition" >&2; exit 1; }

# The banner destination keeps the measured blanked load span (9
# countdown frames plus the commit frame itself); the banner-less
# swap commits on the first stage-1 frame.
if [ "$banner_gap" -lt 9 ]; then
    echo "FAIL: banner transition 0/0 -> 0/3 lost its load delay (gap $banner_gap)" >&2
    exit 1
fi
if [ "$plain_gap" -gt 2 ]; then
    echo "FAIL: banner-less transition 0/3 -> 0/5 kept a load delay (gap $plain_gap)" >&2
    exit 1
fi
echo "aria simulate banner transition: OK (0/3 gap $banner_gap, 0/5 gap $plain_gap)"
