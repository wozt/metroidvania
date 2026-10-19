/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Tangle Vines (src/sprites_ai/tangle_vine.c): the solid,
 * harmful plants on the ground or the ceiling, the red Geruta and larva
 * vines, and the vine holding a Geruta that drops it when the vine dies. */
#include "mzm_sprite_ai.h"

enum {
    TANGLE_VINE_POSE_IDLE = 0x9,
    GERUTA_POSE_IDLE = 0x9,
    GERUTA_POSE_NO_GERUTA = 0x23,
    GERUTA_PART_IDLE = 0xF,

    /* gSubSpriteData1.health: what is left of the vine Geruta. */
    STATUS_VINE_DEAD = 0,
    STATUS_GERUTA_DEAD = 1,
    STATUS_FULL = 2,

    PART_GRIP = 0,
    PART_GERUTA = 1,
    PART_ROOT = 2,
};

static void damaged_sound(MzmSprite *s) {
    if (!s->damaged) return;
    s->damaged = false;
    ai_sound_onscreen_once(s, MZM_SOUND_TANGLE_VINE_DAMAGE);
}

/* One plain vine: its ground and ceiling hitbox heights, draw heights,
 * half widths, death height offsets and explosion. */
typedef struct {
    uint8_t ground_anim, ceiling_anim;
    int16_t reach, half_width;
    uint8_t draw_reach, draw_half_width;
    int16_t death_ceiling, death_ground;
    uint8_t effect;
} Vine;

static const Vine *vine_of(uint8_t sprite_id) {
    static const Vine tall = {MZM_TANGLE_VINE_TALL_ON_GROUND, MZM_TANGLE_VINE_TALL_ON_CEILING,
                              BLOCK_SIZE * 4 - EIGHTH_BLOCK_SIZE, QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
                              BLOCK_SIZE * 4 / PIXEL_SIZE, BLOCK_SIZE / PIXEL_SIZE,
                              BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE, -BLOCK_SIZE,
                              MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG};
    static const Vine medium = {MZM_TANGLE_VINE_MEDIUM_ON_GROUND, MZM_TANGLE_VINE_MEDIUM_ON_CEILING,
                                BLOCK_SIZE * 3 - EIGHTH_BLOCK_SIZE, QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
                                BLOCK_SIZE * 3 / PIXEL_SIZE, BLOCK_SIZE / PIXEL_SIZE,
                                BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE, -THREE_QUARTER_BLOCK_SIZE,
                                MZM_PE_SPRITE_EXPLOSION_BIG};
    static const Vine curved = {MZM_TANGLE_VINE_CURVED_ON_GROUND, MZM_TANGLE_VINE_CURVED_ON_CEILING,
                                BLOCK_SIZE * 2, QUARTER_BLOCK_SIZE,
                                (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE, HALF_BLOCK_SIZE / PIXEL_SIZE,
                                BLOCK_SIZE + EIGHTH_BLOCK_SIZE, -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE),
                                MZM_PE_SPRITE_EXPLOSION_MEDIUM};
    static const Vine shorter = {MZM_TANGLE_VINE_SHORT_ON_GROUND, MZM_TANGLE_VINE_SHORT_ON_CEILING,
                                 BLOCK_SIZE * 2 + EIGHTH_BLOCK_SIZE, QUARTER_BLOCK_SIZE,
                                 (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE, HALF_BLOCK_SIZE / PIXEL_SIZE,
                                 BLOCK_SIZE, -HALF_BLOCK_SIZE, MZM_PE_SPRITE_EXPLOSION_SMALL};
    switch (sprite_id) {
    case MZM_PSPRITE_TANGLE_VINE_TALL: return &tall;
    case MZM_PSPRITE_TANGLE_VINE_MEDIUM: return &medium;
    case MZM_PSPRITE_TANGLE_VINE_CURVED: return &curved;
    default: return &shorter;
    }
}

/* TangleVineTall / Medium / Curved / Short: on the ground when the block
 * below is not air, else hanging from the ceiling a block higher. */
static void plain_vine(Ctx *c) {
    MzmSprite *s = c->s;
    const Vine *v = vine_of(s->sprite_id);
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        if (ai_collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE), s->x) != MZM_COLLISION_AIR) {
            s->draw_top = v->draw_reach;
            s->draw_bottom = 0;
            s->hitbox_top = (int16_t)-v->reach;
            s->hitbox_bottom = 0;
            ai_set_anim(s, v->ground_anim);
        } else {
            s->draw_top = 0;
            s->draw_bottom = v->draw_reach;
            s->hitbox_top = 0;
            s->hitbox_bottom = v->reach;
            ai_set_anim(s, v->ceiling_anim);
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
            s->y = (uint16_t)(s->y - BLOCK_SIZE);
        }
        s->draw_horizontal = v->draw_half_width;
        s->hitbox_left = (int16_t)-v->half_width;
        s->hitbox_right = v->half_width;
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS_SOLID;
        s->pose = TANGLE_VINE_POSE_IDLE;
        s->frozen_row_offset = 1;
    } else if (s->pose >= SPRITE_POSE_DESTROYED) {
        uint16_t y = s->status & MZM_SPRITE_STATUS_FACING_DOWN ? (uint16_t)(s->y + v->death_ceiling)
                                                               : (uint16_t)(s->y + v->death_ground);
        ai_sprite_death_full(c, DEATH_NORMAL, y, s->x, true, v->effect);
    }
}

/* TangleVineRedGeruta, TangleVineLarvaRight and TangleVineLarvaLeft. */
static void special_vine(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        switch (s->sprite_id) {
        case MZM_PSPRITE_TANGLE_VINE_RED_GARUTA:
            s->draw_top = (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_bottom = 0;
            s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->hitbox_top = -(BLOCK_SIZE * 4 + QUARTER_BLOCK_SIZE);
            s->hitbox_bottom = 0;
            s->hitbox_left = -(HALF_BLOCK_SIZE + PIXEL_SIZE);
            s->hitbox_right = HALF_BLOCK_SIZE + PIXEL_SIZE;
            ai_set_anim(s, MZM_TANGLE_VINE_RED_GERUTA);
            break;
        case MZM_PSPRITE_TANGLE_VINE_LARVA_RIGHT:
            s->y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
            s->draw_top = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
            s->hitbox_top = -(BLOCK_SIZE * 3);
            s->hitbox_bottom = 0;
            s->hitbox_left = 0;
            s->hitbox_right = HALF_BLOCK_SIZE + PIXEL_SIZE;
            ai_set_anim(s, MZM_TANGLE_VINE_LARVA_RIGHT);
            break;
        default:
            s->x = (uint16_t)(s->x - QUARTER_BLOCK_SIZE);
            s->draw_top = (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
            s->hitbox_top = -(BLOCK_SIZE * 3);
            s->hitbox_bottom = 0;
            s->hitbox_left = -(HALF_BLOCK_SIZE + PIXEL_SIZE);
            s->hitbox_right = 0;
            ai_set_anim(s, MZM_TANGLE_VINE_LARVA_LEFT);
            s->draw_order = 5;
            break;
        }
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS_SOLID;
        s->pose = TANGLE_VINE_POSE_IDLE;
        s->frozen_row_offset = 2;
        return;
    }
    if (s->pose < SPRITE_POSE_DESTROYED) return;
    uint16_t y = s->y, x = s->x;
    switch (s->sprite_id) {
    case MZM_PSPRITE_TANGLE_VINE_RED_GARUTA:
        ai_particle_set(c, (uint16_t)(y - HALF_BLOCK_SIZE), (uint16_t)(x + QUARTER_BLOCK_SIZE),
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(y - (BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + PIXEL_SIZE / 2)),
                             (uint16_t)(x - QUARTER_BLOCK_SIZE), true, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        break;
    case MZM_PSPRITE_TANGLE_VINE_LARVA_RIGHT:
        ai_particle_set(c, (uint16_t)(y - HALF_BLOCK_SIZE), (uint16_t)(x - QUARTER_BLOCK_SIZE),
                        MZM_PE_SPRITE_EXPLOSION_BIG);
        ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(y - (BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE)),
                             (uint16_t)(x + QUARTER_BLOCK_SIZE), true, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        break;
    default:
        ai_particle_set(c, (uint16_t)(y - HALF_BLOCK_SIZE), (uint16_t)(x + QUARTER_BLOCK_SIZE),
                        MZM_PE_SPRITE_EXPLOSION_BIG);
        ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(y - (BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE)),
                             (uint16_t)(x - QUARTER_BLOCK_SIZE), true, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        break;
    }
}

/* SpriteUtilUpdateSubSprite1Anim on sTangleVineGerutaMultiSpriteData_Idle. */
static void update_multi(const Ctx *c, MzmSubSpriteState *state) {
    const MzmMultiSprite *m = &c->tables->tangle_vine_multi;
    if (state->frame >= MZM_MULTI_FRAMES) state->frame = 0;
    state->adc++;
    if (m->timers[state->frame] < state->adc) {
        state->adc = 1;
        state->frame++;
        if (state->frame >= MZM_MULTI_FRAMES || !m->timers[state->frame]) state->frame = 0;
    }
}

static const int16_t *part_data(const Ctx *c, uint8_t part) {
    const MzmSubSpriteState *state = &c->f->globals->sub1;
    uint8_t frame = state->frame < MZM_MULTI_FRAMES ? state->frame : 0;
    return c->tables->tangle_vine_multi.parts[frame][part < MZM_MULTI_PARTS ? part : 0];
}

/* SpriteUtilSyncCurrentSpritePositionWithSubSpriteData1PositionAndOam (and
 * TangleVineSyncSprites with `oam`): the offset mirrored under X_FLIP. */
static void sync(Ctx *c, bool oam) {
    MzmSprite *s = c->s;
    const MzmSubSpriteState *state = &c->f->globals->sub1;
    const int16_t *part = part_data(c, s->room_slot);
    if (oam && part[0] >= 0 && part[0] < MZM_TANGLE_VINE_ANIMS && s->anim != part[0])
        ai_set_anim(s, (uint8_t)part[0]);
    s->y = (uint16_t)(state->y + part[1]);
    s->x = s->status & MZM_SPRITE_STATUS_X_FLIP ? (uint16_t)(state->x - part[2])
                                                 : (uint16_t)(state->x + part[2]);
}

/* TangleVineGeruta: the vine root, its grip and its Geruta. */
static void geruta_vine(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = &c->f->globals->sub1;
    damaged_sound(s);
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->draw_top = BLOCK_SIZE * 4 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -(BLOCK_SIZE * 3 + HALF_BLOCK_SIZE);
        s->hitbox_bottom = 0;
        s->hitbox_left = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_right = HALF_BLOCK_SIZE - PIXEL_SIZE;
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS_SOLID;
        s->pose = GERUTA_POSE_IDLE;
        ai_set_anim(s, MZM_TANGLE_VINE_GERUTA_ROOT);
        state->frame = 0;
        state->adc = 0;
        state->health = STATUS_VINE_DEAD;
        state->y = s->y;
        state->x = s->x;
        s->frozen_row_offset = 1;
        s->draw_order = 5;
        s->room_slot = PART_ROOT;
        int grip = ai_spawn_secondary(c, MZM_SSPRITE_TANGLE_VINE_GERUTA_PART, PART_GRIP, s->gfx_slot,
                                      state->y, state->x, 0);
        int geruta = ai_spawn_secondary(c, MZM_SSPRITE_TANGLE_VINE_GERUTA_PART, PART_GERUTA, s->gfx_slot,
                                        state->y, state->x, 0);
        if (grip < 0 || geruta < 0) s->status = 0;
        else state->health = STATUS_FULL;
    } else if (s->pose >= SPRITE_POSE_DESTROYED) {
        state->health = STATUS_VINE_DEAD;
        ai_particle_set(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(s->y - (BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + PIXEL_SIZE / 2)),
                             (uint16_t)(s->x + QUARTER_BLOCK_SIZE), true, MZM_PE_SPRITE_EXPLOSION_BIG);
        return;
    } else if (s->pose == GERUTA_POSE_IDLE && state->health == STATUS_GERUTA_DEAD) {
        ai_set_anim(s, MZM_TANGLE_VINE_GERUTA_FULL);
        s->pose = GERUTA_POSE_NO_GERUTA;
    }
    if (!(s->status & MZM_SPRITE_STATUS_EXISTS)) return;
    update_multi(c, state);
    sync(c, false);
}

/* TangleVineGerutaPart: the grip goes with the Geruta; a free Geruta
 * becomes a red Geruta when the vine dies. */
static void geruta_part(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSubSpriteState *state = &c->f->globals->sub1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->pose = GERUTA_PART_IDLE;
        if (s->room_slot == PART_GRIP) {
            s->draw_top = BLOCK_SIZE * 4 / PIXEL_SIZE;
            s->draw_bottom = 0;
            s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
            s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
            s->health = 0;
            s->samus_collision = SSC_NONE;
            s->draw_order = 3;
        } else if (s->room_slot == PART_GERUTA) {
            s->draw_top = s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->hitbox_top = -HALF_BLOCK_SIZE;
            s->hitbox_bottom = HALF_BLOCK_SIZE;
            s->hitbox_left = -BLOCK_SIZE;
            s->hitbox_right = BLOCK_SIZE;
            s->health = 1;
            s->samus_collision = SSC_HURTS_SAMUS;
        } else {
            s->status = 0;
        }
    }
    if (s->room_slot == PART_GRIP) {
        if (state->health != STATUS_FULL) {
            s->status = 0;
            return;
        }
        MzmSprite *root = c->f->sprites && s->ram_slot < c->f->sprite_count ? &c->f->sprites[s->ram_slot] : NULL;
        if (root) s->palette_row = root->palette_row;
    } else if (s->room_slot == PART_GERUTA) {
        if (s->health == 0) {
            c->f->globals->sub1.health = STATUS_GERUTA_DEAD;
            ai_particle_set(c, (uint16_t)(s->y + EIGHTH_BLOCK_SIZE), s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
            mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_MEDIUM);
            s->status = 0;
            return;
        }
        if (state->health == STATUS_VINE_DEAD) {
            ai_spawn_primary(c, MZM_PSPRITE_GERUTA_RED, 0x80, 5, (uint16_t)(s->y + HALF_BLOCK_SIZE), s->x, 0);
            s->status = 0;
            return;
        }
    }
    if (!(s->status & MZM_SPRITE_STATUS_EXISTS)) return;
    sync(c, true);
}

void ai_tangle_vine(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->secondary) {
        if (s->sprite_id == MZM_SSPRITE_TANGLE_VINE_GERUTA_PART && c->f->globals) geruta_part(c);
        else s->status = 0;
        return;
    }
    switch (s->sprite_id) {
    case MZM_PSPRITE_TANGLE_VINE_GERUTA:
        if (c->f->globals) geruta_vine(c);
        else s->status = 0;
        break;
    case MZM_PSPRITE_TANGLE_VINE_RED_GARUTA:
    case MZM_PSPRITE_TANGLE_VINE_LARVA_RIGHT:
    case MZM_PSPRITE_TANGLE_VINE_LARVA_LEFT:
        damaged_sound(s);
        special_vine(c);
        break;
    default:
        damaged_sound(s);
        plain_vine(c);
        break;
    }
}
