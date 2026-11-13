#include "../src/runtime/aos/game/aos_object33.h"
#include <assert.h>
#include <stdio.h>

static uint32_t rng_state = 0x12345678;
static uint32_t rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

int main(void) {
    AosObject33 o;
    bool deleted;

    /* Sprite load failure deletes. */
    aos_object33_create(&o, 0, false, &deleted);
    assert(deleted && o.deleted);

    /* Create flags: + 0x5A = 2, + 0x58 keeps bits 1/3/6 and
     * gains 0x20 for param 0 only. */
    aos_object33_create(&o, 0, true, &deleted);
    assert(!deleted && !o.deleted);
    assert(o.flags5a == 2 && (o.byte58 & 0x20));
    assert(o.timer == 0 && o.state == 0);
    aos_object33_create(&o, 1, true, &deleted);
    assert(!(o.byte58 & 0x20));

    /* First tick fires immediately (old + 0xD == 0): cadence
     * reload (RNG & 0xF) + 0x10, burst A always, burst B only
     * for nonzero param. */
    aos_object33_create(&o, 0, true, &deleted);
    aos_object33_update(&o, rng);
    assert(o.burst_a && !o.burst_b);
    assert(o.timer >= 0x10 && o.timer <= 0x1F);

    /* During the cadence window no burst fires. */
    uint8_t cadence = o.timer;
    for (int i = 0; i < cadence - 1; ++i) {
        aos_object33_update(&o, rng);
        assert(!o.burst_a && !o.burst_b);
    }
    /* Old + 0xD == 1 fires again. */
    aos_object33_update(&o, rng);
    assert(o.burst_a);

    /* Param 1 also fires burst B (sub_08059A8C). */
    aos_object33_create(&o, 1, true, &deleted);
    o.timer = 0;
    aos_object33_update(&o, rng);
    assert(o.burst_a && o.burst_b);

    /* Nonzero + 0xB gates the tick. */
    o.state = 1;
    o.timer = 0;
    aos_object33_update(&o, rng);
    assert(!o.burst_a && !o.burst_b && o.timer == 0);

    puts("aos_object33: ok");
    return 0;
}
