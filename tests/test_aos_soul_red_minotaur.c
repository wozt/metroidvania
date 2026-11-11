/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/red_souls/aos_soul_red_minotaur.h"

int main(void) {
    /* Facing right: the anchor spawns 0x1A above the source, spins
     * the angle down by -0x800/frame with +0x40 accel, ramps the
     * charge to 0x1FFFF, spins up for 0x50 frames and drains back. */
    AosRedMinotaur m = aos_red_minotaur_create(0x6400000, 0x5000000, false);
    assert(m.x == 0x6400000);
    assert(m.y == 0x5000000 - 0x1A0000);
    assert(m.angle == 0x4000);
    assert(m.step == -0x800);
    assert(m.accel == 0x40);
    assert(m.charge == 0);
    assert(m.timer == 0x50);
    assert(m.state == 1);

    /* Mirrored facing flips all three spin fields. */
    AosRedMinotaur mm = aos_red_minotaur_create(0, 0, true);
    assert(mm.angle == -0x4000);
    assert(mm.step == 0x800);
    assert(mm.accel == -0x40);

    /* State 1 ramps +0x24 by 0x2000 per update; the 16th update
     * crosses the cap and the next enters state 2. */
    int frames = 0;
    while (m.state == 1 && frames < 64) {
        assert(aos_red_minotaur_update(&m));
        frames++;
    }
    assert(m.state == 2);
    assert(m.charge == 0x20000);
    assert(frames == 17); /* 16 ramp frames + the state-2 entry */

    /* State 2 spins the angle by the accelerating step for 0x51
     * frames (0x50 decrements plus the zero check) before state 3. */
    int16_t s0 = (int16_t)m.step;
    int16_t a0 = (int16_t)m.angle;
    frames = 0;
    while (m.state == 2 && frames < 128) {
        assert(aos_red_minotaur_update(&m));
        frames++;
    }
    assert(m.state == 3);
    assert(frames == 0x51);
    /* Every spin frame adds the step to the angle once and the accel
     * to the step once: the final step grew by 0x40 * 0x51. */
    assert((int16_t)m.step == (int16_t)(s0 + 0x40 * frames));
    assert((int16_t)m.angle != a0);

    /* State 3 spins down (step - 2*accel) and drains the charge by
     * 0x2000 per frame: 16 drains from 0x20000 plus the zero-check
     * frame that enters state 4. */
    frames = 0;
    while (m.state == 3 && frames < 64) {
        assert(aos_red_minotaur_update(&m));
        frames++;
    }
    assert(m.state == 4);
    assert(frames == 17);
    assert(m.charge == 0);

    /* State 4 deletes on its update. */
    assert(!aos_red_minotaur_update(&m));

    /* The margin probe forces state 4 from any state, and the next
     * update deletes. */
    AosRedMinotaur out = aos_red_minotaur_create(0, 0, false);
    out.outside = true;
    assert(aos_red_minotaur_update(&out));
    assert(out.state == 4);
    assert(out.hidden);
    assert(!aos_red_minotaur_update(&out));

    /* The blade orbits the anchor at radius 80 px on the shared
     * angle, armed except while the anchor sits in state 2. */
    AosRedMinotaur anchor = aos_red_minotaur_create(0x6400000, 0x5000000, false);
    AosMinotaurBlade blade;
    aos_minotaur_blade_init(&blade);
    assert(blade.state == 1);
    assert(blade.attacking);
    assert(aos_minotaur_blade_update(&blade, &anchor));
    /* angle 0x4000: x += sin(0x4000)*80, y += sin(0)*80 — the orbit
     * sits on the anchor horizontally offset by the sine peak. */
    assert(blade.x == anchor.x + 0x500000);
    assert(blade.y == anchor.y);

    /* Walk the anchor into state 2: the blade disarms once. */
    while (anchor.state == 1) {
        assert(aos_red_minotaur_update(&anchor));
        assert(aos_minotaur_blade_update(&blade, &anchor));
    }
    assert(!blade.attacking);
    assert(blade.state == 2);

    /* Spin-up keeps the blade disarmed; the frame the anchor enters
     * state 3 rearms it (the native child sees the parent state on
     * that same update). */
    int rearmed = 0;
    while (anchor.state == 2) {
        assert(aos_red_minotaur_update(&anchor));
        assert(aos_minotaur_blade_update(&blade, &anchor));
        if (blade.attacking) {
            rearmed = 1;
        }
    }
    assert(rearmed);
    assert(blade.attacking);

    /* The blade dies with the anchor. */
    while (anchor.state != 4) {
        assert(aos_red_minotaur_update(&anchor));
    }
    assert(!aos_minotaur_blade_update(&blade, &anchor));
    assert(!blade.alive);

    puts("test_aos_soul_red_minotaur: ok");
    return 0;
}
