/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Metroids (src/sprites_ai/metroid.c): they lock their room,
 * grow out of nothing once on screen, chase Samus with an inertia that
 * bounces off walls and other Metroids, latch on her from above and drain
 * her until a bomb shakes them off; frozen, missiles kill them, and the
 * last one unlocks the room and sets its event. Comments name the pinned
 * decompilation's routines. MetroidDoorLock (an unused sprite) is not
 * ported. */
#include "mzm_projectiles.h"
#include "mzm_sprite_ai.h"

enum {
    METROID_POSE_CHECK_SPAWN = 0x1,
    METROID_POSE_SPAWNING = 0x2,
    METROID_POSE_MOVING_INIT = 0x8,
    METROID_POSE_MOVING = 0x9,
    METROID_POSE_GRAB_SAMUS_INIT = 0x42,
    METROID_POSE_SAMUS_GRABBED = 0x43,
    METROID_POSE_DEATH = 0x62,
    METROID_SHELL_POSE_IDLE = 0x9,
    METROID_GRAB_PALETTES = 5,          /* ARRAY_SIZE(sMetroidPal_SamusGrabbed) / PAL_ROW */
    HDMOVING_LEFT = 1, HDMOVING_RIGHT = 2,
};

/* The room's metroid event (EVENT_*_METROID_ROOM_CLEARED by gCurrentRoom;
 * 0: not a Metroid room). */
static uint8_t room_event(uint8_t room) {
    switch (room) {
    case 14: return MZM_EVENT_FIRST_METROID_ROOM_CLEARED;
    case 1: return MZM_EVENT_FIRST_METROID_ROOM_CLEARED + 1;    /* third */
    case 2: return MZM_EVENT_FIRST_METROID_ROOM_CLEARED + 2;    /* fifth */
    case 15: return MZM_EVENT_FIRST_METROID_ROOM_CLEARED + 3;   /* second */
    case 16: return MZM_EVENT_FIRST_METROID_ROOM_CLEARED + 4;   /* sixth */
    case 19: return MZM_EVENT_FIRST_METROID_ROOM_CLEARED + 5;   /* fourth */
    default: return 0;
    }
}

static void door_timer(const Ctx *c, int8_t timer) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->door_unlock_timer) h->door_unlock_timer(h->context, timer);
}

static bool air(const Ctx *c, uint16_t y, uint16_t x) {
    /* SpriteUtilCheckCollisionAtPosition, gPreviousCollisionCheck */
    return ai_collision_at(c, y, x) == MZM_COLLISION_AIR;
}

/* MetroidCheckAirCollision: a block on the side Samus moves to. */
static bool side_blocked(const Ctx *c, uint16_t y, uint16_t x) {
    int side = c->f->samus_h_moving == HDMOVING_RIGHT ? 1 : c->f->samus_h_moving == HDMOVING_LEFT ? -1 : 0;
    if (!side) return false;
    uint16_t sx = (uint16_t)(x + side * (BLOCK_SIZE - PIXEL_SIZE));
    return !air(c, (uint16_t)(y - THREE_QUARTER_BLOCK_SIZE), sx) ||
           !air(c, (uint16_t)(y + QUARTER_BLOCK_SIZE), sx);
}

/* One axis of MetroidMove: the speed builds up to the cap toward the
 * destination, then the momentum (`momentum`, set at the destination) runs
 * down every other frame; a wall reverses the axis at speed 16, the end of
 * the momentum at speed 1. */
static int move_axis(Ctx *c, uint16_t *position, uint16_t destination, uint8_t *momentum,
                     uint8_t *speed, uint8_t cap, uint8_t divisor, bool positive, bool blocked,
                     bool odd_frames) {
    if (blocked) return 2;
    if (*momentum == 0) {
        bool short_of = positive ? *position <= destination - PIXEL_SIZE
                                 : !(*position < destination + PIXEL_SIZE);
        if (short_of) {
            if (*speed < cap) (*speed)++;
            *position = (uint16_t)(positive ? *position + (*speed >> divisor) : *position - (*speed >> divisor));
        } else {
            *momentum = *speed;
        }
        return 0;
    }
    if (((c->frame8 & 1) != 0) == odd_frames) (*momentum)--;
    if (*momentum == 0) return 1;
    *position = (uint16_t)(positive ? *position + (*momentum >> divisor) : *position - (*momentum >> divisor));
    return 0;
}

/* MetroidMove */
static void metroid_move(Ctx *c, uint16_t dst_y, uint16_t dst_x, uint8_t y_cap, uint8_t x_cap,
                         uint8_t divisor) {
    MzmSprite *s = c->s;
    uint16_t y = s->y, x = s->x;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    bool down = s->status & MZM_SPRITE_STATUS_FACING_DOWN;
    uint16_t side_x = (uint16_t)(right ? x + (BLOCK_SIZE - PIXEL_SIZE) : x - (BLOCK_SIZE - PIXEL_SIZE));
    bool hit_x = !air(c, (uint16_t)(y - THREE_QUARTER_BLOCK_SIZE), side_x) ||
                 !air(c, (uint16_t)(y + QUARTER_BLOCK_SIZE), side_x);
    uint16_t edge_y = down ? (uint16_t)(y + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)
                           : (uint16_t)(y - THREE_QUARTER_BLOCK_SIZE);
    bool hit_y = !air(c, edge_y, (uint16_t)(x - HALF_BLOCK_SIZE)) ||
                 !air(c, edge_y, (uint16_t)(x + HALF_BLOCK_SIZE));
    /* x: work1 momentum, work2 speed (decrements on even frames); y: work0
     * momentum, work3 speed (odd frames). */
    int bounce = move_axis(c, &s->x, dst_x, &s->work1, &s->work2, x_cap, divisor, right, hit_x, false);
    if (bounce) {
        s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
        if (bounce == 2) {
            s->work1 = 0;
            s->work2 = 16;
        } else {
            s->work2 = 1;
        }
    }
    bounce = move_axis(c, &s->y, dst_y, &s->work0, &s->work3, y_cap, divisor, down, hit_y, true);
    if (bounce) {
        s->status ^= MZM_SPRITE_STATUS_FACING_DOWN;
        if (bounce == 2) {
            s->work0 = 0;
            s->work3 = 16;
        } else {
            s->work3 = 1;
        }
    }
}

/* MetroidBombDetection: an exploding bomb (PROJ_STATUS_CAN_AFFECT_ENVIRONMENT)
 * touching it. */
static bool bomb_touching(const Ctx *c) {
    const MzmSprite *s = c->s;
    const MzmWeapons *weapons = c->f->weapons;
    if (!weapons) return false;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int i = 0; i < MZM_MAX_PROJECTILES; ++i) {
        const MzmProjectile *p = &weapons->list[i];
        if (!p->active || p->type != MZM_PROJECTILE_BOMB || !p->hits_sprites) continue;
        uint16_t by = (uint16_t)p->y, bx = (uint16_t)p->x;
        uint16_t ptop = (uint16_t)(by - (BLOCK_SIZE - PIXEL_SIZE)), pbottom = (uint16_t)(by + THREE_QUARTER_BLOCK_SIZE);
        uint16_t pleft = (uint16_t)(bx - THREE_QUARTER_BLOCK_SIZE), pright = (uint16_t)(bx + THREE_QUARTER_BLOCK_SIZE);
        /* SpriteUtilCheckObjectsTouching */
        if (pbottom >= top && ptop < bottom && pright >= left && pleft < right) return true;
    }
    return false;
}

/* MetroidCheckBouncingOnMetroid: a moving, unfrozen Metroid pushes the
 * first such Metroid after its slot that overlaps it (a block square). */
static void bounce_on_metroid(Ctx *c, uint16_t movement) {
    MzmSprite *s = c->s;
    if (!c->f->sprites) return;
    int start = s->pose == METROID_POSE_MOVING && !s->freeze_timer ? s->ram_slot + 1 : 0;
    uint16_t y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE), x = s->x;
    for (int i = start; i < c->f->sprite_count; ++i) {
        MzmSprite *o = &c->f->sprites[i];
        if (!(o->status & MZM_SPRITE_STATUS_EXISTS) || o->samus_collision != SSC_METROID) continue;
        if (o->freeze_timer || o->pose != METROID_POSE_MOVING) continue;
        uint16_t oy = (uint16_t)(o->y - QUARTER_BLOCK_SIZE), ox = o->x;
        if ((uint16_t)(y + HALF_BLOCK_SIZE) <= (uint16_t)(oy - HALF_BLOCK_SIZE)) continue;
        if ((uint16_t)(y - HALF_BLOCK_SIZE) >= (uint16_t)(oy + HALF_BLOCK_SIZE)) continue;
        if ((uint16_t)(x + HALF_BLOCK_SIZE) <= (uint16_t)(ox - HALF_BLOCK_SIZE)) continue;
        if ((uint16_t)(x - HALF_BLOCK_SIZE) >= (uint16_t)(ox + HALF_BLOCK_SIZE)) continue;
        uint8_t push = (uint8_t)(movement * QUARTER_BLOCK_SIZE);
        if (y > oy) {
            if (air(c, (uint16_t)(o->y - HALF_BLOCK_SIZE), o->x)) {
                o->y = (uint16_t)(o->y - movement);
                o->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
                o->work0 = 0;
                o->work3 = push;
            }
        } else if (air(c, (uint16_t)(o->y + HALF_BLOCK_SIZE), o->x)) {
            o->y = (uint16_t)(o->y + movement);
            o->status |= MZM_SPRITE_STATUS_FACING_DOWN;
            o->work0 = 0;
            o->work3 = push;
        }
        if (x > ox) {
            if (air(c, o->y, (uint16_t)(o->x - HALF_BLOCK_SIZE))) {
                o->x = (uint16_t)(o->x - movement);
                o->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
                o->work1 = 0;
                o->work2 = push;
            }
        } else if (air(c, o->y, (uint16_t)(o->x + HALF_BLOCK_SIZE))) {
            o->x = (uint16_t)(o->x + movement);
            o->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            o->work1 = 0;
            o->work2 = push;
        }
        break;
    }
}

/* MetroidCheckSamusGrabbed: a Metroid other than a moving one holds her. */
static bool samus_held(const Ctx *c) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *o = &c->f->sprites[i];
        if (o->status & MZM_SPRITE_STATUS_EXISTS && o->samus_collision == SSC_METROID &&
            o->pose != METROID_POSE_MOVING)
            return true;
    }
    return false;
}

/* MetroidPlayMovingSound */
static void moving_sound(const MzmSprite *s) {
    if (s->frame == 0 && s->adc == 1) ai_sound_onscreen_once(s, MZM_SOUND_METROID_MOVING);
}

/* MetroidInit */
static void metroid_init(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t event = room_event(c->f->room);
    if (event && ai_check_event(c, event)) {
        s->status = 0;              /* already cleared */
        return;
    }
    if (event) door_timer(c, 1);    /* LOCK_DOORS */
    s->status |= MZM_SPRITE_STATUS_MOSAIC | MZM_SPRITE_STATUS_IGNORE_PROJECTILES |
                 MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
    s->scaling = 0x40;              /* Q_8_8(.25f) */
    s->rotation = 0;
    s->draw_top = s->draw_bottom = (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) / PIXEL_SIZE;
    s->draw_horizontal = (QUARTER_BLOCK_SIZE + PIXEL_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
    s->hitbox_bottom = HALF_BLOCK_SIZE;
    s->hitbox_left = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
    s->hitbox_right = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    ai_set_anim(s, MZM_METROID_SPAWNING);
    s->health = c->tables->health[s->sprite_id];
    s->y_spawn = s->health;         /* the health of the last hit */
    s->samus_collision = SSC_NONE;
    /* SpriteUtilMakeSpriteFaceSamusDirection */
    ai_face_samus(c);
    s->pose = METROID_POSE_CHECK_SPAWN;
    s->work2 = 0;
    s->work3 = 0;
    s->draw_order = 12;
    if (ai_spawn_secondary(c, MZM_SSPRITE_METROID_SHELL, s->room_slot, s->gfx_slot, s->y, s->x, 0) < 0)
        s->status = 0;
    s->palette_row = 3;
}

/* MetroidSpawning: it wobbles along the spawning tables and, after a
 * random delay, grows from a quarter of its size (palette rows 3, 2, 1). */
static void metroid_spawning(Ctx *c) {
    MzmSprite *s = c->s;
    const int16_t *ys = c->tables->metroid_spawning_y, *xs = c->tables->metroid_spawning_x;
    uint8_t offset = s->work3;
    int16_t movement = offset < 65 ? ys[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = ys[0];
        offset = 0;
    }
    s->work3 = (uint8_t)(offset + 1);
    s->y = (uint16_t)(s->y + movement);
    offset = s->work2;
    movement = offset < 81 ? xs[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = xs[0];
        offset = 0;
    }
    s->work2 = (uint8_t)(offset + 1);
    s->x = (uint16_t)(s->x + movement);
    if (s->work0 != 0) {
        s->work0--;
        return;
    }
    if (s->scaling < 0x100) {
        s->scaling += 4;            /* Q_8_8(.0175f) */
        if (s->scaling >= 0xD1) s->palette_row = 1;         /* Q_8_8(.82f) */
        else if (s->scaling >= 0xA1) s->palette_row = 2;    /* Q_8_8(.63f) */
    } else {
        s->palette_row = 0;
        s->status &= (uint16_t)~(MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
        s->pose = METROID_POSE_MOVING_INIT;
        ai_set_anim(s, MZM_METROID_MOVING);
        s->samus_collision = SSC_METROID;
        s->draw_order = 4;
    }
}

/* MetroidSamusGrabbed: on Samus's head, cycling the grab palette, until a
 * bomb ejects it. */
static void metroid_grabbed(Ctx *c) {
    MzmSprite *s = c->s;
    moving_sound(s);
    bounce_on_metroid(c, PIXEL_SIZE / 2);
    if (--s->work0 == 0) {
        s->work0 = 4;               /* CONVERT_SECONDS(1.f / 15) */
        if (++s->work1 >= METROID_GRAB_PALETTES) s->work1 = 0;
        /* DMA of sMetroidPal_SamusGrabbed's row to OBJ row 12. */
        if (c->f->globals) c->f->globals->metroid_grab_palette = (int8_t)s->work1;
    }
    MzmSpriteSamusLink *link = c->f->samus_link;
    if (side_blocked(c, s->y, s->x)) {
        if (link) {
            link->x = s->x;
            link->moved = true;
        }
    } else {
        s->x = c->f->samus_x;
    }
    s->y = (uint16_t)(c->f->samus_y + c->f->samus_hitbox_top);
    if (bomb_touching(c)) {
        uint8_t velocity = c->rng & 1 ? HALF_BLOCK_SIZE - PIXEL_SIZE : QUARTER_BLOCK_SIZE;
        s->pose = METROID_POSE_MOVING;
        s->palette_row = 0;
        s->ignore_samus_timer = 15;                     /* CONVERT_SECONDS(.25f) */
        s->status &= (uint16_t)~(MZM_SPRITE_STATUS_FACING_DOWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
        s->work0 = 0;
        s->work3 = velocity;
        s->work1 = 0;
        s->work2 = velocity;
        ai_set_anim(s, MZM_METROID_MOVING);
        s->frozen_row_offset = 0;
        if (c->f->equipment) c->f->equipment->grabbed_by_metroid = false;
        return;
    }
    if ((s->rotation & 31) == 0) {
        /* The sucking sounds by suits (rotation counts the frames). */
        mzm_sfx_play_not_playing(MZM_SOUND_METROID_LEECHING);
        uint32_t items = c->f->equipment ? c->f->equipment->items : 0;
        int suits = !!(items & MZM_ITEM_VARIA_SUIT) + !!(items & MZM_ITEM_GRAVITY_SUIT);
        mzm_sfx_play(suits == 0 ? MZM_SOUND_METROID_SUCKING_NO_SUIT
                     : suits == 2 ? MZM_SOUND_METROID_SUCKING_ALL_SUITS
                                  : MZM_SOUND_METROID_SUCKING_ONE_SUIT);
    }
    s->rotation++;
}

/* MetroidDeath: the explosion, then, once no Metroid is left, the room's
 * event and the doors unlocked. */
static void metroid_death(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t rng = c->rng;
    uint16_t y = s->status & MZM_SPRITE_STATUS_X_FLIP ? (uint16_t)(s->y + rng) : (uint16_t)(s->y - rng);
    ai_sprite_death(c, y, (uint16_t)(s->x + HALF_BLOCK_SIZE + PIXEL_SIZE), MZM_PE_FREEZING_SPRITE_WITH_CHARGED_ICE);
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *o = &c->f->sprites[i];
        if (!(o->status & MZM_SPRITE_STATUS_EXISTS) || o->secondary) continue;
        if ((o->sprite_id == MZM_PSPRITE_METROID || o->sprite_id == MZM_PSPRITE_FROZEN_METROID) &&
            o->pose < METROID_POSE_DEATH)
            return;
    }
    uint8_t event = room_event(c->f->room);
    if (!event) return;
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, event);
    door_timer(c, -20);             /* -ONE_THIRD_SECOND */
}

/* Metroid */
static void metroid(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->damaged) {
        s->damaged = false;
        ai_sound_onscreen_once(s, MZM_SOUND_METROID_DAMAGED);
    } else if ((s->isft & 0x7F) == 2) {
        if (s->health == s->y_spawn) ai_sound_onscreen_once(s, MZM_SOUND_METROID_DETACHED);
        else s->y_spawn = s->health;
    }
    if (s->freeze_timer) {
        /* Frozen: a bigger hitbox, missiles hurt it (PSPRITE_FROZEN_METROID). */
        s->hitbox_top = -THREE_QUARTER_BLOCK_SIZE;
        s->hitbox_bottom = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->hitbox_left = -BLOCK_SIZE;
        s->hitbox_right = BLOCK_SIZE;
        bounce_on_metroid(c, ONE_SUB_PIXEL);
        /* SpriteUtilUnfreezeAnimEasy / SpriteUtilMetroidUnfreezeAnim */
        uint8_t timer;
        if (c->f->difficulty == 0) {
            timer = --s->freeze_timer;
            if (timer == 0) s->adc--;
            if (timer <= 90 && (timer & 1) == 0)
                s->palette_row = timer & 2 ? (uint8_t)(15 - s->gfx_slot - s->frozen_row_offset)
                                           : s->absolute_palette_row;
        } else {
            s->freeze_timer = (uint8_t)(s->freeze_timer - 2);  /* twice APPLY_DELTA_TIME_DEC */
            timer = s->freeze_timer;
            if (timer == 0) s->adc--;
            if (timer <= 120 && (timer & 2) == 0)
                s->palette_row = timer & 4 ? (uint8_t)(15 - s->gfx_slot - s->frozen_row_offset)
                                           : s->absolute_palette_row;
        }
        s->sprite_id = MZM_PSPRITE_FROZEN_METROID;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        return;
    }
    if (s->sprite_id == MZM_PSPRITE_FROZEN_METROID) {
        s->hitbox_top = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_bottom = HALF_BLOCK_SIZE;
        s->hitbox_left = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->sprite_id = MZM_PSPRITE_METROID;
        if (s->pose < METROID_POSE_DEATH) s->pose = METROID_POSE_MOVING_INIT;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: metroid_init(c); break;
    case METROID_POSE_CHECK_SPAWN:
        /* MetroidCheckSpawn */
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN) {
            s->pose = METROID_POSE_SPAWNING;
            s->draw_top = s->draw_bottom = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_horizontal = (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
            s->work0 = (uint8_t)(c->rng * 4 + 1);
        }
        break;
    case METROID_POSE_SPAWNING: metroid_spawning(c); break;
    case METROID_POSE_MOVING_INIT:
        /* MetroidMovingInit */
        s->pose = METROID_POSE_MOVING;
        s->work1 = 0;
        s->work2 = 1;
        s->work0 = 0;
        s->work3 = 1;
        ai_set_anim(s, MZM_METROID_MOVING);
        /* fallthrough */
    case METROID_POSE_MOVING:
        /* MetroidMovement */
        moving_sound(s);
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            if (samus_held(c)) {
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            } else {
                s->pose = METROID_POSE_GRAB_SAMUS_INIT;
                s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
                break;
            }
        }
        bounce_on_metroid(c, ONE_SUB_PIXEL);
        metroid_move(c, (uint16_t)(c->f->samus_y + c->f->samus_hitbox_top), c->f->samus_x,
                     HALF_BLOCK_SIZE - PIXEL_SIZE / 2, HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE, 2);
        break;
    case METROID_POSE_GRAB_SAMUS_INIT:
        /* MetroidSamusGrabbedInit */
        s->pose = METROID_POSE_SAMUS_GRABBED;
        ai_set_anim(s, MZM_METROID_SAMUS_GRABBED);
        s->work0 = 4;               /* CONVERT_SECONDS(1.f / 15) */
        s->work1 = 4;
        s->frozen_row_offset = 4;
        s->rotation = 0;
        if (c->f->equipment) c->f->equipment->grabbed_by_metroid = true;
        /* fallthrough */
    case METROID_POSE_SAMUS_GRABBED: metroid_grabbed(c); break;
    case METROID_POSE_DEATH: metroid_death(c); break;
    default: break;
    }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
}

/* MetroidShell: the dome drawn over the body, hidden while the body grows,
 * and its own explosion when the body dies. */
static void metroid_shell(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *body = c->f->sprites && s->ram_slot < c->f->sprite_count ? &c->f->sprites[s->ram_slot] : NULL;
    s->ignore_samus_timer = 1;
    if (!body) return;
    s->palette_row = body->palette_row;
    if (body->health == 0) {
        uint8_t rng = c->rng;
        uint16_t y = body->status & MZM_SPRITE_STATUS_X_FLIP ? (uint16_t)(s->y - rng) : (uint16_t)(s->y + rng);
        s->sprite_id = MZM_PSPRITE_METROID;
        s->secondary = false;
        ai_sprite_death(c, y, (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE)),
                        MZM_PE_FREEZING_SPRITE_WITH_CHARGED_ICE);
        return;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->samus_collision = SSC_NONE;
        s->draw_top = (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->pose = METROID_SHELL_POSE_IDLE;
        s->draw_order = 3;
        ai_set_anim(s, MZM_METROID_SHELL_IDLE);
    }
    if (s->status & MZM_SPRITE_STATUS_NOT_DRAWN && !(body->status & MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE))
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->y = body->y;
    s->x = body->x;
}

void ai_metroid(Ctx *c) {
    if (c->s->secondary) metroid_shell(c);
    else metroid(c);
}
