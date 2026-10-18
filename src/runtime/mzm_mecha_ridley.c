/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Mecha Ridley (src/sprites_ai/mecha_ridley.c): the native
 * multipart entrance, arena lock, Samus height tracking, attack selection,
 * claw swipe, height transitions, retreat and fireball choreography, plus
 * the arm, neck, eye and missile launcher parts, and the laser, missile
 * and fireball actors. The cover/core damage handoff and the death
 * sequence are not ported yet. */
#include "mzm_sprite_ai.h"
#include "mzm_screen_shake.h"

enum {
    MECHA_CRAWLING_INIT = 0x1,
    MECHA_DELAY_BEFORE_CRAWLING = 0x2,
    MECHA_CRAWLING = 0x3,
    MECHA_DELAY_BEFORE_IDLE = 0x4,
    MECHA_IDLE = 0x9,
    MECHA_CLAW_ATTACK = 0x22,
    MECHA_STANDING_UP = 0x23,
    MECHA_CURLED_UP = 0x25,
    MECHA_RETRACTING = 0x27,
    MECHA_CRAWLING_BACKWARDS = 0x29,
    MECHA_STANDING_FOR_FIREBALLS_INIT = 0x34,
    MECHA_STANDING_FOR_FIREBALLS = 0x35,
    MECHA_OPENING_MOUTH = 0x37,
    MECHA_FIREBALL_ATTACK = 0x39,
    MECHA_CLOSING_MOUTH = 0x3B,
    MECHA_RETRACTING_AFTER_FIREBALLS = 0x3D,
};

enum { SAMUS_LOW, SAMUS_MIDDLE, SAMUS_HIGH };      /* gBossWork.work3 */

enum {                                             /* gBossWork.work4 */
    EYE_IDLE, EYE_BLINKING_INIT, EYE_LASER_ATTACK_INIT, EYE_LASER_ATTACK,
};

enum {
    LASER_FORWARD, LASER_SLIGHTLY_DOWN, LASER_DOWN, LASER_SLIGHTLY_UP, LASER_UP,
};

/* The weapon actors' own poses. */
enum { WEAPON_MOVING = 0x9, MISSILE_HOMING = 0x23, WEAPON_HIT_WALL = 0x44 };

enum {                                             /* gBossWork.work5 */
    LAUNCHER_IDLE, LAUNCHER_ATTACK_INIT, LAUNCHER_OPENING, LAUNCHER_OPENED,
    LAUNCHER_CLOSING,
};

enum {                                             /* gSubSpriteData1.work3 */
    HEALTH_FULL, HEALTH_COVER_DAMAGED, HEALTH_COVER_BROKEN,
    HEALTH_THREE_QUARTERS, HEALTH_HALF, HEALTH_QUARTER,
};

enum { FIREBALL_LOW, FIREBALL_HIGH };

/* CONVERT_SECONDS(2.f + 5.f / 6): the missile attack timer's trigger. */
#define MISSILE_ATTACK_TIME 170

enum {
    PART_RIGHT_ARM,
    PART_EYE,
    PART_HEAD,
    PART_NECK,
    PART_COVER,
    PART_CORE,
    PART_TORSO,
    PART_WAISTBAND,
    PART_LEFT_ARM,
    PART_TAIL,
    PART_MISSILE_LAUNCHER,
    PART_COUNT,

    PART_RIGHT_ARM_IDLE = 0x8,
    PART_LEFT_ARM_IDLE = 0x22,
    PART_NECK_IDLE = 0x24,
    PART_EYE_IDLE = 0x26,
    PART_MISSILE_LAUNCHER_IDLE = 0x28,
    PART_COVER_IDLE = 0x2A,
    PART_HEAD_IDLE = 0x2C,
    PART_IDLE = 0x61,
};

static const MzmMultiSprite *multi(const Ctx *c, const MzmSubSpriteState *state) {
    uint8_t id = state->multi < MZM_MECHA_RIDLEY_MULTI_COUNT ?
                 state->multi : MZM_MECHA_RIDLEY_MULTI_CRAWLING_FORWARD_LOW;
    return &c->tables->mecha_ridley_multi[id];
}

static const int16_t *part_data(const Ctx *c, const MzmSubSpriteState *state,
                                uint8_t part) {
    uint8_t frame = state->frame < MZM_MULTI_FRAMES ? state->frame : 0;
    return multi(c, state)->parts[frame][part < MZM_MULTI_PARTS ? part : 0];
}

static void set_multi(MzmSubSpriteState *state, uint8_t id) {
    state->multi = id;
    state->frame = 0;
    state->adc = 0;
}

/* SpriteUtilUpdateSubSprite1Anim. */
static void update_multi(const Ctx *c, MzmSubSpriteState *state) {
    const MzmMultiSprite *m = multi(c, state);
    if (!m->timers[state->frame]) return;
    state->adc++;
    if (m->timers[state->frame] < state->adc) {
        state->adc = 1;
        state->frame++;
        if (state->frame >= MZM_MULTI_FRAMES || !m->timers[state->frame]) state->frame = 0;
    }
}

/* SpriteUtilHasSubSprite1AnimationEnded, checked before the common update. */
static bool multi_ended(const Ctx *c, const MzmSubSpriteState *state) {
    const MzmMultiSprite *m = multi(c, state);
    unsigned next = state->frame + 1u;
    return m->timers[state->frame] <= state->adc &&
           (next >= MZM_MULTI_FRAMES || !m->timers[next]);
}

/* MechaRidleySyncSubSprites. */
static void sync_part(Ctx *c) {
    MzmSubSpriteState *state = &c->f->globals->sub1;
    const int16_t *part = part_data(c, state, c->s->room_slot);
    if (part[0] >= 0 && part[0] < MZM_MECHA_RIDLEY_ANIMS && c->s->anim != part[0])
        ai_set_anim(c->s, (uint8_t)part[0]);
    c->s->y = (uint16_t)(state->y + part[1]);
    c->s->x = (uint16_t)(state->x + part[2]);
}

static void door_timer(const Ctx *c, int8_t timer) {
    if (c->f->hooks && c->f->hooks->door_unlock_timer)
        c->f->hooks->door_unlock_timer(c->f->hooks->context, timer);
}

static void spawn_parts(Ctx *c, uint16_t y, uint16_t x) {
    MzmSubSpriteState *state = &c->f->globals->sub1;
    state->work4 = (uint8_t)ai_spawn_secondary(c, MZM_SSPRITE_MECHA_RIDLEY_PART,
                                               PART_RIGHT_ARM, c->s->gfx_slot, y, x, 0);
    for (uint8_t part = PART_EYE; part < PART_COUNT; ++part) {
        if (part == PART_CORE) continue;
        int slot = ai_spawn_secondary(c, MZM_SSPRITE_MECHA_RIDLEY_PART, part,
                                      c->s->gfx_slot, y, x, 0);
        if (part == PART_LEFT_ARM && slot >= 0) state->work5 = (uint8_t)slot;
    }
}

/* MechaRidleyInit. */
static void body_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *globals = c->f->globals;
    MzmSubSpriteState *state = &globals->sub1;
    if (ai_check_event(c, MZM_EVENT_MECHA_RIDLEY_KILLED)) {
        s->status = 0;
        return;
    }

    s->x = (uint16_t)(s->x + BLOCK_SIZE);
    globals->boss_work1 = s->y;
    globals->boss_work2 = s->x;
    globals->boss_work3 = globals->boss_work4 = globals->boss_work5 = 0;
    globals->boss_work6 = globals->boss_work7 = globals->boss_work8 = 0;
    globals->boss_work9 = globals->boss_work10 = 0;
    state->y = s->y;
    state->x = (uint16_t)(s->x + BLOCK_SIZE * 11);
    state->work1 = state->work2 = state->work3 = 0;
    set_multi(state, MZM_MECHA_RIDLEY_MULTI_CRAWLING_FORWARD_LOW);

    s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
    s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
    s->draw_order = 10;
    s->frozen_row_offset = 5;
    s->samus_collision = SSC_NONE;
    s->health = c->tables->health[s->sprite_id];
    globals->boss_work9 = s->health;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->y_spawn = 300;
    s->work3 = c->rng;
    s->rotation = 0;
    s->pose = MECHA_CRAWLING_INIT;
    s->room_slot = PART_CORE;
    door_timer(c, 1);
    spawn_parts(c, state->y, state->x);
}

static MzmSprite *part_slot(const Ctx *c, uint8_t slot) {
    return c->f->sprites && slot < c->f->sprite_count ? &c->f->sprites[slot] : NULL;
}

/* MechaRidleyCrawling. */
static void crawl(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    uint16_t target = c->f->globals->boss_work2;
    if (state->x <= target) {
        state->x = target;
        if (multi_ended(c, state)) {
            s->pose = MECHA_DELAY_BEFORE_IDLE;
            set_multi(state, MZM_MECHA_RIDLEY_MULTI_STANDING_LOW);
            s->work0 = 0;
            s->work1 = 32;
            c->f->globals->boss_work4 = 0;
        }
        return;
    }

    MzmSprite *right = part_slot(c, state->work4);
    MzmSprite *left = part_slot(c, state->work5);
    unsigned movement = 0;
    if (right && right->sprite_id == MZM_SSPRITE_MECHA_RIDLEY_PART) {
        if (right->frame == 6) movement += PIXEL_SIZE;
        else if (right->frame == 7) movement += PIXEL_SIZE / 2;
    }
    if (left && left->sprite_id == MZM_SSPRITE_MECHA_RIDLEY_PART) {
        if (left->frame == 0) movement += PIXEL_SIZE;
        else if (left->frame == 1) movement += PIXEL_SIZE / 2;
    }
    uint16_t remaining = (uint16_t)(state->x - target);
    state->x = movement >= remaining ? target : (uint16_t)(state->x - movement);
}

/* MechaRidleyStartBattle: palette DMA/transparency are represented by the
 * native timing; the host renderer already draws baked OBJ palettes. */
static void start_battle(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work1 && --s->work1) return;
    s->work1 = 2;
    if (++s->work0 >= 7) {
        s->pose = MECHA_IDLE;
        mzm_sfx_music_play(MZM_MUSIC_MECHA_RIDLEY_BATTLE, 0);
    }
}

static bool multi_is(const MzmSubSpriteState *state, uint8_t id) {
    return state->multi == id;
}

static void set_part_anim(const Ctx *c, uint8_t slot, uint8_t anim) {
    MzmSprite *part = part_slot(c, slot);
    if (part && part->sprite_id == MZM_SSPRITE_MECHA_RIDLEY_PART) ai_set_anim(part, anim);
}

/* SpriteUtilHasAnimationEnded for one of the arms. */
static bool part_anim_ended(const Ctx *c, uint8_t slot) {
    const MzmSprite *s = part_slot(c, slot);
    if (!s || !c->kind || s->anim >= MZM_SPRITE_MAX_ANIMS) return false;
    unsigned next = s->frame + 1u;
    uint8_t current = s->frame < MZM_SPRITE_MAX_FRAMES ? c->kind->timers[s->anim][s->frame] : 0;
    uint8_t following = next < MZM_SPRITE_MAX_FRAMES ? c->kind->timers[s->anim][next] : 0;
    return current < (uint8_t)(s->adc + 1) && (next >= c->kind->counts[s->anim] || following == 0);
}

/* MechaRidleyUpdateHeight: follows gBossWork.work3; true while changing. */
static bool update_height(Ctx *c) {
    MzmSubSpriteState *state = &c->f->globals->sub1;
    uint16_t samus = c->f->globals->boss_work3;
    if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_LOW)) {
        if (samus == SAMUS_MIDDLE) set_multi(state, MZM_MECHA_RIDLEY_MULTI_LOW_TO_MIDDLE);
        else if (samus == SAMUS_HIGH) set_multi(state, MZM_MECHA_RIDLEY_MULTI_LOW_TO_HIGH);
        else return false;
        return true;
    }
    if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_MIDDLE)) {
        if (samus == SAMUS_HIGH) set_multi(state, MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_HIGH);
        else if (multi_ended(c, state) && samus == SAMUS_LOW)
            set_multi(state, MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_LOW);
        else return false;
        return true;
    }
    if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_HIGH)) {
        if (!multi_ended(c, state)) return false;
        if (samus == SAMUS_LOW) set_multi(state, MZM_MECHA_RIDLEY_MULTI_HIGH_TO_LOW);
        else if (samus == SAMUS_MIDDLE) set_multi(state, MZM_MECHA_RIDLEY_MULTI_HIGH_TO_MIDDLE);
        else return false;
        return true;
    }
    if (!multi_ended(c, state)) return true;
    switch (state->multi) {
    case MZM_MECHA_RIDLEY_MULTI_LOW_TO_MIDDLE:
    case MZM_MECHA_RIDLEY_MULTI_HIGH_TO_MIDDLE:
        state->multi = MZM_MECHA_RIDLEY_MULTI_STANDING_MIDDLE;
        break;
    case MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_HIGH:
    case MZM_MECHA_RIDLEY_MULTI_LOW_TO_HIGH:
        state->multi = MZM_MECHA_RIDLEY_MULTI_STANDING_HIGH;
        break;
    case MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_LOW:
    case MZM_MECHA_RIDLEY_MULTI_HIGH_TO_LOW:
        state->multi = MZM_MECHA_RIDLEY_MULTI_STANDING_LOW;
        break;
    default: break;
    }
    state->frame = 0;
    state->adc = 0;
    return false;
}

/* MechaRidleyCrawlingBackwardsInit. */
static void crawling_backwards_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    c->f->globals->boss_work7 = state->health;
    if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_LOW)) {
        set_part_anim(c, state->work4, MZM_MECHA_RIDLEY_RIGHT_ARM_CRAWLING_BACKWARDS);
        set_part_anim(c, state->work5, MZM_MECHA_RIDLEY_LEFT_ARM_CRAWLING_BACKWARDS);
        state->multi = MZM_MECHA_RIDLEY_MULTI_CRAWLING_BACKWARDS_LOW;
        s->pose = MECHA_CRAWLING_BACKWARDS;
    } else if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_MIDDLE)) {
        state->multi = MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_LOW;
        s->pose = MECHA_RETRACTING;
    } else if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_HIGH)) {
        state->multi = MZM_MECHA_RIDLEY_MULTI_HIGH_TO_LOW;
        s->pose = MECHA_RETRACTING;
    }
    state->frame = 0;
    state->adc = 0;
}

/* MechaRidleyClawAttackInit. */
static void claw_attack_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_MIDDLE)) {
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_HIGH);
        s->work0 = 6;                                  /* CONVERT_SECONDS(.1f) */
    } else if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_LOW)) {
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_LOW_TO_HIGH);
        s->work0 = 12;                                 /* CONVERT_SECONDS(.2f) */
    } else {
        s->work0 = 0;
    }
    set_part_anim(c, state->work5, MZM_MECHA_RIDLEY_LEFT_ARM_SWINGING_AT_CLOSE_GROUND);
    s->pose = MECHA_CLAW_ATTACK;
    s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    c->f->globals->boss_work4 = EYE_BLINKING_INIT;
}

/* MechaRidleyCheckStartFireballAttack: every 64 frames, a claw swipe
 * (half the time, never twice in a row), the fireballs once the cover is
 * damaged, or the laser. */
static bool check_start_fireball_attack(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (s->work3 & 0x3F) return false;
    if (!(s->status & MZM_SPRITE_STATUS_FACING_DOWN) && c->rng < 8) {
        claw_attack_init(c);
        return false;
    }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    const MzmEquipment *eq = c->f->equipment;
    if (g->sub1.work3 == HEALTH_COVER_DAMAGED && g->boss_work6 == 0 &&
        g->boss_work5 == LAUNCHER_IDLE && eq && eq->missiles + eq->super_missiles != 0) {
        s->pose = MECHA_STANDING_FOR_FIREBALLS_INIT;
        return true;
    }
    g->boss_work4 = EYE_LASER_ATTACK_INIT;
    return false;
}

/* MechaRidleyIdle. */
static void idle(Ctx *c) {
    MzmSpriteGlobals *g = c->f->globals;
    if ((update_height(c) || !check_start_fireball_attack(c)) &&
        g->sub1.work2 == MISSILE_ATTACK_TIME)
        g->boss_work5 = LAUNCHER_ATTACK_INIT;
}

/* MechaRidleyClawAttack. */
static void claw_attack(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSubSpriteState *state = &g->sub1;
    uint16_t core_health = g->boss_work9;
    if (s->work0 && --s->work0 == 0) set_multi(state, MZM_MECHA_RIDLEY_MULTI_STANDING_HIGH);

    MzmSprite *left = part_slot(c, state->work5);
    if (left && state->x > g->boss_work2 - BLOCK_SIZE && left->frame >= 4 && left->frame <= 7) {
        if (left->frame == 4 && left->adc == 1) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_CLAW_ATTACK);
        state->x = (uint16_t)(state->x - PIXEL_SIZE);
    }
    if (!part_anim_ended(c, state->work5)) return;
    state->x = (uint16_t)(g->boss_work2 - BLOCK_SIZE);
    set_part_anim(c, state->work5, MZM_MECHA_RIDLEY_LEFT_ARM_TREMBLING);
    if (state->work3 >= HEALTH_COVER_BROKEN) {
        if (state->health < core_health / 2) s->work0 = 40;
        else if (state->health < core_health * 3 / 4) s->work0 = 80;
        else s->work0 = 120;
    } else {
        s->work0 = 160;
    }
    s->pose = MECHA_STANDING_UP;
    g->boss_work7 = state->health;
}

/* MechaRidleyStandingUp. */
static void standing_up(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t left = g->sub1.work5;
    if (s->work0) {
        if (--s->work0 == 0) set_part_anim(c, left, MZM_MECHA_RIDLEY_LEFT_ARM_HOLDING_UP);
        return;
    }
    if (part_anim_ended(c, left)) {
        set_part_anim(c, left, MZM_MECHA_RIDLEY_LEFT_ARM_HELD_UP);
        s->pose = MECHA_CURLED_UP;
    }
    if (!update_height(c) && g->boss_work7 > g->sub1.health) crawling_backwards_init(c);
}

/* MechaRidleyCurledUp. */
static void curled_up(Ctx *c) {
    MzmSpriteGlobals *g = c->f->globals;
    if (!update_height(c)) {
        if (g->boss_work7 > g->sub1.health) crawling_backwards_init(c);
        else if (check_start_fireball_attack(c)) return;
    }
    if (g->sub1.work2 == MISSILE_ATTACK_TIME) g->boss_work5 = LAUNCHER_ATTACK_INIT;
}

/* MechaRidleyRetracting. */
static void retracting(Ctx *c) {
    MzmSubSpriteState *state = &c->f->globals->sub1;
    if (!multi_ended(c, state)) return;
    set_part_anim(c, state->work4, MZM_MECHA_RIDLEY_RIGHT_ARM_CRAWLING_BACKWARDS);
    set_part_anim(c, state->work5, MZM_MECHA_RIDLEY_LEFT_ARM_CRAWLING_BACKWARDS);
    set_multi(state, MZM_MECHA_RIDLEY_MULTI_CRAWLING_BACKWARDS_LOW);
    c->s->pose = MECHA_CRAWLING_BACKWARDS;
    mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_CRAWLING_BACKWARDS);
}

/* MechaRidleyCrawlingBack. */
static void crawling_back(Ctx *c) {
    MzmSubSpriteState *state = &c->f->globals->sub1;
    uint16_t back = c->f->globals->boss_work2;
    if (state->x < back) {
        state->x++;
        return;
    }
    state->x = back;
    if (!multi_ended(c, state)) return;
    set_multi(state, MZM_MECHA_RIDLEY_MULTI_STANDING_LOW);
    c->s->pose = MECHA_IDLE;
    set_part_anim(c, state->work4, MZM_MECHA_RIDLEY_RIGHT_ARM_TREMBLING);
    set_part_anim(c, state->work5, MZM_MECHA_RIDLEY_LEFT_ARM_HELD_UP);
}

/* MechaRidleyStandingForFireballsInit (it falls through to the low check:
 * a middle stance already became MiddleToHigh). */
static void standing_for_fireballs_init(Ctx *c) {
    MzmSubSpriteState *state = &c->f->globals->sub1;
    if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_MIDDLE))
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_HIGH);
    if (multi_is(state, MZM_MECHA_RIDLEY_MULTI_STANDING_LOW))
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_LOW_TO_HIGH);
    c->s->pose = MECHA_STANDING_FOR_FIREBALLS;
}

/* MechaRidleyFireballsAttack: a fireball every 16 frames, alternating low
 * and high, for CONVERT_SECONDS(2.f + 2.f / 15). */
static void fireballs_attack(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    if (!(s->work0 & 15)) {
        ai_spawn_secondary(c, MZM_SSPRITE_MECHA_RIDLEY_FIREBALL,
                           (s->work0 & 16) ? FIREBALL_LOW : FIREBALL_HIGH, s->gfx_slot,
                           (uint16_t)(state->y - BLOCK_SIZE * 4),
                           (uint16_t)(state->x - BLOCK_SIZE), 0);
        mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_SPITTING_FIREBALL);
    }
    if (--s->work0 == 0) {
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_CLOSING_MOUTH_HIGH);
        s->pose = MECHA_CLOSING_MOUTH;
    }
}

/* The fireball choreography's animation-end steps. */
static void fireball_step(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    if (!multi_ended(c, state)) return;
    switch (s->pose) {
    case MECHA_STANDING_FOR_FIREBALLS:
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_OPENING_MOUTH_HIGH);
        s->pose = MECHA_OPENING_MOUTH;
        mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_OPENING_MOUTH);
        break;
    case MECHA_OPENING_MOUTH:
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_SPITTING_FIREBALLS_HIGH);
        s->work0 = 128;
        s->pose = MECHA_FIREBALL_ATTACK;
        break;
    case MECHA_CLOSING_MOUTH:
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_HIGH_TO_LOW);
        s->pose = MECHA_RETRACTING_AFTER_FIREBALLS;
        break;
    case MECHA_RETRACTING_AFTER_FIREBALLS:
        set_multi(state, MZM_MECHA_RIDLEY_MULTI_STANDING_LOW);
        s->pose = state->x == c->f->globals->boss_work2 - BLOCK_SIZE ? MECHA_CURLED_UP : MECHA_IDLE;
        break;
    default: break;
    }
}

/* MechaRidley's Samus height tracking: below its feet is low after two
 * seconds, more than three blocks above is high at once, and between is
 * middle (at once from low, after two seconds from high). */
static void track_samus(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint16_t samus_y = (uint16_t)(c->f->samus_y - 1);
    uint16_t sprite_y = (uint16_t)(g->sub1.y - BLOCK_SIZE);
    if (samus_y > sprite_y) {
        if (s->rotation == 0) {
            if (g->boss_work3 != SAMUS_LOW) s->rotation = 120;
        } else if (--s->rotation == 0) {
            g->boss_work3 = SAMUS_LOW;
        }
    } else if (sprite_y - samus_y > BLOCK_SIZE * 3) {
        g->boss_work3 = SAMUS_HIGH;
    } else if (g->boss_work3 != SAMUS_LOW) {
        if (s->rotation == 0) {
            if (g->boss_work3 != SAMUS_MIDDLE) s->rotation = 120;
        } else if (--s->rotation == 0) {
            g->boss_work3 = SAMUS_MIDDLE;
        }
    } else {
        g->boss_work3 = SAMUS_MIDDLE;
    }
}

static void body(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSubSpriteState *state = &g->sub1;
    if (s->damaged) {
        s->damaged = false;
        ai_sound_onscreen(s, MZM_SOUND_MECHA_RIDLEY_DAMAGED);
    }
    if (s->pose != SPRITE_POSE_UNINITIALIZED) track_samus(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: body_init(c); break;
    case MECHA_CRAWLING_INIT:
        s->pose = MECHA_DELAY_BEFORE_CRAWLING;
        mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_ENTRANCE_CRAWL);
        break;
    case MECHA_DELAY_BEFORE_CRAWLING:
        if (s->y_spawn && --s->y_spawn == 0) s->pose = MECHA_CRAWLING;
        break;
    case MECHA_CRAWLING: crawl(c); break;
    case MECHA_DELAY_BEFORE_IDLE: start_battle(c); break;
    case MECHA_IDLE: idle(c); break;
    case MECHA_CLAW_ATTACK: claw_attack(c); break;
    case MECHA_STANDING_UP: standing_up(c); break;
    case MECHA_CURLED_UP: curled_up(c); break;
    case MECHA_RETRACTING: retracting(c); break;
    case MECHA_CRAWLING_BACKWARDS: crawling_back(c); break;
    case MECHA_STANDING_FOR_FIREBALLS_INIT: standing_for_fireballs_init(c); break;
    case MECHA_STANDING_FOR_FIREBALLS:
    case MECHA_OPENING_MOUTH:
    case MECHA_CLOSING_MOUTH:
    case MECHA_RETRACTING_AFTER_FIREBALLS: fireball_step(c); break;
    case MECHA_FIREBALL_ATTACK: fireballs_attack(c); break;
    default: break;
    }
    if (!(s->status & MZM_SPRITE_STATUS_EXISTS)) return;
    if (state->work3 >= HEALTH_COVER_BROKEN) {
        state->health = s->health;
        uint16_t health = g->boss_work9;
        if (state->health < health / 4) state->work3 = HEALTH_QUARTER;
        else if (state->health < health / 2) state->work3 = HEALTH_HALF;
        else if (state->health < health * 3 / 4) state->work3 = HEALTH_THREE_QUARTERS;
    }
    s->work3++;
    if (s->pose == MECHA_IDLE || s->pose == MECHA_CURLED_UP) state->work2++;
    update_multi(c, state);
    sync_part(c);
}

/* MechaRidleyPartInit. */
static void part_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->health = 1;
    s->pose = PART_IDLE;
    s->samus_collision = SSC_MECHA_RIDLEY;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    switch (s->room_slot) {
    case PART_RIGHT_ARM:
        s->draw_top = 24; s->draw_bottom = 56; s->draw_horizontal = 88;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->immune_to_projectiles = true; s->draw_order = 3; s->pose = PART_RIGHT_ARM_IDLE;
        break;
    case PART_LEFT_ARM:
        s->draw_top = 80; s->draw_bottom = 56; s->draw_horizontal = 112;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->immune_to_projectiles = true; s->draw_order = 13; s->pose = PART_LEFT_ARM_IDLE;
        break;
    case PART_EYE:
        s->draw_top = s->draw_bottom = s->draw_horizontal = 8;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->samus_collision = SSC_NONE; s->draw_order = 4; s->pose = PART_EYE_IDLE;
        s->frozen_row_offset = 1;
        break;
    case PART_HEAD:
        s->draw_top = s->draw_bottom = 40; s->draw_horizontal = 56;
        s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_bottom = BLOCK_SIZE + HALF_BLOCK_SIZE;
        s->hitbox_left = -HALF_BLOCK_SIZE; s->hitbox_right = HALF_BLOCK_SIZE;
        s->immune_to_projectiles = true; s->draw_order = 5; s->pose = PART_HEAD_IDLE;
        s->work0 = s->work1 = s->work2 = s->work3 = 0;
        break;
    case PART_NECK:
        s->draw_top = 48; s->draw_bottom = 32; s->draw_horizontal = 64;
        s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
        s->immune_to_projectiles = true; s->draw_order = 6; s->pose = PART_NECK_IDLE;
        break;
    case PART_COVER:
        s->draw_top = s->draw_bottom = s->draw_horizontal = 16;
        s->hitbox_top = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_bottom = BLOCK_SIZE - QUARTER_BLOCK_SIZE;
        s->hitbox_left = -(BLOCK_SIZE - QUARTER_BLOCK_SIZE); s->hitbox_right = BLOCK_SIZE;
        s->health = c->tables->secondary_health[s->sprite_id];
        c->f->globals->boss_work10 = state->health = c->f->globals->boss_work7 = s->health;
        s->draw_order = 9; s->pose = PART_COVER_IDLE;
        break;
    case PART_TORSO:
        s->draw_top = 40; s->draw_bottom = s->draw_horizontal = 24;
        s->hitbox_top = -BLOCK_SIZE; s->hitbox_bottom = BLOCK_SIZE;
        s->hitbox_left = -QUARTER_BLOCK_SIZE; s->hitbox_right = BLOCK_SIZE;
        s->immune_to_projectiles = true; s->draw_order = 11;
        break;
    case PART_WAISTBAND:
        s->draw_top = s->draw_bottom = s->draw_horizontal = 24;
        s->hitbox_top = s->hitbox_left = -BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = BLOCK_SIZE;
        s->immune_to_projectiles = true; s->draw_order = 12;
        break;
    case PART_TAIL:
        s->draw_top = s->draw_bottom = 16; s->draw_horizontal = 48;
        s->hitbox_top = -BLOCK_SIZE; s->hitbox_bottom = BLOCK_SIZE;
        s->hitbox_left = -(BLOCK_SIZE * 3); s->hitbox_right = BLOCK_SIZE * 3;
        s->immune_to_projectiles = true; s->draw_order = 14;
        break;
    case PART_MISSILE_LAUNCHER:
        s->draw_top = s->draw_bottom = s->draw_horizontal = 32;
        s->hitbox_top = s->hitbox_left = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_bottom = s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE;
        s->immune_to_projectiles = true; s->draw_order = 14;
        s->work2 = s->work3 = 0;
        s->pose = PART_MISSILE_LAUNCHER_IDLE;
        break;
    default: s->status = 0; break;
    }
}

static void hitbox(MzmSprite *s, int top, int bottom, int left, int right) {
    s->hitbox_top = (int16_t)top;
    s->hitbox_bottom = (int16_t)bottom;
    s->hitbox_left = (int16_t)left;
    s->hitbox_right = (int16_t)right;
}

#define B BLOCK_SIZE
#define H HALF_BLOCK_SIZE
#define Q QUARTER_BLOCK_SIZE
#define E EIGHTH_BLOCK_SIZE

/* MechaRidleyPartRightArmIdle: the hitbox follows the arm's animation. */
static void right_arm_idle(Ctx *c, const MzmSprite *body) {
    MzmSprite *s = c->s;
    bool crawl_sound = s->adc == 1 && body && body->pose > 7;
    switch (s->anim) {
    case MZM_MECHA_RIDLEY_RIGHT_ARM_TREMBLING:
        hitbox(s, B * 2, B * 3, -(B * 3 + H + E), B);
        return;
    case MZM_MECHA_RIDLEY_RIGHT_ARM_HOLDING_UP:
        if (s->frame == 0) hitbox(s, B, B * 3, -B * 3, B);
        else if (s->frame <= 4) hitbox(s, H, B * 2, -(B * 2 + H), B);
        else hitbox(s, 0, 0, 0, 0);
        return;
    case MZM_MECHA_RIDLEY_RIGHT_ARM_CRAWLING_FORWARD:
        switch (s->frame) {
        case 0:
            if (crawl_sound) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_RIGHT_ARM_CRAWLING);
            hitbox(s, B, B * 3, -B * 3, B);
            break;
        case 1: hitbox(s, B, B * 3, -(B * 3 + H), B); break;
        case 2: hitbox(s, B, B * 2 + H, -(B * 4 + H), B); break;
        case 3: case 4: case 5: hitbox(s, B * 2, B * 3, -B * 5, B); break;
        case 6: hitbox(s, B * 2, B * 3, -(B * 4 + H), B); break;
        case 7: case 8: case 9: case 10: hitbox(s, B * 2, B * 3, -(B * 3 + H), B); break;
        default: hitbox(s, 0, 0, 0, 0); break;
        }
        return;
    case MZM_MECHA_RIDLEY_RIGHT_ARM_CRAWLING_BACKWARDS:
        switch (s->frame) {
        case 0: case 1: case 2: case 10: hitbox(s, B * 2, B * 3, -(B * 3 + H), B); break;
        case 3: hitbox(s, B * 2, B * 3, -(B * 4 + H), B); break;
        case 4: case 5: case 6: hitbox(s, B * 2, B * 3, -(B * 5 + H), B); break;
        case 7:
            if (crawl_sound) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_RIGHT_ARM_CRAWLING);
            hitbox(s, B, B * 2 + H, -(B * 4 + H), B);
            break;
        case 8: hitbox(s, B, B * 3, -(B * 3 + H), B); break;
        case 9: hitbox(s, B, B * 3, -B * 3, B); break;
        default: hitbox(s, 0, 0, 0, 0); break;
        }
        return;
    default: hitbox(s, 0, 0, 0, 0); return;
    }
}

/* MechaRidleyPartLeftArmIdle: the hitbox and the swipe's sounds and shake. */
static void left_arm_idle(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->anim) {
    case MZM_MECHA_RIDLEY_LEFT_ARM_HELD_UP: hitbox(s, H, B * 2, -(B * 3 + H), B); return;
    case MZM_MECHA_RIDLEY_LEFT_ARM_TREMBLING: hitbox(s, B * 2, B * 3, -B * 5, B); return;
    case MZM_MECHA_RIDLEY_LEFT_ARM_SWINGING_AT_CLOSE_GROUND:
        switch (s->frame) {
        case 0:
            if (s->adc == 1) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_ARM_SWIPE_PREPARING);
            hitbox(s, -B, B, -(B * 2 + H), B);
            break;
        case 1: hitbox(s, -(B * 2 + H), B, -B * 2, B); break;
        case 2: hitbox(s, -(B * 3 + Q), 0, -(B * 2 + H), B); break;
        case 3:
            if (s->adc == 20) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_ARM_SWIPE);
            hitbox(s, -(B * 4 + H), 0, -(B * 2 + H), B);
            break;
        case 4: hitbox(s, -(B * 4 + H), 0, -B * 4, B); break;
        case 5: hitbox(s, -B * 2, 0, -(B * 7 - Q), B); break;
        case 6: hitbox(s, 0, B + Q, -B * 7, B); break;
        case 7:
            if (s->adc == 1) {
                mzm_screen_shake_start_vertical(20, 0x81);
                mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_ARM_SWIPE_HITTING_GROUND);
            }
            /* fallthrough */
        case 8: hitbox(s, B * 2, B * 3, -B * 6, -B * 3); break;
        case 9: hitbox(s, B * 2, B * 3, -(B * 5 + Q), -B * 3); break;
        default: hitbox(s, 0, 0, 0, 0); break;
        }
        return;
    case MZM_MECHA_RIDLEY_LEFT_ARM_HOLDING_UP:
        switch (s->frame) {
        case 0: hitbox(s, B * 2, B * 3, -B * 5, B); break;
        case 1:
            if (s->adc == 8) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_HOLDING_UP_ARM);
            hitbox(s, B * 2, B * 3, -B * 5, B);
            break;
        case 2: hitbox(s, B, B * 3, -B * 4, B); break;
        case 3: case 4: hitbox(s, 0, B * 2, -(B * 3 + H), B); break;
        default: hitbox(s, 0, 0, 0, 0); break;
        }
        return;
    case MZM_MECHA_RIDLEY_LEFT_ARM_CRAWLING_FORWARD:
        switch (s->frame) {
        case 0: hitbox(s, B * 2, B * 3, -B * 4, B); break;
        case 1: case 2: case 3: case 4: hitbox(s, B * 2, B * 3, -(B * 3 + H), B); break;
        case 5: hitbox(s, B, B * 3, -(B * 2 + H), B); break;
        case 6: hitbox(s, B, B * 3, -B * 3, B); break;
        case 7: hitbox(s, B, B * 3, -B * 4, B); break;
        case 8: case 9: case 10: hitbox(s, B * 2, B * 3, -B * 5, H); break;
        default: hitbox(s, 0, 0, 0, 0); break;
        }
        return;
    default: hitbox(s, 0, 0, 0, 0); return;
    }
}

/* One frame-0 neck transition: its sound on the first frame, then the
 * hitboxes of its first and later frames. */
static void neck_transition(MzmSprite *s, uint16_t sound, int t0, int b0, int l0,
                            int t1, int b1, int l1) {
    if (s->frame == 0) {
        if (s->adc == 1) mzm_sfx_play(sound);
        hitbox(s, t0, b0, l0, H);
    } else {
        hitbox(s, t1, b1, l1, H);
    }
}

/* MechaRidleyPartNeckIdle. */
static void neck_idle(Ctx *c) {
    MzmSprite *s = c->s;
    static const int16_t low_high[4][3] = {
        {-B, B, -B * 2}, {-(B + H), Q, -B * 2}, {-B * 2, 0, -(B + H)}, {-(B * 2 + H), 0, -B},
    };
    switch (s->anim) {
    case MZM_MECHA_RIDLEY_NECK_LOW:
    case MZM_MECHA_RIDLEY_NECK_LOW_2: hitbox(s, -H, B + H, -(B + H), H); break;
    case MZM_MECHA_RIDLEY_NECK_MIDDLE:
    case MZM_MECHA_RIDLEY_NECK_MIDDLE_2: hitbox(s, -(B + H), Q, -B * 2, H); break;
    case MZM_MECHA_RIDLEY_NECK_HIGH:
    case MZM_MECHA_RIDLEY_NECK_HIGH_2: hitbox(s, -(B * 2 + H), Q, -B, H); break;
    case MZM_MECHA_RIDLEY_NECK_LOW_TO_MIDDLE:
        neck_transition(s, MZM_SOUND_MECHA_RIDLEY_NECK_MIDDLE_UP_MOVEMENT,
                        -B, B, -B * 2, -(B + H), Q, -B * 2);
        break;
    case MZM_MECHA_RIDLEY_NECK_MIDDLE_TO_HIGH:
        neck_transition(s, MZM_SOUND_MECHA_RIDLEY_NECK_MIDDLE_UP_MOVEMENT,
                        -B * 2, 0, -(B + H), -(B * 2 + H), Q, -B);
        break;
    case MZM_MECHA_RIDLEY_NECK_HIGH_TO_MIDDLE:
        neck_transition(s, MZM_SOUND_MECHA_RIDLEY_NECK_MIDDLE_DOWN_MOVEMENT,
                        -B * 2, 0, -(B + H), -B, Q, -B * 2);
        break;
    case MZM_MECHA_RIDLEY_NECK_MIDDLE_TO_LOW:
        neck_transition(s, MZM_SOUND_MECHA_RIDLEY_NECK_MIDDLE_DOWN_MOVEMENT,
                        -B, B, -B * 2, -H, B + H, -(B + H));
        break;
    case MZM_MECHA_RIDLEY_NECK_LOW_TO_HIGH:
        if (s->frame == 0 && s->adc == 1) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_NECK_LOW_TO_HIGH);
        if (s->frame < 4) hitbox(s, low_high[s->frame][0], low_high[s->frame][1],
                                 low_high[s->frame][2], H);
        else hitbox(s, 0, 0, 0, 0);
        break;
    case MZM_MECHA_RIDLEY_NECK_HIGH_TO_LOW:
        if (s->frame == 0 && s->adc == 1) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_NECK_HIGH_TO_LOW);
        if (s->frame == 3) hitbox(s, -H, B + H, -(B + H), H);
        else if (s->frame < 3) hitbox(s, low_high[2 - s->frame][0], low_high[2 - s->frame][1],
                                      low_high[2 - s->frame][2], H);
        else hitbox(s, 0, 0, 0, 0);
        break;
    default: hitbox(s, 0, 0, 0, 0); break;
    }
}

#undef B
#undef H
#undef Q
#undef E

/* SpriteUtilSyncCurrentSpritePositionWithSubSprite1Position: the part's
 * offset without its animation. */
static void sync_position(Ctx *c) {
    const MzmSubSpriteState *state = &c->f->globals->sub1;
    const int16_t *part = part_data(c, state, c->s->room_slot);
    c->s->y = (uint16_t)(state->y + part[1]);
    c->s->x = (uint16_t)(state->x + part[2]);
}

/* MechaRidleyPartCoverIdle: the cover's health is the boss's until it
 * breaks; below three quarters the fireballs become available. */
static void cover_idle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if ((s->isft & 0x7F) == 16) mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_DAMAGED);
    g->sub1.health = s->health;
    if (g->sub1.health < g->boss_work10 * 3 / 4) g->sub1.work3 = HEALTH_COVER_DAMAGED;
}

/* MechaRidleyPartEyeIdle: the blink; the laser only once the cover is
 * broken. */
static void eye_idle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    switch (g->boss_work4) {
    case EYE_BLINKING_INIT:
        s->anim = MZM_MECHA_RIDLEY_EYE_IDLE;
        s->frame = 1;
        s->adc = 0;
        g->boss_work4 = EYE_IDLE;
        break;
    case EYE_LASER_ATTACK_INIT:
        if (g->sub1.work3 < HEALTH_COVER_BROKEN) {
            g->boss_work4 = EYE_IDLE;
            break;
        }
        ai_set_anim(s, MZM_MECHA_RIDLEY_EYE_GLOWING);
        s->work0 = 40;                                 /* TWO_THIRD_SECOND */
        g->boss_work4 = EYE_LASER_ATTACK;
        break;
    case EYE_LASER_ATTACK:
        if (s->work0) {
            if (--s->work0) break;
            /* Each direction a quarter of the time; never straight up. */
            static const uint8_t anims[4] = {
                MZM_MECHA_RIDLEY_EYE_LASER_FORWARD, MZM_MECHA_RIDLEY_EYE_LASER_DOWN,
                MZM_MECHA_RIDLEY_EYE_LASER_SLIGHTLY_UP, MZM_MECHA_RIDLEY_EYE_LASER_SLIGHTLY_DOWN,
            };
            static const uint8_t directions[4] = {
                LASER_FORWARD, LASER_DOWN, LASER_SLIGHTLY_UP, LASER_SLIGHTLY_DOWN,
            };
            uint8_t quarter = c->rng >= 12 ? 3 : c->rng >= 8 ? 2 : c->rng >= 4 ? 1 : 0;
            ai_set_anim(s, anims[quarter]);
            s->work1 = directions[quarter];
            mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_PREPARING_LASER);
        } else if (ai_animation_ending(c, 1)) {
            uint16_t y = s->y, x = s->x;
            switch (s->work1) {
            case LASER_SLIGHTLY_DOWN: y += BLOCK_SIZE; x -= 0x6C; break;
            case LASER_DOWN: y += BLOCK_SIZE + HALF_BLOCK_SIZE; x -= BLOCK_SIZE + HALF_BLOCK_SIZE; break;
            case LASER_SLIGHTLY_UP: y -= BLOCK_SIZE; x -= 0x6C; break;
            case LASER_UP: y -= BLOCK_SIZE + HALF_BLOCK_SIZE; x -= BLOCK_SIZE + HALF_BLOCK_SIZE; break;
            default: x -= BLOCK_SIZE * 2; break;
            }
            ai_spawn_secondary(c, MZM_SSPRITE_MECHA_RIDLEY_LASER, s->work1, s->gfx_slot, y, x, 0);
            ai_set_anim(s, MZM_MECHA_RIDLEY_EYE_IDLE);
            g->boss_work4 = EYE_IDLE;
        }
        break;
    default: break;
    }
}

/* MechaRidleyLoadMissilesGfx: the next of three missile graphics stages
 * while missiles (and no fireballs) are alive. */
static void load_missiles_gfx(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (g->boss_work8 != 0 || g->boss_work6 == 0) return;
    s->work2++;
    if (++s->work3 > 2) s->work3 = 0;
    g->mecha_weapon_gfx = (uint8_t)(1 + s->work3);
}

/* MechaRidleyLoadFireballsGfx: the next of four fireball stages. */
static void load_fireballs_gfx(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (g->boss_work6 != 0 || g->boss_work8 == 0) return;
    s->work2++;
    if (++s->work3 > 3) s->work3 = 0;
    g->mecha_weapon_gfx = (uint8_t)(1 + 3 + s->work3);
}

/* MechaRidleyPartMissileLauncherIdle: the launcher only opens when the
 * cover is broken, Samus is under 150 energy or out of missiles. */
static void launcher_idle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    const MzmEquipment *eq = c->f->equipment;
    load_missiles_gfx(c);
    switch (g->boss_work5) {
    case LAUNCHER_ATTACK_INIT:
        if (g->sub1.work3 >= HEALTH_COVER_BROKEN ||
            (eq && (eq->energy < 150 || eq->missiles + eq->super_missiles == 0))) {
            ai_set_anim(s, MZM_MECHA_RIDLEY_LAUNCHER_OPENING);
            g->boss_work5 = LAUNCHER_OPENING;
            mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_OPENING_MISSILE_LAUNCHER);
        } else {
            g->boss_work5 = LAUNCHER_IDLE;
        }
        break;
    case LAUNCHER_OPENING:
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_MECHA_RIDLEY_LAUNCHER_OPENED);
            g->boss_work5 = LAUNCHER_OPENED;
            s->work0 = 3;                              /* CONVERT_SECONDS(.05f) */
        }
        break;
    case LAUNCHER_OPENED:
        if (ai_animation_ending(c, 1)) {
            if (--s->work0 == 0) {
                ai_set_anim(s, MZM_MECHA_RIDLEY_LAUNCHER_CLOSING);
                g->boss_work5 = LAUNCHER_CLOSING;
            }
        } else if (s->frame == 1 && s->adc == 1) {
            if (s->work0 == 1)
                ai_spawn_secondary(c, MZM_SSPRITE_MECHA_RIDLEY_MISSILE, 0, s->gfx_slot,
                                   (uint16_t)(s->y - BLOCK_SIZE),
                                   (uint16_t)(s->x - (BLOCK_SIZE - QUARTER_BLOCK_SIZE)), 0);
            else if (s->work0 == 2)
                ai_spawn_secondary(c, MZM_SSPRITE_MECHA_RIDLEY_MISSILE, 1, s->gfx_slot,
                                   (uint16_t)(s->y - (THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE)),
                                   (uint16_t)(s->x - (BLOCK_SIZE - PIXEL_SIZE)), 0);
            else {
                ai_spawn_secondary(c, MZM_SSPRITE_MECHA_RIDLEY_MISSILE, 2, s->gfx_slot,
                                   (uint16_t)(s->y - (THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE)),
                                   (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE)), 0);
                mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_FIRING_MISSILE_LAUNCHER);
            }
        }
        break;
    case LAUNCHER_CLOSING:
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_MECHA_RIDLEY_LAUNCHER_CLOSED);
            g->boss_work5 = LAUNCHER_IDLE;
        }
        break;
    default: break;
    }
}

/* MechaRidleyPart (alive boss): the arms follow the multi-sprite only
 * during the entrance, the eye and launcher animate on their own. */
static void part(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSprite *body = part_slot(c, s->ram_slot);
    bool entrance = !body || body->pose < 8;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        part_init(c);
        if (s->status & MZM_SPRITE_STATUS_EXISTS) sync_part(c);
        break;
    case PART_RIGHT_ARM_IDLE:
        right_arm_idle(c, body);
        if (entrance) sync_part(c);
        else sync_position(c);
        break;
    case PART_LEFT_ARM_IDLE:
        left_arm_idle(c);
        if (entrance) sync_part(c);
        else sync_position(c);
        break;
    case PART_NECK_IDLE: neck_idle(c); sync_part(c); break;
    case PART_EYE_IDLE: eye_idle(c); sync_position(c); break;
    case PART_MISSILE_LAUNCHER_IDLE: launcher_idle(c); sync_position(c); break;
    case PART_COVER_IDLE: cover_idle(c); sync_part(c); break;
    case PART_HEAD_IDLE: load_fireballs_gfx(c); sync_part(c); break;
    default: sync_part(c); break;
    }
}

/* MechaRidleyLaser: flies straight in its direction until it hits a
 * wall. */
static void laser(Ctx *c) {
    MzmSprite *s = c->s;
    enum { SPEED = QUARTER_BLOCK_SIZE + PIXEL_SIZE };
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        static const uint8_t anims[5] = {
            MZM_MECHA_RIDLEY_LASER_FORWARD, MZM_MECHA_RIDLEY_LASER_SLIGHTLY_DOWN,
            MZM_MECHA_RIDLEY_LASER_DOWN, MZM_MECHA_RIDLEY_LASER_SLIGHTLY_UP,
            MZM_MECHA_RIDLEY_LASER_UP,
        };
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 8;
        s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->draw_order = 3;
        s->pose = WEAPON_MOVING;
        ai_set_anim(s, anims[s->room_slot < 5 ? s->room_slot : LASER_FORWARD]);
    }
    switch (s->room_slot) {
    case LASER_SLIGHTLY_DOWN: s->y += SPEED * 3 / 10; s->x -= SPEED * 4 / 5; break;
    case LASER_DOWN: s->y += SPEED * 3 / 4; s->x -= SPEED * 3 / 4; break;
    case LASER_SLIGHTLY_UP: s->y -= SPEED * 3 / 10; s->x -= SPEED * 4 / 5; break;
    case LASER_UP: s->y -= SPEED * 3 / 4; s->x -= SPEED * 3 / 4; break;
    default: s->x -= SPEED; break;
    }
    if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) {
        s->status = 0;
        ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
        mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_LASER_EXPLODING);
    }
}

/* MechaRidleyMissile: rises from the launcher, slowing down, then homes
 * on Samus and turns to face her. */
static void missile(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint16_t samus_y = (uint16_t)(c->f->samus_y - 1), samus_x = c->f->samus_x;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 16;
        s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
        ai_set_anim(s, MZM_MECHA_RIDLEY_MISSILE);
        s->health = c->tables->secondary_health[s->sprite_id];
        s->rotation = 0xA0;
        s->work0 = 30;
        s->pose = WEAPON_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
        s->draw_order = 2;
        g->boss_work6++;
        break;
    case WEAPON_MOVING: {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        uint16_t movement = (uint16_t)(s->work0 / 2);
        if (s->room_slot == 1) movement -= 2;
        else if (s->room_slot == 2) movement += 2;
        s->y = (uint16_t)(s->y - movement);
        s->x = (uint16_t)(s->x - movement);
        if (movement < 3)
            s->rotation = ai_face_target_rotation((uint8_t)s->rotation,
                                                  (uint16_t)(samus_y - BLOCK_SIZE), samus_x,
                                                  s->y, s->x);
        s->work0 = (uint8_t)(s->work0 - 2);
        if (s->work0 < 2) {
            s->pose = MISSILE_HOMING;
            s->work1 = 0;
            s->work2 = 1;
            s->work0 = 0;
            s->work3 = 1;
        }
        break;
    }
    case MISSILE_HOMING: {
        uint16_t aim = s->room_slot == 1 ? BLOCK_SIZE : s->room_slot == 2 ? 0x5C : HALF_BLOCK_SIZE;
        ai_move_towards_target(c, (uint16_t)(samus_y - HALF_BLOCK_SIZE), samus_x, 0x28, 0x28, 2);
        s->rotation = ai_face_target_rotation((uint8_t)s->rotation, (uint16_t)(samus_y - aim),
                                              samus_x, s->y, s->x);
        if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) s->pose = WEAPON_HIT_WALL;
        break;
    }
    case SPRITE_POSE_STOPPED:
        ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_HUGE);
        s->status = 0;
        g->boss_work6--;
        mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_MISSILE_EXPLODING);
        break;
    case WEAPON_HIT_WALL:
        ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
        s->status = 0;
        g->boss_work6--;
        mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_SMALL);
        break;
    default:
        ai_sprite_death_full(c, DEATH_NORMAL, s->y, s->x, true, MZM_PE_SPRITE_EXPLOSION_SMALL);
        g->boss_work6--;
        break;
    }
}

/* MechaRidleyFireball: a straight shot, slightly rising (high) or falling
 * steeply (low), until it hits a wall or Samus. */
static void fireball(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES | MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 0x14;
        s->hitbox_top = s->hitbox_left = -0x1C;
        s->hitbox_bottom = s->hitbox_right = 0x1C;
        ai_set_anim(s, MZM_MECHA_RIDLEY_FIREBALL);
        s->health = c->tables->secondary_health[s->sprite_id];
        s->work0 = 30;
        s->pose = WEAPON_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
        s->draw_order = 2;
        s->rotation = s->room_slot != FIREBALL_LOW ? 0x28 : 0;
        g->boss_work8++;
        break;
    case WEAPON_MOVING:
        if (s->room_slot != FIREBALL_LOW) s->y -= 2;
        else s->y += 12;
        s->x -= 12;
        if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) s->pose = WEAPON_HIT_WALL;
        break;
    case MISSILE_HOMING: break;
    case SPRITE_POSE_STOPPED:
    case WEAPON_HIT_WALL:
        ai_particle_set(c, s->y, s->x, s->pose == SPRITE_POSE_STOPPED ?
                        MZM_PE_SPRITE_EXPLOSION_HUGE : MZM_PE_SPRITE_EXPLOSION_SMALL);
        s->status = 0;
        g->boss_work8--;
        mzm_sfx_play(MZM_SOUND_MECHA_RIDLEY_FIREBALL_EXPLODING);
        break;
    default:
        ai_sprite_death_full(c, DEATH_NORMAL, s->y, s->x, true, MZM_PE_SPRITE_EXPLOSION_SMALL);
        g->boss_work8--;
        break;
    }
}

void ai_mecha_ridley(Ctx *c) {
    if (!c->f->globals) {
        c->s->status = 0;
        return;
    }
    if (!c->s->secondary) body(c);
    else if (c->s->sprite_id == MZM_SSPRITE_MECHA_RIDLEY_PART) part(c);
    else if (c->s->sprite_id == MZM_SSPRITE_MECHA_RIDLEY_LASER) laser(c);
    else if (c->s->sprite_id == MZM_SSPRITE_MECHA_RIDLEY_MISSILE) missile(c);
    else if (c->s->sprite_id == MZM_SSPRITE_MECHA_RIDLEY_FIREBALL) fireball(c);
    else c->s->status = 0;
}
