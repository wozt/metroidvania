/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_UKOBACK_H
#define AOS_SOUL_RED_UKOBACK_H
#include <stdbool.h>
#include <stdint.h>

/* SkillUkobackUse's lingering ghost: glued to the source, a + 0x24 ramp
 * toward 0xFFFF then a -0x2000/frame decay decides its lifetime; the
 * sub_0803E058/sub_0803E6BC palette bookkeeping is not tracked. */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 lingering, 2 decaying */
    uint8_t timer;      /* + 0x0D: counts 0xF0 down in state 1 */
    int32_t ramp;       /* + 0x24: + 0x1000/frame to 0xFFFF, then -0x2000 */
} AosUkoback;

/* State 0 loads the entry-0x108 record, presets attack box 5 with the
 * sub_0802D9B4 callback plus the 0x1010F8F8 box, plays song 0xDC,
 * places it through sub_08031EA4(entity, source, 8, -0x1A) and enters
 * state 1 with + 0x0D = 0xF0. */
AosUkoback aos_ukoback_create(void);
/* One update. `force_end` stands for the native + 0x18 early-out that
 * zeroes + 0x0D; `outside` is the sub_0806D128(e, 2) margin probe.
 * Returns false once deleted. */
bool aos_ukoback_update(AosUkoback *ukoback, bool force_end, bool outside);
#endif
