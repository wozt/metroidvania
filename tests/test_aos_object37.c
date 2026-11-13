#include "../src/runtime/aos/game/aos_object37.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject37 o;
    bool deleted;

    /* Sprite or RNG failure deletes. */
    aos_object37_create(&o, 0, 0, true, true, 0, false, &deleted);
    assert(deleted && o.deleted);
    aos_object37_create(&o, 0, 0, true, false, 0, true, &deleted);
    assert(deleted && o.deleted);

    /* NEAR create: fade 0x3000, no pending placements, no
     * child. */
    aos_object37_create(&o, 0x10000, 0x20000, true, true, 3,
                        true, &deleted);
    assert(!deleted && !o.deleted);
    assert(o.fade == 0x3000 && o.flag_1c == 0);
    assert(o.child_count == 0 && o.place_out == 0);
    assert(o.flag_58 == (3 << 5) && o.anim == 9 && o.sel == 1);
    assert(o.state == 0);

    /* FAR create: flag_1c pending, five placements, one
     * child, fade 0. */
    aos_object37_create(&o, 0, 0, false, true, 0, true, &deleted);
    assert(o.fade == 0 && o.flag_1c == 1);
    assert(o.place_out == 5 && o.child_count == 1);

    /* State 0 decays 0x100/frame clamped at 0; far + flag40
     * -> state 1 + song 0x113. */
    aos_object37_create(&o, 0, 0, true, true, 0, true, &deleted);
    o.fade = 0x250;
    aos_object37_update(&o, false, false);
    assert(o.fade == 0x150 && o.state == 0);
    aos_object37_update(&o, false, true);   /* near: no trigger */
    assert(o.state == 0);
    aos_object37_update(&o, true, false);   /* flag clear: no */
    assert(o.state == 0);
    aos_object37_update(&o, true, true);
    assert(o.state == 1 && o.song == 0x113);

    /* Clamp at 0 keeps decaying harmlessly. */
    o.state = 0; o.fade = 0x50;
    aos_object37_update(&o, false, false);
    assert(o.fade == 0);

    /* State 1 grows to 0x3000 clamp; pending +0x1C runs five
     * sub_08002248 placements once; far + flag clear ->
     * state 0 with +0x1C re-armed, five sub_08002200 and
     * song 0x113. */
    aos_object37_create(&o, 0, 0, false, true, 0, true, &deleted);
    o.state = 1; o.fade = 0x2F00; o.flag_1c = 1;
    aos_object37_update(&o, true, false);
    assert(o.fade == 0x3000 && o.place_in == 5);
    assert(o.state == 0 && o.flag_1c == 1);
    assert(o.place_out == 5 && o.song == 0x113);

    /* State 1 near player: grows but no transition. */
    o.state = 1; o.fade = 0; o.flag_1c = 0;
    aos_object37_update(&o, false, false);
    assert(o.fade == 0x100 && o.state == 1 && o.song == -1);

    /* +0xA gates the machine. */
    o.flag_a = 1;
    aos_object37_update(&o, true, true);
    assert(o.fade == 0x100 && o.state == 1);

    puts("aos_object37: ok");
    return 0;
}
