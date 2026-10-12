/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Chozodia security systems that ring the alarm: searchlights
 * (src/sprites_ai/searchlight.c), security lasers (security_laser.c) and
 * searchlight eyes with their beam and projectile (searchlight_eye.c);
 * comments name the pinned decompilation's routines. Sounds and the BG0
 * transparency (TransparencyUpdateBldcnt / TransparencySpriteUpdateBldalpha,
 * whose fade state is kept in work2 / work3) are not ported. */
#include "mzm_sprite_ai.h"

#include <stddef.h>

enum {
    SEARCHLIGHT_POSE_IDLE = 0x9,
    SEARCHLIGHT_POSE_ACTIVATING = 0x23,
    SEARCHLIGHT_POSE_ACTIVATE_ALARM = 0x25,
    SECURITY_LASER_POSE_IDLE = 0x9,
    SECURITY_LASER_POSE_DISAPPEARING = 0xB,
    SECURITY_LASER_POSE_UNUSED = 0xC,
    SECURITY_LASER_POSE_SET_ALARM = 0x23,
    EYE_POSE_IDLE = 0x9,
    EYE_POSE_GETTING_ALERTED = 0x23,
    EYE_POSE_ALERTED = 0x25,
    EYE_POSE_SHOOTING = 0x27,
    BEAM_POSE_IDLE = 0x9,
    BEAM_POSE_GETTING_ALERTED = 0x23,
    PROJECTILE_POSE_MOVING = 0x9,
    /* work1 of a security laser: its size, and the vertical flag. */
    LASER_SMALL = 0x2,
    LASER_VERTICAL = 0x80,
    BEAM_SIZE = BLOCK_SIZE * 7,
    BLDALPHA_MAX_VALUE = 16,
    ALARM_ACTIVE = MZM_ALARM_TIMER_ACTIVE,
    TWO_THIRD_SECOND = 40,
    SIXTH_SECOND = 10,
    MZM_EVENT_FULLY_POWERED_SUIT_OBTAINED = 67,
};

static MzmSpriteGlobals *globals(const Ctx *c) {
    return c->f->globals;
}

/* Every existing Space Pirate reacts to the tripped alarm (MOSAIC). */
static void mark_pirates(const Ctx *c) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *o = &c->f->sprites[i];
        if (o->status & MZM_SPRITE_STATUS_EXISTS && o->samus_collision == SSC_SPACE_PIRATE)
            o->status |= MZM_SPRITE_STATUS_MOSAIC;
    }
}

/* The fade toward the alarm's full blend (BLDALPHA), one step per frame;
 * back to half without the alarm. */
static void fade(const Ctx *c) {
    MzmSprite *s = c->s;
    if (globals(c)->alarm_timer) {
        if (s->work2) {
            s->work2--;
            if (!s->work2 && s->work3 < BLDALPHA_MAX_VALUE) {
                s->work3++;
                s->work2 = true;
            }
        }
    } else if (s->work3 != BLDALPHA_MAX_VALUE / 2) {
        s->work3 = BLDALPHA_MAX_VALUE / 2;
        s->work2 = true;
    }
}

/* The stop-enemy movement of the block at a point. */
static bool stop_enemy(const Ctx *c, uint16_t y, uint16_t x) {
    return ai_movement_at(c, y, x) == MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID;
}

/* Searchlight: sweeps diagonally between stop-enemy blocks, flickering. */
static void searchlight(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = globals(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        if (g->alarm_timer) {
            s->status = 0;
            break;
        }
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -THREE_QUARTER_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = THREE_QUARTER_BLOCK_SIZE;
        ai_set_anim(s, MZM_SECURITY_SEARCHLIGHT_MOVING);
        s->samus_collision = SSC_CHECK_COLLIDING;
        s->pose = SEARCHLIGHT_POSE_IDLE;
        /* Drawn behind BG1 with draw order 1 (not reproduced). */
        if (s->sprite_id == MZM_PSPRITE_SEARCHLIGHT)
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT | MZM_SPRITE_STATUS_FACING_DOWN;
        else if (s->sprite_id == MZM_PSPRITE_SEARCHLIGHT2)
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        else if (s->sprite_id == MZM_PSPRITE_SEARCHLIGHT3)
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        break;
    case SEARCHLIGHT_POSE_IDLE:
        s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) g->alarm_timer = ALARM_ACTIVE;
        if (g->alarm_timer) {
            s->pose = SEARCHLIGHT_POSE_ACTIVATING;
            s->work0 = SIXTH_SECOND;
            s->samus_collision = SSC_NONE;
            break;
        }
        if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
            if (stop_enemy(c, s->y, (uint16_t)(s->x + BLOCK_SIZE)))
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            else
                s->x = (uint16_t)(s->x + PIXEL_SIZE / 2);
        } else {
            if (stop_enemy(c, s->y, (uint16_t)(s->x - BLOCK_SIZE)))
                s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            else
                s->x = (uint16_t)(s->x - PIXEL_SIZE / 2);
        }
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
            if (stop_enemy(c, (uint16_t)(s->y + BLOCK_SIZE), s->x))
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
            else
                s->y = (uint16_t)(s->y + PIXEL_SIZE / 2);
        } else {
            if (stop_enemy(c, (uint16_t)(s->y - BLOCK_SIZE), s->x))
                s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
            else
                s->y = (uint16_t)(s->y - PIXEL_SIZE / 2);
        }
        break;
    case SEARCHLIGHT_POSE_ACTIVATING:
        s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
        if (!--s->work0) {
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
            s->pose = SEARCHLIGHT_POSE_ACTIVATE_ALARM;
        }
        break;
    case SEARCHLIGHT_POSE_ACTIVATE_ALARM:
        g->alarm_timer = ALARM_ACTIVE;
        break;
    }
}

/* The OAM table of a security laser size (work1), or -1. */
static int laser_anim(uint8_t size, bool disappearing) {
    int index = (size & ~LASER_VERTICAL) - LASER_SMALL;
    if (index < 0 || index > 3) return -1;
    return MZM_SECURITY_LASER_VERTICAL_SMALL + (size & LASER_VERTICAL ? 0 : 4) + index +
           (disappearing ? 8 : 0);
}

/* SecurityLaserInit: the beam reaches the first block above (vertical) or
 * to the right (horizontal), 2 to 5 blocks long. */
static void security_laser_init(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = s->y, x = s->x;
    int size = 5;
    if (s->sprite_id == MZM_PSPRITE_SECURITY_LASER_VERTICAL ||
        s->sprite_id == MZM_PSPRITE_SECURITY_LASER_VERTICAL2) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        for (int n = 2; n < 5; ++n)
            if (ai_collision_at(c, (uint16_t)(y - (BLOCK_SIZE * n + HALF_BLOCK_SIZE)), x) !=
                    MZM_COLLISION_AIR) {
                size = n;
                break;
            }
        s->draw_top = (uint8_t)(BLOCK_SIZE * size / PIXEL_SIZE);
        s->hitbox_top = (int16_t)-(BLOCK_SIZE * (size - 1) + HALF_BLOCK_SIZE + PIXEL_SIZE);
        s->work1 = (uint8_t)(LASER_VERTICAL | size);
        s->draw_bottom = 0;
        s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_bottom = 0;
        s->hitbox_left = -EIGHTH_BLOCK_SIZE;
        s->hitbox_right = EIGHTH_BLOCK_SIZE;
    } else {
        for (int n = 2; n < 5; ++n)
            if (ai_collision_at(c, (uint16_t)(y - HALF_BLOCK_SIZE),
                                (uint16_t)(x + BLOCK_SIZE * n)) != MZM_COLLISION_AIR) {
                size = n;
                break;
            }
        s->draw_horizontal = (uint8_t)((BLOCK_SIZE * (size - 1) + HALF_BLOCK_SIZE) / PIXEL_SIZE);
        s->hitbox_right = (int16_t)(BLOCK_SIZE * (size - 1) + PIXEL_SIZE);
        s->work1 = (uint8_t)size;
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->hitbox_top = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_bottom = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_left = -PIXEL_SIZE;
    }
    ai_set_anim(s, (uint8_t)laser_anim(s->work1, false));
    s->samus_collision = SSC_CHECK_COLLIDING;
    if (globals(c)->alarm_timer) {
        s->pose = SECURITY_LASER_POSE_SET_ALARM;
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        s->work2 = false;
        s->work3 = BLDALPHA_MAX_VALUE;
    } else {
        s->pose = SECURITY_LASER_POSE_IDLE;
        s->work2 = true;
        s->work3 = BLDALPHA_MAX_VALUE / 2;
    }
}

/* A table change of the security laser (an unknown size removes it). */
static void security_laser_anim(MzmSprite *s, bool disappearing) {
    int anim = laser_anim(s->work1, disappearing);
    if (anim < 0) s->status = 0;
    else ai_set_anim(s, (uint8_t)anim);
}

static void security_laser(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = globals(c);
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: security_laser_init(c); break;
    case SECURITY_LASER_POSE_IDLE:
        /* SecurityLaserIdle: tripped by Samus, or gone with the alarm. */
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            g->alarm_timer = ALARM_ACTIVE;
            mark_pirates(c);
        } else if (!g->alarm_timer) {
            break;
        }
        s->pose = SECURITY_LASER_POSE_DISAPPEARING;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        s->ignore_samus_timer = 1;
        security_laser_anim(s, true);
        if (s->status) mzm_sfx_play(MZM_SOUND_SECURITY_LASER_TRIPPED);
        break;
    case SECURITY_LASER_POSE_DISAPPEARING:
        s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 1)) {
            s->pose = SECURITY_LASER_POSE_SET_ALARM;
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
            security_laser_anim(s, false);
        }
        break;
    case SECURITY_LASER_POSE_SET_ALARM:
        s->ignore_samus_timer = 1;
        g->alarm_timer = ALARM_ACTIVE;
        break;
    case SECURITY_LASER_POSE_UNUSED:
        s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 1)) {
            s->pose = SECURITY_LASER_POSE_IDLE;
            security_laser_anim(s, false);
        }
        break;
    }
    if (s->sprite_id == MZM_PSPRITE_SECURITY_LASER_VERTICAL ||
        s->sprite_id == MZM_PSPRITE_SECURITY_LASER_HORIZONTAL)
        fade(c);
}

/* SearchlightEyeInit: on the wall beside it (its right, or flipped on its
 * left), with its beam reaching seven blocks out. */
static void eye_init(Ctx *c) {
    MzmSprite *s = c->s;
    if (!ai_check_event(c, MZM_EVENT_FULLY_POWERED_SUIT_OBTAINED)) s->immune_to_projectiles = true;
    s->draw_top = s->draw_bottom = (THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) / PIXEL_SIZE;
    s->draw_horizontal = THREE_QUARTER_BLOCK_SIZE / PIXEL_SIZE;
    s->hitbox_top = -HALF_BLOCK_SIZE;
    s->hitbox_bottom = HALF_BLOCK_SIZE;
    s->hitbox_left = -(QUARTER_BLOCK_SIZE - PIXEL_SIZE);
    s->hitbox_right = QUARTER_BLOCK_SIZE - PIXEL_SIZE;
    ai_set_anim(s, MZM_SECURITY_EYE_IDLE);
    s->samus_collision = SSC_NONE;
    s->health = c->tables->health[s->sprite_id];
    if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), (uint16_t)(s->x + BLOCK_SIZE)) !=
            MZM_COLLISION_AIR) {
        s->x = (uint16_t)(s->x + HALF_BLOCK_SIZE);
    } else {
        s->status |= MZM_SPRITE_STATUS_X_FLIP;
        s->x = (uint16_t)(s->x - HALF_BLOCK_SIZE);
    }
    if (s->sprite_id == MZM_PSPRITE_SEARCHLIGHT_EYE) s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    if (globals(c)->alarm_timer) {
        s->pose = EYE_POSE_GETTING_ALERTED;
        s->work0 = TWO_THIRD_SECOND;
        return;
    }
    s->pose = EYE_POSE_IDLE;
    if (s->status & MZM_SPRITE_STATUS_X_FLIP)
        ai_spawn_secondary(c, MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2, 0, s->gfx_slot, s->y,
                           (uint16_t)(s->x + BEAM_SIZE), MZM_SPRITE_STATUS_X_FLIP);
    else
        ai_spawn_secondary(c, MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2, 0, s->gfx_slot, s->y,
                           (uint16_t)(s->x - BEAM_SIZE), 0);
}

/* SearchlightEyeMove: a subpixel a frame up or down its wall, turning at
 * stop-enemy blocks. */
static void eye_move(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = s->y;
    uint16_t x = (uint16_t)(s->status & MZM_SPRITE_STATUS_X_FLIP ? s->x - QUARTER_BLOCK_SIZE
                                                                 : s->x + QUARTER_BLOCK_SIZE);
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        s->y++;
        if (stop_enemy(c, (uint16_t)(y + HALF_BLOCK_SIZE), x))
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    } else {
        s->y--;
        if (stop_enemy(c, (uint16_t)(y - HALF_BLOCK_SIZE), x))
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    }
}

static void searchlight_eye(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: eye_init(c); break;
    case EYE_POSE_IDLE:
        if (globals(c)->alarm_timer) {
            s->pose = EYE_POSE_GETTING_ALERTED;
            s->work0 = TWO_THIRD_SECOND;
        } else {
            eye_move(c);
        }
        break;
    case EYE_POSE_GETTING_ALERTED:
        if (!--s->work0) s->pose = EYE_POSE_ALERTED;
        break;
    case EYE_POSE_ALERTED:
        eye_move(c);
        if (s->work0) {
            s->work0--;
        } else {
            ai_set_anim(s, MZM_SECURITY_EYE_SHOOTING);
            s->pose = EYE_POSE_SHOOTING;
            s->work0 = 30;          /* CONVERT_SECONDS(.5f) */
        }
        break;
    case EYE_POSE_SHOOTING:
        eye_move(c);
        if (s->work0) {
            s->work0--;
            break;
        }
        ai_set_anim(s, MZM_SECURITY_EYE_IDLE);
        s->pose = EYE_POSE_ALERTED;
        s->work0 = 90;              /* CONVERT_SECONDS(1.5f) */
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN)
            ai_spawn_secondary(c, MZM_SSPRITE_SEARCHLIGHT_EYE_PROJECTILE, 0, s->gfx_slot, s->y,
                               s->x, s->status & MZM_SPRITE_STATUS_X_FLIP);
        globals(c)->alarm_timer = ALARM_ACTIVE;
        break;
    default:
        ai_sprite_death(c, (uint16_t)(s->y + QUARTER_BLOCK_SIZE), s->x,
                        MZM_PE_SPRITE_EXPLOSION_BIG);
        break;
    }
}

/* SearchlightEyeBeamDetectSamus: Samus in the beam (and not behind the
 * first wall it crosses) trips the alarm. */
static void beam_detect(Ctx *c, const MzmSprite *eye) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = globals(c);
    s->y = eye->y;
    if (g->alarm_timer) {
        s->status = 0;
        return;
    }
    if (!(s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING)) return;
    uint16_t y = s->y;
    const int reach = BLOCK_SIZE * 6 + HALF_BLOCK_SIZE - PIXEL_SIZE;
    bool flipped = s->status & MZM_SPRITE_STATUS_X_FLIP;
    uint16_t x = (uint16_t)(flipped ? s->x - reach : s->x + reach);
    bool found = false;
    for (int i = 0; i <= BEAM_SIZE / BLOCK_SIZE; ++i) {
        if (ai_collision_at(c, y, x) != MZM_COLLISION_AIR) {
            found = true;
            break;
        }
        x = (uint16_t)(flipped ? x + BLOCK_SIZE : x - BLOCK_SIZE);
    }
    if (found) {
        if (flipped) {
            x = (uint16_t)((x & BLOCK_POSITION_FLAG) + BLOCK_SIZE);
            if (c->f->samus_x > x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        } else {
            x = x & BLOCK_POSITION_FLAG;
            if (c->f->samus_x < x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        }
    }
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        s->pose = BEAM_POSE_GETTING_ALERTED;
        s->work0 = TWO_THIRD_SECOND;
        s->samus_collision = SSC_NONE;
        g->alarm_timer = ALARM_ACTIVE;
        mark_pirates(c);
    }
}

static void searchlight_eye_beam(Ctx *c) {
    MzmSprite *s = c->s;
    const MzmSprite *eye = c->f->sprites && s->ram_slot < c->f->sprite_count
                               ? &c->f->sprites[s->ram_slot] : NULL;
    if (!eye || !(eye->status & MZM_SPRITE_STATUS_EXISTS)) {
        s->status = 0;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->draw_top = s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BEAM_SIZE / PIXEL_SIZE;
        s->hitbox_top = -EIGHTH_BLOCK_SIZE;
        s->hitbox_bottom = EIGHTH_BLOCK_SIZE;
        s->hitbox_left = -BEAM_SIZE;
        s->hitbox_right = BEAM_SIZE;
        /* BG priority 3 and draw order 12 (behind everything; not
         * reproduced). */
        ai_set_anim(s, MZM_SECURITY_EYE_BEAM_IDLE);
        s->pose = BEAM_POSE_IDLE;
        s->samus_collision = SSC_CHECK_COLLIDING;
        break;
    case BEAM_POSE_IDLE: beam_detect(c, eye); break;
    case BEAM_POSE_GETTING_ALERTED:
        /* SearchlightEyeBeamDisappear: blinks every 4 frames, then goes. */
        s->ignore_samus_timer = 1;
        if (!(s->work0 & 3)) s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
        if (!--s->work0) s->status = 0;
        break;
    }
}

/* SearchlightEyeProjectile: 12 subpixels a frame until a wall or Samus,
 * then a medium explosion. */
static void searchlight_eye_projectile(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->kill_off_screen = true;
        s->draw_top = s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_bottom = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        ai_set_anim(s, MZM_SECURITY_EYE_PROJECTILE_MOVING);
        s->pose = PROJECTILE_POSE_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            s->hitbox_left = 0;
            s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        } else {
            s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
            s->hitbox_right = 0;
        }
        ai_sound_onscreen(s, MZM_SOUND_SEARCHLIGHT_EYE_PROJECTILE_EMERGING);
        /* fallthrough */
    case PROJECTILE_POSE_MOVING:
        if (s->status & MZM_SPRITE_STATUS_X_FLIP)
            s->x = (uint16_t)(s->x + QUARTER_BLOCK_SIZE - PIXEL_SIZE);
        else
            s->x = (uint16_t)(s->x - (QUARTER_BLOCK_SIZE - PIXEL_SIZE));
        if (ai_collision_at(c, s->y, s->x) != MZM_COLLISION_AIR) s->pose = SPRITE_POSE_STOPPED;
        break;
    default:
        ai_particle_set(c, (uint16_t)(s->y + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE), s->x,
                        MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        ai_sound_onscreen(s, MZM_SOUND_SPRITE_EXPLOSION_MEDIUM);
        s->status = 0;
        break;
    }
}

void ai_chozodia_security(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->secondary) {
        if (s->sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2) searchlight_eye_beam(c);
        else searchlight_eye_projectile(c);
        return;
    }
    switch (s->sprite_id) {
    case MZM_PSPRITE_SEARCHLIGHT:
    case MZM_PSPRITE_SEARCHLIGHT2:
    case MZM_PSPRITE_SEARCHLIGHT3:
    case MZM_PSPRITE_SEARCHLIGHT4: searchlight(c); break;
    case MZM_PSPRITE_SEARCHLIGHT_EYE:
    case MZM_PSPRITE_SEARCHLIGHT_EYE2: searchlight_eye(c); break;
    default: security_laser(c); break;
    }
}
