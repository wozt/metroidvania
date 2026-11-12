#include "../src/runtime/aos/game/aos_object2b.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject2B o;
    aos_object2b_create(&o);
    assert(o.anim == 1);
    aos_object2b_update(&o);
    assert(o.anim == 2);
    aos_object2b_update(&o);
    assert(o.anim == 3);
    puts("aos_object2b: ok");
    return 0;
}
