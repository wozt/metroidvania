/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission room gadgets: the elevator pad (src/sprites_ai/elevator_pad.c),
 * water drops (water_drop.c), steam vents (steam.c), Gerons
 * (geron.c, geron_norfair.c) and the unknown item blocks
 * (unknown_item_block.c), the map stations (map_station.c) and the save
 * platforms (save_platform.c); comments name the pinned decompilation's
 * routines. */
#include "mzm_blocks.h"
#include "mzm_message_banner.h"
#include "mzm_screen_shake.h"
#include "mzm_sprite_ai.h"

enum {
    /* PrimarySprite */
    ELEVATOR_PAD = 80, WATER_DROP = 187, STEAM_LARGE = 168, STEAM_SMALL = 169,
    STEAM_LARGE_DIAGONAL_UP = 191, STEAM_SMALL_DIAGONAL_UP = 192,
    STEAM_LARGE_DIAGONAL_DOWN = 193, STEAM_SMALL_DIAGONAL_DOWN = 194,
    GERON_BRINSTAR_ROOM_15 = 140, GERON_BRINSTAR_ROOM_1C = 141, GERON_VARIA1 = 142,
    GERON_VARIA2 = 143, GERON_VARIA3 = 144, GERON_NORFAIR = 106,
    PLASMA_BEAM_BLOCK = 170, GRAVITY_SUIT_BLOCK = 171, SPACE_JUMP_BLOCK = 172,
    /* SecondarySprite */
    SSPRITE_UNKNOWN_ITEM_BLOCK_LIGHT = 56, SSPRITE_MAP_STATION_PART = 52, MAP_STATION = 58,
    /* MessageId */
    MESSAGE_BRINSTAR_MAP_ACQUIRED = 27, MESSAGE_KRAID_MAP_ACQUIRED = 28,
    MESSAGE_NORFAIR_MAP_ACQUIRED = 29, MESSAGE_RIDLEY_MAP_ACQUIRED = 30,
    MESSAGE_MOTHER_SHIP_MAP_ACQUIRED = 31, MESSAGE_ENEMY_LOCATION_ABNORMAL = 35,
    /* Area */
    AREA_BRINSTAR = 0, AREA_KRAID, AREA_NORFAIR, AREA_RIDLEY, AREA_TOURIAN, AREA_CRATERIA, AREA_CHOZODIA,
    PAUSE_SCREEN_MAP_DOWNLOAD = 5,
    SAVE_PLATFORM = 110, SSPRITE_SAVE_PLATFORM_PART = 13,
    MESSAGE_SAVE_PROMPT = 22, MESSAGE_SAVE_COMPLETE = 23,
    /* Event */
    EVENT_POWER_GRIP_OBTAINED = 16, EVENT_HIGH_JUMP_OBTAINED = 18, EVENT_SPACE_JUMP_OBTAINED = 22,
    EVENT_GRAVITY_SUIT_OBTAINED = 23, EVENT_PLASMA_BEAM_OBTAINED = 24,
    EVENT_PLANT_DESTROYED_LAVA = 47, EVENT_PLANT_DESTROYED_POST_VARIA = 48,
    EVENT_PLANT_DESTROYED_VARIA2 = 49, EVENT_PLANT_DESTROYED_VARIA3 = 50,
    EVENT_PLANT_DESTROYED_VARIA1 = 51,
    /* Sound */
    SOUND_UNKNOWN_ITEM_BLOCK_BREAKING = 315, SOUND_GERON_SHAKING = 619, SOUND_GERON_BREAKING = 620,
    SOUND_STEAM_LARGE = 632, SOUND_STEAM_SMALL = 633, SOUND_USING_MAP_STATION = 0x123,
    SOUND_MAP_STATION_RETRACTING = 0x124, SOUND_SAVE_PLATFORM_OPENING = 0x112,
    SOUND_SAVE_PLATFORM_CLOSING = 0x113, SOUND_SAVING = 0x114, SOUND_SAVE_PLATFORM_RETRACTING = 0x115,
    /* HazardType */
    HAZARD_TYPE_WATER = 1,
    /* Poses */
    ELEVATOR_PAD_POSE_IDLE = 0x9,
    WATER_DROP_POSE_CHECK_SPAWNING_ENDED = 0x9, WATER_DROP_POSE_SPLASHING_INIT = 0xE,
    WATER_DROP_POSE_SPLASHING = 0xF, WATER_DROP_POSE_SPAWNING = 0x11, WATER_DROP_POSE_FALLING = 0x1F,
    STEAM_POSE_IDLE = 0x9,
    GERON_POSE_IDLE = 0x9, GERON_POSE_GETTING_DESTROYED = 0x23,
    GERON_POSE_DELAY_BEFORE_DESTROYED = 0x24, GERON_POSE_DESTROYED = 0x25,
    UNKNOWN_ITEM_BLOCK_POSE_CHECK_ACTIVATE = 0x9, UNKNOWN_ITEM_BLOCK_POSE_WAIT_FOR_PROJECTILE = 0xB,
    UNKNOWN_ITEM_BLOCK_POSE_EXPLODING = 0x67,
    MAP_STATION_POSE_IDLE = 0x9, MAP_STATION_POSE_DO_NOTHING = 0xF, MAP_STATION_POSE_GRABBING_SAMUS = 0x23,
    MAP_STATION_POSE_DOWNLOADING = 0x25, MAP_STATION_POSE_DOWNLOADED = 0x27,
    MAP_STATION_POSE_WAIT_FOR_MESSAGE = 0x29, MAP_STATION_POSE_DELAY_BEFORE_RETRACTING = 0x2B,
    MAP_STATION_POSE_RETRACTING = 0x2D, MAP_STATION_POSE_RETRACTED = 0x2F,
    MAP_STATION_PART_POSE_IDLE = 0x9, MAP_STATION_PART_POSE_DOWNLOADING = 0x23,
    MAP_STATION_PART_POSE_DOWNLOADED = 0x25,
    MAP_STATION_PART_BACK_SCREEN = 0,
    SAVE_PLATFORM_POSE_IDLE = 0x9, SAVE_PLATFORM_POSE_OPENING = 0x23, SAVE_PLATFORM_POSE_OPENED = 0x25,
    SAVE_PLATFORM_POSE_CLOSING = 0x27, SAVE_PLATFORM_POSE_RELEASE_SAMUS = 0x28,
    SAVE_PLATFORM_POSE_AFTER_SAVE = 0x29, SAVE_PLATFORM_POSE_SAVE_PROMPT = 0x42,
    SAVE_PLATFORM_POSE_SAVING = 0x43, SAVE_PLATFORM_POSE_OPENED_OFF_INIT = 0x45,
    SAVE_PLATFORM_POSE_SPAWN_SAVE_DONE_MESSAGE = 0x47, SAVE_PLATFORM_POSE_WAIT_FOR_MESSAGE_OUT = 0x49,
    SAVE_PLATFORM_POSE_DELAY_BEFORE_RELEASING = 0x4B, SAVE_PLATFORM_POSE_OFF = 0x51,
    SAVE_PLATFORM_PART_TUBE = 0, SAVE_PLATFORM_PART_TUBE_SHADOW, SAVE_PLATFORM_PART_RAY,
    SAVE_PLATFORM_PART_TOP,
    SAVE_PLATFORM_PART_POSE_TUBE_SPAWNING = 0x9, SAVE_PLATFORM_PART_POSE_TUBE_CHECK_SHADOW_DISAPPEARED = 0x23,
    SAVE_PLATFORM_PART_POSE_TUBE_IDLE = 0x25, SAVE_PLATFORM_PART_POSE_TUBE_DESPAWNING = 0x27,
    SAVE_PLATFORM_PART_POSE_TUBE_SHADOW_IDLE = 0x29, SAVE_PLATFORM_PART_POSE_RAY_IDLE = 0x2B,
    SAVE_PLATFORM_PART_POSE_TOP_IDLE = 0x43, SAVE_PLATFORM_PART_POSE_TOP_EXTENDING_INIT = 0x44,
    SAVE_PLATFORM_PART_POSE_TOP_EXTENDING = 0x45, SAVE_PLATFORM_PART_POSE_TOP_RETRACTING_INIT = 0x46,
    SAVE_PLATFORM_PART_POSE_TOP_RETRACTING = 0x47,
    AREA_BANNER_POSE_SPAWNING_INIT = 0x8, AREA_BANNER_POSE_SPAWNING = 0x9, AREA_BANNER_POSE_STATIC = 0x23,
    AREA_BANNER_POSE_LEAVING = 0x25,
    /* LocationTextId */
    LT_MOTHERSHIP = 7, LT_SAVE_ROOM = 9, LT_MAP_ROOM = 11,
    MORPH_BALL_LAUNCHER_POSE_IDLE = 0x9, MORPH_BALL_LAUNCHER_POSE_DELAY_BEFORE_LAUNCHING = 0xB,
    MORPH_BALL_LAUNCHER_POSE_LAUNCHING = 0xC, MORPH_BALL_LAUNCHER_PART_POSE_ENERGY = 0xB,
    MORPH_BALL_LAUNCHER_PART_POSE_IDLE = 0x61, MORPH_BALL_LAUNCHER_PART_BACK = 0,
    MORPH_BALL_LAUNCHER_PART_ENERGY = 1,
};

static bool check_event(const Ctx *c, uint8_t event) { return ai_check_event(c, event); }

static void set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, event);
}

/* ClipdataProcess with gCurrentClipdataAffectingAction */
static void caa(const Ctx *c, uint16_t y, uint16_t x, uint8_t action) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->apply_caa) h->apply_caa(h->context, y, x, action);
}

static void kill(MzmSprite *s) { s->status = 0; }

/* ElevatorPad: follows Samus while she rides the elevator. */
static void elevator_pad(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->y = (uint16_t)(s->y - EIGHTH_BLOCK_SIZE);
        s->always_active = true;
        s->samus_collision = SSC_NONE;
        s->pose = ELEVATOR_PAD_POSE_IDLE;
        s->draw_top = PIXEL_SIZE / PIXEL_SIZE;
        s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = -PIXEL_SIZE;
        s->hitbox_bottom = PIXEL_SIZE;
        s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_right = PIXEL_SIZE;
        ai_set_anim(s, MZM_GADGET_ELEVATOR_PAD_IDLE);
    }
    if (c->f->samus_using_elevator) {
        s->y = c->f->samus_y;
        s->x = c->f->samus_x;
        if (s->anim == MZM_GADGET_ELEVATOR_PAD_IDLE) ai_set_anim(s, MZM_GADGET_ELEVATOR_PAD_MOVING);
    } else if (s->anim == MZM_GADGET_ELEVATOR_PAD_MOVING) {
        ai_set_anim(s, MZM_GADGET_ELEVATOR_PAD_IDLE);
    }
}

/* WaterDrop: falls from its spawn point, splashes on the ground or the
 * water, and drips again after a random delay. */
static void water_drop(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* WaterDropInit */
        s->hitbox_top = -PIXEL_SIZE;
        s->hitbox_bottom = PIXEL_SIZE;
        s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_right = PIXEL_SIZE;
        s->samus_collision = SSC_NONE;
        s->draw_order = 1;
        s->adc = 0;
        s->y = (uint16_t)(s->y - BLOCK_SIZE);
        s->draw_top = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->frame = 0;
        s->anim = MZM_GADGET_WATER_DROP_SPAWNING;
        s->y_spawn = s->y;
        s->x_spawn = s->x;
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
        s->pose = WATER_DROP_POSE_SPAWNING;
        s->work0 = (uint8_t)(c->rng * 8);
        break;
    case WATER_DROP_POSE_CHECK_SPAWNING_ENDED:
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_GADGET_WATER_DROP_FALLING);
            s->work1 = false;
            s->work3 = 0;
            s->pose = WATER_DROP_POSE_FALLING;
        }
        break;
    case WATER_DROP_POSE_FALLING: {
        uint16_t block = ai_check_vertical_slopes(c, s->y, s->x);
        if (ai_hazard_at(c, s->y, s->x) == HAZARD_TYPE_WATER) {
            if (c->world->effect_y != 0) {
                s->y = c->world->effect_y;
                s->work1 = true;
            } else {
                s->y = block;
            }
            s->pose = WATER_DROP_POSE_SPLASHING_INIT;
        } else if (c->vertical != MZM_COLLISION_AIR) {
            s->y = block;
            s->pose = WATER_DROP_POSE_SPLASHING_INIT;
        } else {
            uint8_t offset = s->work3;
            int16_t movement = offset < 16 ? c->tables->falling[offset] : SHORT_MAX;
            if (movement == SHORT_MAX) {
                s->y = (uint16_t)(s->y + c->tables->falling[offset > 0 ? offset - 1 : 0]);
            } else {
                s->work3++;
                s->y = (uint16_t)(s->y + movement);
            }
        }
        break;
    }
    case WATER_DROP_POSE_SPLASHING_INIT:
        ai_set_anim(s, MZM_GADGET_WATER_DROP_SPLASHING);
        s->pose = WATER_DROP_POSE_SPLASHING;
        /* fallthrough */
    case WATER_DROP_POSE_SPLASHING:
        if (s->work1) s->y = c->world->effect_y;
        if (ai_animation_ending(c, 1)) {
            s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
            s->pose = WATER_DROP_POSE_SPAWNING;
            /* CONVERT_SECONDS(1.f) + TWO_THIRD_SECOND + rng * 2/15 s */
            s->work0 = (uint8_t)(60 + 40 + c->rng * 8);
        }
        break;
    case WATER_DROP_POSE_SPAWNING:
        if (--s->work0 == 0) {
            ai_set_anim(s, MZM_GADGET_WATER_DROP_SPAWNING);
            s->pose = WATER_DROP_POSE_CHECK_SPAWNING_ENDED;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
            s->y = s->y_spawn;
            /* DIV_SHIFT(gSpriteRng + 1, 2) */
            uint16_t shift = (uint16_t)((c->rng + 1) >> 1);
            s->x = c->rng & 1 ? (uint16_t)(s->x_spawn + shift) : (uint16_t)(s->x_spawn - shift);
        }
        break;
    default:
        break;
    }
}

/* SpriteUtilGetCollisionAtPosition(...) & COLLISION_FLAGS_UNKNOWN_F0 */
static bool solid_at(const Ctx *c, int y, int x) {
    return (ai_collision_at(c, (uint16_t)y, (uint16_t)x) & MZM_COLLISION_FLAGS_F0) != 0;
}

/* The steam sound on the first frame of its animation, on screen. */
static void steam_sound(const MzmSprite *s) {
    if (s->frame == 0 && s->adc == 2 && s->status & MZM_SPRITE_STATUS_ONSCREEN)
        mzm_sfx_play_not_playing(s->work1 ? SOUND_STEAM_LARGE : SOUND_STEAM_SMALL);
}

/* Steam: a vent on the ground, the ceiling or a wall (the sprite dies in
 * the air); the animation starts at a random frame. */
static void steam(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->pose = STEAM_POSE_IDLE;
        bool large = s->sprite_id == STEAM_LARGE;
        s->work1 = large;
        if (solid_at(c, s->y, s->x)) {
            /* On the ground */
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
            s->draw_top = (uint8_t)((large ? BLOCK_SIZE * 4 + HALF_BLOCK_SIZE : BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE);
            s->draw_bottom = (uint8_t)((large ? HALF_BLOCK_SIZE : QUARTER_BLOCK_SIZE) / PIXEL_SIZE);
            s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        } else if (solid_at(c, s->y - (BLOCK_SIZE + PIXEL_SIZE), s->x)) {
            /* On the ceiling */
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
            s->status |= MZM_SPRITE_STATUS_Y_FLIP;
            s->y = (uint16_t)(s->y - BLOCK_SIZE);
            s->draw_top = (uint8_t)((large ? HALF_BLOCK_SIZE : QUARTER_BLOCK_SIZE) / PIXEL_SIZE);
            s->draw_bottom = (uint8_t)((large ? BLOCK_SIZE * 4 + HALF_BLOCK_SIZE : BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE);
            s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        } else {
            s->draw_top = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = (uint8_t)((large ? BLOCK_SIZE * 4 + HALF_BLOCK_SIZE : BLOCK_SIZE * 3) / PIXEL_SIZE);
            if (solid_at(c, s->y - HALF_BLOCK_SIZE, s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE))) {
                /* On a left wall */
                s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
                s->y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
                s->x = (uint16_t)(s->x - HALF_BLOCK_SIZE);
            } else if (solid_at(c, s->y - HALF_BLOCK_SIZE, s->x + HALF_BLOCK_SIZE)) {
                /* On a right wall */
                s->status |= MZM_SPRITE_STATUS_FACING_DOWN | MZM_SPRITE_STATUS_X_FLIP;
                s->y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
                s->x = (uint16_t)(s->x + HALF_BLOCK_SIZE);
            } else {
                kill(s);
                return;
            }
        }
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN)
            s->anim = large ? MZM_GADGET_STEAM_HORIZONTAL_LARGE : MZM_GADGET_STEAM_HORIZONTAL_SMALL;
        else
            s->anim = large ? MZM_GADGET_STEAM_VERTICAL_LARGE : MZM_GADGET_STEAM_VERTICAL_SMALL;
        s->adc = 0;
        s->frame = c->rng & 7;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->samus_collision = SSC_NONE;
        s->always_active = true;
    }
    steam_sound(s);
}

/* SteamDiagonal: faces away from its wall. */
static void steam_diagonal(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->pose = STEAM_POSE_IDLE;
        s->work1 = false;
        if (!(ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                              (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE))) & MZM_COLLISION_FLAGS_F0))
            s->status |= MZM_SPRITE_STATUS_X_FLIP;
        switch (s->sprite_id) {
        case STEAM_LARGE_DIAGONAL_UP:
            s->work1 = true;
            s->draw_top = BLOCK_SIZE * 4 / PIXEL_SIZE;
            s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE * 5 / PIXEL_SIZE;
            s->anim = MZM_GADGET_STEAM_DIAGONAL_LARGE;
            break;
        case STEAM_SMALL_DIAGONAL_UP:
            s->draw_top = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_bottom = QUARTER_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE * 3 / PIXEL_SIZE;
            s->anim = MZM_GADGET_STEAM_DIAGONAL_SMALL;
            break;
        case STEAM_LARGE_DIAGONAL_DOWN:
            s->work1 = true;
            s->status |= MZM_SPRITE_STATUS_Y_FLIP;
            s->draw_top = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_bottom = BLOCK_SIZE * 4 / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE * 5 / PIXEL_SIZE;
            s->anim = MZM_GADGET_STEAM_DIAGONAL_LARGE;
            break;
        case STEAM_SMALL_DIAGONAL_DOWN:
            s->status |= MZM_SPRITE_STATUS_Y_FLIP;
            s->draw_top = QUARTER_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_bottom = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE * 3 / PIXEL_SIZE;
            s->anim = MZM_GADGET_STEAM_DIAGONAL_SMALL;
            break;
        default:
            kill(s);
            return;
        }
        s->adc = 0;
        s->frame = c->rng & 7;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->samus_collision = SSC_NONE;
        s->always_active = true;
    }
    steam_sound(s);
}

/* The three blocks a Geron fills. */
static void geron_clipdata(const Ctx *c, uint8_t action) {
    const MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
    for (int i = 0; i < 3; ++i) caa(c, (uint16_t)(y - BLOCK_SIZE * i), s->x, action);
}

/* The Geron's shared setup (Geron, GeronNorfair). */
static void geron_setup(MzmSprite *s) {
    s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
    s->draw_bottom = 0;
    s->draw_horizontal = (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE * 3);
    s->hitbox_bottom = 0;
    s->hitbox_left = -THREE_QUARTER_BLOCK_SIZE;
    s->hitbox_right = THREE_QUARTER_BLOCK_SIZE;
    s->draw_order = 5;
    s->frame = 0;
    s->adc = 0;
    s->samus_collision = SSC_NONE;
}

/* Geron: a plant plug that blocks a passage; it shakes when hit and only
 * four Parasites (not ported) destroy it. */
static void geron(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: {
        bool destroyed = false;
        switch (s->sprite_id) {
        case GERON_BRINSTAR_ROOM_15: destroyed = check_event(c, EVENT_PLANT_DESTROYED_LAVA); break;
        case GERON_BRINSTAR_ROOM_1C:
            if (!check_event(c, EVENT_HIGH_JUMP_OBTAINED)) {
                kill(s);
                return;
            }
            destroyed = check_event(c, EVENT_PLANT_DESTROYED_POST_VARIA);
            break;
        case GERON_VARIA1: destroyed = check_event(c, EVENT_PLANT_DESTROYED_VARIA2); break;
        case GERON_VARIA2: destroyed = check_event(c, EVENT_PLANT_DESTROYED_VARIA3); break;
        case GERON_VARIA3: destroyed = check_event(c, EVENT_PLANT_DESTROYED_VARIA1); break;
        default:
            kill(s);
            return;
        }
        geron_setup(s);
        s->work3 = 0;
        if (destroyed) {
            s->pose = GERON_POSE_DESTROYED;
            s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            s->anim = MZM_GADGET_GERON_DESTROYED;
        } else {
            s->pose = GERON_POSE_IDLE;
            s->health = 1;
            s->anim = MZM_GADGET_GERON_IDLE;
            geron_clipdata(c, MZM_CAA_MAKE_NON_POWER_GRIP);
        }
        break;
    }
    case GERON_POSE_IDLE: {
        /* The Parasites whose point is inside it grab it (counted once:
         * their collision is cleared); four destroy it. */
        uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + EIGHTH_BLOCK_SIZE);
        uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
        for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
            MzmSprite *p = &c->f->sprites[i];
            if (!(p->status & MZM_SPRITE_STATUS_EXISTS) || p->samus_collision != SSC_PARASITE) continue;
            if (bottom > p->y && top < p->y && right > p->x && left < p->x) {
                p->work1 = s->ram_slot;
                p->pose = 0x4C;                 /* PARASITE_POSE_GERON_GRABBED_INIT */
                p->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
                p->samus_collision = SSC_NONE;
                s->work3++;
            }
        }
        if (s->work3 >= 4) {
            s->pose = GERON_POSE_GETTING_DESTROYED;
            ai_set_anim(s, MZM_GADGET_GERON_GETTING_DESTROYED);
            s->work0 = 200;             /* CONVERT_SECONDS(3.f) + ONE_THIRD_SECOND */
            mzm_sfx_play_not_playing(SOUND_GERON_BREAKING);
        } else if (s->isft & 0x7F) {
            if ((s->isft & 0x7F) == 2) {
                ai_set_anim(s, MZM_GADGET_GERON_SHAKING);
                mzm_sfx_play_not_playing(SOUND_GERON_SHAKING);
            }
        } else if (s->anim == MZM_GADGET_GERON_SHAKING && ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_GADGET_GERON_IDLE);
        }
        break;
    }
    case GERON_POSE_GETTING_DESTROYED:
        if (--s->work0 == 0) {
            s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            s->pose = GERON_POSE_DELAY_BEFORE_DESTROYED;
            geron_clipdata(c, MZM_CAA_REMOVE_SOLID);
            static const struct { uint8_t sprite, event; } events[] = {
                {GERON_BRINSTAR_ROOM_15, EVENT_PLANT_DESTROYED_LAVA},
                {GERON_BRINSTAR_ROOM_1C, EVENT_PLANT_DESTROYED_POST_VARIA},
                {GERON_VARIA1, EVENT_PLANT_DESTROYED_VARIA2},
                {GERON_VARIA2, EVENT_PLANT_DESTROYED_VARIA3},
                {GERON_VARIA3, EVENT_PLANT_DESTROYED_VARIA1}};
            for (size_t i = 0; i < sizeof events / sizeof events[0]; ++i)
                if (s->sprite_id == events[i].sprite) set_event(c, events[i].event);
        }
        break;
    case GERON_POSE_DELAY_BEFORE_DESTROYED:
        if (ai_animation_ending(c, 1)) {
            s->pose = GERON_POSE_DESTROYED;
            ai_set_anim(s, MZM_GADGET_GERON_DESTROYED);
        }
        break;
    default:
        break;
    }
}

/* GeronNorfair: gone once the Power Grip is obtained. */
static void geron_norfair(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->pose != SPRITE_POSE_UNINITIALIZED) return;
    if (check_event(c, EVENT_POWER_GRIP_OBTAINED)) {
        kill(s);
        return;
    }
    geron_setup(s);
    s->pose = GERON_POSE_IDLE;
    s->health = 1;
    s->anim = MZM_GADGET_GERON_NORFAIR_IDLE;
    geron_clipdata(c, MZM_CAA_MAKE_NON_POWER_GRIP);
}

/* UnknownItemBlockChangeClipdata: the 2x2 blocks. */
static void unknown_item_block_clipdata(const Ctx *c, uint8_t action) {
    const MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE), x = (uint16_t)(s->x - HALF_BLOCK_SIZE);
    for (int row = 0; row < 2; ++row)
        for (int column = 0; column < 2; ++column)
            caa(c, (uint16_t)(y + BLOCK_SIZE * row), (uint16_t)(x + BLOCK_SIZE * column), action);
}

/* UnknownItemBlock: a solid 2x2 block that shots only break once its
 * unknown item is obtained. */
static void unknown_item_block(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->draw_order = 1;
        s->hitbox_top = -(BLOCK_SIZE + PIXEL_SIZE);
        s->hitbox_bottom = BLOCK_SIZE + PIXEL_SIZE;
        s->hitbox_left = -(BLOCK_SIZE + PIXEL_SIZE);
        s->hitbox_right = BLOCK_SIZE + PIXEL_SIZE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        ai_set_anim(s, MZM_GADGET_UNKNOWN_ITEM_BLOCK_IDLE);
        s->samus_collision = SSC_NONE;
        s->health = 1;
        s->pose = UNKNOWN_ITEM_BLOCK_POSE_CHECK_ACTIVATE;
        s->y = (uint16_t)(s->y - BLOCK_SIZE);
        s->x = (uint16_t)(s->x + HALF_BLOCK_SIZE);
        unknown_item_block_clipdata(c, MZM_CAA_MAKE_SOLID_GRIPPABLE);
        /* fallthrough */
    case UNKNOWN_ITEM_BLOCK_POSE_CHECK_ACTIVATE: {
        bool activated = (s->sprite_id == PLASMA_BEAM_BLOCK && check_event(c, EVENT_PLASMA_BEAM_OBTAINED)) ||
                         (s->sprite_id == GRAVITY_SUIT_BLOCK && check_event(c, EVENT_GRAVITY_SUIT_OBTAINED)) ||
                         (s->sprite_id == SPACE_JUMP_BLOCK && check_event(c, EVENT_SPACE_JUMP_OBTAINED));
        if (activated) {
            ai_set_anim(s, MZM_GADGET_UNKNOWN_ITEM_BLOCK_ACTIVATED);
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            s->pose = UNKNOWN_ITEM_BLOCK_POSE_WAIT_FOR_PROJECTILE;
        }
        break;
    }
    case UNKNOWN_ITEM_BLOCK_POSE_WAIT_FOR_PROJECTILE:
        break;
    case UNKNOWN_ITEM_BLOCK_POSE_EXPLODING:
        if (ai_animation_ending(c, 1)) {
            kill(s);
            return;
        }
        if (s->frame == 1) {
            s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
            s->draw_bottom = BLOCK_SIZE * 3 / PIXEL_SIZE;
        } else if (s->frame == 8) {
            s->draw_top = BLOCK_SIZE * 6 / PIXEL_SIZE;
            s->draw_bottom = 0;
        } else if (s->frame == 7 && s->adc == 2) {
            int light = ai_spawn_secondary(c, SSPRITE_UNKNOWN_ITEM_BLOCK_LIGHT, 0, s->gfx_slot, s->y, s->x, 0);
            if (light >= 0) {
                MzmSprite *l = &c->f->sprites[light];
                l->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
                l->draw_order = 1;
                l->hitbox_top = l->hitbox_bottom = l->hitbox_left = l->hitbox_right = 0;
                l->draw_top = 0;
                l->draw_bottom = BLOCK_SIZE * 5 / PIXEL_SIZE;
                l->draw_horizontal = BLOCK_SIZE * 6 / PIXEL_SIZE;
                ai_set_anim(l, MZM_GADGET_UNKNOWN_ITEM_BLOCK_LIGHT);
                l->samus_collision = SSC_NONE;
                l->pose = 9;
            }
        }
        break;
    default:
        /* Hit by something: it explodes. */
        s->pose = UNKNOWN_ITEM_BLOCK_POSE_EXPLODING;
        ai_set_anim(s, MZM_GADGET_UNKNOWN_ITEM_BLOCK_EXPLODING);
        s->draw_horizontal = BLOCK_SIZE * 6 / PIXEL_SIZE;
        unknown_item_block_clipdata(c, MZM_CAA_REMOVE_SOLID);
        mzm_sfx_play_not_playing(SOUND_UNKNOWN_ITEM_BLOCK_BREAKING);
        break;
    }
}

/* UnknownItemBlockLight */
static void unknown_item_block_light(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    if (s->frame != 0) {
        s->draw_top = 0;
        s->draw_bottom = BLOCK_SIZE * 6 / PIXEL_SIZE;
    }
    if (ai_animation_ending(c, 1)) kill(s);
}

/* MapStationInit: retracted once the area's map is downloaded or during
 * the alarm, else idle with its two screens. */
static void map_station_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->y = (uint16_t)(s->y + BLOCK_SIZE * 2);
    s->draw_top = BLOCK_SIZE * 8 / PIXEL_SIZE;
    s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE * 2 - QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -(BLOCK_SIZE * 8);
    s->hitbox_left = -HALF_BLOCK_SIZE;
    s->hitbox_right = HALF_BLOCK_SIZE;
    s->samus_collision = SSC_NONE;
    s->draw_order = 4;
    s->health = 1;
    bool has_map = c->f->downloaded_maps >> c->f->area & 1;
    if (has_map || (c->f->globals && c->f->globals->alarm_timer != 0)) {
        s->hitbox_bottom = -BLOCK_SIZE;
        s->pose = MAP_STATION_POSE_DO_NOTHING;
        ai_set_anim(s, MZM_GADGET_MAP_STATION_INACTIVE);
        uint8_t count = c->kind->counts[MZM_GADGET_MAP_STATION_INACTIVE];
        s->frame = count ? (uint8_t)(count - 1) : 0;
    } else {
        s->hitbox_bottom = BLOCK_SIZE;
        s->pose = MAP_STATION_POSE_IDLE;
        ai_set_anim(s, MZM_GADGET_MAP_STATION_IDLE);
        for (uint8_t part = 0; part < 2; ++part)
            ai_spawn_secondary(c, SSPRITE_MAP_STATION_PART, part, s->gfx_slot, s->y, s->x, 0);
    }
}

/* MapStationIdle: Samus standing still on its platform is grabbed. */
static void map_station_idle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteSamusLink *link = c->f->samus_link;
    if (!link || c->f->samus_crouching_or_morphed) return;
    uint16_t sprite_y = (uint16_t)(s->y + BLOCK_SIZE * 2);
    uint16_t sprite_x = (uint16_t)(s->x + BLOCK_SIZE + HALF_BLOCK_SIZE);
    if (link->y != (uint16_t)(sprite_y - 1) || sprite_x - BLOCK_SIZE >= link->x ||
        sprite_x + BLOCK_SIZE <= link->x)
        return;
    ai_set_anim(s, MZM_GADGET_MAP_STATION_GRABBING_SAMUS);
    s->pose = MAP_STATION_POSE_GRABBING_SAMUS;
    link->x = sprite_x;
    link->moved = true;
    link->clear_invincibility = true;
    link->set_pose = link->facing_right ? MZM_POSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA
                                        : MZM_POSE_DOWNLOADING_MAP_DATA;
    mzm_sfx_play(SOUND_USING_MAP_STATION);
}

/* MapStationSpawnMessage's message */
static uint8_t map_station_message(uint8_t area) {
    switch (area) {
    case AREA_BRINSTAR: return MESSAGE_BRINSTAR_MAP_ACQUIRED;
    case AREA_KRAID: return MESSAGE_KRAID_MAP_ACQUIRED;
    case AREA_NORFAIR: return MESSAGE_NORFAIR_MAP_ACQUIRED;
    case AREA_RIDLEY: return MESSAGE_RIDLEY_MAP_ACQUIRED;
    case AREA_CHOZODIA: return MESSAGE_MOTHER_SHIP_MAP_ACQUIRED;
    default: return MESSAGE_ENEMY_LOCATION_ABNORMAL;
    }
}

/* MapStation: grabs Samus, downloads (the pause screen's map download),
 * shows the map message then retracts. */
static void map_station(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteSamusLink *link = c->f->samus_link;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        map_station_init(c);
        break;
    case MAP_STATION_POSE_IDLE:
        map_station_idle(c);
        break;
    case MAP_STATION_POSE_GRABBING_SAMUS:
        /* MapStationCheckSamusGrabbedAnimEnded */
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_GADGET_MAP_STATION_DOWNLOADING);
            s->pose = MAP_STATION_POSE_DOWNLOADING;
            s->work0 = 70;                      /* CONVERT_SECONDS(1.f + 1.f / 6) */
            if (link) link->set_timer = true, link->timer = 0;
        }
        break;
    case MAP_STATION_POSE_DOWNLOADING:
        /* MapStationDownloading */
        if (--s->work0 == 0) {
            ai_set_anim(s, MZM_GADGET_MAP_STATION_RETRACTING);
            s->pose = MAP_STATION_POSE_DOWNLOADED;
            s->work0 = 10;                      /* CONVERT_SECONDS(1.f / 6) */
        } else if (s->work0 == 10 && c->f->globals) {
            c->f->globals->pause_screen_flag = PAUSE_SCREEN_MAP_DOWNLOAD;
        }
        break;
    case MAP_STATION_POSE_DOWNLOADED:
        /* MapStationSpawnMessage */
        if (--s->work0 != 0) break;
        if (c->f->hooks && c->f->hooks->spawn_message)
            c->f->hooks->spawn_message(c->f->hooks->context, map_station_message(c->f->area));
        s->hitbox_bottom = -BLOCK_SIZE;
        s->pose = MAP_STATION_POSE_WAIT_FOR_MESSAGE;
        if (link) link->set_timer = true, link->timer = 1, link->reset_frame = true;
        mzm_sfx_fade(SOUND_USING_MAP_STATION, 10);
        break;
    case MAP_STATION_POSE_WAIT_FOR_MESSAGE:
        /* MapStationWaitForMessage */
        if (c->f->banner_pose == MZM_BANNER_POSE_REMOVAL_ANIMATION) {
            s->pose = MAP_STATION_POSE_DELAY_BEFORE_RETRACTING;
            s->work0 = 10;
        }
        break;
    case MAP_STATION_POSE_DELAY_BEFORE_RETRACTING:
        if (--s->work0 == 0) s->pose = MAP_STATION_POSE_RETRACTING;
        break;
    case MAP_STATION_POSE_RETRACTING:
        s->pose = MAP_STATION_POSE_RETRACTED;
        ai_set_anim(s, MZM_GADGET_MAP_STATION_INACTIVE);
        if (link) link->set_pose = MZM_POSE_STANDING;
        mzm_sfx_play(SOUND_MAP_STATION_RETRACTING);
        break;
    default:
        break;
    }
}

/* MapStationPart: the screens follow the station's poses and vanish once
 * the download ends. */
static void map_station_part(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    uint8_t main_pose = c->f->sprites ? c->f->sprites[s->ram_slot].pose : 0;
    bool back = s->room_slot == MAP_STATION_PART_BACK_SCREEN;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->always_active = true;
        s->samus_collision = SSC_NONE;
        s->draw_order = 3;
        s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->pose = MAP_STATION_PART_POSE_IDLE;
        ai_set_anim(s, back ? MZM_GADGET_MAP_STATION_BACK_SCREEN_IDLE : MZM_GADGET_MAP_STATION_FRONT_SCREEN_IDLE);
        break;
    case MAP_STATION_PART_POSE_IDLE:
        if (main_pose == MAP_STATION_POSE_DOWNLOADING) {
            s->pose = MAP_STATION_PART_POSE_DOWNLOADING;
            ai_set_anim(s, back ? MZM_GADGET_MAP_STATION_BACK_SCREEN_DOWNLOADING
                                : MZM_GADGET_MAP_STATION_FRONT_SCREEN_DOWNLOADING);
        }
        break;
    case MAP_STATION_PART_POSE_DOWNLOADING:
        if (main_pose == MAP_STATION_POSE_DOWNLOADED) {
            s->pose = MAP_STATION_PART_POSE_DOWNLOADED;
            ai_set_anim(s, back ? MZM_GADGET_MAP_STATION_BACK_SCREEN_DOWNLOADED
                                : MZM_GADGET_MAP_STATION_FRONT_SCREEN_DOWNLOADED);
        }
        break;
    case MAP_STATION_PART_POSE_DOWNLOADED:
        if (ai_animation_ending(c, 1)) kill(s);
        break;
    default:
        break;
    }
}

/* SavePlatformDetectSamus: Samus standing on the platform, within a block
 * of its centre. */
static bool save_platform_detect_samus(const Ctx *c) {
    const MzmSpriteSamusLink *link = c->f->samus_link;
    const MzmSprite *s = c->s;
    return link && link->y == (uint16_t)(s->y - (BLOCK_SIZE + 1)) && s->x - BLOCK_SIZE < link->x &&
           s->x + BLOCK_SIZE > link->x;
}

static void save_platform_spawn_part(Ctx *c, uint8_t part, uint16_t y) {
    ai_spawn_secondary(c, SSPRITE_SAVE_PLATFORM_PART, part, c->s->gfx_slot, y, c->s->x, 0);
}

static void set_disable_pause(const Ctx *c, bool disabled) {
    if (c->f->globals) c->f->globals->disable_pause = disabled;
}

static void spawn_message(const Ctx *c, uint8_t message) {
    if (c->f->hooks && c->f->hooks->spawn_message) c->f->hooks->spawn_message(c->f->hooks->context, message);
}

/* SavePlatformInit: off during the alarm (no escape is ported), releasing
 * Samus in a loaded file's room, else idle; the top part above. */
static void save_platform_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->always_active = true;
    s->y_spawn = 0;
    s->samus_collision = SSC_NONE;
    s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
    s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_top = -PIXEL_SIZE;
    s->hitbox_bottom = PIXEL_SIZE;
    s->hitbox_left = -PIXEL_SIZE;
    s->hitbox_right = PIXEL_SIZE;
    s->work2 = c->f->globals && c->f->globals->alarm_timer != 0;
    s->frame = s->adc = 0;
    s->work0 = 10;                              /* CONVERT_SECONDS(1.f / 6) */
    if (s->work2) {
        s->anim = MZM_GADGET_SAVE_PLATFORM_IDLE_OFF;
        s->pose = SAVE_PLATFORM_POSE_OFF;
    } else if (c->f->globals && c->f->globals->loading_file) {
        save_platform_spawn_part(c, SAVE_PLATFORM_PART_TUBE, s->y);
        s->anim = MZM_GADGET_SAVE_PLATFORM_OPENED_OFF;
        s->pose = SAVE_PLATFORM_POSE_RELEASE_SAMUS;
        s->y_spawn = 340;                       /* CONVERT_SECONDS(5.f) + TWO_THIRD_SECOND */
    } else {
        s->anim = MZM_GADGET_SAVE_PLATFORM_IDLE;
        s->pose = SAVE_PLATFORM_POSE_IDLE;
    }
    int top = ai_spawn_secondary(c, SSPRITE_SAVE_PLATFORM_PART, SAVE_PLATFORM_PART_TOP, s->gfx_slot,
                                 (uint16_t)(s->y - BLOCK_SIZE * 8), s->x, 0);
    s->work3 = (uint8_t)(top >= 0 ? top : 0xFF);
}

static MzmSprite *save_platform_top(const Ctx *c) {
    return c->f->sprites && c->s->work3 < c->f->sprite_count ? &c->f->sprites[c->s->work3] : NULL;
}

static void save_platform_closing(MzmSprite *s) {
    s->pose = SAVE_PLATFORM_POSE_CLOSING;
    ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_CLOSING);
    mzm_sfx_play(SOUND_SAVE_PLATFORM_CLOSING);
}

/* SavePlatform: opens under Samus, grabs her for the save prompt, saves
 * (the tube, the ray), shows the save complete message and releases her. */
static void save_platform(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteSamusLink *link = c->f->samus_link;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        save_platform_init(c);
        break;
    case SAVE_PLATFORM_POSE_IDLE:
        /* SavePlatformSamusDetection */
        if (save_platform_detect_samus(c) && !c->f->samus_crouching_or_morphed) {
            if (--s->work0 == 0) {
                s->pose = SAVE_PLATFORM_POSE_OPENING;
                ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_OPENING);
                mzm_sfx_play(SOUND_SAVE_PLATFORM_OPENING);
            }
        } else {
            s->work0 = 10;
        }
        break;
    case SAVE_PLATFORM_POSE_OPENING:
        if (ai_animation_ending(c, 1)) {
            s->pose = SAVE_PLATFORM_POSE_OPENED;
            ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_OPENED);
        }
        break;
    case SAVE_PLATFORM_POSE_OPENED:
        /* SavePlatformSecondSamusDetection */
        if (!save_platform_detect_samus(c)) {
            save_platform_closing(s);
        } else if (!c->f->samus_crouching_or_morphed && link) {
            s->pose = SAVE_PLATFORM_POSE_SAVE_PROMPT;
            s->work0 = 4;
            link->x = s->x;
            link->moved = true;
            link->clear_invincibility = true;
            link->set_pose = MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND;
            link->set_timer = true;
            link->timer = 1;
            set_disable_pause(c, true);
        }
        break;
    case SAVE_PLATFORM_POSE_CLOSING:
        if (ai_animation_ending(c, 1)) {
            s->pose = SAVE_PLATFORM_POSE_IDLE;
            ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_IDLE);
            s->work0 = 10;
        }
        break;
    case SAVE_PLATFORM_POSE_RELEASE_SAMUS:
        /* SavePlatformReleaseSamus */
        if (s->y_spawn != 0) {
            s->y_spawn--;
            break;
        }
        s->pose = SAVE_PLATFORM_POSE_AFTER_SAVE;
        if (link) link->set_pose = MZM_POSE_FACING_THE_FOREGROUND;
        set_disable_pause(c, false);
        break;
    case SAVE_PLATFORM_POSE_AFTER_SAVE:
        if (!save_platform_detect_samus(c)) save_platform_closing(s);
        break;
    case SAVE_PLATFORM_POSE_SAVE_PROMPT:
        /* SavePlatformSavePrompt */
        if (s->work0 != 0) {
            if (--s->work0 == 0) spawn_message(c, MESSAGE_SAVE_PROMPT);
            break;
        }
        if (c->f->banner_pose != MZM_BANNER_POSE_REMOVAL_ANIMATION) break;
        if (c->f->banner_yes) {
            ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_SAVING);
            s->pose = SAVE_PLATFORM_POSE_SAVING;
            save_platform_spawn_part(c, SAVE_PLATFORM_PART_TUBE, s->y);
            MzmSprite *top = save_platform_top(c);
            if (top) top->pose = SAVE_PLATFORM_PART_POSE_TOP_EXTENDING_INIT;
            if (link) link->set_timer = true, link->timer = 0;
            mzm_sfx_play(SOUND_SAVING);
        } else {
            s->pose = SAVE_PLATFORM_POSE_DELAY_BEFORE_RELEASING;
            s->work0 = 10;
        }
        break;
    case SAVE_PLATFORM_POSE_SAVING: {
        /* SavePlatformSaving: the platform and its top flash. */
        MzmSprite *top = save_platform_top(c);
        s->palette_row = s->frame & 1 ? 0 : 2;
        if (top) top->palette_row = s->palette_row;
        break;
    }
    case SAVE_PLATFORM_POSE_OPENED_OFF_INIT: {
        ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_OPENED_OFF);
        s->pose = SAVE_PLATFORM_POSE_SPAWN_SAVE_DONE_MESSAGE;
        s->work0 = 60;
        s->palette_row = 0;
        MzmSprite *top = save_platform_top(c);
        if (top) top->palette_row = 0;
        break;
    }
    case SAVE_PLATFORM_POSE_SPAWN_SAVE_DONE_MESSAGE:
        if (--s->work0 == 0) {
            s->pose = SAVE_PLATFORM_POSE_WAIT_FOR_MESSAGE_OUT;
            spawn_message(c, MESSAGE_SAVE_COMPLETE);
        }
        break;
    case SAVE_PLATFORM_POSE_WAIT_FOR_MESSAGE_OUT:
        if (c->f->banner_pose == MZM_BANNER_POSE_REMOVAL_ANIMATION) {
            s->pose = SAVE_PLATFORM_POSE_DELAY_BEFORE_RELEASING;
            s->work0 = 10;
        }
        break;
    case SAVE_PLATFORM_POSE_DELAY_BEFORE_RELEASING:
        if (--s->work0 == 0) s->pose = SAVE_PLATFORM_POSE_RELEASE_SAMUS;
        break;
    default:
        break;
    }
}

/* SavePlatformPart: the tube (with its shadow and the ray it spawns) and
 * the top. */
static void save_platform_part(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSprite *sprites = c->f->sprites;
    MzmSprite *main = sprites ? &sprites[s->ram_slot] : NULL;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* SavePlatformPartInit */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->always_active = true;
        s->samus_collision = SSC_NONE;
        s->hitbox_top = -PIXEL_SIZE;
        s->hitbox_bottom = PIXEL_SIZE;
        s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_right = PIXEL_SIZE;
        s->adc = s->frame = 0;
        switch (s->room_slot) {
        case SAVE_PLATFORM_PART_TUBE:
            s->draw_top = BLOCK_SIZE * 5 / PIXEL_SIZE;
            s->draw_bottom = 0;
            s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            if (main && main->pose == SAVE_PLATFORM_POSE_RELEASE_SAMUS) {
                s->anim = MZM_GADGET_SAVE_PLATFORM_TUBE_IDLE;
                s->y_spawn = 310;               /* CONVERT_SECONDS(5.f + 1.f / 6) */
                s->pose = SAVE_PLATFORM_PART_POSE_TUBE_IDLE;
            } else {
                s->anim = MZM_GADGET_SAVE_PLATFORM_TUBE_SPAWNING;
                s->pose = SAVE_PLATFORM_PART_POSE_TUBE_SPAWNING;
            }
            break;
        case SAVE_PLATFORM_PART_TUBE_SHADOW:
            s->draw_order = 12;
            s->anim = MZM_GADGET_SAVE_PLATFORM_TUBE_SHADOW;
            s->draw_top = BLOCK_SIZE * 5 / PIXEL_SIZE;
            s->draw_bottom = 0;
            s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->pose = SAVE_PLATFORM_PART_POSE_TUBE_SHADOW_IDLE;
            break;
        case SAVE_PLATFORM_PART_RAY:
            s->draw_order = 5;
            s->anim = MZM_GADGET_SAVE_PLATFORM_RAY;
            s->draw_top = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_bottom = 0;
            s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->pose = SAVE_PLATFORM_PART_POSE_RAY_IDLE;
            s->work0 = 96;                      /* CONVERT_SECONDS(1.6f) */
            break;
        case SAVE_PLATFORM_PART_TOP:
            s->draw_order = 3;
            s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
            s->draw_bottom = BLOCK_SIZE * 4 / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
            s->pose = SAVE_PLATFORM_PART_POSE_TOP_IDLE;
            s->anim = main && main->pose == SAVE_PLATFORM_POSE_RELEASE_SAMUS ? MZM_GADGET_SAVE_PLATFORM_TOP_EXTENDED
                                                                              : MZM_GADGET_SAVE_PLATFORM_TOP_IDLE;
            break;
        default:
            kill(s);
            break;
        }
        break;
    case SAVE_PLATFORM_PART_POSE_TUBE_SPAWNING:
        if (ai_animation_ending(c, 1)) {
            s->pose = SAVE_PLATFORM_PART_POSE_TUBE_CHECK_SHADOW_DISAPPEARED;
            ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_TUBE_IDLE);
            int shadow = ai_spawn_secondary(c, SSPRITE_SAVE_PLATFORM_PART, SAVE_PLATFORM_PART_TUBE_SHADOW,
                                            s->gfx_slot, s->y, s->x, 0);
            s->work1 = (uint8_t)(shadow >= 0 ? shadow : 0xFF);
        }
        break;
    case SAVE_PLATFORM_PART_POSE_TUBE_CHECK_SHADOW_DISAPPEARED:
        /* SavePlatformPartSpawnRay */
        if (!sprites || s->work1 >= c->f->sprite_count || sprites[s->work1].status == 0) {
            s->pose = SAVE_PLATFORM_PART_POSE_TUBE_IDLE;
            s->y_spawn = 120;                   /* CONVERT_SECONDS(2.f) */
            ai_spawn_secondary(c, SSPRITE_SAVE_PLATFORM_PART, SAVE_PLATFORM_PART_RAY, s->gfx_slot, s->y, s->x, 0);
        }
        break;
    case SAVE_PLATFORM_PART_POSE_TUBE_IDLE:
        if (--s->y_spawn == 0) {
            s->pose = SAVE_PLATFORM_PART_POSE_TUBE_DESPAWNING;
            ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_TUBE_DESPAWNING);
        }
        break;
    case SAVE_PLATFORM_PART_POSE_TUBE_DESPAWNING:
        if (ai_animation_ending(c, 1)) {
            kill(s);
            if (main && main->work3 < c->f->sprite_count)
                sprites[main->work3].pose = SAVE_PLATFORM_PART_POSE_TOP_RETRACTING_INIT;
        }
        break;
    case SAVE_PLATFORM_PART_POSE_TUBE_SHADOW_IDLE:
        if (ai_animation_ending(c, 1)) kill(s);
        break;
    case SAVE_PLATFORM_PART_POSE_RAY_IDLE:
        s->y = (uint16_t)(s->y - PIXEL_SIZE);
        if (--s->work0 == 0) {
            if (main) main->pose = SAVE_PLATFORM_POSE_OPENED_OFF_INIT;
            kill(s);
        }
        break;
    case SAVE_PLATFORM_PART_POSE_TOP_EXTENDING_INIT:
        ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_TOP_EXTENDING);
        s->pose = SAVE_PLATFORM_PART_POSE_TOP_EXTENDING;
        break;
    case SAVE_PLATFORM_PART_POSE_TOP_EXTENDING:
        if (ai_animation_ending(c, 1)) {
            s->pose = SAVE_PLATFORM_PART_POSE_TOP_IDLE;
            ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_TOP_EXTENDED);
        }
        break;
    case SAVE_PLATFORM_PART_POSE_TOP_RETRACTING_INIT:
        ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_TOP_RETRACTING);
        s->pose = SAVE_PLATFORM_PART_POSE_TOP_RETRACTING;
        if (c->f->samus_link) c->f->samus_link->set_timer = true, c->f->samus_link->timer = 1;
        mzm_sfx_play(SOUND_SAVE_PLATFORM_RETRACTING);
        break;
    case SAVE_PLATFORM_PART_POSE_TOP_RETRACTING:
        if (ai_animation_ending(c, 1)) {
            s->pose = SAVE_PLATFORM_PART_POSE_TOP_IDLE;
            ai_set_anim(s, MZM_GADGET_SAVE_PLATFORM_TOP_IDLE);
        }
        break;
    default:
        break;
    }
}

/* AreaBanner: the location's name, at the bottom of the screen for an area,
 * in its middle for the rooms (save room, map room...), scrolling in,
 * staying, then out. `room_slot` is the LocationTextId. */
static void area_banner(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* AreaBannerInit */
        s->samus_collision = SSC_NONE;
        s->always_active = true;
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = (SCREEN_SIZE_X_SUB_PIXEL / 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = PIXEL_SIZE;
        s->adc = s->frame = 0;
        s->pose = AREA_BANNER_POSE_SPAWNING_INIT;
        if (s->room_slot < LT_MOTHERSHIP) {
            s->anim = MZM_GADGET_AREA_BANNER_SPAWN_BOTTOM;
            s->work1 = true;
            s->y = 160 - 8;
            s->x = 120;
        } else if ((s->room_slot == LT_SAVE_ROOM || s->room_slot == LT_MAP_ROOM) && c->f->globals &&
                   c->f->globals->alarm_timer != 0) {
            kill(s);
        } else {
            s->anim = MZM_GADGET_AREA_BANNER_SPAWN_MIDDLE;
            s->work1 = false;
            s->y = 160 / 3 + 1;
            s->x = 120;
        }
        break;
    case AREA_BANNER_POSE_SPAWNING_INIT:
        /* AreaBannerSpawningInit */
        s->adc--;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->pose = AREA_BANNER_POSE_SPAWNING;
        break;
    case AREA_BANNER_POSE_SPAWNING:
        /* AreaBannerSpawning */
        if (ai_animation_ending(c, 1)) {
            s->adc = s->frame = 0;
            s->pose = AREA_BANNER_POSE_STATIC;
            if (s->work1) {
                s->anim = MZM_GADGET_AREA_BANNER_STATIC_BOTTOM;
                s->work0 = 120;                 /* CONVERT_SECONDS(2.f) */
            } else {
                s->anim = MZM_GADGET_AREA_BANNER_STATIC_MIDDLE;
                s->work0 = s->room_slot == LT_SAVE_ROOM || s->room_slot == LT_MAP_ROOM ? 50 : 90;
            }
        }
        break;
    case AREA_BANNER_POSE_STATIC:
        /* AreaBannerStatic */
        if (--s->work0 == 0) {
            s->adc = s->frame = 0;
            s->pose = AREA_BANNER_POSE_LEAVING;
            s->anim = s->work1 ? MZM_GADGET_AREA_BANNER_REMOVING_BOTTOM : MZM_GADGET_AREA_BANNER_REMOVING_MIDDLE;
        }
        break;
    case AREA_BANNER_POSE_LEAVING:
        if (ai_animation_ending(c, 1)) kill(s);
        break;
    default:
        break;
    }
}

/* MorphBallLauncherChangeCcaa: the five blocks around and under it. */
static void morph_ball_launcher_caa(Ctx *c, uint8_t action) {
    const MzmSprite *s = c->s;
    caa(c, s->y, (uint16_t)(s->x + BLOCK_SIZE), action);
    caa(c, (uint16_t)(s->y + BLOCK_SIZE), (uint16_t)(s->x + BLOCK_SIZE), action);
    caa(c, s->y, (uint16_t)(s->x - BLOCK_SIZE), action);
    caa(c, (uint16_t)(s->y + BLOCK_SIZE), (uint16_t)(s->x - BLOCK_SIZE), action);
    caa(c, (uint16_t)(s->y + BLOCK_SIZE), s->x, action);
}

/* MorphBallLauncher: a bomb set in it is held, then its explosion launches
 * Samus in morph ball (the bomb's side), with the energy around her. */
static void morph_ball_launcher(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* MorphBallLauncherInit (its BG priority, below BG1, is not
         * modelled) */
        s->y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->draw_top = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->samus_collision = SSC_NONE;
        ai_set_anim(s, MZM_GADGET_MORPH_BALL_LAUNCHER_IDLE);
        s->pose = MORPH_BALL_LAUNCHER_POSE_IDLE;
        s->draw_order = 2;
        ai_spawn_secondary(c, MZM_SSPRITE_MORPH_BALL_LAUNCHER_PART, MORPH_BALL_LAUNCHER_PART_BACK, s->gfx_slot,
                           s->y, s->x, 0);
        morph_ball_launcher_caa(c, MZM_CAA_MAKE_NON_POWER_GRIP);
        break;
    case MORPH_BALL_LAUNCHER_POSE_IDLE: {
        /* MorphBallLauncherDetectBomb */
        MzmWeapons *w = c->f->weapons;
        uint16_t y = (uint16_t)(s->y + HALF_BLOCK_SIZE), x = s->x;
        bool bomb = false;
        for (int i = 0; w && i < MZM_MAX_PROJECTILES; ++i) {
            MzmProjectile *p = &w->list[i];
            if (!p->active || p->type != MZM_PROJECTILE_BOMB || p->bomb_stage != MZM_BOMB_FIRST_SPIN) continue;
            uint16_t py = (uint16_t)p->y, px = (uint16_t)p->x;
            if (py < y && py > y - EIGHTH_BLOCK_SIZE && px < x + EIGHTH_BLOCK_SIZE && px > x - EIGHTH_BLOCK_SIZE) {
                p->bomb_stage = MZM_BOMB_PLACED_ON_LAUNCHER;
                bomb = true;
                break;
            }
        }
        if (bomb) {
            s->pose = MORPH_BALL_LAUNCHER_POSE_DELAY_BEFORE_LAUNCHING;
            s->work0 = 32;                      /* CONVERT_SECONDS(.5f) + 2 * DELTA_TIME */
        }
        break;
    }
    case MORPH_BALL_LAUNCHER_POSE_DELAY_BEFORE_LAUNCHING:
        if (--s->work0 == 0) {
            ai_set_anim(s, MZM_GADGET_MORPH_BALL_LAUNCHER_LAUNCHING);
            s->pose = MORPH_BALL_LAUNCHER_POSE_LAUNCHING;
            s->work0 = 60;
            s->work1 = false;
        }
        break;
    case MORPH_BALL_LAUNCHER_POSE_LAUNCHING: {
        /* MorphBallLauncherLaunchSamus */
        const MzmSpriteSamusLink *link = c->f->samus_link;
        if (!s->work1 && link && link->samus_pose == MZM_POSE_DELAY_BEFORE_BALLSPARKING) {
            ai_spawn_secondary(c, MZM_SSPRITE_MORPH_BALL_LAUNCHER_PART, MORPH_BALL_LAUNCHER_PART_ENERGY, s->gfx_slot,
                               (uint16_t)(link->y - QUARTER_BLOCK_SIZE), link->x, 0);
            s->work1 = true;
        }
        if (--s->work0 == 0) {
            ai_set_anim(s, MZM_GADGET_MORPH_BALL_LAUNCHER_IDLE);
            s->pose = MORPH_BALL_LAUNCHER_POSE_IDLE;
        }
        break;
    }
    default:
        break;
    }
}

/* MorphBallLauncherPart: its back, and the energy around Samus for a
 * second. */
static void morph_ball_launcher_part(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->samus_collision = SSC_NONE;
        s->frame = s->adc = 0;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        if (s->room_slot == MORPH_BALL_LAUNCHER_PART_BACK) {
            s->anim = MZM_GADGET_MORPH_BALL_LAUNCHER_BACK;
            s->draw_top = EIGHTH_BLOCK_SIZE;
            s->draw_bottom = 0;
            s->draw_horizontal = QUARTER_BLOCK_SIZE;
            s->draw_order = 12;
            s->pose = MORPH_BALL_LAUNCHER_PART_POSE_IDLE;
        } else if (s->room_slot == MORPH_BALL_LAUNCHER_PART_ENERGY) {
            s->anim = MZM_GADGET_MORPH_BALL_LAUNCHER_ENERGY;
            s->draw_top = s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_order = 1;
            s->pose = MORPH_BALL_LAUNCHER_PART_POSE_ENERGY;
            s->work0 = 60;
        } else {
            kill(s);
        }
        break;
    case MORPH_BALL_LAUNCHER_PART_POSE_ENERGY:
        if (--s->work0 == 0) kill(s);
        break;
    default:
        break;
    }
}

/* Zipline poses, health and animation states (sprites_ai/zipline.c). */
enum {
    ZIPLINE_POSE_IDLE = 0x9, ZIPLINE_HEALTH_NOT_MOVING = 1, ZIPLINE_HEALTH_MOVING = 2,
    ZIPLINE_RELEASED = 0, ZIPLINE_GRABBING = 1, ZIPLINE_GRABBED = 2, ZIPLINE_RELEASING = 3,
    ZIPLINE_BLOCK_X_RANGE = HALF_BLOCK_SIZE + PIXEL_SIZE * 3,
    ZIPLINE_BUTTON_POSE_BIND_ZIPLINE = 0x1, ZIPLINE_BUTTON_POSE_IDLE = 0x9,
    ZIPLINE_BUTTON_POSE_OFF = 0xF, ZIPLINE_BUTTON_POSE_ACTIVATED = 0x23,
    ZIPLINE_GENERATOR_POSE_DETECT_SAMUS = 0x9, ZIPLINE_GENERATOR_POSE_ACTIVATING = 0xB,
    ZIPLINE_GENERATOR_POSE_ACTIVATED = 0xC, ZIPLINE_GENERATOR_POSE_ALREADY_ACTIVATED = 0xF,
    ZIPLINE_GENERATOR_PART_POSE_MORPH_SYMBOL_ACTIVATING = 0xB,
    ZIPLINE_GENERATOR_PART_POSE_IDLE = 0x61,
    ZIPLINE_GENERATOR_PART_CONDUCTOR = 0, ZIPLINE_GENERATOR_PART_MORPH_SYMBOL = 1,
    ZIPLINE_GENERATOR_PART_ELECTRICITY = 2,
};

/* SpriteUtilCheckOnZipline */
static bool samus_on_zipline(const MzmSpriteSamusLink *link) {
    if (!link) return false;
    switch (link->samus_pose) {
    case MZM_POSE_ON_ZIPLINE: case MZM_POSE_SHOOTING_ON_ZIPLINE:
    case MZM_POSE_TURNING_ON_ZIPLINE: case MZM_POSE_MORPH_BALL_ON_ZIPLINE: return true;
    default: return false;
    }
}

/* SamusSetPose from a sprite: the request, and the pose the next sprites
 * see. */
static void link_set_pose(MzmSpriteSamusLink *link, uint8_t pose) {
    if (!link) return;
    link->set_pose = pose;
    link->samus_pose = pose;
}

static bool ziplines_on(const Ctx *c) { return ai_check_event(c, MZM_EVENT_ZIPLINES_ACTIVATED); }

/* ZiplineCheckColliding: the stop-enemy block ahead (on init, the side
 * without one sets the direction); the movement found. */
static uint8_t zipline_check_colliding(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE), x = s->x;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        uint8_t movement = ai_movement_at(c, y, (uint16_t)(x + ZIPLINE_BLOCK_X_RANGE));
        if (movement == MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            return movement;
        }
        movement = ai_movement_at(c, y, (uint16_t)(x - ZIPLINE_BLOCK_X_RANGE));
        if (movement == MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID)
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
        return movement;
    }
    return ai_movement_at(c, y, (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT
                                                ? x + ZIPLINE_BLOCK_X_RANGE
                                                : x - ZIPLINE_BLOCK_X_RANGE));
}

/* ZiplineMoving: a quarter block a frame (a pixel less with Samus) up to the
 * stop-enemy block, where it snaps to the block's middle and drops her;
 * true once it stopped. */
static bool zipline_moving(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteSamusLink *link = c->f->samus_link;
    bool grabbing = s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING;
    uint16_t velocity = grabbing ? QUARTER_BLOCK_SIZE - PIXEL_SIZE : QUARTER_BLOCK_SIZE;
    bool releasing = false;
    if (zipline_check_colliding(c) == MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID) {
        releasing = true;
        s->x = (uint16_t)((s->x & BLOCK_POSITION_FLAG) + HALF_BLOCK_SIZE);
        if (grabbing && link) {
            link->x = s->x;
            link->moved = true;
        }
    } else {
        s->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? s->x + velocity : s->x - velocity);
        if (grabbing && link) {
            link->x = (uint16_t)(s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? link->x + velocity
                                                                            : link->x - velocity);
            link->moved = true;
        }
    }
    if (releasing && grabbing) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
        link_set_pose(link, MZM_POSE_MID_AIR_REQUEST);
    }
    return releasing;
}

/* ZiplineUpdateOam: grabbing, grabbed, releasing and idle, lit once the
 * ziplines are activated. */
static void zipline_update_oam(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t base = ziplines_on(c) ? MZM_GADGET_ZIPLINE_ON_IDLE : MZM_GADGET_ZIPLINE_OFF_IDLE;
    uint8_t idle = base, grabbing = (uint8_t)(base + 1), grabbed = (uint8_t)(base + 2),
            releasing = (uint8_t)(base + 3);
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        switch (s->work1) {
        case ZIPLINE_RELEASED:
            ai_set_anim(s, grabbing);
            s->work1 = ZIPLINE_GRABBING;
            break;
        case ZIPLINE_GRABBING:
            if (ai_animation_ending(c, 1)) {
                ai_set_anim(s, grabbed);
                s->work1 = ZIPLINE_GRABBED;
                /* Only the lit zipline starts moving. */
                if (base == MZM_GADGET_ZIPLINE_ON_IDLE) s->health = ZIPLINE_HEALTH_MOVING;
            }
            break;
        case ZIPLINE_RELEASING:
            if (ai_animation_ending(c, 1)) {
                ai_set_anim(s, grabbed);
                s->work1 = ZIPLINE_GRABBED;
            }
            break;
        case ZIPLINE_GRABBED:
            if (s->anim != grabbed) ai_set_anim(s, grabbed);
            break;
        default:
            break;
        }
    } else {
        switch (s->work1) {
        case ZIPLINE_GRABBED:
            ai_set_anim(s, releasing);
            s->work1 = ZIPLINE_RELEASING;
            break;
        case ZIPLINE_GRABBING:
        case ZIPLINE_RELEASING:
            if (ai_animation_ending(c, 1)) {
                ai_set_anim(s, idle);
                s->work1 = ZIPLINE_RELEASED;
            }
            break;
        case ZIPLINE_RELEASED:
            if (s->anim != idle) ai_set_anim(s, idle);
            break;
        default:
            break;
        }
    }
}

/* Zipline: grabbed from below, it carries Samus to the other end once the
 * ziplines are activated, or comes back when its button is shot. */
static void zipline(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* ZiplineInit (its BG priority is not modelled) */
        s->hitbox_top = -BLOCK_SIZE;
        s->hitbox_bottom = QUARTER_BLOCK_SIZE;
        s->hitbox_left = -PIXEL_SIZE;
        s->hitbox_right = PIXEL_SIZE;
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->samus_collision = SSC_ZIPLINE;
        s->health = ZIPLINE_HEALTH_NOT_MOVING;
        s->solid_for_projectiles = true;
        s->draw_order = 3;
        s->work0 = s->work1 = 0;
        zipline_check_colliding(c);
        ai_set_anim(s, ziplines_on(c) ? MZM_GADGET_ZIPLINE_ON_IDLE : MZM_GADGET_ZIPLINE_OFF_IDLE);
        s->pose = ZIPLINE_POSE_IDLE;
        break;
    case ZIPLINE_POSE_IDLE:
        /* ZiplineUpdate */
        if (!samus_on_zipline(c->f->samus_link) && s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
        }
        zipline_update_oam(c);
        if (s->health == ZIPLINE_HEALTH_MOVING) {
            if (s->work0 % 16 == 0) mzm_sfx_play(MZM_SOUND_ZIPLING_MOVING);
            s->work0++;
            if (zipline_moving(c)) {
                s->health = ZIPLINE_HEALTH_NOT_MOVING;
                s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
                s->work0 = 0;
                mzm_sfx_fade(MZM_SOUND_ZIPLING_MOVING, 4);
            }
        }
        break;
    default:
        break;
    }
}

/* SpriteUtilFindPrimary */
static int find_primary(const Ctx *c, uint8_t sprite_id) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS && !n->secondary && n->sprite_id == sprite_id)
            return i;
    }
    return -1;
}

/* ZiplineButtonIdleInit */
static void zipline_button_idle(MzmSprite *s) {
    s->pose = ZIPLINE_BUTTON_POSE_IDLE;
    ai_set_anim(s, MZM_GADGET_ZIPLINE_BUTTON_ON_IDLE);
}

/* ZiplineButton: shot once the ziplines are activated, it calls the room's
 * zipline. */
static void zipline_button(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* ZiplineButtonInit (its BG priority is not modelled) */
        ai_set_anim(s, ziplines_on(c) ? MZM_GADGET_ZIPLINE_BUTTON_ON_IDLE
                                      : MZM_GADGET_ZIPLINE_BUTTON_OFF_IDLE);
        s->y = (uint16_t)(s->y - BLOCK_SIZE * 2);
        s->draw_top = 0;
        s->draw_bottom = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = 0;
        s->hitbox_bottom = BLOCK_SIZE + QUARTER_BLOCK_SIZE;
        s->hitbox_left = -(QUARTER_BLOCK_SIZE + PIXEL_SIZE);
        s->hitbox_right = QUARTER_BLOCK_SIZE + PIXEL_SIZE;
        s->samus_collision = SSC_NONE;
        s->health = ZIPLINE_HEALTH_NOT_MOVING;
        s->solid_for_projectiles = true;
        s->pose = ZIPLINE_BUTTON_POSE_BIND_ZIPLINE;
        s->draw_order = 3;
        break;
    case ZIPLINE_BUTTON_POSE_BIND_ZIPLINE: {
        int slot = find_primary(c, MZM_PSPRITE_ZIPLINE);
        if (slot < 0) {
            kill(s);
            break;
        }
        s->work1 = (uint8_t)slot;
        if (ziplines_on(c)) zipline_button_idle(s);
        else s->pose = ZIPLINE_BUTTON_POSE_OFF;
        break;
    }
    case ZIPLINE_BUTTON_POSE_OFF:
        if (ziplines_on(c)) zipline_button_idle(s);
        break;
    case ZIPLINE_BUTTON_POSE_IDLE: {
        MzmSprite *line = &c->f->sprites[s->work1];
        bool moving = false;
        if (s->isft & 0x7F) {
            line->health = ZIPLINE_HEALTH_MOVING;
            moving = true;
            mzm_sfx_play(MZM_SOUND_ZIPLING_CALLED);
        } else if (line->health == ZIPLINE_HEALTH_MOVING) {
            moving = true;
        }
        if (moving) {
            s->pose = ZIPLINE_BUTTON_POSE_ACTIVATED;
            ai_set_anim(s, MZM_GADGET_ZIPLINE_BUTTON_ACTIVE);
        }
        break;
    }
    case ZIPLINE_BUTTON_POSE_ACTIVATED:
        if (c->f->sprites[s->work1].health == ZIPLINE_HEALTH_NOT_MOVING) zipline_button_idle(s);
        break;
    default:
        break;
    }
}

/* ZiplineGeneratorChangeClipdata */
static void zipline_generator_caa(Ctx *c, uint8_t action) {
    const MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE), x = s->x;
    caa(c, y, x, action);
    caa(c, (uint16_t)(y - BLOCK_SIZE), x, action);
    caa(c, (uint16_t)(y - BLOCK_SIZE * 2), (uint16_t)(x + BLOCK_SIZE), action);
    caa(c, (uint16_t)(y - BLOCK_SIZE * 2), (uint16_t)(x - BLOCK_SIZE), action);
}

static void set_part_anim(Ctx *c, uint8_t slot, uint8_t anim) {
    ai_set_anim(&c->f->sprites[slot], anim);
}

/* ZiplineGenerator: Samus in morph ball on top of it activates the
 * ziplines (EVENT_ZIPLINES_ACTIVATED) in two seconds. */
static void zipline_generator(Ctx *c) {
    MzmSprite *s = c->s;
    MzmSpriteSamusLink *link = c->f->samus_link;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: {
        /* ZiplineGeneratorInit */
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->draw_top = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->samus_collision = SSC_NONE;
        s->frame = s->adc = 0;
        int slot = ai_spawn_secondary(c, MZM_SSPRITE_ZIPLINE_GENERATOR_PART,
                                      ZIPLINE_GENERATOR_PART_CONDUCTOR, s->gfx_slot, s->y, s->x, 0);
        if (slot < 0) {
            kill(s);
            break;
        }
        s->work1 = (uint8_t)slot;
        if (ziplines_on(c)) {
            ai_set_anim(s, MZM_GADGET_ZIPLINE_GENERATOR_ACTIVATED);
            s->pose = ZIPLINE_GENERATOR_POSE_ALREADY_ACTIVATED;
            set_part_anim(c, (uint8_t)slot, MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_ACTIVATED);
        } else {
            ai_set_anim(s, MZM_GADGET_ZIPLINE_GENERATOR_DEACTIVATED);
            s->pose = ZIPLINE_GENERATOR_POSE_DETECT_SAMUS;
            set_part_anim(c, (uint8_t)slot, MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_DEACTIVATED);
        }
        zipline_generator_caa(c, MZM_CAA_MAKE_SOLID_GRIPPABLE);
        break;
    }
    case ZIPLINE_GENERATOR_POSE_DETECT_SAMUS:
        /* ZiplineGeneratorDetectSamus */
        if (link && link->y == (uint16_t)(s->y - (BLOCK_SIZE * 2 + ONE_SUB_PIXEL)) &&
            s->x - BLOCK_SIZE < link->x && s->x + BLOCK_SIZE > link->x &&
            link->samus_pose == MZM_POSE_MORPH_BALL) {
            s->pose = ZIPLINE_GENERATOR_POSE_ACTIVATING;
            s->work0 = 120;                     /* CONVERT_SECONDS(2.f) */
            ai_set_anim(s, MZM_GADGET_ZIPLINE_GENERATOR_ACTIVATING);
            link_set_pose(link, MZM_POSE_ACTIVATING_ZIPLINES);
            set_part_anim(c, s->work1, MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_ACTIVATING);
            int slot = ai_spawn_secondary(c, MZM_SSPRITE_ZIPLINE_GENERATOR_PART,
                                          ZIPLINE_GENERATOR_PART_MORPH_SYMBOL, s->gfx_slot,
                                          (uint16_t)(link->y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                                          link->x, 0);
            if (slot >= 0) s->work2 = (uint8_t)slot;
            else kill(s);
            mzm_sfx_play(MZM_SOUND_ACTIVATING_ZIPLINES);
        }
        break;
    case ZIPLINE_GENERATOR_POSE_ACTIVATING:
        /* ZiplineGeneratorActivating */
        if (--s->work0 == 0) {
            ai_set_anim(s, MZM_GADGET_ZIPLINE_GENERATOR_ACTIVATED);
            s->pose = ZIPLINE_GENERATOR_POSE_ACTIVATED;
            link_set_pose(link, MZM_POSE_MORPH_BALL);
            set_part_anim(c, s->work1, MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_ACTIVATED);
            kill(&c->f->sprites[s->work3]);
            const MzmSpriteHooks *h = c->f->hooks;
            if (h && h->set_event) h->set_event(h->context, MZM_EVENT_ZIPLINES_ACTIVATED);
        } else if (s->work0 == 90) {             /* CONVERT_SECONDS(1.5f) */
            int slot = ai_spawn_secondary(c, MZM_SSPRITE_ZIPLINE_GENERATOR_PART,
                                          ZIPLINE_GENERATOR_PART_ELECTRICITY, s->gfx_slot,
                                          (uint16_t)(s->y - BLOCK_SIZE * 6), s->x, 0);
            if (slot >= 0) s->work3 = (uint8_t)slot;
            else kill(s);
        } else if (s->work0 == 16) {             /* CONVERT_SECONDS(.25f + 1.f / 60) */
            MzmSprite *symbol = &c->f->sprites[s->work2];
            ai_set_anim(symbol, MZM_GADGET_ZIPLINE_GENERATOR_MORPH_SYMBOL_ACTIVATED);
            symbol->pose = ZIPLINE_GENERATOR_PART_POSE_MORPH_SYMBOL_ACTIVATING;
            if (c->f->globals) c->f->globals->enable_animated_palette = true;
        }
        break;
    default:
        break;
    }
}

/* ZiplineGeneratorPart: the conductor behind Samus, the morph symbol over
 * her and the electricity rising to the ziplines. */
static void zipline_generator_part(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->samus_collision = SSC_NONE;
        s->frame = s->adc = 0;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        s->pose = ZIPLINE_GENERATOR_PART_POSE_IDLE;
        if (s->room_slot == ZIPLINE_GENERATOR_PART_CONDUCTOR) {
            s->draw_top = BLOCK_SIZE * 6 / PIXEL_SIZE;
            s->draw_bottom = 0;
            s->draw_horizontal = BLOCK_SIZE * 2 / PIXEL_SIZE;
            s->draw_order = 13;
        } else if (s->room_slot == ZIPLINE_GENERATOR_PART_MORPH_SYMBOL) {
            s->anim = MZM_GADGET_ZIPLINE_GENERATOR_MORPH_SYMBOL_ACTIVATING;
            s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
            s->always_active = true;
            s->draw_order = 3;
        } else if (s->room_slot == ZIPLINE_GENERATOR_PART_ELECTRICITY) {
            /* (its BG priority is not modelled) */
            s->anim = MZM_GADGET_ZIPLINE_GENERATOR_ELECTRICITY_ACTIVATING;
            s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
            s->always_active = true;
            s->draw_order = 2;
        } else {
            kill(s);
        }
        break;
    case ZIPLINE_GENERATOR_PART_POSE_MORPH_SYMBOL_ACTIVATING:
        if (ai_animation_ending(c, 1)) kill(s);
        break;
    default:
        break;
    }
}

/* Security gate poses (sprites_ai/security_gate.c). */
enum {
    SECURITY_GATE_POSE_IDLE = 0x9, SECURITY_GATE_POSE_CLOSING = 0x23,
    SECURITY_GATE_POSE_CLOSED = 0x25, SECURITY_GATE_POSE_OPENING = 0x27,
    TWO_THIRD_SECOND = 40,
};

/* SecurityGateChangeClipdata: the four blocks of the gate. */
static void security_gate_caa(Ctx *c, uint8_t action) {
    const MzmSprite *s = c->s;
    for (int b = 0; b < 4; ++b)
        caa(c, (uint16_t)(s->y - (BLOCK_SIZE * b + HALF_BLOCK_SIZE)), s->x, action);
}

/* SpriteCheckCollidingWithSamus */
static bool colliding_with_samus(const Ctx *c) {
    const MzmSprite *s = c->s;
    const MzmSpriteFrame *f = c->f;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    uint16_t samus_top = (uint16_t)(f->samus_y + f->samus_hitbox_top);
    uint16_t samus_bottom = (uint16_t)(f->samus_y + f->samus_hitbox_bottom);
    uint16_t samus_left = (uint16_t)(f->samus_x + f->samus_hitbox_left);
    uint16_t samus_right = (uint16_t)(f->samus_x + f->samus_hitbox_right);
    /* SpriteUtilCheckObjectsTouching */
    return bottom > samus_top && top < samus_bottom && right > samus_left && left < samus_right;
}

/* SecurityGateOpen */
static void security_gate_open(Ctx *c) {
    MzmSprite *s = c->s;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    ai_set_anim(s, MZM_GADGET_SECURITY_GATE_OPENING);
    s->pose = SECURITY_GATE_POSE_OPENING;
    security_gate_caa(c, MZM_CAA_REMOVE_SOLID);
    mzm_sfx_play_not_playing(MZM_SOUND_SECURITY_GATE_OPENING);
}

/* SecurityGateClose */
static void security_gate_close(MzmSprite *s) {
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    ai_set_anim(s, MZM_GADGET_SECURITY_GATE_CLOSING);
    s->pose = SECURITY_GATE_POSE_CLOSING;
    mzm_sfx_play_not_playing(MZM_SOUND_SECURITY_GATE_CLOSING);
}

/* SecurityGateDefaultOpen and SecurityGateDefaultClosed: Chozodia's gates,
 * open (or closed) while the alarm is off and the other way while it
 * rings; a closing gate only becomes solid once Samus is out of it. */
static void security_gate(Ctx *c) {
    MzmSprite *s = c->s;
    bool default_open = s->sprite_id == MZM_PSPRITE_SECURITY_GATE_DEFAULT_OPEN;
    bool alarm = c->f->globals && c->f->globals->alarm_timer != 0;
    /* Closed in this state: the alarm for the open gates, none for the
     * closed ones. */
    bool shut = default_open ? alarm : !alarm;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        if (shut) {
            ai_set_anim(s, MZM_GADGET_SECURITY_GATE_CLOSED);
            s->pose = SECURITY_GATE_POSE_CLOSED;
            s->work0 = true;
            security_gate_caa(c, MZM_CAA_MAKE_NON_POWER_GRIP);
        } else {
            s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            ai_set_anim(s, MZM_GADGET_SECURITY_GATE_OPENED);
            s->pose = SECURITY_GATE_POSE_IDLE;
        }
        s->hitbox_top = -(BLOCK_SIZE * 4);
        s->hitbox_bottom = 0;
        s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->draw_top = BLOCK_SIZE * 4 / PIXEL_SIZE;
        s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->frame = s->adc = 0;
        s->samus_collision = SSC_NONE;
        s->health = 1;
        s->draw_order = 3;
        s->immune_to_projectiles = true;
        break;
    case SECURITY_GATE_POSE_IDLE:
        if (shut) security_gate_close(s);
        break;
    case SECURITY_GATE_POSE_CLOSING:
        /* SecurityGateCheckClosingAnimEnded */
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_GADGET_SECURITY_GATE_CLOSED);
            s->pose = SECURITY_GATE_POSE_CLOSED;
            s->work0 = false;
            s->work3 = 0;
        }
        break;
    case SECURITY_GATE_POSE_CLOSED:
        if (!s->work0 && !colliding_with_samus(c)) {
            security_gate_caa(c, MZM_CAA_MAKE_NON_POWER_GRIP);
            s->work0++;
        }
        if (default_open) {
            /* SecurityGateDefaultOpenOpenAfterAlarm */
            if (!alarm) {
                s->work3++;
                if (s->work3 > TWO_THIRD_SECOND) security_gate_open(c);
            } else {
                s->work3 = 0;
            }
        } else if (alarm) {
            /* SecurityGateDefaultClosedCloseAfterAlarm */
            security_gate_open(c);
        }
        break;
    case SECURITY_GATE_POSE_OPENING:
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_GADGET_SECURITY_GATE_OPENED);
            s->pose = SECURITY_GATE_POSE_IDLE;
        }
        break;
    default: {
        /* SecurityGateDeath (unused: the gates are immune) */
        security_gate_caa(c, MZM_CAA_REMOVE_SOLID);
        uint16_t y = (uint16_t)(s->y - BLOCK_SIZE), x = s->x;
        ai_particle_set(c, y, x, MZM_PE_SPRITE_EXPLOSION_HUGE);
        ai_sprite_death(c, (uint16_t)(y - (BLOCK_SIZE + HALF_BLOCK_SIZE)), x, MZM_PE_SPRITE_EXPLOSION_BIG);
        break;
    }
    }
}

/* Piston poses (sprites_ai/piston.c). */
enum {
    PISTON_POSE_IDLE_INIT = 0x8, PISTON_POSE_CHECK_FOR_PROJECTILES = 0x9,
    PISTON_POSE_OPENING = 0x23, PISTON_POSE_OPENED = 0x25,
};

/* PistonSamusCollision / PistonCheckSamusIn: Samus within three quarters
 * of a block of the point. */
static bool samus_near_point(const Ctx *c, uint16_t y, uint16_t x) {
    const uint16_t range = BLOCK_SIZE - QUARTER_BLOCK_SIZE;
    uint16_t samus_y = c->f->samus_y, samus_x = c->f->samus_x;
    return y - range < samus_y && y + range > samus_y && x - range < samus_x && x + range > samus_x;
}

/* Piston: a column whose lower block (with the four body blocks above it)
 * blocks the way until a beam or bomb hits its head; it then extends one
 * block up, freeing the bottom block. */
static void piston(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* PistonInit (its BG priority is not modelled) */
        s->draw_top = (BLOCK_SIZE * 6 + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->samus_collision = SSC_NONE;
        s->health = 256;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        for (int b = 1; b <= 4; ++b)
            caa(c, (uint16_t)(s->y - (BLOCK_SIZE * b + HALF_BLOCK_SIZE)), s->x, MZM_CAA_MAKE_NON_POWER_GRIP);
        /* fallthrough */
    case PISTON_POSE_IDLE_INIT:
        ai_set_anim(s, MZM_GADGET_PISTON_IDLE);
        s->pose = PISTON_POSE_CHECK_FOR_PROJECTILES;
        s->hitbox_top = -(BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE);
        s->hitbox_bottom = -(BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE * 3);
        /* fallthrough */
    case PISTON_POSE_CHECK_FOR_PROJECTILES:
        /* PistonCheckProjectile: a damaging hit (ISFT_DAMAGED, one frame
         * run down) opens it; the bottom block turns solid once Samus is
         * out of it. */
        if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            if ((s->isft & 0x7F) == 16) {   /* CONVERT_SECONDS(.25f + 1.f / 60) */
                s->isft &= 0x80;
                s->health = 256;
                ai_set_anim(s, MZM_GADGET_PISTON_OPENING);
                s->pose = PISTON_POSE_OPENING;
                s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
                mzm_sfx_play(MZM_SOUND_PISTON_EXTENDING);
            }
        } else if (!samus_near_point(c, s->y, s->x)) {
            caa(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x, MZM_CAA_MAKE_NON_POWER_GRIP);
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        }
        break;
    case PISTON_POSE_OPENING:
        /* PistonOpen */
        if (s->frame == 2 && s->adc == 1) {
            caa(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x, MZM_CAA_REMOVE_SOLID);
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        }
        if (ai_animation_ending(c, 1)) {
            ai_set_anim(s, MZM_GADGET_PISTON_OPENED);
            s->pose = PISTON_POSE_OPENED;
            s->hitbox_top = -(BLOCK_SIZE * 2 + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE);
            s->hitbox_bottom = -(BLOCK_SIZE * 2 + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE);
        }
        break;
    case PISTON_POSE_OPENED:
        /* PistonOpened: the top block turns solid once Samus is out of it. */
        if (!(s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) &&
            !samus_near_point(c, (uint16_t)(s->y - BLOCK_SIZE * 5), s->x)) {
            caa(c, (uint16_t)(s->y - (BLOCK_SIZE * 5 + HALF_BLOCK_SIZE)), s->x, MZM_CAA_MAKE_NON_POWER_GRIP);
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        }
        break;
    default:
        break;
    }
}

/* Glass tube poses (sprites_ai/glass_tube.c). */
enum {
    GLASS_TUBE_POSE_POWER_BOMB_COLLISION = 0x9, GLASS_TUBE_POSE_IDLE = 0xF,
    GLASS_TUBE_POSE_DELAY_BEFORE_CRACKING = 0x23, GLASS_TUBE_POSE_CRACKING = 0x25,
    GLASS_TUBE_POSE_BREAKING = 0x27,
};

/* GlassTubeChangeCcaa: the tube's top and bottom rows, eleven blocks each,
 * removed. */
static void glass_tube_caa(Ctx *c) {
    const MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE), x = s->x;
    for (int row = 0; row < 2; ++row) {
        uint16_t ry = row ? (uint16_t)(y + BLOCK_SIZE * 3) : (uint16_t)(y - BLOCK_SIZE);
        caa(c, ry, x, MZM_CAA_REMOVE_SOLID);
        for (int b = 1; b <= 5; ++b) caa(c, ry, (uint16_t)(x - BLOCK_SIZE * b), MZM_CAA_REMOVE_SOLID);
        for (int b = 1; b <= 5; ++b) caa(c, ry, (uint16_t)(x + BLOCK_SIZE * b), MZM_CAA_REMOVE_SOLID);
    }
}

/* GlassTube: Maridia's tube in Chozodia, broken by a power bomb once the
 * Fully Powered Suit is obtained (EVENT_GLASS_TUBE_BROKEN). */
static void glass_tube(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* GlassTubeInit */
        s->draw_top = s->draw_bottom = BLOCK_SIZE * 3 / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE * 7 / PIXEL_SIZE;
        s->hitbox_top = -(BLOCK_SIZE * 2);
        s->hitbox_bottom = BLOCK_SIZE * 2;
        s->hitbox_left = -(BLOCK_SIZE * 4);
        s->hitbox_right = BLOCK_SIZE * 4;
        s->frame = s->adc = 0;
        s->samus_collision = SSC_NONE;
        if (ai_check_event(c, MZM_EVENT_GLASS_TUBE_BROKEN)) {
            ai_set_anim(s, MZM_GADGET_GLASS_TUBE_BROKEN);
            s->pose = GLASS_TUBE_POSE_IDLE;
            glass_tube_caa(c);
        } else {
            ai_set_anim(s, MZM_GADGET_GLASS_TUBE_INTACT);
            s->pose = GLASS_TUBE_POSE_POWER_BOMB_COLLISION;
        }
        if (!ai_check_event(c, MZM_EVENT_FULLY_POWERED_SUIT_OBTAINED)) mzm_sfx_play(MZM_SOUND_RAIN_HITTING_GROUND);
        break;
    case GLASS_TUBE_POSE_POWER_BOMB_COLLISION: {
        /* GlassTubeCheckPowerBombCollision */
        if (!ai_check_event(c, MZM_EVENT_FULLY_POWERED_SUIT_OBTAINED) || !c->f->weapons || !c->f->equipment)
            break;
        const MzmPowerBomb *pb = &c->f->weapons->power_bomb;
        if (pb->state == MZM_PB_NONE || c->f->equipment->max_power_bombs == 0) break;
        uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
        uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
        uint16_t bomb_top = (uint16_t)(pb->y + pb->hitbox_top), bomb_bottom = (uint16_t)(pb->y + pb->hitbox_bottom);
        uint16_t bomb_left = (uint16_t)(pb->x + pb->hitbox_left), bomb_right = (uint16_t)(pb->x + pb->hitbox_right);
        if (bottom > bomb_top && top < bomb_bottom && right > bomb_left && left < bomb_right) {
            s->pose = GLASS_TUBE_POSE_DELAY_BEFORE_CRACKING;
            s->work0 = 120;                     /* CONVERT_SECONDS(2.f) */
            const MzmSpriteHooks *h = c->f->hooks;
            if (h && h->set_event) h->set_event(h->context, MZM_EVENT_GLASS_TUBE_BROKEN);
        }
        break;
    }
    case GLASS_TUBE_POSE_DELAY_BEFORE_CRACKING:
        if (--s->work0 == 0) {
            s->pose = GLASS_TUBE_POSE_CRACKING;
            ai_set_anim(s, MZM_GADGET_GLASS_TUBE_CRACKING);
            mzm_sfx_play(MZM_SOUND_GLASS_TUBE_BREAKING);
        }
        break;
    case GLASS_TUBE_POSE_CRACKING:
        /* GlassTubeCheckCrackingAnimEnded: the tube opens with dust at its
         * four ends and a half-second shake both ways. */
        if (ai_animation_ending(c, 1)) {
            s->pose = GLASS_TUBE_POSE_BREAKING;
            ai_set_anim(s, MZM_GADGET_GLASS_TUBE_BREAKING);
            glass_tube_caa(c);
            uint16_t y = s->y, x = s->x;
            ai_particle_set(c, (uint16_t)(y - (HALF_BLOCK_SIZE - PIXEL_SIZE / 2)),
                            (uint16_t)(x - (5 * BLOCK_SIZE - QUARTER_BLOCK_SIZE - PIXEL_SIZE)), MZM_PE_MEDIUM_DUST);
            ai_particle_set(c, (uint16_t)(y + 2 * BLOCK_SIZE + HALF_BLOCK_SIZE), (uint16_t)(x - 5 * BLOCK_SIZE),
                            MZM_PE_TWO_MEDIUM_DUST);
            ai_particle_set(c, (uint16_t)(y - (HALF_BLOCK_SIZE - PIXEL_SIZE / 2)),
                            (uint16_t)(x + 5 * BLOCK_SIZE - QUARTER_BLOCK_SIZE - PIXEL_SIZE), MZM_PE_MEDIUM_DUST);
            ai_particle_set(c, (uint16_t)(y + 2 * BLOCK_SIZE + HALF_BLOCK_SIZE), (uint16_t)(x + 5 * BLOCK_SIZE),
                            MZM_PE_TWO_MEDIUM_DUST);
            mzm_screen_shake_start_vertical(30, 0x81);     /* CONVERT_SECONDS(.5f) */
            mzm_screen_shake_start_horizontal(30, 0x81);
        }
        break;
    case GLASS_TUBE_POSE_BREAKING:
        if (ai_animation_ending(c, 1)) {
            s->pose = GLASS_TUBE_POSE_IDLE;
            ai_set_anim(s, MZM_GADGET_GLASS_TUBE_BROKEN);
        }
        break;
    default:
        break;
    }
}

/* Rising Chozo pillar poses and platform parts
 * (sprites_ai/rising_chozo_pillar.c). */
enum {
    RISING_CHOZO_PILLAR_POSE_WAIT_FOR_POWER_GRIP = 0x8, RISING_CHOZO_PILLAR_POSE_EXTENDING = 0x9,
    RISING_CHOZO_PILLAR_POSE_SPAWN_3_PLATFORMS = 0x22, RISING_CHOZO_PILLAR_POSE_SPAWN_2_PLATFORMS = 0x23,
    RISING_CHOZO_PILLAR_POSE_SPAWN_1_PLATFORMS = 0x24, RISING_CHOZO_PILLAR_POSE_EXTENDED = 0x25,
    RISING_CHOZO_PILLAR_POSE_KILL = 0x26,
    CHOZO_PILLAR_PLATFORM_POSE_SPAWNING = 0x8, CHOZO_PILLAR_PLATFORM_POSE_IDLE = 0x9,
    CHOZO_PILLAR_PLATFORM_NO_SHADOW = 0, CHOZO_PILLAR_PLATFORM_SHADOW = 1,
};

/* RisingChozoPillarRandomSpriteDebris */
static void rising_pillar_debris(Ctx *c, uint16_t y, uint16_t x, uint8_t rng) {
    switch (rng) {
    case 1:
        ai_debris_init(c, 0x11, (uint16_t)(y - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), x);
        ai_debris_init(c, 0x12, (uint16_t)(y - QUARTER_BLOCK_SIZE), (uint16_t)(x + BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2));
        break;
    case 3:
        ai_debris_init(c, 0x13, (uint16_t)(y - (QUARTER_BLOCK_SIZE + PIXEL_SIZE)),
                       (uint16_t)(x - (BLOCK_SIZE + HALF_BLOCK_SIZE - PIXEL_SIZE)));
        ai_debris_init(c, 4, (uint16_t)(y - (EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2)), (uint16_t)(x + HALF_BLOCK_SIZE - PIXEL_SIZE / 2));
        break;
    case 7:
        ai_debris_init(c, 0x11, (uint16_t)(y - EIGHTH_BLOCK_SIZE), (uint16_t)(x - (THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE / 2)));
        ai_debris_init(c, 0x12, (uint16_t)(y - (THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE)),
                       (uint16_t)(x + BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE));
        break;
    case 12:
        ai_debris_init(c, 0x13, (uint16_t)(y - HALF_BLOCK_SIZE), (uint16_t)(x + HALF_BLOCK_SIZE - PIXEL_SIZE / 2));
        ai_debris_init(c, 4, (uint16_t)(y - PIXEL_SIZE),
                       (uint16_t)(x - (BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE / 2)));
        break;
    default:
        break;
    }
}

/* RisingChozoPillarRandomParticles */
static void rising_pillar_particles(Ctx *c, uint16_t y, uint16_t x, uint8_t rng) {
    switch (rng) {
    case 1: ai_particle_set(c, y, (uint16_t)(x - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), MZM_PE_TWO_MEDIUM_DUST); break;
    case 16: ai_particle_set(c, y, (uint16_t)(x + BLOCK_SIZE - PIXEL_SIZE), MZM_PE_SECOND_MEDIUM_DUST); break;
    case 33: ai_particle_set(c, y, (uint16_t)(x - (BLOCK_SIZE - PIXEL_SIZE)), MZM_PE_SECOND_MEDIUM_DUST); break;
    case 50: ai_particle_set(c, y, (uint16_t)(x + QUARTER_BLOCK_SIZE + PIXEL_SIZE), MZM_PE_MEDIUM_DUST); break;
    case 66: ai_particle_set(c, y, (uint16_t)(x - (BLOCK_SIZE + QUARTER_BLOCK_SIZE)), MZM_PE_TWO_MEDIUM_DUST); break;
    case 84:
        ai_particle_set(c, y, (uint16_t)(x + BLOCK_SIZE + QUARTER_BLOCK_SIZE), MZM_PE_SECOND_MEDIUM_DUST);
        ai_particle_set(c, y, x, MZM_PE_SECOND_TWO_MEDIUM_DUST);
        break;
    case 100: ai_particle_set(c, y, (uint16_t)(x - (QUARTER_BLOCK_SIZE + PIXEL_SIZE)), MZM_PE_SECOND_TWO_MEDIUM_DUST); break;
    case 110: ai_particle_set(c, y, (uint16_t)(x + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE), MZM_PE_MEDIUM_DUST); break;
    default: break;
    }
}

/* One platform of the pillar: its two grippable blocks and its sprite. */
static void rising_pillar_platform(Ctx *c, uint16_t y, uint16_t x, uint8_t part) {
    caa(c, y, x, MZM_CAA_MAKE_SOLID_GRIPPABLE);
    caa(c, y, (uint16_t)(x + BLOCK_SIZE), MZM_CAA_MAKE_SOLID_GRIPPABLE);
    ai_spawn_secondary(c, MZM_SSPRITE_CHOZO_PILLAR_PLATFORM, part, c->s->gfx_slot, y,
                       (uint16_t)(x + HALF_BLOCK_SIZE), 0);
}

/* RisingChozoPillarSpawnThreePlatforms / TwoPlatforms / OnePlatform */
static void rising_pillar_spawn(Ctx *c, uint16_t y, uint16_t x, int count) {
    if (count == 3) {
        for (int b = 3; b <= 19; b += 8)
            rising_pillar_platform(c, (uint16_t)(y - BLOCK_SIZE * b), (uint16_t)(x + BLOCK_SIZE * 3),
                                   CHOZO_PILLAR_PLATFORM_SHADOW);
    } else if (count == 2) {
        rising_pillar_platform(c, (uint16_t)(y - BLOCK_SIZE * 7), x, CHOZO_PILLAR_PLATFORM_NO_SHADOW);
        rising_pillar_platform(c, (uint16_t)(y - BLOCK_SIZE * 24), x, CHOZO_PILLAR_PLATFORM_NO_SHADOW);
    } else {
        rising_pillar_platform(c, (uint16_t)(y - BLOCK_SIZE * 15), (uint16_t)(x + BLOCK_SIZE * 6),
                               CHOZO_PILLAR_PLATFORM_NO_SHADOW);
    }
}

/* RisingChozoPillar: once the Power Grip is obtained, the pillar (BG2 in
 * its room) rises for 704 frames with dust, debris and shakes, then its
 * six platforms appear (EVENT_CHOZO_PILLAR_FULLY_EXTENDED); later visits
 * place them at once. Never drawn. */
static void rising_chozo_pillar(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE), x = s->x;
    MzmSpriteGlobals *globals = c->f->globals;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        if (check_event(c, MZM_EVENT_CHOZO_PILLAR_FULLY_EXTENDED)) {
            rising_pillar_spawn(c, y, x, 3);
            rising_pillar_spawn(c, y, x, 2);
            rising_pillar_spawn(c, y, x, 1);
            kill(s);
            break;
        }
        s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_NONE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = 1;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        /* (natively sEnemyDropOam_LargeEnergy, never drawn) */
        ai_set_anim(s, MZM_GADGET_CHOZO_PILLAR_PLATFORM_NO_SHADOW_SPAWNED);
        s->pose = RISING_CHOZO_PILLAR_POSE_WAIT_FOR_POWER_GRIP;
        break;
    case RISING_CHOZO_PILLAR_POSE_WAIT_FOR_POWER_GRIP:
        if (check_event(c, EVENT_POWER_GRIP_OBTAINED)) {
            s->pose = RISING_CHOZO_PILLAR_POSE_EXTENDING;
            s->scaling = 704;
            mzm_sfx_play(MZM_SOUND_RISING_CHOZO_PILLAR_RISING);
        }
        break;
    case RISING_CHOZO_PILLAR_POSE_EXTENDING: {
        if (s->scaling % 32 == 0) mzm_screen_shake_start_vertical(30, 0x81);  /* CONVERT_SECONDS(.5f) */
        s->scaling--;
        if (s->scaling != 0) {
            if (globals) globals->bg2_offset_y += PIXEL_SIZE / 2;
        } else {
            s->pose = RISING_CHOZO_PILLAR_POSE_SPAWN_3_PLATFORMS;
            mzm_sfx_fade(MZM_SOUND_RISING_CHOZO_PILLAR_RISING, 10);   /* CONVERT_SECONDS(1.f / 6) */
        }
        uint16_t debris_y = (uint16_t)(y + HALF_BLOCK_SIZE);
        uint16_t debris_x = (uint16_t)(x + BLOCK_SIZE * 3 + HALF_BLOCK_SIZE);
        rising_pillar_debris(c, debris_y, debris_x, (uint8_t)(s->scaling % 16));
        rising_pillar_particles(c, debris_y, debris_x, (uint8_t)(s->scaling % 128));
        break;
    }
    case RISING_CHOZO_PILLAR_POSE_SPAWN_3_PLATFORMS:
        s->pose = RISING_CHOZO_PILLAR_POSE_SPAWN_2_PLATFORMS;
        rising_pillar_spawn(c, y, x, 3);
        break;
    case RISING_CHOZO_PILLAR_POSE_SPAWN_2_PLATFORMS:
        s->pose = RISING_CHOZO_PILLAR_POSE_SPAWN_1_PLATFORMS;
        rising_pillar_spawn(c, y, x, 2);
        break;
    case RISING_CHOZO_PILLAR_POSE_SPAWN_1_PLATFORMS:
        s->pose = RISING_CHOZO_PILLAR_POSE_EXTENDED;
        rising_pillar_spawn(c, y, x, 1);
        break;
    case RISING_CHOZO_PILLAR_POSE_EXTENDED:
        s->pose = RISING_CHOZO_PILLAR_POSE_KILL;
        break;
    case RISING_CHOZO_PILLAR_POSE_KILL: {
        const MzmSpriteHooks *h = c->f->hooks;
        if (h && h->set_event) h->set_event(h->context, MZM_EVENT_CHOZO_PILLAR_FULLY_EXTENDED);
        kill(s);
        break;
    }
    default:
        break;
    }
}

/* ChozoPillarPlatform and ChozoPillarPlatformShadow: a platform appears
 * (with its spawning sound), the shadowed ones drop their shadow behind
 * Samus on the spawning animation's 22nd frame. */
static void chozo_pillar_platform(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    bool shadowed = s->room_slot != CHOZO_PILLAR_PLATFORM_NO_SHADOW;
    if (s->sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM_SHADOW) {
        if (s->pose == SPRITE_POSE_UNINITIALIZED) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
            s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
            s->samus_collision = SSC_NONE;
            s->draw_top = 0;
            s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
            s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
            s->pose = 8;
            ai_set_anim(s, MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW);
            s->draw_order = 12;
        }
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->y = (uint16_t)(s->y + PIXEL_SIZE);
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->samus_collision = SSC_NONE;
        s->draw_top = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_bottom = s->hitbox_left = s->hitbox_right = 0;
        if (check_event(c, MZM_EVENT_CHOZO_PILLAR_FULLY_EXTENDED)) {
            s->pose = CHOZO_PILLAR_PLATFORM_POSE_IDLE;
            if (shadowed) {
                ai_set_anim(s, MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW_SPAWNED);
                ai_spawn_secondary(c, MZM_SSPRITE_CHOZO_PILLAR_PLATFORM_SHADOW, 0, s->gfx_slot, s->y, s->x, 0);
            } else {
                ai_set_anim(s, MZM_GADGET_CHOZO_PILLAR_PLATFORM_NO_SHADOW_SPAWNED);
            }
        } else {
            s->pose = CHOZO_PILLAR_PLATFORM_POSE_SPAWNING;
            ai_set_anim(s, shadowed ? MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW_SPAWNING
                                    : MZM_GADGET_CHOZO_PILLAR_PLATFORM_NO_SHADOW_SPAWNING);
            mzm_sfx_play(MZM_SOUND_RISING_CHOZO_PILLAR_PLATFORM_SPAWNING);
        }
        break;
    case CHOZO_PILLAR_PLATFORM_POSE_SPAWNING:
        if (ai_animation_ending(c, 1)) {
            s->pose = CHOZO_PILLAR_PLATFORM_POSE_IDLE;
            ai_set_anim(s, shadowed ? MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW_SPAWNED
                                    : MZM_GADGET_CHOZO_PILLAR_PLATFORM_NO_SHADOW_SPAWNED);
        } else if (shadowed && s->frame == 21 && s->adc == 1) {
            ai_spawn_secondary(c, MZM_SSPRITE_CHOZO_PILLAR_PLATFORM_SHADOW, 0, s->gfx_slot, s->y, s->x, 0);
        }
        break;
    default:
        break;
    }
}

void ai_gadget(Ctx *c) {
    if (c->s->secondary) {
        if (c->s->sprite_id == SSPRITE_UNKNOWN_ITEM_BLOCK_LIGHT) unknown_item_block_light(c);
        else if (c->s->sprite_id == SSPRITE_MAP_STATION_PART) map_station_part(c);
        else if (c->s->sprite_id == SSPRITE_SAVE_PLATFORM_PART) save_platform_part(c);
        else if (c->s->sprite_id == MZM_SSPRITE_GUNSHIP_PART) ai_gunship(c);
        else if (c->s->sprite_id == MZM_SSPRITE_MORPH_BALL_LAUNCHER_PART) morph_ball_launcher_part(c);
        else if (c->s->sprite_id == MZM_SSPRITE_ZIPLINE_GENERATOR_PART) zipline_generator_part(c);
        else if (c->s->sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM ||
                 c->s->sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM_SHADOW)
            chozo_pillar_platform(c);
        return;
    }
    switch (c->s->sprite_id) {
    case ELEVATOR_PAD: elevator_pad(c); break;
    case WATER_DROP: water_drop(c); break;
    case STEAM_LARGE: case STEAM_SMALL: steam(c); break;
    case STEAM_LARGE_DIAGONAL_UP: case STEAM_SMALL_DIAGONAL_UP:
    case STEAM_LARGE_DIAGONAL_DOWN: case STEAM_SMALL_DIAGONAL_DOWN: steam_diagonal(c); break;
    case GERON_BRINSTAR_ROOM_15: case GERON_BRINSTAR_ROOM_1C: case GERON_VARIA1:
    case GERON_VARIA2: case GERON_VARIA3: geron(c); break;
    case GERON_NORFAIR: geron_norfair(c); break;
    case PLASMA_BEAM_BLOCK: case GRAVITY_SUIT_BLOCK: case SPACE_JUMP_BLOCK: unknown_item_block(c); break;
    case MAP_STATION: map_station(c); break;
    case SAVE_PLATFORM: save_platform(c); break;
    case MZM_PSPRITE_GUNSHIP: ai_gunship(c); break;
    case MZM_PSPRITE_AREA_BANNER: area_banner(c); break;
    case MZM_PSPRITE_MORPH_BALL_LAUNCHER: morph_ball_launcher(c); break;
    case MZM_PSPRITE_ZIPLINE: zipline(c); break;
    case MZM_PSPRITE_ZIPLINE_BUTTON: zipline_button(c); break;
    case MZM_PSPRITE_ZIPLINE_GENERATOR: zipline_generator(c); break;
    case MZM_PSPRITE_SECURITY_GATE_DEFAULT_OPEN:
    case MZM_PSPRITE_SECURITY_GATE_DEFAULT_CLOSED: security_gate(c); break;
    case MZM_PSPRITE_PISTON: piston(c); break;
    case MZM_PSPRITE_GLASS_TUBE: glass_tube(c); break;
    case MZM_PSPRITE_RISING_CHOZO_PILLAR: rising_chozo_pillar(c); break;
    default: break;
    }
}
