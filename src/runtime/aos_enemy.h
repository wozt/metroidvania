/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies.
 *
 * Shared parts of the enemy framework (cvaos code_08060B98): the activity
 * window of sub_0806CC20, facing the player (sub_0806BC40), the signed
 * velocity by facing (sub_0806E120), the vertical distance to the player
 * (sub_0806BDEC) and the sine of sub_080009E4; and the bat (enemy 0x00,
 * EnemyBatCreate / EnemyBatUpdate). Facing flag 0x40 of + 0x58 means
 * mirrored: enemy sprites face left by default. Damage, hit stun, death
 * (states 3) and the contact and hit callbacks are NOT ported yet. No SDL. */
#ifndef AOS_ENEMY_H
#define AOS_ENEMY_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "aos_collision.h"
#include "aos_soma.h"

typedef struct {
    uint8_t id;             /* + 0x36 */
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    int32_t ax, ay;         /* + 0x50, + 0x54 */
    uint8_t state;          /* + 0x0A */
    uint8_t step;           /* + 0x0B */
    uint8_t substep;        /* + 0x0C */
    uint8_t timer;          /* + 0x0D */
    uint16_t phase;         /* + 0x14 */
    bool mirrored;          /* + 0x58 bit 0x40 */
    bool removed;           /* + 0x59 bit 8: deleted by the framework */
    AosAnimState anim;
} AosEnemy;

/* RandomNumberGenerator: r = (r >> 8) * 0x3243F6AD + 0x1B0CB175. The seed
 * the game starts from is not traced; aos_random_seed sets it. */
uint32_t aos_random(void);
void aos_random_seed(uint32_t seed);
/* sub_080009E4: sine of a 16-bit angle (0x10000 per turn), 16.16. */
int32_t aos_sine(uint32_t angle);
/* Creates enemy `id` at a record position; false for unported enemies. */
bool aos_enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, const AosSoma *soma,
                      const AosCollision *layer, const AosAnimSet *anims);
/* One update; cam_x / cam_y place the activity window. random() returns
 * the next RandomNumberGenerator value. */
void aos_enemy_update(AosEnemy *enemy, const AosSoma *soma, const AosAnimSet *anims,
                      int cam_x, int cam_y, uint32_t (*random)(void));

#endif /* AOS_ENEMY_H */
