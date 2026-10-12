/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission weapon selection and projectiles. Function names in comments
 * refer to the pinned mzm decompilation that each routine reproduces. */
#include "mzm_projectiles.h"
#include "mzm_sfx.h"

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

/* SamusSetHighlightedWeapon */
static void set_highlighted_weapon(MzmWeapons *weapons, const MzmSamus *samus,
                                   uint16_t held, uint16_t pressed,
                                   const MzmEquipment *equipment) {
    if (equipment->super_missiles == 0) weapons->super_missiles_selected = false;
    else if (equipment->missiles == 0) weapons->super_missiles_selected = true;
    else if (pressed & MZM_KEY_SELECT) {
        weapons->super_missiles_selected = !weapons->super_missiles_selected;
        mzm_sfx_play(MZM_SOUND_MISSILE_TOGGLE);
    }
    MzmHighlightedWeapon highlighted = MZM_WEAPON_NONE;
    switch (samus->pose) {
    case MZM_POSE_MORPH_BALL:
    case MZM_POSE_ROLLING:
    case MZM_POSE_MORPH_BALL_MIDAIR:
    case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
        if (held & MZM_KEY_R && equipment->power_bombs != 0) highlighted = MZM_WEAPON_POWER_BOMB;
        break;
    case MZM_POSE_HANGING_ON_LEDGE:
    case MZM_POSE_DELAY_BEFORE_SHINESPARKING:
    case MZM_POSE_SHINESPARKING:
    case MZM_POSE_SHINESPARK_COLLISION:
    case MZM_POSE_DELAY_AFTER_SHINESPARKING:
    case MZM_POSE_DELAY_BEFORE_BALLSPARKING:
    case MZM_POSE_BALLSPARKING:
    case MZM_POSE_BALLSPARK_COLLISION:
        break;
    default:
        if (held & MZM_KEY_R) {
            if (!weapons->super_missiles_selected) {
                if (equipment->missiles != 0) highlighted = MZM_WEAPON_MISSILE;
            } else {
                highlighted = MZM_WEAPON_SUPER_MISSILE;
            }
        }
        break;
    }
    /* Armed a new weapon. */
    if (highlighted != MZM_WEAPON_NONE && weapons->highlighted == MZM_WEAPON_NONE)
        mzm_sfx_play(MZM_SOUND_ARMING_WEAPON);
    weapons->highlighted = highlighted;
}

bool mzm_weapons_begin_frame(MzmWeapons *weapons, const MzmSamus *samus,
                             uint16_t held, uint16_t pressed,
                             const MzmEquipment *equipment) {
    if (weapons->cooldown) weapons->cooldown--;
    if (weapons->release_palette_timer) weapons->release_palette_timer--;
    if (equipment->suit == MZM_SUIT_SUITLESS || samus->pose == MZM_POSE_DYING) {
        /* The pistol has its own charge rules and is not implemented. */
        weapons->highlighted = MZM_WEAPON_NONE;
        return false;
    }
    set_highlighted_weapon(weapons, samus, held, pressed, equipment);
    if (samus->pose == MZM_POSE_MORPH_BALL || samus->pose == MZM_POSE_ROLLING ||
        samus->pose == MZM_POSE_MORPH_BALL_MIDAIR) {
        /* Bombs: no hasNewProjectile, so the pose is not interrupted. */
        if (pressed & MZM_KEY_B && weapons->cooldown == 0 && equipment->items & MZM_ITEM_BOMBS &&
            !weapons->pending) {
            weapons->pending = true;
            weapons->pending_type = weapons->highlighted == MZM_WEAPON_POWER_BOMB ?
                MZM_PROJECTILE_POWER_BOMB : MZM_PROJECTILE_BOMB;
        }
        return false;
    }
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
static bool projectile_hits(MzmWeapons *weapons, const MzmCollision *collision,
                            const MzmProjectile *p) {
    if (!point_solid(collision, p->x, p->y)) return false;
    if (weapons->impact_count < MZM_MAX_PROJECTILES) {
        weapons->impacts[weapons->impact_count].x = p->x;
        weapons->impacts[weapons->impact_count].y = p->y;
        weapons->impacts[weapons->impact_count].type = p->type;
        weapons->impact_count++;
    }
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
static void process_beam(MzmWeapons *weapons, MzmProjectile *p, const MzmSamus *samus,
                         const MzmCollision *collision) {
    if (p->stage == MZM_STAGE_MOVING) {
        if (projectile_hits(weapons, collision, p)) { p->active = false; return; }
        projectile_move(p, MZM_QUARTER_BLOCK_SIZE + MZM_PIXEL_SIZE, samus->x_velocity);
    } else if (p->stage == MZM_STAGE_SPAWNING) {
        if (projectile_hits(weapons, collision, p)) { p->active = false; return; }
        p->stage = MZM_STAGE_MOVING;
        projectile_move(p, MZM_QUARTER_BLOCK_SIZE, samus->x_velocity);
    } else {
        initialize(p);
        p->stage = MZM_STAGE_SPAWNING;
        if (projectile_hits(weapons, collision, p)) { p->active = false; return; }
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
        if (projectile_hits(weapons, collision, p)) { p->active = false; return; }
        projectile_move(p, p->timer + (super ? MZM_QUARTER_BLOCK_SIZE - MZM_PIXEL_SIZE :
                                       MZM_EIGHTH_BLOCK_SIZE), samus->x_velocity);
        if (super ? p->timer <= 15 : p->timer < 12) p->timer++;
    } else if (p->stage == MZM_STAGE_SPAWNING) {
        if (projectile_hits(weapons, collision, p)) { p->active = false; return; }
        p->stage = MZM_STAGE_MOVING;
        projectile_move(p, MZM_BLOCK_SIZE * 3 / 4, samus->x_velocity);
    } else {
        initialize(p);
        spend_ammo(weapons, equipment, super);
        p->stage = MZM_STAGE_SPAWNING;
        if (projectile_hits(weapons, collision, p)) p->active = false;
    }
}

static bool point_solid_sprite(const MzmCollision *collision, int32_t x, int32_t y) {
    if (collision->solid_point) return collision->solid_point(collision->context, x, y, 2);
    return collision->blocked(collision->context, (float)(x >> 2), (float)(y >> 2), 1.f, 1.f);
}

/* ProjectileCheckSamusBombBounce (without BUGFIX: ClipdataProcess). */
static void bomb_bounce(MzmWeapons *weapons, const MzmProjectile *p, const MzmSamus *samus,
                        const MzmCollision *collision) {
    uint16_t samus_y = (uint16_t)(samus->y - 1), samus_x = (uint16_t)samus->x;
    if (point_solid_sprite(collision, samus_x, (uint16_t)(samus_y + MZM_HALF_BLOCK_SIZE)) &&
        point_solid_sprite(collision, samus_x,
                           (uint16_t)(samus_y - (MZM_BLOCK_SIZE + MZM_HALF_BLOCK_SIZE))))
        return;
    const MzmBlockHitbox *box = mzm_block_hitbox(mzm_pose_hitbox(samus->pose));
    uint16_t top = (uint16_t)(samus_y + box->top), bottom = samus_y;
    uint16_t left = (uint16_t)(samus_x + box->left), right = (uint16_t)(samus_x + box->right);
    uint16_t py = (uint16_t)p->y, px = (uint16_t)p->x;
    uint16_t ptop = (uint16_t)(py - (MZM_BLOCK_SIZE - MZM_PIXEL_SIZE));
    uint16_t pbottom = (uint16_t)(py + MZM_BLOCK_SIZE * 3 / 4);
    uint16_t pleft = (uint16_t)(px - MZM_BLOCK_SIZE * 3 / 4);
    uint16_t pright = (uint16_t)(px + MZM_BLOCK_SIZE * 3 / 4);
    /* SpriteUtilCheckObjectsTouching */
    if (!(pbottom >= top && ptop < bottom && pright >= left && pleft < right)) return;
    if (samus->invincibility) return;
    uint16_t middle = (uint16_t)(pleft + (pright - pleft) / 2);
    int direction = (uint16_t)(samus_y - MZM_HALF_BLOCK_SIZE) > py ? MZM_BOMB_JUMP_ABOVE : 0;
    if (samus_x < px + (MZM_PIXEL_SIZE + 1) && samus_x > px - (MZM_PIXEL_SIZE + 1))
        direction += MZM_FORCED_BOMB_JUMP_UP;
    else if (middle >= (uint16_t)weapons->samus_previous_x)
        direction += MZM_FORCED_BOMB_JUMP_LEFT;
    else
        direction += MZM_FORCED_BOMB_JUMP_RIGHT;
    /* After a bounce Samus rises in the air: no later bomb bounces her
     * again this frame. */
    if (!weapons->bomb_bounce && mzm_samus_bomb_bounce_allowed(samus, direction))
        weapons->bomb_bounce = direction;
}

/* ProjectileProcessBomb: a slow then a fast spin of 16 frames each, then
 * the explosion (PE_BOMB) whose 16 frames run CAA_BOMB_PISTOL on nine
 * points, one a frame. */
static void process_bomb(MzmWeapons *weapons, MzmProjectile *p, const MzmSamus *samus,
                         const MzmCollision *collision) {
    static const int16_t points[9][2] = {
        /* y, x offsets of the frames timer 15 down to 7 */
        {-MZM_EIGHTH_BLOCK_SIZE, 0},
        {MZM_BLOCK_SIZE - MZM_EIGHTH_BLOCK_SIZE, 0},
        {-MZM_EIGHTH_BLOCK_SIZE, MZM_BLOCK_SIZE * 3 / 4},
        {-MZM_EIGHTH_BLOCK_SIZE, -MZM_BLOCK_SIZE * 3 / 4},
        {MZM_BLOCK_SIZE - MZM_EIGHTH_BLOCK_SIZE, MZM_HALF_BLOCK_SIZE + MZM_PIXEL_SIZE},
        {MZM_BLOCK_SIZE - MZM_EIGHTH_BLOCK_SIZE, -(MZM_HALF_BLOCK_SIZE + MZM_PIXEL_SIZE)},
        {-(MZM_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE), 0},
        {-(MZM_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE), MZM_HALF_BLOCK_SIZE + MZM_PIXEL_SIZE},
        {-(MZM_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE), -(MZM_HALF_BLOCK_SIZE + MZM_PIXEL_SIZE)},
    };
    switch (p->bomb_stage) {
    case MZM_BOMB_INIT:
        p->fast = false;
        p->anim_frame = 0;
        p->anim_counter = 0;
        p->hidden = false;
        p->x_flip = false;
        p->stage = MZM_STAGE_MOVING;    /* drawn from now on */
        p->timer = MZM_BOMB_TIMER;
        p->bomb_stage = MZM_BOMB_FIRST_SPIN;
        mzm_sfx_play(MZM_SOUND_BOMB_SET);
        break;
    case MZM_BOMB_FIRST_SPIN:
        if (--p->timer == 0) {
            p->fast = true;
            p->anim_frame = 0;
            p->anim_counter = 0;
            p->timer = MZM_BOMB_TIMER;
            p->bomb_stage = MZM_BOMB_FAST_SPIN;
        }
        break;
    case MZM_BOMB_FAST_SPIN:
        if (--p->timer == 0) {
            p->timer = MZM_BOMB_TIMER;
            p->bomb_stage = MZM_BOMB_EXPLODING;
            p->hidden = true;
            if (weapons->impact_count < MZM_MAX_PROJECTILES) {
                weapons->impacts[weapons->impact_count].x = p->x;
                weapons->impacts[weapons->impact_count].y = p->y - MZM_QUARTER_BLOCK_SIZE;
                weapons->impacts[weapons->impact_count].type = MZM_PROJECTILE_BOMB;
                weapons->impact_count++;
            }
        }
        break;
    case MZM_BOMB_EXPLODING:
        if (--p->timer == 0) {
            p->active = false;
            break;
        }
        if (p->timer == MZM_BOMB_TIMER - 1) {
            /* The sprite pass of this frame ran before the projectile loop. */
            p->hits_sprites = true;
        }
        if (p->timer >= MZM_BOMB_TIMER - 9) {
            const int16_t *o = points[MZM_BOMB_TIMER - 1 - p->timer];
            if (collision->affect)
                collision->affect(collision->context, (uint16_t)(p->x + o[1]),
                                  (uint16_t)(p->y + o[0]), MZM_DAMAGE_BOMB_PISTOL);
            if (p->timer == MZM_BOMB_TIMER - 1) bomb_bounce(weapons, p, samus, collision);
        }
        break;
    }
}

/* ProjectileProcessPowerBomb: one ammunition, a slow spin of 54 frames and a
 * fast one of 40, then the explosion. */
static void process_power_bomb(MzmWeapons *weapons, MzmProjectile *p, MzmEquipment *equipment) {
    switch (p->bomb_stage) {
    case MZM_BOMB_INIT:
        if (equipment->power_bombs != 0 && --equipment->power_bombs == 0 &&
            weapons->highlighted == MZM_WEAPON_POWER_BOMB)
            weapons->highlighted = MZM_WEAPON_NONE;     /* ^= WH_POWER_BOMB */
        p->fast = false;
        p->anim_frame = 0;
        p->anim_counter = 0;
        p->hidden = false;
        p->x_flip = false;
        p->stage = MZM_STAGE_MOVING;
        p->timer = 54;              /* CONVERT_SECONDS(.9f) */
        p->bomb_stage = MZM_BOMB_FIRST_SPIN;
        weapons->power_bomb_set = true;
        mzm_sfx_play(MZM_SOUND_POWER_BOMB_SET);
        weapons->power_bomb.placed = true;
        break;
    case MZM_BOMB_FIRST_SPIN:
        if (--p->timer == 0) {
            p->fast = true;
            p->anim_frame = 0;
            p->anim_counter = 0;
            p->timer = 40;          /* TWO_THIRD_SECOND */
            p->bomb_stage = MZM_BOMB_FAST_SPIN;
        }
        break;
    default:
        if (--p->timer == 0) {
            mzm_power_bomb_start(&weapons->power_bomb, (uint16_t)p->x, (uint16_t)p->y);
            p->active = false;
        }
        break;
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
    weapons->impact_count = 0;
    weapons->bomb_bounce = 0;
    weapons->power_bomb_set = false;
    if (weapons->pending) {
        static const struct { int limit; uint8_t cooldown; } rules[] = {
            [MZM_PROJECTILE_BEAM] = {MZM_PROJECTILE_LIMIT_BEAM, MZM_BEAM_COOLDOWN},
            [MZM_PROJECTILE_MISSILE] = {MZM_PROJECTILE_LIMIT_MISSILE, MZM_MISSILE_COOLDOWN},
            [MZM_PROJECTILE_SUPER_MISSILE] = {MZM_PROJECTILE_LIMIT_SUPER_MISSILE,
                                              MZM_SUPER_MISSILE_COOLDOWN},
            /* CONVERT_SECONDS(.1f + 1.f / 60) */
            [MZM_PROJECTILE_BOMB] = {MZM_PROJECTILE_LIMIT_BOMB, 7},
            /* CONVERT_SECONDS(.1f) - 1 */
            [MZM_PROJECTILE_POWER_BOMB] = {MZM_PROJECTILE_LIMIT_POWER_BOMB, 5},
        };
        MzmProjectileType type = weapons->pending_type;
        /* gArmCannonX/Y: pixel position plus the arm cannon offset; bombs
         * at Samus's position. */
        int32_t x = ((samus->x >> 2) + cannon_x) * MZM_SUBPIXELS_PER_PIXEL;
        int32_t y = (((samus->y - 1) >> 2) + cannon_y) * MZM_SUBPIXELS_PER_PIXEL;
        if (type == MZM_PROJECTILE_BOMB || type == MZM_PROJECTILE_POWER_BOMB) {
            x = samus->x;
            y = samus->y - 1;
        }
        if (mzm_projectile_count(weapons, type) < rules[type].limit &&
            (type != MZM_PROJECTILE_POWER_BOMB || weapons->power_bomb.state == MZM_PB_NONE) &&
            projectile_spawn(weapons, type, samus, x, y)) {
            weapons->cooldown = rules[type].cooldown;
            /* CONVERT_SECONDS(1.f / 15) for beam shots only. */
            if (type == MZM_PROJECTILE_BEAM) weapons->release_palette_timer = 4;
            /* The power beam alone (no beam upgrade is ported). */
            if (type == MZM_PROJECTILE_BEAM) {
                mzm_sfx_play(MZM_SOUND_BEAM_SHOT);
            } else if (type == MZM_PROJECTILE_MISSILE) {
                mzm_sfx_play(MZM_SOUND_MISSILE_SHOT);
                mzm_sfx_play(MZM_SOUND_MISSILE_THRUST);
            } else if (type == MZM_PROJECTILE_SUPER_MISSILE) {
                mzm_sfx_play(MZM_SOUND_SUPER_MISSILE_SHOT);
                mzm_sfx_play(MZM_SOUND_SUPER_MISSILE_THRUST);
            }
        }
        weapons->pending = false;
    }
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i) {
        MzmProjectile *p = &weapons->list[i];
        if (!p->active) continue;
        if (p->type == MZM_PROJECTILE_BEAM) process_beam(weapons, p, samus, collision);
        else if (p->type == MZM_PROJECTILE_BOMB) process_bomb(weapons, p, samus, collision);
        else if (p->type == MZM_PROJECTILE_POWER_BOMB) process_power_bomb(weapons, p, equipment);
        else process_missile(weapons, p, samus, equipment, collision);
        if (!p->active) continue;
        animate(p, animation);
        /* Bombs are never despawned by distance. */
        if (p->type <= MZM_PROJECTILE_SUPER_MISSILE) check_despawn(p, samus);
    }
}
