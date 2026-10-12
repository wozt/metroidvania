/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission weapon selection and projectile movement.
 *
 * Ports SamusSetHighlightedWeapon, SamusCheckFireBeamMissile, the beam and
 * missile branches of ProjectileUpdate, ProjectileInit, ProjectileMove,
 * ProjectileProcessNormalBeam/Missile/SuperMissile, ProjectileUpdateAnimation
 * and the distance part of ProjectileCheckDespawn from the pinned mzm
 * decompilation, and the bombs (ProjectileProcessBomb,
 * ProjectileCheckSamusBombBounce) and power bombs (ProjectileProcessPowerBomb,
 * the explosion in mzm_power_bomb). Charge beams, beam upgrades and tumbling
 * missiles are not implemented yet.
 * Projectile collision probes one point against the runtime's verified
 * Clipdata boxes instead of ClipdataProcess. No SDL dependency. */
#ifndef MZM_PROJECTILES_H
#define MZM_PROJECTILES_H

#include "mzm_power_bomb.h"
#include "mzm_samus.h"

#define MZM_MAX_PROJECTILES 16
#define MZM_PROJECTILE_LIMIT_BEAM 6
#define MZM_PROJECTILE_LIMIT_MISSILE 4
#define MZM_PROJECTILE_LIMIT_SUPER_MISSILE 4
#define MZM_PROJECTILE_LIMIT_BOMB 4
#define MZM_PROJECTILE_LIMIT_POWER_BOMB 1
/* BOMB_EXPLOSION_TIMER, also each spin's duration. */
#define MZM_BOMB_TIMER 16
#define MZM_SHORT_BEAM_LIFETIME 12
#define MZM_BEAM_COOLDOWN 7
#define MZM_MISSILE_COOLDOWN 9
#define MZM_SUPER_MISSILE_COOLDOWN 11

typedef enum {
    MZM_PROJECTILE_BEAM,
    MZM_PROJECTILE_MISSILE,
    MZM_PROJECTILE_SUPER_MISSILE,
    MZM_PROJECTILE_BOMB,
    MZM_PROJECTILE_POWER_BOMB,
    MZM_PROJECTILE_LONG_BEAM,
    MZM_PROJECTILE_CHARGED_BEAM,
    MZM_PROJECTILE_CHARGED_LONG_BEAM
} MzmProjectileType;

/* CHARGE_BEAM_START_THRESHOLD, CHARGE_BEAM_UNCHARGED_THRESHOLD,
 * CHARGE_BEAM_THRESHOLD and CHARGE_BEAM_MAX_THRESHOLD. */
#define MZM_CHARGE_START 16
#define MZM_CHARGE_UNCHARGED 25
#define MZM_CHARGE_FULL 64
#define MZM_CHARGE_MAX (MZM_CHARGE_FULL + 15)
#define MZM_PROJECTILE_LIMIT_CHARGED_BEAM 2

/* ParticleChargingBeam: 0 none, 1 beginning, 2 charged. */
typedef struct {
    uint8_t stage;
    uint8_t frame_counter;
} MzmChargeParticle;

/* BombStage (the morph ball launcher stages are not ported). */
typedef enum {
    MZM_BOMB_INIT,
    MZM_BOMB_FIRST_SPIN,
    MZM_BOMB_FAST_SPIN,
    MZM_BOMB_EXPLODING
} MzmBombStage;

typedef enum {
    MZM_WEAPON_NONE,
    MZM_WEAPON_MISSILE,
    MZM_WEAPON_SUPER_MISSILE,
    MZM_WEAPON_POWER_BOMB
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
    /* Bombs: their stage, sBombOam_Fast, PROJ_STATUS_NOT_DRAWN, and the
     * ProjectileCheckHittingSprite pass of the explosion (natively run
     * before the projectile loop, on the frame after the explosion sets
     * PROJ_STATUS_CAN_AFFECT_ENVIRONMENT). */
    MzmBombStage bomb_stage;
    bool fast, hidden, hits_sprites;
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
    /* beamReleasePaletteTimer: four frames after each beam shot. */
    uint8_t release_palette_timer;
    MzmHighlightedWeapon highlighted;
    bool super_missiles_selected;
    bool pending;
    MzmProjectileType pending_type;     /* MZM_PROJECTILE_BEAM / _CHARGED_BEAM: the category */
    /* chargeCounter and the charging particle. */
    uint8_t charge_counter;
    MzmChargeParticle charge_particle;
    /* ProjectileSetBeamParticleEffect this update: the shot's aim and side. */
    bool shot_flash;
    MzmAim shot_flash_aim;
    int shot_flash_facing;
    /* Wall impacts of the last update, in order: where the game calls
     * ParticleSet with the projectile's hitting effect. */
    struct { int32_t x, y; MzmProjectileType type; } impacts[MZM_MAX_PROJECTILES];
    int impact_count;
    /* gPreviousXPosition, set by the caller before mzm_weapons_update. */
    int32_t samus_previous_x;
    /* The SamusBombBounce direction of the last update (0: none), for
     * mzm_samus_bomb_bounce; a bomb impact (MZM_PROJECTILE_BOMB in
     * `impacts`) is the explosion's PE_BOMB particle. */
    int bomb_bounce;
    /* gCurrentPowerBomb, and a power bomb placed this update
     * (ProjectileProcessPowerBomb clears every sprite's
     * SPRITE_ISFT_POWER_BOMB_STUNNED then). */
    MzmPowerBomb power_bomb;
    bool power_bomb_set;
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
