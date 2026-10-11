/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission sprites (no SDL).
 *
 * The parts of the native sprite engine the ported AIs need: room sprite
 * initialisation (SpriteInitPrimary), the update order of SpriteUpdate (the
 * sprite RNG from sSpriteRandomNumberTable, the AI, SpriteUpdateAnimation,
 * SpriteCheckOnScreen), the sprite view of Clipdata (ClipdataProcess /
 * ClipdataConvertToCollision for the sprite actor,
 * SpriteUtilGetCollisionAtPosition, SpriteUtilCheckVerticalCollisionAtPosition
 * and its slope variant, SpriteUtilAlignYPositionOnSlopeAtOrigin) and the
 * Zoomer AI (src/sprites_ai/zoomer.c). Positions are native subpixels (64 per
 * block). ROM tables (random numbers, falling speeds, stats) and the frame
 * durations of each OAM table come from scripts/mzm_sprites.py.
 * Combat (Samus contact, projectiles, death) is not ported yet. */
#ifndef MZM_SPRITES_H
#define MZM_SPRITES_H

#include <stdbool.h>
#include <stdint.h>

enum {
    MZM_SPRITE_STATUS_EXISTS = 1 << 0,
    MZM_SPRITE_STATUS_ONSCREEN = 1 << 1,
    MZM_SPRITE_STATUS_X_FLIP = 1 << 6,
    MZM_SPRITE_STATUS_Y_FLIP = 1 << 8,
    MZM_SPRITE_STATUS_FACING_RIGHT = 1 << 9,
    MZM_SPRITE_STATUS_FACING_DOWN = 1 << 10,
};

/* ClipdataType of the pinned decompilation; the solid flag of
 * ClipdataConvertToCollision is bit 24. */
enum {
    MZM_CLIPDATA_AIR, MZM_CLIPDATA_SOLID, MZM_CLIPDATA_LEFT_STEEP,
    MZM_CLIPDATA_RIGHT_STEEP, MZM_CLIPDATA_LEFT_UPPER_SLIGHT, MZM_CLIPDATA_LEFT_LOWER_SLIGHT,
    MZM_CLIPDATA_RIGHT_LOWER_SLIGHT, MZM_CLIPDATA_RIGHT_UPPER_SLIGHT, MZM_CLIPDATA_ENEMY_ONLY,
    MZM_CLIPDATA_STOP_ENEMY, MZM_CLIPDATA_TANK, MZM_CLIPDATA_DOOR,
    MZM_CLIPDATA_PASS_THROUGH_BOTTOM,
};
#define MZM_CLIPDATA_SOLID_FLAG (1u << 24)

/* CollisionResult */
enum {
    MZM_COLLISION_AIR = 0,
    MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE = 2,
    MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE = 3,
    MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE = 4,
    MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE = 5,
    MZM_COLLISION_PASS_THROUGH_BOTTOM = 16,
    MZM_COLLISION_SOLID = 17,
    MZM_COLLISION_FLAGS_F = 0x0F,
    MZM_COLLISION_FLAGS_F0 = 0xF0,
};

/* ClipdataActor */
enum { MZM_ACTOR_SAMUS, MZM_ACTOR_NON_SPRITE, MZM_ACTOR_SPRITE };

/* The room as sprites see it. */
typedef struct {
    void *context;
    /* Native ClipdataType of the block at a subpixel point (air outside the
     * room). */
    uint8_t (*block)(void *context, uint16_t y, uint16_t x);
    uint16_t camera_x, camera_y;    /* gBg1XPosition / gBg1YPosition */
} MzmSpriteWorld;

/* ROM tables of scripts/mzm_sprites.py (metadata/sprites.tsv). */
typedef struct {
    uint8_t random[32];             /* sSpriteRandomNumberTable */
    int16_t falling[16];            /* sSpritesFallingSpeed */
    uint16_t health[256], damage[256];  /* sPrimarySpriteStats */
} MzmSpriteTables;

/* Zoomer OAM tables, in the order of the AI below. */
enum {
    MZM_ZOOMER_FALLING, MZM_ZOOMER_ON_GROUND, MZM_ZOOMER_ON_WALL,
    MZM_ZOOMER_WALKING_ON_LEFT_SLOPE, MZM_ZOOMER_WALKING_ON_RIGHT_SLOPE,
    MZM_ZOOMER_TURNING_EDGE_LEFT, MZM_ZOOMER_TURNING_EDGE_RIGHT,
    MZM_ZOOMER_TURNING_CORNER_LEFT, MZM_ZOOMER_TURNING_CORNER_RIGHT, MZM_ZOOMER_ANIMS,
};
#define MZM_SPRITE_MAX_ANIMS 16
#define MZM_SPRITE_MAX_FRAMES 32
extern const char *const mzm_zoomer_anim_names[MZM_ZOOMER_ANIMS];

/* Frame durations of each OAM table a sprite uses (0 ends a table). */
typedef struct {
    uint8_t timers[MZM_SPRITE_MAX_ANIMS][MZM_SPRITE_MAX_FRAMES];
    uint8_t counts[MZM_SPRITE_MAX_ANIMS];
} MzmSpriteKind;

enum { MZM_PSPRITE_ZOOMER_YELLOW = 18, MZM_PSPRITE_ZOOMER_RED = 19 };

typedef struct {
    uint16_t status;
    uint8_t sprite_id;              /* PSPRITE */
    uint8_t gfx_slot, room_slot;
    uint16_t x, y;                  /* subpixels, bottom middle of the block */
    uint8_t pose;
    uint8_t anim;                   /* pOam: table index of the kind */
    uint8_t frame;                  /* currentAnimationFrame */
    uint8_t adc;                    /* animationDurationCounter */
    uint8_t work1, work3;
    uint16_t health;
    uint8_t palette_row, absolute_palette_row;
    int16_t hitbox_top, hitbox_bottom, hitbox_left, hitbox_right;
    uint8_t draw_top, draw_bottom, draw_horizontal;
    uint8_t samus_collision;
    uint8_t isft, freeze_timer, ignore_samus_timer;
} MzmSprite;

/* ClipdataProcess for an actor: the ClipdataType, plus the solid flag when
 * the point is in the solid part of the block (ClipdataConvertToCollision,
 * original version). Enemy-only, stop-enemy, tank and door blocks become air
 * or solid by actor. */
uint32_t mzm_clipdata_process(const MzmSpriteWorld *world, uint16_t y, uint16_t x, int actor);

/* SpriteInitPrimary for a sprite of a known PSPRITE at a block position. */
void mzm_sprite_init_primary(MzmSprite *sprite, uint8_t sprite_id, uint8_t gfx_slot,
                             uint16_t y_block, uint16_t x_block, uint8_t room_slot);
/* One SpriteUpdate step of sprite `slot` (frame counters as the game's). */
void mzm_sprite_update(MzmSprite *sprite, uint8_t slot, const MzmSpriteWorld *world,
                       const MzmSpriteTables *tables, const MzmSpriteKind *kind,
                       uint8_t frame8, uint16_t frame16);
/* Whether a PSPRITE has a ported AI. */
bool mzm_sprite_supported(uint8_t sprite_id);

#endif /* MZM_SPRITES_H */
