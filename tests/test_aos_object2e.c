#include "../src/runtime/aos/game/aos_object2e.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject2E o;
    aos_object2e_create(&o);
    assert(o.triggered == 0 && o.anim == 1 && o.children == 0);
    assert(o.song == -1);

    /* Misses do nothing. */
    aos_object2e_update(&o, false);
    aos_object2e_update(&o, false);
    assert(o.triggered == 0 && o.song == -1);

    /* First hit latches, spawns three children, plays 0x78. */
    aos_object2e_update(&o, true);
    assert(o.triggered == 1 && o.children == 3 && o.song == 0x78);

    /* One-shot: later hits change nothing. */
    aos_object2e_update(&o, true);
    assert(o.children == 3 && o.song == 0x78);

    puts("aos_object2e: ok");
    return 0;
}
