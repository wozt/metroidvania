#include "../src/runtime/aos/game/aos_object21.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject21 o;
    bool deleted;

    aos_object21_create(&o, 0, 0, 7, &deleted);
    assert(!deleted);
    assert(o.state == 0 && o.vel == 0 && o.accel == 0x2800);
    assert(o.pos_x_px == 0x40 && o.pos_y_px == 0x18);
    assert(o.pos_y == 0x18 << 16);

    /* Room offsets fold into the placement. */
    aos_object21_create(&o, 0x10, 0x20, 0, &deleted);
    assert(o.pos_x_px == 0x30 && o.pos_y_px == (int16_t)(0x18 - 0x20));

    /* State 0 physics: vel += 0x2800, pos += vel each frame. */
    aos_object21_create(&o, 0, 0, 0, &deleted);
    aos_object21_update(&o, 0, 0);
    assert(o.vel == 0x2800 && o.pos_y == (0x18 << 16) + 0x2800);
    aos_object21_update(&o, 0, 0);
    assert(o.vel == 0x5000 && o.pos_y == (0x18 << 16) + 0x2800 + 0x5000);

    /* Velocity clamps at +/- 0x70000. */
    o.vel = 0x70000;
    aos_object21_update(&o, 0, 0);
    assert(o.vel == 0x70000);

    /* Drive pos_y_px to the upper bound 0x7F: state 0 -> 1,
     * vel -0x14000, song 0x7F. */
    aos_object21_create(&o, 0, 0, 0, &deleted);
    o.pos_y = (0x7E << 16);
    aos_object21_update(&o, 0, 0);
    /* physics: vel 0x2800, pos_y += -> 0x7E<<16 + 0x2800 ->
     * pos_y_px still 0x7E < 0x7F: no transition yet. */
    assert(o.state == 0);
    o.pos_y = (0x7F << 16) - 0x1000;
    o.vel = 0x4000;
    aos_object21_update(&o, 0, 0);
    /* pos_y += 0x4000 -> crosses 0x7F<<16 -> px 0x7F >= upper. */
    assert(o.state == 1 && o.vel == -0x14000 && o.song == 0x7F);
    assert(o.pos_y_px == 0x7F);

    /* Anim request in state 0 needs flag6d == 0 and
     * pos_y_px >= 0x24. */
    aos_object21_create(&o, 0, 0, 0, &deleted);
    o.pos_y = (0x30 << 16); o.vel = 0;
    aos_object21_update(&o, 0, 0);
    assert(o.anim_seq == 3);
    aos_object21_update(&o, 0, 3);
    assert(o.anim_seq == -1);   /* flag6d != 0 suppresses it */

    /* State 1: physics continues; reaching the upper bound
     * again zeroes vel into state 2. */
    aos_object21_create(&o, 0, 0, 0, &deleted);
    o.state = 1; o.vel = -0x10000; o.pos_y = (0x80 << 16);
    /* vel += accel -> -0xD800; pos_y decreases, px drops
     * below upper -> keeps falling state. Drive it back:
     * set pos so the step lands above upper. */
    o.pos_y = (0x7F << 16) + 0x8000; o.vel = 0x10000;
    aos_object21_update(&o, 0, 0);
    /* vel = 0x12800, pos_y += -> px 0x80 >= 0x7F -> state 2. */
    assert(o.state == 2 && o.vel == 0 && o.timer == 0);

    /* State 2 timeout: old + 0xD > 0x3B -> state 3. */
    o.timer = 0x3B;
    aos_object21_update(&o, 0, 0);
    assert(o.state == 2 && o.timer == 0x3C);
    aos_object21_update(&o, 0, 0);
    assert(o.state == 3 && o.timer == 0);

    /* State 3: vel pinned -0x10000, pos_y integrates down
     * (upward); bottom bound 0x18 snaps into state 4. */
    o.pos_y = (0x19 << 16);
    aos_object21_update(&o, 0, 0);
    /* pos_y += -0x10000 -> 0x18<<16 - 0x10000 -> px 0x17 <= 0x18
     * -> snap, state 4. */
    assert(o.state == 4 && o.vel == -0x10000);
    assert(o.pos_y_px == 0x18);

    /* State 4 timeout resets the cycle. */
    o.timer = 0x3C;
    aos_object21_update(&o, 0, 0);
    assert(o.state == 0 && o.vel == 0 && o.timer == 0);

    /* State 3 anim request needs flag6d == 3 and
     * pos_y_px <= 0x24. */
    aos_object21_create(&o, 0, 0, 0, &deleted);
    o.state = 3; o.pos_y = (0x20 << 16); o.vel = 0;
    /* vel pinned -0x10000 -> pos_y 0x20<<16 - 0x10000 -> px 0x1F */
    aos_object21_update(&o, 0, 3);
    assert(o.anim_seq == 0);
    aos_object21_update(&o, 0, 0);
    assert(o.anim_seq == -1);

    puts("aos_object21: ok");
    return 0;
}
