/* Tests for the entity-space helpers in aos_math.c ported from
 * code_08060B98.s (sub_08068A70 through sub_08068C74). */
#include "../src/runtime/aos/math/aos_math.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    /* sub_08068AD4: unsigned box probe on +0x42/+0x46. */
    assert(aos_probe_box(100, 50, 90, 40, 20, 20));
    assert(aos_probe_box(90, 40, 90, 40, 20, 20));   /* origin edge */
    assert(aos_probe_box(110, 60, 90, 40, 20, 20)); /* far edge */
    assert(!aos_probe_box(111, 60, 90, 40, 20, 20));
    assert(!aos_probe_box(89, 50, 90, 40, 20, 20));  /* negative dx */
    assert(!aos_probe_box(100, 39, 90, 40, 20, 20)); /* negative dy */
    assert(!aos_probe_box(100, 61, 90, 40, 20, 20));

    /* sub_08068A70: angle between two +0x42/+0x46 positions. */
    assert(aos_entity_angle(0, 0, 0x4000, 0) == 0);        /* +x */
    assert(aos_entity_angle(0, 0, 0, 0x4000) == 0x4000);   /* +y */
    assert(aos_entity_angle(0, 0, -0x4000, 0) == 0x8000);  /* -x */
    assert(aos_entity_angle(10, 20, 10, 20) == 0);         /* same point */

    /* sub_08068AA0/AB4/ABC/AC8: phase complements. */
    assert(aos_phase_opposite(0x1000) == (int32_t)(int16_t)(0x8000 - 0x1000));
    assert(aos_phase_negate(0x2000) == -0x2000);
    assert(aos_phase_negate(-0x2000) == 0x2000);
    assert(aos_phase_comp_f0(0x10000) == 0xF00000 - 0x10000);
    assert(aos_phase_comp_a0(0x20000) == 0xA00000 - 0x20000);

    /* sub_08068B14/44: sign of the delta against a reference. */
    assert(aos_sign_delta(100, 50) == -1);
    assert(aos_sign_delta(50, 100) == 1);
    assert(aos_sign_delta(50, 50) == 0);

    /* sub_08068B74: whole-pixel distance probe. */
    assert(aos_within_range(0x10000, 0, 0, 0, 0x20000));       /* dist 1 <= 2 */
    assert(!aos_within_range(0x30000, 0, 0, 0, 0x20000));      /* dist 3 > 2 */
    assert(aos_within_range(0x10000, 0x10000, 0, 0, 0x20000)); /* ~1.41 <= 2 */
    assert(aos_within_range(0x20000, 0, 0x10000, 0, 0x20000)); /* offset ref */

    /* sub_08068BC0: s16 circle sign. */
    assert(aos_s16_sign(10, 10) == 0);
    assert(aos_s16_sign(20, 10) == -1);  /* (a-b)=10 < (b-a)=65526 */
    assert(aos_s16_sign(10, 20) == 1);   /* (a-b)=65526 > (b-a)=10 */
    assert(aos_s16_sign(-1, 1) == 1);    /* wrap: -2 vs +2 */

    /* sub_08068BF0: sine steering — angle 0 maps the delta
     * through (sine(0x4000), sine(0)) = (max, 0) so the
     * position rotates by a quarter turn around the target. */
    {
        int32_t px = 0x40000, py = 0;      /* 4.0 px right of target */
        int32_t ox = px, oy = py;
        aos_steer_toward(&px, &py, 0, 0, 0);
        assert(px != ox || py != oy);
        /* With angle 0: s90 = sine(0x4000)>>8 (large), s0 = 0
         * so px' = dx*s90, py' = dy*s90 — a scaled delta. */
        int32_t expect = (0x40000 >> 16) * ((int32_t)aos_sine(0x4000) >> 8);
        assert(px == expect && py == 0);
    }
    {
        /* angle 0x4000: s90 = sine(0x8000)>>8 = 0, s0 =
         * sine(0x4000)>>8 so the axes swap. */
        int32_t px = 0x40000, py = 0;
        int32_t expect = (0x40000 >> 16) * ((int32_t)aos_sine(0x4000) >> 8);
        aos_steer_toward(&px, &py, 0, 0, 0x4000);
        assert(px == 0 && py == expect);
    }


    /* sub_08068C74/68CA8: clamped approach step. */
    assert(aos_approach_step(0, 100, 30, 2) == 60);    /* 0+30*2 */
    assert(aos_approach_step(0, 50, 30, 2) == 50);     /* clamped at bound */
    assert(aos_approach_step(100, 0, -30, 2) == 40);   /* 100-60 */
    assert(aos_approach_step(100, 0, -60, 2) == 0);    /* clamped */
    assert(aos_approach_step(10, 10, 5, 2) == 20);     /* bound==cur keeps next */

    /* sub_08068CDC: advance along a heading. angle 0 moves
     * along +x by dist>>8 * sine(0x4000)>>8. */
    {
        int32_t px = 0, py = 0;
        int32_t d = aos_advance_along(&px, &py, 0, 0, 0x100000, 0x400, 0x100);
        /* cur=0, bound=0x100000, step*scale=0x40000 -> dist 0x40000,
         * d = 0x400, sx = sine(0x4000)>>8 */
        int32_t expect = 0x400 * ((int32_t)aos_sine(0x4000) >> 8);
        assert(d == 0x40000 && px == expect && py == 0);
    }
    {
        /* angle 0x4000 swaps the axes. */
        int32_t px = 0, py = 0;
        int32_t d = aos_advance_along(&px, &py, 0x4000, 0, 0x100000, 0x400, 0x100);
        int32_t expect = 0x400 * ((int32_t)aos_sine(0x4000) >> 8);
        assert(d == 0x40000 && px == 0 && py == expect);
    }

    puts("aos_math entity helpers: ok");
    return 0;
}
