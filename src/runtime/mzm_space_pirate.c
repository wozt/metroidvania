/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Space Pirates and their lasers (src/sprites_ai/space_pirate.c);
 * comments name the pinned decompilation's routines. The alarm music and
 * palette and the power-bomb and escape-ship variants are not ported; the
 * black pirates (src/sprites_ai/black_space_pirate.c) share their routines. */
#include "mzm_sprite_ai.h"

#include <stddef.h>
#include <string.h>

/* SpacePiratePose (include/sprites_ai/space_pirate.h). */
enum {
    P_WALKING_INIT = 0x8,
    P_WALKING = 0x9,
    P_TURNING_AROUND_INIT = 0xA,
    P_TURNING_AROUND_FIRST_PART = 0xB,
    P_TURNING_AROUND_SECOND_PART = 0xC,
    P_0xE = 0xE,
    P_0xF = 0xF,
    P_0x10 = 0x10,
    P_0x11 = 0x11,
    P_TURNING_AROUND_TO_WALL_JUMP_INIT = 0x12,
    P_TURNING_AROUND_TO_WALL_JUMP = 0x13,
    P_DELAY_BEFORE_LAUNCHING_FROM_WALL = 0x15,
    P_0x16 = 0x16,
    P_0x18 = 0x18,
    P_0x19 = 0x19,
    P_0x1A = 0x1A,
    P_0x1C = 0x1C,
    P_FALLING_INIT = 0x1E,
    P_FALLING = 0x1F,
    P_WALKING_ALERTED_INIT = 0x22,
    P_WALKING_ALERTED = 0x23,
    P_WAITING_AT_DOOR = 0x24,
    P_SHOOTING_AFTER_WAITING_AT_DOOR = 0x25,
    P_IDLE = 0x27,
    P_IDLE_AT_DOOR = 0x28,
    P_0x2A = 0x2A,
    P_0x2B = 0x2B,
    P_CHARGING_LASER_INIT = 0x34,
    P_CHARGING_LASER = 0x35,
    P_SHOOTING_LASER_INIT = 0x36,
    P_SHOOTING_LASER = 0x37,
    P_CLIMBING_CHARGING_LASER_INIT = 0x38,
    P_CLIMBING_CHARGING_LASER = 0x39,
    P_CLIMBING_SHOOTING_LASER_INIT = 0x3A,
    P_CLIMBING_SHOOTING_LASER = 0x3B,
    P_JUMPING_INIT = 0x42,
    P_JUMPING = 0x43,
    P_TURNING_AROUND_ALERTED_INIT = 0x44,
    P_TURNING_AROUND_ALERTED_FIRST_PART = 0x45,
    P_TURNING_AROUND_ALERTED_SECOND_PART = 0x47,
    P_CLIMBING_UP_INIT = 0x48,
    P_CLIMBING_UP = 0x49,
    P_LAUNCHING_FROM_WALL_INIT = 0x4A,
    P_LAUNCHING_FROM_WALL = 0x4B,
    P_WALL_JUMPING = 0x4D,
    P_STARTING_TO_CRAWL_INIT = 0x4E,
    P_STARTING_TO_CRAWL = 0x4F,
    P_CRAWLING = 0x51,
    P_TURNING_AROUND_WHILE_CRAWLING_INIT = 0x52,
    P_TURNING_AROUND_WHILE_CRAWLING_FIRST_PART = 0x53,
    P_TURNING_AROUND_WHILE_CRAWLING_SECOND_PART = 0x55,
    P_STANDING_UP_INIT = 0x56,
    P_STANDING_UP = 0x57,
    P_CRAWLING_STOPPED_INIT = 0x58,
    P_CRAWLING_STOPPED = 0x59,
    P_FALLING_WHILE_CRAWLING_INIT = 0x5A,
    P_FALLING_WHILE_CRAWLING = 0x5B,
    P_CLIMBING_DOWN_INIT = 0x5E,
    P_CLIMBING_DOWN = 0x5F,
    P_HIT_BY_LASER_INIT = 0x6B,
    P_HIT_BY_LASER = 0x6C,
};

/* SpacePirateAim, SpacePirateClimingDirection */
enum { AIM_FORWARD, AIM_DIAGONALLY_UP, AIM_DIAGONALLY_DOWN };
enum { CLIMBING_UP, CLIMBING_DOWN };

/* Laser poses and SpacePirateLaserDirection (the laser's roomSlot). */
enum {
    LASER_POSE_MOVING = 0x9,
    LASER_POSE_EXPLODING_INIT = 0x42,
    LASER_POSE_EXPLODING = 0x43,
    LASER_FORWARD = 0,
    LASER_DIAGONALLY_UP = 1,
    LASER_DIAGONALLY_DOWN = 2,
    LASER_FROM_DOOR = 1 << 7,
};

enum {
    MAX_AMOUNT_OF_SPRITES = 24,
    AREA_CHOZODIA = 6,
    SPACE_PIRATE_HEIGHT = BLOCK_SIZE * 3 + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE,
    SPACE_PIRATE_LEFT_HITBOX = BLOCK_SIZE,
    SPACE_PIRATE_RIGHT_HITBOX = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    SPACE_PIRATE_CRAWLING_WIDTH = BLOCK_SIZE + QUARTER_BLOCK_SIZE + PIXEL_SIZE,
    LASER_1_X = BLOCK_SIZE + PIXEL_SIZE,
    LASER_2_DIAG_X = BLOCK_SIZE + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    LASER_2_FORWARD_X = BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    LASER_1_FORWARD_Y = THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE,
    LASER_2_FORWARD_Y = BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    LASER_1_DIAG_UP_Y = BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    LASER_2_DIAG_UP_Y = BLOCK_SIZE + HALF_BLOCK_SIZE - PIXEL_SIZE / 2,
    LASER_1_DIAG_DOWN_Y = HALF_BLOCK_SIZE - PIXEL_SIZE,
    LASER_2_DIAG_DOWN_Y = BLOCK_SIZE + HALF_BLOCK_SIZE,
    LASER_1_WALL_Y = BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + PIXEL_SIZE,
    LASER_2_WALL_Y = BLOCK_SIZE + QUARTER_BLOCK_SIZE - PIXEL_SIZE / 2,
    LASER_SPEED = QUARTER_BLOCK_SIZE + PIXEL_SIZE,
    /* FRAME_DATA_NBR_OF_FRAMES of sSpacePirateOam_Walking and of the
     * shooting tables. */
    WALKING_FRAMES = 8,
    SHOOTING_FRAMES = 4,
    /* CONVERT_SECONDS of the timers. */
    ONE_SECOND = 60,
    SIXTH_SECOND = 10,
    ONE_THIRD_SECOND = 20,
    TWO_THIRD_SECOND = 40,
    ALARM_ACTIVE = MZM_ALARM_TIMER_ACTIVE,
};

/* The wall probe distance shared by the climbing poses. */
#define WALL_X (BLOCK_SIZE - QUARTER_BLOCK_SIZE)

static bool facing_right(const MzmSprite *s) {
    return s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
}

static uint16_t ahead(const MzmSprite *s, int distance) {
    return (uint16_t)(facing_right(s) ? s->x + distance : s->x - distance);
}

static uint8_t isft(const MzmSprite *s) {
    return s->isft & 0x7F;      /* SPRITE_GET_ISFT */
}

static void set_anim(MzmSprite *s, uint8_t anim) {
    ai_set_anim(s, anim);
}

static bool ended(const Ctx *c) {
    return ai_animation_ending(c, 1);
}

static bool nearly_ended(const Ctx *c) {
    return ai_animation_ending(c, 2);
}

static bool mecha_ridley_killed(const MzmSpriteHooks *hooks) {
    return hooks && hooks->check_event &&
           hooks->check_event(hooks->context, MZM_EVENT_MECHA_RIDLEY_KILLED);
}

/* DisableChozodiaAlarm (the animated palette and music are not ported): the
 * escape keeps it ringing. */
static void disable_chozodia_alarm(MzmSpriteGlobals *g, const MzmSpriteHooks *hooks) {
    g->alarm_timer = mecha_ridley_killed(hooks) ? ALARM_ACTIVE : 0;
}

static void disable_alarm(const Ctx *c) {
    disable_chozodia_alarm(c->f->globals, c->f->hooks);
}

void mzm_spawn_waiting_pirates(MzmSprite *sprites, int count, MzmSpriteGlobals *globals,
                               const MzmSpriteHooks *hooks, const uint8_t *spriteset,
                               int spriteset_count, uint8_t spriteset_index, uint16_t y,
                               uint16_t x, bool no_hatch) {
    if (mecha_ridley_killed(hooks)) globals->alarm_timer = ALARM_ACTIVE;
    else if (!globals->alarm_timer) return;
    /* The third waiting pirate when the spriteset has it, else the first
     * one for any other pirate. */
    uint8_t sprite_id = 0;
    for (int i = 0; i < spriteset_count && !sprite_id; ++i)
        if (spriteset[i] == MZM_PSPRITE_SPACE_PIRATE_WAITING2) sprite_id = spriteset[i];
    for (int i = 0; i < spriteset_count && !sprite_id; ++i)
        if (spriteset[i] == MZM_PSPRITE_SPACE_PIRATE ||
            spriteset[i] == MZM_PSPRITE_SPACE_PIRATE_WAITING1 ||
            spriteset[i] == MZM_PSPRITE_SPACE_PIRATE_WAITING3 ||
            spriteset[i] == MZM_PSPRITE_SPACE_PIRATE2)
            sprite_id = MZM_PSPRITE_SPACE_PIRATE_WAITING1;
    if (!sprite_id) {
        disable_chozodia_alarm(globals, hooks);
        return;
    }
    /* Spritesets 88 (save platform) and 103 (map station) keep graphics
     * slot 5 for the pirates. */
    uint8_t gfx_slot = spriteset_index == 88 || spriteset_index == 103 ? 5 : 0;
    /* SpriteSpawnPrimary: the first free slot (none: nothing spawns). */
    for (int slot = 0; slot < count; ++slot) {
        MzmSprite *s = &sprites[slot];
        if (s->status & MZM_SPRITE_STATUS_EXISTS) continue;
        memset(s, 0, sizeof *s);
        s->status = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                    MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_FACING_DOWN;
        s->gfx_slot = gfx_slot;
        s->sprite_id = sprite_id;
        s->y = y;
        s->x = x;
        s->room_slot = no_hatch ? 0x80 | 0x1 : 0x80;
        s->pose = SPRITE_POSE_UNINITIALIZED;
        s->ignore_samus_timer = 1;
        s->ram_slot = (uint8_t)slot;
        return;
    }
}

void mzm_sprites_end_frame(MzmSpriteGlobals *globals, const MzmSpriteHooks *hooks) {
    if (!globals) return;
    /* gParasiteRelated */
    if (globals->parasite_related) globals->parasite_related--;
    /* DecrementChozodiaAlarm */
    if (globals->alarm_timer) {
        globals->alarm_timer--;
        if (!globals->alarm_timer && hooks && hooks->check_event &&
            hooks->check_event(hooks->context, MZM_EVENT_MECHA_RIDLEY_KILLED))
            globals->alarm_timer = ALARM_ACTIVE;
    }
    for (int i = 0; i < 5; ++i) globals->draw_order[i] = 0;
}

/* SpacePirateFlip */
static void flip(MzmSprite *s) {
    if (facing_right(s)) {
        s->status |= MZM_SPRITE_STATUS_X_FLIP;
        s->hitbox_left = -SPACE_PIRATE_RIGHT_HITBOX;
        s->hitbox_right = SPACE_PIRATE_LEFT_HITBOX;
    } else {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        s->hitbox_left = -SPACE_PIRATE_LEFT_HITBOX;
        s->hitbox_right = SPACE_PIRATE_RIGHT_HITBOX;
    }
}

static bool solid_f0(const Ctx *c, uint16_t y, uint16_t x) {
    return ai_collision_at(c, y, x) & MZM_COLLISION_FLAGS_F0;
}

/* One row of the straight line of sight of SpacePirateSamusDetection:
 * `count` blocks from `x` by `step`, stopping at a wall (whose index is
 * stored in `wall` when given). */
static bool see_row(const Ctx *c, uint16_t y, uint16_t x, int step, int count, uint16_t samus_y,
                    uint16_t samus_bottom, bool small, uint8_t *wall) {
    for (int i = 0; i < count; x = (uint16_t)(x + step), i++) {
        if (solid_f0(c, y, x)) {
            if (wall) *wall = (uint8_t)i;
            return false;
        }
        if (x != (c->f->samus_x & BLOCK_POSITION_FLAG)) continue;
        if (y == samus_y || (!small && y == samus_bottom)) return true;
    }
    return false;
}

/* One diagonal line of sight of SpacePirateSamusDetection. */
static bool see_diagonal(const Ctx *c, uint16_t y, uint16_t x, int x_step, int y_step,
                         uint16_t samus_tile_y, uint16_t samus_tile_x, uint16_t samus_bottom,
                         bool small) {
    for (int i = 0; i < 5; x = (uint16_t)(x + x_step), y = (uint16_t)(y + y_step), i++) {
        if (solid_f0(c, y, x)) {
            return small && x == samus_tile_x && (uint16_t)(y - BLOCK_SIZE) == samus_tile_y &&
                   !solid_f0(c, samus_tile_y, x);
        }
        if (solid_f0(c, (uint16_t)(y - BLOCK_SIZE), x))
            return small && x == samus_tile_x && y == samus_tile_y;
        if (x != samus_tile_x) continue;
        if (y == samus_tile_y || (uint16_t)(y - BLOCK_SIZE) == samus_tile_y ||
            (!small && (y == samus_bottom || (uint16_t)(y - BLOCK_SIZE) == samus_bottom)))
            return true;
    }
    return false;
}

/* SpacePirateSamusDetection: draw_order[2] whether the pirate sees Samus,
 * [0] the aim, [1] her direction, [3] below, [4] right of it. */
static void samus_detection(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t *d = g->draw_order;
    uint16_t samus_y = (uint16_t)(c->f->samus_y + PIXEL_SIZE), samus_x = c->f->samus_x;
    uint16_t sprite_y = s->y, sprite_x = s->x;
    uint16_t y_distance, x_distance;
    if (samus_y > sprite_y) {
        d[3] = true;
        y_distance = (uint16_t)(samus_y - sprite_y);
    } else {
        d[3] = false;
        y_distance = (uint16_t)(sprite_y - samus_y);
    }
    if (samus_x > sprite_x) {
        d[4] = true;
        x_distance = (uint16_t)(samus_x - sprite_x);
    } else {
        d[4] = false;
        x_distance = (uint16_t)(sprite_x - samus_x);
    }
    g->boss_work2 = x_distance;
    if (x_distance > BLOCK_SIZE * 10) {
        d[2] = false;
        if (s->rotation) s->rotation--;
        else s->rotation = ONE_SECOND;
        return;
    }
    if (y_distance > BLOCK_SIZE * 10) {
        d[2] = false;
        return;
    }
    d[2] = true;
    if (s->rotation) s->rotation--;

    uint16_t samus_tile_y = (uint16_t)((samus_y & BLOCK_POSITION_FLAG) - BLOCK_SIZE);
    uint16_t samus_tile_x = samus_x & BLOCK_POSITION_FLAG;
    uint16_t samus_bottom = (uint16_t)(samus_tile_y - BLOCK_SIZE);
    uint16_t tile_y = sprite_y & BLOCK_POSITION_FLAG, tile_x = sprite_x & BLOCK_POSITION_FLAG;
    bool small = c->f->samus_crouching_or_morphed;
    int step = d[4] ? BLOCK_SIZE : -BLOCK_SIZE;

    switch (s->pose) {
    case P_CRAWLING:
    case P_TURNING_AROUND_WHILE_CRAWLING_INIT:
    case P_TURNING_AROUND_WHILE_CRAWLING_FIRST_PART:
    case P_TURNING_AROUND_WHILE_CRAWLING_SECOND_PART:
        if (see_row(c, (uint16_t)(tile_y - BLOCK_SIZE), (uint16_t)(tile_x + step), step, 8,
                    samus_tile_y, samus_bottom, small, NULL)) {
            d[0] = false;
            d[1] = d[4];
            return;
        }
        break;

    case P_TURNING_AROUND_TO_WALL_JUMP_INIT:
    case P_TURNING_AROUND_TO_WALL_JUMP:
    case P_DELAY_BEFORE_LAUNCHING_FROM_WALL:
    case P_0x18:
    case P_0x19:
    case P_CLIMBING_CHARGING_LASER_INIT:
    case P_CLIMBING_CHARGING_LASER:
    case P_CLIMBING_SHOOTING_LASER_INIT:
    case P_CLIMBING_SHOOTING_LASER:
    case P_CLIMBING_UP_INIT:
    case P_CLIMBING_UP:
    case P_LAUNCHING_FROM_WALL_INIT:
    case P_LAUNCHING_FROM_WALL:
    case P_CLIMBING_DOWN_INIT:
    case P_CLIMBING_DOWN:
        /* A climbing pirate only sees behind itself (away from the wall). */
        if (facing_right(s) == (d[4] == true)) {
            d[2] = false;
            return;
        }
        /* fallthrough */
    default:
        if (y_distance < BLOCK_SIZE * 3) {
            /* Three rows in front, the first limiting the others. */
            uint16_t y = (uint16_t)(tile_y - BLOCK_SIZE * 3);
            uint8_t wall = 8;
            bool found = see_row(c, y, (uint16_t)(tile_x + step), step, 8, samus_tile_y,
                                 samus_bottom, small, &wall);
            if (!found) {
                int range = wall < 8 ? wall + 1 : 8;
                found = see_row(c, (uint16_t)(y + BLOCK_SIZE), (uint16_t)(tile_x + step), step,
                                range, samus_tile_y, samus_bottom, small, NULL) ||
                        see_row(c, (uint16_t)(y + BLOCK_SIZE * 2), (uint16_t)(tile_x + step),
                                step, range, samus_tile_y, samus_bottom, small, NULL);
            }
            if (found) {
                d[0] = false;
                d[1] = d[4];
                return;
            }
        }
        if (d[3] == false) {
            /* Up and forward. */
            if (!solid_f0(c, (uint16_t)(sprite_y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)),
                          (uint16_t)(tile_x + step)) &&
                see_diagonal(c, (uint16_t)(tile_y - BLOCK_SIZE * 3),
                             (uint16_t)(tile_x + step * 2), step, -BLOCK_SIZE, samus_tile_y,
                             samus_tile_x, samus_bottom, small)) {
                d[0] = true;
                d[1] = d[4];
                return;
            }
        } else {
            /* Down and forward. */
            uint16_t x = (uint16_t)(d[4] ? sprite_x + (BLOCK_SIZE - 4)
                                         : sprite_x - (BLOCK_SIZE - 4));
            if (!solid_f0(c, sprite_y, x) &&
                see_diagonal(c, (uint16_t)(tile_y - BLOCK_SIZE), (uint16_t)(tile_x + step),
                             step, BLOCK_SIZE, samus_tile_y, samus_tile_x, samus_bottom,
                             small)) {
                d[0] = 2;
                d[1] = d[4];
                return;
            }
        }
        /* fallthrough */
    case P_JUMPING_INIT:
    case P_JUMPING:
    case P_WALL_JUMPING:
        break;
    }
    d[2] = false;
}

/* SpriteUtilCheckObjectsTouching */
static bool touching(uint16_t o1_top, uint16_t o1_bottom, uint16_t o1_left, uint16_t o1_right,
                     uint16_t o2_top, uint16_t o2_bottom, uint16_t o2_left, uint16_t o2_right) {
    return o2_bottom >= o1_top && o2_top < o1_bottom && o2_right >= o1_left && o2_left < o1_right;
}

/* SpacePirateCheckCollidingWithLaser: a laser fresh out of the gun (work2
 * running down) does not hit its own pirate. */
static void check_colliding_with_laser(Ctx *c) {
    MzmSprite *s = c->s;
    const uint16_t check = MZM_SPRITE_STATUS_ONSCREEN | MZM_SPRITE_STATUS_NOT_DRAWN;
    if ((s->status & check) != MZM_SPRITE_STATUS_ONSCREEN || !c->f->sprites) return;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int i = 0; i < c->f->sprite_count; ++i) {
        MzmSprite *o = &c->f->sprites[i];
        if (!(o->status & MZM_SPRITE_STATUS_EXISTS)) continue;
        if (o->samus_collision != SSC_SPACE_PIRATE_LASER || o->work2) continue;
        if (touching(top, bottom, left, right, (uint16_t)(o->y - PIXEL_SIZE),
                     (uint16_t)(o->y + PIXEL_SIZE), (uint16_t)(o->x - PIXEL_SIZE),
                     (uint16_t)(o->x + PIXEL_SIZE))) {
            o->pose = LASER_POSE_EXPLODING_INIT;
            s->pose = P_HIT_BY_LASER_INIT;
            break;
        }
    }
}

static void spawn_laser(Ctx *c, uint8_t direction, int dy, int dx, uint16_t status) {
    ai_spawn_secondary(c, MZM_SSPRITE_SPACE_PIRATE_LASER, direction, c->s->gfx_slot,
                       (uint16_t)(c->s->y - dy), (uint16_t)(c->s->x + dx), status);
}

/* SpacePirateFireLaserGround */
static void fire_laser_ground(Ctx *c) {
    MzmSprite *s = c->s;
    int side = facing_right(s) ? 1 : -1;
    uint16_t status = facing_right(s) ? MZM_SPRITE_STATUS_X_FLIP : 0;
    switch (s->work1) {
    case AIM_FORWARD:
        spawn_laser(c, LASER_FORWARD, LASER_1_FORWARD_Y, side * LASER_1_X, status);
        spawn_laser(c, LASER_FORWARD, LASER_2_FORWARD_Y, side * LASER_2_FORWARD_X, status);
        break;
    case AIM_DIAGONALLY_UP:
        spawn_laser(c, LASER_DIAGONALLY_UP, LASER_1_DIAG_UP_Y, side * LASER_1_X, status);
        spawn_laser(c, LASER_DIAGONALLY_UP, LASER_2_DIAG_UP_Y, side * LASER_2_DIAG_X, status);
        break;
    case AIM_DIAGONALLY_DOWN:
        spawn_laser(c, LASER_DIAGONALLY_DOWN, LASER_1_DIAG_DOWN_Y, side * LASER_1_X, status);
        spawn_laser(c, LASER_DIAGONALLY_DOWN, LASER_2_DIAG_DOWN_Y, side * LASER_2_DIAG_X, status);
        break;
    }
}

/* SpacePirateFireLaserWall: away from the wall. */
static void fire_laser_wall(Ctx *c) {
    MzmSprite *s = c->s;
    int side = facing_right(s) ? -1 : 1;
    uint16_t status = facing_right(s) ? 0 : MZM_SPRITE_STATUS_X_FLIP;
    spawn_laser(c, LASER_FORWARD, LASER_1_WALL_Y, side * LASER_1_X, status);
    spawn_laser(c, LASER_FORWARD, LASER_2_WALL_Y, side * LASER_1_X, status);
}

/* What a pirate does to another it walks into (MOSAIC marks a pirate that
 * already reacted): true when the walker has to stop. */
static bool bump(MzmSprite *other, bool facing_right_value, bool alerted) {
    bool same = (bool)(other->status & MZM_SPRITE_STATUS_FACING_RIGHT) == facing_right_value;
    if (other->status & MZM_SPRITE_STATUS_MOSAIC) return same;
    other->status |= MZM_SPRITE_STATUS_MOSAIC;
    if (same) return true;
    if (alerted) {
        other->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        other->pose = P_TURNING_AROUND_ALERTED_INIT;
    } else {
        other->pose = P_TURNING_AROUND_INIT;
    }
    return false;
}

/* SpacePirateCheckCollidingWithPirateWhenWalking */
static bool colliding_with_pirate_when_walking(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN && !(s->status & MZM_SPRITE_STATUS_MOSAIC)) {
        if (ai_samus_nslr(c, BLOCK_SIZE * 6, BLOCK_SIZE * 10) == NSLR_OUT_OF_RANGE) return true;
        s->status |= MZM_SPRITE_STATUS_MOSAIC;
    }
    if (!c->f->sprites) return false;
    uint8_t pose = s->pose;
    uint16_t y = s->y, x = s->x;
    uint16_t top = (uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)), bottom = y;
    uint16_t left = (uint16_t)(x - (HALF_BLOCK_SIZE + 12));
    uint16_t right = (uint16_t)(x + (HALF_BLOCK_SIZE + 12));
    bool right_facing = facing_right(s);
    bool alerted = s->status & MZM_SPRITE_STATUS_FACING_DOWN;
    for (int slot = 0; slot < c->f->sprite_count; ++slot) {
        MzmSprite *o = &c->f->sprites[slot];
        if (!(o->status & MZM_SPRITE_STATUS_EXISTS) || o->samus_collision != SSC_SPACE_PIRATE)
            continue;
        if (o->pose == P_WALKING && pose == P_WALKING_ALERTED) continue;
        uint16_t other_top = (uint16_t)(o->y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE));
        uint16_t other_left = (uint16_t)(o->x - (THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE));
        uint16_t other_right = (uint16_t)(o->x + (THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE));
        bool ahead_of = x == o->x ? s->ram_slot < slot
                                  : right_facing ? x < o->x : x > o->x;
        if (!ahead_of ||
            !touching(top, bottom, left, right, other_top, o->y, other_left, other_right))
            continue;
        if (bump(o, right_facing, alerted)) return true;
    }
    return false;
}

/* SpacePirateCheckCollidingWithPirateWhenClimbing */
static bool colliding_with_pirate_when_climbing(Ctx *c, int direction) {
    MzmSprite *s = c->s;
    if (!c->f->sprites) return false;
    const int height = BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE;
    uint16_t top = (uint16_t)(s->y - height), bottom = s->y;
    uint16_t left = (uint16_t)(s->x - (THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE));
    uint16_t right = (uint16_t)(s->x + (THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE));
    for (int i = 0; i < c->f->sprite_count; ++i) {
        const MzmSprite *o = &c->f->sprites[i];
        if (!(o->status & MZM_SPRITE_STATUS_EXISTS) || o->samus_collision != SSC_SPACE_PIRATE)
            continue;
        if (direction == CLIMBING_UP ? !(s->y > o->y) : !(s->y < o->y)) continue;
        if (touching(top, bottom, left, right, (uint16_t)(o->y - height), o->y,
                     (uint16_t)(o->x - (THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE)),
                     (uint16_t)(o->x + (THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE))))
            return true;
    }
    return false;
}

/* The ranges of SpacePirateCheckSamusInShootingRange and
 * SpacePirateShootingLaser. */
static uint8_t shooting_nslr(Ctx *c) {
    uint16_t x_range;
    if (c->s->sprite_id == MZM_PSPRITE_SPACE_PIRATE2) {
        x_range = BLOCK_SIZE * 8;
    } else {
        x_range = BLOCK_SIZE * 6;
        if (c->f->samus_x_velocity) x_range = BLOCK_SIZE * 3;
    }
    uint16_t y_range = c->f->globals->draw_order[0] == AIM_FORWARD ? BLOCK_SIZE * 2
                                                                   : BLOCK_SIZE * 6;
    return ai_samus_nslr(c, y_range, x_range);
}

/* SpacePirateCheckSamusInShootingRange */
static bool samus_in_shooting_range(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t *d = g->draw_order;
    bool right = facing_right(s);
    if (d[2] == 1) {
        uint8_t nslr = shooting_nslr(c);
        if (nslr == (right ? NSLR_RIGHT : NSLR_LEFT)) {
            s->pose = P_CHARGING_LASER_INIT;
            s->work1 = d[0];
            return true;
        }
        nslr = ai_samus_nslr(c, BLOCK_SIZE * 10, BLOCK_SIZE * 10);
        if (nslr != (right ? NSLR_LEFT : NSLR_RIGHT)) return false;
        s->pose = P_TURNING_AROUND_ALERTED_INIT;
        return true;
    }
    /* Samus behind the pirate. */
    bool behind = right ? d[4] == 0 : d[4] == 1;
    if (isft(s) && behind) {
        s->pose = P_TURNING_AROUND_ALERTED_INIT;
        return true;
    }
    if (s->sprite_id != MZM_PSPRITE_SPACE_PIRATE_WAITING2 && behind &&
        (g->alarm_timer || !(s->status & MZM_SPRITE_STATUS_MOSAIC)) && s->rotation == ONE_SECOND) {
        s->pose = P_TURNING_AROUND_ALERTED_INIT;
        return true;
    }
    return false;
}

/* unk_29b68: true when the pose changed. */
static bool detect_samus(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t *d = g->draw_order;
    uint8_t previous = s->pose;
    if (g->alarm_timer >= ALARM_ACTIVE - 1) s->isft++;
    bool right = facing_right(s);
    if (d[2] == true) {
        if (d[1] == (right ? true : false)) {
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
            s->pose = P_WALKING_ALERTED_INIT;
        } else if (g->boss_work2 < 0x300) {
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
            s->pose = P_TURNING_AROUND_ALERTED_INIT;
        }
    }
    if (isft(s)) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        if (d[4] == (right ? false : true)) s->pose = P_TURNING_AROUND_ALERTED_INIT;
        else s->pose = P_WALKING_ALERTED_INIT;
    }
    return previous != s->pose;
}

/* SpacePirateDetectSamusWhileCrawling */
static bool detect_samus_while_crawling(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t *d = g->draw_order;
    uint8_t previous = s->pose;
    if (!(s->status & MZM_SPRITE_STATUS_FACING_DOWN) && g->alarm_timer >= ALARM_ACTIVE - 1)
        s->isft++;
    bool right = facing_right(s);
    if (d[2] == 1 && d[1] == (right ? 1 : 0)) s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    if (isft(s)) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        if (d[4] == (right ? 0 : 1)) s->pose = P_CRAWLING_STOPPED_INIT;
    }
    return previous != s->pose;
}

/* SpacePirateClimbingCheckWallJumpOrFire: 0 when nothing happens, the new
 * pose from pose 0x19, else 1. */
static uint8_t climbing_check_wall_jump_or_fire(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t *d = g->draw_order;
    uint8_t old_pose = s->pose, new_pose = s->pose;
    uint16_t y = s->y, x = ahead(s, BLOCK_SIZE - QUARTER_BLOCK_SIZE);
    if (!(s->status & MZM_SPRITE_STATUS_FACING_DOWN) && g->alarm_timer >= ALARM_ACTIVE - 1)
        s->isft++;
    if (d[2] == 1 || isft(s)) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        if (ai_movement_at(c, y, x) == MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_WALLJUMP_POINT) {
            new_pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        } else if (ai_movement_at(c, (uint16_t)(y - (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE)), x) ==
                   MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_WALLJUMP_POINT) {
            /* A wall jump point above the pirate: nothing happens. */
        } else if (d[0] == 0) {
            uint16_t samus_range = (uint16_t)(c->f->samus_y + c->f->samus_hitbox_top / 2);
            uint16_t pirate_range = (uint16_t)(y - BLOCK_SIZE * 2);
            pirate_range = pirate_range > samus_range ? (uint16_t)(pirate_range - samus_range)
                                                      : (uint16_t)(samus_range - pirate_range);
            if (pirate_range < BLOCK_SIZE + PIXEL_SIZE) new_pose = P_CLIMBING_CHARGING_LASER_INIT;
        } else if (d[0] == 1) {
            if (old_pose != P_CLIMBING_UP) new_pose = P_CLIMBING_UP_INIT;
        } else {
            if (old_pose != P_CLIMBING_DOWN) new_pose = P_CLIMBING_DOWN_INIT;
        }
    } else if (ai_movement_at(c, y, x) == MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_WALLJUMP_POINT) {
        new_pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
    }
    if (old_pose == new_pose) return 0;
    if (old_pose == P_0x19) return new_pose;
    if (new_pose == P_TURNING_AROUND_TO_WALL_JUMP_INIT) {
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        s->work1 = 0;
        s->work0 = true;
    } else {
        s->pose = P_0x18;
    }
    return 1;
}

/* The zoneline exit of unk_29ef0: true when the pirate was removed. */
static bool leave_at_zoneline(Ctx *c, uint16_t y, uint16_t x) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    if (ai_movement_at(c, y, x) != MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_ZONELINE) return false;
    if (s->sprite_id == MZM_PSPRITE_SPACE_PIRATE_WAITING2) {
        s->status = 0;
        if (g->alarm_timer) disable_alarm(c);
        return true;
    }
    if (!g->alarm_timer && s->status & MZM_SPRITE_STATUS_MOSAIC) {
        s->status = 0;
        return true;
    }
    return false;
}

/* unk_29ef0: the ground and obstacles ahead of a walking pirate (jumps,
 * crawling, turning, zonelines). */
static void check_path(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t *d = c->f->globals->draw_order;
    uint8_t previous = s->pose, flags = 0;
    s->work1 = 0;
    uint16_t y = s->y, x = s->x;
    uint16_t top = ai_check_vertical(c, (uint16_t)(y - PIXEL_SIZE), x);
    if ((c->vertical & MZM_COLLISION_FLAGS_F) < MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
        top = ai_check_vertical(c, y, x);
        if ((c->vertical & MZM_COLLISION_FLAGS_F) < MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
            top = ai_check_vertical(c, (uint16_t)(y + PIXEL_SIZE), x);
            if (c->vertical != MZM_COLLISION_AIR) s->y = y = top;
        } else {
            s->y = y = top;
        }
    } else {
        s->y = y = top;
    }
    bool right = facing_right(s);
    int side = right ? 1 : -1;
    if (c->vertical == MZM_COLLISION_AIR) {
        ai_check_vertical(c, y, (uint16_t)(x - side * QUARTER_BLOCK_SIZE));
        if (c->vertical == MZM_COLLISION_AIR) {
            s->pose = P_JUMPING_INIT;
            return;
        }
        uint8_t steep = right ? MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE
                              : MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
        uint8_t slight = right ? MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE
                               : MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE;
        if (c->vertical == steep || c->vertical == slight)
            s->y = ai_check_vertical(c, (uint16_t)(y + BLOCK_SIZE), x);
        return;
    }
    if (c->vertical & MZM_COLLISION_FLAGS_F0) {
        if (ai_collision_at(c, y, (uint16_t)(x + side * (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE))) ==
                MZM_COLLISION_AIR) {
            if (previous == P_WALKING && s->scaling > 20) {
                s->pose = P_0x16;
                return;
            }
            if (d[2] == true && d[0] < 2) s->work1 = 2;
            s->pose = P_JUMPING_INIT;
            return;
        }
        uint16_t front = (uint16_t)(x + side * (BLOCK_SIZE + QUARTER_BLOCK_SIZE));
        if (ai_collision_at(c, (uint16_t)(y - HALF_BLOCK_SIZE), front) == MZM_COLLISION_SOLID) {
            if (leave_at_zoneline(c, (uint16_t)(y - HALF_BLOCK_SIZE), front)) return;
            uint8_t collision = ai_collision_at(c, (uint16_t)(y - HALF_BLOCK_SIZE),
                                                (uint16_t)(x + side * (HALF_BLOCK_SIZE + 8)));
            if (collision == MZM_COLLISION_AIR || collision & MZM_COLLISION_FLAGS_F0) flags |= 1;
        }
        if (ai_collision_at(c, (uint16_t)(y - (BLOCK_SIZE + HALF_BLOCK_SIZE)), front) ==
                MZM_COLLISION_SOLID)
            flags |= 2;
        if (ai_collision_at(c, (uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)), front) ==
                MZM_COLLISION_SOLID)
            flags |= 4;
        if (ai_collision_at(c, (uint16_t)(y - (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE)), front) ==
                MZM_COLLISION_SOLID)
            flags |= 8;
    }
    if (previous == P_WALKING) {
        if (flags == 0 || flags == 8) return;
        if (s->scaling > 20) {
            s->pose = P_0x16;
            return;
        }
    }
    uint16_t samus_y = c->f->samus_y;
    switch (flags) {
    case 1:
        s->work1 = 1;
        s->pose = P_JUMPING_INIT;
        break;
    case 2:
        if (samus_y >= (uint16_t)(y - (BLOCK_SIZE + HALF_BLOCK_SIZE))) {
            s->pose = P_STARTING_TO_CRAWL_INIT;
            break;
        }
        /* fallthrough */
    case 1 | 2:
        s->work1 = 2;
        s->pose = P_JUMPING_INIT;
        break;
    case 4:
    case 2 | 4:
        if (samus_y >= (uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE))) {
            s->pose = P_STARTING_TO_CRAWL_INIT;
        } else {
            s->work1 = 3;
            s->pose = P_JUMPING_INIT;
        }
        break;
    case 2 | 4 | 8:
        if (samus_y >= (uint16_t)(y - (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE))) {
            s->pose = P_STARTING_TO_CRAWL_INIT;
            break;
        }
        /* fallthrough */
    case 1 | 4:
    case 1 | 2 | 4:
    case 1 | 2 | 4 | 8:
        s->work1 = 3;
        s->pose = P_JUMPING_INIT;
        break;
    case 0:
    case 8:
        break;
    default:
        s->pose = previous == P_WALKING ? P_0x16 : P_TURNING_AROUND_ALERTED_INIT;
        break;
    }
    if (s->pose == P_JUMPING_INIT && s->work1 > 1) {
        /* Room above the jump. */
        uint16_t probe = (uint16_t)(x + side * (BLOCK_SIZE - QUARTER_BLOCK_SIZE));
        if (ai_collision_at(c, (uint16_t)(y - (BLOCK_SIZE * 3 + HALF_BLOCK_SIZE)), probe) ==
                MZM_COLLISION_AIR &&
            ai_collision_at(c, (uint16_t)(y - (BLOCK_SIZE * 4 + HALF_BLOCK_SIZE)), probe) ==
                MZM_COLLISION_AIR)
            return;
        s->pose = previous == P_WALKING ? P_0x16 : P_TURNING_AROUND_ALERTED_INIT;
    }
}

/* SpriteUtilMakeSpriteFaceSamusDirection */
static void face_samus_direction(Ctx *c) {
    if (c->s->x > c->f->samus_x) c->s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else c->s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
}

/* SpriteUtilMoveHorizontallyForwardOnSlopeDirection: two thirds of the
 * speed up a steep slope. */
static void move_forward_on_slope(Ctx *c, int16_t movement) {
    MzmSprite *s = c->s;
    int32_t velocity = movement;
    uint8_t collision = ai_collision_at(c, s->y, s->x);
    if (facing_right(s) ? collision == MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE
                        : collision == MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE)
        velocity = (int16_t)(velocity * 2 / 3);
    s->x = (uint16_t)(facing_right(s) ? s->x + velocity : s->x - velocity);
}

/* SpacePirateClimbingUpInit */
static void climbing_up_init(MzmSprite *s) {
    s->pose = P_CLIMBING_UP;
    set_anim(s, MZM_PIRATE_CLIMBING_UP);
    s->x = (uint16_t)((s->x & BLOCK_POSITION_FLAG) + HALF_BLOCK_SIZE);
    s->work0 = true;
    s->work1 = 0;
}

/* SpacePirateClimbingDownInit */
static void climbing_down_init(MzmSprite *s) {
    s->pose = P_CLIMBING_DOWN;
    set_anim(s, MZM_PIRATE_CLIMBING_DOWN);
    s->x = (uint16_t)((s->x & BLOCK_POSITION_FLAG) + HALF_BLOCK_SIZE);
    s->work0 = true;
}

/* SpacePirateInit */
static void pirate_init(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t id = s->sprite_id;
    s->draw_top = BLOCK_SIZE * 4 / PIXEL_SIZE;
    s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->hitbox_bottom = 0;
    s->anim = MZM_PIRATE_WALKING;
    s->frame = c->rng & (WALKING_FRAMES - 1);
    s->adc = 0;
    s->health = c->tables->health[id];
    s->scaling = 0x100;             /* Q_8_8(1.f) */
    s->work2 = 0;
    s->rotation = 0;
    if (id == MZM_PSPRITE_SPACE_PIRATE_WAITING1 || id == MZM_PSPRITE_SPACE_PIRATE_WAITING2 ||
        id == MZM_PSPRITE_SPACE_PIRATE_WAITING3) {
        s->samus_collision = SSC_NONE;
        if (s->room_slot & 0x80) {
            /* Spawned at a door by SpawnWaitingPirates. */
            face_samus_direction(c);
            if (facing_right(s)) s->x = (uint16_t)(s->x - (BLOCK_SIZE * 2 + EIGHTH_BLOCK_SIZE));
            else s->x = (uint16_t)(s->x + (BLOCK_SIZE * 2 + EIGHTH_BLOCK_SIZE));
            if (id == MZM_PSPRITE_SPACE_PIRATE_WAITING2) s->work0 = ONE_SECOND * 2;
            else if (c->f->area == AREA_CHOZODIA && c->f->room == 80) s->work0 = 110;
            else s->work0 = ONE_THIRD_SECOND;
            s->pose = P_WAITING_AT_DOOR;
            s->status |= MZM_SPRITE_STATUS_MOSAIC | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        } else if (id == MZM_PSPRITE_SPACE_PIRATE_WAITING2) {
            s->status = 0;
            return;
        } else {
            if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                                (uint16_t)(s->x - BLOCK_SIZE * 2)) != MZM_COLLISION_AIR) {
                s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
                s->x = (uint16_t)(s->x - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
            } else {
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
                s->x = (uint16_t)(s->x + (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
            }
            s->pose = P_IDLE_AT_DOOR;
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_MOSAIC |
                         MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        }
    } else {
        s->samus_collision = SSC_SPACE_PIRATE;
        uint16_t y = s->y, x = s->x;
        uint8_t collision = ai_collision_at(c, y, x);
        bool wall_left = false, air_right = false;
        if (collision == MZM_COLLISION_AIR) {
            wall_left = ai_collision_at(c, y, (uint16_t)(x - BLOCK_SIZE)) != MZM_COLLISION_AIR;
            if (!wall_left)
                air_right = ai_collision_at(c, y, (uint16_t)(x + BLOCK_SIZE)) == MZM_COLLISION_AIR;
        }
        if (g->alarm_timer) {
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
            if (collision == MZM_COLLISION_AIR && wall_left) {
                climbing_up_init(s);
            } else if (collision == MZM_COLLISION_AIR && !air_right) {
                s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
                climbing_up_init(s);
            } else {
                face_samus_direction(c);
                s->pose = P_WALKING_ALERTED;
                s->work2 = 8;
            }
        } else if (ai_movement_at(c, y, x) == MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID) {
            /* The movement of the block at the pirate, read by the
             * collision check above (no wall probe ran). */
            set_anim(s, MZM_PIRATE_STANDING);
            s->work0 = (uint8_t)(c->rng * SIXTH_SECOND + ONE_SECOND);
            s->work1 = 0;
            s->pose = P_IDLE;
            if (ai_movement_at(c, y, (uint16_t)(x + BLOCK_SIZE)) ==
                    MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID)
                s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            else
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
        } else if (collision == MZM_COLLISION_AIR && wall_left) {
            if (c->rng & 1) climbing_up_init(s);
            else climbing_down_init(s);
        } else if (collision == MZM_COLLISION_AIR && !air_right) {
            if (c->rng & 1) climbing_down_init(s);
            else climbing_up_init(s);
        } else {
            face_samus_direction(c);
            s->pose = P_WALKING;
        }
    }
    flip(s);
}

/* SpacePirateFallingInit */
static void falling_init(MzmSprite *s) {
    s->pose = P_FALLING;
    s->work3 = 0;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    set_anim(s, MZM_PIRATE_FALLING);
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_DOUBLE_SIZE;
}

/* The landing of SpacePirateFalling and SpacePirateJumping (DOUBLE_SIZE
 * marks a landed pirate). */
static void land(MzmSprite *s, uint16_t top) {
    s->y = top;
    s->status |= MZM_SPRITE_STATUS_DOUBLE_SIZE;
    set_anim(s, MZM_PIRATE_LANDING);
    ai_sound_onscreen_once(s, MZM_SOUND_SPACE_PIRATE_LANDING);
}

/* The footsteps, climbing and crawling sounds: once the animation frame
 * (a multiple of 4) has lasted more than `ticks`. */
static void step_sound(const MzmSprite *s, int ticks, uint16_t sound) {
    if (s->adc > ticks && (s->frame & 3) == 0) ai_sound_onscreen_once(s, sound);
}

/* SpacePirateFalling */
static void falling(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t *d = c->f->globals->draw_order;
    if (s->status & MZM_SPRITE_STATUS_DOUBLE_SIZE) {
        if (nearly_ended(c)) {
            if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
                s->pose = P_WALKING_ALERTED_INIT;
                if (d[4] == (facing_right(s) ? false : true))
                    s->pose = P_TURNING_AROUND_ALERTED_INIT;
            } else {
                s->pose = P_0xE;
            }
        }
        return;
    }
    uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        land(s, top);
        return;
    }
    uint8_t offset = s->work3;
    int16_t movement = c->tables->falling[offset < 16 ? offset : 15];
    if (movement == SHORT_MAX) {
        movement = c->tables->falling[offset - 1];
    } else {
        s->work3 = (uint8_t)(offset + 1);
    }
    s->y = (uint16_t)(s->y + movement);
}

/* unk_2a768, unk_2a794: a crouched or alerted stop (pose 0xF). */
static void stop_init(MzmSprite *s, uint8_t anim) {
    s->pose = P_0xF;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    set_anim(s, anim);
    s->work2 = 0;
}

/* unk_2a7c0 */
static void stopped(Ctx *c) {
    ai_align_on_slope(c);
    if (c->vertical == MZM_COLLISION_AIR) c->s->pose = P_FALLING_INIT;
    else if (nearly_ended(c) && !detect_samus(c)) c->s->pose = P_WALKING_INIT;
}

/* SpacePirateWalkingInit */
static void walking_init(MzmSprite *s) {
    s->pose = P_WALKING;
    s->work2 = 0;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    if (s->anim != MZM_PIRATE_WALKING) set_anim(s, MZM_PIRATE_WALKING);
}

/* SpacePirateWalking */
static void walking(Ctx *c) {
    MzmSprite *s = c->s;
    s->adc++;
    if (detect_samus(c)) return;
    /* A pirate removed at a zoneline runs the rest, as natively. */
    check_path(c);
    if (s->pose != P_WALKING) return;
    if (colliding_with_pirate_when_walking(c)) {
        if (s->anim == MZM_PIRATE_WALKING) set_anim(s, MZM_PIRATE_CROUCHED);
        return;
    }
    if (s->anim == MZM_PIRATE_CROUCHED) {
        if (!ended(c)) return;
        set_anim(s, MZM_PIRATE_WALKING);
        s->work2 = 0;
    }
    move_forward_on_slope(c, (int16_t)(s->work2 / 4));
    if (s->work2 < HALF_BLOCK_SIZE) s->work2 += PIXEL_SIZE / 2;
    else if (s->work2 >= HALF_BLOCK_SIZE + 1) s->work2--;
    if (s->scaling < 0x100) s->scaling++;
    /* CONVERT_SECONDS(.1f) */
    step_sound(s, 6 - 1, MZM_SOUND_SPACE_PIRATE_FOOTSTEPS);
}

/* SpacePirateIdle */
static void idle(Ctx *c) {
    MzmSprite *s = c->s;
    bool turn = false;
    switch (s->work1) {
    case 0:
        if (detect_samus(c)) {
            if (s->pose == P_0x10) {
                s->pose = P_IDLE;
                turn = true;
            } else if (s->pose == P_0xE) {
                s->pose = P_IDLE;
                set_anim(s, MZM_PIRATE_CROUCHED);
                s->work1 = 2;
            } else {
                s->samus_collision = SSC_SPACE_PIRATE;
            }
        } else if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            if (ended(c)) turn = true;
        } else if (s->work0) {
            s->work0--;
        } else if (c->rng == s->ram_slot / 2) {
            turn = true;
        }
        if (turn) {
            set_anim(s, MZM_PIRATE_TURNING_AROUND_FIRST_PART);
            s->work1 = 1;
        }
        break;
    case 1:
        if (ended(c)) {
            s->work1 = 2;
            set_anim(s, MZM_PIRATE_TURNING_AROUND_SECOND_PART);
            s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT | MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            flip(s);
        }
        break;
    case 2:
        if (ended(c)) {
            s->work1 = 0;
            set_anim(s, MZM_PIRATE_STANDING);
            s->work0 = (uint8_t)(c->rng * SIXTH_SECOND + ONE_SECOND);
        }
        break;
    }
}

/* SpacePirateIdleAtDoor */
static void idle_at_door(Ctx *c) {
    c->s->ignore_samus_timer = 1;
    if (c->f->globals->alarm_timer) {
        c->s->work0 = ONE_THIRD_SECOND;
        c->s->pose = P_WAITING_AT_DOOR;
    }
}

/* unk_2aa5c */
static void lookout_init(MzmSprite *s) {
    set_anim(s, MZM_PIRATE_STANDING);
    s->pose = P_0x2B;
    s->work2 = 0;
    s->work0 = 6;
}

/* unk_2aa88 */
static void lookout(Ctx *c) {
    MzmSprite *s = c->s;
    ai_align_on_slope(c);
    if (c->vertical == MZM_COLLISION_AIR) {
        s->pose = P_FALLING_INIT;
        return;
    }
    if (samus_in_shooting_range(c)) return;
    if (s->work0) {
        if (ended(c)) s->work0--;
    } else if (nearly_ended(c)) {
        s->pose = P_TURNING_AROUND_ALERTED_INIT;
    }
}

/* unk_2aaec, unk_2ab10, unk_2ab34: a stop before turning (pose 0x11). */
static void pause_init(MzmSprite *s, uint8_t anim) {
    set_anim(s, anim);
    s->pose = P_0x11;
    s->work2 = 0;
}

/* unk_2ab58 */
static void pause_then_turn(Ctx *c) {
    MzmSprite *s = c->s;
    ai_align_on_slope(c);
    if (c->vertical == MZM_COLLISION_AIR) {
        s->pose = P_FALLING_INIT;
        return;
    }
    if (ended(c)) {
        s->pose = P_TURNING_AROUND_FIRST_PART;
        s->scaling = 0;
        set_anim(s, MZM_PIRATE_TURNING_AROUND_FIRST_PART);
    }
}

/* SpacePirateTurningAroundInit */
static void turning_around_init(MzmSprite *s) {
    s->pose = P_TURNING_AROUND_FIRST_PART;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->scaling = 0;
    set_anim(s, MZM_PIRATE_TURNING_AROUND_FIRST_PART);
    s->work2 = 0;
}

/* SpacePirateTurningAroundFirstPart and its alerted version (whose
 * animation runs twice as fast). */
static void turning_around_first_part(Ctx *c, uint8_t next) {
    MzmSprite *s = c->s;
    if (ended(c)) {
        s->pose = next;
        set_anim(s, MZM_PIRATE_TURNING_AROUND_SECOND_PART);
        s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
        flip(s);
    }
}

/* SpacePirateTurningAroundSecondPart */
static void turning_around_second_part(Ctx *c) {
    MzmSprite *s = c->s;
    if (!nearly_ended(c)) return;
    if (detect_samus(c)) {
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN && !samus_in_shooting_range(c))
            s->pose = P_WALKING_ALERTED_INIT;
    } else {
        s->pose = P_WALKING_INIT;
    }
}

/* SpacePirateTurningAroundAlertedInit */
static void turning_around_alerted_init(MzmSprite *s) {
    s->pose = P_TURNING_AROUND_ALERTED_FIRST_PART;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    set_anim(s, MZM_PIRATE_TURNING_AROUND_FIRST_PART);
    s->work2 = 0;
}

/* SpacePirateTurningAroundSecondPartAlerted */
static void turning_around_second_part_alerted(Ctx *c) {
    MzmSprite *s = c->s;
    s->adc++;
    if (!nearly_ended(c)) return;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (!samus_in_shooting_range(c)) {
            check_path(c);
            if (s->pose == P_TURNING_AROUND_ALERTED_SECOND_PART)
                s->pose = P_WALKING_ALERTED_INIT;
            else if (s->sprite_id == MZM_PSPRITE_SPACE_PIRATE2)
                s->pose = P_0x2A;
        }
    } else if (!detect_samus(c)) {
        s->pose = P_0xE;
    }
}

/* SpacePirateWalkingAlertedInit */
static void walking_alerted_init(MzmSprite *s) {
    s->pose = P_WALKING_ALERTED;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->work2 = 8;
    if (s->anim != MZM_PIRATE_WALKING) set_anim(s, MZM_PIRATE_WALKING);
}

/* SpacePirateWalkingAlerted */
static void walking_alerted(Ctx *c) {
    MzmSprite *s = c->s;
    s->adc++;
    if (samus_in_shooting_range(c)) return;
    check_path(c);
    if (s->pose != P_WALKING_ALERTED) {
        if ((s->sprite_id == MZM_PSPRITE_SPACE_PIRATE_WAITING2 ||
             s->sprite_id == MZM_PSPRITE_SPACE_PIRATE2) && s->pose == P_JUMPING_INIT)
            s->pose = P_0x2A;
        return;
    }
    if (colliding_with_pirate_when_walking(c)) {
        if (s->anim == MZM_PIRATE_WALKING) {
            set_anim(s, MZM_PIRATE_CROUCHED);
        } else if (s->anim == MZM_PIRATE_CROUCHED && c->f->globals->draw_order[2] == true) {
            uint8_t nslr = ai_samus_nslr(c, BLOCK_SIZE * 10, BLOCK_SIZE * 10);
            if (nslr == (facing_right(s) ? NSLR_LEFT : NSLR_RIGHT))
                s->pose = P_TURNING_AROUND_ALERTED_INIT;
        }
        return;
    }
    if (s->anim == MZM_PIRATE_CROUCHED) {
        if (!ended(c)) return;
        set_anim(s, MZM_PIRATE_WALKING);
        s->work2 = 0;
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        move_forward_on_slope(c, (int16_t)(s->work2 / 4));
        if (s->work2 < THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE) s->work2 += PIXEL_SIZE / 2;
    } else {
        s->pose = P_WALKING;
    }
    step_sound(s, 6 - 1, MZM_SOUND_SPACE_PIRATE_FOOTSTEPS);
}

/* SpacePirateJumpingInit */
static void jumping_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = P_JUMPING;
    s->work3 = 0;
    set_anim(s, MZM_PIRATE_JUMPING);
    s->work0 = 0;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_DOUBLE_SIZE;
    if (facing_right(s) ? c->f->samus_x < s->x : c->f->samus_x > s->x) s->work2 = 0;
}

/* The pirate's head against a ceiling: snapped below it. */
static void bonk(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t head = (uint16_t)(s->y - (SPACE_PIRATE_HEIGHT - BLOCK_SIZE));
    if (ai_collision_at(c, head, s->x) != MZM_COLLISION_AIR)
        s->y = (uint16_t)((head & BLOCK_POSITION_FLAG) + SPACE_PIRATE_HEIGHT);
}

static int16_t jump_velocity(const int16_t *table, int size, uint8_t *offset) {
    int16_t movement = table[*offset >> 2];
    if (*offset < size * 4 - 1) (*offset)++;
    return movement;
}

/* SpacePirateJumping (also the wall jumps; BlackSpacePirateJumping when
 * `black`). */
static void jumping_of(Ctx *c, bool black) {
    MzmSprite *s = c->s;
    uint8_t *d = c->f->globals->draw_order;
    if (s->status & MZM_SPRITE_STATUS_DOUBLE_SIZE) {
        if (!nearly_ended(c)) return;
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
            if (!samus_in_shooting_range(c)) {
                check_path(c);
                if (s->pose == P_JUMPING || s->pose == P_WALL_JUMPING)
                    s->pose = P_WALKING_ALERTED_INIT;
            }
        } else if (!detect_samus(c)) {
            check_path(c);
            if (s->pose == P_JUMPING || s->pose == P_WALL_JUMPING) s->pose = P_WALKING_INIT;
        }
        /* BlackSpacePirateJumping: alerted, a jump or a crawl ahead away
         * from Samus becomes a turn. */
        if (black && s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
            if (s->pose == P_JUMPING_INIT) {
                if (d[4] == (facing_right(s) ? false : true)) s->pose = P_TURNING_AROUND_ALERTED_INIT;
            } else if (s->pose == P_STARTING_TO_CRAWL_INIT) {
                s->pose = P_TURNING_AROUND_ALERTED_INIT;
            }
        }
        return;
    }
    int movement;
    if (s->pose == P_JUMPING) {
        if (s->frame < 4) {
            /* The crouch before the jump. */
            if (s->frame == 3 && ai_timer(c, s->anim, s->frame) < s->adc + 1) {
                s->y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
                bonk(c);
            }
            return;
        }
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
            movement = s->work2 / 4;
            if (movement < 6) movement = 5;
            else if (movement > (black ? 11 : 8)) movement = black ? 12 : 9;
        } else {
            movement = 4;
        }
    } else {
        movement = 12;
    }
    int side = facing_right(s) ? 1 : -1;
    uint16_t front = (uint16_t)(s->x + side * HALF_BLOCK_SIZE);
    int colliding = 0;
    if (ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE)), front) !=
            MZM_COLLISION_AIR)
        colliding++;
    if (ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE +
                                              PIXEL_SIZE / 2)), front) != MZM_COLLISION_AIR)
        colliding++;
    if (ai_collision_at(c, (uint16_t)(s->y - QUARTER_BLOCK_SIZE), front) != MZM_COLLISION_AIR)
        colliding++;
    if (!colliding) s->x = (uint16_t)(s->x + side * movement);
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN && colliding && s->y < c->f->samus_y &&
        (facing_right(s) ? s->x > c->f->samus_x : s->x < c->f->samus_x))
        colliding = 0;
    if (colliding) {
        uint8_t delay = colliding > 2 ? 2 : colliding > 1 ? 15 : 30;
        s->work0++;
        if (s->work0 > delay) {
            /* Against a wall: climb it. */
            uint8_t collision = ai_collision_at(
                c, (uint16_t)(s->y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                ahead(s, WALL_X));
            if (collision == MZM_COLLISION_AIR) {
                s->pose = P_CLIMBING_UP_INIT;
            } else if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
                if (d[3] == true) {
                    bool up = facing_right(s) ? s->x <= c->f->samus_x : s->x > c->f->samus_x;
                    s->pose = up ? P_CLIMBING_UP_INIT : P_CLIMBING_DOWN_INIT;
                } else {
                    s->pose = P_CLIMBING_UP_INIT;
                }
            } else {
                s->pose = c->rng & 1 ? P_CLIMBING_DOWN_INIT : P_CLIMBING_UP_INIT;
            }
            s->x = (uint16_t)((s->x & BLOCK_POSITION_FLAG) + HALF_BLOCK_SIZE);
            return;
        }
    }
    if (s->work3 == 0) ai_sound_onscreen_once(s, MZM_SOUND_SPACE_PIRATE_JUMPING);
    const MzmSpriteTables *t = c->tables;
    int16_t velocity;
    if (s->work1 == 1) velocity = jump_velocity(t->pirate_jump_low, 11, &s->work3);
    else if (s->work1 == 2) velocity = jump_velocity(t->pirate_jump, 10, &s->work3);
    else if (s->work1 == 3) velocity = jump_velocity(t->pirate_jump_high, 11, &s->work3);
    else if (black && s->work1 == 4) velocity = jump_velocity(t->pirate_jump_black, 12, &s->work3);
    else velocity = jump_velocity(t->pirate_wall_jump, 8, &s->work3);
    s->y = (uint16_t)(s->y + velocity);
    if (velocity > 0) {
        uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
        if (c->vertical != MZM_COLLISION_AIR) land(s, top);
    } else {
        bonk(c);
    }
}

static void jumping(Ctx *c) { jumping_of(c, false); }

/* SpacePirateChargingLaserInit */
static void charging_laser_init(MzmSprite *s) {
    s->pose = P_CHARGING_LASER;
    s->work0 = 24;                  /* CONVERT_SECONDS(.4f) */
    if (s->work1 == AIM_DIAGONALLY_UP) set_anim(s, MZM_PIRATE_CHARGING_LASER_DIAGONALLY_UP);
    else if (s->work1 == AIM_DIAGONALLY_DOWN)
        set_anim(s, MZM_PIRATE_CHARGING_LASER_DIAGONALLY_DOWN);
    else set_anim(s, MZM_PIRATE_CHARGING_LASER_FORWARD);
    s->work2 = 0;
}

/* SpacePirateChargingLaser */
static void charging_laser(Ctx *c) {
    MzmSprite *s = c->s;
    ai_align_on_slope(c);
    if (c->vertical == MZM_COLLISION_AIR) {
        s->pose = P_FALLING_INIT;
        return;
    }
    if (s->work0) s->work0--;
    else s->pose = P_SHOOTING_LASER_INIT;
}

/* SpacePirateShootingLaserInit */
static void shooting_laser_init(MzmSprite *s) {
    s->pose = P_SHOOTING_LASER;
    if (s->work1 == AIM_DIAGONALLY_UP) set_anim(s, MZM_PIRATE_SHOOTING_DIAGONALLY_UP);
    else if (s->work1 == AIM_DIAGONALLY_DOWN) set_anim(s, MZM_PIRATE_SHOOTING_DIAGONALLY_DOWN);
    else set_anim(s, MZM_PIRATE_SHOOTING_FORWARD);
}

/* SpacePirateShootingLaser */
static void shooting_laser(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t *d = c->f->globals->draw_order;
    if (s->frame == SHOOTING_FRAMES / 2 && s->adc == 1) fire_laser_ground(c);
    ai_align_on_slope(c);
    if (c->vertical == MZM_COLLISION_AIR) {
        s->pose = P_FALLING_INIT;
        return;
    }
    if (!nearly_ended(c)) return;
    if (d[2] == 0) {
        s->pose = P_WALKING_ALERTED_INIT;
        return;
    }
    uint8_t nslr = shooting_nslr(c);
    uint8_t front = facing_right(s) ? NSLR_RIGHT : NSLR_LEFT;
    uint8_t back = facing_right(s) ? NSLR_LEFT : NSLR_RIGHT;
    if (nslr == front) {
        s->pose = P_CHARGING_LASER_INIT;
        s->work1 = d[0];
    } else if (nslr == back) {
        s->pose = P_TURNING_AROUND_ALERTED_INIT;
    } else {
        s->pose = P_WALKING_ALERTED_INIT;
    }
}

/* SpacePirateWaitingAtDoor */
static void waiting_at_door(Ctx *c) {
    MzmSprite *s = c->s;
    if (!c->f->globals->alarm_timer) {
        s->status = 0;
        return;
    }
    uint8_t collision = MZM_COLLISION_AIR;
    bool shoot = false;
    s->ignore_samus_timer = 1;
    uint16_t top = ai_check_vertical_slopes(c, s->y, ahead(s, BLOCK_SIZE));
    if (c->vertical == MZM_COLLISION_AIR) {
        s->y = (uint16_t)(s->y + QUARTER_BLOCK_SIZE);
        if (s->work0 > 2) s->work0--;
        return;
    }
    s->y = top;
    if (s->work0) s->work0--;
    if (s->room_slot == (0x80 | 0x1)) {
        if (!s->work0) shoot = true;
    } else {
        collision = ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                                    ahead(s, BLOCK_SIZE + HALF_BLOCK_SIZE));
        if (collision == MZM_COLLISION_AIR) {
            if (s->work0 < ONE_SECOND + TWO_THIRD_SECOND) shoot = true;
        } else if (!s->work0) {
            shoot = true;
        }
    }
    if (!shoot) return;
    if (s->sprite_id != MZM_PSPRITE_SPACE_PIRATE_WAITING3 ||
        ai_samus_nslr(c, BLOCK_SIZE * 6, BLOCK_SIZE * 10) != NSLR_OUT_OF_RANGE) {
        s->pose = P_SHOOTING_AFTER_WAITING_AT_DOOR;
        s->work0 = SIXTH_SECOND;
        if (collision != MZM_COLLISION_AIR)
            ai_spawn_secondary(c, MZM_SSPRITE_SPACE_PIRATE_LASER, LASER_FROM_DOOR, s->gfx_slot,
                               (uint16_t)(s->y - (BLOCK_SIZE + QUARTER_BLOCK_SIZE)), s->x,
                               facing_right(s) ? MZM_SPRITE_STATUS_X_FLIP : 0);
    }
}

/* SpacePirateShootingAfterWaitingAtDoor */
static void shooting_after_waiting_at_door(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    s->work0--;
    if (s->work0) return;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
    s->work2 = 32;
    s->samus_collision = SSC_SPACE_PIRATE;
    if (s->room_slot == (0x80 | 0x1) &&
        ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE)),
                        ahead(s, BLOCK_SIZE)) != MZM_COLLISION_AIR) {
        s->hitbox_top = -BLOCK_SIZE;
        s->pose = P_CRAWLING;
        set_anim(s, MZM_PIRATE_CRAWLING);
        return;
    }
    s->pose = P_WALKING_ALERTED;
}

/* SpacePirateClimbingUp */
static void climbing_up(Ctx *c) {
    MzmSprite *s = c->s;
    if (climbing_check_wall_jump_or_fire(c)) return;
    uint16_t y = s->y, x = s->x;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) s->work1 = 0;
    if (s->work1 > 1) {
        /* A pause on the wall. */
        if (s->frame == 0) {
            s->adc--;
            s->work1--;
        }
        return;
    }
    uint16_t wall = ahead(s, WALL_X);
    if (ai_collision_at(c, (uint16_t)(y - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                        wall) == MZM_COLLISION_AIR) {
        if (ai_collision_at(c, (uint16_t)(y - HALF_BLOCK_SIZE), wall) == MZM_COLLISION_AIR) {
            s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
            s->work1 = 0;
            s->work0 = true;
            return;
        }
        if (!(s->status & MZM_SPRITE_STATUS_FACING_DOWN) && s->work1 == 0) {
            s->work1 = TWO_THIRD_SECOND;
        } else if (ai_collision_at(c, (uint16_t)(y - (BLOCK_SIZE + HALF_BLOCK_SIZE)), wall) ==
                       MZM_COLLISION_AIR) {
            /* The top of the wall: a jump onto it. */
            s->work1 = 1;
            s->pose = P_JUMPING_INIT;
            s->y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
            return;
        }
    }
    uint8_t ceiling = ai_collision_at(c, (uint16_t)(y - BLOCK_SIZE * 3), x);
    if (ceiling != MZM_COLLISION_AIR) {
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        s->work1 = 0;
        s->work0 = false;
        return;
    }
    if (colliding_with_pirate_when_climbing(c, CLIMBING_UP)) {
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        s->work1 = ceiling;
        s->work0 = true;
        return;
    }
    int movement;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        movement = EIGHTH_BLOCK_SIZE;
        s->adc++;
    } else {
        if (s->work0 == 0) {
            if (!(c->rng & 1) && c->frame8 < s->ram_slot * 8) {
                s->pose = P_0x18;
                return;
            }
        } else {
            s->work0++;
        }
        movement = 4;
    }
    s->y = (uint16_t)(s->y - movement);
    /* CONVERT_SECONDS(.15f) */
    step_sound(s, 9, MZM_SOUND_SPACE_PIRATE_CLIMBING);
}

/* SpacePirateClimbingDown */
static void climbing_down(Ctx *c) {
    MzmSprite *s = c->s;
    if (climbing_check_wall_jump_or_fire(c)) return;
    uint16_t y = s->y, x = s->x;
    if (ai_collision_at(c, (uint16_t)(y + BLOCK_SIZE - PIXEL_SIZE), x) != MZM_COLLISION_AIR) {
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        s->work1 = 0;
        s->work0 = false;
        return;
    }
    uint16_t wall = ahead(s, WALL_X);
    if (ai_collision_at(c, y, wall) == MZM_COLLISION_AIR &&
        ai_collision_at(c, (uint16_t)(y + 0x48), wall) == MZM_COLLISION_AIR) {
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        s->work1 = 0;
        s->work0 = false;
        return;
    }
    if (colliding_with_pirate_when_climbing(c, CLIMBING_DOWN)) {
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        s->work1 = 0;
        s->work0 = true;
        return;
    }
    int movement;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        movement = EIGHTH_BLOCK_SIZE;
        s->adc++;
    } else {
        if (!s->work0) {
            if (c->rng & 1 && c->frame8 > s->ram_slot * 8) {
                s->pose = P_0x18;
                return;
            }
        } else {
            s->work0++;
        }
        movement = PIXEL_SIZE;
    }
    s->y = (uint16_t)(s->y + movement);
    step_sound(s, 9, MZM_SOUND_SPACE_PIRATE_CLIMBING);
}

/* No wall under the pirate's feet nor a block lower: it lets go. */
static bool off_wall(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t wall = ahead(s, WALL_X);
    return ai_collision_at(c, s->y, wall) == MZM_COLLISION_AIR &&
           ai_collision_at(c, (uint16_t)(s->y + BLOCK_SIZE + EIGHTH_BLOCK_SIZE), wall) ==
               MZM_COLLISION_AIR;
}

/* unk_2ba7c */
static void wall_pause_init(MzmSprite *s) {
    s->pose = P_0x19;
    s->work1 = 0;
    s->work3 = 0;
}

/* unk_2ba98: aiming on the wall, then a new climbing direction. */
static void wall_pause(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) s->adc++;
    if (off_wall(c)) {
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        s->work1 = 0;
        s->work0 = 0;
        return;
    }
    switch (s->work1) {
    case 1:
        if (ended(c)) {
            set_anim(s, MZM_PIRATE_AIMING_WHILE_CLIMBING);
            s->work1++;
        }
        break;
    case 2:
        if (ended(c)) {
            uint8_t pose = climbing_check_wall_jump_or_fire(c);
            if (pose == P_CLIMBING_CHARGING_LASER_INIT) {
                s->pose = P_CLIMBING_CHARGING_LASER_INIT;
                break;
            }
            if (pose == 0) {
                if (s->status & MZM_SPRITE_STATUS_FACING_DOWN)
                    pose = c->f->globals->draw_order[3] == true ? P_CLIMBING_DOWN_INIT
                                                               : P_CLIMBING_UP_INIT;
                else
                    pose = c->rng & 1 ? P_CLIMBING_DOWN_INIT : P_CLIMBING_UP_INIT;
            }
            if (colliding_with_pirate_when_climbing(
                    c, pose == P_CLIMBING_DOWN_INIT ? CLIMBING_DOWN : CLIMBING_UP)) {
                s->pose = P_LAUNCHING_FROM_WALL;
                set_anim(s, MZM_PIRATE_LAUNCHING_FROM_WALL);
                s->work1 = 0;
            } else {
                s->work1++;
                s->work0 = pose;
                set_anim(s, MZM_PIRATE_CLIMBING_RETRACTING_AIM);
            }
        }
        break;
    case 3:
        if (nearly_ended(c)) s->pose = s->work0;
        break;
    default:
        set_anim(s, MZM_PIRATE_TURNING_TO_AIM_WHILE_CLIMBING);
        s->work1 = 1;
        break;
    }
}

/* SpacePirateClimbingShootingLaser */
static void climbing_shooting_laser(Ctx *c) {
    MzmSprite *s = c->s;
    if (off_wall(c)) {
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP_INIT;
        s->work1 = 0;
        s->work0 = false;
        return;
    }
    if (s->frame == SHOOTING_FRAMES / 2 && s->adc == 1) fire_laser_wall(c);
    if (nearly_ended(c)) {
        set_anim(s, MZM_PIRATE_AIMING_WHILE_CLIMBING);
        s->pose = P_0x19;
        s->work1 = 2;
    }
}

/* SpacePirateTurningAroundToWallJump */
static void turning_around_to_wall_jump(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work0) {
        if (nearly_ended(c)) s->pose = P_LAUNCHING_FROM_WALL_INIT;
    } else if (ended(c)) {
        s->pose = P_DELAY_BEFORE_LAUNCHING_FROM_WALL;
        set_anim(s, MZM_PIRATE_AIMING_WHILE_CLIMBING);
    }
}

/* SpacePirateDelayBeforeLaunchingFromWall */
static void delay_before_launching(Ctx *c) {
    if (off_wall(c) || nearly_ended(c)) c->s->pose = P_LAUNCHING_FROM_WALL_INIT;
}

/* SpacePirateLaunchingFromWall */
static void launching_from_wall(Ctx *c) {
    MzmSprite *s = c->s;
    if (!nearly_ended(c)) return;
    s->pose = P_WALL_JUMPING;
    set_anim(s, MZM_PIRATE_JUMPING_FROM_WALL);
    s->work0 = 0;
    s->work3 = 0;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_DOUBLE_SIZE;
    s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
    flip(s);
}

/* SpacePirateStartingToCrawl */
static void starting_to_crawl(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->frame == 1) {
        s->hitbox_top = -(BLOCK_SIZE * 2);
        return;
    }
    if (s->frame == 2) {
        s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
        return;
    }
    if (s->frame > 2) {
        if (s->adc == 1) {
            s->hitbox_top = -BLOCK_SIZE;
            s->x = ahead(s, HALF_BLOCK_SIZE);
        }
        if (ended(c)) {
            s->pose = P_CRAWLING;
            set_anim(s, MZM_PIRATE_CRAWLING);
        }
    }
}

/* SpacePirateCrawling */
static void crawling(Ctx *c) {
    MzmSprite *s = c->s;
    if (detect_samus_while_crawling(c)) return;
    uint16_t y = s->y, x = s->x;
    if (ai_collision_at(c, (uint16_t)(y - HALF_BLOCK_SIZE),
                        ahead(s, BLOCK_SIZE + QUARTER_BLOCK_SIZE)) != MZM_COLLISION_AIR) {
        s->pose = P_CRAWLING_STOPPED_INIT;
        return;
    }
    uint16_t front = ahead(s, SPACE_PIRATE_CRAWLING_WIDTH);
    if (ai_collision_at(c, y, front) == MZM_COLLISION_AIR &&
        ai_collision_at(c, y, x) == MZM_COLLISION_AIR) {
        s->pose = P_FALLING_WHILE_CRAWLING_INIT;
        return;
    }
    uint16_t head = (uint16_t)(y - (BLOCK_SIZE + HALF_BLOCK_SIZE));
    if (ai_collision_at(c, head, front) == MZM_COLLISION_AIR &&
        ai_collision_at(c, head, x) == MZM_COLLISION_AIR) {
        s->pose = P_STANDING_UP_INIT;
        return;
    }
    int movement;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        movement = PIXEL_SIZE;
        s->adc++;
    } else {
        movement = 1;               /* ONE_SUB_PIXEL */
    }
    s->x = ahead(s, movement);
    step_sound(s, 9, MZM_SOUND_SPACE_PIRATE_CRAWLING);
}

/* The hitbox and position steps of SpacePirateStandingUp and
 * SpacePirateFallingWhileCrawling on frames 2 and 3. */
static bool stand_step(MzmSprite *s) {
    if (s->adc != 1) return false;
    if (s->frame == 2) s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
    else s->hitbox_top = -(BLOCK_SIZE * 2);
    s->x = ahead(s, QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
    return true;
}

/* SpacePirateStandingUp */
static void standing_up(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->frame == 2 || s->frame == 3) {
        stand_step(s);
        return;
    }
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    if (!nearly_ended(c)) return;
    if (detect_samus(c)) {
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN && !samus_in_shooting_range(c))
            s->pose = P_WALKING_ALERTED_INIT;
    } else {
        s->pose = s->status & MZM_SPRITE_STATUS_FACING_DOWN ? P_WALKING_ALERTED_INIT : P_0xE;
    }
}

/* SpacePirateFallingWhileCrawling */
static void falling_while_crawling(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->frame == 2) {
        stand_step(s);
    } else if (s->frame == 3) {
        if (stand_step(s)) return;
        if (s->adc == 5) {          /* CONVERT_SECONDS(1.f / 12) */
            s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
            s->pose = P_WALL_JUMPING;
            set_anim(s, MZM_PIRATE_JUMPING_FROM_WALL);
            s->work0 = 0;
            s->work1 = 0;
            s->work3 = 0;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_DOUBLE_SIZE;
        }
    }
}

/* SpacePirateDeath: the alarm stops with the last pirate. */
static void death(Ctx *c, bool play_sound) {
    MzmSprite *s = c->s;
    if (play_sound) ai_sound_onscreen(s, MZM_SOUND_SPACE_PIRATE_DYING);
    ai_sprite_death_typed(c, DEATH_NORMAL, (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE)),
                          s->x, MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
    if (!c->f->globals->alarm_timer || !c->f->sprites) return;
    int count = 0;
    for (int i = 0; i < c->f->sprite_count; ++i) {
        const MzmSprite *o = &c->f->sprites[i];
        /* The dying pirate's own slot holds its pose of the frame start
         * natively (at least SPRITE_POSE_DESTROYED). */
        if (o == s) continue;
        if (o->status & MZM_SPRITE_STATUS_EXISTS && o->samus_collision == SSC_SPACE_PIRATE &&
            o->pose < SPRITE_POSE_DESTROYED)
            count++;
    }
    if (!count) disable_alarm(c);
}

/* SpacePirateHitByLaserInit */
static void hit_by_laser_init(MzmSprite *s) {
    ai_sound_onscreen(s, MZM_SOUND_SPACE_PIRATE_DYING);
    s->pose = P_HIT_BY_LASER;
    s->health = 0;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->samus_collision = SSC_NONE;
    s->work0 = 33;                  /* CONVERT_SECONDS(.55f) */
}

/* SpacePirateHitByLaser: frozen in place, flashing the stun palette. */
static void hit_by_laser(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    s->adc--;
    uint8_t timer = --s->work0;
    if (timer & 3) return;          /* MOD_AND(timer, CONVERT_SECONDS(1.f / 15)) */
    if (timer & 4) {
        /* SPRITE_GET_STUN_PALETTE */
        s->palette_row = (uint8_t)(15 - s->gfx_slot - 1);
    } else {
        s->palette_row = s->absolute_palette_row;
        if (timer == 0) death(c, false);
    }
}

void ai_space_pirate(Ctx *c) {
    MzmSprite *s = c->s;
    bool alerted = (s->status & MZM_SPRITE_STATUS_FACING_DOWN) != 0;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t *d = g->draw_order;
    if (s->pose < SPRITE_POSE_DESTROYED) {
        if (s->freeze_timer) {
            if (c->f->equipment && c->f->equipment->suit == MZM_SUIT_SUITLESS) {
                if (c->frame8 & 1) s->freeze_timer--;
                if (s->freeze_timer == 0) s->adc--;
                if (s->freeze_timer < 46) {
                    s->palette_row = s->absolute_palette_row = s->freeze_timer & 1 ? 1 : 0;
                }
                s->ignore_samus_timer = 1;
            } else {
                ai_update_freeze_timer(c);
            }
            check_colliding_with_laser(c);
            return;
        }
        samus_detection(c);
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
            if (d[2] == true) {
                g->alarm_timer = ALARM_ACTIVE;
                if (s->sprite_id == MZM_PSPRITE_SPACE_PIRATE_WAITING2) {
                    /* Becomes a normal pirate once it sees Samus ahead. */
                    if (d[1] == (facing_right(s) ? true : false) &&
                        !(s->status & MZM_SPRITE_STATUS_NOT_DRAWN))
                        s->sprite_id = MZM_PSPRITE_SPACE_PIRATE;
                    else
                        d[2] = false;
                }
            } else if (!g->alarm_timer) {
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
            }
        } else if (g->alarm_timer && s->pose != SPRITE_POSE_UNINITIALIZED) {
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        }
        check_colliding_with_laser(c);
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: pirate_init(c); break;
    case P_WALKING_INIT: walking_init(s); break;
    case P_WALKING: walking(c); break;
    case P_IDLE: idle(c); break;
    case P_IDLE_AT_DOOR: idle_at_door(c); break;
    case P_WAITING_AT_DOOR: waiting_at_door(c); break;
    case P_SHOOTING_AFTER_WAITING_AT_DOOR: shooting_after_waiting_at_door(c); break;
    case P_0x2A: lookout_init(s); break;
    case P_0x2B: lookout(c); break;
    case P_FALLING_INIT:
        falling_init(s);
        /* fallthrough */
    case P_FALLING: falling(c); break;
    case P_0x1A:
        stop_init(s, MZM_PIRATE_STANDING_ALERTED);
        stopped(c);
        break;
    case P_0xE:
        stop_init(s, MZM_PIRATE_CROUCHED);
        stopped(c);
        break;
    case P_0xF: stopped(c); break;
    case P_0x1C:
        pause_init(s, MZM_PIRATE_STANDING_ALERTED);
        pause_then_turn(c);
        break;
    case P_0x16:
        pause_init(s, MZM_PIRATE_STANDING);
        pause_then_turn(c);
        break;
    case P_0x10:
        pause_init(s, MZM_PIRATE_CROUCHED);
        pause_then_turn(c);
        break;
    case P_0x11: pause_then_turn(c); break;
    case P_TURNING_AROUND_INIT:
        turning_around_init(s);
        /* fallthrough */
    case P_TURNING_AROUND_FIRST_PART:
        turning_around_first_part(c, P_TURNING_AROUND_SECOND_PART);
        break;
    case P_TURNING_AROUND_SECOND_PART: turning_around_second_part(c); break;
    case P_WALKING_ALERTED_INIT:
        walking_alerted_init(s);
        /* fallthrough */
    case P_WALKING_ALERTED: walking_alerted(c); break;
    case P_CHARGING_LASER_INIT:
        charging_laser_init(s);
        /* fallthrough */
    case P_CHARGING_LASER: charging_laser(c); break;
    case P_SHOOTING_LASER_INIT:
        shooting_laser_init(s);
        /* fallthrough */
    case P_SHOOTING_LASER: shooting_laser(c); break;
    case P_JUMPING_INIT:
        jumping_init(c);
        /* fallthrough */
    case P_JUMPING:
    case P_WALL_JUMPING: jumping(c); break;
    case P_TURNING_AROUND_ALERTED_INIT:
        turning_around_alerted_init(s);
        /* fallthrough */
    case P_TURNING_AROUND_ALERTED_FIRST_PART:
        s->adc++;
        turning_around_first_part(c, P_TURNING_AROUND_ALERTED_SECOND_PART);
        break;
    case P_TURNING_AROUND_ALERTED_SECOND_PART: turning_around_second_part_alerted(c); break;
    case P_CLIMBING_UP_INIT:
        climbing_up_init(s);
        /* fallthrough */
    case P_CLIMBING_UP: climbing_up(c); break;
    case P_CLIMBING_DOWN_INIT:
        climbing_down_init(s);
        /* fallthrough */
    case P_CLIMBING_DOWN: climbing_down(c); break;
    case P_0x18:
        wall_pause_init(s);
        /* fallthrough */
    case P_0x19: wall_pause(c); break;
    case P_CLIMBING_CHARGING_LASER_INIT:
        s->pose = P_CLIMBING_CHARGING_LASER;
        set_anim(s, MZM_PIRATE_CLIMBING_CHARGING_LASER_FORWARD);
        /* fallthrough */
    case P_CLIMBING_CHARGING_LASER:
        if (nearly_ended(c)) s->pose = P_CLIMBING_SHOOTING_LASER_INIT;
        break;
    case P_CLIMBING_SHOOTING_LASER_INIT:
        s->pose = P_CLIMBING_SHOOTING_LASER;
        set_anim(s, MZM_PIRATE_CLIMBING_SHOOTING_FORWARD);
        s->work3++;
        /* fallthrough */
    case P_CLIMBING_SHOOTING_LASER: climbing_shooting_laser(c); break;
    case P_TURNING_AROUND_TO_WALL_JUMP_INIT:
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP;
        set_anim(s, MZM_PIRATE_TURNING_TO_AIM_WHILE_CLIMBING);
        /* fallthrough */
    case P_TURNING_AROUND_TO_WALL_JUMP: turning_around_to_wall_jump(c); break;
    case P_DELAY_BEFORE_LAUNCHING_FROM_WALL: delay_before_launching(c); break;
    case P_LAUNCHING_FROM_WALL_INIT:
        s->pose = P_LAUNCHING_FROM_WALL;
        set_anim(s, MZM_PIRATE_LAUNCHING_FROM_WALL);
        /* fallthrough */
    case P_LAUNCHING_FROM_WALL: launching_from_wall(c); break;
    case P_STARTING_TO_CRAWL_INIT:
        s->pose = P_STARTING_TO_CRAWL;
        set_anim(s, MZM_PIRATE_STARTING_TO_CRAWL);
        /* fallthrough */
    case P_STARTING_TO_CRAWL: starting_to_crawl(c); break;
    case P_CRAWLING: crawling(c); break;
    case P_TURNING_AROUND_WHILE_CRAWLING_INIT:
        s->pose = P_TURNING_AROUND_WHILE_CRAWLING_FIRST_PART;
        set_anim(s, MZM_PIRATE_TURNING_WHILE_CRAWLING_FIRST_PART);
        break;
    case P_TURNING_AROUND_WHILE_CRAWLING_FIRST_PART:
        if (ended(c)) {
            s->pose = P_TURNING_AROUND_WHILE_CRAWLING_SECOND_PART;
            set_anim(s, MZM_PIRATE_TURNING_WHILE_CRAWLING_SECOND_PART);
            s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
            flip(s);
        }
        break;
    case P_TURNING_AROUND_WHILE_CRAWLING_SECOND_PART:
        if (ended(c)) {
            s->pose = P_CRAWLING;
            set_anim(s, MZM_PIRATE_CRAWLING);
        }
        break;
    case P_STANDING_UP_INIT:
        s->pose = P_STANDING_UP;
        set_anim(s, MZM_PIRATE_STANDING_UP);
        /* fallthrough */
    case P_STANDING_UP: standing_up(c); break;
    case P_CRAWLING_STOPPED_INIT:
        s->pose = P_CRAWLING_STOPPED;
        set_anim(s, MZM_PIRATE_CRAWLING_STOPPED);
        /* fallthrough */
    case P_CRAWLING_STOPPED:
        if (nearly_ended(c)) s->pose = P_TURNING_AROUND_WHILE_CRAWLING_INIT;
        break;
    case P_FALLING_WHILE_CRAWLING_INIT:
        s->pose = P_FALLING_WHILE_CRAWLING;
        set_anim(s, MZM_PIRATE_STANDING_UP);
        /* fallthrough */
    case P_FALLING_WHILE_CRAWLING: falling_while_crawling(c); break;
    case P_HIT_BY_LASER_INIT:
        hit_by_laser_init(s);
        /* fallthrough */
    case P_HIT_BY_LASER: hit_by_laser(c); break;
    default: death(c, true); break;
    }
    if (!alerted && (s->status & (MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                                  MZM_SPRITE_STATUS_FACING_DOWN |
                                  MZM_SPRITE_STATUS_IGNORE_PROJECTILES)) ==
                      (MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                       MZM_SPRITE_STATUS_FACING_DOWN))
        mzm_sfx_play_not_playing(MZM_SOUND_SPACE_PIRATE_GETTING_ALERTED);
}

/* BlackSpacePirateCollidingWithLaser: the other pirates' lasers burst on
 * it. */
static void black_colliding_with_laser(Ctx *c) {
    MzmSprite *s = c->s;
    if ((s->status & (MZM_SPRITE_STATUS_ONSCREEN | MZM_SPRITE_STATUS_NOT_DRAWN)) != MZM_SPRITE_STATUS_ONSCREEN ||
        !c->f->sprites)
        return;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int i = 0; i < c->f->sprite_count; ++i) {
        MzmSprite *l = &c->f->sprites[i];
        if (!(l->status & MZM_SPRITE_STATUS_EXISTS) || l->samus_collision != SSC_SPACE_PIRATE_LASER || l->work2)
            continue;
        if (touching(top, bottom, left, right, (uint16_t)(l->y - PIXEL_SIZE), (uint16_t)(l->y + PIXEL_SIZE),
                     (uint16_t)(l->x - PIXEL_SIZE), (uint16_t)(l->x + PIXEL_SIZE))) {
            l->status = 0;
            ai_particle_set(c, l->y, l->x, MZM_PE_HITTING_SOMETHING_INVINCIBLE);
            break;
        }
    }
}

/* BlackSpacePirateInit: the pirate in palette row 2, waiting to be on
 * screen; only the Plasma Beam and missiles hurt it. */
static void black_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->draw_top = BLOCK_SIZE * 4 / PIXEL_SIZE;
    s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE * 2 + HALF_BLOCK_SIZE);
    s->hitbox_bottom = 0;
    s->health = s->y_spawn = c->tables->health[s->sprite_id];
    s->scaling = 0x100;
    s->work2 = 0;
    s->rotation = 0;
    s->samus_collision = SSC_SPACE_PIRATE;
    s->absolute_palette_row = s->palette_row = 2;
    set_anim(s, MZM_PIRATE_STANDING);
    s->work0 = (uint8_t)(c->rng * 10 + ONE_SECOND);
    s->work1 = AIM_FORWARD;
    s->pose = 0x1;                                     /* BLACK_SPACE_PIRATE_POSE_INACTIVE */
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    if (s->room_slot & 0x80) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    } else {
        face_samus_direction(c);
    }
    flip(s);
}

/* BlackSpacePirateWalkingAlerted: hops over the pirate ahead when there
 * is room above. */
static void black_walking_alerted(Ctx *c) {
    MzmSprite *s = c->s;
    s->adc++;
    if (samus_in_shooting_range(c)) return;
    check_path(c);
    uint8_t *d = c->f->globals->draw_order;
    if (s->pose != P_WALKING_ALERTED) {
        if (s->pose == P_JUMPING_INIT) {
            if (d[4] == (facing_right(s) ? false : true)) s->pose = P_TURNING_AROUND_ALERTED_INIT;
        } else if (s->pose == P_STARTING_TO_CRAWL_INIT) {
            s->pose = P_TURNING_AROUND_ALERTED_INIT;
        }
        return;
    }
    bool mosaic = s->status & MZM_SPRITE_STATUS_MOSAIC;
    if (colliding_with_pirate_when_walking(c)) {
        if (ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE * 4 + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE)), s->x) ==
            MZM_COLLISION_AIR) {
            if (ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE)), s->x) ==
                MZM_COLLISION_AIR) {
                s->work2 = mosaic ? QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE : THREE_QUARTER_BLOCK_SIZE;
                s->work1 = 4;
                jumping_init(c);
                jumping_of(c, true);
            }
        } else if (s->anim == MZM_PIRATE_WALKING) {
            set_anim(s, MZM_PIRATE_CROUCHED);
        }
        return;
    }
    if (s->anim == MZM_PIRATE_CROUCHED) {
        if (!ended(c)) return;
        set_anim(s, MZM_PIRATE_WALKING);
        s->work2 = 0;
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        move_forward_on_slope(c, (int16_t)(s->work2 / 4));
        if (s->work2 < THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE) s->work2 += 2;
    } else {
        s->pose = P_WALKING;
    }
    if (s->adc >= 6 && (s->frame & 3) == 0) ai_sound_onscreen_once(s, MZM_SOUND_SPACE_PIRATE_FOOTSTEPS);
}

/* BlackSpacePirateChargingLaserInit / ChargingLaser: twice as fast. */
static void black_charging_laser_init(MzmSprite *s) {
    charging_laser_init(s);
    s->work0 = 12;                                     /* CONVERT_SECONDS(.2f) */
}

/* BlackSpacePirateShooting: twice as fast, firing a frame later. */
static void black_shooting(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t *d = c->f->globals->draw_order;
    s->adc++;
    if (s->frame == SHOOTING_FRAMES / 2 && s->adc == 2) fire_laser_ground(c);
    ai_align_on_slope(c);
    if (c->vertical == MZM_COLLISION_AIR) {
        s->pose = P_FALLING_INIT;
        return;
    }
    if (!nearly_ended(c)) return;
    if (d[2] == false) {
        s->pose = P_WALKING_ALERTED_INIT;
        return;
    }
    uint8_t nslr = shooting_nslr(c);
    uint8_t front = facing_right(s) ? NSLR_RIGHT : NSLR_LEFT;
    uint8_t back = facing_right(s) ? NSLR_LEFT : NSLR_RIGHT;
    if (nslr == front) {
        s->pose = P_CHARGING_LASER_INIT;
        s->work1 = d[0];
    } else if (nslr == back) {
        s->pose = P_TURNING_AROUND_ALERTED_INIT;
    } else {
        s->pose = P_WALKING_ALERTED_INIT;
    }
}

/* BlackSpacePirateDeath: without the alarm bookkeeping of the others. */
static void black_death(Ctx *c, bool play_sound) {
    MzmSprite *s = c->s;
    if (play_sound) ai_sound_onscreen(s, MZM_SOUND_SPACE_PIRATE_DYING);
    ai_sprite_death_full(c, DEATH_NORMAL, (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE)), s->x, true,
                         MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
}

void ai_black_space_pirate(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteGlobals *g = c->f->globals;
    uint8_t *d = g->draw_order;
    ai_projectile_collision_self(c);
    bool alerted = (s->status & MZM_SPRITE_STATUS_FACING_DOWN) != 0;
    if (s->pose < SPRITE_POSE_DESTROYED) {
        samus_detection(c);
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
            if (d[2] == true) g->alarm_timer = ALARM_ACTIVE;
            else if (!g->alarm_timer) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        } else if (g->alarm_timer && s->pose != SPRITE_POSE_UNINITIALIZED) {
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        }
        black_colliding_with_laser(c);
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: black_init(c); break;
    case 0x1:
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN) s->pose = P_IDLE;
        break;
    case P_WALKING_INIT: walking_init(s); break;
    case P_WALKING: walking(c); break;
    case P_IDLE: idle(c); break;
    case P_IDLE_AT_DOOR: idle_at_door(c); break;
    case P_WAITING_AT_DOOR: waiting_at_door(c); break;
    case P_SHOOTING_AFTER_WAITING_AT_DOOR: shooting_after_waiting_at_door(c); break;
    case P_0x2A: lookout_init(s); break;
    case P_0x2B: lookout(c); break;
    case P_FALLING_INIT:
        falling_init(s);
        /* fallthrough */
    case P_FALLING: falling(c); break;
    case P_0x1A:
        stop_init(s, MZM_PIRATE_STANDING_ALERTED);
        stopped(c);
        break;
    case P_0xE:
        stop_init(s, MZM_PIRATE_CROUCHED);
        stopped(c);
        break;
    case P_0xF: stopped(c); break;
    case P_0x1C:
        pause_init(s, MZM_PIRATE_STANDING_ALERTED);
        pause_then_turn(c);
        break;
    case P_0x16:
        pause_init(s, MZM_PIRATE_STANDING);
        pause_then_turn(c);
        break;
    case P_0x10:
        pause_init(s, MZM_PIRATE_CROUCHED);
        pause_then_turn(c);
        break;
    case P_0x11: pause_then_turn(c); break;
    case P_TURNING_AROUND_INIT:
        turning_around_init(s);
        /* fallthrough */
    case P_TURNING_AROUND_FIRST_PART: turning_around_first_part(c, P_TURNING_AROUND_SECOND_PART); break;
    case P_TURNING_AROUND_SECOND_PART: turning_around_second_part(c); break;
    case P_WALKING_ALERTED_INIT:
        walking_alerted_init(s);
        /* fallthrough */
    case P_WALKING_ALERTED: black_walking_alerted(c); break;
    case P_CHARGING_LASER_INIT:
        black_charging_laser_init(s);
        /* fallthrough */
    case P_CHARGING_LASER:
        s->adc++;
        charging_laser(c);
        break;
    case P_SHOOTING_LASER_INIT:
        shooting_laser_init(s);
        /* fallthrough */
    case P_SHOOTING_LASER:
        black_shooting(c);
        /* After shooting, a jump at the room above (once per hit). */
        if (s->pose != P_SHOOTING_LASER && s->pose != P_TURNING_AROUND_ALERTED_INIT &&
            ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE * 5 - QUARTER_BLOCK_SIZE - PIXEL_SIZE)), s->x) ==
                MZM_COLLISION_AIR &&
            ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE)), s->x) == MZM_COLLISION_AIR &&
            s->y_spawn != s->health) {
            s->work2 = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
            s->work1 = 4;
            jumping_init(c);
            jumping_of(c, true);
            s->y_spawn = s->health;
        }
        break;
    case P_JUMPING_INIT:
        jumping_init(c);
        /* fallthrough */
    case P_JUMPING:
    case P_WALL_JUMPING: jumping_of(c, true); break;
    case P_TURNING_AROUND_ALERTED_INIT:
        turning_around_alerted_init(s);
        /* fallthrough */
    case P_TURNING_AROUND_ALERTED_FIRST_PART:
        s->adc++;
        turning_around_first_part(c, P_TURNING_AROUND_ALERTED_SECOND_PART);
        break;
    case P_TURNING_AROUND_ALERTED_SECOND_PART: turning_around_second_part_alerted(c); break;
    case P_CLIMBING_UP_INIT:
        climbing_up_init(s);
        /* fallthrough */
    case P_CLIMBING_UP: climbing_up(c); break;
    case P_CLIMBING_DOWN_INIT:
        climbing_down_init(s);
        /* fallthrough */
    case P_CLIMBING_DOWN: climbing_down(c); break;
    case P_0x18:
        wall_pause_init(s);
        /* fallthrough */
    case P_0x19: wall_pause(c); break;
    case P_CLIMBING_CHARGING_LASER_INIT:
        s->pose = P_CLIMBING_CHARGING_LASER;
        set_anim(s, MZM_PIRATE_CLIMBING_CHARGING_LASER_FORWARD);
        /* fallthrough */
    case P_CLIMBING_CHARGING_LASER:
        if (nearly_ended(c)) s->pose = P_CLIMBING_SHOOTING_LASER_INIT;
        break;
    case P_CLIMBING_SHOOTING_LASER_INIT:
        s->pose = P_CLIMBING_SHOOTING_LASER;
        set_anim(s, MZM_PIRATE_CLIMBING_SHOOTING_FORWARD);
        s->work3++;
        /* fallthrough */
    case P_CLIMBING_SHOOTING_LASER: climbing_shooting_laser(c); break;
    case P_TURNING_AROUND_TO_WALL_JUMP_INIT:
        s->pose = P_TURNING_AROUND_TO_WALL_JUMP;
        set_anim(s, MZM_PIRATE_TURNING_TO_AIM_WHILE_CLIMBING);
        /* fallthrough */
    case P_TURNING_AROUND_TO_WALL_JUMP: turning_around_to_wall_jump(c); break;
    case P_DELAY_BEFORE_LAUNCHING_FROM_WALL: delay_before_launching(c); break;
    case P_LAUNCHING_FROM_WALL_INIT:
        s->pose = P_LAUNCHING_FROM_WALL;
        set_anim(s, MZM_PIRATE_LAUNCHING_FROM_WALL);
        /* fallthrough */
    case P_LAUNCHING_FROM_WALL: launching_from_wall(c); break;
    case P_STARTING_TO_CRAWL_INIT:
        s->pose = P_STARTING_TO_CRAWL;
        set_anim(s, MZM_PIRATE_STARTING_TO_CRAWL);
        /* fallthrough */
    case P_STARTING_TO_CRAWL: starting_to_crawl(c); break;
    case P_CRAWLING: crawling(c); break;
    case P_TURNING_AROUND_WHILE_CRAWLING_INIT:
        s->pose = P_TURNING_AROUND_WHILE_CRAWLING_FIRST_PART;
        set_anim(s, MZM_PIRATE_TURNING_WHILE_CRAWLING_FIRST_PART);
        break;
    case P_TURNING_AROUND_WHILE_CRAWLING_FIRST_PART:
        if (ended(c)) {
            s->pose = P_TURNING_AROUND_WHILE_CRAWLING_SECOND_PART;
            set_anim(s, MZM_PIRATE_TURNING_WHILE_CRAWLING_SECOND_PART);
            s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
            flip(s);
        }
        break;
    case P_TURNING_AROUND_WHILE_CRAWLING_SECOND_PART:
        if (ended(c)) {
            s->pose = P_CRAWLING;
            set_anim(s, MZM_PIRATE_CRAWLING);
        }
        break;
    case P_STANDING_UP_INIT:
        s->pose = P_STANDING_UP;
        set_anim(s, MZM_PIRATE_STANDING_UP);
        /* fallthrough */
    case P_STANDING_UP: standing_up(c); break;
    case P_CRAWLING_STOPPED_INIT:
        s->pose = P_CRAWLING_STOPPED;
        set_anim(s, MZM_PIRATE_CRAWLING_STOPPED);
        /* fallthrough */
    case P_CRAWLING_STOPPED:
        if (nearly_ended(c)) s->pose = P_TURNING_AROUND_WHILE_CRAWLING_INIT;
        break;
    case P_FALLING_WHILE_CRAWLING_INIT:
        s->pose = P_FALLING_WHILE_CRAWLING;
        set_anim(s, MZM_PIRATE_STANDING_UP);
        /* fallthrough */
    case P_FALLING_WHILE_CRAWLING: falling_while_crawling(c); break;
    case P_HIT_BY_LASER_INIT:
        hit_by_laser_init(s);
        /* fallthrough */
    case P_HIT_BY_LASER: {
        s->ignore_samus_timer = 1;
        s->adc--;
        uint8_t timer = --s->work0;
        if (timer & 3) break;
        if (timer & 4) {
            s->palette_row = (uint8_t)(15 - s->gfx_slot - 1);
        } else {
            s->palette_row = s->absolute_palette_row;
            if (timer == 0) black_death(c, false);
        }
        break;
    }
    default: black_death(c, true); break;
    }
    if (!alerted && (s->status & (MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                                  MZM_SPRITE_STATUS_FACING_DOWN |
                                  MZM_SPRITE_STATUS_IGNORE_PROJECTILES)) ==
                      (MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN | MZM_SPRITE_STATUS_FACING_DOWN))
        mzm_sfx_play_not_playing(MZM_SOUND_SPACE_PIRATE_GETTING_ALERTED);
}

/* SpacePirateLaserInit */
static void laser_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->samus_collision = SSC_SPACE_PIRATE_LASER;
    s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    s->hitbox_top = s->hitbox_left = -(QUARTER_BLOCK_SIZE - PIXEL_SIZE);
    s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE - PIXEL_SIZE;
    s->adc = 0;
    s->frame = 0;
    s->work0 = ONE_SECOND;
    s->work3 = 0;
    s->work2 = 10;
    s->pose = LASER_POSE_MOVING;
    /* drawOrder 3: drawn before the default sprites (not reproduced). */
    switch (s->room_slot) {
    case LASER_FORWARD: s->anim = MZM_PIRATE_LASER_FORWARD; break;
    case LASER_DIAGONALLY_UP: s->anim = MZM_PIRATE_LASER_DIAGONAL; break;
    case LASER_DIAGONALLY_DOWN:
        s->anim = MZM_PIRATE_LASER_DIAGONAL;
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
        break;
    case LASER_FROM_DOOR:
        /* An invisible shot through the door hatch. */
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        s->anim = MZM_PIRATE_LASER_FORWARD;
        s->samus_collision = SSC_NONE;
        s->work0 = 4;               /* CONVERT_SECONDS(1.f / 15) */
        s->ignore_samus_timer = (uint8_t)(s->work0 + 1);
        break;
    default:
        s->status = 0;
        break;
    }
    ai_sound_onscreen_once(s, MZM_SOUND_SPACE_PIRATE_FIRING_LASER);
}

/* SpacePirateLaserExplodingInit (the BG1 priority is not reproduced). */
static void laser_exploding_init(MzmSprite *s) {
    if (!(s->status & MZM_SPRITE_STATUS_ONSCREEN)) {
        s->status = 0;
        return;
    }
    set_anim(s, MZM_PIRATE_LASER_EXPLODING);
    s->samus_collision = SSC_NONE;
    s->pose = LASER_POSE_EXPLODING;
    s->ignore_samus_timer = 1;
    mzm_sfx_play_not_playing(MZM_SOUND_SPACE_PIRATE_LASER_EXPLODING);
}

/* SpacePirateLaserMove: CAA_BOMB_PISTOL at the laser's position (opening
 * hatches and breaking blocks), as a non-sprite actor. */
static void laser_move(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work2) s->work2--;
    int side = s->status & MZM_SPRITE_STATUS_X_FLIP ? 1 : -1;
    switch (s->room_slot) {
    case LASER_DIAGONALLY_UP:
        s->y = (uint16_t)(s->y - LASER_SPEED * 3 / 4);
        s->x = (uint16_t)(s->x + side * (LASER_SPEED * 3 / 4));
        break;
    case LASER_DIAGONALLY_DOWN:
        s->y = (uint16_t)(s->y + LASER_SPEED * 3 / 4);
        s->x = (uint16_t)(s->x + side * (LASER_SPEED * 3 / 4));
        break;
    default:
        s->x = (uint16_t)(s->x + side * LASER_SPEED);
        break;
    }
    /* The collision is that of the block before the action changes it. */
    uint8_t collision = ai_collision_at_actor(c, s->y, s->x, MZM_ACTOR_NON_SPRITE);
    const MzmSpriteHooks *hooks = c->f->hooks;
    if (hooks && hooks->affect) hooks->affect(hooks->context, s->y, s->x, MZM_DAMAGE_BOMB_PISTOL);
    if (collision != MZM_COLLISION_AIR) {
        if (s->room_slot == LASER_FROM_DOOR) s->status = 0;
        else laser_exploding_init(s);
    } else if (s->room_slot != LASER_FROM_DOOR) {
        if (!(s->status & MZM_SPRITE_STATUS_ONSCREEN) && !--s->work0) s->status = 0;
    } else if (!--s->work0) {
        s->status = 0;
    }
}

void ai_space_pirate_laser(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: laser_init(c); break;
    case LASER_POSE_EXPLODING_INIT: laser_exploding_init(s); break;
    case LASER_POSE_EXPLODING:
        s->ignore_samus_timer = 1;
        if (ended(c)) s->status = 0;
        break;
    default: laser_move(c); break;
    }
}
