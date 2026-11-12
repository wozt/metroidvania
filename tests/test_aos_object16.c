#include "../src/runtime/aos/game/aos_object16.h"
#include "../src/runtime/aos/math/aos_math.h"
#include <assert.h>
#include <stdio.h>

static int32_t sine0(uint32_t p) { (void)p; return 0; }

int main(void) {
    AosObject16 o;

    /* Create presets. */
    aos_object16_create(&o, 3);
    assert(o.param == 3 && o.state == 0 && o.counter == 0xC0);
    assert(o.zoom == 0xF0000 && o.scroll == 0x40 && o.scroll_acc == 0);
    assert(o.forced == -1 && o.song == -1 && o.particles == 0);

    /* State 0 waits for the + 0x42C bit-0 flag. */
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.state == 0);
    aos_object16_update(&o, true, false, false, sine0);
    assert(o.state == 1 && o.forced == 0);

    /* State 1: x-compare direction, probe enters state 2. */
    aos_object16_update(&o, false, false, true, sine0);
    assert(o.forced == 1 && o.walk_dir == 0x20);
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.walk_dir == 0x10);
    aos_object16_update(&o, false, true, false, sine0);
    assert(o.state == 2);

    /* State 2 locks input, plays 0x157, enters state 3. */
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.state == 3 && o.song == 0x157 && o.forced == 0);

    /* State 3: zoom from the sine, scroll steps, song+fade at
     * counter == 0x40 post-decrement. */
    o.counter = 0x42;
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.counter == 0x41 && o.scroll == 0x42);
    assert(o.zoom == 0x1000);           /* sine 0 -> 0x1000 - 0 */
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.counter == 0x40 && o.song == 0x1AE && o.fade);
    assert(!o.fade || o.state == 3);

    /* Counter expiry enters state 4 with the dialogue record. */
    o.counter = 1;
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.counter == 0 && o.state == 3);
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.state == 4);
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.dialogue && o.forced == 0);

    /* Tail: scroll accumulator grows every frame and particles
     * spawn on + 0xD parity. */
    aos_object16_create(&o, 0);
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.scroll_acc == 0x40 && o.particles == 1);
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.scroll_acc == 0x80 && o.particles == 0);
    aos_object16_update(&o, false, false, false, sine0);
    assert(o.particles == 1);

    /* Native sine drives a non-trivial zoom. */
    aos_object16_create(&o, 0);
    o.state = 3; o.counter = 0x100;
    aos_object16_update(&o, false, false, false, aos_sine);
    assert(o.zoom != 0xF0000);

    puts("aos_object16: ok");
    return 0;
}
