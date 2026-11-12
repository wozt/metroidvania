#include "../src/runtime/aos/game/aos_object15.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject15 o;

    /* Create: event bit picks state 4, else state 1; presets. */
    aos_object15_create(&o, 0, false);
    assert(o.state == 1 && o.counter == 0x200000 && o.angle == 0x6000);
    assert(o.forced == -1 && o.dialogue == -1);
    aos_object15_create(&o, 1, true);
    assert(o.state == 4);

    /* State 1 gates: probe + flag40 + gate_clear all required. */
    aos_object15_create(&o, 0, false);
    aos_object15_update(&o, false, false, true, true, 0, false);
    assert(o.state == 1);
    aos_object15_update(&o, true, false, false, true, 0, false);
    assert(o.state == 1);                    /* flag40 missing */
    aos_object15_update(&o, true, false, true, false, 0, false);
    assert(o.state == 1);                    /* gate busy */
    aos_object15_update(&o, true, false, true, true, 0x20, false);
    assert(o.state == 2 && o.forced == 0);

    /* State 2: lock while counter > 0x90000, direction below. */
    assert(o.counter == 0x200000);
    o.counter = 0xA0000;
    aos_object15_update(&o, false, false, false, false, 0x20, false);
    assert(o.forced == 0);
    o.counter = 0x80000;
    aos_object15_update(&o, false, false, false, false, 0x20, false);
    assert(o.forced == 1 && o.dir_word == 0x20);
    /* angle += 0x40 each state-2 update; counter -= 0x4000. */
    assert(o.angle == (uint16_t)(0x6000 + 0x80));
    assert(o.counter == 0x7C000);

    /* Fade fires exactly at counter == 0x100000. */
    aos_object15_create(&o, 0, false);
    o.state = 2; o.counter = 0x100000;
    aos_object15_update(&o, false, false, false, false, 0, false);
    assert(o.fade);
    aos_object15_update(&o, false, false, false, false, 0, false);
    assert(!o.fade);

    /* Counter expiry enters state 3 with the dialogue record. */
    o.counter = 0x4000;
    aos_object15_update(&o, false, false, false, false, 0x10, false);
    assert(o.state == 3);
    aos_object15_create(&o, 1, false); o.state = 3;
    aos_object15_update(&o, false, false, false, false, 0x10, false);
    assert(o.dialogue == 1);

    /* State 4: x-compare direction and the return probe. */
    aos_object15_create(&o, 0, true);
    aos_object15_update(&o, false, false, false, false, 0, true);
    assert(o.forced == 2 && o.walk_dir == 0x20 && o.state == 4);
    aos_object15_update(&o, false, true, false, false, 0, false);
    assert(o.state == 2);

    puts("aos_object15: ok");
    return 0;
}
