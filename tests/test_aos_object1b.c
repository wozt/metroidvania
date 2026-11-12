#include "../src/runtime/aos/game/aos_object1b.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject1b o;
    int32_t clock = 0;

    aos_object1b_create(&o);
    assert(o.state == 0 && o.toggle == 0 && o.angle == 0);
    assert(o.frame == 0xFF && o.song == -1);

    /* Unfocused state 0: the tail still runs — angle += clock (0)
     * and the frame resolves to table[0] = 0x18. */
    aos_object1b_update(&o, false, &clock);
    assert(o.state == 0 && o.song == -1);
    assert(o.frame == 0x18);

    /* Focus toggles, plays song 0x133, enters state 1 — the clock
     * ramp belongs to the state-1 case, so it starts next update. */
    aos_object1b_update(&o, true, &clock);
    assert(o.state == 1 && o.toggle == 1);
    assert(o.song == 0x133);
    assert(clock == 0);
    assert(o.angle == 0 && o.frame == 0x18);

    /* Four state-1 updates ramp the shared clock 0x1000 each and
     * accumulate it into the angle. */
    for (int i = 0; i < 4; i++) aos_object1b_update(&o, false, &clock);
    assert(clock == 0x4000);
    assert(o.angle == 0x1000 + 0x2000 + 0x3000 + 0x4000);

    /* Second focus press toggles back (native eors). */
    o.state = 0;
    aos_object1b_update(&o, true, &clock);
    assert(o.toggle == 0 && o.song == 0x133);
    assert(o.state == 1);

    /* Clock above 0x13FFF stops ramping but still feeds the angle. */
    clock = 0x80000;
    int32_t before = o.angle;
    aos_object1b_update(&o, false, &clock);
    assert(clock == 0x80000);
    assert(o.angle == before + 0x80000);
    assert(o.frame == 0x18 || o.frame == 0x1D || o.frame == 0x1E);

    puts("aos_object1b: ok");
    return 0;
}
