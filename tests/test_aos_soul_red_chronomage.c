/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillChronomageUse. */
#include "aos/red_souls/aos_soul_red_chronomage.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* A 0x21-frame hold: charge ramps, ticks fire every 16th count. */
    AosChronomage c = aos_chronomage_create(0x29); /* + 8 pre-updates */
    assert(c.state == 0 && c.field == 0x29);
    assert(aos_chronomage_update(&c));
    assert(c.state == 1 && c.charge == 0);
    /* + 0x20 gains 0x1000/frame, + 0x24 gains 0x2000/frame, cap 0x10000. */
    for (int i = 0; i < 8; ++i)
        assert(aos_chronomage_update(&c));
    assert(c.charge == 8 * 0x1000 && c.charge2 == 8 * 0x2000);
    /* The hold counts down; tick requests land on the sixteenth frames. */
    int ticks = 0;
    while (c.state == 1) {
        assert(aos_chronomage_update(&c));
        ticks += c.tick;
    }
    assert(c.state == 2 && c.timer == 0x20 && c.release);
    assert(ticks == 2); /* 0x21 counts: ticks at 0x20 and 0x10 */
    /* Release: -0x1000/-0x800 per frame clamped at 0, 0x20 frames. */
    int frames = 0;
    while (aos_chronomage_update(&c)) ++frames;
    assert(frames == 0x20 && c.charge == 0);
    /* The published time value (+ 0x28 = + 0x20) peaks at 0x10000. */
    AosChronomage p = aos_chronomage_create(0x100);
    for (int i = 0; i < 0x11; ++i) aos_chronomage_update(&p);
    assert(p.charge == 0x10000 && p.charge2 == 0x10000);
    /* A zero-length hold releases as soon as state 1 runs. */
    AosChronomage z = aos_chronomage_create(0);
    aos_chronomage_update(&z);
    assert(aos_chronomage_update(&z));
    assert(z.state == 2 && z.release);
    puts("aos_soul_red_chronomage: ok");
    return 0;
}
