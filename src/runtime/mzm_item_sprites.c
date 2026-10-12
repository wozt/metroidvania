/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission item sprites outside the Chozo statues: the Morph Ball
 * (src/sprites_ai/morph_ball.c), the Power Grip (power_grip.c) and the
 * Charge Beam (charge_beam.c), with their secondary sprites; comments name
 * the pinned decompilation's routines. */
#include "mzm_items.h"
#include "mzm_sprite_ai.h"

enum {
    /* PrimarySprite */
    MORPH_BALL = 33, POWER_GRIP = 76, CHARGE_BEAM = 68,
    /* SecondarySprite */
    SSPRITE_MORPH_BALL_OUTSIDE = 19, SSPRITE_POWER_GRIP_GLOW = 70, SSPRITE_CHARGE_BEAM_GLOW = 5,
    /* Message */
    MESSAGE_CHARGE_BEAM = 9, MESSAGE_MORPH_BALL = 16, MESSAGE_POWER_GRIP = 21,
    /* Event */
    EVENT_POWER_GRIP_OBTAINED = 16, EVENT_CHARGE_BEAM_OBTAINED = 20,
    /* The item poses (MORPH_BALL_POSE_*, POWER_GRIP_POSE_*, CHARGE_BEAM_POSE_*). */
    POSE_IDLE = 0x9, POSE_SPAWN_GLOW = 0xA, POSE_IDLE_INIT = 0xB, POSE_BEING_ACQUIRED = 0x23,
    /* The secondaries' idle pose. */
    GLOW_POSE_IDLE = 0x9,
};

static MzmSprite *primary(const Ctx *c) {
    return c->f->sprites && c->s->ram_slot < c->f->sprite_count ? &c->f->sprites[c->s->ram_slot]
                                                                : NULL;
}

static bool has_item(const Ctx *c, uint32_t item) {
    return c->f->equipment && c->f->equipment->items & item;
}

static void spawn_banner(const Ctx *c, uint8_t message) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->spawn_message) h->spawn_message(h->context, message);
}

static void set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, event);
}

/* MorphBallGet / the Power Grip and Charge Beam pick-ups: Samus touched the
 * item. */
static void acquire(Ctx *c, uint32_t item, uint8_t message) {
    MzmSprite *s = c->s;
    if (c->f->prevent_movement) *c->f->prevent_movement = MZM_SAMUS_ITEM_PMT;
    s->always_active = true;
    s->ignore_samus_timer = 1;
    s->pose = POSE_BEING_ACQUIRED;
    s->work0 = 0;
    if (c->f->equipment) c->f->equipment->items |= item;
    spawn_banner(c, message);
}

/* MorphBallFlashAnim (and the others'): the flicker, whose timer never
 * moves, until the banner lets Samus go. */
static void flash(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if ((s->work0 & 1) == 0) s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
    if (c->f->prevent_movement && *c->f->prevent_movement < MZM_SAMUS_ITEM_PMT - 1) s->status = 0;
}

/* MorphBall */
static void morph_ball(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* MorphBallInit */
        if (has_item(c, MZM_ITEM_MORPH_BALL)) {
            s->status = 0;
            return;
        }
        s->immune_to_projectiles = true;
        s->hitbox_top = s->hitbox_left = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE - PIXEL_SIZE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        ai_set_anim(s, MZM_ITEM_SPRITE_MORPH_BALL);
        s->samus_collision = SSC_CHECK_COLLIDING;
        s->health = 1;
        s->y -= HALF_BLOCK_SIZE;
        s->pose = POSE_IDLE;
        s->draw_order = 3;
        ai_spawn_secondary(c, SSPRITE_MORPH_BALL_OUTSIDE, s->room_slot, s->gfx_slot, s->y, s->x, 0);
        break;
    case POSE_IDLE:
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING)
            acquire(c, MZM_ITEM_MORPH_BALL, MESSAGE_MORPH_BALL);
        break;
    case POSE_BEING_ACQUIRED:
        flash(c);
        break;
    default:
        break;
    }
}

/* MorphBallOutside: its visibility follows the ball's. */
static void morph_ball_outside(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* MorphBallOutsideInit */
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        ai_set_anim(s, MZM_ITEM_SPRITE_MORPH_BALL_OUTSIDE);
        s->samus_collision = SSC_NONE;
        s->draw_order = 2;
        s->pose = GLOW_POSE_IDLE;
        /* fallthrough */
    case GLOW_POSE_IDLE: {
        /* MorphBallOutsideFlashAnim */
        const MzmSprite *p = primary(c);
        if (!p) break;
        s->status = p->status;
        if (p->always_active) s->always_active = true;
        break;
    }
    default:
        break;
    }
}

/* PowerGrip */
static void power_grip(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        if (has_item(c, MZM_ITEM_POWER_GRIP)) {
            s->status = 0;
            break;
        }
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE - PIXEL_SIZE;
        ai_set_anim(s, MZM_ITEM_SPRITE_POWER_GRIP);
        s->samus_collision = SSC_CHECK_COLLIDING;
        s->health = 1;
        s->y -= BLOCK_SIZE;
        s->pose = POSE_IDLE;
        ai_spawn_secondary(c, SSPRITE_POWER_GRIP_GLOW, s->room_slot, s->gfx_slot, s->y, s->x, 0);
        break;
    case POSE_IDLE:
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            acquire(c, MZM_ITEM_POWER_GRIP, MESSAGE_POWER_GRIP);
            set_event(c, EVENT_POWER_GRIP_OBTAINED);
        }
        break;
    case POSE_BEING_ACQUIRED:
        flash(c);
        break;
    default:
        break;
    }
}

/* PowerGripGlow: removed once the Power Grip is being acquired. */
static void power_grip_glow(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        ai_set_anim(s, MZM_ITEM_SPRITE_POWER_GRIP_GLOW);
        s->samus_collision = SSC_NONE;
        s->pose = GLOW_POSE_IDLE;
        s->draw_order = 5;
        break;
    case GLOW_POSE_IDLE: {
        const MzmSprite *p = primary(c);
        if (!p || p->pose >= POSE_BEING_ACQUIRED) s->status = 0;
        break;
    }
    default:
        break;
    }
}

/* ChargeBeam */
static void charge_beam(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* ChargeBeamInit */
        if (has_item(c, MZM_ITEM_CHARGE_BEAM)) {
            s->status = 0;
            return;
        }
        s->immune_to_projectiles = true;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->hitbox_top = s->hitbox_left = -(QUARTER_BLOCK_SIZE + PIXEL_SIZE * 3);
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE + PIXEL_SIZE * 3;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        ai_set_anim(s, MZM_ITEM_SPRITE_CHARGE_BEAM_SPAWNING);
        s->samus_collision = SSC_NONE;
        s->health = 1;
        s->work3 = 0;               /* CHARGE_BEAM_IDLE_MOVEMENT_OFFSET */
        s->pose = POSE_SPAWN_GLOW;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->draw_order = 2;
        break;
    case POSE_SPAWN_GLOW:
        /* ChargeBeamSpawnGlow */
        s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 2)) {
            s->pose = POSE_IDLE_INIT;
            int slot = ai_spawn_secondary(c, SSPRITE_CHARGE_BEAM_GLOW, 0, s->gfx_slot, s->y, s->x, 0);
            s->work1 = slot < 0 ? 0xFF : (uint8_t)slot;     /* CHARGE_BEAM_SPRITE_SLOT */
        }
        break;
    case POSE_IDLE_INIT:
        /* ChargeBeamVisibleInit */
        s->ignore_samus_timer = 1;
        s->pose = POSE_IDLE;
        ai_set_anim(s, MZM_ITEM_SPRITE_CHARGE_BEAM_VISIBLE);
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_CHECK_COLLIDING;
        s->work3 = 0;
        break;
    case POSE_IDLE: {
        /* ChargeBeamIdle: sChargeBeamIdleYMovement */
        uint8_t offset = s->work3;
        int16_t velocity = offset < 66 ? c->tables->charge_beam_idle_y[offset] : SHORT_MAX;
        if (velocity == SHORT_MAX) {
            velocity = c->tables->charge_beam_idle_y[0];
            offset = 0;
        }
        s->work3 = (uint8_t)(offset + 1);
        s->y = (uint16_t)(s->y + velocity);
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            if (c->f->sprites && s->work1 < c->f->sprite_count) c->f->sprites[s->work1].status = 0;
            acquire(c, MZM_ITEM_CHARGE_BEAM, MESSAGE_CHARGE_BEAM);
            s->adc = 0;
            s->frame = 0;
            set_event(c, EVENT_CHARGE_BEAM_OBTAINED);
        }
        break;
    }
    case POSE_BEING_ACQUIRED:
        /* ChargeBeamFlashingAnim: the animation held. */
        if (s->adc) s->adc--;
        flash(c);
        break;
    default:
        break;
    }
}

/* ChargeBeamGlow: follows the Charge Beam's height. */
static void charge_beam_glow(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* ChargeBeamGlowInit */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_order = 3;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
        ai_set_anim(s, MZM_ITEM_SPRITE_CHARGE_BEAM_GLOW);
        s->samus_collision = SSC_NONE;
        s->pose = GLOW_POSE_IDLE;
        /* fallthrough */
    case GLOW_POSE_IDLE: {
        /* ChargeBeamGlowMovement */
        const MzmSprite *p = primary(c);
        if (p) s->y = p->y;
        break;
    }
    default:
        break;
    }
}

void ai_item_sprite(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->secondary) {
        if (s->sprite_id == SSPRITE_MORPH_BALL_OUTSIDE) morph_ball_outside(c);
        else if (s->sprite_id == SSPRITE_POWER_GRIP_GLOW) power_grip_glow(c);
        else if (s->sprite_id == SSPRITE_CHARGE_BEAM_GLOW) charge_beam_glow(c);
        return;
    }
    if (s->sprite_id == MORPH_BALL) morph_ball(c);
    else if (s->sprite_id == POWER_GRIP) power_grip(c);
    else if (s->sprite_id == CHARGE_BEAM) charge_beam(c);
}
