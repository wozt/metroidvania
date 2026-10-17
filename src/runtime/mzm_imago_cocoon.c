/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Imago cocoon actors (src/sprites_ai/imago_cocoon.c).
 * This first slice covers the tunnel discovery trigger and the persistent
 * remains used after the cocoon and Imago fights. */
#include "mzm_sprite_ai.h"

enum {
    PASSAGE_TRIGGER_IDLE = 0x8,
    COCOON_REMAINS_IDLE = 0x9,
};

static void set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *hooks = c->f->hooks;
    if (hooks && hooks->set_event) hooks->set_event(hooks->context, event);
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
        if (c->s->sprite_id == MZM_SSPRITE_DEFEATED_IMAGO_COCOON) defeated_cocoon(c);
        else if (c->s->sprite_id == MZM_SSPRITE_IMAGO_CEILING_VINE) ceiling_vine(c);
        return;
    }
    if (c->s->sprite_id == MZM_PSPRITE_DISCOVERED_IMAGO_PASSAGE_EVENT_TRIGGER)
        passage_trigger(c);
    else if (c->s->sprite_id == MZM_PSPRITE_IMAGO_COCOON_AFTER_FIGHT)
        cocoon_after_fight(c);
}
