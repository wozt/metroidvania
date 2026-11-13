/* SPDX-License-Identifier: GPL-3.0-only */
/* Thrown-object update callbacks from code_08060B98.s:
 * sub_08065858, sub_080658AC and sub_0806592C share the same
 * ballistic preamble (position += velocity, velocity +=
 * acceleration) and the sub_08068AD4 box probe (-0x80, -0x40,
 * 0x1F0, 0x120); a failed probe marks + 0x59 bit 3. The latter
 * two also spin + 0x38 by s16 + 0x26 and run the sub_0803E058
 * affine call; sub_080658AC additionally restores the palette
 * through sub_0803E6BC(+ 0x58 & 7) on the failed probe. */
#ifndef AOS_FLYER_H
#define AOS_FLYER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t x;          /* +0x40 */
    int32_t y;          /* +0x44 */
    int32_t vx;         /* +0x48 */
    int32_t vy;         /* +0x4C */
    int32_t ax;         /* +0x50 */
    int32_t ay;         /* +0x54 */
    int32_t spin;       /* +0x38 */
    int16_t spin_rate;  /* +0x26 */
    uint8_t flags59;    /* +0x59 */
    uint8_t pal_nibble; /* +0x58 & 7 */
    uint32_t affine;    /* +0x1C */
    bool probe_inside;  /* sub_08068AD4 result, injected */
} AosFlyer;

/* sub_08065858: ballistic step, then the probe; a miss sets
 * + 0x59 bit 3. */
void aos_flyer_step_65858(AosFlyer *f);

/* sub_080658AC: ballistic step, + 0x38 += s16 + 0x26, the
 * sub_0803E058 affine request, then the probe; a miss restores
 * the palette nibble through sub_0803E6BC and sets + 0x59 bit 3.
 * `pal_restored` reports the sub_0803E6BC call. */
void aos_flyer_step_658ac(AosFlyer *f, bool *pal_restored);

/* sub_0806592c: same as sub_080658AC without the palette
 * restore. */
void aos_flyer_step_6592c(AosFlyer *f);

#endif
