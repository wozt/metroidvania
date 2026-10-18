/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Baristutes (src/sprites_ai/baristute.c): the Norfair
 * jumpers and the two that lock Kraid's lair until both are killed. They
 * jump or walk toward Samus, walk faster as they get hurt and leave two
 * extra drops. */
#include "mzm_sprite_ai.h"
#include "mzm_screen_shake.h"

enum {
    POSE_JUMP_WARNING_INIT = 0x8,
    POSE_CHECK_WARNING_ENDED = 0x9,
    POSE_IDLE_INIT = 0xE,
    POSE_IDLE = 0xF,
    POSE_FALLING_INIT = 0x1E,
    POSE_FALLING = 0x1F,
    POSE_JUMPING = 0x23,
    POSE_LANDING_INIT = 0x24,
    POSE_LANDING = 0x25,
    POSE_WALKING_INIT = 0x26,
    POSE_WALKING = 0x27,
};

#define JUMPING_X_VELOCITY (EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2)
#define WALKING_SPEED 3                                /* ONE_SUB_PIXEL * 3 */
#define JUMPING_Y_ENTRIES 10                           /* sBaristuteJumpingYVelocity */

static bool kraid_variant(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_BARISTUTE_KRAID_UPPER ||
           s->sprite_id == MZM_PSPRITE_BARISTUTE_KRAID_LOWER;
}

static void door_timer(const Ctx *c, int8_t timer) {
    if (c->f->hooks && c->f->hooks->door_unlock_timer)
        c->f->hooks->door_unlock_timer(c->f->hooks->context, timer);
}

/* BaristuteInit: Kraid's pair is gone once killed, else locks the doors. */
static void init(Ctx *c) {
    MzmSprite *s = c->s;
    if (kraid_variant(s)) {
        if (ai_check_event(c, MZM_EVENT_KRAID_BARISTUTES_KILLED)) {
            s->status = 0;
            return;
        }
        door_timer(c, 1);
    }
    s->pose = POSE_IDLE_INIT;
    ai_set_anim(s, MZM_BARISTUTE_IDLE);
    s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
    s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->hitbox_bottom = 0;
    s->hitbox_left = -(BLOCK_SIZE + QUARTER_BLOCK_SIZE + PIXEL_SIZE);
    s->hitbox_right = BLOCK_SIZE + QUARTER_BLOCK_SIZE + PIXEL_SIZE;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = c->tables->health[s->sprite_id];
    ai_face_samus(c);
}

/* Neither foot on the ground. */
static bool over_air(Ctx *c) {
    MzmSprite *s = c->s;
    return ai_collision_at(c, s->y, (uint16_t)(s->x + s->hitbox_right)) == MZM_COLLISION_AIR &&
           ai_collision_at(c, s->y, (uint16_t)(s->x + s->hitbox_left)) == MZM_COLLISION_AIR;
}

/* BaristuteJumpWarningInit */
static void jump_warning_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = POSE_CHECK_WARNING_ENDED;
    ai_set_anim(s, MZM_BARISTUTE_WARNING);
    ai_sound_onscreen_once(s, MZM_SOUND_BARISTUTE_JUMP_WARNING);
}

/* BaristuteCheckBeforeJumpingAnimEnded */
static void check_warning_ended(Ctx *c) {
    MzmSprite *s = c->s;
    if (over_air(c)) {
        s->pose = POSE_FALLING_INIT;
        return;
    }
    if (ai_animation_ending(c, 1)) {
        s->pose = POSE_JUMPING;
        s->work3 = 0;
        ai_set_anim(s, MZM_BARISTUTE_JUMPING);
        ai_sound_onscreen_once(s, MZM_SOUND_BARISTUTE_JUMPING);
    }
}

/* BaristuteJumping: an arc along sBaristuteJumpingYVelocity (each entry
 * four frames); a wall turns it around and makes it fall. */
static void jumping(Ctx *c) {
    MzmSprite *s = c->s;
    unsigned index = s->work3 / 4u;
    int16_t movement = c->tables->baristute_jumping_y[index < JUMPING_Y_ENTRIES ? index : JUMPING_Y_ENTRIES - 1];
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        if (ai_collision_at(c, (uint16_t)(s->y - QUARTER_BLOCK_SIZE),
                            (uint16_t)(s->x + s->hitbox_right + PIXEL_SIZE)) == MZM_COLLISION_SOLID) {
            s->x = (uint16_t)(s->x - JUMPING_X_VELOCITY);
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = POSE_FALLING_INIT;
            return;
        }
        s->x = (uint16_t)(s->x + JUMPING_X_VELOCITY);
    } else {
        if (ai_collision_at(c, (uint16_t)(s->y - QUARTER_BLOCK_SIZE),
                            (uint16_t)(s->x + s->hitbox_left - PIXEL_SIZE)) == MZM_COLLISION_SOLID) {
            s->x = (uint16_t)(s->x + JUMPING_X_VELOCITY);
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = POSE_FALLING_INIT;
            return;
        }
        s->x = (uint16_t)(s->x - JUMPING_X_VELOCITY);
    }
    s->y = (uint16_t)(s->y + movement);
    if (s->work3 < JUMPING_Y_ENTRIES * 4 - 1) s->work3++;
    if (movement > 0) {
        uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
        if (c->vertical != MZM_COLLISION_AIR) {
            s->y = top;
            s->pose = POSE_LANDING_INIT;
        }
    } else if (ai_collision_at(c, (uint16_t)(s->y + s->hitbox_top), s->x) == MZM_COLLISION_SOLID) {
        s->pose = POSE_FALLING_INIT;
    }
}

/* BaristuteLandingInit: a short screen shake when on screen. */
static void landing_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = POSE_LANDING;
    ai_set_anim(s, MZM_BARISTUTE_LANDING);
    if (s->status & MZM_SPRITE_STATUS_ONSCREEN) {
        mzm_screen_shake_start_vertical(10, 0x81);     /* CONVERT_SECONDS(1.f / 6) */
        mzm_sfx_play_not_playing(MZM_SOUND_BARISTUTE_LANDING);
    }
}

/* BaristuteIdleInit: the random choice between walking and jumping. */
static void idle_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = POSE_IDLE;
    ai_set_anim(s, MZM_BARISTUTE_IDLE);
    s->work1 = c->rng & 3;
}

/* BaristuteIdle: after the idle animation it faces Samus and, with room
 * in front, walks or jumps (Kraid's upper one always walks, its lower one
 * always jumps). */
static void idle(Ctx *c) {
    MzmSprite *s = c->s;
    if (over_air(c)) {
        s->pose = POSE_FALLING_INIT;
        return;
    }
    if (!ai_animation_ending(c, 2)) return;
    ai_face_samus(c);
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    uint16_t front = s->status & MZM_SPRITE_STATUS_FACING_RIGHT
                         ? (uint16_t)(s->x + s->hitbox_right + QUARTER_BLOCK_SIZE)
                         : (uint16_t)(s->x + s->hitbox_left - QUARTER_BLOCK_SIZE);
    if (ai_collision_at(c, (uint16_t)(s->y - QUARTER_BLOCK_SIZE), front) != MZM_COLLISION_AIR) return;
    if (s->sprite_id == MZM_PSPRITE_BARISTUTE_KRAID_UPPER) s->pose = POSE_WALKING_INIT;
    else if (s->sprite_id == MZM_PSPRITE_BARISTUTE_KRAID_LOWER) s->pose = POSE_JUMP_WARNING_INIT;
    else s->pose = s->work1 >= 2 ? POSE_WALKING_INIT : POSE_JUMP_WARNING_INIT;
}

/* BaristuteWalkingInit: the faster walk of its damage rows. */
static void walking_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = POSE_WALKING;
    s->x_spawn = s->x;
    switch (s->absolute_palette_row) {
    case 1:
        ai_set_anim(s, MZM_BARISTUTE_WALKING_FAST);
        s->work2 = WALKING_SPEED * 2;
        break;
    case 2:
        ai_set_anim(s, MZM_BARISTUTE_WALKING_VERY_FAST);
        s->work2 = 8;                                  /* WALKING_SPEED * 2.8f */
        break;
    case 3:
        ai_set_anim(s, MZM_BARISTUTE_WALKING_EXTREMELY_FAST);
        s->work2 = 10;                                 /* WALKING_SPEED * 3.5f */
        break;
    default:
        ai_set_anim(s, MZM_BARISTUTE_WALKING_SLOW);
        s->work2 = WALKING_SPEED;
        break;
    }
}

/* BaristutePlayWalkingSound */
static void walking_sound(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->adc != 1 || (s->frame != 1 && s->frame != 4)) return;
    ai_sound_onscreen_once(s, s->work2 <= WALKING_SPEED * 2 ? MZM_SOUND_BARISTUTE_MOVING_SLOW
                                                            : MZM_SOUND_BARISTUTE_MOVING_FAST);
}

/* BaristuteWalking: six blocks (twelve one time in four) along the
 * ground, stopping at walls and ledges. */
static void walking(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t velocity = s->work2;
    uint16_t distance = BLOCK_SIZE * 6;
    if (s->work1 == 3) distance *= 2;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    uint16_t front = right ? (uint16_t)(s->x + s->hitbox_right + PIXEL_SIZE)
                           : (uint16_t)(s->x + s->hitbox_left - PIXEL_SIZE);
    if (ai_collision_at(c, (uint16_t)(s->y - QUARTER_BLOCK_SIZE), front) == MZM_COLLISION_AIR &&
        ai_collision_at(c, s->y, front) == MZM_COLLISION_SOLID) {
        uint16_t walked = right ? (uint16_t)(s->x - s->x_spawn) : (uint16_t)(s->x_spawn - s->x);
        if (walked > distance) {
            s->pose = POSE_IDLE_INIT;
        } else {
            s->x = (uint16_t)(right ? s->x + velocity : s->x - velocity);
            walking_sound(c);
        }
    } else {
        s->x = (uint16_t)(right ? s->x - velocity : s->x + velocity);
        s->pose = POSE_IDLE_INIT;
    }
}

/* BaristuteFalling: sBaristuteFallingYVelocity, its last entry held. */
static void falling(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = top;
        s->pose = POSE_LANDING_INIT;
        return;
    }
    uint8_t offset = s->work3;
    int16_t velocity = offset < 8 ? c->tables->baristute_falling_y[offset] : SHORT_MAX;
    if (velocity == SHORT_MAX) {
        velocity = c->tables->baristute_falling_y[offset ? offset - 1 : 0];
    } else {
        s->work3 = (uint8_t)(offset + 1);
    }
    s->y = (uint16_t)(s->y + velocity);
}

/* BaristuteDeath: Kraid's pair unlocks the lair once both are dead; a
 * drop brings two more energy or missile drops. */
static void death(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t other = s->sprite_id == MZM_PSPRITE_BARISTUTE_KRAID_UPPER ? MZM_PSPRITE_BARISTUTE_KRAID_LOWER :
                    s->sprite_id == MZM_PSPRITE_BARISTUTE_KRAID_LOWER ? MZM_PSPRITE_BARISTUTE_KRAID_UPPER : 0;
    if (other) {
        int alive = 0;
        for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
            const MzmSprite *o = &c->f->sprites[i];
            if (o->status & MZM_SPRITE_STATUS_EXISTS && !o->secondary && o->sprite_id == other && o->health)
                ++alive;
        }
        if (alive == 0) {
            const MzmSpriteHooks *h = c->f->hooks;
            if (h && h->set_event) h->set_event(h->context, MZM_EVENT_KRAID_BARISTUTES_KILLED);
            door_timer(c, -20);
        }
    }
    uint16_t y = (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE)), x = s->x;
    uint8_t rng = (uint8_t)(c->rng * 2);
    ai_sprite_death_full(c, DEATH_NORMAL, y, x, true, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
    if (s->status == 0) return;
    uint8_t drop = c->rng & 1 ? MZM_PSPRITE_LARGE_ENERGY_DROP : MZM_PSPRITE_MISSILE_DROP;
    ai_spawn_drop_follower(c, drop, s->room_slot, 0, s->ram_slot,
                           (uint16_t)(y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) + rng),
                           (uint16_t)(x + (BLOCK_SIZE + QUARTER_BLOCK_SIZE - PIXEL_SIZE) - rng), 0);
    drop = c->tables->random[c->rng & 31] & 1 ? MZM_PSPRITE_LARGE_ENERGY_DROP : MZM_PSPRITE_MISSILE_DROP;
    ai_spawn_drop_follower(c, drop, s->room_slot, 0, s->ram_slot,
                           (uint16_t)(y + (QUARTER_BLOCK_SIZE + PIXEL_SIZE) - rng),
                           (uint16_t)(x - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE) + rng), 0);
}

void ai_baristute(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->damaged) {
        s->damaged = false;
        ai_sound_onscreen_once(s, MZM_SOUND_BARISTUTE_DAMAGED);
    }
    /* Its health quarters set the damage rows (and the walking speed). */
    if (s->isft & 0x7F) {
        uint16_t spawn = c->tables->health[s->sprite_id], health = s->health;
        if (health <= spawn / 4) s->absolute_palette_row = 3;
        else if (health <= spawn / 2) s->absolute_palette_row = 2;
        else if (health <= spawn * 3 / 4) s->absolute_palette_row = 1;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: init(c); break;
    case POSE_JUMP_WARNING_INIT:
        jump_warning_init(c);
        /* fallthrough */
    case POSE_CHECK_WARNING_ENDED: check_warning_ended(c); break;
    case POSE_JUMPING: jumping(c); break;
    case POSE_LANDING_INIT:
        landing_init(c);
        /* fallthrough */
    case POSE_LANDING:
        if (ai_animation_ending(c, 2)) s->pose = POSE_IDLE_INIT;
        break;
    case POSE_WALKING_INIT:
        walking_init(c);
        /* fallthrough */
    case POSE_WALKING: walking(c); break;
    case POSE_IDLE_INIT:
        idle_init(c);
        /* fallthrough */
    case POSE_IDLE: idle(c); break;
    case POSE_FALLING_INIT:
        s->pose = POSE_FALLING;
        s->work3 = 0;
        ai_set_anim(s, MZM_BARISTUTE_JUMPING);
        /* fallthrough */
    case POSE_FALLING: falling(c); break;
    default: death(c); break;
    }
}
