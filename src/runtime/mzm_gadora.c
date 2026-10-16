/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Kraid and Ridley Gadoras (src/sprites_ai/gadora.c): the
 * living hatch seal, invisible vulnerable eye, Ridley beam and persistent
 * hatch-opening death event. */
#include "mzm_sprite_ai.h"

enum {
    GADORA_IDLE_INIT = 0x8,
    GADORA_DETECTING = 0x9,
    GADORA_WARNING = 0x23,
    GADORA_OPENING = 0x25,
    GADORA_OPEN = 0x27,
    GADORA_CLOSING = 0x29,
    GADORA_DEATH = 0x67,
    GADORA_EYE_IDLE = 0x9,
    GADORA_EYE_DEATH = 0x67,
    GADORA_BEAM_MOVING = 0x9,
};

static MzmSprite *parent(const Ctx *c) {
    return c->f->sprites && c->s->ram_slot < c->f->sprite_count ?
           &c->f->sprites[c->s->ram_slot] : NULL;
}

static void hatch_exists(const Ctx *c, bool exists) {
    if (c->f->hooks && c->f->hooks->set_hatch_exists)
        c->f->hooks->set_hatch_exists(c->f->hooks->context, c->s->y, c->s->x, exists);
}

static void set_event(const Ctx *c, uint8_t event) {
    if (c->f->hooks && c->f->hooks->set_event)
        c->f->hooks->set_event(c->f->hooks->context, event);
}

static void init_body(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t event = s->sprite_id == MZM_PSPRITE_GADORA_KRAID ?
                    MZM_EVENT_KRAID_GADORA_KILLED : MZM_EVENT_RIDLEY_GADORA_KILLED;
    if (ai_check_event(c, event)) {
        hatch_exists(c, true);
        s->status = 0;
        return;
    }
    if (s->sprite_id == MZM_PSPRITE_GADORA_KRAID)
        s->status |= MZM_SPRITE_STATUS_HIGH_PRIORITY;
    s->status |= MZM_SPRITE_STATUS_X_FLIP;
    s->x = (uint16_t)(s->x + PIXEL_SIZE * 3);
    hatch_exists(c, false);
    if (ai_spawn_secondary(c, MZM_SSPRITE_GADORA_EYE, s->room_slot, s->gfx_slot,
                           s->y, (uint16_t)(s->x + QUARTER_BLOCK_SIZE), 0) < 0) {
        s->status = 0;
        return;
    }
    s->draw_top = s->draw_bottom = 40;
    s->draw_horizontal = 16;
    s->hitbox_top = -(BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE);
    s->hitbox_bottom = BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE;
    s->hitbox_left = -(QUARTER_BLOCK_SIZE + PIXEL_SIZE);
    s->hitbox_right = QUARTER_BLOCK_SIZE + PIXEL_SIZE;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->draw_order = 5;
    s->health = c->tables->health[s->sprite_id];
    if (s->sprite_id == MZM_PSPRITE_GADORA_RIDLEY)
        s->palette_row = s->absolute_palette_row = 1;
    ai_set_anim(s, MZM_GADORA_EYE_CLOSED);
    s->pose = GADORA_IDLE_INIT;
}

static bool samus_in_front(const Ctx *c) {
    const MzmSprite *s = c->s;
    int samus_middle = (int)c->f->samus_y + c->f->samus_hitbox_top / 2;
    int dy = samus_middle - (int)s->y;
    int dx = (int)c->f->samus_x - (int)s->x;
    return dy > -BLOCK_SIZE * 2 && dy < BLOCK_SIZE * 2 &&
           dx >= 0 && dx < BLOCK_SIZE * 7;
}

static void body_death(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose != GADORA_DEATH) {
        s->pose = GADORA_DEATH;
        s->work0 = 40;
        s->samus_collision = SSC_NONE;
        ai_set_anim(s, MZM_GADORA_DEATH);
        return;
    }
    s->ignore_samus_timer = 1;
    if (s->work0) --s->work0;
    if (s->work0 & 3) return;
    if (s->work0 & 4) {
        s->palette_row = (uint8_t)(14 - s->gfx_slot);
        return;
    }
    s->palette_row = s->absolute_palette_row;
    if (s->work0) return;
    hatch_exists(c, true);
    set_event(c, s->sprite_id == MZM_PSPRITE_GADORA_KRAID ?
                 MZM_EVENT_KRAID_GADORA_KILLED : MZM_EVENT_RIDLEY_GADORA_KILLED);
    uint16_t y = s->y;
    uint16_t x = (uint16_t)(s->x + QUARTER_BLOCK_SIZE);
    uint8_t room_slot = s->room_slot;
    uint8_t ram_slot = s->ram_slot;
    ai_sprite_death(c, (uint16_t)(y + BLOCK_SIZE + EIGHTH_BLOCK_SIZE - c->rng),
                    (uint16_t)(x - c->rng), MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
    if (s->status)
        ai_spawn_drop_follower(c, c->rng & 1 ? MZM_PSPRITE_LARGE_ENERGY_DROP :
                                                   MZM_PSPRITE_MISSILE_DROP,
                               room_slot, 0, ram_slot, y, x, 0);
}

static void body_ai(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) { init_body(c); return; }
    if (s->health == 0 || s->pose == SPRITE_POSE_DESTROYED) {
        body_death(c);
        return;
    }
    switch (s->pose) {
    case GADORA_IDLE_INIT:
        ai_set_anim(s, MZM_GADORA_EYE_CLOSED);
        s->work0 = (uint8_t)(61 + c->rng * 4);
        s->pose = GADORA_DETECTING;
        /* fallthrough */
    case GADORA_DETECTING:
        if (s->work0 && --s->work0) break;
        if (!samus_in_front(c)) {
            s->work0 = (uint8_t)(61 + c->rng * 4);
            break;
        }
        s->pose = GADORA_WARNING;
        ai_set_anim(s, MZM_GADORA_WARNING);
        break;
    case GADORA_WARNING:
        if (!ai_animation_ending(c, 1)) break;
        s->pose = GADORA_OPENING;
        ai_set_anim(s, MZM_GADORA_OPENING_EYE);
        mzm_sfx_play(MZM_SOUND_GADORA_OPENING_EYE);
        break;
    case GADORA_OPENING:
        if (!ai_animation_ending(c, 1)) break;
        s->pose = GADORA_OPEN;
        s->work0 = s->sprite_id == MZM_PSPRITE_GADORA_RIDLEY ? 88 : 60;
        ai_set_anim(s, MZM_GADORA_EYE_OPENED);
        if (s->sprite_id == MZM_PSPRITE_GADORA_RIDLEY)
            ai_spawn_secondary(c, MZM_SSPRITE_GADORA_BEAM, 0, s->gfx_slot, s->y,
                               (uint16_t)(s->x + QUARTER_BLOCK_SIZE),
                               MZM_SPRITE_STATUS_X_FLIP);
        break;
    case GADORA_OPEN:
        if (s->work0) {
            --s->work0;
            if (s->anim == MZM_GADORA_EYE_OPENED && ai_animation_ending(c, 1))
                ai_set_anim(s, MZM_GADORA_EYE_MOVING);
            else if (s->work0 == 7)
                ai_set_anim(s, MZM_GADORA_BEFORE_CLOSING);
            break;
        }
        s->pose = GADORA_CLOSING;
        ai_set_anim(s, MZM_GADORA_CLOSING_EYE);
        mzm_sfx_play(MZM_SOUND_GADORA_CLOSING_EYE);
        break;
    case GADORA_CLOSING:
        if (ai_animation_ending(c, 1)) s->pose = GADORA_IDLE_INIT;
        break;
    case GADORA_DEATH:
        body_death(c);
        break;
    default:
        body_death(c);
        break;
    }
}

static void eye_ai(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *body = parent(c);
    s->ignore_samus_timer = 1;
    if (!body || !(body->status & MZM_SPRITE_STATUS_EXISTS)) {
        s->status = 0;
        return;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->health = body->sprite_id == MZM_PSPRITE_GADORA_KRAID ?
                    1 : c->tables->secondary_health[s->sprite_id];
        s->absolute_palette_row = s->palette_row =
            body->sprite_id == MZM_PSPRITE_GADORA_RIDLEY ? 1 : 0;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 8;
        s->hitbox_top = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_bottom = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->hitbox_left = -QUARTER_BLOCK_SIZE;
        s->hitbox_right = QUARTER_BLOCK_SIZE;
        s->draw_order = 4;
        s->samus_collision = SSC_NONE;
        s->pose = GADORA_EYE_IDLE;
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        return;
    }
    if (s->damaged) {
        s->damaged = false;
        ai_sound_onscreen(s, MZM_SOUND_GADORA_EYE_DAMAGED);
    }
    body->palette_row = s->palette_row;
    if (s->health == 0 || s->pose == SPRITE_POSE_DESTROYED) {
        if (s->pose != GADORA_EYE_DEATH) {
            body->pose = SPRITE_POSE_DESTROYED;
            body->health = 0;
            body->ignore_samus_timer = 1;
            s->pose = GADORA_EYE_DEATH;
            s->work0 = 40;
            s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        }
        if (s->work0 && --s->work0) return;
        ai_sprite_death(c, (uint16_t)(s->y + c->rng - BLOCK_SIZE - PIXEL_SIZE),
                        (uint16_t)(s->x + c->rng + QUARTER_BLOCK_SIZE),
                        MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
        return;
    }
    if (body->pose == GADORA_OPEN) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        if ((s->isft & 0x7F) == 16) body->work0 = 0;
    } else {
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    }
}

static void beam_ai(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->kill_off_screen = true;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 16;
        s->hitbox_top = -THREE_QUARTER_BLOCK_SIZE;
        s->hitbox_bottom = THREE_QUARTER_BLOCK_SIZE;
        s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->pose = GADORA_BEAM_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->draw_order = 3;
        s->health = 1;
        s->work0 = 16;
        s->absolute_palette_row = s->palette_row = 1;
        ai_set_anim(s, MZM_GADORA_BEAM_MOVING);
    }
    if (s->work0) {
        if (--s->work0 == 0) mzm_sfx_play(MZM_SOUND_GADORA_BEAM_EMERGING);
        return;
    }
    s->x = (uint16_t)(s->x + PIXEL_SIZE * 3);
    if (!(ai_collision_at(c, s->y, s->x) & MZM_CLIPDATA_SOLID_FLAG)) return;
    ai_particle_set(c, (uint16_t)(s->y + QUARTER_BLOCK_SIZE + PIXEL_SIZE * 3), s->x,
                    MZM_PE_SPRITE_EXPLOSION_BIG);
    ai_sound_onscreen(s, MZM_SOUND_GADORA_BEAM_MOVING);
    s->status = 0;
}

void ai_gadora(Ctx *c) {
    if (!c->s->secondary) body_ai(c);
    else if (c->s->sprite_id == MZM_SSPRITE_GADORA_EYE) eye_ai(c);
    else beam_ai(c);
}
