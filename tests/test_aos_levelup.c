/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_levelup.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

/* sub_080009E4 stands in with the same generated table formula the
 * enemy sine uses: floor(sin(i * pi / 2048) * 65536) lookups through
 * the quadrant logic, here approximated by libm for the envelope
 * checks (the module only consumes its Q16.16 result). */
static int32_t test_sine(uint32_t angle) {
    double turns = (double)(angle & 0xFFFFu) / 65536.0;
    double s = sin(turns * 2.0 * 3.14159265358979323846);
    return (int32_t)floor(s * 65536.0);
}

int main(void) {
    /* State machine: start, 61 visible envelopes, freeze window
     * [2, 0x3C], 20 sparkles on odd counters <= 0x27. */
    AosLevelUp d;
    aos_levelup_start(&d);
    assert(d.active && d.frame == 0 && !aos_levelup_frozen(&d));
    int sparkles = 0;
    int ticks = 0;
    while (d.active) {
        uint8_t env = d.frame;          /* envelope of this pass */
        if (aos_levelup_tick(&d)) ++sparkles;
        assert(d.env_frame == env);     /* snapshot of the pre-tick frame */
        assert(d.frame <= AOS_LEVELUP_DURATION + 1);
        ++ticks;
        assert(ticks < 100);
    }
    assert(ticks == AOS_LEVELUP_DURATION + 1);   /* passes over 0..0x3C */
    assert(sparkles == 20);                            /* odd 1..0x27 */
    assert(!aos_levelup_frozen(&d));

    /* Freeze window: counters 2 through 0x3C inclusive. */
    aos_levelup_start(&d);
    assert(!aos_levelup_frozen(&d));
    aos_levelup_tick(&d);                              /* frame 1 */
    assert(!aos_levelup_frozen(&d));
    aos_levelup_tick(&d);                              /* frame 2 */
    assert(aos_levelup_frozen(&d));
    while (d.active && d.frame < AOS_LEVELUP_DURATION) aos_levelup_tick(&d);
    assert(d.frame == AOS_LEVELUP_DURATION && aos_levelup_frozen(&d));
    aos_levelup_tick(&d);                              /* frame 0x3D: end */
    assert(!d.active && !aos_levelup_frozen(&d));

    /* Envelope: scale is 0x2000 through frame 7, then
     * 0x10000 - sine(frame * 0x800), unit from 0x10; angle sweeps
     * 0x10000 - sine(frame * 0x400 + 0x4000) turns to 0x10000. */
    for (int f = 0; f <= 7; ++f)
        assert(aos_levelup_scale((uint8_t)f, test_sine) == 0x2000);
    assert(aos_levelup_scale(8, test_sine) == 0x10000 - test_sine(0x4000));
    assert(aos_levelup_scale(0xF, test_sine) ==
           0x10000 - test_sine(0xF * 0x800));
    assert(aos_levelup_scale(0x10, test_sine) == 0x10000);
    assert(aos_levelup_scale(0x3C, test_sine) == 0x10000);
    assert(aos_levelup_angle(0, test_sine) == 0x10000 - test_sine(0x4000));
    assert(aos_levelup_angle(0xF, test_sine) ==
           0x10000 - test_sine(0xF * 0x400 + 0x4000));
    assert(aos_levelup_angle(0x10, test_sine) == 0x10000);
    assert(aos_levelup_angle(0x3C, test_sine) == 0x10000);

    /* Sparkle velocity: random byte 0 launches along +x with the
     * cos/sin split of sub_08045270 (angle = byte << 8). */
    int32_t v[4];
    aos_levelup_sparkle_velocity(0, test_sine, v);
    /* angle 0: vx from sine(0x4000) (~+1.0), vy from sine(0) = 0. */
    assert(v[1] == 0);
    int64_t prod = (int64_t)test_sine(0x4000) * 24;
    int32_t expect_vx = prod >= 0 ? (int32_t)(prod >> 4)
                                 : (int32_t)((prod + 15) >> 4);
    assert(v[0] == expect_vx && v[0] > 0 && v[0] <= 0x18000);
    int64_t nprod = -(int64_t)v[0];
    int32_t expect_ax = nprod >= 0 ? (int32_t)(nprod >> 6)
                                  : (int32_t)((nprod + 63) >> 6);
    assert(v[2] == expect_ax && v[2] < 0);
    assert(v[3] == 0);
    /* Byte 0x40 (quarter turn of the byte circle): vx ~ 0, vy max. */
    aos_levelup_sparkle_velocity(0x40, test_sine, v);
    assert(v[0] == 0 && v[2] == 0);
    assert(v[1] > 0 && v[3] < 0);
    printf("aos_levelup: ok\n");
    return 0;
}
