#include "../src/runtime/aos/game/aos_object35.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject35 o;
    bool deleted;

    aos_object35_create(&o, 0, 0, 0x10000, 0x20000, false,
                        false, false, &deleted);
    assert(deleted && o.deleted);

    /* param0 == 0 actor: collision variant, sel 2, state 0,
     * one inert child. */
    aos_object35_create(&o, 0, 0, 0x10000, 0x20000, false,
                        false, true, &deleted);
    assert(!deleted && !o.deleted);
    assert(o.sel == 2 && o.state == 0 && o.child_count == 1);
    assert(o.anim == 0x12 && o.flags59 == 2);
    assert(o.world_x == o.pos_x && o.world_y == o.pos_y);

    /* Preset global bit lifts +0x44 by 0x60000 and starts in
     * state 2. */
    aos_object35_create(&o, 0, 0, 0, 0, false, true, true,
                        &deleted);
    assert(o.state == 2);
    assert(o.pos_y == 0x60000);

    /* param0 != 0 effect: no child, sel 1, starts state 0xA. */
    aos_object35_create(&o, 1, 0, 0, 0, false, false, true,
                        &deleted);
    assert(o.child_count == 0 && o.sel == 1 && o.state == 0xA);

    /* State 0 waits for linked_self; +0x18 counts to 8 ->
     * state 1 + song 0x133. */
    aos_object35_create(&o, 0, 0, 0, 0, false, false, true,
                        &deleted);
    aos_object35_update(&o, false, false);
    assert(o.state == 0 && o.counter == 0);
    aos_object35_update(&o, true, false);
    assert(o.counter == 1 && o.state == 0);
    /* A broken link resets the counter. */
    aos_object35_update(&o, false, false);
    assert(o.counter == 0);
    for (int i = 0; i < 8; ++i) aos_object35_update(&o, true, false);
    assert(o.state == 1 && o.song == 0x133);

    /* State 1: first frame integrates +0 (vel armed to
     * 0x10000 after), then descends 1 px/frame; +0xD past 4
     * -> state 2 after 6 updates. */
    o.state = 1; o.vel_y = 0; o.timer = 0; o.pos_y = 0;
    int32_t y0 = o.pos_y;
    for (int i = 0; i < 6; ++i) aos_object35_update(&o, true, false);
    assert(o.state == 2 && o.flag_1c == 0);
    assert(o.pos_y == y0 + 5 * 0x10000);

    /* param0 == 0 requests deactivate every frame. */
    assert(o.deactivate);

    /* State 10 waits for the global bit -> 11, song 0x113,
     * vel 0x8000. */
    aos_object35_create(&o, 1, 0, 0, 0, false, false, true,
                        &deleted);
    aos_object35_update(&o, true, false);
    assert(o.state == 0xA && o.song == -1);
    aos_object35_update(&o, true, true);
    assert(o.state == 11 && o.song == 0x113 && o.vel_y == 0x8000);

    /* State 11: flash window, particle every 8th frame,
     * +0xD past 0x5F -> 12 on the 97th update. */
    int particles = 0;
    for (int i = 0; i < 97; ++i) {
        aos_object35_update(&o, true, true);
        particles += (int)o.particle_count;
        assert(o.flash);
    }
    assert(o.state == 12);
    assert(particles == 13);                /* old 0,8,...,96 */

    /* State 12: flash cleared. */
    aos_object35_update(&o, true, true);
    assert(!o.flash && o.state == 12);

    puts("aos_object35: ok");
    return 0;
}
