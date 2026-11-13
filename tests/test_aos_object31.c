#include "../src/runtime/aos/game/aos_object31.h"
#include <assert.h>
#include <stdio.h>

static void create(AosObject31 *o, int param) {
    bool deleted = false;
    aos_object31_create(o, param, 0, 3, true, &deleted);
    assert(!deleted);
    assert(!o->deleted);
}

int main(void) {
    AosObject31 o;
    bool deleted;

    /* Event bit 2 at + 0x37E deletes at create. */
    aos_object31_create(&o, 0, 4, 0, true, &deleted);
    assert(deleted && o.deleted);

    /* Palette failure deletes for the palette path. */
    aos_object31_create(&o, 0, 0, 0, false, &deleted);
    assert(deleted && o.deleted);

    /* Param tables: waits 60/120/240/360, px 72/72/156/156,
     * py 26/44/26/44. */
    create(&o, 0);
    assert(o.wait == 60 && o.pos_x_px == 72 && o.pos_y_px == 26);
    assert(o.spawn_idx == 3 && o.state == 0);
    create(&o, 3);
    assert(o.wait == 360 && o.pos_x_px == 156 && o.pos_y_px == 44);

    /* State 0 waits for gEwramData+0x42C bit 12. */
    create(&o, 1);
    assert(aos_object31_update(&o, 100, 80, 0));
    assert(o.state == 0);
    assert(aos_object31_update(&o, 100, 80, 0x1000));
    assert(o.state == 1);

    /* Param 1 (non-palette path): wait expiry -> state 2 with
     * accel 0x400, drift physics, offscreen probe -> 3 -> 4
     * parked (no delete). */
    create(&o, 1);
    aos_object31_update(&o, 100, 80, 0x1000);   /* -> state 1 */
    o.wait = 1;
    assert(aos_object31_update(&o, 100, 80, 0));
    assert(o.state == 2 && o.accel_y == 0x400);
    /* Move far offscreen: probe returns leave -> state 3 -> 4. */
    assert(aos_object31_update(&o, -100, 80, 0));
    assert(o.state == 3);
    assert(aos_object31_update(&o, -100, 80, 0));
    assert(o.state == 4 && !o.deleted);
    assert(aos_object31_update(&o, -100, 80, 0));
    assert(o.state == 4);

    /* Param 0 (palette path): expiry -> state 10 launch with
     * child fx, wind-down timer, then spin state 11. */
    create(&o, 0);
    aos_object31_update(&o, 100, 80, 0x1000);
    o.wait = 1;
    assert(aos_object31_update(&o, 100, 80, 0));
    assert(o.state == 10 && o.child_spawn);
    assert(o.spin_rate == 4 && o.accel_x == 0x40000);
    assert(o.timer == 0x3C);
    o.timer = 1;
    assert(aos_object31_update(&o, 100, 80, 0));
    assert(o.state == 11 && o.accel_x == 0);
    /* Spin accumulates + 0x1C (rate 4) into + 0x18, clamped. */
    int32_t before = o.spin_total;
    assert(aos_object31_update(&o, 100, 80, 0));
    assert(o.spin_total > before);
    /* Offscreen probe in state 11 -> 3 -> palette free + delete. */
    assert(aos_object31_update(&o, 400, 80, 0));
    assert(o.state == 3);
    assert(!aos_object31_update(&o, 400, 80, 0));
    assert(o.deleted && o.palette_slot == -1);

    /* Spin accumulator clamps at 0x60. */
    create(&o, 0);
    o.state = 11;
    o.spin_rate = 0x60;
    for (int i = 0; i < 40; i++)
        aos_object31_update(&o, 100, 80, 0);
    assert(o.spin_acc <= 0x60);

    puts("aos_object31: ok");
    return 0;
}
