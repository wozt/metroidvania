#include "../src/runtime/aos/game/aos_object2c.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject2C o;

    aos_object2c_create(&o);
    assert(!o.deleted && o.child_count == 4);
    /* Per-index create constants: y offsets, x steps, layers. */
    const uint16_t y_exp[4] = {0x1E, 0x3E, 0x56, 0x66};
    const uint16_t x_exp[4] = {0x4000, 0x3000, 0x2000, 0x1000};
    for (int i = 0; i < 4; ++i) {
        assert(o.y_off[i] == y_exp[i]);
        assert(o.x_step[i] == x_exp[i]);
        assert(o.layer[i] == (uint8_t)i);
    }

    /* The parent update is a bare frame: state must not change. */
    aos_object2c_update(&o);
    assert(o.child_count == 4 && !o.deleted);
    for (int i = 0; i < 100; ++i) aos_object2c_update(&o);
    assert(o.child_count == 4);

    puts("aos_object2c: ok");
    return 0;
}
