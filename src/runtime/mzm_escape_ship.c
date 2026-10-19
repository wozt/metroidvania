/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's Chozodia escape: the escape ship Samus leaves in
 * (src/sprites_ai/escape_ship.c) and the two gates of its hangar
 * (src/sprites_ai/escape_gate.c), the first spawning the black pirates and
 * the pirate that chases the ship. */
#include "mzm_sprite_ai.h"
#include "mzm_blocks.h"
#include "mzm_samus.h"

enum {
    SHIP_POSE_CHECK_OPEN = 0x8,
    SHIP_POSE_OPENING = 0x9,
    SHIP_POSE_OPENED = 0x22,
    SHIP_POSE_DELAY_BEFORE_CLOSING = 0x23,
    SHIP_POSE_CLOSING = 0x24,
    SHIP_POSE_CLOSED = 0x25,
    SHIP_POSE_HOVERING = 0x26,
    SHIP_POSE_ESCAPING = 0x27,
    SHIP_POSE_ESCAPED = 0x28,

    PART_TOP = 0,
    PART_TAIL = 2,
    PART_FLAMES = 3,
    PART_POSE_WAIT_FOR_MOVING_TAIL = 0x8,
    PART_POSE_UPDATE_PALETTE = 0x9,
    PART_POSE_UPDATE_TOP = 0xE,
    PART_POSE_UPDATE_FLAMES = 0x32,

    GATE_POSE_LOAD_GRAPHICS = 0x1,
    GATE_POSE_IDLE = 0x9,
    GATE_POSE_CHECK_OPEN = 0x22,
    GATE_POSE_OPENING = 0x23,
    GATE_POSE_OPENED = 0x25,
    GATE_POSE_ESCAPING = 0x27,

    /* The StartEffectForCutscene requests (MzmSpriteGlobals.cutscene_effect,
     * the runtime's own numbering: EFFECT_CUTSCENE_ESCAPING_CHOZODIA and
     * EFFECT_CUTSCENE_SAMUS_IN_BLUE_SHIP). */
    CUTSCENE_ESCAPING_CHOZODIA = 4,
    CUTSCENE_SAMUS_IN_BLUE_SHIP = 5,
};

static MzmSprite *sprite_at(const Ctx *c, uint8_t slot) {
    return c->f->sprites && slot < c->f->sprite_count ? &c->f->sprites[slot] : NULL;
}

static void set_cutscene(const Ctx *c, uint8_t effect) {
    if (c->f->globals) c->f->globals->cutscene_effect = effect;
}

/* SpriteUtilCheckObjectsTouching */
static bool touching(uint16_t o1_top, uint16_t o1_bottom, uint16_t o1_left, uint16_t o1_right,
                     uint16_t o2_top, uint16_t o2_bottom, uint16_t o2_left, uint16_t o2_right) {
    return o2_bottom >= o1_top && o2_top < o1_bottom && o2_right >= o1_left && o2_left < o1_right;
}

/* The flashing row offset both palette animations step through: up to
 * `top`, flagged at 2, then back down to 0. Returns whether it stepped
 * (the DMA of the row). */
static bool step_flash(MzmSprite *s, uint8_t delay, uint8_t top) {
    uint8_t timer = --s->work2;
    if (timer) return false;
    s->work2 = delay;
    if (s->work1 & 0x80) {
        if (s->work1 > 0x80) s->work1--;
        if (s->work1 == 0x80) s->work1 = timer;
    } else {
        if (s->work1 < top) s->work1++;
        if (s->work1 == 2) s->work1 |= 0x80;
    }
    return true;
}

/* EscapeShipPartUpdatePalette: sEscapeShipFlashingPal to OBJ row 9 + slot. */
static void part_update_palette(Ctx *c, uint8_t delay) {
    if (step_flash(c->s, delay, 2) && c->f->globals)
        c->f->globals->escape_ship_palette = (int8_t)(c->s->work1 & 0x7F);
}

/* EscapeShipCheckCollidingWithLaser: the first pirate laser touching the
 * ship bursts. */
static void check_colliding_with_laser(Ctx *c) {
    MzmSprite *s = c->s;
    if (!c->f->sprites) return;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int i = 0; i < c->f->sprite_count; ++i) {
        MzmSprite *l = &c->f->sprites[i];
        if (!(l->status & MZM_SPRITE_STATUS_EXISTS) || l->samus_collision != SSC_SPACE_PIRATE_LASER) continue;
        if (touching(top, bottom, left, right, (uint16_t)(l->y - PIXEL_SIZE), (uint16_t)(l->y + PIXEL_SIZE),
                     (uint16_t)(l->x - PIXEL_SIZE), (uint16_t)(l->x + PIXEL_SIZE))) {
            l->status = 0;
            ai_particle_set(c, l->y, l->x, MZM_PE_HITTING_SOMETHING_INVINCIBLE);
            break;
        }
    }
}

/* EscapeShipPirateCollision: the first pirate the flying ship touches is
 * destroyed. */
static void pirate_collision(Ctx *c) {
    MzmSprite *s = c->s;
    if (!c->f->sprites) return;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int i = 0; i < c->f->sprite_count; ++i) {
        MzmSprite *p = &c->f->sprites[i];
        if (!(p->status & MZM_SPRITE_STATUS_EXISTS) || p->samus_collision != SSC_SPACE_PIRATE) continue;
        if (touching(top, bottom, left, right, (uint16_t)(p->y + p->hitbox_top),
                     (uint16_t)(p->y + p->hitbox_bottom), (uint16_t)(p->x + p->hitbox_left),
                     (uint16_t)(p->x + p->hitbox_right))) {
            p->pose = SPRITE_POSE_DESTROYED;
            break;
        }
    }
}

/* The hovering sway shared by the closed and hovering poses. */
static void sway(Ctx *c) {
    MzmSprite *s = c->s;
    const int16_t *ys = c->tables->escape_ship_hovering_y, *xs = c->tables->escape_ship_hovering_x;
    uint8_t offset = s->work3;
    int16_t movement = ys[offset];
    if (movement == SHORT_MAX) {
        movement = ys[0];
        offset = 0;
    }
    s->work3 = (uint8_t)(offset + 1);
    s->y = (uint16_t)(s->y + movement);
    offset = s->work2;
    movement = xs[offset];
    if (movement == SHORT_MAX) {
        movement = xs[0];
        offset = 0;
    }
    s->work2 = (uint8_t)(offset + 1);
    s->x = (uint16_t)(s->x + movement);
}

/* EscapeShip */
static void ship(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    MzmSpriteSamusLink *link = c->f->samus_link;
    uint16_t y = s->y, x = s->x;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE + PIXEL_SIZE);
        s->hitbox_bottom = 0;
        s->hitbox_left = THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->hitbox_right = BLOCK_SIZE + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->samus_collision = SSC_NONE;
        s->draw_order = 11;
        ai_set_anim(s, MZM_ESCAPE_SHIP_IDLE);
        s->pose = SHIP_POSE_CHECK_OPEN;
        if (g) {
            g->sub1.work3 = 0;
            g->escape_ship_palette = -1;               /* sEscapeShipPal */
        }
        ai_spawn_secondary(c, MZM_SSPRITE_ESCAPE_SHIP_PART, PART_TOP, s->gfx_slot, y, x, 0);
        ai_spawn_secondary(c, MZM_SSPRITE_ESCAPE_SHIP_PART, PART_TAIL, s->gfx_slot, y, x, 0);
        break;
    case SHIP_POSE_CHECK_OPEN: {
        /* Once Mecha Ridley is dead, Samus up to 12 blocks above and less
         * than 4 blocks to its left. */
        uint16_t samus_y = (uint16_t)(c->f->samus_y - 1), samus_x = c->f->samus_x;
        if (ai_check_event(c, MZM_EVENT_MECHA_RIDLEY_KILLED) && samus_y <= y && y - samus_y <= BLOCK_SIZE * 12 &&
            samus_x < x && x - samus_x < BLOCK_SIZE * 4) {
            s->pose = SHIP_POSE_OPENING;
            ai_set_anim(s, MZM_ESCAPE_SHIP_OPENING);
            ai_spawn_primary(c, MZM_PSPRITE_ESCAPE_SHIP_SPACE_PIRATE, s->room_slot, 0, (uint16_t)(s->y - PIXEL_SIZE),
                             (uint16_t)(s->x + BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE), 0);
            mzm_sfx_play(MZM_SOUND_ESCAPE_SHIP_OPENING);
        }
        break;
    }
    case SHIP_POSE_OPENING:
        if (s->frame > 6) {
            s->pose = SHIP_POSE_OPENED;
            s->samus_collision = SSC_CAN_STAND_ON_TOP;
        }
        break;
    case SHIP_POSE_OPENED:
        if (!(s->status & MZM_SPRITE_STATUS_SAMUS_ON_TOP)) break;
        s->standing_on = MZM_STANDING_ON_SPRITE_OFF;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_ON_TOP;
        s->draw_order = 4;
        s->pose = SHIP_POSE_DELAY_BEFORE_CLOSING;
        s->work0 = 20;                                 /* ONE_THIRD_SECOND */
        s->samus_collision = SSC_NONE;
        if (link) {
            if (link->invincibility) link->clear_invincibility = true;
            link->set_pose = link->facing_right ? MZM_POSE_IN_ESCAPE_SHIP : MZM_POSE_TURNING_TO_ENTER_ESCAPE_SHIP;
        }
        {
            const MzmSpriteHooks *h = c->f->hooks;
            if (h && h->set_event) h->set_event(h->context, MZM_EVENT_ESCAPED_CHOZODIA);
        }
        s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
        s->hitbox_bottom = 0;
        s->hitbox_left = 0;
        s->hitbox_right = BLOCK_SIZE * 5;
        break;
    case SHIP_POSE_DELAY_BEFORE_CLOSING:
        if (--s->work0 == 0) {
            s->pose = SHIP_POSE_CLOSING;
            ai_set_anim(s, MZM_ESCAPE_SHIP_CLOSING);
            mzm_sfx_play(MZM_SOUND_ESCAPE_SHIP_CLOSING);
        }
        break;
    case SHIP_POSE_CLOSING:
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_ESCAPE_SHIP_IDLE);
            s->pose = SHIP_POSE_CLOSED;
            s->work0 = 30;                             /* CONVERT_SECONDS(.5f) */
            s->work1 = 90;                             /* CONVERT_SECONDS(1.5f) */
            s->work2 = 0;
            s->work3 = 0;
            if (g) g->sub1.work3 = 1;
        }
        break;
    case SHIP_POSE_CLOSED:
        if (s->work0) {
            uint8_t timer = --s->work0;
            if (timer <= 10) {                         /* CONVERT_SECONDS(1.f / 6) */
                s->x = (uint16_t)(s->x + PIXEL_SIZE / 2);
            } else if (timer == 11) {
                ai_spawn_secondary(c, MZM_SSPRITE_ESCAPE_SHIP_PART, PART_FLAMES, s->gfx_slot, y, x, 0);
                mzm_sfx_play(MZM_SOUND_ESCAPE_SHIP_FLYING);
            } else if (timer == 12) {
                set_cutscene(c, CUTSCENE_SAMUS_IN_BLUE_SHIP);
            }
            break;
        }
        s->work1--;
        if (s->work1 < 60) {                           /* CONVERT_SECONDS(1.f) */
            s->y = (uint16_t)(s->y - ONE_SUB_PIXEL);
            if (s->work1 == 0) {
                s->pose = SHIP_POSE_HOVERING;
                s->work0 = 200;                        /* CONVERT_SECONDS(3.f) + ONE_THIRD_SECOND */
                s->work1 = 100;                        /* CONVERT_SECONDS(1.f) + TWO_THIRD_SECOND */
            }
        } else if (s->work1 <= 66) {                   /* CONVERT_SECONDS(1.1f) */
            s->x = (uint16_t)(s->x + ONE_SUB_PIXEL);
        }
        sway(c);
        y = s->y;
        x = s->x;
        break;
    case SHIP_POSE_HOVERING:
        if (s->work0) {
            if (--s->work0 == 0) {
                ai_set_anim(s, MZM_ESCAPE_SHIP_FLYING);
                mzm_sfx_play(MZM_SOUND_ESCAPE_SHIP_SHUTTER_OPENING);
            } else if (s->work0 == 100 && g) {         /* CONVERT_SECONDS(1.f) + TWO_THIRD_SECOND */
                g->sub1.work3 = 2;
            }
            s->y = (uint16_t)(s->y - PIXEL_SIZE / 2);
        } else if (--s->work1 == 0) {
            s->pose = SHIP_POSE_ESCAPING;
            s->y_spawn = 200;                          /* CONVERT_SECONDS(3.f) + ONE_THIRD_SECOND */
            s->work1 = 4;                              /* CONVERT_SECONDS(1.f / 15) */
            mzm_sfx_play(MZM_SOUND_ESCAPE_SHIP_FLYING_FAST);
        }
        sway(c);
        y = s->y;
        x = s->x;
        break;
    case SHIP_POSE_ESCAPING:
        if (s->work1 < 64) s->work1++;                 /* CONVERT_SECONDS(1.f) + CONVERT_SECONDS(1.f / 15) */
        s->x = (uint16_t)(s->x + s->work1 / 2);
        if (--s->y_spawn == 0) {
            s->pose = SHIP_POSE_ESCAPED;
            set_cutscene(c, CUTSCENE_ESCAPING_CHOZODIA);
        }
        break;
    default: break;
    }
    if (s->pose <= SHIP_POSE_OPENED) return;
    /* Samus rides in the cockpit. */
    if (link) {
        link->y = (uint16_t)(y - (HALF_BLOCK_SIZE + 11));
        link->x = (uint16_t)(x + BLOCK_SIZE + QUARTER_BLOCK_SIZE);
        link->moved = true;
    }
    /* EscapeShipSetIgnoreSamus */
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i)
        if (c->f->sprites[i].status & MZM_SPRITE_STATUS_EXISTS) c->f->sprites[i].ignore_samus_timer = 15;
    check_colliding_with_laser(c);
    /* EscapeShipSetPirateDrawOrder */
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *p = &c->f->sprites[i];
        if (p->status & MZM_SPRITE_STATUS_EXISTS && p->samus_collision == SSC_SPACE_PIRATE) p->draw_order = 2;
    }
    if (s->pose > SHIP_POSE_HOVERING && s->work1 > 16) /* CONVERT_SECONDS(.25f) + 1 */
        pirate_collision(c);
}

/* EscapeShipPart: the top over Samus, the tail with the flashing lights
 * and the flames, following the ship. */
static void ship_part(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *owner = sprite_at(c, s->ram_slot);
    if (!owner) {
        s->status = 0;
        return;
    }
    s->y = owner->y;
    s->x = owner->x;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->always_active = true;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->frame = 0;
        s->adc = 0;
        s->samus_collision = SSC_NONE;
        if (s->room_slot == PART_TOP) {
            s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
            s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_order = 10;
            ai_set_anim(s, MZM_ESCAPE_SHIP_PART_TOP);
            s->pose = PART_POSE_UPDATE_TOP;
        } else if (s->room_slot == PART_TAIL) {
            s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
            s->draw_bottom = 0;
            s->draw_horizontal = BLOCK_SIZE * 7 / PIXEL_SIZE;
            s->draw_order = 13;
            ai_set_anim(s, MZM_ESCAPE_SHIP_PART_TAIL);
            s->pose = PART_POSE_WAIT_FOR_MOVING_TAIL;
            s->work1 = 0;
            s->work2 = 4;                              /* CONVERT_SECONDS(1.f / 15) */
        } else if (s->room_slot == PART_FLAMES) {
            s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
            s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE * 6 / PIXEL_SIZE;
            s->draw_order = 14;
            ai_set_anim(s, MZM_ESCAPE_SHIP_PART_FLAMES);
            s->pose = PART_POSE_UPDATE_FLAMES;
        }
        break;
    case PART_POSE_UPDATE_TOP:
        /* In front of Samus once she is in. */
        s->draw_order = owner->draw_order == 4 ? 3 : 10;
        break;
    case PART_POSE_WAIT_FOR_MOVING_TAIL:
        if (owner->pose == SHIP_POSE_CLOSED && owner->work0 == 1) {
            ai_set_anim(s, MZM_ESCAPE_SHIP_PART_TAIL_MOVING);
            s->pose = PART_POSE_UPDATE_PALETTE;
        }
        part_update_palette(c, 8);
        break;
    case PART_POSE_UPDATE_PALETTE: {
        uint8_t delay = 8;                             /* CONVERT_SECONDS(2.f / 15) */
        if (owner->pose >= SHIP_POSE_HOVERING) {
            s->adc++;
            delay = 4;                                 /* CONVERT_SECONDS(1.f / 15) */
        }
        part_update_palette(c, delay);
        break;
    }
    case PART_POSE_UPDATE_FLAMES:
        if (owner->pose > SHIP_POSE_HOVERING) s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        break;
    default: break;
    }
}

/* EscapeGateChangeCcaa: the gate's two columns of four blocks. */
static void gate_blocks(Ctx *c, uint8_t caa) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (!h || !h->apply_caa) return;
    uint16_t y = (uint16_t)(c->s->y - HALF_BLOCK_SIZE), x = (uint16_t)(c->s->x - HALF_BLOCK_SIZE);
    for (int column = 0; column < 2; ++column, x = (uint16_t)(x + BLOCK_SIZE))
        for (int i = 0; i < 4; ++i) h->apply_caa(h->context, (uint16_t)(y - BLOCK_SIZE * i), x, caa);
}

/* SpriteUtilCountPrimarySprites */
static int count_primary(const Ctx *c, uint8_t sprite_id) {
    int count = 0;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *p = &c->f->sprites[i];
        if (p->status & MZM_SPRITE_STATUS_EXISTS && !p->secondary && p->sprite_id == sprite_id) count++;
    }
    return count;
}

/* EscapeGate: the first gate opens once its black pirates are dead and
 * Samus is near, the second once the ship hovers. */
static void gate(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    bool first = s->sprite_id == MZM_PSPRITE_ESCAPE_GATE1;
    uint8_t delay = 16;                                /* CONVERT_SECONDS(.25f) + 1 */
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        s->x = (uint16_t)(s->x + HALF_BLOCK_SIZE);
        s->draw_top = (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->samus_collision = SSC_NONE;
        ai_set_anim(s, MZM_ESCAPE_GATE_CLOSED);
        s->pose = GATE_POSE_LOAD_GRAPHICS;
        s->work1 = 0;
        s->work2 = 1;
        s->draw_order = 3;
        if (g) g->sub1.work3 = 0;
        gate_blocks(c, MZM_CAA_MAKE_SOLID_GRIPPABLE);
        break;
    case GATE_POSE_LOAD_GRAPHICS:
        if (first) {
            /* sEscapeGateAndTimerGfx and its palette in the last OBJ row,
             * then the two black pirates. */
            if (g) g->escape_gate_palette = -1;
            ai_spawn_primary(c, MZM_PSPRITE_BLACK_SPACE_PIRATE, 0x80, s->gfx_slot, s->y,
                             (uint16_t)(s->x - BLOCK_SIZE * 2), 0);
            ai_spawn_primary(c, MZM_PSPRITE_BLACK_SPACE_PIRATE, 0x80, s->gfx_slot, s->y,
                             (uint16_t)(s->x - BLOCK_SIZE * 10), MZM_SPRITE_STATUS_X_FLIP);
        }
        s->pose = GATE_POSE_IDLE;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        break;
    case GATE_POSE_IDLE:
        if (first ? count_primary(c, MZM_PSPRITE_BLACK_SPACE_PIRATE) == 0 : g && g->sub1.work3 == 2)
            s->pose = GATE_POSE_CHECK_OPEN;
        break;
    case GATE_POSE_CHECK_OPEN:
        if (!first || ai_samus_nslr(c, BLOCK_SIZE, BLOCK_SIZE * 6) == NSLR_LEFT) {
            ai_set_anim(s, MZM_ESCAPE_GATE_OPENING);
            s->pose = GATE_POSE_OPENING;
            mzm_sfx_play(MZM_SOUND_ESCAPE_GATE_OPENING);
        }
        break;
    case GATE_POSE_OPENING:
        delay = 3;
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_ESCAPE_GATE_OPENED);
            s->pose = GATE_POSE_OPENED;
        } else if (s->frame == 4 && s->adc == 10) {   /* CONVERT_SECONDS(1.f / 6) */
            gate_blocks(c, MZM_CAA_REMOVE_SOLID);
        }
        break;
    case GATE_POSE_OPENED:
        if (!first) return;
        if (g && g->sub1.work3 == 1) {
            /* The pirate that chases the ship. */
            ai_spawn_primary(c, MZM_PSPRITE_SPACE_PIRATE_WAITING1, 0x80 | 0x1, 0, s->y,
                             (uint16_t)(s->x - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)), 0);
            s->pose = GATE_POSE_ESCAPING;
        }
        break;
    case GATE_POSE_ESCAPING:
        if (!first) return;
        if (g && g->sub1.work3 == 2) delay = 3;
        break;
    default: break;
    }
    /* sEscapeGateFlashingPal to the last OBJ row. */
    if (first && step_flash(s, delay, 5) && g) g->escape_gate_palette = (int8_t)(s->work1 & 0x7F);
}

void ai_escape_ship(Ctx *c) {
    if (c->s->secondary) {
        if (c->s->sprite_id == MZM_SSPRITE_ESCAPE_SHIP_PART) ship_part(c);
        else c->s->status = 0;
        return;
    }
    if (c->s->sprite_id == MZM_PSPRITE_ESCAPE_SHIP) ship(c);
    else gate(c);
}
