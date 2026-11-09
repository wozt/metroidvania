/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_KILLER_DOLL_H
#define AOS_SOUL_RED_KILLER_DOLL_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_collision.h"

/* SkillKillerDollUse (0x08024F38): a thrown doll the enemies aim at
 * (gEwramData + 0x13168) until it vanishes. It has no attack. */
enum { AOS_DECOY_FRAMES = 0xB4, AOS_DECOY_BLINK = 0x1E };

typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    uint8_t state;          /* + 0x0A: 1 thrown, 2 blinking */
    uint8_t timer;          /* + 0x0D */
    bool blinking;          /* + 0x0F */
} AosDecoy;

/* State 0: placed by sub_08031EA4 at Soma's facing and (8, -16), thrown at
 * 1.5 forward and 2.5 up for 0xB4 frames (sound 0xE5, sequence 0 or 1 at
 * random and the 0x13168 link belong to the caller). */
AosDecoy aos_decoy_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* One update: thrown, it moves, gains 0x2000 of vy (8.0 cap), lands on
 * floors probed at y + 1 (y += depth + 2, vy 0, then braking by 0x4000
 * instead of 0x100), meets ceilings at y - 32 and walls at x +/- 8, y - 16
 * (bouncing back at half speed); once its 0xB4 frames are out it blinks in
 * place for 0x1E more. Returns false when it vanishes (the caller gives
 * 0x13168 back to Soma). */
bool aos_decoy_update(AosDecoy *decoy, const AosCollision *layer);
#endif
