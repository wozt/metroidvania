#include "../src/runtime/aos/game/aos_object29.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject29 o;
    bool deleted;

    aos_object29_create(&o, 0, 0, 100, 200, 0x10000, 0x30000,
                        false, &deleted);
    assert(deleted && o.deleted);

    /* param0 = 4 -> threshold 4<<16 = 0x40000; bottom =
     * 0x30000 + 200 = 0x300C8 < threshold -> dir 0. */
    aos_object29_create(&o, 4, 1, 100, 200, 0x10000, 0x30000,
                        true, &deleted);
    assert(!deleted && !o.deleted);
    assert(o.threshold == 0x40000);
    assert(o.bottom == 0x30000 + 200);
    assert(o.dir == 0 && o.state == 0);
    assert(o.pos_y == 0x30000 + 200);

    /* State 0, dir 0, pos (0x300C8) < target (0x40000):
     * dist = 0xFF38 -> px = 0 -> near step = 0x1000.
     * vel set to +0x1000, pos advances. */
    aos_object29_update(&o, 100, 200);
    assert(o.vel_y == 0x1000);
    assert(o.pos_y == 0x300C8 + 0x1000);

    /* Dwell states hold position and toggle dir. */
    o.pos_y = o.threshold;              /* force arrival next */
    o.vel_y = 0;
    /* pos >= target with dir 0 -> arrive state 1, vel 0. */
    aos_object29_update(&o, 100, 200);
    assert(o.state == 1 && o.vel_y == 0 && o.timer == 2);
    aos_object29_update(&o, 100, 200);  /* 2->1 stay */
    assert(o.state == 1);
    aos_object29_update(&o, 100, 200);  /* 1->0 fire -> state 2,
                                         * dir toggled */
    assert(o.state == 2 && o.dir == 1);

    /* State 2 target = bottom (0x300C8), dir 1 requires
     * pos > target to continue; pos == threshold > bottom so
     * the approach runs negative. dist = 0xFF38 -> px 0 ->
     * vel = -0x1000. */
    aos_object29_update(&o, 100, 200);
    assert(o.vel_y == -0x1000);
    assert(o.pos_y == o.threshold - 0x1000);

    /* Drive to bottom arrival -> state 3 -> dwell -> state 0
     * with dir toggled back to 0. */
    o.pos_y = o.bottom + 0x10;
    o.vel_y = 0;
    aos_object29_update(&o, 100, 200);
    /* dir 1, pos > bottom -> not arrived; dist small ->
     * vel = -step, pos decreases toward bottom. */
    assert(o.state == 2);
    o.pos_y = o.bottom;                 /* pos <= target & dir1
                                         * -> arrive */
    aos_object29_update(&o, 100, 200);
    assert(o.state == 3 && o.timer == 2);
    aos_object29_update(&o, 100, 200);
    aos_object29_update(&o, 100, 200);
    assert(o.state == 0 && o.dir == 0);

    /* Screen/world mirrors track the moving origin. */
    int32_t before = o.pos_y;
    aos_object29_update(&o, 100, 200);
    assert(o.scr_y == o.pos_y - 200);
    assert(o.world_y == o.scr_y + 200);
    (void)before;

    puts("aos_object29: ok");
    return 0;
}
