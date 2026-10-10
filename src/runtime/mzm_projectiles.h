/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission weapon selection and projectile movement.
 *
 * Ports SamusSetHighlightedWeapon, SamusCheckFireBeamMissile, the beam and
 * missile branches of ProjectileUpdate, ProjectileInit, ProjectileMove,
 * ProjectileProcessNormalBeam/Missile/SuperMissile, ProjectileUpdateAnimation
 * and the distance part of ProjectileCheckDespawn from the pinned mzm
 * decompilation. Sprite and environment collisions, particles, charge beams,
 * beam upgrades, bombs and tumbling missiles are not implemented yet.
 * Projectile collision probes one point against the runtime's verified
 * Clipdata boxes instead of ClipdataProcess. No SDL dependency. */
#ifndef MZM_PROJECTILES_H
#define MZM_PROJECTILES_H

#include "mzm_samus.h"

#define MZM_MAX_PROJECTILES 16
#define MZM_PROJECTILE_LIMIT_BEAM 6
#define MZM_PROJECTILE_LIMIT_MISSILE 4
#define MZM_PROJECTILE_LIMIT_SUPER_MISSILE 4
#define MZM_SHORT_BEAM_LIFETIME 12
#define MZM_BEAM_COOLDOWN 7
#define MZM_MISSILE_COOLDOWN 9
#define MZM_SUPER_MISSILE_COOLDOWN 11

typedef enum {
    MZM_PROJECTILE_BEAM,
    MZM_PROJECTILE_MISSILE,
    MZM_PROJECTILE_SUPER_MISSILE
} MzmProjectileType;

typedef enum {
    MZM_WEAPON_NONE,
    MZM_WEAPON_MISSILE,
    MZM_WEAPON_SUPER_MISSILE
} MzmHighlightedWeapon;

typedef enum {
    MZM_STAGE_INIT,
    MZM_STAGE_SPAWNING,
    MZM_STAGE_MOVING
} MzmProjectileStage;

typedef struct {
    bool active;
    MzmProjectileType type;
    int32_t x, y;          /* subpixels */
    MzmAim direction;
    bool x_flip, y_flip;   /* x_flip: travelling right */
    MzmProjectileStage stage;
    uint8_t timer;
    uint8_t anim_frame, anim_counter;
} MzmProjectile;

struct MzmProjectile;
typedef struct {
    void *context;
    /* Native frame durations of the projectile's current animation;
     * returns the frame count, or 0 when unavailable. */
    int (*durations)(void *context, const MzmProjectile *projectile,
                     uint8_t *durations, int max);
} MzmProjectileAnimation;

typedef struct {
    MzmProjectile list[MZM_MAX_PROJECTILES];
    uint8_t cooldown;
    MzmHighlightedWeapon highlighted;
    bool super_missiles_selected;
    bool pending;
    MzmProjectileType pending_type;
} MzmWeapons;

void mzm_weapons_init(MzmWeapons *weapons);
/* SamusExecutePoseHandler prelude: cooldown, weapon highlight and the fire
 * check. Returns the native hasNewProjectile flag for the pose handler. The
 * R button arms missiles, Select toggles missiles and super missiles. */
bool mzm_weapons_begin_frame(MzmWeapons *weapons, const MzmSamus *samus,
                             uint16_t held, uint16_t pressed,
                             const MzmEquipment *equipment);
/* ProjectileUpdate: spawn the pending projectile at the arm cannon (pixel
 * offset from Samus's native pixel position) and advance every projectile. */
void mzm_weapons_update(MzmWeapons *weapons, const MzmSamus *samus,
                        MzmEquipment *equipment, int cannon_x, int cannon_y,
                        const MzmCollision *collision,
                        const MzmProjectileAnimation *animation);
int mzm_projectile_count(const MzmWeapons *weapons, MzmProjectileType type);

#endif /* MZM_PROJECTILES_H */
