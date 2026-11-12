#include "../src/runtime/aos/game/aos_object11.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject11 o;
    aos_object11_create(&o);
    assert(o.helper && o.removed_save);
    assert(o.held_ticks == 0);

    /* The + 0x14 counter only advances while the hold bit lasts. */
    aos_object11_update(&o, false);
    assert(o.held_ticks == 0);
    for (int i = 0; i < 100; i++) aos_object11_update(&o, true);
    assert(o.held_ticks == 100);
    aos_object11_update(&o, false);
    aos_object11_update(&o, true);
    assert(o.held_ticks == 101);

    /* The cap at 0x34BBD. */
    o.held_ticks = 0x34BBC;
    aos_object11_update(&o, true);
    assert(o.held_ticks == 0x34BBD);
    aos_object11_update(&o, true);
    assert(o.held_ticks == 0x34BBD);

    puts("aos_object11: ok");
    return 0;
}
