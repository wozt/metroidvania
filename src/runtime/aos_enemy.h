/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies.
 *
 * Shared parts of the enemy framework (cvaos code_08060B98): the activity
 * window of sub_0806CC20, facing the player (sub_0806BC40), the signed
 * velocity by facing (sub_0806E120), the vertical distance to the player
 * (sub_0806BDEC) and the sine of sub_080009E4; and the bat (enemy 0x00,
 * EnemyBatCreate / EnemyBatUpdate). Facing flag 0x40 of + 0x58 means
 * mirrored: enemy sprites face left by default. Combat: the frame boxes of
 * sub_0806B1FC, the collision pass of sub_0806E314 / sub_080421AC against
 * the player and his weapon, the bat's hit callback (sub_080AD6E4 /
 * sub_0806E218) and its death (state 3, sub_080AD5B8 / sub_0806BE74).
 * Hit stun, damage numbers, particles and drops are NOT ported. No SDL. */
#ifndef AOS_ENEMY_H
#define AOS_ENEMY_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "aos_collision.h"
#include "aos_combat.h"
#include "aos_soma.h"
#include "aos_weapon.h"

/* Stats of the enemy record copied by sub_0806B04C (from enemies.tsv). */
typedef struct {
    int16_t hp;             /* record + 0xC */
    uint8_t contact;        /* record + 0x13 -> + 0x3C: attack against the player */
    uint8_t defence;        /* record + 0x14 -> + 0x3D */
    uint16_t weak, resist;  /* record + 0x1A, + 0x1C */
} AosEnemyStats;

#define AOS_ENEMY_MAX_ANIMS 8
#define AOS_ENEMY_MAX_FRAMES 16

/* Per animation frame: box mode (frame record + 4: 0 none, 1 one box for
 * both, 2 hurtbox then attack box) and boxes. */
typedef struct {
    const AosAnimSet *anims;
    uint8_t modes[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES];
    AosBox hurt[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES];
    AosBox attack[AOS_ENEMY_MAX_ANIMS][AOS_ENEMY_MAX_FRAMES];
    uint8_t blink[40];      /* 0x08118CE0 & 1: hidden while dying */
} AosEnemyKind;

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
    bool vflip;             /* + 0x58 bit 0x80 */
    bool hidden;            /* + 0x58 bit 0x20 */
    int16_t hp;             /* + 0x34 */
    uint8_t hit_flash;      /* + 0x2D */
    AosEnemyStats stats;
    AosCombat combat;
    AosAnimState anim;
} AosEnemy;

/* What one collision pass did. */
typedef struct {
    bool soma_hit;          /* the enemy's attack hit Soma */
    int soma_damage;
    bool enemy_hit;         /* the weapon hit the enemy */
    int enemy_damage;
    bool killed;
} AosHitReport;

/* RandomNumberGenerator: r = (r >> 8) * 0x3243F6AD + 0x1B0CB175. The seed
 * the game starts from is not traced; aos_random_seed sets it. */
uint32_t aos_random(void);
void aos_random_seed(uint32_t seed);
/* sub_080009E4: sine of a 16-bit angle (0x10000 per turn), 16.16. */
int32_t aos_sine(uint32_t angle);
/* Creates enemy `id` at a record position; false for unported enemies. */
bool aos_enemy_create(AosEnemy *enemy, uint8_t id, int32_t x, int32_t y, const AosSoma *soma,
                      const AosCollision *layer, const AosEnemyKind *kind,
                      const AosEnemyStats *stats);
/* One update; cam_x / cam_y place the activity window. random() returns
 * the next RandomNumberGenerator value. The collision pass runs inside, as
 * in the game: soma_atk / soma_def are the player's current ATK and DEF
 * (new-game values are not traced); the weapon may be NULL. */
AosHitReport aos_enemy_update(AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
                              const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
                              int soma_atk, int soma_def, int cam_x, int cam_y,
                              uint32_t (*random)(void));

#endif /* AOS_ENEMY_H */
