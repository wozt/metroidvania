/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Tourian defences (src/sprites_ai/zebetite_and_cannon.c):
 * the Zebetites, which heal unless hurt again quickly and leave their room
 * for the next one when destroyed, and the ceiling cannons that sweep and
 * shoot until Mother Brain dies; comments name the pinned decompilation's
 * routines. frozenPaletteRowOffset (1 for both) and the cannons' BG
 * priority are not modelled. */
#include "mzm_sprite_ai.h"

enum {
    ZEBETITE_POSE_IDLE = 0x9,
    ZEBETITE_HEALTH_THRESHOLD = 20,
    ZEBETITE_HEAL_TIMER = 30,                   /* CONVERT_SECONDS(.5f) */
    CANNON_POSE_IDLE = 0x9,
    CANNON_BULLET_POSE_IDLE = 0x9,
    CANNON_AIM_LEFT = 0, CANNON_AIM_LEFT_TO_DOWN_LEFT, CANNON_AIM_DOWN_LEFT,
    CANNON_AIM_DOWN_LEFT_TO_DOWN, CANNON_AIM_DOWN, CANNON_AIM_DOWN_TO_DOWN_RIGHT,
    CANNON_AIM_DOWN_RIGHT, CANNON_AIM_DOWN_RIGHT_TO_RIGHT, CANNON_AIM_RIGHT,
    CANNON_VIEW_EVERYWHERE = 0, CANNON_VIEW_RIGHT, CANNON_VIEW_LEFT,
    CANNON_TURN_DELAY = 66,                     /* CONVERT_SECONDS(1.1f) */
    CANNON_BULLET_SPEED = PIXEL_SIZE + ONE_SUB_PIXEL,
    CANNON_BULLET_SPEED_DIAG = CANNON_BULLET_SPEED - CANNON_BULLET_SPEED / 2,
    /* EventFunction values */
    EVENT_ZEBETITE_ONE = MZM_EVENT_ZEBETITE_ONE_DESTROYED, EVENT_ZEBETITE_TWO,
    EVENT_ZEBETITE_THREE, EVENT_ZEBETITE_FOUR,
};

static void set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, event);
}

/* Zebetite */
static void zebetite(Ctx *c) {
    MzmSprite *s = c->s;
    bool one_and_three = s->sprite_id == MZM_PSPRITE_ZEBETITE_ONE_AND_THREE;
    uint8_t first = one_and_three ? EVENT_ZEBETITE_ONE : EVENT_ZEBETITE_TWO;
    uint8_t second = one_and_three ? EVENT_ZEBETITE_THREE : EVENT_ZEBETITE_FOUR;
    uint16_t max_health = c->tables->health[s->sprite_id];
    uint8_t heal_timer = c->f->difficulty == 0 ? ZEBETITE_HEAL_TIMER * 2 : ZEBETITE_HEAL_TIMER;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        /* The first one destroyed, the room shows the next, 20 blocks left. */
        if (ai_check_event(c, first)) {
            s->x = (uint16_t)(s->x - BLOCK_SIZE * 20);
            if (ai_check_event(c, second)) {
                s->status = 0;
                return;
            }
        }
        s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = -(BLOCK_SIZE * 3);
        s->hitbox_bottom = 0;
        s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_right = HALF_BLOCK_SIZE;
        s->samus_collision = SSC_SOLID_SIDEWAYS;
        ai_set_anim(s, MZM_TOURIAN_ZEBETITE_PHASE1);
        s->draw_order = 5;
        s->health = max_health;
        s->scaling = max_health;
        s->pose = ZEBETITE_POSE_IDLE;
        s->work0 = heal_timer;
    }
    unsigned phase = (unsigned)(max_health - s->health) / ZEBETITE_HEALTH_THRESHOLD;
    if (s->health == s->scaling) {
        /* Not hurt since: it heals a phase every half second. */
        if (phase != 0 && --s->work0 == 0) s->health += ZEBETITE_HEALTH_THRESHOLD;
        return;
    }
    if (s->health < max_health) s->work0 = heal_timer;
    s->scaling = s->health;
    uint8_t anim = s->anim;
    switch (phase) {
    case 0:
        if (anim != MZM_TOURIAN_ZEBETITE_PHASE1) {
            mzm_sfx_play_not_playing(MZM_SOUND_ZEBETITE_HURT_2);
            ai_set_anim(s, MZM_TOURIAN_ZEBETITE_PHASE1);
        }
        break;
    case 1: case 2:
        if (anim != MZM_TOURIAN_ZEBETITE_PHASE2) {
            if (anim == MZM_TOURIAN_ZEBETITE_PHASE1) mzm_sfx_play_not_playing(MZM_SOUND_ZEBETITE_HURT_1);
            else if (anim == MZM_TOURIAN_ZEBETITE_PHASE4) mzm_sfx_play_not_playing(MZM_SOUND_ZEBETITE_HURT_2);
            ai_set_anim(s, MZM_TOURIAN_ZEBETITE_PHASE2);
        }
        break;
    case 3: case 4:
        if (anim != MZM_TOURIAN_ZEBETITE_PHASE4) {
            if (anim == MZM_TOURIAN_ZEBETITE_PHASE2) mzm_sfx_play_not_playing(MZM_SOUND_ZEBETITE_HURT_1);
            else if (anim == MZM_TOURIAN_ZEBETITE_PHASE6) mzm_sfx_play_not_playing(MZM_SOUND_ZEBETITE_HURT_2);
            ai_set_anim(s, MZM_TOURIAN_ZEBETITE_PHASE4);
        }
        break;
    case 5: case 6:
        if (anim != MZM_TOURIAN_ZEBETITE_PHASE6) {
            mzm_sfx_play_not_playing(MZM_SOUND_ZEBETITE_HURT_1);
            ai_set_anim(s, MZM_TOURIAN_ZEBETITE_PHASE6);
        }
        break;
    default:
        /* Destroyed: the explosion, then its event; the first one comes
         * back as the second. */
        ai_particle_set(c, (uint16_t)(s->y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), s->x,
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        if (!ai_check_event(c, first)) {
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
            s->pose = SPRITE_POSE_UNINITIALIZED;
            set_event(c, first);
        } else {
            s->status = 0;
            set_event(c, second);
        }
        break;
    }
}

/* CannonCheckSurroundings: a wall on one side sets that side's view, else
 * a random direction; a random delay before the next turn. */
static void cannon_surroundings(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t rng = c->rng;
    s->work0 = (uint8_t)(rng * 8 + 10);
    s->adc = s->frame = 0;
    if (ai_collision_at(c, s->y, (uint16_t)(s->x + BLOCK_SIZE)) != MZM_COLLISION_AIR) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
        s->work1 = CANNON_AIM_DOWN_LEFT;
        s->work3 = CANNON_VIEW_LEFT;
        s->anim = MZM_TOURIAN_CANNON_AIMING_DOWN_LEFT;
    } else if (ai_collision_at(c, s->y, (uint16_t)(s->x - BLOCK_SIZE)) != MZM_COLLISION_AIR) {
        s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        s->work1 = CANNON_AIM_DOWN_RIGHT;
        s->work3 = CANNON_VIEW_RIGHT;
        s->anim = MZM_TOURIAN_CANNON_AIMING_DOWN_RIGHT;
    } else {
        if (rng & 1) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
        else s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        if (rng >= 8) {
            s->work1 = CANNON_AIM_DOWN_RIGHT;
            s->anim = MZM_TOURIAN_CANNON_AIMING_DOWN_RIGHT;
        } else {
            s->work1 = CANNON_AIM_DOWN_LEFT;
            s->anim = MZM_TOURIAN_CANNON_AIMING_DOWN_LEFT;
        }
        s->work3 = CANNON_VIEW_EVERYWHERE;
    }
}

/* The aiming and shooting tables of each aim (the aims are even) and the
 * transition between an aim and the next. */
static uint8_t aiming(int aim) { return (uint8_t)(MZM_TOURIAN_CANNON_AIMING_LEFT + aim / 2 * 3); }
static uint8_t shooting(int aim) { return (uint8_t)(aiming(aim) + 1); }
static uint8_t transition(int aim) { return (uint8_t)(MZM_TOURIAN_CANNON_AIMING_LEFT + aim / 2 * 3 + 2); }

/* The SPRITE_RNG_PROB thresholds an arrival at each aim shoots above. */
static uint8_t shoot_threshold(int aim) {
    switch (aim) {
    case CANNON_AIM_LEFT: case CANNON_AIM_RIGHT: return 10;     /* .625 */
    case CANNON_AIM_DOWN: return 4;                             /* .25 */
    default: return 2;                                          /* .125 */
    }
}

/* CannonFireBullet: on the shooting table's second frame, on screen. */
static void cannon_fire(Ctx *c) {
    MzmSprite *s = c->s;
    if (!(s->status & MZM_SPRITE_STATUS_ONSCREEN)) return;
    if (s->frame == 1 && s->adc == 1)
        ai_spawn_secondary(c, MZM_SSPRITE_CANNON_BULLET, s->work1, s->gfx_slot, s->y, s->x, 0);
}

/* The cannons that sit 20 (24) blocks apart in their room: past the range
 * on one side, the cannon moves to its other place (Cannon3 a block
 * higher there). */
static void cannon_place(Ctx *c) {
    MzmSprite *s = c->s;
    int span = s->sprite_id == MZM_PSPRITE_CANNON ? 20 : 24;
    uint16_t range = (uint16_t)(s->sprite_id == MZM_PSPRITE_CANNON ? BLOCK_SIZE * 10 : BLOCK_SIZE * 12);
    bool raised = s->sprite_id == MZM_PSPRITE_CANNON3;
    uint16_t spawn_y = s->y_spawn, spawn_x = s->x_spawn, samus_x = c->f->samus_x;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        uint16_t other = (uint16_t)(spawn_x - BLOCK_SIZE * span);
        if (other < samus_x && samus_x - other > range) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            s->x = s->x_spawn;
            if (raised) s->y = s->y_spawn;
            cannon_surroundings(c);
        } else {
            s->x = other;
            if (raised) s->y = (uint16_t)(spawn_y - BLOCK_SIZE);
        }
    } else if (samus_x < spawn_x && spawn_x - samus_x > range) {
        s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        s->x = (uint16_t)(spawn_x - BLOCK_SIZE * span);
        if (raised) s->y = (uint16_t)(spawn_y - BLOCK_SIZE);
        cannon_surroundings(c);
    } else {
        s->x = spawn_x;
        if (raised) s->y = spawn_y;
    }
}

/* Cannon: sweeps from left to right and back through its aims, a delay at
 * each, shooting on arrival by chance. */
static void cannon(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (ai_check_event(c, MZM_EVENT_MOTHER_BRAIN_KILLED)) {
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN) ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
        s->status = 0;
        return;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->samus_collision = SSC_NONE;
        s->draw_order = 5;
        s->pose = CANNON_POSE_IDLE;
        s->y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
        cannon_surroundings(c);
        s->y_spawn = s->y;
        s->x_spawn = s->x;
        return;
    }
    cannon_place(c);
    int aim = s->work1;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    if (aim % 2 == 0) {
        /* An aim: the shot ends, then after the delay the next transition
         * (the down aim turns unless it sees everywhere; the ends turn
         * back). */
        if (s->anim == shooting(aim)) {
            if (ai_animation_ending(c, 1)) ai_set_anim(s, aiming(aim));
            else cannon_fire(c);
        }
        if (--s->work0 != 0) return;
        s->work0 = CANNON_TURN_DELAY;
        s->adc = s->frame = 0;
        if (aim == CANNON_AIM_LEFT) {
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            right = true;
        } else if (aim == CANNON_AIM_RIGHT) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            right = false;
        } else if (aim == CANNON_AIM_DOWN && s->work3 != CANNON_VIEW_EVERYWHERE) {
            s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
            right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
        }
        if (right) {
            s->work1 = (uint8_t)(aim + 1);
            s->anim = transition(aim);
        } else {
            s->work1 = (uint8_t)(aim - 1);
            s->anim = transition(aim - 2);
        }
        return;
    }
    /* A transition: at its end, the next aim, shooting by chance. */
    if (!ai_animation_ending(c, 1)) return;
    int next = right ? aim + 1 : aim - 1;
    s->anim = c->rng > shoot_threshold(next) ? shooting(next) : aiming(next);
    s->work1 = (uint8_t)next;
    s->adc = s->frame = 0;
}

/* CannonBullet: from the cannon's mouth along its aim, until a block. */
static void cannon_bullet(Ctx *c) {
    MzmSprite *s = c->s;
    if (ai_check_event(c, MZM_EVENT_MOTHER_BRAIN_KILLED)) {
        s->status = 0;
        return;
    }
    static const int8_t direction[9][2] = {
        [CANNON_AIM_LEFT] = {-1, 0}, [CANNON_AIM_DOWN_LEFT] = {-1, 1}, [CANNON_AIM_DOWN] = {0, 1},
        [CANNON_AIM_DOWN_RIGHT] = {1, 1}, [CANNON_AIM_RIGHT] = {1, 0},
    };
    uint8_t aim = s->room_slot;
    bool valid = aim <= CANNON_AIM_RIGHT && aim % 2 == 0;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        if (!valid) {
            s->status = 0;
            return;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->kill_off_screen = true;
        s->draw_order = 2;
        s->health = 0;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
        s->pose = CANNON_BULLET_POSE_IDLE;
        s->adc = s->frame = 0;
        s->anim = (uint8_t)(MZM_TOURIAN_CANNON_BULLET_LEFT + aim / 2);
        /* The mouth: a block straight, three quarters diagonally. */
        {
            int straight = direction[aim][0] == 0 || direction[aim][1] == 0;
            int reach = straight ? BLOCK_SIZE : THREE_QUARTER_BLOCK_SIZE;
            s->x = (uint16_t)(s->x + direction[aim][0] * reach);
            s->y = (uint16_t)(s->y + direction[aim][1] * reach);
        }
        break;
    case CANNON_BULLET_POSE_IDLE: {
        if (ai_collision_at(c, s->y, s->x) == MZM_COLLISION_SOLID) {
            s->pose = SPRITE_POSE_STOPPED;
            s->samus_collision = SSC_NONE;
            return;
        }
        bool diagonal = direction[aim][0] != 0 && direction[aim][1] != 0;
        int speed = diagonal ? CANNON_BULLET_SPEED_DIAG : CANNON_BULLET_SPEED;
        s->x = (uint16_t)(s->x + direction[aim][0] * speed);
        s->y = (uint16_t)(s->y + direction[aim][1] * speed);
        break;
    }
    default:
        ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_SMALL);
        s->status = 0;
        break;
    }
}

void ai_tourian(Ctx *c) {
    if (c->s->secondary) cannon_bullet(c);
    else if (c->s->sprite_id == MZM_PSPRITE_ZEBETITE_ONE_AND_THREE ||
             c->s->sprite_id == MZM_PSPRITE_ZEBETITE_TWO_AND_FOUR) zebetite(c);
    else cannon(c);
}
