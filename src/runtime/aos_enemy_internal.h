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
uint32_t aos_enemy_umod(uint32_t value, uint32_t divisor);
void aos_enemy_spawn_flame_at(int32_t cx, int32_t cy, uint32_t w,
                              uint32_t h, uint32_t (*random)(void),
                              AosHitReport *report);
int32_t aos_enemy_pixel_delta(int32_t to, int32_t from);
void aos_enemy_play_loop(AosEnemy *enemy, const AosAnimSet *anims,
                         unsigned id, bool loop);
void aos_enemy_play(AosEnemy *enemy, const AosAnimSet *anims, unsigned id);
int32_t aos_enemy_target_x(const AosSoma *soma);
int32_t aos_enemy_target_y(const AosSoma *soma);
void aos_enemy_face_player(AosEnemy *enemy, const AosSoma *soma);
bool aos_enemy_player_near(const AosSoma *soma, int32_t x, int32_t y,
                           int w, int h);
void aos_enemy_generic_death(AosEnemy *enemy, const AosEnemyKind *kind,
                             uint32_t (*random)(void),
                             AosHitReport *report);
void aos_enemy_merge_hit(AosHitReport *report, AosHitReport hit);
int aos_enemy_walk_collide(AosEnemy *enemy, const AosCollision *layer,
                           int32_t bounce_vy, int32_t head,
                           int32_t max_speed, int half_width);
AosEnemy aos_enemy_make_gib(const AosEnemy *enemy, int32_t dx, int32_t dy,
                            int32_t vx, int32_t vy, int16_t frame);
int32_t aos_enemy_gib_speed(const AosEnemy *enemy,
                            uint32_t (*random)(void));
int32_t aos_enemy_slope_speed(const AosEnemy *enemy,
                              const AosCollision *layer, int32_t speed);
bool aos_enemy_blocked_ahead(const AosCollision *layer, int32_t x, int32_t y);
bool aos_enemy_at_room_edge(const AosEnemy *enemy,
                            const AosCollision *layer);

AosHitReport aos_enemy_sky_fish_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));
void aos_enemy_ectoplasm_struck(AosEnemy *enemy, const AosSoma *soma,
                                const AosAnimSet *anims);
AosHitReport aos_enemy_ectoplasm_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));
AosHitReport aos_enemy_ghost_dancer_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y);
AosHitReport aos_enemy_medusa_head_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));
AosHitReport aos_enemy_ghost_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));
AosHitReport aos_enemy_zombie_officer_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
AosHitReport aos_enemy_curly_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
void aos_enemy_needle_piece_update(AosEnemy *enemy, int cam_x, int cam_y);
AosHitReport aos_enemy_needles_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));

#endif
