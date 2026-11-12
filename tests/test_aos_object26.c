#include "../src/runtime/aos/game/aos_object26.h"

#include <assert.h>
#include <stdio.h>

static void object26_tests(void) {
    AosObject26 o;
    aos_object26_create(&o);
    /* Create writes + 0x1A = 0x5A, + 0x18 = 0x1E and marks + 0x58
     * bit 0x20. */
    assert(o.counter_a == 0x1E);
    assert(o.counter_b == 0x5A);
    assert(o.done);

    /* A probe miss still sets + 0x58 bit 0x20 but leaves both
     * counters alone. */
    for (int i = 0; i < 10; ++i) aos_object26_update(&o, false);
    assert(o.counter_a == 0x1E);
    assert(o.counter_b == 0x5A);
    assert(o.done);

    /* A probe hit increments both u16 counters together; the
     * Object28 child (counter_b from 0x5A) crosses the orb's
     * signed s16 > 0x77 gate after 0x1E hits, the Object27 child
     * (counter_a from 0x1E) after 0x59. */
    for (int i = 0; i < 0x1E; ++i) aos_object26_update(&o, true);
    assert(o.counter_a == 0x3C);
    assert(o.counter_b == 0x78);
    assert((int16_t)o.counter_b > 0x77);
    assert((int16_t)o.counter_a <= 0x77);
    for (int i = 0; i < 0x3B; ++i) aos_object26_update(&o, true);
    assert(o.counter_a == 0x77);
    aos_object26_update(&o, true);
    assert(o.counter_a == 0x78);
    assert((int16_t)o.counter_a > 0x77);

    /* The counters are plain u16: they wrap at 0xFFFF like the
     * native strh writes. */
    o.counter_a = 0xFFFE;
    aos_object26_update(&o, true);
    assert(o.counter_a == 0xFFFF);
    aos_object26_update(&o, true);
    assert(o.counter_a == 0);
}

int main(void) {
    object26_tests();
    puts("aos_object26: ok");
    return 0;
}
