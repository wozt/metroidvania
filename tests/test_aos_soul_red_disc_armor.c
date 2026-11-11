/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillDiscArmorUse. */
#include "aos/red_souls/aos_soul_red_disc_armor.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void) {
    /* sub_08031EA4-less anchor: + 0x50/+ 0x54 = source +0x40/+0x44 -
     * 0x18000, + 0x2C = 0x2000 (0x6000 mirrored), + 0x2E = -+0x276. */
    AosDiscArmor d = aos_disc_armor_create(0x100000, 0x200000, false);
    assert(d.state == 0 && d.timer == 0x1A);
    assert(d.angle == 0x2000 && d.step == -0x276);
    assert(d.anchor_x == 0x100000 && d.anchor_y == 0x200000 - 0x18000);
    AosDiscArmor m = aos_disc_armor_create(0x100000, 0x200000, true);
    assert(m.angle == 0x6000 && m.step == 0x276);
    /* State 0 falls through into the state-1 orbit on the same frame. */
    assert(aos_disc_armor_update(&d));
    assert(d.state == 1);
    /* The orbit stays within 60 px of the anchor and the angle steps
     * by -0x276 per frame. */
    int32_t a0 = d.angle;
    double max_r = 0;
    for (int i = 0; i < 0x1A; ++i) {
        assert(aos_disc_armor_update(&d));
        double dx = (double)(d.x - d.anchor_x) / 65536.0;
        double dy = (double)(d.y - d.anchor_y) / 65536.0;
        double r = sqrt(dx * dx + dy * dy);
        if (r > max_r) max_r = r;
    }
    assert(max_r <= 60.0 + 0.01);
    assert(d.angle == a0 - 0x276 * 0x1A);
    /* + 0x0D reaching 0 enters state 2; the next update deletes. */
    assert(d.state == 2);
    assert(!aos_disc_armor_update(&d));
    puts("aos_soul_red_disc_armor: ok");
    return 0;
}
