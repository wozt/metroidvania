#include "../src/runtime/aos/game/aos_object19.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject19 o;

    /* Probe miss: +0x58 bit 0x20, update skipped. */
    aos_object19_create(&o, 100, 0, 0, 0, 0);
    o.flags58 = 0;
    aos_object19_update(&o, false);
    assert(o.flags58 & 0x20);
    assert(o.state == 0 && o.fx == 0 && o.fy == 0);

    /* First update: lifetime 0 expires state 0 into state 1 armed
     * with the down leg (vy = +1.0, lifetime 22.0). */
    aos_object19_update(&o, true);
    assert(!(o.flags58 & 0x21));
    assert(o.state == 1);
    assert(o.vy == 0x10000 && o.vx == 0);
    assert(o.lifetime == 0x160000);
    /* The down leg integrates this same frame? No — integration
     * ran before the expiry with zero velocity. */
    assert(o.fx == 0 && o.fy == 0);

    /* param1 mirrors the horizontal legs. */
    AosObject19 m;
    aos_object19_create(&m, 100, 1, 0, 0, 0);
    /* state0 expiry arms state1 (down, no x) — drive to state2. */
    for (int i = 0; i < 30; i++) aos_object19_update(&m, true);
    /* state1's 22-frame leg expired into state2 = left leg;
     * param1 == 1 negates vx: -(-1.0) = +1.0. */
    assert(m.state == 2);
    assert(m.vx == 0x10000);

    /* param0 supplies the horizontal leg lifetime. */
    AosObject19 h;
    aos_object19_create(&h, 100, 0, 0, 0, 0);
    for (int i = 0; i < 30; i++) aos_object19_update(&h, true);
    assert(h.state == 2);
    assert(h.lifetime > 90 << 16 && h.lifetime <= 100 << 16);
    assert(h.vx == -0x10000);

    /* angle accumulator. */
    AosObject19 a;
    aos_object19_create(&a, 0, 0, 0, 0, 0x40);
    aos_object19_update(&a, true);
    assert(a.angle == 0x40);

    /* Full rectangle cycle returns to state 0 armed right. */
    AosObject19 c;
    aos_object19_create(&c, 10, 0, 0, 0, 0);
    int guard = 0;
    do {
        aos_object19_update(&c, true);
    } while (!(c.state == 0 && c.vx == 0x10000 && c.vy == 0) &&
             ++guard < 400);
    assert(guard < 400);

    puts("aos_object19: ok");
    return 0;
}
