/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission weapon selection and projectiles. Function names in comments
 * refer to the pinned mzm decompilation that each routine reproduces. */
#include "mzm_projectiles.h"

#include <string.h>

void mzm_weapons_init(MzmWeapons *weapons) {
    memset(weapons, 0, sizeof *weapons);
}

int mzm_projectile_count(const MzmWeapons *weapons, MzmProjectileType type) {
    int count = 0;
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i)
        if (weapons->list[i].active && weapons->list[i].type == type) ++count;
    return count;
}

/* SamusCheckNewProjectile: poses that may fire beams or missiles. */
static bool pose_can_fire(MzmPose pose) {
    switch (pose) {
        case MZM_POSE_RUNNING:
        case MZM_POSE_STANDING:
        case MZM_POSE_TURNING_AROUND:
        case MZM_POSE_SHOOTING:
        case MZM_POSE_CROUCHING:
        case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
        case MZM_POSE_SHOOTING_AND_CROUCHING:
        case MZM_POSE_MIDAIR:
        case MZM_POSE_TURNING_AROUND_MIDAIR:
        case MZM_POSE_LANDING:
        case MZM_POSE_STARTING_SPIN_JUMP:
        case MZM_POSE_SPINNING:
        case MZM_POSE_SPACE_JUMPING:
        case MZM_POSE_SCREW_ATTACKING:
        case MZM_POSE_HANGING_ON_LEDGE:
            return true;
        default:
            return false;
    }
}

/* SamusSetHighlightedWeapon without power bombs. */
static void set_highlighted_weapon(MzmWeapons *weapons, const MzmSamus *samus,
                                   uint16_t held, uint16_t pressed,
                                   const MzmEquipment *equipment) {
    if (equipment->super_missiles == 0) weapons->super_missiles_selected = false;
    else if (equipment->missiles == 0) weapons->super_missiles_selected = true;
    else if (pressed & MZM_KEY_SELECT)
        weapons->super_missiles_selected = !weapons->super_missiles_selected;
    MzmHighlightedWeapon highlighted = MZM_WEAPON_NONE;
    if (!mzm_pose_is_morphed(samus->pose) &&
        samus->pose != MZM_POSE_HANGING_ON_LEDGE && (held & MZM_KEY_R)) {
        if (!weapons->super_missiles_selected) {
            if (equipment->missiles != 0) highlighted = MZM_WEAPON_MISSILE;
        } else {
            highlighted = MZM_WEAPON_SUPER_MISSILE;
        }
    }
    weapons->highlighted = highlighted;
}

bool mzm_weapons_begin_frame(MzmWeapons *weapons, const MzmSamus *samus,
                             uint16_t held, uint16_t pressed,
                             const MzmEquipment *equipment) {
    if (weapons->cooldown) weapons->cooldown--;
    if (equipment->suit == MZM_SUIT_SUITLESS || samus->pose == MZM_POSE_DYING) {
        /* The pistol has its own charge rules and is not implemented. */
        weapons->highlighted = MZM_WEAPON_NONE;
        return false;
    }
    set_highlighted_weapon(weapons, samus, held, pressed, equipment);
    if (!pose_can_fire(samus->pose)) return false;
    /* SamusCheckFireBeamMissile without the charge beam. */
    if (weapons->cooldown || weapons->pending || !(pressed & MZM_KEY_B)) return false;
    weapons->pending = true;
    weapons->pending_type =
        weapons->highlighted == MZM_WEAPON_MISSILE ? MZM_PROJECTILE_MISSILE :
        weapons->highlighted == MZM_WEAPON_SUPER_MISSILE ?
            MZM_PROJECTILE_SUPER_MISSILE : MZM_PROJECTILE_BEAM;
    return true;
}

static bool point_solid(const MzmCollision *collision, int32_t x, int32_t y) {
    if (collision->solid_point) return collision->solid_point(collision->context, x, y, 1);
    return collision->blocked(collision->context, (float)(x >> 2), (float)(y >> 2),
                              1.f, 1.f);
}

/* ProjectileCheckVerticalCollisionAtPosition with the projectile's
 * clipdata-affecting action: solid impacts reach hatches and blocks. */
static bool projectile_hits(const MzmCollision *collision, const MzmProjectile *p) {
    if (!point_solid(collision, p->x, p->y)) return false;
    if (collision->affect)
        collision->affect(collision->context, p->x, p->y,
                          p->type == MZM_PROJECTILE_BEAM ? MZM_DAMAGE_BEAM :
                          p->type == MZM_PROJECTILE_MISSILE ? MZM_DAMAGE_MISSILE :
                          MZM_DAMAGE_SUPER_MISSILE);
    return true;
}

/* ProjectileMove */
static void projectile_move(MzmProjectile *p, int distance, int16_t samus_x_velocity) {
    int sign = p->x_flip ? 1 : -1;
    switch (p->direction) {
        case MZM_AIM_UP:
            p->y -= distance;
            return;
        case MZM_AIM_DOWN:
            p->y += distance;
            return;
        case MZM_AIM_DIAGONAL_UP:
            distance = distance * 7 / 10;
            p->y -= distance;
            p->x += sign * distance;
            break;
        case MZM_AIM_DIAGONAL_DOWN:
            distance = distance * 7 / 10;
            p->y += distance;
            p->x += sign * distance;
            break;
        case MZM_AIM_FORWARD:
            p->x += sign * distance;
            break;
    }
    /* Samus's horizontal velocity is added when moving the same way. */
    if ((p->x_flip && samus_x_velocity > 0) || (!p->x_flip && samus_x_velocity < 0))
        p->x += samus_x_velocity >> 3;
}

/* ProjectileInit */
static bool projectile_spawn(MzmWeapons *weapons, MzmProjectileType type,
                             const MzmSamus *samus, int32_t x, int32_t y) {
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i) {
        MzmProjectile *p = &weapons->list[i];
        if (p->active) continue;
        *p = (MzmProjectile){0};
        p->active = true;
        p->type = type;
        p->x = x;
        p->y = y;
        p->x_flip = samus->facing > 0;
        p->direction = samus->aim;
        p->stage = MZM_STAGE_INIT;
        return true;
    }
    return false;
}

static void initialize(MzmProjectile *p) {
    p->y_flip = p->direction == MZM_AIM_DIAGONAL_DOWN || p->direction == MZM_AIM_DOWN;
    p->anim_frame = 0;
    p->anim_counter = 0;
}

/* ProjectileProcessNormalBeam */
static void process_beam(MzmProjectile *p, const MzmSamus *samus,
                         const MzmCollision *collision) {
    if (p->stage == MZM_STAGE_MOVING) {
        if (projectile_hits(collision, p)) { p->active = false; return; }
        projectile_move(p, MZM_QUARTER_BLOCK_SIZE + MZM_PIXEL_SIZE, samus->x_velocity);
    } else if (p->stage == MZM_STAGE_SPAWNING) {
        if (projectile_hits(collision, p)) { p->active = false; return; }
        p->stage = MZM_STAGE_MOVING;
        projectile_move(p, MZM_QUARTER_BLOCK_SIZE, samus->x_velocity);
    } else {
        initialize(p);
        p->stage = MZM_STAGE_SPAWNING;
        if (projectile_hits(collision, p)) { p->active = false; return; }
    }
    if (++p->timer > MZM_SHORT_BEAM_LIFETIME) p->active = false;
}

/* ProjectileDecrementMissileCounter and its super missile twin. */
static void spend_ammo(MzmWeapons *weapons, MzmEquipment *equipment, bool super) {
    int *count = super ? &equipment->super_missiles : &equipment->missiles;
    if (*count == 0) return;
    if (--*count == 0 && weapons->highlighted ==
        (super ? MZM_WEAPON_SUPER_MISSILE : MZM_WEAPON_MISSILE))
        weapons->highlighted = MZM_WEAPON_NONE;
}

/* ProjectileProcessMissile / ProjectileProcessSuperMissile */
static void process_missile(MzmWeapons *weapons, MzmProjectile *p, const MzmSamus *samus,
                            MzmEquipment *equipment, const MzmCollision *collision) {
    bool super = p->type == MZM_PROJECTILE_SUPER_MISSILE;
    if (p->stage == MZM_STAGE_MOVING) {
        if (projectile_hits(collision, p)) { p->active = false; return; }
        projectile_move(p, p->timer + (super ? MZM_QUARTER_BLOCK_SIZE - MZM_PIXEL_SIZE :
                                       MZM_EIGHTH_BLOCK_SIZE), samus->x_velocity);
        if (super ? p->timer <= 15 : p->timer < 12) p->timer++;
    } else if (p->stage == MZM_STAGE_SPAWNING) {
        if (projectile_hits(collision, p)) { p->active = false; return; }
        p->stage = MZM_STAGE_MOVING;
        projectile_move(p, MZM_BLOCK_SIZE * 3 / 4, samus->x_velocity);
    } else {
        initialize(p);
        spend_ammo(weapons, equipment, super);
        p->stage = MZM_STAGE_SPAWNING;
        if (projectile_hits(collision, p)) p->active = false;
    }
}

/* ProjectileUpdateAnimation */
static void animate(MzmProjectile *p, const MzmProjectileAnimation *animation) {
    uint8_t durations[64];
    int count = animation && animation->durations ?
        animation->durations(animation->context, p, durations, 64) : 0;
    p->anim_counter++;
    if (count <= 0) { p->anim_frame = 0; return; }
    if (p->anim_frame >= count) p->anim_frame = 0;
    if (durations[p->anim_frame] < p->anim_counter) {
        p->anim_counter = 1;
        if (++p->anim_frame >= count) p->anim_frame = 0;
    }
}

/* Distance part of ProjectileCheckDespawn: beams and missiles far from
 * Samus are removed (they are always off-screen at that distance). */
static void check_despawn(MzmProjectile *p, const MzmSamus *samus) {
    int32_t dy = p->y - (samus->y - 1 - (MZM_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE));
    int32_t dx = p->x - samus->x;
    if (dy < 0) dy = -dy;
    if (dx < 0) dx = -dx;
    if (dy > MZM_BLOCK_SIZE * 12 || dx > MZM_BLOCK_SIZE * 10) p->active = false;
}

void mzm_weapons_update(MzmWeapons *weapons, const MzmSamus *samus,
                        MzmEquipment *equipment, int cannon_x, int cannon_y,
                        const MzmCollision *collision,
                        const MzmProjectileAnimation *animation) {
    if (weapons->pending) {
        static const struct { int limit; uint8_t cooldown; } rules[] = {
            [MZM_PROJECTILE_BEAM] = {MZM_PROJECTILE_LIMIT_BEAM, MZM_BEAM_COOLDOWN},
            [MZM_PROJECTILE_MISSILE] = {MZM_PROJECTILE_LIMIT_MISSILE, MZM_MISSILE_COOLDOWN},
            [MZM_PROJECTILE_SUPER_MISSILE] = {MZM_PROJECTILE_LIMIT_SUPER_MISSILE,
                                              MZM_SUPER_MISSILE_COOLDOWN},
        };
        MzmProjectileType type = weapons->pending_type;
        /* gArmCannonX/Y: pixel position plus the arm cannon offset. */
        int32_t x = ((samus->x >> 2) + cannon_x) * MZM_SUBPIXELS_PER_PIXEL;
        int32_t y = (((samus->y - 1) >> 2) + cannon_y) * MZM_SUBPIXELS_PER_PIXEL;
        if (mzm_projectile_count(weapons, type) < rules[type].limit &&
            projectile_spawn(weapons, type, samus, x, y))
            weapons->cooldown = rules[type].cooldown;
        weapons->pending = false;
    }
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i) {
        MzmProjectile *p = &weapons->list[i];
        if (!p->active) continue;
        if (p->type == MZM_PROJECTILE_BEAM) process_beam(p, samus, collision);
        else process_missile(weapons, p, samus, equipment, collision);
        if (!p->active) continue;
        animate(p, animation);
        check_despawn(p, samus);
    }
}
