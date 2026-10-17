/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Imago cocoon actors (src/sprites_ai/imago_cocoon.c): the
 * active multi-sprite cocoon, its destructible vines and spore nests, the
 * tunnel discovery trigger, and the persistent post-fight remains. */
#include "mzm_sprite_ai.h"

enum {
    COCOON_IDLE = 0x8,
    COCOON_FALLING_BEFORE_BLOCKS = 0x9,
    COCOON_UNLOCK_PASSAGE = 0x25,
    COCOON_IN_GROUND = 0x27,

    VINE_IDLE = 0x8,
    VINE_SPAWN_SPORES = 0xE,
    VINE_CEILING_IDLE = 0x42,
    VINE_CEILING_DEATH = 0x43,

    SPORE_SPAWNING = 0x9,
    SPORE_NEST = 0x23,
    SPORE_MOVING = 0x25,
    SPORE_EXPLODING_INIT = 0x42,
    SPORE_EXPLODING = 0x43,

    PASSAGE_TRIGGER_IDLE = 0x8,
    COCOON_REMAINS_IDLE = 0x9,
};

enum {
    PART_DECORATIONS_LEFT,
    PART_VINE_LEFT_MIDDLE,
    PART_VINE_RIGHT_MIDDLE,
    PART_CEILING_VINE,
    PART_COCOON,
    PART_DECORATIONS_RIGHT,
    PART_VINE_RIGHT_RIGHT,
    PART_VINE_LEFT_LEFT,
    PART_VINE_RIGHT_LEFT,
    PART_VINE_LEFT_RIGHT,
};

enum { MUSIC_STARTED, MUSIC_FADING, MUSIC_WAITING };

static MzmSubSpriteState *cocoon_state(Ctx *c) {
    return &c->f->globals->sub1;
}

static const MzmMultiSprite *cocoon_multi(const Ctx *c, const MzmSubSpriteState *state) {
    uint8_t id = state->multi < MZM_IMAGO_COCOON_MULTI_COUNT ?
                 state->multi : MZM_IMAGO_COCOON_MULTI_IDLE;
    return &c->tables->imago_cocoon_multi[id];
}

static const int16_t *cocoon_part(const Ctx *c, const MzmSubSpriteState *state,
                                  uint8_t part) {
    uint8_t frame = state->frame < MZM_MULTI_FRAMES ? state->frame : 0;
    return cocoon_multi(c, state)->parts[frame][part < MZM_MULTI_PARTS ? part : 0];
}

static void set_cocoon_multi(MzmSubSpriteState *state, uint8_t id) {
    state->multi = id;
    state->frame = 0;
    state->adc = 0;
}

/* SpriteUtilUpdateSubSprite1Anim. */
static void update_cocoon_multi(const Ctx *c, MzmSubSpriteState *state) {
    const MzmMultiSprite *multi = cocoon_multi(c, state);
    state->adc++;
    if (multi->timers[state->frame] < state->adc) {
        state->adc = 1;
        state->frame++;
        if (state->frame >= MZM_MULTI_FRAMES || multi->timers[state->frame] == 0)
            state->frame = 0;
    }
}

static void sync_cocoon_part(Ctx *c, const MzmSubSpriteState *state) {
    const int16_t *part = cocoon_part(c, state, c->s->room_slot);
    if (part[0] >= 0 && part[0] < MZM_IMAGO_COCOON_ANIMS && c->s->anim != part[0])
        ai_set_anim(c->s, (uint8_t)part[0]);
    c->s->y = (uint16_t)(state->y + part[1]);
    c->s->x = (uint16_t)(state->x + part[2]);
}

static void door_timer(const Ctx *c, int8_t timer) {
    if (c->f->hooks && c->f->hooks->door_unlock_timer)
        c->f->hooks->door_unlock_timer(c->f->hooks->context, timer);
}

static void set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *hooks = c->f->hooks;
    if (hooks && hooks->set_event) hooks->set_event(hooks->context, event);
}

static void change_scaling(MzmSprite *s, uint16_t limit, uint16_t value) {
    if (s->work2) {
        if (s->scaling > 256u - limit) s->scaling -= value;
        else s->work2 = 0;
    } else {
        if (s->scaling < 256u + limit) s->scaling += value;
        else s->work2 = 1;
    }
}

static void spawn_vines(Ctx *c, uint16_t y, uint16_t x) {
    static const uint8_t parts[] = {
        PART_VINE_LEFT_MIDDLE, PART_VINE_RIGHT_MIDDLE, PART_DECORATIONS_LEFT,
        PART_CEILING_VINE, PART_DECORATIONS_RIGHT, PART_VINE_RIGHT_RIGHT,
        PART_VINE_LEFT_LEFT, PART_VINE_RIGHT_LEFT, PART_VINE_LEFT_RIGHT,
    };
    for (size_t i = 0; i < sizeof parts; ++i)
        ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_COCOON_VINE, parts[i], c->s->gfx_slot,
                           y, x, 0);
}

/* ImagoCocoonInit and the combat's idle phase. */
static void active_cocoon_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = cocoon_state(c);
    state->y = s->y;
    state->x = s->x;
    s->y_spawn = s->y;
    s->draw_top = 56;
    s->draw_bottom = 24;
    s->draw_horizontal = 40;
    s->scaling = 256;
    s->rotation = 0;
    s->room_slot = PART_COCOON;
    set_cocoon_multi(state, MZM_IMAGO_COCOON_MULTI_IDLE);

    if (ai_check_event(c, MZM_EVENT_IMAGO_COCOON_KILLED)) {
        ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_CEILING_VINE, 0, s->gfx_slot,
                           state->y, state->x, 0);
        if (ai_check_event(c, MZM_EVENT_ENTER_RIDLEY_DEMO_PLAYED)) {
            s->status = 0;
            return;
        }
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES |
                     MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE;
        s->samus_collision = SSC_NONE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->draw_order = 4;
        ai_set_anim(s, MZM_IMAGO_COCOON_STATIC);
        s->pose = COCOON_IN_GROUND;
        state->y = (uint16_t)(state->y + BLOCK_SIZE * 15);
        return;
    }

    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->hitbox_bottom = 0;
    s->hitbox_left = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
    s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE;
    s->draw_order = 6;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = c->tables->health[s->sprite_id];
    s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE;
    s->work2 = 0;
    state->work1 = state->work2 = 0;
    state->health = 6;
    s->pose = COCOON_IDLE;
    door_timer(c, 1);
    spawn_vines(c, state->y, state->x);
    c->f->globals->boss_work1 = MUSIC_WAITING;
}

static void active_cocoon_idle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = cocoon_state(c);
    if (c->f->globals->boss_work1 == MUSIC_WAITING &&
        s->status & MZM_SPRITE_STATUS_ONSCREEN && c->f->samus_y > state->y &&
        c->f->samus_y - state->y > BLOCK_SIZE * 8) {
        mzm_sfx_music_fade(80);
        c->f->globals->boss_work1 = MUSIC_FADING;
    } else if (c->f->globals->boss_work1 == MUSIC_FADING && c->f->samus_x > state->x &&
               c->f->samus_x - state->x < BLOCK_SIZE * 7) {
        mzm_sfx_music_play(MZM_MUSIC_IMAGO_COCOON_BATTLE, 0);
        c->f->globals->boss_work1 = MUSIC_STARTED;
    }
    if (s->adc == 1 && (s->frame == 0 || s->frame == 2))
        mzm_sfx_play(MZM_SOUND_IMAGO_COCOON_IDLE);
    if (state->health < 4) {
        s->palette_row = s->absolute_palette_row = 1;
        if ((c->frame8 & 63) == 0) change_scaling(s, 1, 1);
    }
    if (state->health == 0) {
        s->palette_row = s->absolute_palette_row = 2;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_NONE;
        set_cocoon_multi(state, MZM_IMAGO_COCOON_MULTI_DYING);
        s->work0 = s->work3 = 0;
        s->pose = COCOON_FALLING_BEFORE_BLOCKS;
        set_event(c, MZM_EVENT_IMAGO_COCOON_KILLED);
        mzm_sfx_play(MZM_SOUND_IMAGO_COCOON_VINES_CRACKING);
    }
}

static void active_cocoon(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSubSpriteState *state = cocoon_state(c);
    if (s->pose == SPRITE_POSE_UNINITIALIZED) active_cocoon_init(c);
    else if (s->pose == COCOON_IDLE) active_cocoon_idle(c);
    else if (s->pose == COCOON_IN_GROUND && (c->frame8 & 3) == 0) {
        change_scaling(s, 4, 1);
        state->y = (uint16_t)(state->y + (s->work2 ? -1 : 1));
    }
    if (!s->status) return;
    update_cocoon_multi(c, state);
    sync_cocoon_part(c, state);
}

static void vine_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->draw_order = 4;
    switch (s->room_slot) {
    case PART_VINE_LEFT_MIDDLE:
        s->draw_top = s->draw_bottom = 32; s->draw_horizontal = 16;
        s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE); s->hitbox_bottom = BLOCK_SIZE;
        s->hitbox_left = -QUARTER_BLOCK_SIZE; s->hitbox_right = QUARTER_BLOCK_SIZE;
        break;
    case PART_VINE_RIGHT_MIDDLE:
        s->draw_top = 24; s->draw_bottom = 32; s->draw_horizontal = 16;
        s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_bottom = BLOCK_SIZE + HALF_BLOCK_SIZE;
        s->hitbox_left = -QUARTER_BLOCK_SIZE; s->hitbox_right = QUARTER_BLOCK_SIZE;
        break;
    case PART_VINE_RIGHT_RIGHT:
        s->draw_top = 24; s->draw_bottom = 32; s->draw_horizontal = 16;
        s->hitbox_top = -BLOCK_SIZE; s->hitbox_bottom = BLOCK_SIZE + HALF_BLOCK_SIZE;
        s->hitbox_left = -QUARTER_BLOCK_SIZE; s->hitbox_right = QUARTER_BLOCK_SIZE;
        break;
    case PART_VINE_LEFT_LEFT:
        s->draw_top = 32; s->draw_bottom = 40; s->draw_horizontal = 16;
        s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE); s->hitbox_bottom = BLOCK_SIZE * 2;
        s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        break;
    case PART_VINE_RIGHT_LEFT:
        s->draw_top = s->draw_bottom = s->draw_horizontal = 16;
        s->hitbox_top = -(BLOCK_SIZE - EIGHTH_BLOCK_SIZE);
        s->hitbox_bottom = BLOCK_SIZE - EIGHTH_BLOCK_SIZE;
        s->hitbox_left = -QUARTER_BLOCK_SIZE; s->hitbox_right = QUARTER_BLOCK_SIZE;
        s->draw_order = 7;
        break;
    case PART_VINE_LEFT_RIGHT:
        s->draw_top = s->draw_bottom = 16; s->draw_horizontal = 8;
        s->hitbox_top = -(BLOCK_SIZE - EIGHTH_BLOCK_SIZE);
        s->hitbox_bottom = BLOCK_SIZE - EIGHTH_BLOCK_SIZE;
        s->hitbox_left = -QUARTER_BLOCK_SIZE; s->hitbox_right = QUARTER_BLOCK_SIZE;
        s->draw_order = 7;
        break;
    case PART_DECORATIONS_LEFT:
    case PART_DECORATIONS_RIGHT:
        s->draw_top = 56; s->draw_bottom = s->room_slot == PART_DECORATIONS_LEFT ? 16 : 24;
        s->draw_horizontal = s->room_slot == PART_DECORATIONS_LEFT ? 32 : 40;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->samus_collision = SSC_NONE;
        s->draw_order = s->room_slot == PART_DECORATIONS_LEFT ? 3 : 8;
        s->pose = VINE_SPAWN_SPORES;
        s->y_spawn = 0;
        return;
    case PART_CEILING_VINE:
        s->draw_top = 8; s->draw_bottom = 40; s->draw_horizontal = 32;
        s->hitbox_top = -HALF_BLOCK_SIZE; s->hitbox_bottom = BLOCK_SIZE * 2 + HALF_BLOCK_SIZE;
        s->hitbox_left = -HALF_BLOCK_SIZE; s->hitbox_right = HALF_BLOCK_SIZE;
        s->health = 1; s->samus_collision = SSC_HURTS_SAMUS; s->draw_order = 5;
        s->pose = VINE_CEILING_IDLE; s->frozen_row_offset = 5;
        s->immune_to_projectiles = true;
        return;
    default:
        s->status = 0;
        return;
    }
    s->health = c->tables->secondary_health[s->sprite_id];
    s->samus_collision = SSC_HURTS_SAMUS;
    s->pose = VINE_IDLE;
}

static void vine_death(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = s->y;
    if (s->room_slot == PART_VINE_RIGHT_MIDDLE || s->room_slot == PART_VINE_RIGHT_RIGHT ||
        s->room_slot == PART_VINE_LEFT_LEFT) y = (uint16_t)(y + HALF_BLOCK_SIZE);
    else if (s->room_slot == PART_VINE_RIGHT_LEFT || s->room_slot == PART_VINE_LEFT_RIGHT)
        y = (uint16_t)(y + QUARTER_BLOCK_SIZE);
    else if (s->room_slot != PART_VINE_LEFT_MIDDLE) { s->status = 0; return; }
    ai_particle_set(c, y, s->x, MZM_PE_SPRITE_EXPLOSION_HUGE);
    MzmSubSpriteState *state = cocoon_state(c);
    if (state->health) state->health--;
    s->status = 0;
}

static void spawn_spore_group(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->room_slot != PART_DECORATIONS_LEFT || cocoon_state(c)->health == 0) return;
    s->y_spawn++;
    if ((s->y_spawn & 255u) != 0) return;
    uint16_t status = s->y_spawn & 256u ? 0 : MZM_SPRITE_STATUS_X_FLIP;
    uint16_t y = (uint16_t)(s->y - (status ? 0x90 : 0x60));
    uint16_t x = (uint16_t)(s->x + (status ? 0x64 : -0x46));
    uint8_t parent_slot = (uint8_t)(s - c->f->sprites);
    for (uint8_t part = 0; part < 8; ++part) {
        int slot = ai_spawn_secondary(c, MZM_SSPRITE_IMAGO_COCOON_SPORE, part,
                                      s->gfx_slot, y, x, status);
        if (slot >= 0) c->f->sprites[slot].ram_slot = parent_slot;
    }
}

static void active_vine(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) vine_init(c);
    else if (s->pose == SPRITE_POSE_DESTROYED) { vine_death(c); return; }
    else if (s->pose == VINE_IDLE && s->damaged) {
        s->damaged = false;
        ai_sound_onscreen_once(s, MZM_SOUND_IMAGO_COCOON_VINE_DAMAGED);
    } else if (s->pose == VINE_SPAWN_SPORES) spawn_spore_group(c);
    else if (s->pose == VINE_CEILING_IDLE && cocoon_state(c)->health == 0 &&
             cocoon_state(c)->frame > 7) {
        s->ignore_samus_timer = 1;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_NONE;
        s->pose = VINE_CEILING_DEATH;
    }
    if (s->status) sync_cocoon_part(c, cocoon_state(c));
}

static MzmSprite *spore_parent(Ctx *c) {
    return c->f->sprites && c->s->ram_slot < c->f->sprite_count ?
           &c->f->sprites[c->s->ram_slot] : NULL;
}

static bool sync_spore(Ctx *c) {
    MzmSprite *parent = spore_parent(c);
    if (!parent || !(parent->status & MZM_SPRITE_STATUS_EXISTS)) {
        c->s->status = 0;
        return false;
    }
    if (c->s->status & MZM_SPRITE_STATUS_X_FLIP) {
        c->s->y = (uint16_t)(parent->y - 0x90);
        c->s->x = (uint16_t)(parent->x + 0x64);
    } else {
        c->s->y = (uint16_t)(parent->y - 0x60);
        c->s->x = (uint16_t)(parent->x - 0x46);
    }
    return true;
}

static void spore_init(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->room_slot == 0) s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->draw_top = s->draw_bottom = s->draw_horizontal = 12;
    s->hitbox_top = s->hitbox_left = -EIGHTH_BLOCK_SIZE;
    s->hitbox_bottom = s->hitbox_right = EIGHTH_BLOCK_SIZE;
    ai_set_anim(s, MZM_IMAGO_COCOON_SPORE_SPAWNING);
    s->pose = SPORE_SPAWNING;
    s->samus_collision = SSC_NONE;
    s->draw_order = 3;
    s->health = c->tables->secondary_health[s->sprite_id];
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->ignore_samus_timer = 1;
    sync_spore(c);
}

static void spore_explode(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    ai_set_anim(s, MZM_IMAGO_COCOON_SPORE_EXPLODING);
    s->pose = SPORE_EXPLODING;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
}

static void move_spore(Ctx *c) {
    MzmSprite *s = c->s;
    int movement = (s->room_slot & 1u) ? 3 : PIXEL_SIZE;
    switch (s->room_slot) {
    case 1: s->y -= movement; s->x += movement; break;
    case 2: s->x += movement; break;
    case 3: s->y += movement; s->x += movement; break;
    case 4: s->y -= movement; break;
    case 5: s->y += movement; s->x -= movement; break;
    case 6: s->x -= movement; break;
    case 7: s->y -= movement; s->x -= movement; break;
    default: s->y += movement; break;
    }
    if (ai_collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0)
        s->pose = SPORE_EXPLODING_INIT;
}

static void active_spore(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) spore_init(c);
    else if (s->pose == SPORE_SPAWNING) {
        if (!sync_spore(c)) return;
        if (cocoon_state(c)->health == 0) s->pose = SPORE_EXPLODING_INIT;
        else {
            if (s->room_slot == 0 && s->status & MZM_SPRITE_STATUS_ONSCREEN &&
                s->frame == 0 && s->adc == 1)
                mzm_sfx_play(MZM_SOUND_IMAGO_COCOON_SPORE_SPAWNING);
            if (ai_animation_ending(c, 1)) {
                ai_set_anim(s, MZM_IMAGO_COCOON_SPORE_NEST);
                s->pose = SPORE_NEST;
                s->work0 = 60;
            }
        }
    } else if (s->pose == SPORE_NEST) {
        if (!sync_spore(c)) return;
        if (cocoon_state(c)->health == 0) s->pose = SPORE_EXPLODING_INIT;
        else if (s->work0 && --s->work0 == 0) {
            s->status &= (uint16_t)~(MZM_SPRITE_STATUS_NOT_DRAWN |
                                      MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
            ai_set_anim(s, MZM_IMAGO_COCOON_SPORE_MOVING);
            s->kill_off_screen = true;
            s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
            s->pose = SPORE_MOVING;
            if (s->room_slot == 0 && s->status & MZM_SPRITE_STATUS_ONSCREEN)
                mzm_sfx_play(MZM_SOUND_IMAGO_COCOON_SPORE_NEST_SPAWNING);
        }
    } else if (s->pose == SPORE_MOVING) move_spore(c);
    else if (s->pose == SPORE_EXPLODING_INIT) spore_explode(c);
    else if (s->pose == SPORE_EXPLODING) {
        s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 1)) s->status = 0;
    } else {
        ai_sprite_death_full(c, DEATH_NORMAL, s->y, s->x, true,
                             MZM_PE_SPRITE_EXPLOSION_MEDIUM);
    }
}

static void passage_trigger(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        if (ai_check_event(c, MZM_EVENT_IMAGO_TUNNEL_DISCOVERED)) {
            s->status = 0;
            return;
        }
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_CHECK_COLLIDING;
        s->draw_top = 16;
        s->draw_bottom = 0;
        s->draw_horizontal = 8;
        s->hitbox_top = -BLOCK_SIZE;
        s->hitbox_bottom = 0;
        s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_right = HALF_BLOCK_SIZE;
        s->pose = PASSAGE_TRIGGER_IDLE;
        ai_set_anim(s, MZM_IMAGO_COCOON_STATIC);
    }
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        set_event(c, MZM_EVENT_IMAGO_TUNNEL_DISCOVERED);
        s->status = 0;
    }
}

static void defeated_cocoon(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose != SPRITE_POSE_UNINITIALIZED) return;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->draw_top = 12;
    s->draw_bottom = 40;
    s->draw_horizontal = 48;
    s->hitbox_top = s->hitbox_left = -MZM_PIXEL_SIZE;
    s->hitbox_bottom = s->hitbox_right = MZM_PIXEL_SIZE;
    ai_set_anim(s, MZM_IMAGO_COCOON_DEFEATED);
    s->samus_collision = SSC_NONE;
    s->pose = COCOON_REMAINS_IDLE;
    s->frozen_row_offset = 5;
}

static void ceiling_vine(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose != SPRITE_POSE_UNINITIALIZED) return;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->draw_top = 12;
    s->draw_bottom = 40;
    s->draw_horizontal = 48;
    s->hitbox_top = s->hitbox_left = -MZM_PIXEL_SIZE;
    s->hitbox_bottom = s->hitbox_right = MZM_PIXEL_SIZE;
    ai_set_anim(s, MZM_IMAGO_COCOON_CEILING_VINE_BROKEN);
    s->samus_collision = SSC_NONE;
    s->draw_order = 5;
    s->pose = COCOON_REMAINS_IDLE;
    s->frozen_row_offset = 5;
}

static void cocoon_after_fight(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->draw_top = 48;
        s->draw_bottom = 16;
        s->draw_horizontal = 32;
        s->hitbox_top = s->hitbox_left = -MZM_PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = MZM_PIXEL_SIZE;
        ai_set_anim(s, MZM_IMAGO_COCOON_DEFEATED);
        s->samus_collision = SSC_NONE;
        s->pose = COCOON_REMAINS_IDLE;
    }
    if (s->adc == 1 && (s->frame == 0 || s->frame == c->kind->counts[s->anim] / 2))
        mzm_sfx_play(MZM_SOUND_IMAGO_COCOON_IN_GROUND_IDLE);
}

void ai_imago_cocoon(Ctx *c) {
    if (c->s->secondary) {
        if (c->s->sprite_id == MZM_SSPRITE_IMAGO_COCOON_VINE) active_vine(c);
        else if (c->s->sprite_id == MZM_SSPRITE_IMAGO_COCOON_SPORE) active_spore(c);
        else if (c->s->sprite_id == MZM_SSPRITE_DEFEATED_IMAGO_COCOON) defeated_cocoon(c);
        else if (c->s->sprite_id == MZM_SSPRITE_IMAGO_CEILING_VINE) ceiling_vine(c);
        return;
    }
    if (c->s->sprite_id == MZM_PSPRITE_IMAGO_COCOON) active_cocoon(c);
    else if (c->s->sprite_id == MZM_PSPRITE_DISCOVERED_IMAGO_PASSAGE_EVENT_TRIGGER)
        passage_trigger(c);
    else if (c->s->sprite_id == MZM_PSPRITE_IMAGO_COCOON_AFTER_FIGHT)
        cocoon_after_fight(c);
}
