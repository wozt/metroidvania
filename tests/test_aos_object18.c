#include "../src/runtime/aos/game/aos_object18.h"
#include <assert.h>
#include <stdio.h>

static uint32_t rng_state = 0x12345678;
static uint32_t test_random(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

int main(void) {
    AosObject18 o;

    /* Create: param0 0 is unarmed, nonzero takes the state-2 path. */
    aos_object18_create(&o, 0);
    assert(o.state == 0 && !o.armed && !o.finished);
    aos_object18_create(&o, 3);
    assert(o.armed);

    /* Probe miss: + 0x58 bit 0x20, nothing else happens. */
    aos_object18_create(&o, 0);
    aos_object18_update(&o, false, true, false, 100, 100, test_random);
    assert((o.flags58 & 0x20) && o.state == 0 && o.song == -1);

    /* Near but no attack gate: state 0 clears + 0x58 bits 0/5
     * and waits. */
    o.flags58 = 0x21;
    aos_object18_update(&o, true, false, false, 100, 100, test_random);
    assert(o.state == 0 && !(o.flags58 & 0x21));

    /* Attack with the event flag clear: eight money drops of
     * variants i / 2 scattered around the room position, the
     * ring, song 0xA2, timer 0x28 and state 1. */
    rng_state = 0x12345678;
    aos_object18_create(&o, 0);
    aos_object18_update(&o, true, true, false, 100, 100, test_random);
    assert(o.state == 1 && o.timer == 0x28);
    assert(o.song == 0xA2 && o.ring_fx == 4);
    assert((o.flags58 & 0x20) && o.drop_count == 8 && !o.heart);
    for (int i = 0; i < 8; ++i) {
        assert(o.drops[i].variant == (uint8_t)(i / 2));
        assert(o.drops[i].drop_x >= 100 - 16 && o.drops[i].drop_x <= 100 + 15);
        assert(o.drops[i].drop_y >= 100 - 8 - 15 && o.drops[i].drop_y <= 100 - 8);
    }

    /* Event flag set: one big heart instead of the scatter. */
    aos_object18_create(&o, 0);
    aos_object18_update(&o, true, true, true, 100, 100, test_random);
    assert(o.heart && o.drop_count == 0 && o.state == 1);

    /* Armed param0: straight to state 2, no drops. */
    aos_object18_create(&o, 1);
    aos_object18_update(&o, true, true, false, 100, 100, test_random);
    assert(o.state == 2 && o.timer == 0x20 && o.drop_count == 0);
    assert(o.ring_fx == 4 && o.song == 0xA2);

    /* State 1 gore: timer 0x28, 0x24 and 0x20 qualify
     * (> 0x1E and & 3 == 0); 0x1C does not. */
    aos_object18_create(&o, 0);
    o.state = 1; o.timer = 0x28;
    int gore = 0;
    for (int t = 0x28; t > 0; --t) {
        o.timer = (uint8_t)t;
        aos_object18_update(&o, true, false, false, 0, 0, test_random);
        gore += o.gore_fx;
    }
    assert(gore == 3);

    /* State 2 children: spawn while timer & 3 == 0, counting
     * down to the 0xFF wrap that sets + 0x59 bit 3. */
    aos_object18_create(&o, 1);
    o.state = 2; o.timer = 0x20;
    int kids = 0, frames = 0;
    while (!o.finished && frames < 0x30) {
        aos_object18_update(&o, true, false, false, 0, 0, test_random);
        kids += o.child_fx;
        frames++;
    }
    assert(o.finished && kids == 9);   /* 0x20..0x00 step 4 */

    /* State 0 has no countdown: attack-less frames leave the
     * timer untouched. */
    aos_object18_create(&o, 0);
    o.timer = 0x10;
    aos_object18_update(&o, true, false, false, 0, 0, test_random);
    assert(o.timer == 0x10 && o.state == 0);

    puts("aos_object18: ok");
    return 0;
}
