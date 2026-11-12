#include "../src/runtime/aos/game/aos_object30.h"
#include <assert.h>
#include <stdio.h>

static uint32_t rng_state = 0x55AA33CC;
static uint32_t test_random(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

int main(void) {
    AosObject30 o;
    bool deleted;

    aos_object30_create(&o, &deleted);
    assert(!deleted && o.gate == 0 && o.timer == 0);

    /* Global bit 0x1000 clear: the gate never latches and the
     * timer stays put. */
    for (int i = 0; i < 300; ++i)
        aos_object30_update(&o, 0x0000EFF0u, test_random);
    assert(o.gate == 0 && o.spawn_fx == 0);

    /* Bit 0x1000 latches + 0xB to 1 and the frame ends — the
     * countdown starts on the NEXT update, where old timer 0
     * fires the child and reloads to 0x20..0x9F. */
    aos_object30_update(&o, 0x00001000u, test_random);
    assert(o.gate == 1 && o.spawn_fx == 0);
    aos_object30_update(&o, 0, test_random);
    assert(o.spawn_fx == 1);
    assert(o.timer >= 0x20 && o.timer <= 0x9F);

    /* With the bit clear again the emitter keeps running
     * (the latch is permanent). */
    int spawns = 0;
    for (int i = 0; i < 0x200; ++i) {
        aos_object30_update(&o, 0, test_random);
        spawns += o.spawn_fx;
    }
    assert(spawns >= 4 && spawns <= 20);   /* ~0x20..0x9F period */

    /* The reload always lands in 0x20..0x9F. */
    for (int i = 0; i < 50; ++i) {
        aos_object30_update(&o, 0, test_random);
        if (o.spawn_fx)
            assert(o.timer >= 0x20 && o.timer <= 0x9F);
    }

    /* + 0xB values other than 0/1 exit without touching the
     * timer (native default branch). */
    o.gate = 2; o.timer = 5;
    aos_object30_update(&o, 0x1000, test_random);
    assert(o.timer == 5 && o.spawn_fx == 0);

    puts("aos_object30: ok");
    return 0;
}
