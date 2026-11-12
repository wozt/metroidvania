#include "../src/runtime/aos/game/aos_object34.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    AosObject34 o;

    /* param0 (+ 0x30) == 0: phase 0, vel +0x100; the + 0x32 u16
     * param becomes the + 0x1C acceleration. */
    aos_object34_create(&o, 0, 0x40, 0x100000, 0x80000,
                        0x200000, 0x180000);
    assert(!o.deleted && o.phase == 0 && o.vel == 0x100 &&
           o.accel == 0x40 && o.offset == 0x4000 &&
           o.child_count == 3);
    assert(o.world_x == 0x300000 && o.world_y == 0x200000);
    assert(o.screen_x == 0x200000 && o.screen_y == 0x180000);

    /* Update 1 runs the state-0 leg: vel 0x100 - 0x40 = 0xC0,
     * offset 0x4000 + 0xC0 = 0x40C0; 0x40C0 << 16 exceeds the
     * 0x3FFF0000 bound so the phase flips to 1. */
    aos_object34_update(&o, 0x100000, 0x80000);
    assert(o.vel == 0xC0 && o.offset == 0x40C0 && o.phase == 1);
    /* Screen copies track world - CURRENT scroll. */
    assert(o.screen_x == 0x200000 && o.screen_y == 0x180000);

    /* Update 2 runs state 1: vel 0xC0 + 0x40 = 0x100, offset
     * 0x41C0; 0x41C0 << 16 exceeds 0x40000000 -> phase 0. */
    aos_object34_update(&o, 0x180000, 0x80000);
    assert(o.vel == 0x100 && o.offset == 0x41C0 && o.phase == 0);
    assert(o.screen_x == 0x180000 && o.screen_y == 0x180000);

    /* param0 (+ 0x30) != 0: phase 1 and vel -256 (0xFF00). */
    aos_object34_create(&o, 5, 0x10, 0, 0, 0, 0);
    assert(o.phase == 1 && o.vel == (int16_t)0xFF00);

    /* State 0 floors vel at -256 even with a huge accel. */
    aos_object34_create(&o, 0, 0x7FFF, 0, 0, 0, 0);
    aos_object34_update(&o, 0, 0);
    assert(o.vel == (int16_t)0xFF00);
    /* State 1 clamps vel at +256 with a huge accel. */
    aos_object34_create(&o, 1, 0x7FFF, 0, 0, 0, 0);
    aos_object34_update(&o, 0, 0);
    assert(o.vel == 0x100);

    /* Long run stays bounded: vel within [-256, 256], phase in
     * {0,1}, offset wraps as u16. */
    for (int i = 0; i < 500; ++i)
        aos_object34_update(&o, 0x12345, 0x54321);
    assert(o.vel >= -256 && o.vel <= 256 &&
           (o.phase == 0 || o.phase == 1));

    puts("aos_object34: ok");
    return 0;
}
