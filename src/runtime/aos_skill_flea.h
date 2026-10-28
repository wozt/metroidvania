/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_FLEA_H
#define AOS_SKILL_FLEA_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_enemy.h"

enum { AOS_FLEA_FRAMES = 0xA0, AOS_FLEA_END_SEQUENCE = 1, AOS_FLEA_END_FRAMES = 0x10 };

/* SkillFleamanUse's flea; its walker state lives in an AosEnemy. */
typedef struct {
    AosEnemy body;      /* x, y, vx, vy, ay, mirrored (+ 0x58 bit 6), ground */
    uint8_t state;      /* + 0x0A: 1 hopping, 2 ending */
    uint8_t timer;      /* + 0x0D */
    uint8_t frame;      /* + 0x65 while hopping: 0 rising, 1 level, 2 falling */
    bool blinking;      /* + 0x0F */
} AosFlea;

/* State 0: placed by sub_08031EA4 at Soma's facing and (8, -0x1A), then
 * turned around (+ 0x58 bit 6 flipped: it walks ahead of Soma), gravity
 * 0x2000, 160 frames. */
AosFlea aos_flea_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* One update: hopping, it steps the walker (sub_0806CAF8 at 1.5 with mode
 * 8), picks its still frame from vy, turns at walls and on landing hops
 * at -1.0 three times in four, else at -5.0 plus a random fraction; when
 * its 160 frames are out it starts its end (sequence 1, blinking, 16
 * frames), during which a spark position is drawn every fourth frame (the
 * sparks of sub_08045CEC are not created) and it is deleted (false) when
 * the timer is out. It is deleted as well outside the screen margins (mx,
 * my) of entry 2. `started_end` reports the frame the end begins. */
bool aos_flea_update(AosFlea *flea, const AosCollision *layer, const AosProbes *probes,
                     uint32_t (*random)(void), int cam_x, int cam_y, int mx, int my,
                     bool *started_end);
#endif
