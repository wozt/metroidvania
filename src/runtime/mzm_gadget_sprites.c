/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission room gadgets: the elevator pad (src/sprites_ai/elevator_pad.c),
 * water drops (water_drop.c), steam vents (steam.c), Gerons
 * (geron.c, geron_norfair.c) and the unknown item blocks
 * (unknown_item_block.c) and the map stations (map_station.c); comments
 * name the pinned decompilation's routines. */
#include "mzm_blocks.h"
#include "mzm_message_banner.h"
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
    /* Event */
    EVENT_POWER_GRIP_OBTAINED = 16, EVENT_HIGH_JUMP_OBTAINED = 18, EVENT_SPACE_JUMP_OBTAINED = 22,
    EVENT_GRAVITY_SUIT_OBTAINED = 23, EVENT_PLASMA_BEAM_OBTAINED = 24,
    EVENT_PLANT_DESTROYED_LAVA = 47, EVENT_PLANT_DESTROYED_POST_VARIA = 48,
    EVENT_PLANT_DESTROYED_VARIA2 = 49, EVENT_PLANT_DESTROYED_VARIA3 = 50,
    EVENT_PLANT_DESTROYED_VARIA1 = 51,
    /* Sound */
    SOUND_UNKNOWN_ITEM_BLOCK_BREAKING = 315, SOUND_GERON_SHAKING = 619, SOUND_GERON_BREAKING = 620,
    SOUND_STEAM_LARGE = 632, SOUND_STEAM_SMALL = 633, SOUND_USING_MAP_STATION = 0x123,
    SOUND_MAP_STATION_RETRACTING = 0x124,
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
    case GERON_POSE_IDLE:
        /* The Parasites that would grab it are not ported (work3 stays
         * under four). */
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

void ai_gadget(Ctx *c) {
    if (c->s->secondary) {
        if (c->s->sprite_id == SSPRITE_UNKNOWN_ITEM_BLOCK_LIGHT) unknown_item_block_light(c);
        else if (c->s->sprite_id == SSPRITE_MAP_STATION_PART) map_station_part(c);
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
    default: break;
    }
}
