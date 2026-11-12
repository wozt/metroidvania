#include "../src/runtime/aos/game/aos_object12.h"
#include <assert.h>
#include <stdio.h>

static uint32_t rng_state = 0x12345678;
static uint32_t test_rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

int main(void) {
    AosObject12 o;
    bool deleted;

    /* Create maps param0 onto the four records. */
    aos_object12_create(&o, 0, 0, &deleted);
    assert(!deleted && o.variant == 0 && o.flags == 0x00);
    assert(o.animated);
    aos_object12_create(&o, 1, 0, &deleted);
    assert(o.variant == 1 && o.flags == 0x08);
    aos_object12_create(&o, 2, 1, &deleted);
    assert(o.variant == 2 && o.flags == 0x04 && !o.animated);
    aos_object12_create(&o, 99, 0, &deleted);
    assert(o.variant == 3 && o.flags == 0x06);

    /* Record 1: the bit-3 probe gates everything. */
    aos_object12_update(&o, false, false, false, test_rng);
    /* variant is 3 here (flags&4 -> drift), so nothing observable;
     * rebuild record 1 to test the probe. */
    aos_object12_create(&o, 1, 0, &deleted);
    o.seq = 0x55; o.palette_step = 7;
    aos_object12_update(&o, false, true, true, test_rng);
    assert(o.seq == 0x55 && o.palette_step == 7);  /* full skip */
    aos_object12_update(&o, true, false, false, test_rng);
    /* near: record 1 has no callback -> only the anim step (runtime). */
    assert(o.seq == 0x55);

    /* Record 0: the five-state sequence machine. */
    aos_object12_create(&o, 0, 0, &deleted);
    rng_state = 1;
    aos_object12_update(&o, true, false, false, test_rng);
    assert(o.state == 1 && o.seq == 0);
    assert(o.timer >= 8 && o.timer <= 0x87);
    /* Drain state 1. */
    while (o.state == 1) aos_object12_update(&o, true, false, false, test_rng);
    assert(o.state == 2);
    /* State 2 waits for the hit flag. */
    aos_object12_update(&o, true, false, false, test_rng);
    assert(o.state == 2);
    aos_object12_update(&o, true, true, false, test_rng);
    assert(o.state == 3 && o.timer >= 0x10 && o.timer <= 0x8F);
    while (o.state == 3) aos_object12_update(&o, true, false, false, test_rng);
    assert(o.state == 4 && o.seq == 2);
    /* State 4 re-arms on a hit. */
    aos_object12_update(&o, true, true, false, test_rng);
    assert(o.state == 0);
    aos_object12_update(&o, true, false, false, test_rng);
    assert(o.state == 1 && o.seq == 0);

    /* Records 2/3: the drift callback arms once and cycles the
     * palette step only while param1 == 1. */
    aos_object12_create(&o, 2, 0, &deleted);
    aos_object12_update(&o, true, false, false, test_rng);
    assert(o.state == 1);
    assert(o.palette_step == 0);   /* param1 != 1: no cycling */
    aos_object12_update(&o, true, false, true, test_rng);
    assert(o.palette_step == 1);
    aos_object12_update(&o, true, false, true, test_rng);
    assert(o.palette_step == 2);

    puts("aos_object12: ok");
    return 0;
}
