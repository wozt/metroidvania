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
bool aos_enemy_move_toward(AosEnemy *enemy, int32_t tx, int32_t ty,
                           int32_t speed);
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
void aos_enemy_make_static_child(AosEnemy *child, const AosEnemy *owner,
                                 uint8_t role, int frame);
void aos_enemy_integrate(AosEnemy *enemy);
void aos_enemy_stop(AosEnemy *enemy);
void aos_enemy_falling_fragment(AosEnemy *owner, int frame,
                                AosHitReport *report,
                                uint32_t (*random)(void));
void aos_enemy_snap_to_floor(AosEnemy *enemy, const AosCollision *layer);
int32_t aos_enemy_toward_facing(const AosEnemy *enemy, int32_t speed);
void aos_enemy_copy_collision_result(AosHitReport *report,
                                     const AosHitReport *hit);
int32_t aos_enemy_random_centred(uint32_t (*random)(void), uint32_t n);
int aos_enemy_ground_walk(AosEnemy *enemy, const AosSoma *soma,
                          const AosCollision *layer, const AosProbes *probes,
                          int32_t speed, int mode);
bool aos_enemy_player_in_centred(const AosEnemy *enemy,
                                 const AosSoma *soma, int w, int h);
bool aos_enemy_player_in(const AosSoma *soma, int32_t x, int32_t y,
                         int w, int h);
int aos_enemy_fall_collide(AosEnemy *enemy, const AosCollision *layer,
                           int32_t floor_vy, int32_t head, int32_t limit);
bool aos_enemy_death_flames(AosEnemy *enemy, const AosEnemyKind *kind,
                            int period, uint32_t w, uint32_t h,
                            uint32_t (*random)(void),
                            AosHitReport *report);
bool aos_enemy_clamp_to_room(AosEnemy *enemy, const AosCollision *layer,
                             int margin);
int16_t aos_enemy_player_dy(const AosEnemy *enemy, const AosSoma *soma);
bool aos_enemy_player_within(const AosEnemy *enemy, const AosSoma *soma,
                             uint32_t range);
int aos_enemy_room_columns(const AosCollision *layer);
void aos_enemy_frame_boxes(const AosEnemy *enemy, const AosEnemyKind *kind,
                           AosBox *hurt, AosBox *attack, bool *hurt_on,
                           bool *attack_on);
extern const int8_t aos_enemy_killer_doll_pieces[5][6];
extern const int8_t aos_enemy_lightning_doll_pieces[5][6];

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
AosHitReport aos_enemy_gorgon_breath_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y);
AosHitReport aos_enemy_gorgon_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
AosHitReport aos_enemy_dead_warrior_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
AosHitReport aos_enemy_mimic_trigger_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, uint32_t (*random)(void));
AosHitReport aos_enemy_mimic_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
AosHitReport aos_enemy_fleaman_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    uint32_t (*random)(void));
AosHitReport aos_enemy_skeleton_knight_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
AosHitReport aos_enemy_killer_doll_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    uint32_t (*random)(void));
void aos_enemy_doll_shatter(AosEnemy *enemy, AosHitReport *report);
void aos_enemy_doll_piece_update(
    AosEnemy *enemy, const AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, int cam_y, uint32_t (*random)(void),
    AosHitReport *report);
AosHitReport aos_enemy_lightning_doll_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    uint32_t (*random)(void));
void aos_enemy_beam_shatter(AosEnemy *enemy, const AosSoma *soma,
                            AosHitReport *report);
AosHitReport aos_enemy_beam_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def);
AosHitReport aos_enemy_beam_skeleton_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
AosHitReport aos_enemy_skull_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));
void aos_enemy_fin_update(AosEnemy *enemy, const AosEnemyKind *kind,
                          int cam_x, int cam_y,
                          uint32_t (*random)(void));
AosHitReport aos_enemy_crusader_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
bool aos_enemy_player_ahead_within(const AosEnemy *enemy,
                                   const AosSoma *soma, int range);
AosHitReport aos_enemy_gargoyle_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));
AosHitReport aos_enemy_axe_update(
    AosEnemy *enemy, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void));
AosHitReport aos_enemy_armor_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
extern const AosProbes aos_enemy_rock_armor_probes;
extern const AosProbes aos_enemy_rock_probes;
int32_t aos_enemy_rock_armor_speed(const AosEnemy *enemy);
void aos_enemy_rock_armor_walk(AosEnemy *enemy, const AosSoma *soma,
                               const AosCollision *layer);
void aos_enemy_rock_armor_die(AosEnemy *enemy, const AosEnemyKind *kind,
                              uint32_t (*random)(void),
                              AosHitReport *report);
AosHitReport aos_enemy_rock_armor_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
AosHitReport aos_enemy_rock_update(
    AosEnemy *enemy, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void));
void aos_enemy_rock_piece_update(AosEnemy *enemy,
                                 const AosCollision *layer,
                                 const AosEnemyKind *kind, int cam_x,
                                 int cam_y, uint32_t (*random)(void));

#endif
