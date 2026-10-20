/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Atomics (src/sprites_ai/atomic.c): they hover around their
 * spawn, flee Samus, charge (flashing palette rows 2 and 3) and shoot their
 * electricity at her, and chase her arm cannon while she charges a beam;
 * comments name the pinned decompilation's routines. The exploding
 * electricity's BG priority is not modelled. */
#include "mzm_sprite_ai.h"

enum {
    ATOMIC_POSE_IDLE_INIT = 0x8,
    ATOMIC_POSE_IDLE = 0x9,
    ATOMIC_POSE_MOVE = 0x23,
    ATOMIC_POSE_MOVE_BACK_IDLE = 0x25,
    ATOMIC_POSE_CHASING_SAMUS_INIT = 0x26,
    ATOMIC_POSE_CHASING_SAMUS = 0x27,
    ATOMIC_ELECTRICITY_POSE_SPAWN = 0x8,
    ATOMIC_ELECTRICITY_POSE_MOVE = 0x9,
    ATOMIC_ELECTRICITY_POSE_EXPLODING = 0x23,
    ATOMIC_ELECTRICITY_POSE_ON_GROUND = 0x25,
    ATOMIC_DIRECTION_DIAGONAL = 0,
    ATOMIC_DIRECTION_HORIZONTAL = 1,
    ATOMIC_DIRECTION_VERTICAL = 2,
    /* CONVERT_SECONDS(3.f) + ONE_THIRD_SECOND */
    ATOMIC_CHARGE_DELAY = 200,
    CHARGE_BEAM_THRESHOLD = 64,
    PIXEL_POSITION_FLAG = 0xFFFC,
};

/* sAtomicDynamicPaletteData: palette row and duration; mild flashing,
 * then from 16 the intense flashing, then the end. */
static const uint8_t dynamic_palette[33][2] = {
    {0, 4}, {2, 4}, {0, 4}, {2, 4}, {0, 4}, {2, 4}, {0, 4}, {2, 4},
    {3, 4}, {2, 4}, {3, 4}, {2, 4}, {3, 4}, {2, 4}, {3, 4}, {2, 4},
    {3, 2}, {2, 2}, {3, 2}, {2, 2}, {3, 2}, {2, 2}, {3, 2}, {2, 2},
    {3, 2}, {2, 2}, {3, 2}, {2, 2}, {3, 2}, {2, 2}, {3, 2}, {2, 2},
    {0, 0},
};

/* The Atomic's work variables (ATOMIC_* in atomic.c). */
#define horizontal_deceleration work1
#define horizontal_chase work2
#define vertical_deceleration work0
#define vertical_chase work3
#define palette_offset work1
#define idle_x_offset work2
#define idle_y_offset work3
#define move_timer work0
#define move_speed work3
#define charge_delay scaling
#define charge_palette_timer rotation

static bool air(const Ctx *c, uint16_t y, uint16_t x) {
    return ai_collision_at(c, y, x) == MZM_COLLISION_AIR;
}

/* One axis of AtomicSmoothMovement: accelerate towards the arm cannon, then
 * slow down past it and turn. */
static bool smooth_axis(uint16_t *position, uint8_t *deceleration, uint8_t *chase, bool forward,
                        bool blocked, uint16_t destination) {
    const unsigned limit = (PIXEL_SIZE + PIXEL_SIZE / 4) * PIXEL_SIZE;   /* PIXEL_TO_SUB_PIXEL */
    if (blocked) return true;
    if (*deceleration != 0) {
        /* Slow to a stable position */
        *deceleration -= PIXEL_SIZE / 4;
        if (*deceleration == 0) return true;
        *position = (uint16_t)(forward ? *position + (*deceleration >> 2) : *position - (*deceleration >> 2));
        return false;
    }
    bool short_of = forward ? *position <= destination - PIXEL_SIZE : !(*position < destination + PIXEL_SIZE);
    if (!short_of) {
        *deceleration = *chase;
        return false;
    }
    if (*chase < limit) *chase += PIXEL_SIZE / 4;
    *position = (uint16_t)(forward ? *position + (*chase >> 2) : *position - (*chase >> 2));
    return false;
}

/* AtomicSmoothMovement: towards gArmCannonY / gArmCannonX. */
static void smooth_movement(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = s->y, x = s->x;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    bool down = s->status & MZM_SPRITE_STATUS_FACING_DOWN;
    uint16_t side = (uint16_t)(right ? x + HALF_BLOCK_SIZE : x - HALF_BLOCK_SIZE);
    bool blocked_x = !air(c, y, side) || !air(c, (uint16_t)(y + HALF_BLOCK_SIZE), side) ||
                     !air(c, (uint16_t)(y - HALF_BLOCK_SIZE), side);
    uint16_t row = (uint16_t)(down ? y + HALF_BLOCK_SIZE : y - HALF_BLOCK_SIZE);
    bool blocked_y = !air(c, row, x) || !air(c, row, (uint16_t)(x + HALF_BLOCK_SIZE)) ||
                     !air(c, row, (uint16_t)(x - HALF_BLOCK_SIZE));
    if (smooth_axis(&s->x, &s->horizontal_deceleration, &s->horizontal_chase, right, blocked_x,
                    c->f->arm_cannon_x)) {
        s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
        s->horizontal_chase = 1;
    }
    if (smooth_axis(&s->y, &s->vertical_deceleration, &s->vertical_chase, down, blocked_y,
                    c->f->arm_cannon_y)) {
        s->status ^= MZM_SPRITE_STATUS_FACING_DOWN;
        s->vertical_chase = 1;
    }
}

/* AtomicUpdateDirectionToFleeSamus: within five blocks, away from her. */
static void flee_samus(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t samus_y = (uint16_t)(c->f->samus_y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), samus_x = c->f->samus_x;
    const int range = BLOCK_SIZE * 5;
    if (!(s->y > samus_y - range && s->y < samus_y + range && s->x > samus_x - range &&
          s->x < samus_x + range))
        return;
    if (s->y > samus_y) s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    if (s->x > samus_x) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    s->pose = ATOMIC_POSE_MOVE;
    s->move_speed = PIXEL_SIZE;
    s->move_timer = 0;
}

/* AtomicCheckShootElectricity: the charge delay spawns the electricity,
 * then the palette flashes through sAtomicDynamicPaletteData. */
static void check_shoot(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->charge_delay != 0) {
        if (--s->charge_delay == 0) {
            s->palette_offset = 0;
            s->charge_palette_timer = 1;
            ai_spawn_secondary(c, MZM_SSPRITE_ATOMIC_ELECTRICITY, s->room_slot, s->gfx_slot, s->y, s->x, 0);
        }
        return;
    }
    if (--s->charge_palette_timer != 0) return;
    uint8_t offset = s->palette_offset++;
    if (offset >= 33) offset = 32;
    s->absolute_palette_row = s->palette_row = dynamic_palette[offset][0];
    if (dynamic_palette[offset][1] != 0) {
        s->charge_palette_timer = dynamic_palette[offset][1];
        if (offset == 16 && s->status & MZM_SPRITE_STATUS_ONSCREEN)
            mzm_sfx_play_not_playing(MZM_SOUND_ATOMIC_CHARGE_SILENT);
    } else {
        s->charge_delay = ATOMIC_CHARGE_DELAY;
    }
}

/* AtomicMove and AtomicMaybeMoveBackToIdle's step along each axis: a
 * collision stops (or, fleeing, turns) the move. */
static void step_axes(Ctx *c, uint16_t movement, bool turn) {
    MzmSprite *s = c->s;
    uint16_t y = s->y, x = s->x;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (air(c, (uint16_t)(y + HALF_BLOCK_SIZE), x)) s->y = (uint16_t)(s->y + movement);
        else if (turn) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    } else {
        if (air(c, (uint16_t)(y - HALF_BLOCK_SIZE), x)) s->y = (uint16_t)(s->y - movement);
        else if (turn) s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        if (air(c, y, (uint16_t)(x + HALF_BLOCK_SIZE))) s->x = (uint16_t)(s->x + movement);
        else if (turn) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    } else {
        if (air(c, y, (uint16_t)(x - HALF_BLOCK_SIZE))) s->x = (uint16_t)(s->x - movement);
        else if (turn) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    }
}

/* A table step of AtomicIdle (SHORT_MAX restarts it). */
static int16_t idle_step(const int16_t *table, int size, uint8_t *offset) {
    int16_t movement = *offset < size ? table[*offset] : SHORT_MAX;
    uint8_t next = *offset;
    if (movement == SHORT_MAX) {
        movement = table[0];
        next = 0;
    }
    *offset = (uint8_t)(next + 1);
    return movement;
}

static void atomic(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if ((s->isft & 0x7F) > ai_stun_threshold(c) && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    bool charging = c->f->charge_counter >= CHARGE_BEAM_THRESHOLD;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* AtomicInit: none on easy. */
        if (c->f->difficulty == 0) {
            s->status = 0;
            return;
        }
        s->draw_top = s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE - QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
        ai_set_anim(s, MZM_ATOMIC_IDLE);
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS;
        s->pose = ATOMIC_POSE_IDLE_INIT;
        s->y_spawn = s->y;
        s->x_spawn = s->x;
        s->charge_delay = (uint16_t)(c->rng * 16);
        break;
    case ATOMIC_POSE_IDLE_INIT:
        s->pose = ATOMIC_POSE_IDLE;
        ai_set_anim(s, MZM_ATOMIC_IDLE);
        s->idle_y_offset = s->idle_x_offset = 0;
        break;
    case ATOMIC_POSE_IDLE:
        if (charging) {
            s->pose = ATOMIC_POSE_CHASING_SAMUS_INIT;
            return;
        }
        s->y = (uint16_t)(s->y + idle_step(c->tables->atomic_idle_y, 65, &s->idle_y_offset));
        s->x = (uint16_t)(s->x + idle_step(c->tables->atomic_idle_x, 81, &s->idle_x_offset));
        flee_samus(c);
        check_shoot(c);
        break;
    case ATOMIC_POSE_MOVE:
        /* AtomicMove: fleeing, slowing down every 16 frames. */
        if (charging) {
            s->pose = ATOMIC_POSE_CHASING_SAMUS_INIT;
            return;
        }
        if (++s->move_timer > 80) {             /* CONVERT_SECONDS(1.f) + ONE_THIRD_SECOND */
            s->pose = ATOMIC_POSE_MOVE_BACK_IDLE;
            return;
        }
        if (s->move_timer % 16 == 0 && s->move_speed != 0) s->move_speed--;
        step_axes(c, s->move_speed, true);
        flee_samus(c);
        check_shoot(c);
        break;
    case ATOMIC_POSE_MOVE_BACK_IDLE: {
        /* AtomicMaybeMoveBackToIdle: back to the spawn, by half pixels. */
        if (charging) {
            s->pose = ATOMIC_POSE_CHASING_SAMUS_INIT;
            return;
        }
        uint16_t y_spawn = s->y_spawn & PIXEL_POSITION_FLAG, x_spawn = s->x_spawn & PIXEL_POSITION_FLAG;
        uint16_t y = s->y & PIXEL_POSITION_FLAG, x = s->x & PIXEL_POSITION_FLAG;
        if (y_spawn < y) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        else if (y_spawn > y) s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        if (x_spawn < x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
        else if (x_spawn > x) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        step_axes(c, PIXEL_SIZE / 2, false);
        if ((s->y & PIXEL_POSITION_FLAG) == y_spawn && (s->x & PIXEL_POSITION_FLAG) == x_spawn) {
            s->pose = ATOMIC_POSE_IDLE_INIT;
        } else {
            flee_samus(c);
            check_shoot(c);
        }
        break;
    }
    case ATOMIC_POSE_CHASING_SAMUS_INIT:
        s->pose = ATOMIC_POSE_CHASING_SAMUS;
        s->absolute_palette_row = s->palette_row = 0;
        s->horizontal_deceleration = 0;
        s->horizontal_chase = PIXEL_SIZE / 4;
        s->vertical_deceleration = 0;
        s->vertical_chase = PIXEL_SIZE / 4;
        /* fallthrough */
    case ATOMIC_POSE_CHASING_SAMUS:
        /* AtomicChaseSamus: until the charge is released. */
        if (c->f->charge_counter == 0) {
            s->pose = ATOMIC_POSE_IDLE_INIT;
            if (s->charge_delay == 0) {
                s->palette_offset = 0;
                s->charge_palette_timer = 1;
            }
        } else {
            smooth_movement(c);
        }
        break;
    default:
        ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        break;
    }
}

/* AtomicElectricityCheckAtomicDead */
static bool atomic_dead(const Ctx *c, uint8_t slot) {
    const MzmSprite *a = c->f->sprites ? &c->f->sprites[slot] : NULL;
    return !a || !(a->status & MZM_SPRITE_STATUS_EXISTS) || a->secondary || a->sprite_id != MZM_PSPRITE_ATOMIC;
}

/* AtomicElectricitySpawn: following the Atomic until its charge ends, then
 * shot horizontally, vertically or diagonally at Samus. */
static void electricity_spawn(Ctx *c) {
    MzmSprite *s = c->s;
    if (atomic_dead(c, s->ram_slot)) {
        s->status = 0;
        return;
    }
    const MzmSprite *a = &c->f->sprites[s->ram_slot];
    s->y = a->y;
    s->x = a->x;
    if (a->charge_delay != ATOMIC_CHARGE_DELAY) return;
    if (s->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play_not_playing(MZM_SOUND_ATOMIC_ELECTRICITY_SHOOTING);
    s->adc = s->frame = 0;
    s->samus_collision = SSC_HURTS_SAMUS_A_LOT;
    s->pose = ATOMIC_ELECTRICITY_POSE_MOVE;
    uint16_t samus_y = (uint16_t)(c->f->samus_y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), samus_x = c->f->samus_x;
    if (s->x <= samus_x) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    if (s->y > samus_y) s->status |= MZM_SPRITE_STATUS_Y_FLIP;
    else s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    bool flip_x = s->status & MZM_SPRITE_STATUS_X_FLIP, down = s->status & MZM_SPRITE_STATUS_FACING_DOWN;
    if (s->y + BLOCK_SIZE > samus_y && s->y - BLOCK_SIZE < samus_y) {
        s->anim = MZM_ATOMIC_ELECTRICITY_MOVING_HORIZONTAL;
        s->work1 = ATOMIC_DIRECTION_HORIZONTAL;
        s->hitbox_top = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_bottom = HALF_BLOCK_SIZE - PIXEL_SIZE;
        s->hitbox_left = flip_x ? 0 : -(2 * BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_right = flip_x ? 2 * BLOCK_SIZE + HALF_BLOCK_SIZE : 0;
    } else if (s->x + BLOCK_SIZE > samus_x && s->x - BLOCK_SIZE < samus_x) {
        s->anim = MZM_ATOMIC_ELECTRICITY_MOVING_VERTICAL;
        s->work1 = ATOMIC_DIRECTION_VERTICAL;
        s->hitbox_left = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_right = HALF_BLOCK_SIZE - PIXEL_SIZE;
        s->hitbox_top = down ? 0 : -(2 * BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_bottom = down ? 2 * BLOCK_SIZE + HALF_BLOCK_SIZE : 0;
    } else {
        /* (the diagonal hitboxes have their top below their bottom, as in
         * the game) */
        s->anim = MZM_ATOMIC_ELECTRICITY_MOVING_DIAGONAL;
        s->work1 = ATOMIC_DIRECTION_DIAGONAL;
        s->hitbox_left = flip_x ? HALF_BLOCK_SIZE : -(BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_right = flip_x ? BLOCK_SIZE + HALF_BLOCK_SIZE : -HALF_BLOCK_SIZE;
        s->hitbox_top = down ? BLOCK_SIZE + HALF_BLOCK_SIZE : -HALF_BLOCK_SIZE;
        s->hitbox_bottom = down ? HALF_BLOCK_SIZE : -(BLOCK_SIZE + HALF_BLOCK_SIZE);
    }
}

/* AtomicElectricity */
static void electricity(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* AtomicElectricityInit */
        if (atomic_dead(c, s->ram_slot)) {
            s->status = 0;
            return;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 3 * BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        ai_set_anim(s, MZM_ATOMIC_ELECTRICITY_CHARGING);
        s->draw_order = 3;
        s->samus_collision = SSC_NONE;
        s->pose = ATOMIC_ELECTRICITY_POSE_SPAWN;
        break;
    case ATOMIC_ELECTRICITY_POSE_SPAWN:
        electricity_spawn(c);
        break;
    case ATOMIC_ELECTRICITY_POSE_MOVE: {
        /* AtomicElectricityMove: until a block, then the explosion. */
        const uint16_t speed = QUARTER_BLOCK_SIZE + PIXEL_SIZE;
        if (!(s->work1 & ATOMIC_DIRECTION_HORIZONTAL))
            s->y = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_DOWN ? s->y + speed : s->y - speed);
        if (!(s->work1 & ATOMIC_DIRECTION_VERTICAL))
            s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? s->x + speed : s->x - speed);
        if (!air(c, s->y, s->x)) {
            s->pose = ATOMIC_ELECTRICITY_POSE_EXPLODING;
            s->hitbox_top = s->hitbox_left = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
            s->hitbox_bottom = s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE;
            s->ignore_samus_timer = 1;
            ai_set_anim(s, s->work1 != ATOMIC_DIRECTION_DIAGONAL ? MZM_ATOMIC_ELECTRICITY_EXPLODING_NON_DIAGONAL
                                                                 : MZM_ATOMIC_ELECTRICITY_EXPLODING_DIAGONAL);
            mzm_sfx_play(MZM_SOUND_ATOMIC_ELECTRICITY_EXPLODING);
        }
        break;
    }
    case ATOMIC_ELECTRICITY_POSE_EXPLODING:
        if (s->frame < 4) s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 1)) {
            /* (ATOMIC_POSE_MOVE_BACK_IDLE, which is the on-ground pose) */
            s->pose = ATOMIC_ELECTRICITY_POSE_ON_GROUND;
            s->hitbox_top = s->hitbox_left = -QUARTER_BLOCK_SIZE;
            s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE;
            s->samus_collision = SSC_HURTS_SAMUS;
            ai_set_anim(s, MZM_ATOMIC_ELECTRICITY_CHARGING);
        }
        break;
    case ATOMIC_ELECTRICITY_POSE_ON_GROUND:
        if (ai_animation_ending(c, 1)) s->status = 0;
        break;
    default:
        break;
    }
}

void ai_atomic(Ctx *c) {
    if (c->s->secondary) electricity(c);
    else atomic(c);
}
