#include "../src/runtime/aos/game/aos_object1a.h"
#include "../src/runtime/aos/game/aos_object1e.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    /* Object1ACreate/Object1AUpdate and
     * Object1ECreate/Object1EUpdate are bare bx lr in the ROM —
     * the calls must simply return without touching state. */
    AosObject1A a;
    aos_object1a_create(&a);
    aos_object1a_update(&a);

    AosObject1E e;
    aos_object1e_create(&e);
    aos_object1e_update(&e);

    puts("aos_object1a_1e: ok");
    return 0;
}
