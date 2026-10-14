/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Worker Robots (src/sprites_ai/worker_robot.c): solid
 * robots, immune to projectiles, that sleep until something hits them, then
 * walk until Samus or a ledge stops them; a missile on their side pushes
 * them back. Comments name the pinned decompilation's routines. */
#include "mzm_projectiles.h"
#include "mzm_sprite_ai.h"

enum {
    WORKER_ROBOT_POSE_WALKING_INIT = 0x8,
    WORKER_ROBOT_POSE_WALKING = 0x9,
    WORKER_ROBOT_POSE_BACK_TO_SLEEP_INIT = 0xA,
    WORKER_ROBOT_POSE_BACK_TO_SLEEP = 0xB,
    WORKER_ROBOT_POSE_TURNING_AROUND = 0xC,
    WORKER_ROBOT_POSE_CHECK_TURNING_AROUND_ENDED = 0xD,
    WORKER_ROBOT_POSE_STANDING_INIT = 0xE,
    WORKER_ROBOT_POSE_STANDING = 0xF,
    WORKER_ROBOT_POSE_SLEEPING_INIT = 0x10,
    WORKER_ROBOT_POSE_SLEEPING = 0x11,
    WORKER_ROBOT_POSE_WAKING_UP_INIT = 0x12,
    WORKER_ROBOT_POSE_WAKING_UP = 0x13,
    WORKER_ROBOT_POSE_FALLING_INIT = 0x1E,
    WORKER_ROBOT_POSE_FALLING = 0x1F,
    WORKER_ROBOT_POSE_FALLING_SLEEPING_INIT = 0x20,
    WORKER_ROBOT_POSE_FALLING_SLEEPING = 0x21,
};

static bool touching(uint16_t top1, uint16_t bottom1, uint16_t left1, uint16_t right1,
                     uint16_t top2, uint16_t bottom2, uint16_t left2, uint16_t right2) {
    /* SpriteUtilCheckObjectsTouching */
    return bottom2 >= top1 && top2 < bottom1 && right2 >= left1 && left2 < right1;
}

static void set_pose_anim(MzmSprite *s, uint8_t pose, uint8_t anim) {
    s->pose = pose;
    ai_set_anim(s, anim);
}

/* WorkerRobotCheckSamusInFront */
static bool samus_in_front(const Ctx *c) {
    const MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_ON_TOP) return false;
    uint16_t y = s->y, x = s->x, samus_y = c->f->samus_y, samus_x = c->f->samus_x;
    if (!touching((uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE + PIXEL_SIZE)), y,
                  (uint16_t)(x - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                  (uint16_t)(x + (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                  (uint16_t)(samus_y + c->f->samus_hitbox_top),
                  (uint16_t)(samus_y + c->f->samus_hitbox_bottom),
                  (uint16_t)(samus_x + c->f->samus_hitbox_left),
                  (uint16_t)(samus_x + c->f->samus_hitbox_right)))
        return false;
    return s->status & MZM_SPRITE_STATUS_X_FLIP ? x < samus_x : x > samus_x;
}

/* WorkerRobotWalkingDetectProjectile: the first missile or super missile
 * touching the robot is removed with its impact; from the side, it pushes
 * the robot away (work1: the push's duration). */
static void detect_projectile(Ctx *c) {
    MzmSprite *s = c->s;
    MzmWeapons *weapons = c->f->weapons;
    if (!weapons) return;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i) {
        MzmProjectile *p = &weapons->list[i];
        /* PROJ_STATUS_CAN_AFFECT_ENVIRONMENT: not a tumbling missile. */
        if (!p->active || p->stage == MZM_STAGE_TUMBLING) continue;
        if (p->type != MZM_PROJECTILE_MISSILE && p->type != MZM_PROJECTILE_SUPER_MISSILE) continue;
        int16_t ptop, pbottom, pleft, pright;
        mzm_projectile_hitbox(p->type, p->direction, weapons->items, &ptop, &pbottom, &pleft, &pright);
        uint16_t py = (uint16_t)p->y, px = (uint16_t)p->x;
        if (!touching(top, bottom, left, right, (uint16_t)(py + ptop), (uint16_t)(py + pbottom),
                      (uint16_t)(px + pleft), (uint16_t)(px + pright)))
            continue;
        bool on_side = p->direction == MZM_AIM_FORWARD ||
                       ((p->direction == MZM_AIM_DIAGONAL_UP || p->direction == MZM_AIM_DIAGONAL_DOWN) &&
                        py > top && py < bottom);
        if (on_side) {
            bool flipped = s->status & MZM_SPRITE_STATUS_X_FLIP;
            if (p->x_flip) {
                px = left;
                s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
                s->anim = flipped ? MZM_WORKER_ROBOT_WALKING : MZM_WORKER_ROBOT_WALKING_BACKWARDS;
            } else {
                px = right;
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
                s->anim = flipped ? MZM_WORKER_ROBOT_WALKING_BACKWARDS : MZM_WORKER_ROBOT_WALKING;
            }
            s->adc = 0;
        }
        if (p->type == MZM_PROJECTILE_SUPER_MISSILE) {
            ai_particle_set(c, py, px, MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE);
            if (on_side) s->work1 = 60;                 /* CONVERT_SECONDS(1.f) */
        } else {
            ai_particle_set(c, py, px, MZM_PE_HITTING_SOMETHING_WITH_MISSILE);
            if (on_side) s->work1 = 30;                 /* CONVERT_SECONDS(.5f) */
        }
        p->active = false;
        break;
    }
}

/* The robot's facing once a push ends: back to the side it looks at. */
static void face_forward(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
}

/* WorkerRobotWalking */
static void walking(Ctx *c) {
    MzmSprite *s = c->s;
    ai_align_on_slope(c);
    if (c->vertical == MZM_COLLISION_AIR &&
        ai_collision_at(c, s->y, (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR &&
        ai_collision_at(c, s->y, (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR) {
        s->pose = WORKER_ROBOT_POSE_FALLING_INIT;
        if (s->work1 == 0) return;
        s->work1 = 0;
        face_forward(s);
        return;
    }
    uint16_t movement = ONE_SUB_PIXEL;
    detect_projectile(c);
    if (s->work1 != 0) {
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN && (s->frame & 3) == 3 && s->adc == 6)
            mzm_sfx_play_not_playing(MZM_SOUND_WORKER_ROBOT_FOOTSTEPS);
        /* Five times the animation speed. */
        s->adc = (uint8_t)(s->adc + 4);
        movement = s->work1 / 4;
        if (movement > EIGHTH_BLOCK_SIZE) movement = EIGHTH_BLOCK_SIZE;
        else if (movement == 0) movement = ONE_SUB_PIXEL;
        if (--s->work1 == 0) {
            face_forward(s);
            s->pose = WORKER_ROBOT_POSE_STANDING_INIT;
            return;
        }
    } else {
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN && (s->frame & 3) == 3 && s->adc == 8)
            mzm_sfx_play_not_playing(MZM_SOUND_WORKER_ROBOT_FOOTSTEPS);
        if (samus_in_front(c)) {
            s->pose = WORKER_ROBOT_POSE_BACK_TO_SLEEP_INIT;
            return;
        }
    }
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    int side = right ? 1 : -1;
    if (c->vertical & MZM_COLLISION_FLAGS_F0) {
        /* On a slope: a ledge or a wall ahead ends the walk. */
        if (s->work1 == 0 &&
            ai_collision_at(c, s->y, (uint16_t)(s->x + side * HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR) {
            s->pose = WORKER_ROBOT_POSE_BACK_TO_SLEEP_INIT;
            return;
        }
        if (ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                            (uint16_t)(s->x + side * (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE))) ==
            MZM_COLLISION_SOLID) {
            if (s->work1 == 0) s->pose = WORKER_ROBOT_POSE_BACK_TO_SLEEP_INIT;
            return;
        }
    }
    s->x = (uint16_t)(s->x + side * movement);
    MzmSpriteSamusLink *link = c->f->samus_link;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_ON_TOP && link) {
        link->x = (uint16_t)(link->x + side * movement);
        link->moved = true;
    }
}

/* WorkerRobotFalling / WorkerRobotFallingSleep: the falling table until a
 * floor, then standing (sleeping) with the landing sound. */
static void fall(Ctx *c, const int16_t *table, int length, uint8_t landed_pose) {
    MzmSprite *s = c->s;
    uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = top;
        s->pose = landed_pose;
        mzm_sfx_play(MZM_SOUND_WORKER_ROBOT_LANDING);
        return;
    }
    uint8_t offset = s->work3;
    int16_t movement = offset < length ? table[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = table[offset - 1];
    } else {
        s->work3++;
    }
    s->y = (uint16_t)(s->y + movement);
}

/* WorkerRobot */
static void worker_robot(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* WorkerRobotInit */
        s->hitbox_top = -(BLOCK_SIZE + 3 * QUARTER_BLOCK_SIZE + PIXEL_SIZE);
        s->hitbox_bottom = 0;
        s->hitbox_left = -(EIGHTH_BLOCK_SIZE * 3 + PIXEL_SIZE);
        s->hitbox_right = EIGHTH_BLOCK_SIZE * 3 + PIXEL_SIZE;
        s->draw_top = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        ai_set_anim(s, MZM_WORKER_ROBOT_SLEEPING);
        s->immune_to_projectiles = true;
        s->work1 = 0;
        s->samus_collision = SSC_SOLID;
        s->health = c->tables->health[s->sprite_id];
        /* SpriteUtilMakeSpriteFaceAwayFromSamusXFlip */
        if (s->x > c->f->samus_x) s->status |= MZM_SPRITE_STATUS_X_FLIP;
        else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        /* fallthrough */
    case WORKER_ROBOT_POSE_SLEEPING_INIT:
        set_pose_anim(s, WORKER_ROBOT_POSE_SLEEPING, MZM_WORKER_ROBOT_SLEEPING);
        /* fallthrough */
    case WORKER_ROBOT_POSE_SLEEPING:
        /* WorkerRobotSleeping: a hit (its stun flash) wakes it. */
        if (ai_collision_at(c, s->y, s->x) == MZM_COLLISION_AIR)
            s->pose = WORKER_ROBOT_POSE_FALLING_SLEEPING_INIT;
        else if (s->isft & 0x7F)
            s->pose = WORKER_ROBOT_POSE_WAKING_UP_INIT;
        break;
    case WORKER_ROBOT_POSE_WAKING_UP_INIT:
        set_pose_anim(s, WORKER_ROBOT_POSE_WAKING_UP, MZM_WORKER_ROBOT_WAKING_UP);
        ai_sound_onscreen_once(s, MZM_SOUND_WORKER_ROBOT_WAKING_UP);
        /* fallthrough */
    case WORKER_ROBOT_POSE_WAKING_UP:
        if (ai_animation_ending(c, 2)) s->pose = WORKER_ROBOT_POSE_STANDING_INIT;
        break;
    case WORKER_ROBOT_POSE_STANDING_INIT:
        set_pose_anim(s, WORKER_ROBOT_POSE_STANDING, MZM_WORKER_ROBOT_STANDING);
        s->work0 = 30;                                  /* CONVERT_SECONDS(.5f) */
        s->work1 = 0;
        s->hitbox_top = -(BLOCK_SIZE * 2 + PIXEL_SIZE);
        /* fallthrough */
    case WORKER_ROBOT_POSE_STANDING:
        detect_projectile(c);
        if (s->work1 != 0) s->pose = WORKER_ROBOT_POSE_WALKING;
        else if (--s->work0 == 0) s->pose = WORKER_ROBOT_POSE_WALKING_INIT;
        break;
    case WORKER_ROBOT_POSE_WALKING_INIT:
        set_pose_anim(s, WORKER_ROBOT_POSE_WALKING, MZM_WORKER_ROBOT_WALKING);
        /* fallthrough */
    case WORKER_ROBOT_POSE_WALKING:
        walking(c);
        break;
    case WORKER_ROBOT_POSE_BACK_TO_SLEEP_INIT:
        set_pose_anim(s, WORKER_ROBOT_POSE_BACK_TO_SLEEP, MZM_WORKER_ROBOT_BACK_TO_SLEEP);
        ai_sound_onscreen_once(s, MZM_SOUND_WORKER_ROBOT_FALLING_ASLEEP);
        /* fallthrough */
    case WORKER_ROBOT_POSE_BACK_TO_SLEEP:
        if (ai_animation_ending(c, 1)) {
            set_pose_anim(s, WORKER_ROBOT_POSE_TURNING_AROUND, MZM_WORKER_ROBOT_GOING_TO_SLEEP);
            s->hitbox_top = -(BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE);
        }
        break;
    case WORKER_ROBOT_POSE_TURNING_AROUND:
        /* WorkerRobotTurningAroundInit: it lies down facing the other way. */
        if (ai_animation_ending(c, 1)) {
            if (s->status & MZM_SPRITE_STATUS_X_FLIP)
                s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_RIGHT);
            else
                s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_RIGHT;
            set_pose_anim(s, WORKER_ROBOT_POSE_CHECK_TURNING_AROUND_ENDED, MZM_WORKER_ROBOT_TURNING_AROUND);
        }
        break;
    case WORKER_ROBOT_POSE_CHECK_TURNING_AROUND_ENDED:
        if (ai_animation_ending(c, 2)) s->pose = WORKER_ROBOT_POSE_SLEEPING_INIT;
        break;
    case WORKER_ROBOT_POSE_FALLING_INIT:
        set_pose_anim(s, WORKER_ROBOT_POSE_FALLING, MZM_WORKER_ROBOT_WALKING);
        s->work3 = 0;
        s->work1 = 0;
        /* fallthrough */
    case WORKER_ROBOT_POSE_FALLING:
        /* Three times the animation speed. */
        s->adc = (uint8_t)(s->adc + 2);
        fall(c, c->tables->falling, 16, WORKER_ROBOT_POSE_STANDING_INIT);
        break;
    case WORKER_ROBOT_POSE_FALLING_SLEEPING_INIT:
        s->pose = WORKER_ROBOT_POSE_FALLING_SLEEPING;
        s->work3 = 0;
        s->work1 = 0;
        /* fallthrough */
    case WORKER_ROBOT_POSE_FALLING_SLEEPING:
        fall(c, c->tables->worker_robot_sleeping_falling, 10, WORKER_ROBOT_POSE_SLEEPING_INIT);
        break;
    default:
        ai_sprite_death(c, (uint16_t)(s->y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE - PIXEL_SIZE / 2)), s->x,
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        break;
    }
}

void ai_worker_robot(Ctx *c) { worker_robot(c); }
