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
 * ripper2.c, squeept.c, gamet.c, skultera.c, dragon.c) with SpriteSpawnSecondary for the Skree's explosion and
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
#include "mzm_escape.h"
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
    /* gMusicTrackInfo.currentRoomTrack, replayed after a Chozo hint (0:
     * none). */
    uint16_t room_track;
    /* BGCNT_GET_PRIORITY(gIoRegistersBackup.BG1CNT). */
    uint8_t bg1_priority;
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
#define MZM_MULTI_FRAMES 24
#define MZM_MULTI_PARTS 12
typedef struct {
    uint8_t timers[MZM_MULTI_FRAMES];
    int16_t parts[MZM_MULTI_FRAMES][MZM_MULTI_PARTS][3];
} MzmMultiSprite;
enum { MZM_CHOZO_MULTI_STANDING, MZM_CHOZO_MULTI_SITTING, MZM_CHOZO_MULTI_SEATED,
       /* sUnknownItemChozoStatueMultiSpriteData_*: the same three, offset. */
       MZM_CHOZO_MULTI_UNKNOWN, MZM_CHOZO_MULTI_COUNT = MZM_CHOZO_MULTI_UNKNOWN * 2 };
extern const char *const mzm_chozo_multi_names[MZM_CHOZO_MULTI_COUNT];
/* sKraidMultiSpriteData_* */
enum { MZM_KRAID_MULTI_RISING, MZM_KRAID_MULTI_STANDING, MZM_KRAID_MULTI_STANDING_BETWEEN_STEPS,
       MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_RIGHT, MZM_KRAID_MULTI_MOVING_RIGHT_FEET_TO_RIGHT,
       MZM_KRAID_MULTI_MOVING_LEFT_FEET_TO_LEFT, MZM_KRAID_MULTI_MOVING_RIGHT_FEET_TO_LEFT,
       MZM_KRAID_MULTI_DYING1, MZM_KRAID_MULTI_DYING2, MZM_KRAID_MULTI_COUNT };
extern const char *const mzm_kraid_multi_names[MZM_KRAID_MULTI_COUNT];
/* sRidleyMultiSpriteData_* (gSubSpriteData1) and
 * sRidleyTailMultiSpriteData_* (gSubSpriteData2) */
enum { MZM_RIDLEY_MULTI_IDLE, MZM_RIDLEY_MULTI_SPITTING_FIREBALLS, MZM_RIDLEY_MULTI_TURNING_AROUND_FIRST_PART, MZM_RIDLEY_MULTI_TURNING_AROUND_SECOND_PART, MZM_RIDLEY_MULTI_TAIL_IDLE, MZM_RIDLEY_MULTI_TAIL_DYING, MZM_RIDLEY_MULTI_TAIL_MOVE_TO_ATTACK, MZM_RIDLEY_MULTI_TAIL_SETTING_UP_DIAGONAL, MZM_RIDLEY_MULTI_TAIL_CHARGING_DIAGONAL, MZM_RIDLEY_MULTI_TAIL_DIAGONAL, MZM_RIDLEY_MULTI_TAIL_CHARGING_VERTICAL, MZM_RIDLEY_MULTI_TAIL_VERTICAL, MZM_RIDLEY_MULTI_TAIL_LAST_VERTICAL, MZM_RIDLEY_MULTI_TAIL_BACK_TO_IDLE, MZM_RIDLEY_MULTI_COUNT };
extern const char *const mzm_ridley_multi_names[MZM_RIDLEY_MULTI_COUNT];
#define MZM_MOTHER_BRAIN_MULTI_NAME "sMotherBrainMultiSpriteData"
enum {
    MZM_IMAGO_MULTI_ATTACKING, MZM_IMAGO_MULTI_IDLE, MZM_IMAGO_MULTI_RETREATING,
    MZM_IMAGO_MULTI_DYING, MZM_IMAGO_MULTI_WARNING, MZM_IMAGO_MULTI_TAKING_DAMAGE,
    MZM_IMAGO_MULTI_COUNT,
};
extern const char *const mzm_imago_multi_names[MZM_IMAGO_MULTI_COUNT];
enum { MZM_IMAGO_COCOON_MULTI_IDLE, MZM_IMAGO_COCOON_MULTI_DYING,
       MZM_IMAGO_COCOON_MULTI_COUNT };
extern const char *const mzm_imago_cocoon_multi_names[MZM_IMAGO_COCOON_MULTI_COUNT];
enum {
    MZM_MECHA_RIDLEY_MULTI_CRAWLING_FORWARD_LOW,
    MZM_MECHA_RIDLEY_MULTI_STANDING_LOW,
    MZM_MECHA_RIDLEY_MULTI_STANDING_MIDDLE,
    MZM_MECHA_RIDLEY_MULTI_STANDING_HIGH,
    MZM_MECHA_RIDLEY_MULTI_LOW_TO_MIDDLE,
    MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_HIGH,
    MZM_MECHA_RIDLEY_MULTI_HIGH_TO_MIDDLE,
    MZM_MECHA_RIDLEY_MULTI_MIDDLE_TO_LOW,
    MZM_MECHA_RIDLEY_MULTI_LOW_TO_HIGH,
    MZM_MECHA_RIDLEY_MULTI_HIGH_TO_LOW,
    MZM_MECHA_RIDLEY_MULTI_CRAWLING_BACKWARDS_LOW,
    MZM_MECHA_RIDLEY_MULTI_OPENING_MOUTH_HIGH,
    MZM_MECHA_RIDLEY_MULTI_SPITTING_FIREBALLS_HIGH,
    MZM_MECHA_RIDLEY_MULTI_CLOSING_MOUTH_HIGH,
    MZM_MECHA_RIDLEY_MULTI_DYING_STANDING_LOW,
    MZM_MECHA_RIDLEY_MULTI_DYING_STANDING_MIDDLE,
    MZM_MECHA_RIDLEY_MULTI_DYING_STANDING_HIGH,
    MZM_MECHA_RIDLEY_MULTI_DYING,
    MZM_MECHA_RIDLEY_MULTI_COUNT
};
extern const char *const mzm_mecha_ridley_multi_names[MZM_MECHA_RIDLEY_MULTI_COUNT];

/* ROM tables of scripts/mzm_sprites.py (metadata/sprites.tsv). */
typedef struct {
    uint8_t random[32];             /* sSpriteRandomNumberTable */
    uint8_t generic_random[256];    /* sRandomNumberTable */
    int16_t falling[16];            /* sSpritesFallingSpeed */
    uint16_t health[256], damage[256];  /* sPrimarySpriteStats */
    uint8_t weakness[256];          /* SpriteWeakness (low byte of column 2) */
    /* The freeze flag (high byte): frozen when gFrameCounter8Bit & flag is 0. */
    uint8_t freeze[256], secondary_freeze[256];
    uint16_t drops[256][5];         /* drop probabilities (columns 4-8), out of 1024 */
    uint16_t secondary_damage[256]; /* sSecondarySpriteStats */
    uint16_t secondary_health[256];
    uint8_t secondary_weakness[256];
    uint16_t secondary_drops[256][5];
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
    int16_t dragon_fireball_y[40];  /* sDragonFireballYMovement (SHORT_MAX ends it) */
    int16_t dragon_fireball_rotation[40]; /* sDragonFireballOamRotation */
    int16_t pirate_wall_jump[8];    /* sSpacePirateWallJumpingVelocity */
    int16_t mellow_idle_y[65];      /* sMellowIdleYVelocity (SHORT_MAX ends it) */
    int16_t mellow_idle_x[81];      /* sMellowIdleXVelocity (SHORT_MAX ends it) */
    int16_t baristute_jumping_y[10]; /* sBaristuteJumpingYVelocity */
    int16_t baristute_falling_y[8]; /* sBaristuteFallingYVelocity (SHORT_MAX ends it) */
    int16_t pirate_jump_low[11];    /* sSpacePirate_2e1030 */
    int16_t pirate_jump[10];        /* sSpacePirateJumpingVelocity */
    int16_t pirate_jump_high[11];   /* sSpacePirate_2e105a */
    int16_t gunship_taking_off[153]; /* sGunshipTakingOffYVelocity (SHORT_MAX ends it) */
    int16_t gunship_flying[22];     /* sGunshipFlyingYVelocity (SHORT_MAX ends it) */
    int16_t atomic_idle_y[65];      /* sAtomicIdleYMovement (SHORT_MAX ends it) */
    int16_t atomic_idle_x[81];      /* sAtomicIdleXMovement */
    int16_t polyp_projectile_y[44]; /* sPolypProjectileYVelocity (SHORT_MAX ends it) */
    int16_t worker_robot_sleeping_falling[10]; /* sWorkerRobotSleepingFallingSpeed (SHORT_MAX ends it) */
    int16_t metroid_spawning_y[65]; /* sMetroidSpawningYMovement (SHORT_MAX restarts it) */
    int16_t metroid_spawning_x[81]; /* sMetroidSpawningXMovement (SHORT_MAX restarts it) */
    /* sChozoStatueMultiSpriteData_Standing, _Sitting and _Seated. */
    MzmMultiSprite chozo_multi[MZM_CHOZO_MULTI_COUNT];
    MzmMultiSprite kraid_multi[MZM_KRAID_MULTI_COUNT];
    int16_t kraid_palette[128];     /* sKraidPal (BGR555) */
    MzmMultiSprite ridley_multi[MZM_RIDLEY_MULTI_COUNT];
    int16_t ridley_idle_y[65];      /* sRidleyIdleYOffsets (SHORT_MAX restarts it) */
    int16_t ridley_spawning_y[81];  /* sRidleySpawningSpittingFireballsYOffsets */
    int16_t sine[320];              /* sSineTable (one turn plus a cosine quarter) */
    int16_t acid_worm_spit_close[16]; /* sAcidWormSpitCloseYVelocity */
    int16_t acid_worm_spit_far[26];   /* sAcidWormSpitFarYVelocity */
    int16_t acid_worm_spit_middle[20]; /* sAcidWormSpitMiddleYVelocity */
    int16_t acid_worm_swinging[33];   /* sAcidWormSwingingMovement */
    int16_t acid_worm_head_x[45];     /* sAcidWormHeadRandomXVelocity */
    int16_t deorem_segment_x[33];   /* sDeoremSegmentXVelocity */
    int16_t deorem_thorn_y[40];     /* sDeoremThornYVelocity */
    MzmMultiSprite imago_larva_multi[MZM_IMAGO_MULTI_COUNT];
    MzmMultiSprite imago_cocoon_multi[MZM_IMAGO_COCOON_MULTI_COUNT];
    MzmMultiSprite mother_brain_multi;  /* sMotherBrainMultiSpriteData */
    MzmMultiSprite tangle_vine_multi;   /* sTangleVineGerutaMultiSpriteData_Idle */
    MzmMultiSprite mecha_ridley_multi[MZM_MECHA_RIDLEY_MULTI_COUNT];
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
#define MZM_SPRITE_MAX_ANIMS 128  /* the room gadgets use 102 */
#define MZM_SPRITE_MAX_FRAMES 64   /* sMultiviolaOam_Moving has 56 */
extern const char *const mzm_zoomer_anim_names[MZM_ZOOMER_ANIMS];

/* Enemy drop OAM tables (src/data/sprites/enemy_drop.c), one per drop. */
enum {
    MZM_DROP_SMALL_ENERGY, MZM_DROP_LARGE_ENERGY, MZM_DROP_MISSILE,
    MZM_DROP_SUPER_MISSILE, MZM_DROP_POWER_BOMB,
    MZM_DROP_MULTIPLE_LARGE_ENERGY_SLOT1, MZM_DROP_MULTIPLE_LARGE_ENERGY_SLOT2,
    MZM_DROP_MULTIPLE_LARGE_ENERGY_SLOT3, MZM_DROP_ANIMS,
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
/* Dragon OAM tables (src/data/sprites/dragon.c). */
enum {
    MZM_DRAGON_IDLE, MZM_DRAGON_WARNING, MZM_DRAGON_SPITTING, MZM_DRAGON_TURNING_AROUND,
    MZM_DRAGON_TURNING_AROUND_PART2, MZM_DRAGON_FIREBALL_MOVING, MZM_DRAGON_FIREBALL_EXPLODING,
    MZM_DRAGON_ANIMS,
};
extern const char *const mzm_dragon_anim_names[MZM_DRAGON_ANIMS];
/* Room gadget OAM tables (src/data/sprites/elevator_pad.c, particle_data.c
 * for the water drop, steam.c, geron.c, geron_norfair.c,
 * unknown_item_block.c). */
enum {
    MZM_GADGET_ELEVATOR_PAD_IDLE, MZM_GADGET_ELEVATOR_PAD_MOVING,
    MZM_GADGET_WATER_DROP_SPAWNING, MZM_GADGET_WATER_DROP_FALLING, MZM_GADGET_WATER_DROP_SPLASHING,
    MZM_GADGET_STEAM_VERTICAL_LARGE, MZM_GADGET_STEAM_VERTICAL_SMALL,
    MZM_GADGET_STEAM_HORIZONTAL_LARGE, MZM_GADGET_STEAM_HORIZONTAL_SMALL,
    MZM_GADGET_STEAM_DIAGONAL_LARGE, MZM_GADGET_STEAM_DIAGONAL_SMALL,
    MZM_GADGET_GERON_IDLE, MZM_GADGET_GERON_SHAKING, MZM_GADGET_GERON_GETTING_DESTROYED,
    MZM_GADGET_GERON_DESTROYED, MZM_GADGET_GERON_NORFAIR_IDLE,
    MZM_GADGET_UNKNOWN_ITEM_BLOCK_IDLE, MZM_GADGET_UNKNOWN_ITEM_BLOCK_ACTIVATED,
    MZM_GADGET_UNKNOWN_ITEM_BLOCK_EXPLODING, MZM_GADGET_UNKNOWN_ITEM_BLOCK_LIGHT,
    MZM_GADGET_MAP_STATION_IDLE, MZM_GADGET_MAP_STATION_GRABBING_SAMUS,
    MZM_GADGET_MAP_STATION_DOWNLOADING, MZM_GADGET_MAP_STATION_RETRACTING,
    MZM_GADGET_MAP_STATION_INACTIVE, MZM_GADGET_MAP_STATION_BACK_SCREEN_IDLE,
    MZM_GADGET_MAP_STATION_BACK_SCREEN_DOWNLOADING, MZM_GADGET_MAP_STATION_BACK_SCREEN_DOWNLOADED,
    MZM_GADGET_MAP_STATION_FRONT_SCREEN_IDLE, MZM_GADGET_MAP_STATION_FRONT_SCREEN_DOWNLOADING,
    MZM_GADGET_MAP_STATION_FRONT_SCREEN_DOWNLOADED,
    MZM_GADGET_SAVE_PLATFORM_IDLE,
    MZM_GADGET_SAVE_PLATFORM_OPENING,
    MZM_GADGET_SAVE_PLATFORM_OPENED,
    MZM_GADGET_SAVE_PLATFORM_SAVING,
    MZM_GADGET_SAVE_PLATFORM_OPENED_OFF,
    MZM_GADGET_SAVE_PLATFORM_CLOSING,
    MZM_GADGET_SAVE_PLATFORM_IDLE_OFF,
    MZM_GADGET_SAVE_PLATFORM_TOP_IDLE,
    MZM_GADGET_SAVE_PLATFORM_TOP_EXTENDING,
    MZM_GADGET_SAVE_PLATFORM_TOP_EXTENDED,
    MZM_GADGET_SAVE_PLATFORM_TOP_RETRACTING,
    MZM_GADGET_SAVE_PLATFORM_TUBE_SPAWNING,
    MZM_GADGET_SAVE_PLATFORM_TUBE_SHADOW,
    MZM_GADGET_SAVE_PLATFORM_TUBE_IDLE,
    MZM_GADGET_SAVE_PLATFORM_TUBE_DESPAWNING,
    MZM_GADGET_SAVE_PLATFORM_RAY,
    MZM_GADGET_GUNSHIP_IDLE,
    MZM_GADGET_GUNSHIP_REFILLING,
    MZM_GADGET_GUNSHIP_FLYING,
    MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_CLOSED,
    MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_OPENING,
    MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_OPENED,
    MZM_GADGET_GUNSHIP_ENTRANCE_FRONT_CLOSING,
    MZM_GADGET_GUNSHIP_ENTRANCE_BACK_CLOSED,
    MZM_GADGET_GUNSHIP_ENTRANCE_BACK_OPENING,
    MZM_GADGET_GUNSHIP_ENTRANCE_BACK_OPENED,
    MZM_GADGET_GUNSHIP_ENTRANCE_BACK_CLOSING,
    MZM_GADGET_GUNSHIP_PLATFORM,
    MZM_GADGET_GUNSHIP_FLAMES_HORIZONTAL,
    MZM_GADGET_GUNSHIP_FLAMES_VERTICAL,
    MZM_GADGET_AREA_BANNER_SPAWN_MIDDLE,
    MZM_GADGET_AREA_BANNER_STATIC_MIDDLE,
    MZM_GADGET_AREA_BANNER_REMOVING_MIDDLE,
    MZM_GADGET_AREA_BANNER_SPAWN_BOTTOM,
    MZM_GADGET_AREA_BANNER_STATIC_BOTTOM,
    MZM_GADGET_AREA_BANNER_REMOVING_BOTTOM,
    MZM_GADGET_MORPH_BALL_LAUNCHER_IDLE,
    MZM_GADGET_MORPH_BALL_LAUNCHER_LAUNCHING,
    MZM_GADGET_MORPH_BALL_LAUNCHER_BACK,
    MZM_GADGET_MORPH_BALL_LAUNCHER_ENERGY,
    MZM_GADGET_ZIPLINE_OFF_IDLE,
    MZM_GADGET_ZIPLINE_OFF_GRABBING_SAMUS,
    MZM_GADGET_ZIPLINE_OFF_SAMUS_GRABBED,
    MZM_GADGET_ZIPLINE_OFF_RELEASING_SAMUS,
    MZM_GADGET_ZIPLINE_ON_IDLE,
    MZM_GADGET_ZIPLINE_ON_GRABBING_SAMUS,
    MZM_GADGET_ZIPLINE_ON_SAMUS_GRABBED,
    MZM_GADGET_ZIPLINE_ON_RELEASING_SAMUS,
    MZM_GADGET_ZIPLINE_BUTTON_OFF_IDLE,
    MZM_GADGET_ZIPLINE_BUTTON_ON_IDLE,
    MZM_GADGET_ZIPLINE_BUTTON_ACTIVE,
    MZM_GADGET_ZIPLINE_GENERATOR_DEACTIVATED,
    MZM_GADGET_ZIPLINE_GENERATOR_ACTIVATING,
    MZM_GADGET_ZIPLINE_GENERATOR_ACTIVATED,
    MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_DEACTIVATED,
    MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_ACTIVATING,
    MZM_GADGET_ZIPLINE_GENERATOR_CONDUCTOR_ACTIVATED,
    MZM_GADGET_ZIPLINE_GENERATOR_MORPH_SYMBOL_ACTIVATING,
    MZM_GADGET_ZIPLINE_GENERATOR_MORPH_SYMBOL_ACTIVATED,
    MZM_GADGET_ZIPLINE_GENERATOR_ELECTRICITY_ACTIVATING,
    MZM_GADGET_SECURITY_GATE_OPENED,
    MZM_GADGET_SECURITY_GATE_CLOSING,
    MZM_GADGET_SECURITY_GATE_CLOSED,
    MZM_GADGET_SECURITY_GATE_OPENING,
    MZM_GADGET_PISTON_IDLE,
    MZM_GADGET_PISTON_OPENING,
    MZM_GADGET_PISTON_OPENED,
    MZM_GADGET_GLASS_TUBE_INTACT,
    MZM_GADGET_GLASS_TUBE_CRACKING,
    MZM_GADGET_GLASS_TUBE_BREAKING,
    MZM_GADGET_GLASS_TUBE_BROKEN,
    MZM_GADGET_CHOZO_PILLAR_PLATFORM_NO_SHADOW_SPAWNING,
    MZM_GADGET_CHOZO_PILLAR_PLATFORM_NO_SHADOW_SPAWNED,
    MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW_SPAWNING,
    MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW_SPAWNED,
    MZM_GADGET_CHOZO_PILLAR_PLATFORM_SHADOW,
    /* The Chozodia save platform: the save platform's tables in the same
     * order, then the refill light. */
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_IDLE,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_OPENING,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_OPENED,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_SAVING,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_OPENED_OFF,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_CLOSING,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_IDLE_OFF,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_TOP_IDLE,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_TOP_EXTENDING,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_TOP_EXTENDED,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_TOP_RETRACTING,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_TUBE_SPAWNING,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_TUBE_SHADOW,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_TUBE_IDLE,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_TUBE_DESPAWNING,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_RAY,
    MZM_GADGET_SAVE_PLATFORM_CHOZODIA_REFILL_LIGHT,
    MZM_GADGET_ANIMS,
};
extern const char *const mzm_gadget_anim_names[MZM_GADGET_ANIMS];
/* Parasite OAM tables (src/data/sprites/parasite.c). */
enum {
    MZM_PARASITE_IDLE, MZM_PARASITE_LANDING_AFTER_FALLING, MZM_PARASITE_LANDING,
    MZM_PARASITE_TUMBLING, MZM_PARASITE_TURNING_BACK_ON_FEET, MZM_PARASITE_TURNING_AROUND,
    MZM_PARASITE_JUMPING_UP, MZM_PARASITE_ATTACHED, MZM_PARASITE_EXPELLED, MZM_PARASITE_DYING,
    MZM_PARASITE_ANIMS,
};
extern const char *const mzm_parasite_anim_names[MZM_PARASITE_ANIMS];
/* Atomic OAM tables (src/data/sprites/atomic.c). */
enum {
    MZM_ATOMIC_IDLE, MZM_ATOMIC_ELECTRICITY_CHARGING, MZM_ATOMIC_ELECTRICITY_MOVING_VERTICAL,
    MZM_ATOMIC_ELECTRICITY_MOVING_DIAGONAL, MZM_ATOMIC_ELECTRICITY_MOVING_HORIZONTAL,
    MZM_ATOMIC_ELECTRICITY_EXPLODING_NON_DIAGONAL, MZM_ATOMIC_ELECTRICITY_EXPLODING_DIAGONAL,
    MZM_ATOMIC_ANIMS,
};
extern const char *const mzm_atomic_anim_names[MZM_ATOMIC_ANIMS];
/* Polyp OAM tables (src/data/sprites/polyp.c). */
enum {
    MZM_POLYP_IDLE, MZM_POLYP_WARNING, MZM_POLYP_SPITTING, MZM_POLYP_AFTER_SPITTING,
    MZM_POLYP_RETRACTING, MZM_POLYP_PROJECTILE_RIGHT, MZM_POLYP_PROJECTILE_LEFT,
    MZM_POLYP_PROJECTILE_EXPLODING, MZM_POLYP_ANIMS,
};
extern const char *const mzm_polyp_anim_names[MZM_POLYP_ANIMS];
/* Worker Robot OAM tables (src/data/sprites/worker_robot.c). */
enum {
    MZM_WORKER_ROBOT_WALKING, MZM_WORKER_ROBOT_WALKING_BACKWARDS, MZM_WORKER_ROBOT_STANDING,
    MZM_WORKER_ROBOT_BACK_TO_SLEEP, MZM_WORKER_ROBOT_SLEEPING, MZM_WORKER_ROBOT_GOING_TO_SLEEP,
    MZM_WORKER_ROBOT_TURNING_AROUND, MZM_WORKER_ROBOT_WAKING_UP, MZM_WORKER_ROBOT_ANIMS,
};
extern const char *const mzm_worker_robot_anim_names[MZM_WORKER_ROBOT_ANIMS];
/* Kraid OAM tables in KraidOam order (src/data/sprites/kraid.c), so the
 * multi-sprite data's OAM indices are these. */
enum {
    MZM_KRAID_MOUTH_CLOSED,
    MZM_KRAID_MOUTH_CLOSED_BLINK,
    MZM_KRAID_OPENING_MOUTH,
    MZM_KRAID_MOUTH_OPENED,
    MZM_KRAID_RISING,
    MZM_KRAID_CLOSING_MOUTH,
    MZM_KRAID_PART_2CAC5C,
    MZM_KRAID_LEFT_ARM_IDLE,
    MZM_KRAID_LEFT_ARM_DYING,
    MZM_KRAID_LEFT_ARM_THROWING_NAILS,
    MZM_KRAID_PART_2CADC4,
    MZM_KRAID_RIGHT_ARM_IDLE,
    MZM_KRAID_RIGHT_ARM_ATTACKING,
    MZM_KRAID_RIGHT_ARM_DYING,
    MZM_KRAID_LEFT_FEET_RISING,
    MZM_KRAID_LEFT_FEET_IDLE_1,
    MZM_KRAID_LEFT_FEET_MOVING_RIGHT,
    MZM_KRAID_LEFT_FEET_IDLE_2,
    MZM_KRAID_LEFT_FEET_MOVED_RIGHT,
    MZM_KRAID_LEFT_FEET_MOVING_LEFT,
    MZM_KRAID_LEFT_FEET_MOVED_LEFT,
    MZM_KRAID_RIGHT_FEET_RISING,
    MZM_KRAID_RIGHT_FEET_IDLE_1,
    MZM_KRAID_RIGHT_FEET_MOVED_RIGHT,
    MZM_KRAID_RIGHT_FEET_IDLE_2,
    MZM_KRAID_RIGHT_FEET_MOVING_RIGHT,
    MZM_KRAID_RIGHT_FEET_MOVED_LEFT,
    MZM_KRAID_RIGHT_FEET_MOVING_LEFT,
    MZM_KRAID_TOP_HOLE_LEFT,
    MZM_KRAID_TOP_HOLE_RIGHT,
    MZM_KRAID_MIDDLE_HOLE_LEFT,
    MZM_KRAID_MIDDLE_HOLE_RIGHT,
    MZM_KRAID_BOTTOM_HOLE_LEFT,
    MZM_KRAID_BOTTOM_HOLE_RIGHT,
    MZM_KRAID_NAIL,
    MZM_KRAID_2CB29C,
    MZM_KRAID_2CB2AC,
    MZM_KRAID_SPIKE,
    MZM_KRAID_ANIMS,
};
extern const char *const mzm_kraid_anim_names[MZM_KRAID_ANIMS];
/* Ridley OAM tables in RidleyOam order (src/data/sprites/ridley.c). */
enum {
    MZM_RIDLEY_IDLE,
    MZM_RIDLEY_SPITTING_FIREBALLS,
    MZM_RIDLEY_TURNING_AROUND_FIRST_PART,
    MZM_RIDLEY_TURNING_AROUND_SECOND_PART,
    MZM_RIDLEY_HEAD_IDLE,
    MZM_RIDLEY_OPENING_MOUTH,
    MZM_RIDLEY_MOUTH_OPENED,
    MZM_RIDLEY_HEAD_DYING,
    MZM_RIDLEY_HEAD_TURNING_AROUND,
    MZM_RIDLEY_CLAW_IDLE,
    MZM_RIDLEY_CLAW_SPITTING_FIREBALLS,
    MZM_RIDLEY_CLAW_TURNING_AROUND_FIRST_PART,
    MZM_RIDLEY_CLAW_TURNING_AROUND_SECOND_PART,
    MZM_RIDLEY_CLAW_CARRYING_SAMUS,
    MZM_RIDLEY_CLAW_LIFTING_SAMUS,
    MZM_RIDLEY_CLAW_SAMUS_LIFTED,
    MZM_RIDLEY_CLAW_RELEASING_SAMUS,
    MZM_RIDLEY_LEFT_WING_IDLE,
    MZM_RIDLEY_RIGHT_WING_IDLE,
    MZM_RIDLEY_LEFT_WING_UNUSED,
    MZM_RIDLEY_RIGHT_WING_UNUSED,
    MZM_RIDLEY_LEFT_WING_SPITTING_FIREBALLS,
    MZM_RIDLEY_RIGHT_WING_SPITTING_FIREBALLS,
    MZM_RIDLEY_TAIL_PART,
    MZM_RIDLEY_TAIL_TIP_POINTING_DOWN,
    MZM_RIDLEY_TAIL_TIP_POINTING_UP,
    MZM_RIDLEY_TAIL_TIP_POINTING_DIAGONALLY_DOWN_RIGHT,
    MZM_RIDLEY_TAIL_TIP_POINTING_DIAGONALLY_UP_RIGHT,
    MZM_RIDLEY_TAIL_TIP_POINTING_DIAGONALLY_DOWN_LEFT,
    MZM_RIDLEY_TAIL_TIP_POINTING_DIAGONALLY_UP_LEFT,
    MZM_RIDLEY_SQUARE,
    MZM_RIDLEY_FIREBALL_SMALL,
    MZM_RIDLEY_FIREBALL_BIG,
    MZM_RIDLEY_ANIMS,
};
extern const char *const mzm_ridley_anim_names[MZM_RIDLEY_ANIMS];
/* Mother Brain OAM tables in MotherBrainOam order (src/data/sprites/
 * mother_brain.c), then the eye closing and glass tables, and the block's
 * (src/data/sprites/zebetite_and_cannon.c). */
enum {
    MZM_MOTHER_BRAIN_IDLE, MZM_MOTHER_BRAIN_CHARGING_BEAM, MZM_MOTHER_BRAIN_EYE_CLOSED,
    MZM_MOTHER_BRAIN_PART_2FA934, MZM_MOTHER_BRAIN_EYE_OPENING, MZM_MOTHER_BRAIN_PART_2FA984,
    MZM_MOTHER_BRAIN_EYE_DYING, MZM_MOTHER_BRAIN_BOTTOM, MZM_MOTHER_BRAIN_BEAM_SPAWNING,
    MZM_MOTHER_BRAIN_BEAM_MOVING, MZM_MOTHER_BRAIN_EYE_CLOSING, MZM_MOTHER_BRAIN_GLASS_BREAKING,
    MZM_MOTHER_BRAIN_BLOCK, MZM_MOTHER_BRAIN_ANIMS,
};
extern const char *const mzm_mother_brain_anim_names[MZM_MOTHER_BRAIN_ANIMS];
/* Metroid OAM tables (src/data/sprites/metroid.c). */
enum {
    MZM_METROID_SPAWNING, MZM_METROID_MOVING, MZM_METROID_SAMUS_GRABBED, MZM_METROID_SHELL_IDLE,
    MZM_METROID_ANIMS,
};
extern const char *const mzm_metroid_anim_names[MZM_METROID_ANIMS];
/* Tourian's Zebetites and cannons (src/data/sprites/zebetite_and_cannon.c),
 * the cannons' aims in CannonAim order. */
enum {
    MZM_TOURIAN_ZEBETITE_PHASE1,
    MZM_TOURIAN_ZEBETITE_PHASE2,
    MZM_TOURIAN_ZEBETITE_PHASE4,
    MZM_TOURIAN_ZEBETITE_PHASE6,
    MZM_TOURIAN_CANNON_AIMING_LEFT,
    MZM_TOURIAN_CANNON_SHOOTING_LEFT,
    MZM_TOURIAN_CANNON_LEFT_TO_DOWN_LEFT_TRANSITION,
    MZM_TOURIAN_CANNON_AIMING_DOWN_LEFT,
    MZM_TOURIAN_CANNON_SHOOTING_DOWN_LEFT,
    MZM_TOURIAN_CANNON_DOWN_LEFT_TO_DOWN_TRANSITION,
    MZM_TOURIAN_CANNON_AIMING_DOWN,
    MZM_TOURIAN_CANNON_SHOOTING_DOWN,
    MZM_TOURIAN_CANNON_DOWN_TO_DOWN_RIGHT_TRANSITION,
    MZM_TOURIAN_CANNON_AIMING_DOWN_RIGHT,
    MZM_TOURIAN_CANNON_SHOOTING_DOWN_RIGHT,
    MZM_TOURIAN_CANNON_DOWN_RIGHT_TO_RIGHT_TRANSITION,
    MZM_TOURIAN_CANNON_AIMING_RIGHT,
    MZM_TOURIAN_CANNON_SHOOTING_RIGHT,
    MZM_TOURIAN_CANNON_BULLET_LEFT,
    MZM_TOURIAN_CANNON_BULLET_DOWN_LEFT,
    MZM_TOURIAN_CANNON_BULLET_DOWN,
    MZM_TOURIAN_CANNON_BULLET_DOWN_RIGHT,
    MZM_TOURIAN_CANNON_BULLET_RIGHT,
    MZM_TOURIAN_ANIMS,
};
extern const char *const mzm_tourian_anim_names[MZM_TOURIAN_ANIMS];
/* Skultera OAM tables (src/data/sprites/skultera.c). */
enum { MZM_SKULTERA_IDLE, MZM_SKULTERA_TURNING_AROUND, MZM_SKULTERA_CHASING_SAMUS, MZM_SKULTERA_ANIMS };
extern const char *const mzm_skultera_anim_names[MZM_SKULTERA_ANIMS];
/* Squeept OAM tables (src/data/sprites/squeept.c). */
enum { MZM_SQUEEPT_GOING_UP, MZM_SQUEEPT_TURNING_AROUND, MZM_SQUEEPT_GOING_DOWN, MZM_SQUEEPT_ANIMS };
extern const char *const mzm_squeept_anim_names[MZM_SQUEEPT_ANIMS];

/* Rinka OAM tables (src/data/sprites/rinka.c). */
enum {
    MZM_RINKA_ORANGE_SPAWNING, MZM_RINKA_ORANGE_MOVING, MZM_RINKA_GREEN_SPAWNING,
    MZM_RINKA_GREEN_MOVING, MZM_RINKA_MOTHER_BRAIN_SPAWNING, MZM_RINKA_MOTHER_BRAIN_MOVING,
    MZM_RINKA_ANIMS,
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

/* The two Brinstar boss statues (src/data/sprites/boss_statues.c). */
enum {
    MZM_BOSS_STATUE_KRAID_IDLE, MZM_BOSS_STATUE_KRAID_ACTIVATING,
    MZM_BOSS_STATUE_KRAID_ACTIVATED, MZM_BOSS_STATUE_KRAID_OPENING,
    MZM_BOSS_STATUE_KRAID_OPENED, MZM_BOSS_STATUE_RIDLEY_IDLE,
    MZM_BOSS_STATUE_RIDLEY_ACTIVATING, MZM_BOSS_STATUE_RIDLEY_ACTIVATED,
    MZM_BOSS_STATUE_RIDLEY_OPENING, MZM_BOSS_STATUE_RIDLEY_OPENED,
    MZM_BOSS_STATUE_ANIMS,
};
extern const char *const mzm_boss_statue_anim_names[MZM_BOSS_STATUE_ANIMS];

/* Deorem, its body segments, eye and thorns (src/data/sprites/deorem.c). */
enum {
    MZM_DEOREM_SEGMENT_MIDDLE, MZM_DEOREM_SEGMENT_JUNCTION, MZM_DEOREM_SEGMENT_TAIL,
    MZM_DEOREM_THORN_IDLE, MZM_DEOREM_EYE_IDLE, MZM_DEOREM_EYE_PULSING,
    MZM_DEOREM_CLOSED_SLOW, MZM_DEOREM_OPENING, MZM_DEOREM_OPENED_SLOW,
    MZM_DEOREM_CLOSING, MZM_DEOREM_WARNING, MZM_DEOREM_GOING_DOWN,
    MZM_DEOREM_OPENED_FAST, MZM_DEOREM_CLOSED_FAST, MZM_DEOREM_DYING,
    MZM_DEOREM_ANIMS,
};
extern const char *const mzm_deorem_anim_names[MZM_DEOREM_ANIMS];

/* Acid Worm, its body parts and spit (src/data/sprites/acid_worm.c). */
enum {
    MZM_ACID_WORM_IDLE, MZM_ACID_WORM_MOVING, MZM_ACID_WORM_SPITTING,
    MZM_ACID_WORM_MOUTH_CLOSED, MZM_ACID_WORM_PART_AROUND_MOUTH,
    MZM_ACID_WORM_PART_WEAK_POINT, MZM_ACID_WORM_PART_UNUSED,
    MZM_ACID_WORM_PART_BELOW_WEAK_POINT, MZM_ACID_WORM_PART_ABOVE_SEGMENTS,
    MZM_ACID_WORM_PART_SEGMENT, MZM_ACID_WORM_PART_UNUSED2,
    MZM_ACID_WORM_SPIT_MOVING, MZM_ACID_WORM_SPIT_EXPLODING,
    MZM_ACID_WORM_WARNING, MZM_ACID_WORM_SPAWN_ON_TOP, MZM_ACID_WORM_ANIMS,
};
extern const char *const mzm_acid_worm_anim_names[MZM_ACID_WORM_ANIMS];

/* Kraid and Ridley's Gadoras, their invisible eye and Ridley's beam. */
enum {
    MZM_GADORA_EYE_CLOSED, MZM_GADORA_WARNING, MZM_GADORA_OPENING_EYE,
    MZM_GADORA_BEAM_MOVING, MZM_GADORA_EYE_MOVING, MZM_GADORA_CLOSING_EYE,
    MZM_GADORA_DEATH, MZM_GADORA_EYE_OPENED, MZM_GADORA_BEFORE_CLOSING,
    MZM_GADORA_ANIMS,
};
extern const char *const mzm_gadora_anim_names[MZM_GADORA_ANIMS];

/* The Imago larva body and its claws, shell and three glowing dots. */
enum {
    MZM_IMAGO_SHELL_ATTACKING, MZM_IMAGO_SHELL_IDLE, MZM_IMAGO_SHELL_RETREATING,
    MZM_IMAGO_SHELL_DYING, MZM_IMAGO_CLAWS_ATTACKING, MZM_IMAGO_CLAWS_IDLE,
    MZM_IMAGO_CLAWS_RETREATING, MZM_IMAGO_CLAWS_TAKING_DAMAGE,
    MZM_IMAGO_LEFT_DOT_APPEARING, MZM_IMAGO_MIDDLE_DOT_APPEARING,
    MZM_IMAGO_RIGHT_DOT_APPEARING, MZM_IMAGO_LEFT_DOT_VISIBLE,
    MZM_IMAGO_MIDDLE_DOT_VISIBLE, MZM_IMAGO_RIGHT_DOT_VISIBLE,
    MZM_IMAGO_LEFT_DOT_DISAPPEARING, MZM_IMAGO_MIDDLE_DOT_DISAPPEARING,
    MZM_IMAGO_RIGHT_DOT_DISAPPEARING, MZM_IMAGO_SHELL_WARNING,
    MZM_IMAGO_IDLE, MZM_IMAGO_CLAWS_WARNING_FIRST,
    MZM_IMAGO_CLAWS_WARNING_SECOND, MZM_IMAGO_WARNING,
    MZM_IMAGO_SHELL_TAKING_DAMAGE, MZM_IMAGO_LARVA_ANIMS,
};
extern const char *const mzm_imago_larva_anim_names[MZM_IMAGO_LARVA_ANIMS];

/* The Imago cocoon, its vines, spores, ripper and persistent remains. */
enum {
    MZM_IMAGO_COCOON_CEILING_VINE_MOTIONLESS,
    MZM_IMAGO_COCOON_CEILING_VINE_MOVING,
    MZM_IMAGO_COCOON_CEILING_VINE_BROKEN,
    MZM_IMAGO_COCOON_DECORATIVE_VINE_LEFT,
    MZM_IMAGO_COCOON_DECORATIVE_VINE_RIGHT,
    MZM_IMAGO_COCOON_VINE_LEFT_MIDDLE,
    MZM_IMAGO_COCOON_VINE_RIGHT_MIDDLE,
    MZM_IMAGO_COCOON_VINE_LEFT_RIGHT,
    MZM_IMAGO_COCOON_VINE_RIGHT_RIGHT,
    MZM_IMAGO_COCOON_VINE_LEFT_LEFT,
    MZM_IMAGO_COCOON_VINE_RIGHT_LEFT,
    MZM_IMAGO_COCOON_IDLE,
    MZM_IMAGO_COCOON_UNKNOWN,
    MZM_IMAGO_COCOON_STATIC,
    MZM_IMAGO_COCOON_SPORE_SPAWNING,
    MZM_IMAGO_COCOON_SPORE_NEST,
    MZM_IMAGO_COCOON_SPORE_MOVING,
    MZM_IMAGO_COCOON_SPORE_EXPLODING,
    MZM_IMAGO_COCOON_WINGED_RIPPER_MOVING,
    MZM_IMAGO_COCOON_WINGED_RIPPER_TURNING,
    MZM_IMAGO_COCOON_DEFEATED,
    MZM_IMAGO_COCOON_ANIMS,
};
extern const char *const mzm_imago_cocoon_anim_names[MZM_IMAGO_COCOON_ANIMS];

/* The animated Imago larva remnant on the right side of the passage. */
enum { MZM_IMAGO_LARVA_RIGHT_SIDE_IDLE, MZM_IMAGO_LARVA_RIGHT_SIDE_ANIMS };
extern const char *const
    mzm_imago_larva_right_side_anim_names[MZM_IMAGO_LARVA_RIGHT_SIDE_ANIMS];

/* sMechaRidleyFrameDataPointers, shared by the body and every part. */
enum {
    MZM_MECHA_RIDLEY_TAIL, MZM_MECHA_RIDLEY_RING, MZM_MECHA_RIDLEY_COVER,
    MZM_MECHA_RIDLEY_TORSO, MZM_MECHA_RIDLEY_CORE_IDLE, MZM_MECHA_RIDLEY_HEAD_IDLE,
    MZM_MECHA_RIDLEY_HEAD_OPENING_MOUTH, MZM_MECHA_RIDLEY_HEAD_SPITTING_FIREBALLS,
    MZM_MECHA_RIDLEY_HEAD_CLOSING_MOUTH, MZM_MECHA_RIDLEY_EYE_IDLE,
    MZM_MECHA_RIDLEY_EYE_GLOWING, MZM_MECHA_RIDLEY_EYE_LASER_FORWARD,
    MZM_MECHA_RIDLEY_EYE_LASER_SLIGHTLY_DOWN, MZM_MECHA_RIDLEY_EYE_LASER_DOWN,
    MZM_MECHA_RIDLEY_EYE_LASER_SLIGHTLY_UP, MZM_MECHA_RIDLEY_EYE_LASER_UP,
    MZM_MECHA_RIDLEY_EYE_INACTIVE, MZM_MECHA_RIDLEY_LEFT_ARM_CRAWLING_FORWARD,
    MZM_MECHA_RIDLEY_RIGHT_ARM_CRAWLING_FORWARD, MZM_MECHA_RIDLEY_LEFT_ARM_HOLDING_UP,
    MZM_MECHA_RIDLEY_RIGHT_ARM_HOLDING_UP, MZM_MECHA_RIDLEY_LEFT_ARM_HELD_UP,
    MZM_MECHA_RIDLEY_RIGHT_ARM_HELD_UP, MZM_MECHA_RIDLEY_LEFT_ARM_LAYING_DOWN,
    MZM_MECHA_RIDLEY_RIGHT_ARM_LAYING_DOWN, MZM_MECHA_RIDLEY_LEFT_ARM_TREMBLING,
    MZM_MECHA_RIDLEY_RIGHT_ARM_TREMBLING, MZM_MECHA_RIDLEY_LEFT_ARM_IDLE,
    MZM_MECHA_RIDLEY_RIGHT_ARM_IDLE, MZM_MECHA_RIDLEY_LEFT_ARM_SWINGING,
    MZM_MECHA_RIDLEY_NECK_LOW, MZM_MECHA_RIDLEY_NECK_MIDDLE, MZM_MECHA_RIDLEY_NECK_HIGH,
    MZM_MECHA_RIDLEY_NECK_ROTATE_LOW, MZM_MECHA_RIDLEY_NECK_ROTATE_MIDDLE,
    MZM_MECHA_RIDLEY_NECK_ROTATE_HIGH, MZM_MECHA_RIDLEY_NECK_LOW_TO_MIDDLE,
    MZM_MECHA_RIDLEY_NECK_MIDDLE_TO_HIGH, MZM_MECHA_RIDLEY_NECK_HIGH_TO_MIDDLE,
    MZM_MECHA_RIDLEY_NECK_MIDDLE_TO_LOW, MZM_MECHA_RIDLEY_NECK_LOW_TO_HIGH,
    MZM_MECHA_RIDLEY_NECK_HIGH_TO_LOW, MZM_MECHA_RIDLEY_NECK_LOW_2,
    MZM_MECHA_RIDLEY_NECK_MIDDLE_2, MZM_MECHA_RIDLEY_NECK_HIGH_2,
    MZM_MECHA_RIDLEY_LAUNCHER_CLOSED, MZM_MECHA_RIDLEY_LAUNCHER_OPENING,
    MZM_MECHA_RIDLEY_LAUNCHER_OPENED, MZM_MECHA_RIDLEY_LAUNCHER_CLOSING,
    MZM_MECHA_RIDLEY_MISSILE, MZM_MECHA_RIDLEY_FIREBALL, MZM_MECHA_RIDLEY_LASER_FORWARD,
    MZM_MECHA_RIDLEY_LASER_SLIGHTLY_DOWN, MZM_MECHA_RIDLEY_LASER_DOWN,
    MZM_MECHA_RIDLEY_LASER_SLIGHTLY_UP, MZM_MECHA_RIDLEY_LASER_UP,
    MZM_MECHA_RIDLEY_LEFT_ARM_DYING, MZM_MECHA_RIDLEY_RIGHT_ARM_DYING,
    MZM_MECHA_RIDLEY_NECK_DYING, MZM_MECHA_RIDLEY_LEFT_ARM_CRAWLING_BACKWARDS,
    MZM_MECHA_RIDLEY_RIGHT_ARM_CRAWLING_BACKWARDS,
    MZM_MECHA_RIDLEY_LEFT_ARM_SWINGING_AT_GROUND,
    MZM_MECHA_RIDLEY_LEFT_ARM_SWINGING_AT_CLOSE_GROUND,
    /* Not in sMechaRidleyFrameDataPointers: set by the dying eye. */
    MZM_MECHA_RIDLEY_EYE_DYING,
    MZM_MECHA_RIDLEY_ANIMS,
};
extern const char *const mzm_mecha_ridley_anim_names[MZM_MECHA_RIDLEY_ANIMS];

/* The Brinstar hives, their roots and the Mellows (sprites_ai/hive.c). */
enum {
    MZM_HIVE_MELLOW_IDLE, MZM_HIVE_MELLOW_SAMUS_DETECTED, MZM_HIVE_IDLE, MZM_HIVE_ROOTS_IDLE,
    MZM_HIVE_PHASE2, MZM_HIVE_PHASE3, MZM_HIVE_DYING, MZM_HIVE_ANIMS,
};
extern const char *const mzm_hive_anim_names[MZM_HIVE_ANIMS];

/* The Baristutes (sprites_ai/baristute.c). */
enum {
    MZM_BARISTUTE_IDLE, MZM_BARISTUTE_WARNING, MZM_BARISTUTE_JUMPING, MZM_BARISTUTE_LANDING,
    MZM_BARISTUTE_WALKING_SLOW, MZM_BARISTUTE_WALKING_FAST, MZM_BARISTUTE_WALKING_VERY_FAST,
    MZM_BARISTUTE_WALKING_EXTREMELY_FAST, MZM_BARISTUTE_ANIMS,
};
extern const char *const mzm_baristute_anim_names[MZM_BARISTUTE_ANIMS];

/* The Tangle Vines, in sTangleVineFrameDataPointers order, then the two
 * larva vines (sprites_ai/tangle_vine.c). */
enum {
    MZM_TANGLE_VINE_TALL_ON_GROUND, MZM_TANGLE_VINE_MEDIUM_ON_GROUND,
    MZM_TANGLE_VINE_SHORT_ON_GROUND, MZM_TANGLE_VINE_CURVED_ON_GROUND,
    MZM_TANGLE_VINE_TALL_ON_CEILING, MZM_TANGLE_VINE_MEDIUM_ON_CEILING,
    MZM_TANGLE_VINE_SHORT_ON_CEILING, MZM_TANGLE_VINE_CURVED_ON_CEILING,
    MZM_TANGLE_VINE_GERUTA_GRIP, MZM_TANGLE_VINE_GERUTA_ROOT, MZM_TANGLE_VINE_GERUTA_FULL,
    MZM_TANGLE_VINE_RED_GERUTA, MZM_TANGLE_VINE_GERUTA,
    MZM_TANGLE_VINE_LARVA_RIGHT, MZM_TANGLE_VINE_LARVA_LEFT, MZM_TANGLE_VINE_ANIMS,
};
extern const char *const mzm_tangle_vine_anim_names[MZM_TANGLE_VINE_ANIMS];

/* The Ruins Test (Chozo ghost) and its actors (sprites_ai/ruins_test.c). */
enum {
    MZM_RUINS_TEST_SAMUS_REFLECTION,
    MZM_RUINS_TEST_GHOST_OUTLINE_SPAWNING,
    MZM_RUINS_TEST_GHOST_NOT_MOVING,
    MZM_RUINS_TEST_GHOST_MOVING,
    MZM_RUINS_TEST_GHOST_SYMBOL_PLACED,
    MZM_RUINS_TEST_SYMBOL_FOUR_SYMBOLS,
    MZM_RUINS_TEST_SYMBOL_MERGING,
    MZM_RUINS_TEST_SYMBOL_DISAPPEARING,
    MZM_RUINS_TEST_TURNING_INTO_REFLECTION,
    MZM_RUINS_TEST_REFLECTION_COVER,
    MZM_RUINS_TEST_SYMBOL,
    MZM_RUINS_TEST_SHOOTABLE_SYMBOL_SPAWNING,
    MZM_RUINS_TEST_SHOOTABLE_SYMBOL_SPAWNED,
    MZM_RUINS_TEST_SHOOTABLE_SYMBOL_DESPAWNING,
    MZM_RUINS_TEST_SHOOTABLE_SYMBOL_TURNING_INTO_SYMBOL,
    MZM_RUINS_TEST_GHOST_SYMBOL_SHOT,
    MZM_RUINS_TEST_GHOST_SYMBOL_PLACING,
    MZM_RUINS_TEST_DESPAWN,
    MZM_RUINS_TEST_UNUSED,
    MZM_RUINS_TEST_SPAWNING,
    MZM_RUINS_TEST_LIGHTNING_IN_AIR,
    MZM_RUINS_TEST_LIGHTNING_GOING_ON_GROUND,
    MZM_RUINS_TEST_LIGHTNING_ON_GROUND_HORIZONTAL,
    MZM_RUINS_TEST_LIGHTNING_ON_GROUND_VERTICAL,
    MZM_RUINS_TEST_GHOST_OUTLINE_SHOOTING_GROUND_LIGHTNING,
    MZM_RUINS_TEST_ANIMS,
};
extern const char *const mzm_ruins_test_anim_names[MZM_RUINS_TEST_ANIMS];

/* Chozodia's scripted sprites: the falling Chozo pillar and the Power
 * Bomb theft (sprites_ai/falling_chozo_pillar.c and
 * space_pirate_carrying_power_bomb.c). */
enum {
    MZM_CHOZODIA_PILLAR_FALLING, MZM_CHOZODIA_FAKE_POWER_BOMB,
    MZM_CHOZODIA_PIRATE_CARRYING_POWER_BOMB, MZM_CHOZODIA_SCRIPTED_ANIMS,
};
extern const char *const mzm_chozodia_scripted_anim_names[MZM_CHOZODIA_SCRIPTED_ANIMS];
#define MZM_TANGLE_VINE_MULTI_NAME "sTangleVineGerutaMultiSpriteData_Idle"

/* Sprites sharing an AI and its OAM tables. `library_sprites` names, per
 * table, the exported sprite whose frame durations the family uses. */
enum {
    MZM_FAMILY_ZOOMER, MZM_FAMILY_ZEELA, MZM_FAMILY_RIPPER, MZM_FAMILY_SKREE, MZM_FAMILY_WAVER,
    MZM_FAMILY_DROP, MZM_FAMILY_SIDEHOPPER, MZM_FAMILY_DESSGEEGA, MZM_FAMILY_VIOLA,
    MZM_FAMILY_SOVA, MZM_FAMILY_ZEB, MZM_FAMILY_GEEGA, MZM_FAMILY_ZEBBO, MZM_FAMILY_REO,
    MZM_FAMILY_MELLA, MZM_FAMILY_HOLTZ, MZM_FAMILY_MULTIVIOLA, MZM_FAMILY_RINKA,
    MZM_FAMILY_SPACE_PIRATE, MZM_FAMILY_CHOZODIA_SECURITY, MZM_FAMILY_CHOZO_STATUE,
    MZM_FAMILY_GERUTA, MZM_FAMILY_RIPPER2, MZM_FAMILY_SQUEEPT, MZM_FAMILY_GAMET,
    MZM_FAMILY_ITEM_SPRITE, MZM_FAMILY_SKULTERA, MZM_FAMILY_DRAGON, MZM_FAMILY_GADGET,
    MZM_FAMILY_PARASITE, MZM_FAMILY_ATOMIC, MZM_FAMILY_POLYP, MZM_FAMILY_TOURIAN,
    MZM_FAMILY_WORKER_ROBOT, MZM_FAMILY_METROID, MZM_FAMILY_KRAID, MZM_FAMILY_RIDLEY,
    MZM_FAMILY_MOTHER_BRAIN, MZM_FAMILY_BOSS_STATUE, MZM_FAMILY_DEOREM,
    MZM_FAMILY_ACID_WORM, MZM_FAMILY_GADORA, MZM_FAMILY_IMAGO_LARVA,
    MZM_FAMILY_IMAGO_LARVA_RIGHT_SIDE, MZM_FAMILY_IMAGO_COCOON,
    MZM_FAMILY_MECHA_RIDLEY, MZM_FAMILY_HIVE, MZM_FAMILY_BARISTUTE,
    MZM_FAMILY_TANGLE_VINE, MZM_FAMILY_RUINS_TEST,
    MZM_FAMILY_CHOZODIA_SCRIPTED,
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
/* Every family's OAM tables fit a kind. */
_Static_assert(MZM_GADGET_ANIMS <= MZM_SPRITE_MAX_ANIMS, "room gadget OAM tables overflow MzmSpriteKind");
_Static_assert(MZM_PIRATE_ANIMS <= MZM_SPRITE_MAX_ANIMS, "Space Pirate OAM tables overflow MzmSpriteKind");
_Static_assert(MZM_SECURITY_ANIMS <= MZM_SPRITE_MAX_ANIMS, "security OAM tables overflow MzmSpriteKind");
_Static_assert(MZM_MECHA_RIDLEY_ANIMS <= MZM_SPRITE_MAX_ANIMS,
               "Mecha Ridley OAM tables overflow MzmSpriteKind");

enum {
    MZM_PSPRITE_UNUSED0 = 0,
    MZM_PSPRITE_ZOOMER_YELLOW = 18, MZM_PSPRITE_ZOOMER_RED = 19,
    MZM_PSPRITE_ZEELA = 20, MZM_PSPRITE_ZEELA_RED = 21,
    MZM_PSPRITE_RIPPER_BROWN = 22, MZM_PSPRITE_RIPPER_PURPLE = 23,
    MZM_PSPRITE_LARGE_ENERGY_DROP = 26, MZM_PSPRITE_SMALL_ENERGY_DROP = 27,
    MZM_PSPRITE_MISSILE_DROP = 28, MZM_PSPRITE_SUPER_MISSILE_DROP = 29,
    MZM_PSPRITE_POWER_BOMB_DROP = 30,
    MZM_PSPRITE_MULTIPLE_LARGE_ENERGY = 53,
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
    MZM_PSPRITE_KRAID_STATUE = 163, MZM_PSPRITE_RIDLEY_STATUE = 164,
    MZM_PSPRITE_REO_GREEN_WINGS = 63, MZM_PSPRITE_REO_PURPLE_WINGS = 64,
    MZM_PSPRITE_MELLA = 114, MZM_PSPRITE_HOLTZ = 107, MZM_PSPRITE_MULTIVIOLA = 52,
    MZM_PSPRITE_RINKA_ORANGE = 102, MZM_PSPRITE_RINKA_GREEN = 165,
    MZM_PSPRITE_RINKA_MOTHER_BRAIN = 124, MZM_PSPRITE_RINKA_MOTHER_BRAIN2 = 201,
    MZM_PSPRITE_RINKA_MOTHER_BRAIN3 = 202, MZM_PSPRITE_RINKA_MOTHER_BRAIN4 = 203,
    MZM_PSPRITE_RINKA_MOTHER_BRAIN5 = 204, MZM_PSPRITE_RINKA_MOTHER_BRAIN6 = 205,
    MZM_PSPRITE_GERUTA_RED = 54, MZM_PSPRITE_GERUTA_GREEN = 55, MZM_PSPRITE_RIPPERII = 113,
    MZM_PSPRITE_SQUEEPT = 56,
    MZM_PSPRITE_SKULTERA = 69, MZM_PSPRITE_DRAGON = 59,
    MZM_PSPRITE_ELEVATOR_PAD = 80, MZM_PSPRITE_WATER_DROP = 187, MZM_PSPRITE_STEAM_LARGE = 168,
    MZM_PSPRITE_STEAM_SMALL = 169, MZM_PSPRITE_STEAM_LARGE_DIAGONAL_UP = 191,
    MZM_PSPRITE_STEAM_SMALL_DIAGONAL_UP = 192, MZM_PSPRITE_STEAM_LARGE_DIAGONAL_DOWN = 193,
    MZM_PSPRITE_STEAM_SMALL_DIAGONAL_DOWN = 194, MZM_PSPRITE_GERON_BRINSTAR_ROOM_15 = 140,
    MZM_PSPRITE_GERON_BRINSTAR_ROOM_1C = 141, MZM_PSPRITE_GERON_VARIA1 = 142,
    MZM_PSPRITE_GERON_VARIA2 = 143, MZM_PSPRITE_GERON_VARIA3 = 144, MZM_PSPRITE_GERON_NORFAIR = 106,
    MZM_PSPRITE_PLASMA_BEAM_BLOCK = 170, MZM_PSPRITE_GRAVITY_SUIT_BLOCK = 171,
    MZM_PSPRITE_SPACE_JUMP_BLOCK = 172, MZM_PSPRITE_MAP_STATION = 58, MZM_PSPRITE_SAVE_PLATFORM = 110, MZM_PSPRITE_GUNSHIP = 65, MZM_PSPRITE_AREA_BANNER = 116, MZM_PSPRITE_MORPH_BALL_LAUNCHER = 78, MZM_PSPRITE_ZIPLINE = 61, MZM_PSPRITE_ZIPLINE_BUTTON = 62, MZM_PSPRITE_ZIPLINE_GENERATOR = 99, MZM_PSPRITE_SECURITY_GATE_DEFAULT_OPEN = 90, MZM_PSPRITE_SECURITY_GATE_DEFAULT_CLOSED = 98, MZM_PSPRITE_PISTON = 96, MZM_PSPRITE_GLASS_TUBE = 145, MZM_PSPRITE_RISING_CHOZO_PILLAR = 151, MZM_PSPRITE_PARASITE = 95, MZM_PSPRITE_SAVE_PLATFORM_CHOZODIA = 146, MZM_PSPRITE_ATOMIC = 115, MZM_PSPRITE_POLYP = 103, MZM_PSPRITE_KRAID = 111, MZM_PSPRITE_RIDLEY = 97, MZM_PSPRITE_MOTHER_BRAIN = 117, MZM_PSPRITE_METROID = 100, MZM_PSPRITE_FROZEN_METROID = 101, MZM_PSPRITE_WORKER_ROBOT = 93, MZM_PSPRITE_CANNON = 126, MZM_PSPRITE_CANNON2 = 136, MZM_PSPRITE_CANNON3 = 137, MZM_PSPRITE_ZEBETITE_ONE_AND_THREE = 125, MZM_PSPRITE_ZEBETITE_TWO_AND_FOUR = 135, MZM_PSPRITE_PARASITE_MULTIPLE = 94, MZM_PSPRITE_MORPH_BALL = 33, MZM_PSPRITE_POWER_GRIP = 76, MZM_PSPRITE_CHARGE_BEAM = 68, MZM_PSPRITE_DEOREM_FIRST_LOCATION = 66, MZM_PSPRITE_DEOREM_SECOND_LOCATION = 67, MZM_PSPRITE_ACID_WORM = 119,
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
    MZM_PSPRITE_GADORA_KRAID = 173, MZM_PSPRITE_GADORA_RIDLEY = 174,
    MZM_PSPRITE_IMAGO_LARVA_RIGHT = 77, MZM_PSPRITE_IMAGO_COCOON = 79,
    MZM_PSPRITE_MELLOW = 74, MZM_PSPRITE_HIVE = 75, MZM_PSPRITE_MELLOW_SWARM = 132,
    MZM_PSPRITE_MELLOW_SWARM_HEALTH_BASED = 133, MZM_PSPRITE_BARISTUTE = 147,
    MZM_PSPRITE_BARISTUTE_KRAID_UPPER = 195, MZM_PSPRITE_BARISTUTE_KRAID_LOWER = 200,
    MZM_PSPRITE_TANGLE_VINE_TALL = 128, MZM_PSPRITE_TANGLE_VINE_MEDIUM = 129,
    MZM_PSPRITE_TANGLE_VINE_CURVED = 130, MZM_PSPRITE_TANGLE_VINE_SHORT = 131,
    MZM_PSPRITE_TANGLE_VINE_RED_GERUTA = 183, MZM_PSPRITE_TANGLE_VINE_GERUTA = 184,
    MZM_PSPRITE_TANGLE_VINE_LARVA_RIGHT = 185, MZM_PSPRITE_TANGLE_VINE_LARVA_LEFT = 186,
    MZM_PSPRITE_RUINS_TEST = 109, MZM_PSPRITE_FAKE_POWER_BOMB_EVENT_TRIGGER = 118,
    MZM_PSPRITE_FAKE_POWER_BOMB = 181, MZM_PSPRITE_SPACE_PIRATE_CARRYING_POWER_BOMB = 182,
    MZM_PSPRITE_FALLING_CHOZO_PILLAR = 188,
    MZM_PSPRITE_IMAGO_COCOON_AFTER_FIGHT = 112,
    MZM_PSPRITE_IMAGO_LARVA_RIGHT_SIDE = 127,
    MZM_PSPRITE_IMAGO_LARVA_LEFT = 139,
    MZM_PSPRITE_DISCOVERED_IMAGO_PASSAGE_EVENT_TRIGGER = 180,
    MZM_PSPRITE_MECHA_RIDLEY = 189,
};

/* SecondarySprite */
enum {
    MZM_SSPRITE_SKREE_EXPLOSION = 12, MZM_SSPRITE_BLUE_SKREE_EXPLOSION = 15,
    MZM_SSPRITE_CHOZO_BALL = 0, MZM_SSPRITE_CHOZO_STATUE_PART = 1,
    MZM_SSPRITE_CHOZO_STATUE_REFILL = 2, MZM_SSPRITE_CHOZO_STATUE_MOVEMENT = 4,
    MZM_SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_PART = 34, MZM_SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_REFILL = 35,
    MZM_SSPRITE_ZEELA_EYES = 16, MZM_SSPRITE_SPACE_PIRATE_LASER = 22,
    MZM_SSPRITE_MORPH_BALL_OUTSIDE = 19, MZM_SSPRITE_POWER_GRIP_GLOW = 70,
    MZM_SSPRITE_CHARGE_BEAM_GLOW = 5, MZM_SSPRITE_DRAGON_FIREBALL = 8,
    MZM_SSPRITE_UNKNOWN_ITEM_BLOCK_LIGHT = 56, MZM_SSPRITE_MAP_STATION_PART = 52,
    MZM_SSPRITE_SAVE_PLATFORM_PART = 13, MZM_SSPRITE_GUNSHIP_PART = 50, MZM_SSPRITE_MORPH_BALL_LAUNCHER_PART = 36,
    MZM_SSPRITE_ZIPLINE_GENERATOR_PART = 30, MZM_SSPRITE_CHOZO_PILLAR_PLATFORM = 53,
    MZM_SSPRITE_CHOZO_PILLAR_PLATFORM_SHADOW = 58, MZM_SSPRITE_CHOZODIA_SAVE_PLATFORM_PART = 46,
    MZM_SSPRITE_ATOMIC_ELECTRICITY = 31, MZM_SSPRITE_POLYP_PROJECTILE = 27, MZM_SSPRITE_CANNON_BULLET = 39, MZM_SSPRITE_KRAID_PART = 3, MZM_SSPRITE_KRAID_SPIKE = 28,
    MZM_SSPRITE_KRAID_NAIL = 29, MZM_SSPRITE_RIDLEY_PART = 23, MZM_SSPRITE_RIDLEY_TAIL = 24,
    MZM_SSPRITE_RIDLEY_FIREBALL = 33, MZM_SSPRITE_RIDLEY_BIG_FIREBALL = 67,
    MZM_SSPRITE_MOTHER_BRAIN_PART = 32, MZM_SSPRITE_MOTHER_BRAIN_BEAM = 74,
    MZM_SSPRITE_MOTHER_BRAIN_BLOCK = 75, MZM_SSPRITE_MOTHER_BRAIN_GLASS_BREAKING = 76, MZM_SSPRITE_METROID_SHELL = 26,
    MZM_SSPRITE_DEOREM_SEGMENT = 9, MZM_SSPRITE_DEOREM_EYE = 10, MZM_SSPRITE_DEOREM_THORN = 11,
    MZM_SSPRITE_ACID_WORM_PART = 37, MZM_SSPRITE_ACID_WORM_SPIT = 38,
    MZM_SSPRITE_GADORA_EYE = 54, MZM_SSPRITE_GADORA_BEAM = 55,
    MZM_SSPRITE_IMAGO_LARVA_PART = 18, MZM_SSPRITE_HIVE_ROOTS = 17,
    MZM_SSPRITE_TANGLE_VINE_GERUTA_PART = 45,
    MZM_SSPRITE_RUINS_TEST_SYMBOL = 59, MZM_SSPRITE_RUINS_TEST_SAMUS_REFLECTION_START = 60,
    MZM_SSPRITE_RUINS_TEST_REFLECTION_COVER = 61, MZM_SSPRITE_RUINS_TEST_GHOST_OUTLINE = 62,
    MZM_SSPRITE_RUINS_TEST_GHOST = 63, MZM_SSPRITE_RUINS_TEST_SHOOTABLE_SYMBOL = 64,
    MZM_SSPRITE_RUINS_TEST_SAMUS_REFLECTION_END = 65, MZM_SSPRITE_RUINS_TEST_LIGHTNING = 66,
    MZM_SSPRITE_WINGED_RIPPER = 6, MZM_SSPRITE_IMAGO_COCOON_VINE = 20,
    MZM_SSPRITE_IMAGO_COCOON_SPORE = 21, MZM_SSPRITE_DEFEATED_IMAGO_COCOON = 42,
    MZM_SSPRITE_IMAGO_CEILING_VINE = 43,
    MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2 = 44, MZM_SSPRITE_SEARCHLIGHT_EYE_PROJECTILE = 57,
    MZM_SSPRITE_MECHA_RIDLEY_PART = 68, MZM_SSPRITE_MECHA_RIDLEY_LASER = 71,
    MZM_SSPRITE_MECHA_RIDLEY_MISSILE = 72, MZM_SSPRITE_MECHA_RIDLEY_FIREBALL = 73,
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
    /* SP_ABSOLUTE_POSITION: x and y are GBA screen pixels (the area
     * banner). */
    bool absolute;
    /* SP_SOLID_FOR_PROJECTILES: every projectile only hits it (the
     * ziplines and their buttons). */
    bool solid_for_projectiles;
    /* frozenPaletteRowOffset: the frozen and stun rows sit this many rows
     * lower (the Mother Brain Rinkas). */
    uint8_t frozen_row_offset;
    /* bgPriority BGCNT_LOW_PRIORITY: drawn behind the backgrounds above
     * BG3 (Kraid's right arm and feet). */
    bool low_bg_priority;
    /* Explicit native bgPriority plus one; zero keeps SpriteInit's default
     * priority 2. This encoding preserves zero-initialized test actors. */
    uint8_t bg_priority_override;
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
 * `landing_request` (SamusSetPose(SPOSE_LANDING_REQUEST)), `y_set`, and the
 * carried `x`/`x_set` pair used by horizontally moving platforms. */
typedef struct {
    bool on_enemy, midair;
    uint16_t y, copy_y;
    bool landing_request, y_set;
    uint16_t x;
    bool x_set;
} MzmSpriteStanding;

/* EventFunction values read by the ported sprites (include/constants/event.h). */
enum {
    MZM_EVENT_EASY = 1, MZM_EVENT_HARD = 2,
    MZM_EVENT_ENTER_RIDLEY_DEMO_PLAYED = 5,
    MZM_EVENT_VIEWED_STATUE_ROOM = 42, MZM_EVENT_LONG_BEAM_DESSGEEGA_KILLED = 43,
    MZM_EVENT_MECHA_RIDLEY_KILLED = 74, MZM_EVENT_ZIPLINES_ACTIVATED = 46,
    MZM_EVENT_FULLY_POWERED_SUIT_OBTAINED = 67, MZM_EVENT_GLASS_TUBE_BROKEN = 73,
    MZM_EVENT_POWER_GRIP_OBTAINED = 16, MZM_EVENT_CHOZO_PILLAR_FULLY_EXTENDED = 17,
    MZM_EVENT_MOTHER_BRAIN_KILLED = 39, MZM_EVENT_ZEBETITE_ONE_DESTROYED = 61,
    /* EVENT_FIRST_, THIRD_, FIFTH_, SECOND_, SIXTH_ and FOURTH_METROID_ROOM_CLEARED
     * follow in that order. */
    MZM_EVENT_FIRST_METROID_ROOM_CLEARED = 55, MZM_EVENT_KRAID_KILLED = 30,
    MZM_EVENT_RIDLEY_KILLED = 37, MZM_EVENT_GRAVITY_SUIT_OBTAINED = 23, MZM_EVENT_ESCAPED_ZEBES = 65,
    MZM_EVENT_ESCAPED_CHOZODIA = 75,
    MZM_EVENT_DEOREM_FIRST_LOCATION = 24, MZM_EVENT_DEOREM_SECOND_LOCATION = 25,
    MZM_EVENT_DEOREM_KILLED_AT_SECOND_LOCATION = 26,
    MZM_EVENT_ACID_WORM_KILLED = 27,
    MZM_EVENT_KRAID_GADORA_KILLED = 28, MZM_EVENT_RIDLEY_GADORA_KILLED = 36,
    MZM_EVENT_THREE_HIVES_DESTROYED = 44, MZM_EVENT_KRAID_BARISTUTES_KILLED = 52,
    MZM_EVENT_POWER_BOMB_STOLEN = 70, MZM_EVENT_SPACE_PIRATE_WITH_POWER_BOMB_ONE = 71,
    MZM_EVENT_SPACE_PIRATE_WITH_POWER_BOMB_TWO = 72,
    MZM_EVENT_CATERPILLAR_KILLED = 31, MZM_EVENT_IMAGO_TUNNEL_DISCOVERED = 32,
    MZM_EVENT_IMAGO_COCOON_KILLED = 33,
    MZM_EVENT_KRAID_STATUE_OPENED = 53, MZM_EVENT_RIDLEY_STATUE_OPENED = 54,
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
    /* BgClipSetBgBlockValue: a background's block value at a block
     * position (Mother Brain's glass on BG0); NULL: none. */
    void (*set_bg_block)(void *context, uint8_t bg, uint16_t value, uint16_t y, uint16_t x);
    /* CLEAR_EVENT, used by Deorem when it moves between its two rooms. */
    void (*clear_event)(void *context, uint8_t event);
    /* gEffectYPositionOffset: move the room liquid surface in subpixels. */
    void (*move_effect_y)(void *context, int16_t delta);
    /* GadoraHatchUpdate: enable or suppress the closed hatch at a point. */
    void (*set_hatch_exists)(void *context, uint16_t y, uint16_t x, bool exists);
} MzmSpriteHooks;

/* ALARM_TIMER_ACTIVE_TIMER: CONVERT_SECONDS(8.f). */
#define MZM_ALARM_TIMER_ACTIVE 480

typedef struct {
    uint16_t y, x;
    uint8_t multi, frame, adc;
    uint8_t work1, work2, work3, work4, work5, work6;
    uint16_t health;
} MzmSubSpriteState;

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
    MzmSubSpriteState sub1;
    /* gSubSpriteData2: Ridley's tail or the left Imago larva. */
    MzmSubSpriteState sub2;
    /* gBossWork.work1 and work3 (Ridley's spawn y and damage taken). */
    uint16_t boss_work1, boss_work3, boss_work4, boss_work5;
    /* gBossWork.work6..work10 (Mecha Ridley: missiles and fireballs alive,
     * the health before the last retreat check, core and cover spawn
     * health). */
    uint16_t boss_work6, boss_work7, boss_work8, boss_work9, boss_work10;
    /* gPreviousYPosition / gPreviousXPosition as the Ruins Test reads
     * them: Samus's position on its previous update. */
    uint16_t previous_samus_y, previous_samus_x;
    /* The Mecha Ridley weapon graphics last copied to OBJ VRAM 0x4280
     * (MechaRidleyLoadMissilesGfx / LoadFireballsGfx): 0 none, else 1 plus
     * the baked "/wN" variant (0-2 missile stages, 3-6 fireball stages). */
    uint8_t mecha_weapon_gfx;
    /* gEnergyRefillAnimation, gMissileRefillAnimation,
     * gSuperMissileRefillAnimation and gPowerBombRefillAnimation. */
    uint8_t refill_animation[4];
    /* gPauseScreenFlag: the map station requests the map download (5). */
    uint8_t pause_screen_flag;
    /* gDisablePause (the save platforms) and gIsLoadingFile (the room of a
     * loaded file). */
    bool disable_pause, loading_file;
    /* gLastDoorUsed == 0 as the runtime knows it: the room entered at a new
     * game's start (the gunship lands). */
    bool landing;
    /* gHideHud and the StartEffectForCutscene request (EFFECT_CUTSCENE_*,
     * 0: none) the runtime takes. */
    bool hide_hud;
    uint8_t cutscene_effect;
    /* The gunship's palette animations as last copied to OBJ rows 9 and 10
     * (its entrance and flames palette offsets, -1: still sGunshipPal). */
    int8_t gunship_entrance_palette, gunship_flames_palette;
    /* gDisableAnimatedPalette = FALSE of the zipline generator (the
     * runtime takes the request). */
    bool enable_animated_palette;
    /* gBg2Movement (subpixels): BG2's offset from BG1 in the rooms whose
     * BG2 is BG_PROP_MOVING (the rising Chozo pillar); cleared by the room
     * load. */
    int16_t bg2_offset_x, bg2_offset_y;
    /* gParasiteRelated: the Parasites run fast for 1.5 seconds once Samus
     * came near or one grabbed her; cleared by the room load. */
    uint8_t parasite_related;
    /* The sMetroidPal_SamusGrabbed row a grabbing Metroid last copied to
     * OBJ row 12 (-1: sMetroidPal's own); reset by the room load. */
    int8_t metroid_grab_palette;
    /* gLockScreen (LOCK_SCREEN_TYPE_POSITION): the camera centred on a
     * native position instead of Samus; cleared by the room load. */
    bool lock_screen;
    uint16_t lock_y, lock_x;
    /* IoUpdateDispcnt(FALSE, DCNT_BG2) (Kraid's death) and the sKraidPal
     * row DMAed to BG palette row 10 (-1: none); reset by the room load. */
    bool bg2_hidden;
    int8_t bg_palette_row10;
    /* EscapeUpdateTimer and the invisible Zebes explosion actor. The
     * countdown survives room loads with the other game globals. */
    MzmEscape escape;
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
    /* The map station: Samus facing right in; out, a SamusSetPose request
     * (`set_pose`, MZM_POSE_NONE: none), gSamusData.timer (`set_timer`,
     * with currentAnimationFrame reset when `reset_frame`) and the
     * invincibility timer cleared. */
    bool facing_right;
    uint8_t set_pose;
    bool set_timer, reset_frame, clear_invincibility;
    uint8_t timer;
    /* The gunship: Samus's pose in (MZM_POSE_*); out, standingStatus
     * STANDING_ENEMY and lastWallTouchedMidAir (`set_last_wall`). */
    uint8_t samus_pose;
    bool standing_enemy, set_last_wall;
    uint16_t last_wall;
    /* Ridley's claw: gSamusData.invincibilityTimer written (0: not). */
    uint8_t set_invincibility;
    /* The Ruins Test: gSamusData.invincibilityTimer in; out, the
     * forcedMovement of its hurt request (0: none). */
    uint8_t invincibility;
    uint8_t set_forced;
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
    /* gSamusData.pose == SPOSE_USING_AN_ELEVATOR (the elevator pad). */
    bool samus_using_elevator;
    /* The message banner's pose (MZM_BANNER_POSE_*; it stays the last one
     * once the banner is gone, as its sprite slot does) and
     * gEquipment.downloadedMapStatus (the map stations). */
    uint8_t banner_pose;
    uint8_t downloaded_maps;
    /* The banner's work1: YES on the save prompt. */
    bool banner_yes;
    /* gProjectileData (the morph ball launchers take bombs; NULL: none). */
    MzmWeapons *weapons;
    /* The rest of gSamusPhysics' hitbox (SpriteCheckCollidingWithSamus). */
    int16_t samus_hitbox_bottom, samus_hitbox_left, samus_hitbox_right;
    /* gSamusWeaponInfo.chargeCounter, gArmCannonY / gArmCannonX (native
     * subpixels, as ProjectileUpdate left them last frame) and gDifficulty
     * (0 easy, 1 normal, 2 hard). */
    uint8_t charge_counter;
    uint16_t arm_cannon_y, arm_cannon_x;
    uint8_t difficulty;
    /* gSamusPhysics.horizontalMovingDirection (0 none, 1 left, 2 right). */
    uint8_t samus_h_moving;
    /* gChangedInput: the keys pressed this frame (MZM_KEY_* bits). */
    uint16_t changed_input;
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
    /* gSamusData.pose (MZM_POSE_*) in; out, the SamusSetPose request of a
     * zipline grabbing her or of a frozen sprite dropping her from a
     * zipline (MZM_POSE_NONE: none). A zipline grab also moves `x`, `y`
     * and `previous_x` (gPreviousXPosition). */
    uint8_t pose, set_pose;
    /* The Parasites: the Varia or Gravity Suit (SMF_ALL_SUITS, immune);
     * `leech` takes the forced damage of 1 (SpriteUtilTakeDamageFromSprite
     * with no knockback: the hurt pose only when it kills her) and, when
     * she survives, makes her invincible 16 frames, returning that she
     * survived (NULL: no leech); `leech_count` is gSubSpriteData1.work2. */
    bool suited;
    bool (*leech)(void *context);
    uint8_t *leech_count;
    /* Set during `hurt` for SSC_HURTS_SAMUS_A_LOT: five times the damage
     * and a knockback a subpixel stronger. */
    bool hurts_a_lot;
    /* SSC_KNOCKS_BACK_SAMUS: SamusSetPose(SPOSE_KNOCKBACK_REQUEST) with
     * an x velocity of a pixel away from the sprite (NULL: none). */
    void (*knockback)(void *context, bool sprite_on_left);
    /* SpriteUtilTakeDamageFromSprite without knockback for `damage` (the
     * hurt pose only when it kills her), then, when she survives, an
     * invincibility of `invincibility` frames when she has none, or for
     * SSC_METROID (0) of 1, 4 or 8 frames by suits (NULL: none). */
    void (*metroid_drain)(void *context, uint16_t damage, uint8_t invincibility);
    /* gSamusData.invincibilityTimer (the stand-on-top check of
     * SSC_HURTS_RIGHT_CAN_STAND_ON_TOP), and set during `knockback` for a
     * knockback of an eighth of a block a frame instead of a pixel. */
    uint8_t invincibility_timer;
    bool big_knockback;
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
/* Native OBJ priority before gSamusOnTopOfBackgrounds adjusts it. */
uint8_t mzm_sprite_bg_priority(const MzmSprite *sprite);
/* The OAM table name of a sprite's animation, or NULL. */
const char *mzm_sprite_anim_name(const MzmSprite *sprite);
/* SpriteUtilSamusAndSpriteCollision over the sprite array (run at the start
 * of SpriteUpdate): at most one hurting or collecting contact per frame, then
 * every ignore timer runs down. */
void mzm_sprites_samus_collision(MzmSprite *sprites, int count, MzmSpriteSamus *samus,
                                 const MzmSpriteTables *tables);
/* ProjectileCheckHittingSprite for beams, missiles and super missiles (run
 * by ProjectileUpdate after the new projectile's spawn and before the
 * projectile loop, through MzmWeapons.hit_sprites): damage, stun and
 * removal of the projectile; missiles not hurting the sprite tumble. */
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
