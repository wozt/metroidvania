#include "../src/runtime/aos/game/aos_object24.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject24 o;

    /* Create: phase = rng & 0xF, variant = rng % 3. */
    aos_object24_create(&o, 0x1F, 7);
    assert(o.phase == 0xF && o.variant == 1);
    aos_object24_create(&o, 0x20, 8);
    assert(o.phase == 0 && o.variant == 2);
    assert(o.tick == 0 && o.anim == 0 && !o.hidden);

    /* Update: roll <= 0xDF advances the animation. */
    aos_object24_update(&o, 0xDF);
    assert(o.advanced && o.anim == 1);
    aos_object24_update(&o, 0xE0);
    assert(!o.advanced && o.anim == 1);
    aos_object24_update(&o, 0xFF);
    assert(!o.advanced);

    /* + 0x58 bit 0x20 follows the OLD + 0xD parity: ticks 0,1,2
     * give hidden false, true, false. */
    AosObject24 p;
    aos_object24_create(&p, 0, 0);
    aos_object24_update(&p, 0);         /* old tick 0 -> clear */
    assert(!p.hidden && p.tick == 1);
    aos_object24_update(&p, 0);         /* old tick 1 -> set */
    assert(p.hidden && p.tick == 2);
    aos_object24_update(&p, 0);         /* old tick 2 -> clear */
    assert(!p.hidden && p.tick == 3);

    puts("aos_object24: ok");
    return 0;
}
