#include "../src/runtime/aos/game/aos_object28.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject28 o;
    aos_object28_create(&o, 0x11);
    assert(o.phase == 0 && o.variant == 0x11 && o.done);

    aos_object28_update(&o, false);
    assert(o.phase == 0);
    aos_object28_update(&o, true);
    assert(o.phase == 1 && o.spin == 0xC000);

    for (int i = 0; i < 64 && o.phase == 1; ++i)
        aos_object28_update(&o, false);
    assert(o.phase == 2 && o.spin == 0x40000 && o.timer == 0x1E);

    o.timer = 1;
    aos_object28_update(&o, false);
    assert(o.phase == 3 && o.attack_armed);
    while (o.phase == 3 && o.spin > 0xC000)
        aos_object28_update(&o, false);
    aos_object28_update(&o, false);
    assert(o.phase == 0 && o.done);

    puts("aos_object28: ok");
    return 0;
}
