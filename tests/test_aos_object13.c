#include "../src/runtime/aos/game/aos_object13.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject13 o;

    /* Amplitude cap at 0x2300 (then the usual -4 decay). */
    aos_object13_create(&o, 0, 0, 0x3000, 0, 0);
    aos_object13_update(&o, false);
    assert(o.amplitude == 0x2300 - 4);

    /* Phase advances 0x200/frame, amplitude decays by 4. */
    aos_object13_create(&o, 0, 0, 0x100, 0, 0);
    aos_object13_update(&o, false);
    assert(o.phase == 0x200 && o.amplitude == 0xFC);
    /* sine(0x200) is positive small -> sway positive. */
    assert(o.sway > 0);

    /* param1 == 0 disables the event path entirely. */
    aos_object13_create(&o, 0, 0, 0x2200, 0xB000, 0);
    aos_object13_update(&o, false);
    assert(!o.event_fired);

    /* param0 == 0 threshold is 0x2100: amplitude 0x2200 in window
     * fires once, then the bit is set so it cannot refire. */
    aos_object13_create(&o, 0, 5, 0x2200, 0xB000, 0);
    aos_object13_update(&o, false);
    assert(o.event_fired);
    assert(o.phase == (uint16_t)(0xB000 - 0x40 + 0x200));
    assert(o.amplitude == 0x2200 - 0x100 - 4);
    /* event_set now true -> no refire even with a fresh window. */
    o.phase = 0xB000; o.amplitude = 0x2200;
    aos_object13_update(&o, true);
    assert(!o.event_fired);

    /* param0 != 0 uses the lower 0x1200 threshold. */
    aos_object13_create(&o, 1, 7, 0x1300, 0xAC01, 0);
    aos_object13_update(&o, false);
    assert(o.event_fired);

    /* Window edges: phase 0xAC00 (one below) does not fire. */
    aos_object13_create(&o, 1, 7, 0x1300, 0xAC00, 0);
    aos_object13_update(&o, false);
    assert(!o.event_fired);
    /* phase 0xB400 (one above) does not fire. */
    aos_object13_create(&o, 1, 7, 0x1300, 0xB400, 0);
    aos_object13_update(&o, false);
    assert(!o.event_fired);

    /* Below-threshold amplitude does not fire. */
    aos_object13_create(&o, 0, 5, 0x2100, 0xB000, 0);
    aos_object13_update(&o, false);
    assert(!o.event_fired);   /* needs strictly greater */

    /* state 0xFF: the three-tick delete countdown. */
    aos_object13_create(&o, 0, 0, 0, 0, 0xFF);
    aos_object13_update(&o, false);
    assert(!o.deleted && o.death == 1);
    aos_object13_update(&o, false);
    assert(!o.deleted && o.death == 2);
    aos_object13_update(&o, false);
    assert(o.deleted);

    /* Zero amplitude resets phase and sway. */
    aos_object13_create(&o, 0, 0, 0, 0x1234, 0);
    aos_object13_update(&o, false);
    assert(o.amplitude == 0 && o.phase == 0 && o.sway == 0);

    puts("aos_object13: ok");
    return 0;
}
