#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# First boss route integration: approach Object02 from inside room 0/9,
# cross its native exit, then prove Creaking Skull and its thirteen still-frame
# bone records are available in room 0/10.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"
index=assets/extracted/aria/sprites/objects/runtime/runtime_index.tsv

for f in \
    assets/extracted/aria/rooms/runtime/area_00_room_009/room.tsv \
    assets/extracted/aria/rooms/runtime/area_00_room_010/room.tsv \
    assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv \
    "$index"; do
    if [ ! -f "$f" ]; then
        echo "aria first boss room skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria first boss room skipped: $bin missing" >&2
    exit 77
fi

# The boss children select native +0x65 still-frame ids 0..11 and 20.
for frame in 0 1 2 3 4 5 6 7 8 9 10 11 20; do
    grep -q "^Enemy/creaking_skull/frame_${frame}[[:space:]]" "$index" || {
        echo "FAIL: Creaking Skull still frame $frame is absent" >&2
        exit 1
    }
done

# Object02 uses descriptor record +0x65 = 0x3F; +0x1B = 6 is its
# palette slot, not animation 6.
grep -q '^Object/special_15_v6/frame_63[[:space:]]' "$index" || {
    echo "FAIL: Object02 static frame 0x3F is absent" >&2
    exit 1
}

# Start far enough from x=504 that Object02 takes its ordinary outside
# create path, opens as Soma approaches and exposes the room transition.
out=$("$bin" --simulate 300 --simulate-hold 16 \
      --area 0 --room 9 --spawn 400 1440 --mute 2>/dev/null)
printf '%s\n' "$out"

printf '%s\n' "$out" | grep -q "Room 0/9 -> 0/10" || {
    echo "FAIL: Object02 did not admit Soma to room 0/10" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "frames, room 0/10," || {
    echo "FAIL: simulation did not remain in the first boss room" >&2
    exit 1
}
printf '%s\n' "$out" | grep -q "music 0x11" || {
    echo "FAIL: Creaking Skull create did not select native song 0x11" >&2
    exit 1
}
alive=$(printf '%s\n' "$out" |
    sed -n 's/.*SPECIAL [0-9][0-9]*\/[0-9][0-9]* spawned, \([0-9][0-9]*\) enemies alive.*/\1/p')
if [ -z "$alive" ] || [ "$alive" -lt 14 ]; then
    echo "FAIL: Creaking Skull body and thirteen bones are not alive" >&2
    exit 1
fi

# Enter normally, allow the near-created left Object02 to finish its
# 48-frame close, then walk back into it.  The three native block writes
# must keep Soma inside room 0/10 instead of permitting a second room exit.
script=$(mktemp "${TMPDIR:-/tmp}/aria_first_boss_gate.XXXXXX")
trap 'rm -f "$script"' EXIT HUP INT TERM
cat > "$script" <<'EOF'
hold 16 130
hold 32 300
EOF
gate_out=$("$bin" --simulate 430 --simulate-script "$script" \
           --area 0 --room 9 --spawn 400 1440 --hp 9999 --mute 2>/dev/null)
changes=$(printf '%s\n' "$gate_out" |
    sed -n 's/.*room changes \([0-9][0-9]*\).*/\1/p' | tail -1)
final_x=$(printf '%s\n' "$gate_out" |
    sed -n 's/.*Soma \([0-9][0-9]*\),[0-9-]* hp.*/\1/p' | tail -1)
if [ "$changes" != 1 ] || [ -z "$final_x" ] || [ "$final_x" -lt 16 ]; then
    echo "FAIL: the closed left boss gate did not retain Soma in room 0/10" >&2
    exit 1
fi

# The runtime path must use the record-container hitbox, not the empty
# animation-frame boxes: overlapping the assembled boss costs HP.
contact_out=$("$bin" --simulate 120 --area 0 --room 10 \
              --spawn 250 176 --hp 500 --mute 2>/dev/null)
printf '%s\n' "$contact_out" | grep -q 'Soma hit:' || {
    echo "FAIL: Creaking Skull did not damage overlapping Soma" >&2
    exit 1
}

# Complete the arena loop: kill the real room entity through Soma's
# weapon path, wait through its death machine, approach the right
# Object02 and cross the native transition after bit 11 releases it.
cat > "$script" <<'EOF'
hold 16 25
tap 2 10
wait 10
tap 2 10
wait 10
tap 2 10
wait 100
hold 16 700
EOF
clear_out=$("$bin" --simulate 865 --simulate-script "$script" \
            --area 0 --room 10 --spawn 180 176 --atk 9999 \
            --def 9999 --hp 9999 --mute 2>/dev/null)
printf '%s\n' "$clear_out" | grep -q 'creaking_skull hit: .* killed' || {
    echo "FAIL: the playable route did not kill Creaking Skull" >&2
    exit 1
}
printf '%s\n' "$clear_out" | grep -q 'Room 0/10 -> 0/35' || {
    echo "FAIL: the right boss gate did not release after the clear" >&2
    exit 1
}

echo "aria first boss room: OK ($alive boss entities, both gates, contact and clear route)"
