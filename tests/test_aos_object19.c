#include "../src/runtime/aos/game/aos_object19.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject19 o;

    /* Probe miss: +0x58 bit 0x20, update skipped. */
    aos_object19_create(&o, 100, 0, 0, 0);
    o.flags58 = 0;
    aos_object19_update(&o, false);
    assert(o.flags58 & 0x20);
    assert(o.fx == 0 && o.fy == 0);

    /* Object19Create arms the + 0xA = 0 record immediately:
     * right leg at +1.0, lifetime (param0 + 0xE) << 16 halved
     * once = 114 << 15 = 0x390000, angle rate 0x400. */
    aos_object19_create(&o, 100, 0, 0, 0);
    assert(o.state == 0);
    assert(o.vx == 0x10000 && o.vy == 0);
    assert(o.lifetime == 0x390000);
    assert(o.angle_rate == 0x400);
    assert(!(o.flags58 & 0x40));

    /* First near update integrates the armed right leg and
     * counts the lifetime down by 0x10000. */
    aos_object19_update(&o, true);
    assert(!(o.flags58 & 0x21));
    assert(o.fx == 0x10000 && o.fy == 0);
    assert(o.lifetime == 0x380000);
    assert(o.angle == 0x400);

    /* param1 bit 0 mirrors: vx negated, +0x16 negated to
     * 0xFC00 and +0x58 bit 0x40 set. param1 = 2 does NOT
     * mirror — + 0x32 collapses to bit 0. */
    AosObject19 m;
    aos_object19_create(&m, 100, 1, 0, 0);
    assert(m.mirror == 1 && m.vx == -0x10000);
    assert(m.angle_rate == (uint16_t)-0x400);
    assert(m.flags58 & 0x40);
    AosObject19 m2;
    aos_object19_create(&m2, 100, 2, 0, 0);
    assert(m2.mirror == 0 && m2.vx == 0x10000);
    assert(m2.angle_rate == 0x400 && !(m2.flags58 & 0x40));

    /* The right leg expires into state 1 = down 22.0 (vy
     * +1.0, lifetime 0x160000, NOT halved — the asrs #1 ran
     * only in create). */
    AosObject19 d;
    aos_object19_create(&d, 100, 0, 0, 0);
    int guard = 0;
    while (d.state == 0 && ++guard < 200)
        aos_object19_update(&d, true);
    assert(d.state == 1);
    assert(d.vx == 0 && d.vy == 0x10000);
    assert(d.lifetime == 0x160000);

    /* param1 mirrors the horizontal legs: state 2's left leg
     * becomes +1.0 under the mirror. */
    AosObject19 h;
    aos_object19_create(&h, 100, 1, 0, 0);
    guard = 0;
    while (h.state != 2 && ++guard < 400)
        aos_object19_update(&h, true);
    assert(h.state == 2);
    assert(h.vx == 0x10000 && h.vy == 0);
    /* Horizontal lifetime: (param0 + 0xE) << 16, unhalved. */
    assert(h.lifetime > 90 << 16 && h.lifetime <= 114 << 16);

    /* Full rectangle cycle returns to state 0 armed right;
     * the update-path arm does NOT halve, so the lifetime is
     * (param0 + 0xE) << 16 = 24 << 16. */
    AosObject19 c;
    aos_object19_create(&c, 10, 0, 0, 0);
    guard = 0;
    do {
        aos_object19_update(&c, true);
    } while (!(c.state == 0 && c.vx == 0x10000 && c.vy == 0 &&
               c.lifetime == (24 << 16)) &&
             ++guard < 400);
    assert(guard < 400);

    puts("aos_object19: ok");
    return 0;
}
