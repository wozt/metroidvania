/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_GLADIATOR_H
#define AOS_SKILL_GLADIATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_enemy.h"

enum { AOS_GLADIATOR_MOVING = 1, AOS_GLADIATOR_ENDING = 2 };

/* SkillGladiatorUse, 0x0802B4C8; red soul 42. */
typedef struct {
    AosEnemy body;          /* +0x40 through +0x54 */
    int32_t stored_vx;      /* +0x24; the walker receives its negation */
    uint16_t angle;         /* +0x2C */
    uint8_t state;          /* +0x0A */
    uint8_t timer;          /* +0x0D */
    bool blinking;          /* +0x0F */
    bool facing_left;       /* +0x58 bit 6 */
} AosGladiatorSoul;

typedef struct {
    bool spawn;
    int32_t x, y;
} AosGladiatorDust;

/* State 0 places frame 0 at Soma's facing and (8, -26), throws it forward
 * at 2 pixels per frame and up at 2.5, and gives it 1/8 gravity. */
AosGladiatorSoul aos_gladiator_soul_create(int32_t soma_x, int32_t soma_y,
                                           bool facing_left);

/* States 1-2 of SkillGladiatorUse. The rolling probe walker receives mode
 * 2 and the table at 0x084F13C8. A wall reverses half the speed; otherwise
 * it loses 1/128 pixel per frame until its 16-frame blinking end. Every
 * fourth ending frame returns one sub_08045CEC animation-0 particle. */
bool aos_gladiator_soul_update(AosGladiatorSoul *soul, const AosCollision *layer,
                               const AosProbes *probes, uint32_t (*random)(void),
                               int cam_x, int cam_y, int margin_x, int margin_y,
                               AosGladiatorDust *dust);

#endif
