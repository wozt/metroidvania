/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Polyps (src/sprites_ai/polyp.c): lava plants that knock
 * Samus back on contact and spit a projectile arcing towards her every two
 * seconds while on screen; comments name the pinned decompilation's
 * routines. The exploding projectile's BG priority is not modelled. */
#include "mzm_sprite_ai.h"

enum {
    POLYP_POSE_IDLE_INIT = 0x8,
    POLYP_POSE_IDLE = 0x9,
    POLYP_POSE_WARNING = 0x23,
    POLYP_POSE_SPITTING = 0x25,
    POLYP_POSE_AFTER_SPITTING = 0x27,
    POLYP_POSE_RETRACTING = 0x29,
    POLYP_PROJECTILE_POSE_SPAWN = 0x8,
    POLYP_PROJECTILE_POSE_MOVING = 0x9,
    POLYP_PROJECTILE_POSE_EXPLODING = 0x43,
    POLYP_SHOOT_DELAY = 120,            /* CONVERT_SECONDS(2.f) */
    POLYP_SPIT_DURATION = 20,           /* ONE_THIRD_SECOND */
};

/* SpriteUtilCountChildSecondarySprites */
static int count_child_secondaries(const Ctx *c, uint8_t sprite_id) {
    int count = 0;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS && n->secondary && n->sprite_id == sprite_id &&
            n->ram_slot == c->s->ram_slot)
            ++count;
    }
    return count;
}

static void polyp_idle_init(MzmSprite *s) {
    s->pose = POLYP_POSE_IDLE;
    ai_set_anim(s, MZM_POLYP_IDLE);
    s->work0 = POLYP_SHOOT_DELAY;
}

/* Polyp */
static void polyp(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* PolypInit */
        s->hitbox_top = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_bottom = 0;
        s->hitbox_left = -(BLOCK_SIZE - EIGHTH_BLOCK_SIZE);
        s->hitbox_right = BLOCK_SIZE - EIGHTH_BLOCK_SIZE;
        s->draw_top = s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->samus_collision = SSC_KNOCKS_BACK_SAMUS;
        s->health = c->tables->health[s->sprite_id];
        /* fallthrough */
    case POLYP_POSE_IDLE_INIT:
        polyp_idle_init(s);
        /* fallthrough */
    case POLYP_POSE_IDLE:
        /* PolypCheckSpawnProjectile: on screen, without a projectile out
         * or a drop of its own. */
        if (--s->work0 != 0) break;
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN && count_child_secondaries(c, MZM_SSPRITE_POLYP_PROJECTILE) == 0 &&
            !ai_has_drops(c)) {
            s->pose = POLYP_POSE_WARNING;
            ai_set_anim(s, MZM_POLYP_WARNING);
            mzm_sfx_play(MZM_SOUND_POLYP_WARNING);
        } else {
            s->work0 = POLYP_SHOOT_DELAY;
        }
        break;
    case POLYP_POSE_WARNING:
        if (ai_animation_ending(c, 1)) {
            s->pose = POLYP_POSE_SPITTING;
            ai_set_anim(s, MZM_POLYP_SPITTING);
            s->work0 = POLYP_SPIT_DURATION;
        }
        break;
    case POLYP_POSE_SPITTING:
        /* PolypSpawnProjectile */
        if (--s->work0 == 0) {
            s->pose = POLYP_POSE_AFTER_SPITTING;
            ai_set_anim(s, MZM_POLYP_AFTER_SPITTING);
        } else if (s->work0 == POLYP_SPIT_DURATION / 5) {
            ai_spawn_secondary(c, MZM_SSPRITE_POLYP_PROJECTILE, s->room_slot, s->gfx_slot,
                               (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x, 0);
        }
        break;
    case POLYP_POSE_AFTER_SPITTING:
        if (ai_animation_ending(c, 1)) {
            s->pose = POLYP_POSE_RETRACTING;
            ai_set_anim(s, MZM_POLYP_RETRACTING);
        }
        break;
    case POLYP_POSE_RETRACTING:
        if (ai_animation_ending(c, 2)) s->pose = POLYP_POSE_IDLE_INIT;
        break;
    default:
        ai_sprite_death(c, (uint16_t)(s->y - QUARTER_BLOCK_SIZE), s->x, MZM_PE_SPRITE_EXPLOSION_BIG);
        break;
    }
}

/* PolypProjectile */
static void projectile(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: {
        /* PolypProjectileInit: towards Samus when near, else at random. */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->kill_off_screen = true;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -QUARTER_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE;
        s->adc = s->frame = 0;
        s->pose = POLYP_PROJECTILE_POSE_SPAWN;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->draw_order = 5;
        s->health = c->tables->secondary_health[s->sprite_id];
        uint8_t nslr = ai_samus_nslr(c, BLOCK_SIZE * 5,
                                     BLOCK_SIZE * 8 + HALF_BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2);
        if (nslr == NSLR_RIGHT) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        else if (nslr != NSLR_LEFT) ai_random_x_direction(c);
        s->anim = s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? MZM_POLYP_PROJECTILE_RIGHT
                                                             : MZM_POLYP_PROJECTILE_LEFT;
        s->work0 = 4;                           /* CONVERT_SECONDS(1.f / 15) */
        s->work3 = 0;
        break;
    }
    case POLYP_PROJECTILE_POSE_SPAWN:
        if (--s->work0 == 0) {
            s->pose = POLYP_PROJECTILE_POSE_MOVING;
            s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            if (s->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play(MZM_SOUND_POLYP_SPITTING);
        }
        break;
    case POLYP_PROJECTILE_POSE_MOVING: {
        /* PolypProjectileMove: sPolypProjectileYVelocity, a pixel a frame
         * sideways, stopped by a wall or the floor. */
        const int16_t *table = c->tables->polyp_projectile_y;
        int16_t movement = s->work3 < 44 ? table[s->work3] : SHORT_MAX;
        if (movement == SHORT_MAX) {
            s->y = (uint16_t)(s->y + table[s->work3 - 1]);
        } else {
            s->work3++;
            s->y = (uint16_t)(s->y + movement);
        }
        s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? s->x + PIXEL_SIZE : s->x - PIXEL_SIZE);
        if (ai_collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0) s->pose = SPRITE_POSE_STOPPED;
        break;
    }
    case SPRITE_POSE_STOPPED:
        /* PolypProjectileExplodingInit */
        ai_set_anim(s, MZM_POLYP_PROJECTILE_EXPLODING);
        s->pose = POLYP_PROJECTILE_POSE_EXPLODING;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play(MZM_SOUND_POLYP_PROJECTILE_EXPLODING);
        /* fallthrough */
    case POLYP_PROJECTILE_POSE_EXPLODING:
        s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 1)) s->status = 0;
        break;
    default:
        ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
        break;
    }
}

void ai_polyp(Ctx *c) {
    if (c->s->secondary) projectile(c);
    else polyp(c);
}
