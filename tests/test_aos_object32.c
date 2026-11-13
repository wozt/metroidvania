#include "../src/runtime/aos/game/aos_object32.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject32 o;
    bool deleted;

    aos_object32_create(&o, 0, 1, 100, 200, 0x10000, 0x30000,
                        false, &deleted);
    assert(deleted && o.deleted);

    /* param1 == 1 -> anim_sel 7, param1 == 0 -> 0 and the
     * two extra finale children. */
    aos_object32_create(&o, 0, 1, 100, 200, 0x10000, 0x30000,
                        true, &deleted);
    assert(!deleted && !o.deleted);
    assert(o.anim_sel == 7 && o.state == 0);
    aos_object32_create(&o, 0, 0, 100, 200, 0x10000, 0x30000,
                        true, &deleted);
    assert(o.anim_sel == 0);

    /* State 0 waits for linked_self -> state 1 + song 0x153. */
    aos_object32_create(&o, 0, 1, 0, 0, 0, 0, true, &deleted);
    aos_object32_update(&o, false, false);
    assert(o.state == 0 && o.song == -1);
    aos_object32_update(&o, true, false);
    assert(o.state == 1 && o.song == 0x153 && o.timer == 0);

    /* State 1 fires when OLD + 0xD > 9: 11 increments. */
    for (int i = 0; i < 11; ++i)
        aos_object32_update(&o, true, false);
    assert(o.state == 2 && o.anim_sel == 8 && o.timer == 0);

    /* State 2 fires when OLD > 0x13 = 19: 20 increments,
     * song 0xA8. */
    for (int i = 0; i < 21; ++i)
        aos_object32_update(&o, true, false);
    assert(o.state == 3 && o.song == 0xA8 && o.anim_sel == 9);

    /* State 3 fires when OLD > 9: 11 increments, song 0x150. */
    for (int i = 0; i < 11; ++i)
        aos_object32_update(&o, true, false);
    assert(o.state == 4 && o.song == 0x150 && o.anim_sel == 10);

    /* State 4 fires when OLD > 5: 7 increments -> done mark +
     * 3 children (param1 == 1). */
    for (int i = 0; i < 7; ++i)
        aos_object32_update(&o, true, false);
    assert(o.flags59 & 8);
    assert(o.child_count == 3);

    /* param1 == 0 -> 5 children. */
    aos_object32_create(&o, 0, 0, 0, 0, 0, 0, true, &deleted);
    o.state = 4;
    o.timer = 6;
    aos_object32_update(&o, true, false);
    assert(o.child_count == 5);

    /* Proximity player -> deactivate. */
    aos_object32_update(&o, true, true);
    assert(o.deactivate);

    /* World mirror is the static origin. */
    assert(o.world_x == o.pos_x && o.world_y == o.pos_y);

    puts("aos_object32: ok");
    return 0;
}
