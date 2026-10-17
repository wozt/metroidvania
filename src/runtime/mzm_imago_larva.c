/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Imago larva actors (src/sprites_ai/imago_larva.c and
 * imago_larva_right_side.c): the
 * left larva starts the encounter and the right one blocks the passage,
 * retreats when its shell is hit, attacks after each stun, and completes
 * the Caterpillar event when killed. Both bodies drive five secondary
 * parts through one of the native multi-sprite tables. */
#include <limits.h>

#include "mzm_sprite_ai.h"

enum {
    LARVA_IDLE_INIT = 0x8,
    LARVA_IDLE = 0x9,
    LARVA_DETECT_SAMUS = 0xF,
    LARVA_RETREATING_INIT = 0x22,
    LARVA_RETREATING = 0x23,
    LARVA_STUNNED_INIT = 0x24,
    LARVA_STUNNED = 0x25,
    LARVA_WARNING_INIT = 0x26,
    LARVA_WARNING = 0x27,
    LARVA_ATTACKING_INIT = 0x28,
    LARVA_ATTACKING = 0x29,
    LARVA_TAKING_DAMAGE_INIT = 0x42,
    LARVA_TAKING_DAMAGE = 0x43,
    LARVA_DYING_INIT = 0x62,
    LARVA_DYING = 0x67,
    LARVA_DEAD = 0x68,

    PART_DOT_IDLE = 0x9,
    PART_DOT_REMOVING = 0x23,
    PART_DOT_CHECK_REAPPEAR = 0x25,
    PART_DOT_REAPPEARING = 0x27,
    PART_SHELL_IDLE = 0x42,
    PART_CLAWS_IDLE = 0x60,
    PART_DEAD = 0x67,

    PART_CLAWS = 0,
    PART_RIGHT_DOT,
    PART_MIDDLE_DOT,
    PART_LEFT_DOT,
    PART_SHELL,
    PART_LARVA,
};

static MzmSprite *parent(const Ctx *c) {
    return c->f->sprites && c->s->ram_slot < c->f->sprite_count ?
           &c->f->sprites[c->s->ram_slot] : NULL;
}

static MzmSubSpriteState *larva_state(Ctx *c) {
    MzmSprite *body = c->s->secondary ? parent(c) : c->s;
    return body && body->sprite_id == MZM_PSPRITE_IMAGO_LARVA_LEFT ?
           &c->f->globals->sub2 : &c->f->globals->sub1;
}

static const MzmMultiSprite *multi(const Ctx *c, const MzmSubSpriteState *state) {
    uint8_t id = state->multi < MZM_IMAGO_MULTI_COUNT ? state->multi : MZM_IMAGO_MULTI_IDLE;
    return &c->tables->imago_larva_multi[id];
}

static const int16_t *part_data(const Ctx *c, const MzmSubSpriteState *state, uint8_t part) {
    uint8_t frame = state->frame < MZM_MULTI_FRAMES ? state->frame : 0;
    return multi(c, state)->parts[frame][part < MZM_MULTI_PARTS ? part : 0];
}

static void set_multi(MzmSubSpriteState *state, uint8_t id) {
    state->multi = id;
    state->frame = 0;
    state->adc = 0;
}

/* SpriteUtilUpdateSubSpriteAnim. */
static void update_multi(const Ctx *c, MzmSubSpriteState *state) {
    const MzmMultiSprite *m = multi(c, state);
    state->adc++;
    if (m->timers[state->frame] < state->adc) {
        state->adc = 1;
        state->frame++;
        if (state->frame >= MZM_MULTI_FRAMES || m->timers[state->frame] == 0)
            state->frame = 0;
    }
}

/* SpriteUtilHasSubSpriteAnimationNearlyEnded. */
static bool multi_nearly_ended(const Ctx *c, const MzmSubSpriteState *state) {
    const MzmMultiSprite *m = multi(c, state);
    unsigned next = state->frame + 1u;
    return m->timers[state->frame] < (uint8_t)(state->adc + 2) &&
           (next >= MZM_MULTI_FRAMES || m->timers[next] == 0);
}

static void sync_position(Ctx *c, const MzmSubSpriteState *state) {
    const int16_t *p = part_data(c, state, c->s->room_slot);
    c->s->y = (uint16_t)(state->y + p[1]);
    c->s->x = (uint16_t)(c->s->status & MZM_SPRITE_STATUS_X_FLIP ?
                         state->x - p[2] : state->x + p[2]);
}

/* ImagoLarvaSyncSubSprites: synchronize both the OAM and the position. */
static void sync_part(Ctx *c, const MzmSubSpriteState *state) {
    uint8_t oam = (uint8_t)part_data(c, state, c->s->room_slot)[0];
    if (oam < MZM_IMAGO_LARVA_ANIMS && c->s->anim != oam) ai_set_anim(c->s, oam);
    sync_position(c, state);
}

static void door_timer(const Ctx *c, int8_t timer) {
    if (c->f->hooks && c->f->hooks->door_unlock_timer)
        c->f->hooks->door_unlock_timer(c->f->hooks->context, timer);
}

static void set_event(const Ctx *c, uint8_t event) {
    if (c->f->hooks && c->f->hooks->set_event)
        c->f->hooks->set_event(c->f->hooks->context, event);
}

/* ImagoLarvaUpdatePalette. */
static void update_palette(MzmSprite *s) {
    static const uint8_t rows[] = {1, 2, 1, 0};
    if (s->isft & 0x7F) return;
    if (++s->work2 >= 10) {
        s->work2 = 0;
        if (++s->work3 >= sizeof rows) s->work3 = 0;
    }
    s->palette_row = s->absolute_palette_row = rows[s->work3];
}

static void spawn_parts(Ctx *c, uint16_t y, uint16_t x, uint16_t status) {
    for (uint8_t part = PART_CLAWS; part <= PART_SHELL; ++part)
        ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_LARVA_PART, part, c->s->gfx_slot,
                           y, x, status);
}

/* ImagoLarvaInit. */
static void body_init(Ctx *c, MzmSubSpriteState *state) {
    MzmSprite *s = c->s;
    if (ai_check_event(c, MZM_EVENT_CATERPILLAR_KILLED)) {
        s->status = 0;
        return;
    }
    s->y = (uint16_t)(s->y + EIGHTH_BLOCK_SIZE);
    state->y = s->y;
    state->x = s->x;
    uint16_t spawn_y = state->y, spawn_x = state->x;
    s->x_spawn = spawn_x;
    if (s->sprite_id == MZM_PSPRITE_IMAGO_LARVA_LEFT) {
        s->status |= MZM_SPRITE_STATUS_X_FLIP;
        s->hitbox_left = -(BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE);
        s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE - EIGHTH_BLOCK_SIZE;
        s->pose = LARVA_IDLE;
        door_timer(c, 1);            /* LOCK_DOORS */
        mzm_sfx_music_play(MZM_MUSIC_CATTERPILLARS_BATTLE_2, 0);
    } else {
        s->hitbox_left = -(BLOCK_SIZE + HALF_BLOCK_SIZE - EIGHTH_BLOCK_SIZE);
        s->hitbox_right = BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE;
        state->x = (uint16_t)(state->x + BLOCK_SIZE * 11);
        s->pose = LARVA_DETECT_SAMUS;
    }
    s->draw_top = s->draw_bottom = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE - QUARTER_BLOCK_SIZE);
    s->hitbox_bottom = 0;
    s->work0 = s->work1 = s->work2 = s->work3 = 0;
    s->y_spawn = 0;
    s->samus_collision = SSC_NONE;
    s->health = c->tables->health[s->sprite_id];
    state->health = s->health;
    state->work1 = 0;               /* retreat count */
    state->work2 = 0;               /* retreat immunity */
    set_multi(state, MZM_IMAGO_MULTI_IDLE);
    s->draw_order = 6;
    s->frozen_row_offset = 2;
    s->room_slot = PART_LARVA;
    spawn_parts(c, spawn_y, spawn_x, s->status & MZM_SPRITE_STATUS_X_FLIP);
}

static void body_death(Ctx *c, MzmSubSpriteState *state) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case LARVA_DYING_INIT:
        set_multi(state, MZM_IMAGO_MULTI_DYING);
        s->pose = LARVA_DYING;
        s->work0 = 100;
        s->isft = (uint8_t)((s->isft & 0x80) | 100);
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->health = 1;
        mzm_sfx_fade(MZM_SOUND_IMAGO_LARVA_ATTACKING, 10);
        mzm_sfx_play(MZM_SOUND_IMAGO_LARVA_DYING);
        if (s->sprite_id == MZM_PSPRITE_IMAGO_LARVA_RIGHT) mzm_sfx_music_fade(10);
        /* fallthrough */
    case LARVA_DYING:
        if (s->work0 && --s->work0 == 0) {
            s->pose = LARVA_DEAD;
            s->work0 = 2;
        }
        break;
    case LARVA_DEAD:
        if (s->work0 && --s->work0 != 0) break;
        if (s->sprite_id == MZM_PSPRITE_IMAGO_LARVA_RIGHT) {
            set_event(c, MZM_EVENT_CATERPILLAR_KILLED);
            door_timer(c, -60);
            mzm_sfx_music_play(MZM_MUSIC_BOSS_KILLED, 0);
        }
        ai_sprite_death_full(c, DEATH_NORMAL,
                             (uint16_t)(s->y - (HALF_BLOCK_SIZE + PIXEL_SIZE)), s->x,
                             false, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        break;
    default:
        break;
    }
}

static void body_ai(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = larva_state(c);
    s->ignore_samus_timer = 1;
    if (s->pose > SPRITE_POSE_UNINITIALIZED && s->pose < LARVA_DYING_INIT) {
        if (s->damaged) {
            s->damaged = false;
            ai_sound_onscreen(s, MZM_SOUND_IMAGO_LARVA_DAMAGED);
        }
        if (state->health != s->health) {
            state->health = s->health;
            s->pose = LARVA_TAKING_DAMAGE_INIT;
        }
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        body_init(c, state);
        break;
    case LARVA_DETECT_SAMUS:
        update_palette(s);
        if (state->x > c->f->samus_x && state->x - c->f->samus_x < BLOCK_SIZE * 5)
            s->pose = LARVA_WARNING_INIT;
        break;
    case LARVA_IDLE_INIT:
        set_multi(state, MZM_IMAGO_MULTI_IDLE);
        s->pose = LARVA_IDLE;
        /* fallthrough */
    case LARVA_IDLE:
        update_palette(s);
        break;
    case LARVA_RETREATING_INIT:
        if (state->multi != MZM_IMAGO_MULTI_RETREATING)
            set_multi(state, MZM_IMAGO_MULTI_RETREATING);
        s->pose = LARVA_RETREATING;
        /* fallthrough */
    case LARVA_RETREATING:
        update_palette(s);
        if (s->work0-- != 0) {
            if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
                if (state->x >= (uint16_t)(s->x_spawn - BLOCK_SIZE * 5)) state->x -= s->y_spawn;
            } else if (state->x <= (uint16_t)(s->x_spawn + BLOCK_SIZE * 11)) {
                state->x += s->y_spawn;
            }
        } else {
            s->pose = LARVA_STUNNED_INIT;
        }
        break;
    case LARVA_STUNNED_INIT:
        set_multi(state, MZM_IMAGO_MULTI_IDLE);
        s->pose = LARVA_STUNNED;
        /* fallthrough */
    case LARVA_STUNNED:
        update_palette(s);
        if (s->work1-- == 0) s->pose = LARVA_WARNING_INIT;
        break;
    case LARVA_WARNING_INIT:
        set_multi(state, MZM_IMAGO_MULTI_WARNING);
        s->pose = LARVA_WARNING;
        s->work0 = 60;
        state->work1 = 0;
        mzm_sfx_play(MZM_SOUND_IMAGO_LARVA_WARNING);
        /* fallthrough */
    case LARVA_WARNING:
        update_palette(s);
        if (multi_nearly_ended(c, state)) s->pose = LARVA_ATTACKING_INIT;
        break;
    case LARVA_ATTACKING_INIT:
        set_multi(state, MZM_IMAGO_MULTI_ATTACKING);
        s->pose = LARVA_ATTACKING;
        mzm_sfx_play(MZM_SOUND_IMAGO_LARVA_ATTACKING);
        /* fallthrough */
    case LARVA_ATTACKING:
        update_palette(s);
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            state->x = (uint16_t)(state->x + EIGHTH_BLOCK_SIZE);
            if (state->x >= s->x_spawn) {
                state->x = s->x_spawn;
                s->pose = LARVA_IDLE_INIT;
                mzm_sfx_fade(MZM_SOUND_IMAGO_LARVA_ATTACKING, 10);
            }
        } else {
            state->x = (uint16_t)(state->x - EIGHTH_BLOCK_SIZE);
            if (state->x <= s->x_spawn) {
                state->x = s->x_spawn;
                s->pose = LARVA_IDLE_INIT;
                mzm_sfx_fade(MZM_SOUND_IMAGO_LARVA_ATTACKING, 10);
            }
        }
        break;
    case LARVA_TAKING_DAMAGE_INIT:
        set_multi(state, MZM_IMAGO_MULTI_TAKING_DAMAGE);
        s->pose = LARVA_TAKING_DAMAGE;
        s->work0 = 47;
        state->work1 = 0;
        mzm_sfx_fade(MZM_SOUND_IMAGO_LARVA_ATTACKING, 10);
        /* fallthrough */
    case LARVA_TAKING_DAMAGE:
        if (s->work0 && --s->work0 == 0) s->pose = LARVA_ATTACKING_INIT;
        break;
    case LARVA_DYING_INIT:
    case LARVA_DYING:
    case LARVA_DEAD:
        body_death(c, state);
        break;
    default:
        break;
    }
    if (s->health != 0) {
        update_multi(c, state);
        sync_part(c, state);
    }
}

static uint8_t dot_anim(uint8_t part, uint8_t left, uint8_t middle, uint8_t right) {
    return part == PART_RIGHT_DOT ? right : part == PART_MIDDLE_DOT ? middle : left;
}

/* ImagoLarvaPartInit. */
static void part_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->health = 1;
    if (s->room_slot >= PART_RIGHT_DOT && s->room_slot <= PART_LEFT_DOT) {
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        ai_set_anim(s, dot_anim(s->room_slot, MZM_IMAGO_LEFT_DOT_VISIBLE,
                                MZM_IMAGO_MIDDLE_DOT_VISIBLE, MZM_IMAGO_RIGHT_DOT_VISIBLE));
        s->samus_collision = SSC_NONE;
        s->pose = PART_DOT_IDLE;
    } else if (s->room_slot == PART_SHELL) {
        s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -(BLOCK_SIZE * 3 - QUARTER_BLOCK_SIZE);
        s->hitbox_bottom = 0;
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            s->hitbox_left = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
            s->hitbox_right = BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        } else {
            s->hitbox_left = -(BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
            s->hitbox_right = BLOCK_SIZE * 2 + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        }
        s->samus_collision = SSC_HURTS_KNOCKBACK_IF_INVINCIBLE;
        s->draw_order = 5;
        s->pose = PART_SHELL_IDLE;
        s->health = UCHAR_MAX;
    } else if (s->room_slot == PART_CLAWS) {
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->draw_top = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->samus_collision = SSC_NONE;
        s->draw_order = 3;
        s->pose = PART_CLAWS_IDLE;
    } else {
        s->status = 0;
    }
}

/* ImagoLarvaPartShellIdle. */
static void shell_idle(Ctx *c, MzmSubSpriteState *state, MzmSprite *body) {
    MzmSprite *s = c->s;
    if (body && (body->pose == LARVA_IDLE || body->pose == LARVA_STUNNED ||
                 body->pose == LARVA_DETECT_SAMUS) && (s->frame & 1) && s->adc == 1)
        ai_sound_onscreen(s, MZM_SOUND_IMAGO_LARVA_IDLE);
    if (body && body->sprite_id == MZM_PSPRITE_IMAGO_LARVA_LEFT) {
        s->hitbox_bottom = state->x - (BLOCK_SIZE * 3 + QUARTER_BLOCK_SIZE) < c->f->samus_x &&
                           state->x + BLOCK_SIZE + HALF_BLOCK_SIZE > c->f->samus_x ?
                           -HALF_BLOCK_SIZE : 0;
        if (s->isft & 0x7F) goto reset_hit;
    } else if (state->x < c->f->samus_x) {
        if (s->isft & 0x7F) goto reset_hit;
    } else if (s->isft & 0x7F) {
        if (!state->work2 && body) {
            uint16_t damage = UCHAR_MAX - s->health;
            uint8_t speed = damage >= 100 ? 8 : damage >= 20 ? 4 : 1;
            mzm_sfx_play(speed > 1 ? MZM_SOUND_IMAGO_LARVA_CRAWLING_FAST :
                                     MZM_SOUND_IMAGO_LARVA_CRAWLING_SLOW);
            if (state->work1 < 8) state->work1++;
            body->work0 = 16;
            body->work1 = (uint8_t)(state->work1 * 8);
            body->y_spawn = speed;
            body->pose = LARVA_RETREATING_INIT;
        }
reset_hit:
        s->palette_row = s->absolute_palette_row;
        s->isft &= 0x80;
        s->health = UCHAR_MAX;
    }
}

static void dot_ai(Ctx *c, MzmSubSpriteState *state, const MzmSprite *body) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case PART_DOT_IDLE:
        if (body && body->pose == LARVA_WARNING) {
            ai_set_anim(s, dot_anim(s->room_slot, MZM_IMAGO_LEFT_DOT_DISAPPEARING,
                                    MZM_IMAGO_MIDDLE_DOT_DISAPPEARING,
                                    MZM_IMAGO_RIGHT_DOT_DISAPPEARING));
            s->pose = PART_DOT_REMOVING;
            state->work2 = 1;
        }
        break;
    case PART_DOT_REMOVING:
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, dot_anim(s->room_slot, MZM_IMAGO_LEFT_DOT_VISIBLE,
                                    MZM_IMAGO_MIDDLE_DOT_VISIBLE, MZM_IMAGO_RIGHT_DOT_VISIBLE));
            s->pose = PART_DOT_CHECK_REAPPEAR;
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        }
        break;
    case PART_DOT_CHECK_REAPPEAR:
        if (body && body->pose == LARVA_IDLE) {
            ai_set_anim(s, dot_anim(s->room_slot, MZM_IMAGO_LEFT_DOT_APPEARING,
                                    MZM_IMAGO_MIDDLE_DOT_APPEARING, MZM_IMAGO_RIGHT_DOT_APPEARING));
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
            s->pose = PART_DOT_REAPPEARING;
        }
        break;
    case PART_DOT_REAPPEARING:
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, dot_anim(s->room_slot, MZM_IMAGO_LEFT_DOT_VISIBLE,
                                    MZM_IMAGO_MIDDLE_DOT_VISIBLE, MZM_IMAGO_RIGHT_DOT_VISIBLE));
            s->pose = PART_DOT_IDLE;
            state->work2 = 0;
        }
        break;
    default:
        break;
    }
}

static void part_death(Ctx *c, const MzmSprite *body) {
    MzmSprite *s = c->s;
    if (body && body->sprite_id != MZM_PSPRITE_IMAGO_LARVA_LEFT &&
        body->sprite_id != MZM_PSPRITE_IMAGO_LARVA_RIGHT)
        body = NULL;                /* the body became a drop this frame */
    if (body && body->pose != LARVA_DEAD) return;
    uint16_t y = s->y, x = s->x;
    switch (s->room_slot) {
    case PART_RIGHT_DOT:
        y = (uint16_t)(y + QUARTER_BLOCK_SIZE);
        x = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? x - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)
                                                             : x + BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        break;
    case PART_MIDDLE_DOT:
        y = (uint16_t)(y - QUARTER_BLOCK_SIZE);
        x = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? x - HALF_BLOCK_SIZE
                                                             : x + HALF_BLOCK_SIZE);
        break;
    case PART_SHELL:
        y = (uint16_t)(y - BLOCK_SIZE);
        x = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? x - (BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE)
                                                             : x + BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE);
        break;
    case PART_CLAWS:
        s->status = 0;
        return;
    default:
        break;
    }
    ai_sprite_death_full(c, DEATH_NORMAL, y, x, false, MZM_PE_SPRITE_EXPLOSION_BIG);
}

static void part_ai(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *body = parent(c);
    MzmSubSpriteState *state = larva_state(c);
    if (body && body->pose >= LARVA_DYING_INIT && s->pose <= PART_CLAWS_IDLE + 1) {
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->pose = PART_DEAD;
        s->isft = body->isft;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: part_init(c); break;
    case PART_DOT_IDLE:
    case PART_DOT_REMOVING:
    case PART_DOT_CHECK_REAPPEAR:
    case PART_DOT_REAPPEARING: dot_ai(c, state, body); break;
    case PART_SHELL_IDLE: shell_idle(c, state, body); break;
    case PART_DEAD: part_death(c, body); break;
    default: break;
    }
    if (s->health == 0) return;
    if (s->room_slot == PART_SHELL || s->room_slot == PART_CLAWS) {
        sync_part(c, state);
    } else if (s->pose == PART_DEAD) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        sync_part(c, state);
    } else {
        sync_position(c, state);
    }
}

void ai_imago_larva(Ctx *c) {
    if (c->s->secondary) part_ai(c);
    else body_ai(c);
}

void ai_imago_larva_right_side(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->hitbox_top = s->hitbox_left = -MZM_PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = MZM_PIXEL_SIZE;
        s->draw_top = s->draw_horizontal = 48;
        s->draw_bottom = 8;
        s->samus_collision = SSC_NONE;
        ai_set_anim(s, MZM_IMAGO_LARVA_RIGHT_SIDE_IDLE);
        s->pose = LARVA_IDLE;
    }
    if (s->adc != 1) return;
    if (s->frame == 0) {
        mzm_sfx_play(MZM_SOUND_IMAGO_LARVA_RIGHT_SIDE_SHAKING);
    } else if (s->frame == 11 || s->frame == 15 || s->frame == 19 ||
               s->frame == 23 || s->frame == 27 || s->frame == 31) {
        mzm_sfx_play(MZM_SOUND_IMAGO_LARVA_RIGHT_SIDE_CORE_BEATING);
    }
}
