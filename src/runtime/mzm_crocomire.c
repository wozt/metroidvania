/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's unused Crocomire and Gekitai Machine prototypes
 * (src/sprites_ai/crocomire.c and gekitai_machine.c). Crocomire only
 * assembles and loops its initial composition; Gekitai removes itself. */
#include "mzm_sprite_ai.h"

enum {
    CROCOMIRE_POSE_IDLE_INIT = 0x8,
    CROCOMIRE_POSE_IDLE = 0x9,
    CROCOMIRE_PART_POSE_IDLE = 0x9,
    CROCOMIRE_PART_POSE_DYING = 0x67,
    CROCOMIRE_PART_RIGHT_ARM = 0,
    CROCOMIRE_PART_HEAD = 1,
    CROCOMIRE_PART_LEGS = 2,
    CROCOMIRE_PART_BODY = 3,
    CROCOMIRE_PART_TONGUE = 4,
    CROCOMIRE_PART_LEFT_ARM = 5,
};

static const MzmMultiSprite *multi(const Ctx *c) {
    uint8_t id = c->f->globals->sub1.multi;
    if (id >= MZM_CROCOMIRE_MULTI_COUNT) id = MZM_CROCOMIRE_MULTI_SCREAMING;
    return &c->tables->crocomire_multi[id];
}

static void set_multi(Ctx *c, uint8_t id) {
    MzmSubSpriteState *state = &c->f->globals->sub1;
    state->multi = id;
    state->frame = 0;
    state->adc = 0;
}

/* SpriteUtilUpdateSubSprite1Anim. */
static void update_multi(Ctx *c) {
    MzmSubSpriteState *state = &c->f->globals->sub1;
    const MzmMultiSprite *m = multi(c);
    if (state->frame >= MZM_MULTI_FRAMES || !m->timers[state->frame]) state->frame = 0;
    state->adc++;
    if (m->timers[state->frame] < state->adc) {
        state->adc = 1;
        state->frame++;
        if (state->frame >= MZM_MULTI_FRAMES || !m->timers[state->frame]) state->frame = 0;
    }
}

/* CrocomireSyncSubSprites. */
static void sync(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSubSpriteState *state = &c->f->globals->sub1;
    const MzmMultiSprite *m = multi(c);
    uint8_t frame = state->frame < MZM_MULTI_FRAMES && m->timers[state->frame] ? state->frame : 0;
    uint8_t part_id = s->room_slot < MZM_MULTI_PARTS ? s->room_slot : 0;
    const int16_t *part = m->parts[frame][part_id];
    if (part[0] >= 0 && part[0] < MZM_CROCOMIRE_ANIMS && s->anim != part[0])
        ai_set_anim(s, (uint8_t)part[0]);
    s->y = (uint16_t)(state->y + part[1]);
    s->x = (uint16_t)(state->x + part[2]);
}

static void init_part(Ctx *c) {
    MzmSprite *s = c->s;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->health = 1;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->pose = CROCOMIRE_PART_POSE_IDLE;
    s->immune_to_projectiles = true;
    switch (s->room_slot) {
    case CROCOMIRE_PART_RIGHT_ARM:
        s->draw_top = 32; s->draw_bottom = 44; s->draw_horizontal = 88;
        s->hitbox_top = -PIXEL_SIZE; s->hitbox_bottom = PIXEL_SIZE;
        s->hitbox_left = -PIXEL_SIZE; s->hitbox_right = PIXEL_SIZE;
        s->draw_order = 2;
        break;
    case CROCOMIRE_PART_LEGS:
        s->draw_top = 16; s->draw_bottom = 16; s->draw_horizontal = 16;
        s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE); s->hitbox_bottom = 0;
        s->hitbox_left = -(BLOCK_SIZE * 3); s->hitbox_right = BLOCK_SIZE;
        s->draw_order = 3;
        break;
    case CROCOMIRE_PART_BODY:
        s->draw_top = 80; s->draw_bottom = 0; s->draw_horizontal = 48;
        s->hitbox_top = -(BLOCK_SIZE * 3 + HALF_BLOCK_SIZE); s->hitbox_bottom = 0;
        s->hitbox_left = -(BLOCK_SIZE * 2); s->hitbox_right = BLOCK_SIZE * 2 + HALF_BLOCK_SIZE;
        s->draw_order = 5;
        break;
    case CROCOMIRE_PART_TONGUE:
        s->draw_top = 10; s->draw_bottom = 10; s->draw_horizontal = 16;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->samus_collision = SSC_NONE;
        s->draw_order = 6;
        break;
    case CROCOMIRE_PART_LEFT_ARM:
        s->draw_top = 32; s->draw_bottom = 42; s->draw_horizontal = 88;
        s->hitbox_top = -PIXEL_SIZE; s->hitbox_bottom = PIXEL_SIZE;
        s->hitbox_left = -PIXEL_SIZE; s->hitbox_right = PIXEL_SIZE;
        s->draw_order = 6;
        break;
    default:
        s->status = 0;
        break;
    }
}

static void part(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    MzmSprite *parent = c->f->sprites && s->ram_slot < c->f->sprite_count
                            ? &c->f->sprites[s->ram_slot] : NULL;
    if (!parent || !(parent->status & MZM_SPRITE_STATUS_EXISTS)) {
        s->status = 0;
        return;
    }
    if (parent->pose > 0x61 && s->pose < 0x62) {
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->pose = CROCOMIRE_PART_POSE_DYING;
        s->isft = parent->isft;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) init_part(c);
    if (s->status & MZM_SPRITE_STATUS_EXISTS) sync(c);
}

static void init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    state->y = s->y;
    state->x = s->x;
    s->x_spawn = s->x;
    s->draw_top = 32;
    s->draw_bottom = 24;
    s->draw_horizontal = 48;
    s->hitbox_top = -BLOCK_SIZE;
    s->hitbox_bottom = HALF_BLOCK_SIZE;
    s->hitbox_left = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->hitbox_right = BLOCK_SIZE * 2;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = c->tables->health[s->sprite_id];
    set_multi(c, MZM_CROCOMIRE_MULTI_SCREAMING);
    state->work1 = 0;
    state->work2 = 0;
    s->pose = CROCOMIRE_POSE_IDLE;
    s->frozen_row_offset = 2;
    s->room_slot = CROCOMIRE_PART_HEAD;
    ai_spawn_secondary(c, MZM_SSPRITE_CROCOMIRE_PART, CROCOMIRE_PART_RIGHT_ARM,
                       s->gfx_slot, state->y, state->x, 0);
    ai_spawn_secondary(c, MZM_SSPRITE_CROCOMIRE_PART, CROCOMIRE_PART_LEGS,
                       s->gfx_slot, state->y, state->x, 0);
    ai_spawn_secondary(c, MZM_SSPRITE_CROCOMIRE_PART, CROCOMIRE_PART_BODY,
                       s->gfx_slot, state->y, state->x, 0);
    ai_spawn_secondary(c, MZM_SSPRITE_CROCOMIRE_PART, CROCOMIRE_PART_TONGUE,
                       s->gfx_slot, state->y, state->x, 0);
    ai_spawn_secondary(c, MZM_SSPRITE_CROCOMIRE_PART, CROCOMIRE_PART_LEFT_ARM,
                       s->gfx_slot, state->y, state->x, 0);
}

void ai_crocomire(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->secondary) {
        part(c);
        return;
    }
    /* GekitaiMachine consists solely of `gCurrentSprite.status = 0`. */
    if (s->sprite_id == MZM_PSPRITE_GEKITAI_MACHINE) {
        s->status = 0;
        return;
    }
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) init(c);
    else if (s->pose == CROCOMIRE_POSE_IDLE_INIT) {
        set_multi(c, MZM_CROCOMIRE_MULTI_IDLE);
        s->pose = CROCOMIRE_POSE_IDLE;
    }
    update_multi(c);
    sync(c);
}
