/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/red_souls/aos_soul_red_flame_demon.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* Volley bolt: sine fan velocity, mirrored flips vx only. */
    AosFlameSpirit bolt = aos_flame_bolt_create(100 << 16, 50 << 16, 0, false);
    assert(bolt.state == 1 && !bolt.lord);
    assert(bolt.vx == 0x20000 && bolt.vy == 0);
    AosFlameSpirit bolt_m = aos_flame_bolt_create(100 << 16, 50 << 16, 0, true);
    assert(bolt_m.vx == -0x20000 && bolt_m.vy == 0);

    /* Bolt drifts on its velocity and ends on the margin probe. */
    int32_t x0 = bolt.x;
    bool puff = true;
    assert(aos_flame_spirit_update(&bolt, false, false, &puff));
    assert(bolt.x > x0 && !puff);           /* bolts never puff */
    assert(aos_flame_spirit_update(&bolt, false, true, &puff));
    assert(bolt.state == 2);
    assert(!aos_flame_spirit_update(&bolt, false, false, &puff));

    /* Demon Lord wisp: vx +/-0x18000, vy +/-0x400 by facing, spin 0x234,
     * fire puff on every eighth frame. */
    AosFlameSpirit wisp = aos_flame_spirit_create(10 << 16, 20 << 16, false);
    assert(wisp.lord && wisp.vx == 0x18000 && wisp.vy == 0x400);
    assert(wisp.spin_rate == 0x234);
    AosFlameSpirit wisp_m = aos_flame_spirit_create(10 << 16, 20 << 16, true);
    assert(wisp_m.vx == -0x18000 && wisp_m.vy == -0x400 && wisp_m.spin_rate == -0x234);

    for (int i = 1; i <= 8; ++i) {
        bool p = false;
        assert(aos_flame_spirit_update(&wisp, false, false, &p));
        assert(wisp.spin == (uint16_t)(0x234 * i));
        assert(p == (i == 8));
    }
    assert(aos_flame_spirit_update(&wisp, true, false, &puff));
    assert(wisp.state == 2);
    assert(!aos_flame_spirit_update(&wisp, false, false, &puff));

    /* The 0x80000 speed cap holds. */
    AosFlameSpirit fast = aos_flame_spirit_create(0, 0, false);
    fast.vx = 0x100000;
    fast.vy = -0x100000;
    (void)aos_flame_spirit_update(&fast, false, false, NULL);
    assert(fast.vx == 0x80000 && fast.vy == -0x80000);

    puts("aos_soul_red_flame_demon: ok");
    return 0;
}
