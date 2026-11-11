/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_COCKATRICE_H
#define AOS_SOUL_RED_COCKATRICE_H
#include <stdbool.h>
#include <stdint.h>

/* SkillCockatriceUse's serpent: state 1 spins the + 0x2C angle by the
 * facing-signed + 0x2E (-+0x40) for 0x20 frames of + 0x0D while the
 * + 0x50/+ 0x54 sine velocities (angle +/- 0x4000, * 32) shape the
 * dynamic attack box; the + 0x48/+ 0x4C drift fields and the
 * sub_08023530(e, sub_08029568) partner registration are not ported.
 * Song 0xCD is requested once through the sub_08013D60 history dedup. */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 spin */
    uint8_t timer;      /* + 0x0D: 0x20 spin frames */
    int32_t angle;      /* + 0x2C: 0x2800, mirrored -0x8000 - 0x2800 */
    int32_t step;       /* + 0x2E: -0x40, +0x40 when mirrored */
    int32_t x;          /* + 0x40: entity position (16.16) */
    int32_t y;          /* + 0x44 */
    int32_t vx;         /* + 0x50: sine(angle + 0x4000) * 32 */
    int32_t vy;         /* + 0x54: sine(angle) * 32 */
} AosCockatrice;

/* State 0 loads the entry-0x144 record (the Kyoma Demon art), + 0x65 =
 * 1, + 0x5C = 0xD, + 0x59 |= 0x12, + 0x58 = variant | facing bit 6,
 * + 0x2C/+ 0x2E facing-signed, + 0x4A = -+8, + 0x4E = -+0x1A, attack
 * box preset 5, + 0x72 |= 4, song-history 0xCD and enters state 1 on
 * the same frame. `mirrored` is the source + 0x58 bit 6. */
AosCockatrice aos_cockatrice_create(int32_t x, int32_t y, bool mirrored);
/* One update; returns false once the + 0x0D timer expires (the native
 * runs the state-2 cleanup and EntityDelete on that frame) or the
 * sub_0806D128(e, 2) margin probe / + 0x59 bit 4 fires. */
bool aos_cockatrice_update(AosCockatrice *cockatrice, bool outside);
#endif
