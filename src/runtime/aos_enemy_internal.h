/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_ENEMY_INTERNAL_H
#define AOS_ENEMY_INTERNAL_H

#include "aos_enemy.h"

/* Internal framework API shared by the per-family translation units. */
AosEffect *aos_enemy_spawn_drift(uint8_t kind, int32_t x, int32_t y,
                                 AosHitReport *report);
void aos_enemy_take_effects(AosHitReport *to, const AosHitReport *from);
bool aos_enemy_outside_margins(const AosEnemyKind *kind, int n, int sx, int sy);
void aos_enemy_step_anim(AosEnemy *enemy, const AosAnimSet *anims);
AosHitReport aos_enemy_collide(AosEnemy *enemy, AosSoma *soma,
                               const AosEnemyKind *kind,
                               const AosWeaponEntity *weapon,
                               const AosWeaponFrames *weapon_frames,
                               int soma_atk, int soma_def);
void aos_enemy_accel_move(AosEnemy *enemy, int32_t limit);
void aos_enemy_soldier_face(AosEnemy *enemy, const AosSoma *soma);

AosHitReport aos_enemy_sky_fish_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));

#endif
