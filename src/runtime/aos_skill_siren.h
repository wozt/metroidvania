/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_SIREN_H
#define AOS_SKILL_SIREN_H

#include <stdbool.h>
#include <stdint.h>

enum { AOS_SIREN_FRAME_FIRST = 2, AOS_SIREN_SOUND_FIRST = 0xDD };

/* The note projectile created by SkillSirenUse (parameter 0). */
typedef struct {
    int32_t x, y;       /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;     /* + 0x48, + 0x4C */
    uint16_t phase;     /* + 0x24 */
    uint8_t state;      /* + 0x0A: 1 flying, 2 deleting */
    uint8_t frame;      /* direct frame 2 or 3 at + 0x65 */
    uint16_t song;      /* 0xDD or 0xDE from 0x084F13A6 */
    bool struck;        /* + 0x18: sub_0802D9B4 ran */
} AosSirenSoul;

/* State 0: choose the note variant from the first random draw, place it at
 * Soma's facing and (8, -26), fly at 4 pixels per frame and seed its sine
 * phase from the second draw. The sprite itself is never mirrored. */
AosSirenSoul aos_siren_soul_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                   uint32_t variant_random, uint32_t phase_random);

/* States 1 and 2: add sin(phase) to Y, advance phase by 0x400 and move X.
 * A hit or margin exit enters state 2 for one frame; the next update returns
 * false and deletes the entity. */
bool aos_siren_soul_update(AosSirenSoul *note, int cam_x, int cam_y, int mx, int my);

#endif
