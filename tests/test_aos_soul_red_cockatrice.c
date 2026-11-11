/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillCockatriceUse. */
#include "aos/red_souls/aos_soul_red_cockatrice.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* + 0x2C = 0x2800 with + 0x2E = -0x40; mirrored inverts both. */
    AosCockatrice c = aos_cockatrice_create(0x100000, 0x200000, false);
    assert(c.state == 0 && c.timer == 0x20);
    assert(c.angle == 0x2800 && c.step == -0x40);
    AosCockatrice m = aos_cockatrice_create(0x100000, 0x200000, true);
    assert(m.angle == (int32_t)0xFFFF8000 - 0x2800 && m.step == 0x40);
    /* State 0 falls through into the state-1 spin on the same frame. */
    int32_t a0 = c.angle;
    assert(aos_cockatrice_update(&c, false));
    assert(c.state == 1 && c.angle == a0 - 0x40);
    /* + 0x50/+ 0x54 = sine(angle +/- 0x4000) * 32 track the spin. */
    assert(c.vx != 0 || c.vy != 0);
    /* 0x20 frames of spin then the delete frame returns false. */
    int frames = 1;
    while (aos_cockatrice_update(&c, false)) ++frames;
    assert(frames == 0x20);
    /* The margin probe deletes from any state. */
    AosCockatrice o = aos_cockatrice_create(0, 0, false);
    aos_cockatrice_update(&o, false);
    assert(!aos_cockatrice_update(&o, true));
    puts("aos_soul_red_cockatrice: ok");
    return 0;
}
