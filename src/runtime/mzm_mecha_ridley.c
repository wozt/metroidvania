/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Mecha Ridley (src/sprites_ai/mecha_ridley.c): the native
 * multipart entrance, arena lock and transition into the first combat idle.
 * Attack selection and the weapon actors are added by the following slice. */
#include "mzm_sprite_ai.h"

enum {
    MECHA_CRAWLING_INIT = 0x1,
    MECHA_DELAY_BEFORE_CRAWLING = 0x2,
    MECHA_CRAWLING = 0x3,
    MECHA_DELAY_BEFORE_IDLE = 0x4,
    MECHA_IDLE = 0x9,
};

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
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->y_spawn = 300;
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

static void body(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    if (s->damaged) {
        s->damaged = false;
        ai_sound_onscreen(s, MZM_SOUND_MECHA_RIDLEY_DAMAGED);
    }
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
    case MECHA_IDLE: break;
    default: break;
    }
    if (!(s->status & MZM_SPRITE_STATUS_EXISTS)) return;
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
        state->health = s->health; s->draw_order = 9; s->pose = PART_COVER_IDLE;
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
        s->pose = PART_MISSILE_LAUNCHER_IDLE;
        break;
    default: s->status = 0; break;
    }
}

static void part(Ctx *c) {
    if (c->s->pose == SPRITE_POSE_UNINITIALIZED) part_init(c);
    if (c->s->status & MZM_SPRITE_STATUS_EXISTS) sync_part(c);
}

void ai_mecha_ridley(Ctx *c) {
    if (!c->f->globals) {
        c->s->status = 0;
        return;
    }
    if (!c->s->secondary) body(c);
    else if (c->s->sprite_id == MZM_SSPRITE_MECHA_RIDLEY_PART) part(c);
    else c->s->status = 0;
}
