/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_MAN_EATER_H
#define AOS_SOUL_RED_MAN_EATER_H
#include <stdbool.h>
#include <stdint.h>

/* SkillManEaterUse (red soul 27): the lunging bite. State 0 rolls
 * the sub_0803E654 variant into + 0xB (rng % 3), derives + 0xC =
 * variant * 5, places the entity through sub_08031EA4(source, 8,
 * -0x1A) — 8 px along the facing, 0x1A px up — sets the facing-signed
 * + 0x48 = +-0x12000, registers the type-5 contact with the
 * sub_0802D9B4 callback (+ 0x18 = 1 on touch), presets attack 1 /
 * 0x14, packs the 0x0C0CFAFA box ({-6,-6,12,12}), arms + 0x72 bit 4,
 * plays song 0x15E and bumps the + 0x131BE counter. State 1 flies
 * straight: + 0x65 = + 0xC + (+ 0x24 / 10), the sub_0806D3D8
 * integration under the 0x80000 cap and + 0x24 up to 0x1D, then
 * + 0xD = 0x30 into state 2. State 2 lingers + 0xD frames into
 * state 3 (+ 0xF = 1, + 0xD = 0x10); state 3 counts its own + 0xD
 * into state 5, which clears gEwramData + 0x13124[+ 0x1A * 4], drops
 * the + 0x131BE counter and deletes. State 4 is the bitten pose the
 * shared tail enters while the state is <= 2 and + 0x18 is set:
 * + 0x48 = 0, + 0x65 = + 0xC + 4, + 0xD = 0x10 — it counts down into
 * state 3 the same way. The tail also deletes on the
 * sub_0806D128(e, 2) margin probe or the + 0x59 bit-4 mark. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    int16_t tick;           /* + 0x24: state-1 frame counter */
    uint8_t timer;          /* + 0xD: state 2/3/4 countdown */
    uint8_t flag_f;         /* + 0xF: set entering states 3/4 */
    uint8_t variant;        /* + 0xB: rng % 3 */
    uint8_t base;           /* + 0xC: variant * 5 */
    uint8_t frame;          /* + 0x65: still-frame selector */
    bool hit;               /* + 0x18: the sub_0802D9B4 contact flag */
    uint8_t state;          /* + 0x0A: 1 fly, 2 linger, 3 fade, 4 bitten, 5 delete */
    bool outside;           /* sub_0806D128(e, 2): injected */
    bool marked;            /* + 0x59 bit 4: injected */
    bool released;          /* state 5 ran: + 0x13124 slot cleared, counter-- */
} AosManEater;

/* The entity as the state-0 init leaves it: sub_08031EA4(source, 8,
 * -0x1A) — 8 px along the facing, 0x1A px up — vx -+0x12000 signed
 * by the facing, variant/base/frame set, state 1 on the same frame;
 * song 0x15E and the + 0x131BE bump are the caller's. `rng` supplies
 * the variant roll. */
AosManEater aos_man_eater_create(int32_t soma_x, int32_t soma_y,
                                 bool facing_left, uint32_t (*rng)(void));
/* One update; false once the entity is deleted (state 5, the margin
 * probe or the + 0x59 bit-4 mark). `rng` is unused by the machine —
 * kept for signature symmetry. */
bool aos_man_eater_update(AosManEater *man_eater, uint32_t (*rng)(void));
#endif
