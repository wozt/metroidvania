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
 * Zoomer, Zeela, Ripper, Skree, Waver, Sidehopper, Dessgeega, Viola, Sova,
 * Zeb, Geega, Zebbo, Reo, Mella, Holtz, Multiviola, Rinka, Geruta,
 * Ripper II and Squeept AIs (src/sprites_ai/zoomer.c, zeela.c, ripper.c,
 * skree.c, waver.c, sidehopper.c, dessgeega.c, viola.c, sova.c, zeb.c,
 * geega.c, zebbo.c, reo.c, mella.c, holtz.c, multiviola.c, rinka.c, geruta.c,
 * ripper2.c, squeept.c, gamet.c, skultera.c) with SpriteSpawnSecondary for the Skree's explosion and
 * the red Zeela's eyes, and the Space Pirates with their lasers
 * (src/sprites_ai/space_pirate.c, ported in mzm_space_pirate.c). Positions are native subpixels (64 per
 * block). ROM tables (random numbers, falling speeds, jump velocities,
 * stats) and the frame durations of each OAM table come from
 * scripts/mzm_sprites.py.
 * Combat ports SpriteUtilSamusAndSpriteCollision for sprites that hurt Samus
 * (with the screw attack kill of SpriteUtilSpriteTakeDamageFromSamusContact)
 * and the beam and missile parts of ProjectileCheckHittingSprite. A killed
 * sprite becomes the enemy drop of SpriteUtilDetermineEnemyDrop, or is
 * removed, with its explosion particle (and the screw attack debris); drops
 * follow src/sprites_ai/enemy_drop.c. Sprite debris (SpriteDebrisInit,
 * SpriteDebrisProcess) is processed here. Sounds are not ported. */
#ifndef MZM_SPRITES_H
#define MZM_SPRITES_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_effects.h"
#include "mzm_projectiles.h"

enum {
    MZM_SPRITE_STATUS_EXISTS = 1 << 0,
    MZM_SPRITE_STATUS_ONSCREEN = 1 << 1,
    MZM_SPRITE_STATUS_NOT_DRAWN = 1 << 2,
    MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE = 1 << 3,
    MZM_SPRITE_STATUS_MOSAIC = 1 << 5,
    MZM_SPRITE_STATUS_X_FLIP = 1 << 6,
    MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE = 1 << 7,
    MZM_SPRITE_STATUS_Y_FLIP = 1 << 8,
    MZM_SPRITE_STATUS_FACING_RIGHT = 1 << 9,
    MZM_SPRITE_STATUS_FACING_DOWN = 1 << 10,
    MZM_SPRITE_STATUS_SAMUS_COLLIDING = 1 << 11,
    MZM_SPRITE_STATUS_SAMUS_ON_TOP = 1 << 12,
    MZM_SPRITE_STATUS_HIGH_PRIORITY = 1 << 13,
    MZM_SPRITE_STATUS_DOUBLE_SIZE = 1 << 14,
    MZM_SPRITE_STATUS_IGNORE_PROJECTILES = 1 << 15,
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
    /* gCurrentAffectingClipdata.movement after ClipdataProcess at a subpixel
     * point (sMovementClipdataValues of the block's behavior;
     * CLIPDATA_MOVEMENT_NONE outside the room). NULL: no movement blocks. */
    uint8_t (*movement)(void *context, uint16_t y, uint16_t x);
    /* gEffectYPosition: the room effect's surface in subpixels (0 without
     * one). */
    uint16_t effect_y;
    /* gCurrentAffectingClipdata.hazard after ClipdataProcess at a subpixel
     * point (HazardType); NULL: no hazards. */
    uint8_t (*hazard)(void *context, uint16_t y, uint16_t x);
} MzmSpriteWorld;

/* ClipdataMovement values the ported sprites read. */
enum {
    MZM_CLIPDATA_MOVEMENT_NONE = 0,
    MZM_CLIPDATA_MOVEMENT_STOP_ENEMY_BLOCK_SOLID = 7,
    MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_ZONELINE = 8,
    MZM_CLIPDATA_MOVEMENT_SPACE_PIRATE_WALLJUMP_POINT = 9,
};

/* Sprite debris velocity tables, in this order. */
enum {
    MZM_DEBRIS_BOUNCING, MZM_DEBRIS_11, MZM_DEBRIS_12, MZM_DEBRIS_13, MZM_DEBRIS_4,
    MZM_DEBRIS_5, MZM_DEBRIS_6, MZM_DEBRIS_7, MZM_DEBRIS_8, MZM_DEBRIS_TABLES,
};
extern const char *const mzm_debris_table_names[MZM_DEBRIS_TABLES];

/* A MultiSpriteData table (gSubSpriteData1.pMultiOam): per frame, its
 * timer (0 ends the table) and each part's OAM index, y and x offsets. */
#define MZM_MULTI_FRAMES 12
#define MZM_MULTI_PARTS 5
typedef struct {
    uint8_t timers[MZM_MULTI_FRAMES];
    int16_t parts[MZM_MULTI_FRAMES][MZM_MULTI_PARTS][3];
} MzmMultiSprite;
enum { MZM_CHOZO_MULTI_STANDING, MZM_CHOZO_MULTI_SITTING, MZM_CHOZO_MULTI_SEATED,
       /* sUnknownItemChozoStatueMultiSpriteData_*: the same three, offset. */
       MZM_CHOZO_MULTI_UNKNOWN, MZM_CHOZO_MULTI_COUNT = MZM_CHOZO_MULTI_UNKNOWN * 2 };
extern const char *const mzm_chozo_multi_names[MZM_CHOZO_MULTI_COUNT];

/* ROM tables of scripts/mzm_sprites.py (metadata/sprites.tsv). */
typedef struct {
    uint8_t random[32];             /* sSpriteRandomNumberTable */
    int16_t falling[16];            /* sSpritesFallingSpeed */
    uint16_t health[256], damage[256];  /* sPrimarySpriteStats */
    uint8_t weakness[256];          /* SpriteWeakness (low byte of column 2) */
    /* The freeze flag (high byte): frozen when gFrameCounter8Bit & flag is 0. */
    uint8_t freeze[256], secondary_freeze[256];
    uint16_t drops[256][5];         /* drop probabilities (columns 4-8), out of 1024 */
    uint16_t secondary_damage[256]; /* sSecondarySpriteStats */
    uint8_t secondary_weakness[256];
    int16_t skree_falling[16];      /* sSkreeFallingSpeed (SHORT_MAX ends it) */
    int16_t zeela_eyes_up[16];      /* sZeelaEyesFallingFromUpSpeed */
    int16_t zeela_eyes_bottom[16];  /* sZeelaEyesFallingFromBottomSpeed */
    int16_t falling_ceiling[16];    /* sSpritesFallingCeilingSpeed (SHORT_MAX ends it) */
    int16_t sidehopper_high[10];    /* sSidehopperHighJumpVelocity */
    int16_t sidehopper_low[10];     /* sSidehopperLowJumpVelocity */
    int16_t dessgeega_high[10];     /* sDessgeegaHighJumpYVelocity */
    int16_t dessgeega_low[10];      /* sDessgeegaLowJumpYVelocity */
    int16_t debris[MZM_DEBRIS_TABLES][9]; /* sDebris*YVelocity, sDebrisBouncingVelocity */
    int16_t reo_idle_y[65];         /* sReoIdleYMovement (SHORT_MAX ends it) */
    int16_t reo_idle_x[81];         /* sReoIdleXMovement */
    int16_t mella_idle_y[65];       /* sMellaIdleYMovement */
    int16_t mella_idle_x[49];       /* sMellaIdleXMovement */
    int16_t mella_down[6];          /* sMellaGoingDownYMovement (u16) */
    int16_t mella_up[7];            /* sMellaGoingUpYMovement (u16) */
    int16_t mella_x[7];             /* sMellaMovingXMovement (u16) */
    int16_t holtz_idle_y[65];       /* sHoltzIdleYVelocity */
    int16_t squeept_rising[65];     /* sSqueeptRisingMovement (SHORT_MAX ends it) */
    int16_t squeept_falling[16];    /* sSqueeptFallingMovement (SHORT_MAX ends it) */
    int16_t charge_beam_idle_y[66]; /* sChargeBeamIdleYMovement (SHORT_MAX ends it) */
    int16_t pirate_wall_jump[8];    /* sSpacePirateWallJumpingVelocity */
    int16_t pirate_jump_low[11];    /* sSpacePirate_2e1030 */
    int16_t pirate_jump[10];        /* sSpacePirateJumpingVelocity */
    int16_t pirate_jump_high[11];   /* sSpacePirate_2e105a */
    /* sChozoStatueMultiSpriteData_Standing, _Sitting and _Seated. */
    MzmMultiSprite chozo_multi[MZM_CHOZO_MULTI_COUNT];
} MzmSpriteTables;

/* Columns of MzmSpriteTables.drops. */
enum {
    MZM_DROP_PROB_SMALL_ENERGY, MZM_DROP_PROB_LARGE_ENERGY, MZM_DROP_PROB_MISSILE,
    MZM_DROP_PROB_SUPER_MISSILE, MZM_DROP_PROB_POWER_BOMB,
};

/* SpriteWeakness */
enum {
    MZM_WEAKNESS_CHARGE_BEAM_PISTOL = 1 << 0,
    MZM_WEAKNESS_BEAM_BOMBS = 1 << 1,
    MZM_WEAKNESS_SUPER_MISSILES = 1 << 2,
    MZM_WEAKNESS_MISSILES = 1 << 3,
    MZM_WEAKNESS_POWER_BOMB = 1 << 4,
    MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK = 1 << 5,
    MZM_WEAKNESS_CAN_BE_FROZEN = 1 << 6,
};

/* Zoomer OAM tables, in the order of the AI below. */
enum {
    MZM_ZOOMER_FALLING, MZM_ZOOMER_ON_GROUND, MZM_ZOOMER_ON_WALL,
    MZM_ZOOMER_WALKING_ON_LEFT_SLOPE, MZM_ZOOMER_WALKING_ON_RIGHT_SLOPE,
    MZM_ZOOMER_TURNING_EDGE_LEFT, MZM_ZOOMER_TURNING_EDGE_RIGHT,
    MZM_ZOOMER_TURNING_CORNER_LEFT, MZM_ZOOMER_TURNING_CORNER_RIGHT, MZM_ZOOMER_ANIMS,
};
#define MZM_SPRITE_MAX_ANIMS 40   /* the Space Pirate and its laser use 33 */
#define MZM_SPRITE_MAX_FRAMES 64   /* sMultiviolaOam_Moving has 56 */
extern const char *const mzm_zoomer_anim_names[MZM_ZOOMER_ANIMS];

/* Enemy drop OAM tables (src/data/sprites/enemy_drop.c), one per drop. */
enum {
    MZM_DROP_SMALL_ENERGY, MZM_DROP_LARGE_ENERGY, MZM_DROP_MISSILE,
    MZM_DROP_SUPER_MISSILE, MZM_DROP_POWER_BOMB, MZM_DROP_ANIMS,
};
extern const char *const mzm_drop_anim_names[MZM_DROP_ANIMS];

/* Zeela OAM tables (src/data/sprites/zeela.c); the shared crawler code
 * uses the Zoomer's indices for the ground, wall and turn tables. */
enum {
    MZM_ZEELA_FALLING_ON_GROUND, MZM_ZEELA_ON_GROUND, MZM_ZEELA_ON_WALL,
    MZM_ZEELA_WALKING_ON_SLOPE, MZM_ZEELA_FALLING_ON_WALL, MZM_ZEELA_TURNING_EDGE_LEFT,
    MZM_ZEELA_TURNING_EDGE_RIGHT, MZM_ZEELA_TURNING_CORNER_LEFT, MZM_ZEELA_TURNING_CORNER_RIGHT,
    MZM_ZEELA_EYES_IDLE, MZM_ZEELA_EYES_EXPLODING, MZM_ZEELA_ANIMS,
};
extern const char *const mzm_zeela_anim_names[MZM_ZEELA_ANIMS];

/* Ripper OAM tables (src/data/sprites/ripper.c). */
enum {
    MZM_RIPPER_MOVING, MZM_RIPPER_TURNING_AROUND, MZM_RIPPER_TURNING_AROUND_PART2,
    MZM_RIPPER_ANIMS,
};
extern const char *const mzm_ripper_anim_names[MZM_RIPPER_ANIMS];

/* Skree OAM tables (src/data/sprites/skree.c), its explosion's last. */
enum {
    MZM_SKREE_IDLE, MZM_SKREE_SPINNING, MZM_SKREE_GOING_DOWN, MZM_SKREE_CRASHING,
    MZM_SKREE_EXPLOSION_GOING_DOWN, MZM_SKREE_EXPLOSION_GOING_UP, MZM_SKREE_ANIMS,
};
extern const char *const mzm_skree_anim_names[MZM_SKREE_ANIMS];

/* Waver OAM table (src/data/sprites/waver.c). */
enum { MZM_WAVER_MOVING, MZM_WAVER_ANIMS };
extern const char *const mzm_waver_anim_names[MZM_WAVER_ANIMS];

/* Sidehopper OAM tables (src/data/sprites/sidehopper.c), in the order the
 * AI assigns them. */
enum {
    MZM_SIDEHOPPER_IDLE, MZM_SIDEHOPPER_SHAKING_HEAD, MZM_SIDEHOPPER_JUMP_WARNING,
    MZM_SIDEHOPPER_JUMPING, MZM_SIDEHOPPER_LANDING, MZM_SIDEHOPPER_ANIMS,
};
extern const char *const mzm_sidehopper_anim_names[MZM_SIDEHOPPER_ANIMS];

/* Dessgeega OAM tables (src/data/sprites/dessgeega.c). */
enum {
    MZM_DESSGEEGA_IDLE, MZM_DESSGEEGA_SCREAMING, MZM_DESSGEEGA_JUMP_WARNING,
    MZM_DESSGEEGA_JUMPING, MZM_DESSGEEGA_LANDING, MZM_DESSGEEGA_ANIMS,
};
extern const char *const mzm_dessgeega_anim_names[MZM_DESSGEEGA_ANIMS];

/* Viola OAM table (src/data/sprites/viola.c; the offset tables are unused). */
enum { MZM_VIOLA_MOVING, MZM_VIOLA_ANIMS };
extern const char *const mzm_viola_anim_names[MZM_VIOLA_ANIMS];

/* Sova OAM tables (src/data/sprites/sova.c). */
enum {
    MZM_SOVA_ON_GROUND, MZM_SOVA_WALKING_ON_LEFT_SLOPE, MZM_SOVA_WALKING_ON_RIGHT_SLOPE,
    MZM_SOVA_WALKING_ON_LEFT, MZM_SOVA_WALKING_ON_RIGHT, MZM_SOVA_WALKING_ON_CEILING,
    MZM_SOVA_TURNING_CORNER_RIGHT, MZM_SOVA_TURNING_EDGE_RIGHT, MZM_SOVA_TURNING_EDGE_LEFT,
    MZM_SOVA_TURNING_CORNER_LEFT, MZM_SOVA_FALLING, MZM_SOVA_ANIMS,
};
extern const char *const mzm_sova_anim_names[MZM_SOVA_ANIMS];

/* Zeb and Geega OAM tables (src/data/sprites/zeb.c, geega.c). */
enum { MZM_PIPE_BUG_IDLE, MZM_PIPE_BUG_MOVING, MZM_PIPE_BUG_ANIMS };
extern const char *const mzm_zeb_anim_names[MZM_PIPE_BUG_ANIMS];
extern const char *const mzm_geega_anim_names[MZM_PIPE_BUG_ANIMS];
extern const char *const mzm_zebbo_anim_names[MZM_PIPE_BUG_ANIMS];
extern const char *const mzm_gamet_anim_names[MZM_PIPE_BUG_ANIMS];

/* Reo OAM tables (src/data/sprites/reo.c). */
enum { MZM_REO_IDLE, MZM_REO_MOVING, MZM_REO_ANIMS };
extern const char *const mzm_reo_anim_names[MZM_REO_ANIMS];

/* Mella, Holtz and Multiviola OAM tables (src/data/sprites/mella.c,
 * holtz.c, multiviola.c; the Mella moves with sMellowOam_Moving). */
enum { MZM_MELLA_IDLE, MZM_MELLA_MOVING, MZM_MELLA_ANIMS };
extern const char *const mzm_mella_anim_names[MZM_MELLA_ANIMS];
enum {
    MZM_HOLTZ_IDLE, MZM_HOLTZ_IDLE_AWARE, MZM_HOLTZ_WARNING, MZM_HOLTZ_GOING_DOWN,
    MZM_HOLTZ_GOING_UP, MZM_HOLTZ_BONKING_ON_CEILING, MZM_HOLTZ_ANIMS,
};
extern const char *const mzm_holtz_anim_names[MZM_HOLTZ_ANIMS];
enum { MZM_MULTIVIOLA_MOVING, MZM_MULTIVIOLA_ANIMS };
extern const char *const mzm_multiviola_anim_names[MZM_MULTIVIOLA_ANIMS];

/* Geruta and Ripper II OAM tables (src/data/sprites/geruta.c, ripper2.c). */
enum {
    MZM_GERUTA_IDLE, MZM_GERUTA_WARNING, MZM_GERUTA_LAUNCHING, MZM_GERUTA_GOING_DOWN,
    MZM_GERUTA_BOUNCING, MZM_GERUTA_GOING_UP, MZM_GERUTA_BOUNCING_ON_CEILING, MZM_GERUTA_ANIMS,
};
extern const char *const mzm_geruta_anim_names[MZM_GERUTA_ANIMS];
enum {
    MZM_RIPPER2_MOVING, MZM_RIPPER2_TURNING_AROUND, MZM_RIPPER2_TURNING_AROUND_PART2,
    MZM_RIPPER2_ANIMS,
};
extern const char *const mzm_ripper2_anim_names[MZM_RIPPER2_ANIMS];
/* The item sprites' OAM tables (src/data/sprites/morph_ball.c,
 * power_grip.c, charge_beam.c). */
enum {
    MZM_ITEM_SPRITE_MORPH_BALL, MZM_ITEM_SPRITE_MORPH_BALL_OUTSIDE, MZM_ITEM_SPRITE_POWER_GRIP,
    MZM_ITEM_SPRITE_POWER_GRIP_GLOW, MZM_ITEM_SPRITE_CHARGE_BEAM_SPAWNING,
    MZM_ITEM_SPRITE_CHARGE_BEAM_VISIBLE, MZM_ITEM_SPRITE_CHARGE_BEAM_GLOW, MZM_ITEM_SPRITE_ANIMS,
};
extern const char *const mzm_item_sprite_anim_names[MZM_ITEM_SPRITE_ANIMS];
/* Skultera OAM tables (src/data/sprites/skultera.c). */
enum { MZM_SKULTERA_IDLE, MZM_SKULTERA_TURNING_AROUND, MZM_SKULTERA_CHASING_SAMUS, MZM_SKULTERA_ANIMS };
extern const char *const mzm_skultera_anim_names[MZM_SKULTERA_ANIMS];
/* Squeept OAM tables (src/data/sprites/squeept.c). */
enum { MZM_SQUEEPT_GOING_UP, MZM_SQUEEPT_TURNING_AROUND, MZM_SQUEEPT_GOING_DOWN, MZM_SQUEEPT_ANIMS };
extern const char *const mzm_squeept_anim_names[MZM_SQUEEPT_ANIMS];

/* Rinka OAM tables (src/data/sprites/rinka.c). */
enum {
    MZM_RINKA_ORANGE_SPAWNING, MZM_RINKA_ORANGE_MOVING, MZM_RINKA_GREEN_SPAWNING,
    MZM_RINKA_GREEN_MOVING, MZM_RINKA_ANIMS,
};
extern const char *const mzm_rinka_anim_names[MZM_RINKA_ANIMS];

/* Space Pirate and Space Pirate laser OAM tables
 * (src/data/sprites/space_pirate.c). */
enum {
    MZM_PIRATE_CROUCHED, MZM_PIRATE_WALKING, MZM_PIRATE_CHARGING_LASER_FORWARD,
    MZM_PIRATE_TURNING_AROUND_FIRST_PART, MZM_PIRATE_TURNING_AROUND_SECOND_PART,
    MZM_PIRATE_SHOOTING_FORWARD, MZM_PIRATE_JUMPING, MZM_PIRATE_CHARGING_LASER_DIAGONALLY_DOWN,
    MZM_PIRATE_FALLING, MZM_PIRATE_LANDING, MZM_PIRATE_SHOOTING_DIAGONALLY_DOWN,
    MZM_PIRATE_CHARGING_LASER_DIAGONALLY_UP, MZM_PIRATE_SHOOTING_DIAGONALLY_UP,
    MZM_PIRATE_CLIMBING_UP, MZM_PIRATE_CLIMBING_DOWN, MZM_PIRATE_STARTING_TO_CRAWL,
    MZM_PIRATE_CRAWLING, MZM_PIRATE_STANDING_UP, MZM_PIRATE_TURNING_WHILE_CRAWLING_FIRST_PART,
    MZM_PIRATE_TURNING_WHILE_CRAWLING_SECOND_PART, MZM_PIRATE_CRAWLING_STOPPED,
    MZM_PIRATE_TURNING_TO_AIM_WHILE_CLIMBING, MZM_PIRATE_AIMING_WHILE_CLIMBING,
    MZM_PIRATE_CLIMBING_CHARGING_LASER_FORWARD, MZM_PIRATE_CLIMBING_SHOOTING_FORWARD,
    MZM_PIRATE_CLIMBING_RETRACTING_AIM, MZM_PIRATE_LAUNCHING_FROM_WALL,
    MZM_PIRATE_JUMPING_FROM_WALL, MZM_PIRATE_STANDING, MZM_PIRATE_STANDING_ALERTED,
    MZM_PIRATE_LASER_FORWARD, MZM_PIRATE_LASER_DIAGONAL, MZM_PIRATE_LASER_EXPLODING,
    MZM_PIRATE_ANIMS,
};
extern const char *const mzm_space_pirate_anim_names[MZM_PIRATE_ANIMS];

/* Chozodia security OAM tables (src/data/sprites/searchlight.c,
 * security_laser.c, searchlight_eye.c). */
enum {
    MZM_SECURITY_SEARCHLIGHT_MOVING,
    MZM_SECURITY_LASER_VERTICAL_SMALL, MZM_SECURITY_LASER_VERTICAL_MEDIUM,
    MZM_SECURITY_LASER_VERTICAL_LARGE, MZM_SECURITY_LASER_VERTICAL_VERY_LARGE,
    MZM_SECURITY_LASER_HORIZONTAL_SMALL, MZM_SECURITY_LASER_HORIZONTAL_MEDIUM,
    MZM_SECURITY_LASER_HORIZONTAL_LARGE, MZM_SECURITY_LASER_HORIZONTAL_VERY_LARGE,
    MZM_SECURITY_LASER_VERTICAL_SMALL_DISAPPEARING,
    MZM_SECURITY_LASER_VERTICAL_MEDIUM_DISAPPEARING,
    MZM_SECURITY_LASER_VERTICAL_LARGE_DISAPPEARING,
    MZM_SECURITY_LASER_VERTICAL_VERY_LARGE_DISAPPEARING,
    MZM_SECURITY_LASER_HORIZONTAL_SMALL_DISAPPEARING,
    MZM_SECURITY_LASER_HORIZONTAL_MEDIUM_DISAPPEARING,
    MZM_SECURITY_LASER_HORIZONTAL_LARGE_DISAPPEARING,
    MZM_SECURITY_LASER_HORIZONTAL_VERY_LARGE_DISAPPEARING,
    MZM_SECURITY_EYE_IDLE, MZM_SECURITY_EYE_SHOOTING, MZM_SECURITY_EYE_BEAM_IDLE,
    MZM_SECURITY_EYE_PROJECTILE_MOVING, MZM_SECURITY_ANIMS,
};
extern const char *const mzm_chozodia_security_anim_names[MZM_SECURITY_ANIMS];

/* Chozo statue OAM tables, in ChozoStatueOam order (sChozoStatueFrameDataPointers,
 * src/data/sprites/chozo_statue.c). */
enum {
    MZM_CHOZO_LEG_STANDING, MZM_CHOZO_LEG_SITTING, MZM_CHOZO_LEG_SEATED, MZM_CHOZO_IDLE,
    MZM_CHOZO_EYE_OPENED, MZM_CHOZO_EYE_CLOSING, MZM_CHOZO_EYE_OPENING, MZM_CHOZO_EYE_CLOSED,
    MZM_CHOZO_ARM_IDLE, MZM_CHOZO_ARM_GLOW, MZM_CHOZO_ARM_SAMUS_GRABBED,
    MZM_CHOZO_BALL_NORMAL_CLOSED, MZM_CHOZO_BALL_NORMAL_REVEALING, MZM_CHOZO_BALL_NORMAL_REVEALED,
    MZM_CHOZO_REFILL, MZM_CHOZO_REFILL_GLOW_IDLE,
    /* The unknown item statues' tables (UnknownItemChozoStatueOam), in the
     * same order. */
    MZM_CHOZO_UNKNOWN_BASE, MZM_CHOZO_ANIMS = MZM_CHOZO_UNKNOWN_BASE * 2,
};
extern const char *const mzm_chozo_statue_anim_names[MZM_CHOZO_ANIMS];

/* Sprites sharing an AI and its OAM tables. `library_sprites` names, per
 * table, the exported sprite whose frame durations the family uses. */
enum {
    MZM_FAMILY_ZOOMER, MZM_FAMILY_ZEELA, MZM_FAMILY_RIPPER, MZM_FAMILY_SKREE, MZM_FAMILY_WAVER,
    MZM_FAMILY_DROP, MZM_FAMILY_SIDEHOPPER, MZM_FAMILY_DESSGEEGA, MZM_FAMILY_VIOLA,
    MZM_FAMILY_SOVA, MZM_FAMILY_ZEB, MZM_FAMILY_GEEGA, MZM_FAMILY_ZEBBO, MZM_FAMILY_REO,
    MZM_FAMILY_MELLA, MZM_FAMILY_HOLTZ, MZM_FAMILY_MULTIVIOLA, MZM_FAMILY_RINKA,
    MZM_FAMILY_SPACE_PIRATE, MZM_FAMILY_CHOZODIA_SECURITY, MZM_FAMILY_CHOZO_STATUE,
    MZM_FAMILY_GERUTA, MZM_FAMILY_RIPPER2, MZM_FAMILY_SQUEEPT, MZM_FAMILY_GAMET,
    MZM_FAMILY_ITEM_SPRITE, MZM_FAMILY_SKULTERA,
    MZM_SPRITE_FAMILIES,
};
typedef struct {
    int count;
    const char *const *anims;
    const char *const *library_sprites;
} MzmSpriteFamily;
extern const MzmSpriteFamily mzm_sprite_families[MZM_SPRITE_FAMILIES];

/* Frame durations of each OAM table a sprite uses (0 ends a table). */
typedef struct {
    uint8_t timers[MZM_SPRITE_MAX_ANIMS][MZM_SPRITE_MAX_FRAMES];
    uint8_t counts[MZM_SPRITE_MAX_ANIMS];
} MzmSpriteKind;

enum {
    MZM_PSPRITE_UNUSED0 = 0,
    MZM_PSPRITE_ZOOMER_YELLOW = 18, MZM_PSPRITE_ZOOMER_RED = 19,
    MZM_PSPRITE_ZEELA = 20, MZM_PSPRITE_ZEELA_RED = 21,
    MZM_PSPRITE_RIPPER_BROWN = 22, MZM_PSPRITE_RIPPER_PURPLE = 23,
    MZM_PSPRITE_LARGE_ENERGY_DROP = 26, MZM_PSPRITE_SMALL_ENERGY_DROP = 27,
    MZM_PSPRITE_MISSILE_DROP = 28, MZM_PSPRITE_SUPER_MISSILE_DROP = 29,
    MZM_PSPRITE_POWER_BOMB_DROP = 30,
    MZM_PSPRITE_SKREE_GREEN = 31, MZM_PSPRITE_SKREE_BLUE = 32,
    MZM_PSPRITE_WAVER = 72,
    MZM_PSPRITE_DESSGEEGA = 70, MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM = 71,
    MZM_PSPRITE_SIDEHOPPER = 121,
    MZM_PSPRITE_VIOLA_BLUE = 104, MZM_PSPRITE_VIOLA_ORANGE = 105,
    MZM_PSPRITE_SOVA_PURPLE = 50, MZM_PSPRITE_SOVA_ORANGE = 51,
    MZM_PSPRITE_ZEB = 24, MZM_PSPRITE_ZEB_BLUE = 25,
    MZM_PSPRITE_GEEGA = 122, MZM_PSPRITE_GEEGA_WHITE = 123,
    MZM_PSPRITE_GEEGA_LEADER = 159, MZM_PSPRITE_GEEGA_FOLLOWER = 160,
    MZM_PSPRITE_ZEBBO_GREEN = 91, MZM_PSPRITE_ZEBBO_YELLOW = 92,
    MZM_PSPRITE_REO_GREEN_WINGS = 63, MZM_PSPRITE_REO_PURPLE_WINGS = 64,
    MZM_PSPRITE_MELLA = 114, MZM_PSPRITE_HOLTZ = 107, MZM_PSPRITE_MULTIVIOLA = 52,
    MZM_PSPRITE_RINKA_ORANGE = 102, MZM_PSPRITE_RINKA_GREEN = 165,
    MZM_PSPRITE_GERUTA_RED = 54, MZM_PSPRITE_GERUTA_GREEN = 55, MZM_PSPRITE_RIPPERII = 113,
    MZM_PSPRITE_SQUEEPT = 56,
    MZM_PSPRITE_SKULTERA = 69, MZM_PSPRITE_MORPH_BALL = 33, MZM_PSPRITE_POWER_GRIP = 76, MZM_PSPRITE_CHARGE_BEAM = 68,
    MZM_PSPRITE_GAMET_BLUE_SINGLE = 86, MZM_PSPRITE_GAMET_RED_SINGLE = 87,
    MZM_PSPRITE_GAMET_BLUE_LEADER = 157, MZM_PSPRITE_GAMET_BLUE_FOLLOWER = 158,
    MZM_PSPRITE_ZEBBO_GREEN_LEADER = 161, MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER = 162,
    MZM_PSPRITE_SPACE_PIRATE = 81, MZM_PSPRITE_SPACE_PIRATE_WAITING1 = 82,
    MZM_PSPRITE_SPACE_PIRATE_WAITING2 = 83, MZM_PSPRITE_SPACE_PIRATE_WAITING3 = 84,
    MZM_PSPRITE_SPACE_PIRATE2 = 85,
    MZM_PSPRITE_SECURITY_LASER_VERTICAL = 152, MZM_PSPRITE_SECURITY_LASER_HORIZONTAL = 153,
    MZM_PSPRITE_SECURITY_LASER_VERTICAL2 = 154, MZM_PSPRITE_SECURITY_LASER_HORIZONTAL2 = 155,
    MZM_PSPRITE_SEARCHLIGHT_EYE = 166, MZM_PSPRITE_SEARCHLIGHT_EYE2 = 167,
    MZM_PSPRITE_SEARCHLIGHT = 175, MZM_PSPRITE_SEARCHLIGHT2 = 176,
    MZM_PSPRITE_SEARCHLIGHT3 = 177, MZM_PSPRITE_SEARCHLIGHT4 = 178,
    MZM_PSPRITE_CHOZO_STATUE_LONG_HINT = 34, MZM_PSPRITE_CHOZO_STATUE_LONG = 35, MZM_PSPRITE_CHOZO_STATUE_ICE_HINT = 36, MZM_PSPRITE_CHOZO_STATUE_ICE = 37, MZM_PSPRITE_CHOZO_STATUE_WAVE_HINT = 38, MZM_PSPRITE_CHOZO_STATUE_WAVE = 39, MZM_PSPRITE_CHOZO_STATUE_BOMB_HINT = 40, MZM_PSPRITE_CHOZO_STATUE_BOMB = 41,
    MZM_PSPRITE_CHOZO_STATUE_SPEEDBOOSTER_HINT = 42, MZM_PSPRITE_CHOZO_STATUE_SPEEDBOOSTER = 43, MZM_PSPRITE_CHOZO_STATUE_HIGH_JUMP_HINT = 44, MZM_PSPRITE_CHOZO_STATUE_HIGH_JUMP = 45, MZM_PSPRITE_CHOZO_STATUE_SCREW_HINT = 46, MZM_PSPRITE_CHOZO_STATUE_SCREW = 47, MZM_PSPRITE_CHOZO_STATUE_VARIA_HINT = 48, MZM_PSPRITE_CHOZO_STATUE_VARIA = 49,
    MZM_PSPRITE_CHOZO_STATUE_GRAVITY = 88, MZM_PSPRITE_CHOZO_STATUE_SPACE_JUMP = 89,
    MZM_PSPRITE_CHOZO_STATUE_PLASMA_BEAM = 148,
};

/* SecondarySprite */
enum {
    MZM_SSPRITE_SKREE_EXPLOSION = 12, MZM_SSPRITE_BLUE_SKREE_EXPLOSION = 15,
    MZM_SSPRITE_CHOZO_BALL = 0, MZM_SSPRITE_CHOZO_STATUE_PART = 1,
    MZM_SSPRITE_CHOZO_STATUE_REFILL = 2, MZM_SSPRITE_CHOZO_STATUE_MOVEMENT = 4,
    MZM_SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_PART = 34, MZM_SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_REFILL = 35,
    MZM_SSPRITE_ZEELA_EYES = 16, MZM_SSPRITE_SPACE_PIRATE_LASER = 22,
    MZM_SSPRITE_MORPH_BALL_OUTSIDE = 19, MZM_SSPRITE_POWER_GRIP_GLOW = 70,
    MZM_SSPRITE_CHARGE_BEAM_GLOW = 5,
    MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2 = 44, MZM_SSPRITE_SEARCHLIGHT_EYE_PROJECTILE = 57,
};

typedef struct {
    uint16_t status;
    bool secondary;                 /* SP_SECONDARY_SPRITE: sprite_id is an SSPRITE */
    bool kill_off_screen;           /* SP_KILL_OFF_SCREEN */
    bool immune_to_projectiles;     /* SP_IMMUNE_TO_PROJECTILES */
    bool damaged;                   /* SP_DAMAGED: the AI plays its damaged sound */
    uint8_t sprite_id;              /* PSPRITE or SSPRITE */
    uint8_t gfx_slot, room_slot;
    uint16_t x, y;                  /* subpixels, bottom middle of the block */
    uint8_t pose;
    uint8_t anim;                   /* pOam: table index of the kind */
    uint8_t frame;                  /* currentAnimationFrame */
    uint8_t adc;                    /* animationDurationCounter */
    uint8_t work0, work1, work2, work3;
    uint16_t rotation;              /* 256 per turn, clockwise on screen (u8 natively) */
    uint16_t health;
    uint8_t palette_row, absolute_palette_row;
    int16_t hitbox_top, hitbox_bottom, hitbox_left, hitbox_right;
    uint8_t draw_top, draw_bottom, draw_horizontal;
    uint8_t samus_collision;
    uint8_t isft, freeze_timer, ignore_samus_timer;
    uint16_t y_spawn, x_spawn;      /* yPositionSpawn / xPositionSpawn (drop timers) */
    uint8_t ram_slot;               /* primarySpriteRamSlot: the owning primary's slot */
    uint16_t scaling;               /* also a saved Samus y for Zebs and Geegas */
    uint8_t standing_on;            /* standingOnSprite (MZM_STANDING_ON_SPRITE_*) */
    /* drawOrder (1 drawn in front; SpriteInitPrimary and
     * SpriteSpawnSecondary give 4) and SP_ALWAYS_ACTIVE. */
    uint8_t draw_order;
    bool always_active;
    /* The primary sprite whose graphics a secondary sprite uses (its
     * spawning sprite's). */
    uint8_t owner;
} MzmSprite;

/* SamusStandingOnSprite values; DROPPED (port only) is OFF after a kill
 * that natively drops Samus to STANDING_MIDAIR when she stood on the sprite
 * (mzm_sprites_samus_dropped applies it). */
enum {
    MZM_STANDING_ON_SPRITE_OFF,
    MZM_STANDING_ON_SPRITE_ON,
    MZM_STANDING_ON_SPRITE_RELEASING,
    MZM_STANDING_ON_SPRITE_DROPPED
};

/* gSamusData as SpriteUtilSamusStandingOnSprite sees and changes it:
 * `on_enemy` is standingStatus STANDING_ENEMY, `midair` STANDING_MIDAIR;
 * `y` (native subpixels) and `copy_y` (gSamusDataCopy.yPosition). Outputs:
 * `landing_request` (SamusSetPose(SPOSE_LANDING_REQUEST)) and `y_set`. */
typedef struct {
    bool on_enemy, midair;
    uint16_t y, copy_y;
    bool landing_request, y_set;
} MzmSpriteStanding;

/* EventFunction values read by the ported sprites (include/constants/event.h). */
enum {
    MZM_EVENT_VIEWED_STATUE_ROOM = 42, MZM_EVENT_LONG_BEAM_DESSGEEGA_KILLED = 43,
    MZM_EVENT_MECHA_RIDLEY_KILLED = 74,
};

/* Game systems outside the sprite engine. */
typedef struct {
    void *context;
    /* CHECK_EVENT / SET_EVENT */
    bool (*check_event)(void *context, uint8_t event);
    void (*set_event)(void *context, uint8_t event);
    /* gDoorUnlockTimer: 1 locks the room's doors (LOCK_DOORS), a negative
     * value unlocks them after that many frames. */
    void (*door_unlock_timer)(void *context, int8_t timer);
    /* ClipdataProcess with CAA_REMOVE_SOLID at a subpixel point. */
    void (*remove_solid)(void *context, uint16_t y, uint16_t x);
    /* BlockApplyCcaa at a subpixel point for a clipdata-affecting action of
     * the given damage type (MZM_DAMAGE_*): hatches and breakable blocks.
     * NULL: nothing is affected. */
    void (*affect)(void *context, uint16_t y, uint16_t x, int damage);
    /* ClipdataProcess with a clipdata-affecting action (MZM_CAA_*) at a
     * subpixel point; NULL: nothing changes. */
    void (*apply_caa)(void *context, uint16_t y, uint16_t x, uint8_t caa);
    /* SpriteSpawnPrimary(PSPRITE_MESSAGE_BANNER, message, ...); NULL: none. */
    void (*spawn_message)(void *context, uint8_t message);
} MzmSpriteHooks;

/* ALARM_TIMER_ACTIVE_TIMER: CONVERT_SECONDS(8.f). */
#define MZM_ALARM_TIMER_ACTIVE 480

/* Game globals the sprites keep between frames. */
typedef struct {
    uint16_t alarm_timer;           /* gAlarmTimer (the Chozodia alarm) */
    /* gSpriteDrawOrder[0..4], which the Space Pirates use as scratch flags
     * (aim, direction, Samus seen, below, right of the pirate); the sprite
     * drawing rewrites them every frame (mzm_sprites_end_frame clears them). */
    uint8_t draw_order[5];
    uint16_t boss_work2;            /* gBossWork.work2: the last pirate's x distance */
    /* gSubSpriteData1: the multi-sprite's position, table
     * (MZM_CHOZO_MULTI_*), frame, counter and work variables. */
    struct {
        uint16_t y, x;
        uint8_t multi, frame, adc, work2;
        bool work3;
    } sub1;
    /* gEnergyRefillAnimation, gMissileRefillAnimation,
     * gSuperMissileRefillAnimation and gPowerBombRefillAnimation. */
    uint8_t refill_animation[4];
} MzmSpriteGlobals;

/* gSamusData as the Chozo statues read and move it: her pose
 * (MZM_LINK_POSE_*) and native position; the statues request
 * SamusSetPose(SPOSE_GRABBED_BY_CHOZO_STATUE) (`grab`), SamusSetPose
 * (SPOSE_MORPH_BALL) (`release`) and a new position (`moved`), which also
 * update `pose`, `y` and `x` for the sprites that follow. */
enum { MZM_LINK_POSE_OTHER, MZM_LINK_POSE_MORPH_BALL, MZM_LINK_POSE_ROLLING,
       MZM_LINK_POSE_GRABBED };
typedef struct {
    uint8_t pose;
    uint16_t y, x;
    bool grab, release, moved;
} MzmSpriteSamusLink;

/* What SpriteUpdate shares with every sprite this frame. Enemy drops are
 * chosen from, and refill, the equipment (power bombs are not ported: their
 * counts are zero). */
typedef struct {
    const MzmSpriteWorld *world;
    const MzmSpriteTables *tables;
    /* Frame durations by family; NULL leaves the family's sprites idle. */
    const MzmSpriteKind *kinds[MZM_SPRITE_FAMILIES];
    MzmEquipment *equipment;
    uint8_t frame8;                 /* gFrameCounter8Bit */
    uint16_t frame16;               /* gFrameCounter16Bit */
    uint16_t samus_x, samus_y;      /* gSamusData position, subpixels */
    int16_t samus_hitbox_top;       /* gSamusPhysics.hitboxTop */
    /* gSpriteData, for SpriteSpawnSecondary (NULL: no spawning). */
    MzmSprite *sprites;
    int sprite_count;
    /* Particles and sprite debris the sprites set (NULL: none). */
    MzmEffects *effects;
    /* Game state some sprites read or change (NULL: no event set, nothing
     * changed). */
    const MzmSpriteHooks *hooks;
    /* Globals the Space Pirates need (NULL: they are not updated). */
    MzmSpriteGlobals *globals;
    /* SpriteUtilCheckCrouchingOrMorphed, gSamusData.xVelocity. */
    bool samus_crouching_or_morphed;
    int16_t samus_x_velocity;
    uint8_t area, room;             /* gCurrentArea / gCurrentRoom */
    /* Samus's standing state (NULL: SpriteUtilSamusStandingOnSprite does
     * nothing). */
    MzmSpriteStanding *standing;
    /* SpriteUtilCheckStopSpritesPose (gPreventMovementTimer): only sprites
     * still uninitialized run; the others only check if on screen. */
    bool stopped;
    /* gPreventMovementTimer (NULL: none); the Chozo balls set and read it. */
    uint16_t *prevent_movement;
    /* Samus for the Chozo statues' hands (NULL: no grab). */
    MzmSpriteSamusLink *samus_link;
} MzmSpriteFrame;

/* Samus as SpriteUtilSamusAndSpriteCollision sees her: subpixel position,
 * gPreviousXPosition, the gSamusPhysics hitbox, and the parts of her state
 * the contact rules read. `hurt` applies SpriteUtilTakeDamageFromSprite with
 * the sprite's stats damage (suit and difficulty reductions, the hurt pose)
 * and, when Samus survives, the knockback away from the sprite; like
 * SamusChangeToHurtPose it makes her invincible for the rest of the loop. */
typedef struct {
    void *context;
    uint16_t x, y, previous_x;
    int16_t hitbox_top, hitbox_bottom, hitbox_left, hitbox_right;
    bool screw_attacking, suitless, invincible;
    void (*hurt)(void *context, uint16_t damage, bool sprite_on_left);
    /* speedboostingShinesparking: contact kills as DCT_SPEEDBOOSTER, or as
     * DCT_SHINESPARK while shinesparking or ballsparking. */
    bool speedboosting, shinesparking;
    /* DCT_PSEUDO_SCREW: the damage of a charged spin (0 when not charged
     * or not spinning) and gSamusWeaponInfo.chargeCounter to reset. */
    uint16_t pseudo_screw_damage;
    uint8_t *charge_counter;
    /* Frozen sprites are solid: the room for SpriteUtilCheckCollisionAtPosition
     * (NULL: frozen sprites only take contact damage), gSamusData.yVelocity,
     * SpriteUtilCheckPullingSelfUp, SpriteUtilCheckMorphed, the facing
     * direction (1 right, -1 left) and a non-zero energy. The collision moves
     * `x` and `y` (native subpixels) and sets `stop_x` / `stop_y` where the
     * native zeroes gSamusData.xVelocity / yVelocity. */
    const MzmSpriteWorld *world;
    int16_t y_velocity;
    bool pulling_up, morphed;
    int8_t facing;
    bool has_energy;
    bool stop_x, stop_y;
} MzmSpriteSamus;

/* ClipdataProcess for an actor: the ClipdataType, plus the solid flag when
 * the point is in the solid part of the block (ClipdataConvertToCollision,
 * original version). Enemy-only, stop-enemy, tank and door blocks become air
 * or solid by actor. */
uint32_t mzm_clipdata_process(const MzmSpriteWorld *world, uint16_t y, uint16_t x, int actor);

/* SpriteInitPrimary for a sprite of a known PSPRITE at a block position. */
void mzm_sprite_init_primary(MzmSprite *sprite, uint8_t sprite_id, uint8_t gfx_slot,
                             uint16_t y_block, uint16_t x_block, uint8_t room_slot,
                             uint8_t ram_slot);
/* One SpriteUpdate step of sprite `slot`. */
void mzm_sprite_update(MzmSprite *sprite, uint8_t slot, const MzmSpriteFrame *frame);
/* The OAM table name of a sprite's animation, or NULL. */
const char *mzm_sprite_anim_name(const MzmSprite *sprite);
/* SpriteUtilSamusAndSpriteCollision over the sprite array (run at the start
 * of SpriteUpdate): at most one hurting or collecting contact per frame, then
 * every ignore timer runs down. */
void mzm_sprites_samus_collision(MzmSprite *sprites, int count, MzmSpriteSamus *samus,
                                 const MzmSpriteTables *tables);
/* ProjectileCheckHittingSprite for beams, missiles and super missiles (run
 * at the end of ProjectileUpdate): damage, stun and removal of the
 * projectile. Missiles bouncing off a sprite (tumbling) are removed. */
void mzm_sprites_projectile_hits(MzmSprite *sprites, int count, MzmWeapons *weapons,
                                 const MzmSpriteTables *tables, MzmEffects *effects);
/* SpriteDebrisInit (the cloud type is unused by the game). */
void mzm_sprite_debris_init(MzmEffects *effects, uint8_t type, uint16_t y, uint16_t x);
/* SpriteDebrisProcessAll (start of SpriteUpdate), then the vertical range
 * check of SpriteDebrisDraw against the camera. */
void mzm_sprite_debris_process(MzmEffects *effects, const MzmSpriteWorld *world,
                               const MzmSpriteTables *tables, const MzmEffectKinds *kinds);
/* The end of SpriteUpdate: DecrementChozodiaAlarm (its music and palette are
 * not ported), then the draw-order scratch the drawing rewrites. */
void mzm_sprites_end_frame(MzmSpriteGlobals *globals, const MzmSpriteHooks *hooks);
/* The kills that natively drop Samus standing on the sprite to
 * STANDING_MIDAIR: clears the DROPPED marks and returns true when one was
 * found. */
bool mzm_sprites_samus_dropped(MzmSprite *sprites, int count);
/* SpawnWaitingPirates at a room load through a door: with the alarm on (or
 * Mecha Ridley dead), a waiting pirate is spawned at the arrival position
 * (Samus's position, half a block back toward the door), shooting through
 * the door, or crawling out of a hatchless one; a spriteset without pirates
 * stops the alarm. `spriteset` lists the room spriteset's PSPRITE ids. */
void mzm_spawn_waiting_pirates(MzmSprite *sprites, int count, MzmSpriteGlobals *globals,
                               const MzmSpriteHooks *hooks, const uint8_t *spriteset,
                               int spriteset_count, uint8_t spriteset_index, uint16_t y,
                               uint16_t x, bool no_hatch);
/* Whether a PSPRITE has a ported AI. */
bool mzm_sprite_supported(uint8_t sprite_id);
/* The family of a sprite, or -1 without a ported AI. */
int mzm_sprite_family(uint8_t sprite_id, bool secondary);
/* The PSPRITE whose exported graphics and palette draw the sprite (a
 * secondary sprite draws with its parent's graphics slot). */
uint8_t mzm_sprite_graphics_owner(const MzmSprite *sprite);

#endif /* MZM_SPRITES_H */
