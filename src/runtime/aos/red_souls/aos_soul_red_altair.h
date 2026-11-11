/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_ALTAIR_H
#define AOS_SOUL_RED_ALTAIR_H
#include <stdbool.h>
#include <stdint.h>

/* SkillAltairUse's four-state spirit: init, a hit-gated flicker
 * (state 2 alternates the + 0x5A variant bits every other frame),
 * then a knocked-back state whose + 0x6E count reaching 2 plays
 * song 0xEE. The sub_0806D430 physics and the + 0x5A animation
 * variants are not ported. */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 idle, 2 flicker, 3 launch */
    uint8_t timer;      /* + 0x0D: parity counter in state 2 */
    uint16_t phase;     /* + 0x6E: reaches 2 in state 3 for song 0xEE */
} AosAltair;

/* State 0 loads the entry-0x3C record, presets attack box 5 plus the
 * 0x1020F8F0 box {-16,-8,32,16}, sets + 0x48 = -0x28000, + 0x72 |= 6,
 * places it through sub_08031EA4(entity, source, 0x14, -0x1A), plays
 * song 0xD6 and enters state 1. */
AosAltair aos_altair_create(void);
/* One update. `hit` stands for the native + 0x6C bit 4 that advances
 * states 1->2->3; `outside` is the sub_0806D128(e, 2) margin probe.
 * `song_ee` reports the frame + 0x6E == 2 is observed in state 3.
 * Returns false once deleted. */
bool aos_altair_update(AosAltair *altair, bool hit, bool outside,
                       bool *song_ee);
#endif
