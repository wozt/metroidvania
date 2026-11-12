/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_BLUE_FINAL_GUARD_H
#define AOS_SOUL_BLUE_FINAL_GUARD_H
#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_soma.h"
#include "../souls/aos_soul.h"

/* SkillFinalGuardUse (blue soul 7, held, 60 MP per second): the guard
 * that trails Soma. The upkeep sub_0802DE5C deletes the entity
 * outright when it ends (no shrink-out). State 0 requires the source's
 * + 0xA == 0 and (+ 0x10 & 0x001001A2) == 0x00100000 — grounded with
 * airborne/attacking/stop-at-wall/body-special all clear — else it
 * deletes; on success it loads the record (tiles 0x081DF960, palette
 * 0x0820B5E0, frames 0x082371F0), forces the source's + 0xA to 0xA,
 * requests its 0x35 sequence through sub_0803F2C8, clears + 0x10 bits
 * 0x400 and 0x1000000, plays song 0x1A7 and enters state 1. State 1
 * only advances to state 2. From state 2 on, the same + 0x10 mask is
 * re-checked every update (a miss deletes), then + 0x13260 |= 0x4000
 * and + 0x131B8 |= 0x820 are set — the + 0x131B8 bits have no
 * modelled reader and are exposed through aux_bits — and the guardian
 * copies the source's facing and pixel position with y + 4. */

typedef struct {
    uint8_t phase;          /* the entity's + 0xA: 0 init, 1, 2 follow */
    int32_t x, y;           /* + 0x40/+ 0x44: the source's pixel pos, y + 4 */
    bool facing_left;       /* + 0x58 bit 6: the source's facing */
    bool song_1a7;          /* cast sound request */
    uint32_t aux_bits;      /* gEwramData + 0x131B8 bits set while active */
    bool dead;              /* upkeep ended or the + 0x10 mask failed */
} AosFinalGuardGuardian;

/* One update after Soma's; `soma` is the + 0x13110 source entity.
 * Returns false once the entity is deleted. */
bool aos_final_guard_guardian_update(AosFinalGuardGuardian *guard,
                                     AosGuardian *guardian, AosStats *stats,
                                     const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                     uint16_t held, uint16_t pressed,
                                     uint32_t *soma_flags, AosSoma *soma,
                                     uint32_t (*random)(void));
#endif
