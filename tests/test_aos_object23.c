#include "../src/runtime/aos/game/aos_object23.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject23 o;
    aos_object23_create(&o);
    assert(o.anim == 1);
    aos_object23_update(&o);
    assert(o.anim == 2);
    aos_object23_update(&o);
    assert(o.anim == 3);
    puts("aos_object23: ok");
    return 0;
}
