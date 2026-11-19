#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Water entry/jump contract for room 0/5's pool (bit-3 cells, float depth
# ~world y 424, mGBA-measured):
#   1. falling in fast makes Soma dip below the float depth before buoyancy
#      settles him (the "sink a bit" on water entry);
#   2. the surface float is stable - after settling, y/vy/flags stop changing
#      (no head-above/head-below limit cycle that read as sprite tremble);
#   3. tapping jump while treading fires the swim stroke and breaches the
#      surface (BODY_SPECIAL lets the stroke run with the head just out).
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
bin="${FUSION_ARIA_RUNTIME:-build/fusion_aria_runtime}"

need="assets/extracted/aria/rooms/runtime/area_00_room_005/room.tsv
assets/extracted/aria/sprites/soma/runtime/runtime_index.tsv"
for f in $need; do
    if [ ! -f "$f" ]; then
        echo "aria simulate water entry/jump skipped: $f missing" >&2
        exit 77
    fi
done
if [ ! -x "$bin" ]; then
    echo "aria simulate water entry/jump skipped: $bin missing" >&2
    exit 77
fi

# Drop into the pool from above the surface: he should penetrate below the
# float depth (entry dip) and then float back to ~424.
trace=$(AOS_SIM_TRACE_SOMA=1 "$bin" --simulate 90 --area 0 --room 5 \
    --spawn 360 340 --mute 2>/dev/null | sed -n 's/.*pos=[0-9]*,\([0-9]*\).*/\1/p')
[ -n "$trace" ] || { echo "FAIL: no SOMATRACE positions"; exit 1; }

max_y=$(printf '%s\n' "$trace" | sort -n | tail -1)
final_y=$(printf '%s\n' "$trace" | tail -1)
# Float depth is ~424; the pool floor is ~448. A real entry dip goes clearly
# below 424 but must never reach the floor.
if [ "$max_y" -le 426 ]; then
    echo "FAIL: no entry dip (max y=$max_y, expected > 426)" >&2
    exit 1
fi
if [ "$max_y" -ge 446 ]; then
    echo "FAIL: sank to the pool floor (max y=$max_y)" >&2
    exit 1
fi
if [ "$final_y" -gt 428 ]; then
    echo "FAIL: did not float back (final y=$final_y)" >&2
    exit 1
fi

# Stability: once settled the last frames must hold one y (no tremble cycle).
settled=$(printf '%s\n' "$trace" | tail -20 | sort -u | wc -l | tr -d ' ')
if [ "$settled" -ne 1 ]; then
    echo "FAIL: surface float oscillates ($settled distinct settled y values)" >&2
    exit 1
fi

# Jump out: tap jump repeatedly while treading; the stroke must carry him
# above the float depth (breach the surface).
script=$(mktemp)
cat > "$script" <<'EOF'
wait 30
tap 1 1
wait 6
tap 1 1
wait 6
tap 1 1
wait 6
tap 1 1
wait 6
tap 1 1
wait 6
tap 1 1
wait 6
tap 1 1
wait 6
tap 1 1
wait 40
EOF
jtrace=$(AOS_SIM_TRACE_SOMA=1 "$bin" --simulate 130 --area 0 --room 5 \
    --spawn 360 430 --simulate-script "$script" --mute 2>/dev/null \
    | sed -n 's/.*pos=[0-9]*,\([0-9]*\).*/\1/p')
rm -f "$script"
[ -n "$jtrace" ] || { echo "FAIL: no jump SOMATRACE positions"; exit 1; }
min_y=$(printf '%s\n' "$jtrace" | sort -n | head -1)
# Float depth ~424; breaching the surface reaches well above it.
if [ "$min_y" -ge 414 ]; then
    echo "FAIL: swim stroke did not breach surface (min y=$min_y)" >&2
    exit 1
fi

# Jump-cancel during penetration: re-jumping while the body probes are
# already in the water must not park Soma in the head-out/body-in band.
# Measured on the native pool, gravity runs at full strength through the
# whole band (vy reached +2.3 px/frame), so he has to keep sinking until
# the head probes reach the top water cell and the float line holds.
cat > "$script" <<'EOF'
wait 26
tap 1 1
wait 12
tap 1 1
wait 150
EOF
ctrace=$(AOS_SIM_TRACE_SOMA=1 "$bin" --simulate 200 --area 0 --room 5 \
    --spawn 360 340 --simulate-script "$script" --mute 2>/dev/null \
    | sed -n 's/.*pos=[0-9]*,\([0-9]*\).*/\1/p')
rm -f "$script"
[ -n "$ctrace" ] || { echo "FAIL: no cancel SOMATRACE positions"; exit 1; }
cancel_y=$(printf '%s\n' "$ctrace" | tail -1)
if [ "$cancel_y" -lt 420 ] || [ "$cancel_y" -gt 428 ]; then
    echo "FAIL: jump-cancel left Soma off the float line (final y=$cancel_y)" >&2
    exit 1
fi
# He must not linger mid-band: after the inputs end, the last ~30 frames
# have to sit at the float, not strung along the head-out/body-in band.
late=$(printf '%s\n' "$ctrace" | tail -30 | sort -un | tail -1)
if [ "$late" -lt 420 ]; then
    echo "FAIL: jump-cancel still sinking late (max late y=$late)" >&2
    exit 1
fi

echo "aria simulate water entry/jump: OK (dip to y=$max_y, float y=$final_y, breach to y=$min_y, cancel y=$cancel_y)"
