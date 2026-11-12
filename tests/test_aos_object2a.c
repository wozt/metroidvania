#include "../src/runtime/aos/game/aos_object2a.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject2A o;

    /* param1 outside 0-3 fails the record load natively (create
     * deletes the entity); the module reports it invalid. */
    aos_object2a_create(&o, 100, 7, 0x500000, 0x300000, 0, 0);
    assert(!o.valid);
    aos_object2a_update(&o, 0, 0, false, false);
    assert(o.out_x == 0x500000 && o.out_y == 0x300000);

    /* Variant 0: spawn screen x = 0x50.0 px, target A = 0x30.0 px
     * (param0 0x30 << 16) sits LEFT of target B -> dir = 1 and the
     * state-0 leg accelerates negative. */
    aos_object2a_create(&o, 0x30, 0, 0x500000, 0x300000, 0x100000, 0);
    assert(o.valid && o.variant == 0 && o.preset == 0 &&
           o.pal_bank == 6 && o.wait == 2);
    assert(o.target_a == 0x300000 && o.target_b == 0x600000);
    assert(o.dir == 1 && o.state == 0 && o.out_x == 0x500000);
    assert(o.hitbox_word == 0x0C2000F0);

    /* First update integrates the zero velocity (x stays) then
     * ramps -0x800; the second moves by it. */
    aos_object2a_update(&o, 0x100000, 0, false, false);
    assert(o.x_acc == 0x600000 && o.vel == (int32_t)0xFFFFF800);
    aos_object2a_update(&o, 0x100000, 0, false, false);
    assert(o.x_acc == 0x5FF800 && o.vel == (int32_t)0xFFFFF000);

    /* The epilogue reports the world position: accumulator minus
     * the CURRENT scroll — moving the camera shifts out_x even
     * with no approach progress (screen-locked sprite). */
    aos_object2a_update(&o, 0x180000, 0, false, false);
    assert(o.out_x == o.x_acc - 0x180000);
    assert(o.out_y == o.y_acc); /* scroll_y 0: y_acc = spawn_y */

    /* Run until the sprite reaches target A: state 1 waits two
     * frames (timer 2) then state 2 with dir toggled. */
    int guard = 0;
    while (o.state == 0 && guard++ < 100000)
        aos_object2a_update(&o, 0x100000, 0, false, false);
    assert(o.state == 1 && o.timer == 2 && o.vel == 0);
    aos_object2a_update(&o, 0x100000, 0, false, false);
    assert(o.state == 1 && o.timer == 1);
    aos_object2a_update(&o, 0x100000, 0, false, false);
    assert(o.state == 2 && o.dir == 0 && o.timer == 0);

    /* State 2 approaches target B (right) with positive ramp;
     * arrival enters state 3 (dir still 0 — the toggle fires on
     * the wait exit) and state 3's expiry returns to state 0
     * with dir back to 1. */
    guard = 0;
    while (o.state == 2 && guard++ < 100000)
        aos_object2a_update(&o, 0x100000, 0, false, false);
    assert(o.state == 3 && o.dir == 0 && o.timer == 2);
    aos_object2a_update(&o, 0x100000, 0, false, false);
    aos_object2a_update(&o, 0x100000, 0, false, false);
    assert(o.state == 0 && o.dir == 1);

    /* Focus/attack inputs only count. */
    unsigned f0 = o.focus_calls, a0 = o.attacks;
    aos_object2a_update(&o, 0x100000, 0, true, true);
    assert(o.focus_calls == f0 + 1 && o.attacks == a0 + 1);

    /* Variant 1 keeps preset argument 2; variant 2 bank 0. */
    aos_object2a_create(&o, 0x30, 1, 0, 0, 0, 0);
    assert(o.valid && o.variant == 1 && o.preset == 2 && o.pal_bank == 6);
    aos_object2a_create(&o, 0x30, 2, 0, 0, 0, 0);
    assert(o.valid && o.variant == 2 && o.preset == 0 && o.pal_bank == 0);

    puts("aos_object2a: ok");
    return 0;
}
