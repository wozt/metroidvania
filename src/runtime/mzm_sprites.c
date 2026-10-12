/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission sprites; comments name the pinned decompilation's routines. */
#include "mzm_sprites.h"
#include "mzm_sprite_ai.h"
#include "mzm_sfx.h"

#include <string.h>

/* Zoomer poses (zoomer.c). */
enum {
    ZOOMER_POSE_IDLE_INIT = 0x8,
    ZOOMER_POSE_IDLE = 0x9,
    ZOOMER_POSE_TURNING_AROUND_INIT = 0xA,
    ZOOMER_POSE_TURNING_AROUND = 0xB,
    ZOOMER_POSE_LANDING_INIT = 0xE,
    ZOOMER_POSE_LANDING = 0xF,
    ZOOMER_POSE_FALLING_INIT = 0x1E,
    ZOOMER_POSE_FALLING = 0x1F,
    ZEELA_EYES_POSE_MOVING = 0x9,
    ZEELA_EYES_POSE_EXPLODING = 0x43,
    ENEMY_DROP_POSE_IDLE = 0x9,
};

/* Ripper poses (ripper.c). */
enum {
    RIPPER_POSE_MOVING_INIT = 0x8,
    RIPPER_POSE_MOVING = 0x9,
    RIPPER_POSE_TURNING_AROUND_INIT = 0xA,
    RIPPER_POSE_TURNING_AROUND_FIRST_PART = 0xB,
    RIPPER_POSE_TURNING_AROUND_SECOND_PART = 0xC,
};

/* Skree poses (skree.c). */
enum {
    SKREE_POSE_IDLE_INIT = 0x8,
    SKREE_POSE_IDLE = 0x9,
    SKREE_POSE_SPINNING_INIT = 0x22,
    SKREE_POSE_SPINNING = 0x23,
    SKREE_POSE_GOING_DOWN_INIT = 0x34,
    SKREE_POSE_GOING_DOWN = 0x35,
    SKREE_POSE_CRASHING = 0x37,
    SKREE_EXPLOSION_PART_GOING_UP = 0,
    SKREE_EXPLOSION_POSE_MOVING = 0x9,
    SKREE_Y_RANGE = BLOCK_SIZE * 10 + PIXEL_SIZE,
    SKREE_X_RANGE = BLOCK_SIZE * 2 + QUARTER_BLOCK_SIZE + PIXEL_SIZE + PIXEL_SIZE / 2,
    PI_4 = 32,                      /* PI = Q_8_8(.5f) */
};

/* Sidehopper (sidehopper.c) and Dessgeega (dessgeega.c) poses. */
enum {
    HOPPER_POSE_JUMP_WARNING_INIT = 0x8,
    HOPPER_POSE_JUMP_WARNING = 0x9,
    HOPPER_POSE_IDLE = 0xF,
    DESSGEEGA_POSE_LONG_BEAM_DETECT = 0x11,
    HOPPER_POSE_FALLING = 0x1F,
    HOPPER_POSE_JUMPING = 0x23,
    HOPPER_POSE_LANDING = 0x25,
    DESSGEEGA_POSE_LONG_BEAM_SPAWNING = 0x33,
    /* HEAD_HITBOX (sidehopper: - PIXEL_SIZE) / feet hitbox. */
    SIDEHOPPER_HEAD_HITBOX = BLOCK_SIZE + HALF_BLOCK_SIZE - PIXEL_SIZE,
    DESSGEEGA_HEAD_HITBOX = BLOCK_SIZE + QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    /* 0x70 / 0x5C of SidehopperJumpingInit / SidehopperLandingInit. */
    SIDEHOPPER_JUMPING_HEAD_HITBOX = 0x70,
    SIDEHOPPER_LANDING_HEAD_HITBOX = 0x5C,
    HOPPER_DETECTION_RANGE = BLOCK_SIZE * 5,
    DESSGEEGA_LONG_BEAM_Y_RANGE = BLOCK_SIZE * 12,
    DESSGEEGA_LONG_BEAM_X_RANGE = BLOCK_SIZE * 4 - QUARTER_BLOCK_SIZE,
    DESSGEEGA_SPAWNING_SPEED = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    HOPPER_JUMP_VELOCITY_FRAMES = 39,   /* ARRAY_SIZE(sSidehopperLowJumpVelocity) * 4 - 1 */
};

/* ZoomerTurningDirection */
enum {
    TURN_BOTTOM_LEFT_CORNER, TURN_TOP_RIGHT_EDGE, TURN_BOTTOM_RIGHT_EDGE, TURN_TOP_RIGHT_CORNER,
    TURN_TOP_LEFT_EDGE, TURN_BOTTOM_RIGHT_CORNER, TURN_TOP_LEFT_CORNER, TURN_BOTTOM_LEFT_EDGE,
};

const char *const mzm_zoomer_anim_names[MZM_ZOOMER_ANIMS] = {
    "ZoomerOam_Falling", "ZoomerOam_OnGround", "ZoomerOam_OnWall",
    "ZoomerOam_WalkingOnLeftSlope", "ZoomerOam_WalkingOnRightSlope",
    "ZoomerOam_TurningEdgeLeft", "ZoomerOam_TurningEdgeRight",
    "ZoomerOam_TurningCornerLeft", "ZoomerOam_TurningCornerRight",
};

const char *const mzm_drop_anim_names[MZM_DROP_ANIMS] = {
    "EnemyDropOam_SmallEnergy", "EnemyDropOam_LargeEnergy", "EnemyDropOam_Missile",
    "EnemyDropOam_SuperMissile", "EnemyDropOam_PowerBomb",
};

const char *const mzm_ripper_anim_names[MZM_RIPPER_ANIMS] = {
    "RipperOam_Moving", "RipperOam_TurningAround", "RipperOam_TurningAroundPart2",
};

const char *const mzm_zeela_anim_names[MZM_ZEELA_ANIMS] = {
    "ZeelaOam_FallingOnGround", "ZeelaOam_OnGround", "ZeelaOam_OnWall",
    "ZeelaOam_WalkingOnSlope", "ZeelaOam_FallingOnWall", "ZeelaOam_TurningEdgeLeft",
    "ZeelaOam_TurningEdgeRight", "ZeelaOam_TurningCornerLeft", "ZeelaOam_TurningCornerRight",
    "ZeelaEyesOam_Idle", "ZeelaEyesOam_Exploding",
};
static const char *const zeela_sprites[MZM_ZEELA_ANIMS] = {
    "ZEELA", "ZEELA", "ZEELA", "ZEELA", "ZEELA", "ZEELA", "ZEELA", "ZEELA", "ZEELA", "ZEELA",
    "ZEELA",
};
const char *const mzm_waver_anim_names[MZM_WAVER_ANIMS] = {"WaverOam"};
static const char *const waver_sprites[MZM_WAVER_ANIMS] = {"WAVER"};
const char *const mzm_sidehopper_anim_names[MZM_SIDEHOPPER_ANIMS] = {
    "SidehopperOam_Idle", "SidehopperOam_ShakingHead", "SidehopperOam_JumpWarning",
    "SidehopperOam_Jumping", "SidehopperOam_Landing",
};
static const char *const sidehopper_sprites[MZM_SIDEHOPPER_ANIMS] = {
    "SIDEHOPPER", "SIDEHOPPER", "SIDEHOPPER", "SIDEHOPPER", "SIDEHOPPER",
};
const char *const mzm_dessgeega_anim_names[MZM_DESSGEEGA_ANIMS] = {
    "DessGeegaOam_Idle", "DessGeegaOam_Screaming", "DessGeegaOam_JumpWarning",
    "DessGeegaOam_Jumping", "DessGeegaOam_Landing",
};
static const char *const dessgeega_sprites[MZM_DESSGEEGA_ANIMS] = {
    "DESSGEEGA", "DESSGEEGA", "DESSGEEGA", "DESSGEEGA", "DESSGEEGA",
};
const char *const mzm_viola_anim_names[MZM_VIOLA_ANIMS] = {"ViolaOam_Moving"};
static const char *const viola_sprites[MZM_VIOLA_ANIMS] = {"VIOLA_BLUE"};
const char *const mzm_sova_anim_names[MZM_SOVA_ANIMS] = {
    "SovaOam_OnGround", "SovaOam_WalkingOnLeftSlope", "SovaOam_WalkingOnRightSlope",
    "SovaOam_WalkingOnLeft", "SovaOam_WalkingOnRight", "SovaOam_WalkingOnCeiling",
    "SovaOam_TurningCornerRight", "SovaOam_TurningEdgeRight", "SovaOam_TurningEdgeLeft",
    "SovaOam_TurningCornerLeft", "SovaOam_Falling",
};
static const char *const sova_sprites[MZM_SOVA_ANIMS] = {
    "SOVA_PURPLE", "SOVA_PURPLE", "SOVA_PURPLE", "SOVA_PURPLE", "SOVA_PURPLE", "SOVA_PURPLE",
    "SOVA_PURPLE", "SOVA_PURPLE", "SOVA_PURPLE", "SOVA_PURPLE", "SOVA_PURPLE",
};
const char *const mzm_zeb_anim_names[MZM_PIPE_BUG_ANIMS] = {"ZebOam_Idle", "ZebOam_Moving"};
const char *const mzm_geega_anim_names[MZM_PIPE_BUG_ANIMS] = {"GeegaOam_Idle", "GeegaOam_Moving"};
static const char *const zeb_sprites[MZM_PIPE_BUG_ANIMS] = {"ZEB", "ZEB"};
static const char *const geega_sprites[MZM_PIPE_BUG_ANIMS] = {"GEEGA", "GEEGA"};
const char *const mzm_zebbo_anim_names[MZM_PIPE_BUG_ANIMS] = {"ZebboOam_Idle", "ZebboOam_Moving"};
static const char *const zebbo_sprites[MZM_PIPE_BUG_ANIMS] = {"ZEBBO_GREEN", "ZEBBO_GREEN"};
const char *const mzm_reo_anim_names[MZM_REO_ANIMS] = {"ReoOam_Idle", "ReoOam_Moving"};
static const char *const reo_sprites[MZM_REO_ANIMS] = {"REO_GREEN_WINGS", "REO_GREEN_WINGS"};
const char *const mzm_mella_anim_names[MZM_MELLA_ANIMS] = {"MellaOam_Idle", "MellowOam_Moving"};
static const char *const mella_sprites[MZM_MELLA_ANIMS] = {"MELLA", "MELLA"};
const char *const mzm_holtz_anim_names[MZM_HOLTZ_ANIMS] = {
    "HoltzOam_Idle", "HoltzOam_IdleAware", "HoltzOam_Warning", "HoltzOam_GoingDown",
    "HoltzOam_GoingUp", "HoltzOam_BonkingOnCeiling",
};
static const char *const holtz_sprites[MZM_HOLTZ_ANIMS] = {
    "HOLTZ", "HOLTZ", "HOLTZ", "HOLTZ", "HOLTZ", "HOLTZ",
};
const char *const mzm_multiviola_anim_names[MZM_MULTIVIOLA_ANIMS] = {"MultiviolaOam_Moving"};
static const char *const multiviola_sprites[MZM_MULTIVIOLA_ANIMS] = {"MULTIVIOLA"};
const char *const mzm_rinka_anim_names[MZM_RINKA_ANIMS] = {
    "RinkaOrangeOam_Spawning", "RinkaOrangeOam_Moving", "RinkaGreenOam_Spawning",
    "RinkaGreenOam_Moving",
};
static const char *const rinka_sprites[MZM_RINKA_ANIMS] = {
    "RINKA_ORANGE", "RINKA_ORANGE", "RINKA_GREEN", "RINKA_GREEN",
};
const char *const mzm_space_pirate_anim_names[MZM_PIRATE_ANIMS] = {
    "SpacePirateOam_Crouched", "SpacePirateOam_Walking", "SpacePirateOam_ChargingLaserForward",
    "SpacePirateOam_TurningAroundFirstPart", "SpacePirateOam_TurningAroundSecondPart",
    "SpacePirateOam_ShootingForward", "SpacePirateOam_Jumping",
    "SpacePirateOam_ChargingLaserDiagonallyDown", "SpacePirateOam_Falling",
    "SpacePirateOam_Landing", "SpacePirateOam_ShootingDiagonallyDown",
    "SpacePirateOam_ChargingLaserDiagonallyUp", "SpacePirateOam_ShootingDiagonallyUp",
    "SpacePirateOam_ClimbingUp", "SpacePirateOam_ClimbingDown",
    "SpacePirateOam_StartingToCrawl", "SpacePirateOam_Crawling", "SpacePirateOam_StandingUp",
    "SpacePirateOam_TurningAroundWhileCrawlingFirstPart",
    "SpacePirateOam_TurningAroundWhileCrawlingSecondPart", "SpacePirateOam_CrawlingStopped",
    "SpacePirateOam_TurningToAimWhileClimbing", "SpacePirateOam_AimingWhileClimbing",
    "SpacePirateOam_ClimbingChargingLaserForward", "SpacePirateOam_ClimbingShootingForward",
    "SpacePirateOam_ClimbingRetractingAim", "SpacePirateOam_LaunchingFromWall",
    "SpacePirateOam_JumpingFromWall", "SpacePirateOam_Standing",
    "SpacePirateOam_StandingAlerted", "SpacePirateLaserOam_Forward",
    "SpacePirateLaserOam_Diagonal", "SpacePirateLaserOam_Exploding",
};
static const char *const space_pirate_sprites[MZM_PIRATE_ANIMS] = {
    "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE",
    "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE",
    "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE",
    "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE",
    "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE",
    "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE",
    "SPACE_PIRATE", "SPACE_PIRATE", "SPACE_PIRATE",
};
const char *const mzm_chozodia_security_anim_names[MZM_SECURITY_ANIMS] = {
    "SearchlightOam_Moving",
    "SecurityLaserOam_VerticalSmall", "SecurityLaserOam_VerticalMedium",
    "SecurityLaserOam_VerticalLarge", "SecurityLaserOam_VerticalVeryLarge",
    "SecurityLaserOam_HorizontalSmall", "SecurityLaserOam_HorizontalMedium",
    "SecurityLaserOam_HorizontalLarge", "SecurityLaserOam_HorizontalVeryLarge",
    "SecurityLaserOam_VerticalSmallDisappearing", "SecurityLaserOam_VerticalMediumDisappearing",
    "SecurityLaserOam_VerticalLargeDisappearing",
    "SecurityLaserOam_VerticalVeryLargeDisappearing",
    "SecurityLaserOam_HorizontalSmallDisappearing",
    "SecurityLaserOam_HorizontalMediumDisappearing",
    "SecurityLaserOam_HorizontalLargeDisappearing",
    "SecurityLaserOam_HorizontalVeryLargeDisappearing",
    "SearchlightEyeOam_Idle", "SearchlightEyeOam_Shooting", "SearchlightEyeBeamOam_Idle",
    "SearchlightEyeProjectileOam_Moving",
};
static const char *const chozodia_security_sprites[MZM_SECURITY_ANIMS] = {
    "SEARCHLIGHT",
    "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL",
    "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL",
    "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL",
    "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL",
    "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL", "SECURITY_LASER_VERTICAL",
    "SECURITY_LASER_VERTICAL",
    "SEARCHLIGHT_EYE", "SEARCHLIGHT_EYE", "SEARCHLIGHT_EYE", "SEARCHLIGHT_EYE",
};
static const char *const zoomer_sprites[MZM_ZOOMER_ANIMS] = {
    "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW",
    "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW", "ZOOMER_YELLOW",
};
static const char *const ripper_sprites[MZM_RIPPER_ANIMS] = {
    "RIPPER_BROWN", "RIPPER_BROWN", "RIPPER_BROWN",
};
const char *const mzm_skree_anim_names[MZM_SKREE_ANIMS] = {
    "SkreeOam_Idle", "SkreeOam_Spinning", "SkreeOam_GoingDown", "SkreeOam_Crashing",
    "SkreeExplosionOam_GoingDown", "SkreeExplosionOam_GoingUp",
};
static const char *const skree_sprites[MZM_SKREE_ANIMS] = {
    "SKREE_GREEN", "SKREE_GREEN", "SKREE_GREEN", "SKREE_GREEN", "SKREE_GREEN", "SKREE_GREEN",
};
static const char *const drop_sprites[MZM_DROP_ANIMS] = {
    "SMALL_ENERGY_DROP", "LARGE_ENERGY_DROP", "MISSILE_DROP", "SUPER_MISSILE_DROP",
    "POWER_BOMB_DROP",
};

const MzmSpriteFamily mzm_sprite_families[MZM_SPRITE_FAMILIES] = {
    [MZM_FAMILY_ZOOMER] = {MZM_ZOOMER_ANIMS, mzm_zoomer_anim_names, zoomer_sprites},
    [MZM_FAMILY_ZEELA] = {MZM_ZEELA_ANIMS, mzm_zeela_anim_names, zeela_sprites},
    [MZM_FAMILY_RIPPER] = {MZM_RIPPER_ANIMS, mzm_ripper_anim_names, ripper_sprites},
    [MZM_FAMILY_SKREE] = {MZM_SKREE_ANIMS, mzm_skree_anim_names, skree_sprites},
    [MZM_FAMILY_WAVER] = {MZM_WAVER_ANIMS, mzm_waver_anim_names, waver_sprites},
    [MZM_FAMILY_DROP] = {MZM_DROP_ANIMS, mzm_drop_anim_names, drop_sprites},
    [MZM_FAMILY_SIDEHOPPER] = {MZM_SIDEHOPPER_ANIMS, mzm_sidehopper_anim_names,
                               sidehopper_sprites},
    [MZM_FAMILY_DESSGEEGA] = {MZM_DESSGEEGA_ANIMS, mzm_dessgeega_anim_names, dessgeega_sprites},
    [MZM_FAMILY_VIOLA] = {MZM_VIOLA_ANIMS, mzm_viola_anim_names, viola_sprites},
    [MZM_FAMILY_SOVA] = {MZM_SOVA_ANIMS, mzm_sova_anim_names, sova_sprites},
    [MZM_FAMILY_ZEB] = {MZM_PIPE_BUG_ANIMS, mzm_zeb_anim_names, zeb_sprites},
    [MZM_FAMILY_GEEGA] = {MZM_PIPE_BUG_ANIMS, mzm_geega_anim_names, geega_sprites},
    [MZM_FAMILY_ZEBBO] = {MZM_PIPE_BUG_ANIMS, mzm_zebbo_anim_names, zebbo_sprites},
    [MZM_FAMILY_REO] = {MZM_REO_ANIMS, mzm_reo_anim_names, reo_sprites},
    [MZM_FAMILY_MELLA] = {MZM_MELLA_ANIMS, mzm_mella_anim_names, mella_sprites},
    [MZM_FAMILY_HOLTZ] = {MZM_HOLTZ_ANIMS, mzm_holtz_anim_names, holtz_sprites},
    [MZM_FAMILY_MULTIVIOLA] = {MZM_MULTIVIOLA_ANIMS, mzm_multiviola_anim_names,
                               multiviola_sprites},
    [MZM_FAMILY_RINKA] = {MZM_RINKA_ANIMS, mzm_rinka_anim_names, rinka_sprites},
    [MZM_FAMILY_SPACE_PIRATE] = {MZM_PIRATE_ANIMS, mzm_space_pirate_anim_names,
                                 space_pirate_sprites},
    [MZM_FAMILY_CHOZODIA_SECURITY] = {MZM_SECURITY_ANIMS, mzm_chozodia_security_anim_names,
                                      chozodia_security_sprites},
};


uint32_t mzm_clipdata_process(const MzmSpriteWorld *world, uint16_t y, uint16_t x, int actor) {
    uint8_t type = world->block(world->context, y, x);
    unsigned sy = y % BLOCK_SIZE, sx = x % BLOCK_SIZE;
    switch (type) {
    case MZM_CLIPDATA_SOLID: return type | MZM_CLIPDATA_SOLID_FLAG;
    case MZM_CLIPDATA_LEFT_STEEP: return sy >= sx ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_RIGHT_STEEP:
        return sy >= SUB_PIXEL_POSITION_FLAG - sx ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_LEFT_UPPER_SLIGHT:
        return sy >= sx >> 1 ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_LEFT_LOWER_SLIGHT:
        return sy >= (sx + SUB_PIXEL_POSITION_FLAG) >> 1 ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_RIGHT_LOWER_SLIGHT:
        return sy >= SUB_PIXEL_POSITION_FLAG - (sx >> 1) ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_RIGHT_UPPER_SLIGHT:
        return sy >= (SUB_PIXEL_POSITION_FLAG - sx) >> 1 ? type | MZM_CLIPDATA_SOLID_FLAG : type;
    case MZM_CLIPDATA_ENEMY_ONLY:
        return actor > MZM_ACTOR_NON_SPRITE ? MZM_CLIPDATA_AIR
                                            : MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG;
    case MZM_CLIPDATA_DOOR: return MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG;
    case MZM_CLIPDATA_STOP_ENEMY:
        return actor < MZM_ACTOR_SPRITE ? MZM_CLIPDATA_AIR
                                        : MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG;
    case MZM_CLIPDATA_TANK:
        return actor == MZM_ACTOR_SAMUS ? MZM_CLIPDATA_AIR
                                        : MZM_CLIPDATA_SOLID | MZM_CLIPDATA_SOLID_FLAG;
    default: return type;   /* air, pass-through bottom */
    }
}

/* SpriteUtilGetCollisionAtPosition */
uint8_t ai_collision_at(const Ctx *c, uint16_t y, uint16_t x) {
    return ai_collision_at_actor(c, y, x, MZM_ACTOR_SPRITE);
}

uint8_t ai_collision_at_actor(const Ctx *c, uint16_t y, uint16_t x, int actor) {
    uint32_t clip = mzm_clipdata_process(c->world, y, x, actor);
    uint8_t collision = clip & MZM_CLIPDATA_SOLID_FLAG ? MZM_COLLISION_SOLID : MZM_COLLISION_AIR;
    switch (clip & 0xFF) {
    case MZM_CLIPDATA_RIGHT_STEEP: return MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
    case MZM_CLIPDATA_RIGHT_UPPER_SLIGHT:
    case MZM_CLIPDATA_RIGHT_LOWER_SLIGHT: return MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE;
    case MZM_CLIPDATA_LEFT_STEEP: return MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE;
    case MZM_CLIPDATA_LEFT_UPPER_SLIGHT:
    case MZM_CLIPDATA_LEFT_LOWER_SLIGHT: return MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE;
    case MZM_CLIPDATA_PASS_THROUGH_BOTTOM: return MZM_COLLISION_PASS_THROUGH_BOTTOM;
    default: return collision;
    }
}

/* The block top of SpriteUtilCheckVerticalCollisionAtPosition(Slopes) and
 * the collision it reports. */
static uint16_t vertical_block_top(const Ctx *c, uint16_t y, uint16_t x, uint8_t *collision) {
    uint32_t clip = mzm_clipdata_process(c->world, y, x, MZM_ACTOR_SPRITE);
    *collision = clip & MZM_CLIPDATA_SOLID_FLAG ? MZM_COLLISION_SOLID : MZM_COLLISION_AIR;
    unsigned sx = x & SUB_PIXEL_POSITION_FLAG;
    uint16_t top = y & BLOCK_POSITION_FLAG;
    switch (clip & 0xFF) {
    case MZM_CLIPDATA_RIGHT_STEEP:
        *collision = MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
        return (uint16_t)(int16_t)(top - ((int)sx - SUB_PIXEL_POSITION_FLAG));
    case MZM_CLIPDATA_RIGHT_LOWER_SLIGHT:
        *collision = MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE;
        return (uint16_t)(int16_t)(top - ((int)(sx >> 1) - SUB_PIXEL_POSITION_FLAG));
    case MZM_CLIPDATA_RIGHT_UPPER_SLIGHT:
        *collision = MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE;
        return (uint16_t)(int16_t)(top - ((int)(sx >> 1) - SUB_PIXEL_POSITION_FLAG / 2));
    case MZM_CLIPDATA_LEFT_STEEP:
        *collision = MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE;
        return top | sx;
    case MZM_CLIPDATA_LEFT_LOWER_SLIGHT:
        *collision = MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE;
        return top | ((sx >> 1) + SUB_PIXEL_POSITION_FLAG / 2);
    case MZM_CLIPDATA_LEFT_UPPER_SLIGHT:
        *collision = MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE;
        return top | (sx >> 1);
    case MZM_CLIPDATA_PASS_THROUGH_BOTTOM:
        *collision = MZM_COLLISION_PASS_THROUGH_BOTTOM;
        return top;
    default:
        return top;
    }
}

/* SpriteUtilCheckVerticalCollisionAtPosition */
uint16_t ai_check_vertical(Ctx *c, uint16_t y, uint16_t x) {
    return vertical_block_top(c, y, x, &c->vertical);
}

/* SpriteUtilCheckVerticalCollisionAtPositionSlopes: the collision only
 * counts when the point is at or below the block top. */
uint16_t ai_check_vertical_slopes(Ctx *c, uint16_t y, uint16_t x) {
    uint8_t collision;
    uint16_t top = vertical_block_top(c, y, x, &collision);
    c->vertical = y >= top ? collision : MZM_COLLISION_AIR;
    return top;
}

uint8_t ai_movement_at(const Ctx *c, uint16_t y, uint16_t x) {
    return c->world->movement ? c->world->movement(c->world->context, y, x)
                              : MZM_CLIPDATA_MOVEMENT_NONE;
}

/* SpriteUtilAlignYPositionOnSlopeAtOrigin */
void ai_align_on_slope(Ctx *c) {
    uint16_t y = c->s->y, x = c->s->x;
    uint16_t top = ai_check_vertical(c, (uint16_t)(y - PIXEL_SIZE), x);
    if ((c->vertical & MZM_COLLISION_FLAGS_F) >= MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
        c->s->y = top;
        return;
    }
    top = ai_check_vertical(c, y, x);
    if ((c->vertical & MZM_COLLISION_FLAGS_F) >= MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
        c->s->y = top;
        return;
    }
    top = ai_check_vertical(c, (uint16_t)(y + PIXEL_SIZE), x);
    if (c->vertical != MZM_COLLISION_AIR) c->s->y = top;
}

static uint8_t timer(const Ctx *c, unsigned anim, unsigned frame) {
    if (anim >= MZM_SPRITE_MAX_ANIMS || frame >= MZM_SPRITE_MAX_FRAMES ||
        frame >= c->kind->counts[anim])
        return 0;
    return c->kind->timers[anim][frame];
}

uint8_t ai_timer(const Ctx *c, unsigned anim, unsigned frame) {
    return timer(c, anim, frame);
}

/* SpriteUtilHasCurrentAnimationEnded (extra = 1) and ...NearlyEnded (2). */
bool ai_animation_ending(const Ctx *c, unsigned extra) {
    unsigned adc = (uint8_t)(c->s->adc + extra);
    return timer(c, c->s->anim, c->s->frame) < adc && timer(c, c->s->anim, c->s->frame + 1u) == 0;
}

void ai_set_anim(MzmSprite *s, uint8_t anim) {
    s->anim = anim;
    s->adc = 0;
    s->frame = 0;
}

/* SpriteUtilChooseRandomXDirection */
void ai_random_x_direction(Ctx *c) {
    if (c->rng & 1) c->s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else c->s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
}

/* Zeelas share the Zoomer's crawler code (zeela.c mirrors zoomer.c). */
static bool is_zeela(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_ZEELA || s->sprite_id == MZM_PSPRITE_ZEELA_RED;
}

/* ZoomerCheckCollidingWithAir / ZeelaCheckCollidingWithAir */
static bool zoomer_in_air(const Ctx *c) {
    const MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        uint16_t x = s->status & MZM_SPRITE_STATUS_X_FLIP ? s->x : (uint16_t)(s->x - PIXEL_SIZE);
        return ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), x) == MZM_COLLISION_AIR &&
               ai_collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE), x) == MZM_COLLISION_AIR;
    }
    uint16_t y = s->status & MZM_SPRITE_STATUS_Y_FLIP ? (uint16_t)(s->y - PIXEL_SIZE) : s->y;
    return ai_collision_at(c, y, (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR &&
           ai_collision_at(c, y, (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR;
}

/* ZoomerUpdateHitbox / ZeelaUpdateHitbox: legs against the surface, the
 * body (Zeela: eyes) away from it, the sides along it. */
static void zoomer_hitbox(MzmSprite *s) {
    bool zeela = is_zeela(s);
    int16_t legs = PIXEL_SIZE;
    int16_t body = zeela ? THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE
                         : THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE;
    int16_t side = zeela ? QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE : HALF_BLOCK_SIZE - PIXEL_SIZE;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        s->hitbox_top = -side;
        s->hitbox_bottom = side;
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            s->hitbox_left = -body;
            s->hitbox_right = legs;
        } else {
            s->hitbox_left = -legs;
            s->hitbox_right = body;
        }
    } else {
        s->hitbox_left = -side;
        s->hitbox_right = side;
        if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
            s->hitbox_top = -legs;
            s->hitbox_bottom = body;
        } else {
            s->hitbox_top = -body;
            s->hitbox_bottom = legs;
        }
    }
}

/* ZoomerSetCrawlingOam / ZoomerSetFallingOam */
static void zoomer_crawling_oam(MzmSprite *s) {
    /* ZeelaSetCrawlingOam: on the ground the X flip follows the facing. */
    if (is_zeela(s) && !(s->status & MZM_SPRITE_STATUS_FACING_DOWN)) {
        if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) s->status |= MZM_SPRITE_STATUS_X_FLIP;
        else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    }
    ai_set_anim(s, s->status & MZM_SPRITE_STATUS_FACING_DOWN ? MZM_ZOOMER_ON_WALL : MZM_ZOOMER_ON_GROUND);
}

static void zoomer_falling_oam(MzmSprite *s) {
    uint8_t wall = is_zeela(s) ? MZM_ZEELA_FALLING_ON_WALL : MZM_ZOOMER_ON_WALL;
    ai_set_anim(s, s->status & MZM_SPRITE_STATUS_FACING_DOWN ? wall : MZM_ZOOMER_FALLING);
}

/* ZoomerInit: the surface it starts on is searched below, above, then on
 * the left and right walls; without one the sprite is removed. */
static void zoomer_init(Ctx *c) {
    MzmSprite *s = c->s;
    ai_random_x_direction(c);
    s->pose = ZOOMER_POSE_IDLE;
    if (ai_collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    } else if (ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE + PIXEL_SIZE)), s->x) &
               MZM_COLLISION_FLAGS_F0) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
        s->y -= BLOCK_SIZE;
    } else if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                            (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE))) &
               MZM_COLLISION_FLAGS_F0) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        s->y -= HALF_BLOCK_SIZE;
        s->x -= HALF_BLOCK_SIZE;
    } else if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                            (uint16_t)(s->x + HALF_BLOCK_SIZE)) & MZM_COLLISION_FLAGS_F0) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN | MZM_SPRITE_STATUS_X_FLIP;
        s->y -= HALF_BLOCK_SIZE;
        s->x += HALF_BLOCK_SIZE;
    } else {
        s->status = 0;
        return;
    }
    s->samus_collision = SSC_HURTS_SAMUS;
    zoomer_crawling_oam(s);
    zoomer_hitbox(s);
    s->health = c->tables->health[s->sprite_id];
    s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    if (s->sprite_id == MZM_PSPRITE_ZOOMER_RED || s->sprite_id == MZM_PSPRITE_ZEELA_RED)
        s->absolute_palette_row = s->palette_row = 1;
}

/* A turn chosen by ZoomerCrawling: the new facing and direction. */
static bool zoomer_turn(MzmSprite *s, bool set_right, uint8_t direction) {
    if (set_right) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    s->work1 = direction;
    return true;
}

/* ZoomerCrawling */
static void zoomer_crawling(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t speed = is_zeela(s) ? 1 :      /* ONE_SUB_PIXEL */
                     s->sprite_id == MZM_PSPRITE_ZOOMER_RED ? PIXEL_SIZE * 3 / 4 : PIXEL_SIZE / 2;
    bool turning = false, right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    if (zoomer_in_air(c)) {
        s->pose = ZOOMER_POSE_FALLING_INIT;
        return;
    }
    /* SpriteUtilShouldFall: only during a screen shake, not ported. */
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            if (right) {
                if (!(ai_collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, true, TURN_BOTTOM_LEFT_EDGE);
                else if (ai_collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE),
                                      (uint16_t)(s->x - PIXEL_SIZE)) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_RIGHT_CORNER);
                else
                    s->y += speed;
            } else {
                if (!(ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), s->x) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, true, TURN_BOTTOM_RIGHT_CORNER);
                else if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                                      (uint16_t)(s->x - PIXEL_SIZE)) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_LEFT_EDGE);
                else
                    s->y -= speed;
            }
        } else {
            if (right) {
                if (!(ai_collision_at(c, s->y, (uint16_t)(s->x - PIXEL_SIZE)) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, false, TURN_TOP_LEFT_CORNER);
                else if (ai_collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE), s->x) ==
                         MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, true, TURN_TOP_LEFT_EDGE);
                else
                    s->y += speed;
            } else {
                if (!(ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), (uint16_t)(s->x - PIXEL_SIZE)) &
                      MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, false, TURN_TOP_LEFT_EDGE);
                else if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x) ==
                         MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, true, TURN_TOP_LEFT_CORNER);
                else
                    s->y -= speed;
            }
        }
    } else if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
        if (right) {
            if (!(ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), s->x) & MZM_COLLISION_FLAGS_F))
                turning = zoomer_turn(s, false, TURN_BOTTOM_RIGHT_EDGE);
            else if (ai_collision_at(c, s->y, (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
                turning = zoomer_turn(s, true, TURN_TOP_RIGHT_CORNER);
            else
                s->x += speed;
        } else {
            if (!(ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), (uint16_t)(s->x - PIXEL_SIZE)) &
                  MZM_COLLISION_FLAGS_F))
                turning = zoomer_turn(s, false, TURN_TOP_RIGHT_CORNER);
            else if (ai_collision_at(c, s->y, (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
                turning = zoomer_turn(s, true, TURN_BOTTOM_RIGHT_EDGE);
            else
                s->x -= speed;
        }
    } else {
        ai_align_on_slope(c);
        if (c->vertical == MZM_COLLISION_AIR || (c->vertical & MZM_COLLISION_FLAGS_F0)) {
            if (s->anim != MZM_ZOOMER_ON_GROUND) ai_set_anim(s, MZM_ZOOMER_ON_GROUND);
            if (right) {
                if (ai_collision_at(c, s->y, s->x) == MZM_COLLISION_AIR)
                    turning = zoomer_turn(s, true, TURN_BOTTOM_LEFT_CORNER);
                else if (ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE),
                                      (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_TOP_RIGHT_EDGE);
                else
                    s->x += speed;
            } else {
                if (ai_collision_at(c, s->y, (uint16_t)(s->x - PIXEL_SIZE)) == MZM_COLLISION_AIR)
                    turning = zoomer_turn(s, true, TURN_TOP_RIGHT_EDGE);
                else if (ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE),
                                      (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_LEFT_CORNER);
                else
                    s->x -= speed;
            }
        } else if (is_zeela(s)) {
            /* ZeelaMove on a slope: one table, flipped once when it starts
             * (no flip on a left slope going right or a right slope going
             * left), at the same speed both ways. */
            bool left_slope = c->vertical == MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE ||
                              c->vertical == MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE;
            bool right_slope = c->vertical == MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE ||
                               c->vertical == MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
            if (s->anim != MZM_ZEELA_WALKING_ON_SLOPE) {
                bool flip = right ? !left_slope : right_slope;
                if (flip) s->status |= MZM_SPRITE_STATUS_X_FLIP;
                else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
                ai_set_anim(s, MZM_ZEELA_WALKING_ON_SLOPE);
            }
            if (right) s->x += speed;
            else s->x -= speed;
        } else {
            bool left_slope = c->vertical == MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE ||
                              c->vertical == MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE;
            bool right_slope = c->vertical == MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE ||
                               c->vertical == MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
            /* Down a slope at full speed, up it at two thirds with the
             * animation slowed every other frame. */
            bool down = right ? left_slope : right_slope;
            /* Only slopes reach here, so the table follows the slope. */
            uint8_t anim = left_slope ? MZM_ZOOMER_WALKING_ON_LEFT_SLOPE
                                      : MZM_ZOOMER_WALKING_ON_RIGHT_SLOPE;
            if (s->anim != anim) ai_set_anim(s, anim);
            uint16_t step = down ? speed : (uint16_t)(speed * 2 / 3);
            if (right) s->x += step;
            else s->x -= step;
            if (!down && (c->frame8 & 1) && s->adc != 0) s->adc--;
        }
    }
    if (turning) s->pose = ZOOMER_POSE_TURNING_AROUND_INIT;
}

/* ZoomerTurningAroundInit */
static void zoomer_turning_init(MzmSprite *s) {
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    s->pose = ZOOMER_POSE_TURNING_AROUND;
    s->adc = 0;
    s->frame = 0;
    static const struct { uint8_t if_right, otherwise; bool x_flip, y_flip; } turns[8] = {
        [TURN_BOTTOM_LEFT_CORNER] = {MZM_ZOOMER_TURNING_EDGE_LEFT, MZM_ZOOMER_TURNING_CORNER_RIGHT, false, false},
        [TURN_TOP_RIGHT_EDGE] = {MZM_ZOOMER_TURNING_EDGE_LEFT, MZM_ZOOMER_TURNING_CORNER_RIGHT, true, false},
        [TURN_BOTTOM_RIGHT_EDGE] = {MZM_ZOOMER_TURNING_CORNER_RIGHT, MZM_ZOOMER_TURNING_EDGE_LEFT, false, true},
        [TURN_TOP_RIGHT_CORNER] = {MZM_ZOOMER_TURNING_CORNER_RIGHT, MZM_ZOOMER_TURNING_EDGE_LEFT, true, true},
        [TURN_TOP_LEFT_EDGE] = {MZM_ZOOMER_TURNING_CORNER_LEFT, MZM_ZOOMER_TURNING_EDGE_RIGHT, false, false},
        [TURN_BOTTOM_RIGHT_CORNER] = {MZM_ZOOMER_TURNING_EDGE_RIGHT, MZM_ZOOMER_TURNING_CORNER_LEFT, true, false},
        [TURN_TOP_LEFT_CORNER] = {MZM_ZOOMER_TURNING_CORNER_LEFT, MZM_ZOOMER_TURNING_EDGE_RIGHT, false, true},
        [TURN_BOTTOM_LEFT_EDGE] = {MZM_ZOOMER_TURNING_EDGE_RIGHT, MZM_ZOOMER_TURNING_CORNER_LEFT, true, true},
    };
    if (s->work1 >= 8) {
        s->status = 0;
        return;
    }
    s->anim = right ? turns[s->work1].if_right : turns[s->work1].otherwise;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP);
    if (turns[s->work1].x_flip) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    if (turns[s->work1].y_flip) s->status |= MZM_SPRITE_STATUS_Y_FLIP;
}

/* ZoomerTurningAround: once the turn animation ends, the zoomer moves onto
 * the new surface. */
static void zoomer_turning(Ctx *c) {
    MzmSprite *s = c->s;
    if (!ai_animation_ending(c, 1)) return;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    s->pose = ZOOMER_POSE_IDLE;
    switch (s->work1) {
    case TURN_BOTTOM_LEFT_CORNER:
        if (!right) {
            s->y -= HALF_BLOCK_SIZE - PIXEL_SIZE;
            s->x &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_RIGHT_EDGE:
        if (!right) {
            s->y -= HALF_BLOCK_SIZE;
            s->x &= BLOCK_POSITION_FLAG;
            s->x += BLOCK_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_BOTTOM_RIGHT_EDGE:
        if (right) {
            s->y += HALF_BLOCK_SIZE - PIXEL_SIZE;
            s->x &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_RIGHT_CORNER:
        if (right) {
            s->y += HALF_BLOCK_SIZE;
            s->x &= BLOCK_POSITION_FLAG;
            s->x += BLOCK_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_LEFT_EDGE:
        if (right) {
            s->x += HALF_BLOCK_SIZE;
            s->y &= BLOCK_POSITION_FLAG;
            s->y += BLOCK_SIZE;
        } else {
            s->y &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP |
                                 MZM_SPRITE_STATUS_FACING_DOWN);
        break;
    case TURN_BOTTOM_RIGHT_CORNER:
        if (!right) {
            s->x -= HALF_BLOCK_SIZE;
            s->y &= BLOCK_POSITION_FLAG;
            s->y += BLOCK_SIZE;
        } else {
            s->y &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP |
                                 MZM_SPRITE_STATUS_FACING_DOWN);
        break;
    case TURN_TOP_LEFT_CORNER:
        if (right) {
            s->y &= BLOCK_POSITION_FLAG;
            s->x += HALF_BLOCK_SIZE - PIXEL_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_BOTTOM_LEFT_EDGE:
        if (!right) {
            s->y &= BLOCK_POSITION_FLAG;
            s->x -= HALF_BLOCK_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_Y_FLIP;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    default:
        s->status = 0;
        return;
    }
    zoomer_crawling_oam(s);
    zoomer_hitbox(s);
}

/* ZoomerFallingInit */
static void zoomer_falling_init(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) s->x -= HALF_BLOCK_SIZE;
        else s->x += HALF_BLOCK_SIZE;
    } else if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
        s->y += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    }
    s->pose = ZOOMER_POSE_FALLING;
    s->work3 = 0;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP |
                             MZM_SPRITE_STATUS_FACING_DOWN);
    zoomer_hitbox(s);
    zoomer_falling_oam(s);
}

/* ZoomerFalling: sSpritesFallingSpeed until its SHORT_MAX end, then the last
 * speed; landing on any collision (the splash of SpriteUtilCheckInRoomEffect
 * is not ported). */
static void zoomer_falling(Ctx *c) {
    MzmSprite *s = c->s;
    if (is_zeela(s)) s->adc++;      /* ZeelaFalling doubles the animation speed */
    uint8_t offset = s->work3;
    int16_t movement = offset < 16 ? c->tables->falling[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        s->y = (uint16_t)(s->y + c->tables->falling[offset > 0 ? offset - 1 : 0]);
    } else {
        s->work3++;
        s->y = (uint16_t)(s->y + movement);
    }
    uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = top;
        s->pose = ZOOMER_POSE_LANDING;
        ai_random_x_direction(c);
    }
}

/* ZoomerLanding */
static void zoomer_landing(Ctx *c) {
    if (is_zeela(c->s)) c->s->adc += 2;  /* ZeelaLanding triples it */
    if (zoomer_in_air(c)) c->s->pose = ZOOMER_POSE_FALLING_INIT;
    else if (ai_animation_ending(c, 2)) c->s->pose = ZOOMER_POSE_IDLE_INIT;
}

/* SpriteUtilDetermineEnemyDrop: the sprite's drop probabilities stacked
 * down from SPRITE_DROP_MAX_PROB (power bomb, super missile, missile, large
 * energy, small energy), with the substitutions for full ammunition and
 * energy. */
static uint8_t determine_drop(const Ctx *c) {
    const MzmEquipment *e = c->f->equipment;
    const uint16_t *probs = c->tables->drops[c->s->sprite_id];
    bool full_life = e->energy == e->max_energy;
    uint16_t rng = (uint16_t)(c->rng * 256);
    rng = (uint16_t)(c->frame8 + c->f->frame16 + rng);
    rng %= SPRITE_DROP_MAX_PROB;
    if (rng == 0) rng = 1;          /* not REGION_US_BETA */
    /* Ammunition drops, power bombs first (none ported: 0 of 0). */
    static const uint8_t ammo_drops[3] = {
        MZM_PSPRITE_POWER_BOMB_DROP, MZM_PSPRITE_SUPER_MISSILE_DROP, MZM_PSPRITE_MISSILE_DROP,
    };
    int current[3] = {0, e->super_missiles, e->missiles};
    int maximum[3] = {0, e->max_super_missiles, e->max_missiles};
    uint16_t upper = SPRITE_DROP_MAX_PROB;
    for (int i = 0; i < 3; ++i) {
        uint16_t prob = probs[MZM_DROP_PROB_POWER_BOMB - i];
        if (prob == 0) continue;
        uint16_t lower = (uint16_t)(upper - prob);
        if (upper >= rng && rng > lower) {
            if (maximum[i] > current[i]) return ammo_drops[i];
            if (!full_life) return MZM_PSPRITE_LARGE_ENERGY_DROP;
            return maximum[i] != 0 ? ammo_drops[i] : MZM_PSPRITE_SMALL_ENERGY_DROP;
        }
        upper = lower;
    }
    static const uint8_t energy_drops[2] = {
        MZM_PSPRITE_LARGE_ENERGY_DROP, MZM_PSPRITE_SMALL_ENERGY_DROP,
    };
    for (int i = 0; i < 2; ++i) {
        uint16_t prob = probs[MZM_DROP_PROB_LARGE_ENERGY - i];
        if (prob == 0) continue;
        uint16_t lower = (uint16_t)(upper - prob);
        if (upper >= rng && rng > lower) {
            if (full_life && e->max_missiles > e->missiles) return MZM_PSPRITE_MISSILE_DROP;
            return energy_drops[i];
        }
        upper = lower;
    }
    return MZM_PSPRITE_UNUSED0;
}

/* SpriteDebrisInit, for the sprites. */
void ai_debris_init(const Ctx *c, uint8_t type, uint16_t y, uint16_t x) {
    if (c->f->effects) mzm_sprite_debris_init(c->f->effects, type, y, x);
}

void ai_particle_set(const Ctx *c, uint16_t y, uint16_t x, uint8_t effect) {
    if (c->f->effects) mzm_particle_set(c->f->effects, y, x, effect);
}

/* SpriteUtilRandomSpriteDebris with three pieces. */
static void random_debris_3(const Ctx *c, uint16_t y, uint16_t x) {
    if (c->frame8 & 1) {
        ai_debris_init(c, 1, (uint16_t)(y - EIGHTH_BLOCK_SIZE), (uint16_t)(x - EIGHTH_BLOCK_SIZE));
        ai_debris_init(c, 3, (uint16_t)(y - QUARTER_BLOCK_SIZE), (uint16_t)(x + EIGHTH_BLOCK_SIZE));
        ai_debris_init(c, 4, (uint16_t)(y + EIGHTH_BLOCK_SIZE), (uint16_t)(x - EIGHTH_BLOCK_SIZE));
    } else {
        ai_debris_init(c, 2, (uint16_t)(y - QUARTER_BLOCK_SIZE), (uint16_t)(x + EIGHTH_BLOCK_SIZE));
        ai_debris_init(c, 3, (uint16_t)(y + EIGHTH_BLOCK_SIZE), (uint16_t)(x + EIGHTH_BLOCK_SIZE));
        ai_debris_init(c, 4, (uint16_t)(y + QUARTER_BLOCK_SIZE), (uint16_t)(x - EIGHTH_BLOCK_SIZE));
    }
}

/* SpriteUtilSpriteDeath (DEATH_NORMAL): the death particle and sound (the
 * screw attack kill adds three debris), then the sprite becomes its drop at
 * the given position, or is removed. Every ported caller passes playSound
 * TRUE. */
void ai_sprite_death_typed(Ctx *c, int type, uint16_t y, uint16_t x, uint8_t effect) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_SCREW_ATTACK_DESTROYED) {
        ai_particle_set(c, y, x, MZM_PE_SCREW_ATTACK_DESTROYED);
        random_debris_3(c, y, x);
        mzm_sfx_play(MZM_SOUND_SCREW_ATTACK_DESTROYED);
    } else {
        ai_particle_set(c, y, x, effect);
        switch (effect) {
        case MZM_PE_SPRITE_EXPLOSION_SMALL: mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_SMALL); break;
        case MZM_PE_SPRITE_EXPLOSION_MEDIUM: mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_MEDIUM); break;
        case MZM_PE_SPRITE_EXPLOSION_BIG: mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_BIG); break;
        case MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG:
            mzm_sfx_play(MZM_SOUND_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
            break;
        default: break;
        }
    }
    uint8_t drop = determine_drop(c);
    if (type != DEATH_NORMAL) {
        /* The drop follows the sprite's slot; the sprite itself stays. */
        if (drop != MZM_PSPRITE_UNUSED0 && type == DEATH_RESPAWNING)
            ai_spawn_drop_follower(c, drop, s->room_slot, 0, s->ram_slot, y, x, 0);
        return;
    }
    if (drop == MZM_PSPRITE_UNUSED0) {
        s->status = 0;
        return;
    }
    s->status = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN | MZM_SPRITE_STATUS_NOT_DRAWN;
    s->secondary = false;           /* properties = 0 */
    s->damaged = false;
    s->kill_off_screen = false;
    s->gfx_slot = 0;
    s->sprite_id = drop;
    s->y = y;
    s->x = x;
    s->pose = SPRITE_POSE_UNINITIALIZED;
    s->health = 0;
    s->isft = 0;
    s->palette_row = 0;
    s->absolute_palette_row = 0;
    s->ignore_samus_timer = 1;
    s->freeze_timer = 0;
}

/* ZeelaSpawnEyes: two eyes beside the body, thrown away from the surface. */
static void zeela_spawn_eyes(Ctx *c) {
    const MzmSprite *s = c->s;
    const uint16_t near = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE, far = THREE_QUARTER_BLOCK_SIZE;
    const uint8_t id = MZM_SSPRITE_ZEELA_EYES;
    const uint16_t flip = MZM_SPRITE_STATUS_X_FLIP;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        bool right_wall = s->status & MZM_SPRITE_STATUS_X_FLIP;
        uint16_t x = right_wall ? (uint16_t)(s->x - far) : (uint16_t)(s->x + far);
        uint16_t status = right_wall ? 0 : flip;
        ai_spawn_secondary(c, id, 0, s->gfx_slot, (uint16_t)(s->y - near), x, status);
        ai_spawn_secondary(c, id, 1, s->gfx_slot, (uint16_t)(s->y + near), x, status);
    } else if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
        ai_spawn_secondary(c, id, 1, s->gfx_slot, (uint16_t)(s->y + far), (uint16_t)(s->x - near), 0);
        ai_spawn_secondary(c, id, 1, s->gfx_slot, (uint16_t)(s->y + far), (uint16_t)(s->x + near), flip);
    } else {
        ai_spawn_secondary(c, id, 2, s->gfx_slot, (uint16_t)(s->y - far), (uint16_t)(s->x - near), 0);
        ai_spawn_secondary(c, id, 2, s->gfx_slot, (uint16_t)(s->y - far), (uint16_t)(s->x + near), flip);
    }
}

/* ZeelaEyes: thrown with sZeelaEyesFalling*Speed and a part speed (1, 2
 * or 3 subpixels a frame) while spinning; stopped by solid collision or by
 * touching Samus, then exploding until the table ends. */
static void zeela_eyes(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
        s->kill_off_screen = true;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = -EIGHTH_BLOCK_SIZE;
        s->hitbox_bottom = PIXEL_SIZE;
        s->hitbox_left = -EIGHTH_BLOCK_SIZE;
        s->hitbox_right = EIGHTH_BLOCK_SIZE;
        ai_set_anim(s, MZM_ZEELA_EYES_IDLE);
        s->work3 = 0;
        s->pose = ZEELA_EYES_POSE_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
        s->work1 = c->rng & 1;
        s->rotation = s->work1 ? 128 : 0;  /* PI */
        /* fallthrough */
    case ZEELA_EYES_POSE_MOVING: {
        if (ai_collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0) {
            s->pose = SPRITE_POSE_STOPPED;
            return;
        }
        const int16_t *speeds = s->room_slot == 1 ? c->tables->zeela_eyes_bottom
                                                  : c->tables->zeela_eyes_up;
        uint8_t offset = s->work3;
        int16_t movement = offset < 16 ? speeds[offset] : SHORT_MAX;
        if (movement == SHORT_MAX) {
            s->y = (uint16_t)(s->y + speeds[offset > 0 ? offset - 1 : 0]);
        } else {
            s->work3++;
            s->y = (uint16_t)(s->y + movement);
        }
        uint16_t x_movement = s->room_slot == 0 ? 1 : s->room_slot == 1 ? PIXEL_SIZE / 2
                                                                         : PIXEL_SIZE - 1;
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) s->x += x_movement;
        else s->x -= x_movement;
        /* PI / 6 - PI / PI */
        if (s->work1) s->rotation -= 20;
        else s->rotation += 20;
        break;
    }
    case ZEELA_EYES_POSE_EXPLODING:
        s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 1)) s->status = 0;
        break;
    default:                        /* stopped or destroyed */
        s->pose = ZEELA_EYES_POSE_EXPLODING;
        s->samus_collision = SSC_NONE;
        ai_set_anim(s, MZM_ZEELA_EYES_EXPLODING);
        break;
    }
}

void ai_sprite_death(Ctx *c, uint16_t y, uint16_t x, uint8_t effect) {
    ai_sprite_death_typed(c, DEATH_NORMAL, y, x, effect);
}

/* ZoomerDeath / ZeelaDeath: the explosion point is the middle of the body;
 * a red Zeela throws its eyes first. */
static void zoomer_death(Ctx *c) {
    const MzmSprite *s = c->s;
    if (s->sprite_id == MZM_PSPRITE_ZEELA_RED) zeela_spawn_eyes(c);
    uint16_t y = s->y, x = s->x;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) x -= HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        else x += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    } else {
        if (s->status & MZM_SPRITE_STATUS_Y_FLIP) y += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        else y -= HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    }
    ai_sprite_death(c, y, x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
}

/* Zoomer: the AI (its damaged sound in mzm_sprite_update). Freezing is not
 * ported. */
static void zoomer(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    /* SpriteUtilIsSpriteStunned */
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: zoomer_init(c); break;
    case ZOOMER_POSE_IDLE_INIT:
        zoomer_crawling_oam(s);
        s->pose = ZOOMER_POSE_IDLE;
        /* fallthrough */
    case ZOOMER_POSE_IDLE: zoomer_crawling(c); break;
    case ZOOMER_POSE_TURNING_AROUND_INIT:
        zoomer_turning_init(s);
        if (!s->status) break;
        /* fallthrough */
    case ZOOMER_POSE_TURNING_AROUND: zoomer_turning(c); break;
    case ZOOMER_POSE_LANDING_INIT:
        s->pose = ZOOMER_POSE_LANDING;
        zoomer_falling_oam(s);
        /* fallthrough */
    case ZOOMER_POSE_LANDING: zoomer_landing(c); break;
    case ZOOMER_POSE_FALLING_INIT:
        zoomer_falling_init(s);
        /* fallthrough */
    case ZOOMER_POSE_FALLING: zoomer_falling(c); break;
    default: zoomer_death(c); break;
    }
}

/* SpriteUtilChooseRandomXFlip */
static void random_x_flip(Ctx *c) {
    if (c->rng & 1) c->s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    else c->s->status |= MZM_SPRITE_STATUS_X_FLIP;
}

/* RipperInit */
static void ripper_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->hitbox_top = -(HALF_BLOCK_SIZE + PIXEL_SIZE);
    s->hitbox_bottom = 0;
    s->hitbox_left = -HALF_BLOCK_SIZE;
    s->hitbox_right = HALF_BLOCK_SIZE;
    s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
    s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
    s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    ai_set_anim(s, MZM_RIPPER_MOVING);
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = c->tables->health[s->sprite_id];
    s->y -= EIGHTH_BLOCK_SIZE;
    random_x_flip(c);
    s->pose = RIPPER_POSE_MOVING_INIT;
}

/* RipperMove: half a pixel per frame (purple: one), turning at a wall a
 * quarter block above its feet. */
static void ripper_move(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t speed = s->sprite_id == MZM_PSPRITE_RIPPER_PURPLE ? PIXEL_SIZE : PIXEL_SIZE / 2;
    uint16_t y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        if (ai_collision_at(c, y, (uint16_t)(s->x + HALF_BLOCK_SIZE + PIXEL_SIZE)) != MZM_COLLISION_SOLID)
            s->x += speed;
        else
            s->pose = RIPPER_POSE_TURNING_AROUND_INIT;
    } else {
        if (ai_collision_at(c, y, (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE))) == MZM_COLLISION_SOLID)
            s->pose = RIPPER_POSE_TURNING_AROUND_INIT;
        else
            s->x -= speed;
    }
}

/* Ripper: the AI (its damaged sound in mzm_sprite_update). Freezing is not
 * ported. */
static void ripper(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: ripper_init(c); break;
    case RIPPER_POSE_MOVING_INIT:
        s->pose = RIPPER_POSE_MOVING;
        ai_set_anim(s, MZM_RIPPER_MOVING);
        /* fallthrough */
    case RIPPER_POSE_MOVING: ripper_move(c); break;
    case RIPPER_POSE_TURNING_AROUND_INIT:
        s->pose = RIPPER_POSE_TURNING_AROUND_FIRST_PART;
        ai_set_anim(s, MZM_RIPPER_TURNING_AROUND);
        /* fallthrough */
    case RIPPER_POSE_TURNING_AROUND_FIRST_PART:
        if (ai_animation_ending(c, 1)) {
            s->status ^= MZM_SPRITE_STATUS_X_FLIP;
            s->pose = RIPPER_POSE_TURNING_AROUND_SECOND_PART;
            ai_set_anim(s, MZM_RIPPER_TURNING_AROUND_PART2);
        }
        break;
    case RIPPER_POSE_TURNING_AROUND_SECOND_PART:
        if (ai_animation_ending(c, 2)) s->pose = RIPPER_POSE_MOVING_INIT;
        break;
    default:
        ai_sprite_death(c, (uint16_t)(s->y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)), s->x,
                     MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        break;
    }
}

/* SpriteSpawnSecondary: the first free slot, or nothing. */
void ai_spawn_secondary(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot,
                            uint16_t y, uint16_t x, uint16_t status) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS) continue;
        memset(n, 0, sizeof *n);
        n->status = status | MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                    MZM_SPRITE_STATUS_NOT_DRAWN;
        n->secondary = true;
        n->gfx_slot = gfx_slot;
        n->sprite_id = sprite_id;
        n->y = y;
        n->x = x;
        n->room_slot = part;
        n->pose = SPRITE_POSE_UNINITIALIZED;
        n->ignore_samus_timer = 1;
        n->ram_slot = c->s->ram_slot;
        return;
    }
}

/* SkreeInit, SkreeIdleInit */
static void skree_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->samus_collision = SSC_HURTS_SAMUS;
    s->draw_top = 0;
    s->draw_bottom = BLOCK_SIZE * 2 / PIXEL_SIZE;
    s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    s->hitbox_top = 0;
    s->hitbox_bottom = BLOCK_SIZE + HALF_BLOCK_SIZE;
    s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
    s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    s->health = c->tables->health[s->sprite_id];
    s->y -= BLOCK_SIZE;
}

/* SkreeDetectSamus: Samus below it within 10 blocks, and within 150
 * subpixels horizontally, while it is on screen. */
static void skree_detect(Ctx *c) {
    MzmSprite *s = c->s;
    int samus_y = c->f->samus_y, samus_x = c->f->samus_x;
    if (samus_y <= s->y || samus_y - s->y >= SKREE_Y_RANGE) return;
    if (!(s->status & MZM_SPRITE_STATUS_ONSCREEN)) return;
    if (samus_x <= s->x - SKREE_X_RANGE || samus_x >= s->x + SKREE_X_RANGE) return;
    s->pose = SKREE_POSE_SPINNING_INIT;
}

/* SkreeGoingDownInit */
static void skree_going_down_init(Ctx *c) {
    MzmSprite *s = c->s;
    ai_set_anim(s, MZM_SKREE_GOING_DOWN);
    s->work3 = 0;
    s->work2 = 0;
    s->pose = SKREE_POSE_GOING_DOWN;
    if (s->x > c->f->samus_x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    ai_sound_onscreen(s, MZM_SOUND_SKREE_GOING_DOWN);
}

/* SkreeGoDown: sSkreeFallingSpeed down, drifting toward Samus faster every
 * frame (a quarter of the frame count, below a quarter block), until its
 * hitbox bottom lands. */
static void skree_go_down(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t top = ai_check_vertical_slopes(c, (uint16_t)(s->hitbox_bottom + s->y), s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = (uint16_t)(top - s->hitbox_bottom);
        s->pose = SKREE_POSE_CRASHING;
        s->work0 = 0;
        ai_sound_onscreen(s, MZM_SOUND_SKREE_CRASHING);
        return;
    }
    uint16_t x_movement = s->work2 / 4;
    uint8_t offset = s->work3;
    int16_t y_movement = offset < 16 ? c->tables->skree_falling[offset] : SHORT_MAX;
    if (y_movement == SHORT_MAX) {
        s->y = (uint16_t)(s->y + c->tables->skree_falling[offset > 0 ? offset - 1 : 0]);
    } else {
        s->work3++;
        s->y = (uint16_t)(s->y + y_movement);
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        if (ai_collision_at(c, (uint16_t)(s->y + BLOCK_SIZE), (uint16_t)(s->x + HALF_BLOCK_SIZE)) ==
            MZM_COLLISION_SOLID)
            return;
        s->x += x_movement;
    } else {
        if (ai_collision_at(c, (uint16_t)(s->y + BLOCK_SIZE), (uint16_t)(s->x - HALF_BLOCK_SIZE)) ==
            MZM_COLLISION_SOLID)
            return;
        s->x -= x_movement;
    }
    if (x_movement < QUARTER_BLOCK_SIZE) s->work2++;
}

/* SkreeCrashGround: four debris on impact, the crash table after 40 frames
 * (pOam only), the four explosion parts and the huge explosion after 60. */
static void skree_crash(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = s->y, x = s->x;
    s->work0++;
    if (s->work0 == 1) {
        y += BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        ai_debris_init(c, 0x11, (uint16_t)(y - QUARTER_BLOCK_SIZE), x);
        ai_debris_init(c, 0x12, y, (uint16_t)(x + (QUARTER_BLOCK_SIZE - PIXEL_SIZE)));
        ai_debris_init(c, 0x13, (uint16_t)(y - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2)),
                    (uint16_t)(x + (QUARTER_BLOCK_SIZE + PIXEL_SIZE)));
        ai_debris_init(c, 4, (uint16_t)(y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                    (uint16_t)(x - (QUARTER_BLOCK_SIZE + (QUARTER_BLOCK_SIZE - PIXEL_SIZE) +
                                    PIXEL_SIZE / 2)));
    } else if (s->work0 == 40) {
        s->anim = MZM_SKREE_CRASHING;
    } else if (s->work0 == 60) {
        uint8_t id = s->sprite_id == MZM_PSPRITE_SKREE_BLUE ? MZM_SSPRITE_BLUE_SKREE_EXPLOSION
                                                            : MZM_SSPRITE_SKREE_EXPLOSION;
        ai_spawn_secondary(c, id, SKREE_EXPLOSION_PART_GOING_UP, s->gfx_slot,
                        (uint16_t)(y - EIGHTH_BLOCK_SIZE), x, 0);
        ai_spawn_secondary(c, id, SKREE_EXPLOSION_PART_GOING_UP, s->gfx_slot,
                        (uint16_t)(y - EIGHTH_BLOCK_SIZE), x, MZM_SPRITE_STATUS_X_FLIP);
        ai_spawn_secondary(c, id, 1, s->gfx_slot, (uint16_t)(y + EIGHTH_BLOCK_SIZE),
                        (uint16_t)(x - (QUARTER_BLOCK_SIZE - PIXEL_SIZE)), 0);
        ai_spawn_secondary(c, id, 1, s->gfx_slot, (uint16_t)(y + EIGHTH_BLOCK_SIZE),
                        (uint16_t)(x + (QUARTER_BLOCK_SIZE - PIXEL_SIZE)), MZM_SPRITE_STATUS_X_FLIP);
        s->status = 0;
        ai_particle_set(c, (uint16_t)(y + HALF_BLOCK_SIZE + PIXEL_SIZE), x, MZM_PE_SPRITE_EXPLOSION_HUGE);
        mzm_sfx_play(MZM_SOUND_SKREE_EXPLODING);
    }
}

/* Skree: the AI (its damaged sound in mzm_sprite_update). Freezing is not
 * ported. */
static void skree(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        skree_init(c);
        /* fallthrough */
    case SKREE_POSE_IDLE_INIT:
        ai_set_anim(s, MZM_SKREE_IDLE);
        s->pose = SKREE_POSE_IDLE;
        /* fallthrough */
    case SKREE_POSE_IDLE: skree_detect(c); break;
    case SKREE_POSE_SPINNING_INIT:
        ai_set_anim(s, MZM_SKREE_SPINNING);
        s->pose = SKREE_POSE_SPINNING;
        /* fallthrough */
    case SKREE_POSE_SPINNING:
        if (ai_animation_ending(c, 2)) s->pose = SKREE_POSE_GOING_DOWN_INIT;
        break;
    case SKREE_POSE_GOING_DOWN_INIT:
        skree_going_down_init(c);
        /* fallthrough */
    case SKREE_POSE_GOING_DOWN: skree_go_down(c); break;
    case SKREE_POSE_CRASHING: skree_crash(c); break;
    default:
        ai_sprite_death(c, (uint16_t)(s->y + THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE), s->x,
                     MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        break;
    }
}

/* SkreeExplosionInit and SkreeExplosionMove: a part flies off diagonally,
 * spinning an eighth of a turn per frame, harmless after its second frame,
 * until its table ends; removed off screen. */
static void skree_explosion(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        s->kill_off_screen = true;
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -(QUARTER_BLOCK_SIZE - PIXEL_SIZE);
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE - PIXEL_SIZE;
        s->adc = 0;
        s->frame = 0;
        s->pose = SKREE_EXPLOSION_POSE_MOVING;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->y += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->status |= MZM_SPRITE_STATUS_DOUBLE_SIZE | MZM_SPRITE_STATUS_ROTATION_SCALING_WHOLE;
        s->rotation = 0;
        s->anim = s->room_slot != SKREE_EXPLOSION_PART_GOING_UP ? MZM_SKREE_EXPLOSION_GOING_DOWN
                                                                : MZM_SKREE_EXPLOSION_GOING_UP;
    }
    if (s->pose != SKREE_EXPLOSION_POSE_MOVING) return;
    if (s->frame > 1) s->ignore_samus_timer = 1;
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        s->x += EIGHTH_BLOCK_SIZE;
        s->rotation += PI_4;
    } else {
        s->x -= EIGHTH_BLOCK_SIZE;
        s->rotation -= PI_4;
    }
    if (s->room_slot != SKREE_EXPLOSION_PART_GOING_UP) s->y += PIXEL_SIZE;
    else s->y -= PIXEL_SIZE;
    if (ai_animation_ending(c, 1)) s->status = 0;
}

/* Waver: bouncing diagonally between walls, a pixel a frame horizontally
 * and half a pixel vertically (FACING_DOWN means moving up), starting
 * toward Samus (SpriteUtilMakeSpriteFaceSamusXFlip). The damaged sound and
 * freezing are not ported. */
static void waver(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
        ai_set_anim(s, MZM_WAVER_MOVING);
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS;
        if (s->x > c->f->samus_x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        else s->status |= MZM_SPRITE_STATUS_X_FLIP;
        s->pose = 9;
        return;
    }
    if (s->pose != 9) {
        ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        if (ai_collision_at(c, s->y, (uint16_t)(s->x + s->hitbox_right)) == MZM_COLLISION_AIR)
            s->x += PIXEL_SIZE;
        else
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    } else {
        if (ai_collision_at(c, s->y, (uint16_t)(s->x + s->hitbox_left)) == MZM_COLLISION_AIR)
            s->x -= PIXEL_SIZE;
        else
            s->status |= MZM_SPRITE_STATUS_X_FLIP;
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (ai_collision_at(c, (uint16_t)(s->y + s->hitbox_top), s->x) == MZM_COLLISION_AIR)
            s->y -= PIXEL_SIZE / 2;
        else
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    } else {
        if (ai_collision_at(c, (uint16_t)(s->y + s->hitbox_bottom), s->x) == MZM_COLLISION_AIR)
            s->y += PIXEL_SIZE / 2;
        else
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    }
}

/* SpriteUtilCheckSamusNearSpriteLeftRight: Samus's middle (her position
 * plus half her hitbox top) within the ranges. */
bool ai_samus_near_left_right(const Ctx *c, uint16_t y_range, uint16_t x_range) {
    uint16_t samus_y = (uint16_t)(c->f->samus_y + c->f->samus_hitbox_top / 2);
    uint16_t samus_x = c->f->samus_x, y = c->s->y, x = c->s->x;
    if (y > samus_y ? y - samus_y >= y_range : samus_y - y >= y_range) return false;
    return x > samus_x ? x - samus_x < x_range : samus_x - x < x_range;
}

uint8_t ai_samus_nslr(const Ctx *c, uint16_t y_range, uint16_t x_range) {
    if (!ai_samus_near_left_right(c, y_range, x_range)) return NSLR_OUT_OF_RANGE;
    return c->s->x > c->f->samus_x ? NSLR_LEFT : NSLR_RIGHT;
}

bool ai_check_event(const Ctx *c, uint8_t event) {
    return c->f->hooks && c->f->hooks->check_event &&
           c->f->hooks->check_event(c->f->hooks->context, event);
}

static void hook_set_event(const Ctx *c, uint8_t event) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->set_event) h->set_event(h->context, event);
}

static void hook_doors(const Ctx *c, int8_t timer) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->door_unlock_timer) h->door_unlock_timer(h->context, timer);
}

static void hook_remove_solid(const Ctx *c, uint16_t y, uint16_t x) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->remove_solid) h->remove_solid(h->context, y, x);
}

/* Sidehopper (sidehopper.c) and Dessgeega (dessgeega.c): the two routines
 * are parallel; the Sidehopper alone resizes its head hitbox when it jumps
 * and lands, and they differ by their tables, sizes and death. */
static bool is_dessgeega(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_DESSGEEGA ||
           s->sprite_id == MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM;
}

/* *CheckSamusNearLeftRight: within five blocks, facing her. */
static bool hopper_check_samus(Ctx *c) {
    if (!ai_samus_near_left_right(c, HOPPER_DETECTION_RANGE, HOPPER_DETECTION_RANGE)) return false;
    if (c->s->x > c->f->samus_x) c->s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else c->s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    return true;
}

static void hopper_jump_warning_init(MzmSprite *s) {
    s->pose = HOPPER_POSE_JUMP_WARNING;
    ai_set_anim(s, MZM_SIDEHOPPER_JUMP_WARNING);
}

/* *JumpingInit: a low jump when the RNG is odd. */
static void hopper_jumping_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = HOPPER_POSE_JUMPING;
    ai_set_anim(s, MZM_SIDEHOPPER_JUMPING);
    s->work3 = 0;
    s->work2 = c->rng & 1;
    if (!is_dessgeega(s)) {
        if (s->status & MZM_SPRITE_STATUS_Y_FLIP) s->hitbox_bottom = SIDEHOPPER_JUMPING_HEAD_HITBOX;
        else s->hitbox_top = -SIDEHOPPER_JUMPING_HEAD_HITBOX;
    }
    ai_sound_onscreen_once(s, is_dessgeega(s) ? MZM_SOUND_DESSGEEGA_JUMPING
                                              : MZM_SOUND_SIDEHOPPER_JUMPING);
}

static void hopper_landing_init(MzmSprite *s) {
    s->pose = HOPPER_POSE_LANDING;
    ai_set_anim(s, MZM_SIDEHOPPER_LANDING);
    if (!is_dessgeega(s)) {
        if (s->status & MZM_SPRITE_STATUS_Y_FLIP) s->hitbox_bottom = SIDEHOPPER_LANDING_HEAD_HITBOX;
        else s->hitbox_top = -SIDEHOPPER_LANDING_HEAD_HITBOX;
    }
    ai_sound_onscreen_once(s, is_dessgeega(s) ? MZM_SOUND_DESSGEEGA_LANDING
                                              : MZM_SOUND_SIDEHOPPER_LANDING);
}

/* The head shake (Sidehopper) or scream (Dessgeega) sound. */
static void hopper_idle_sound(const MzmSprite *s) {
    ai_sound_onscreen_once(s, is_dessgeega(s) ? MZM_SOUND_DESSGEEGA_SCREAMING
                                              : MZM_SOUND_SIDEHOPPER_SHAKING_HEAD);
}

/* *IdleInit: idle or shaking its head / screaming at random, jumping after
 * 1 to 4 animation loops. */
static void hopper_idle_init(Ctx *c) {
    MzmSprite *s = c->s;
    if (hopper_check_samus(c)) {
        hopper_jump_warning_init(s);
        return;
    }
    s->pose = HOPPER_POSE_IDLE;
    s->work0 = 0;
    s->work1 = c->rng & 3;
    ai_set_anim(s, c->rng >= 8 ? MZM_SIDEHOPPER_IDLE : MZM_SIDEHOPPER_SHAKING_HEAD);
    if (s->anim == MZM_SIDEHOPPER_SHAKING_HEAD) hopper_idle_sound(s);
}

static void hopper_falling_init(MzmSprite *s) {
    s->pose = HOPPER_POSE_FALLING;
    ai_set_anim(s, MZM_SIDEHOPPER_JUMPING);
    s->work3 = 0;
}

/* *Init. Without BUGFIX, work1 keeps its spawn value (0). */
static void hopper_init(Ctx *c) {
    MzmSprite *s = c->s;
    bool dess = is_dessgeega(s);
    if (s->sprite_id == MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM) {
        if (ai_check_event(c, MZM_EVENT_LONG_BEAM_DESSGEEGA_KILLED) ||
            !ai_check_event(c, MZM_EVENT_VIEWED_STATUE_ROOM)) {
            s->status = 0;
            return;
        }
        hook_doors(c, 1);           /* LOCK_DOORS */
        s->pose = DESSGEEGA_POSE_LONG_BEAM_DETECT;
    } else {
        if (ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE + PIXEL_SIZE)), s->x) &
            MZM_COLLISION_FLAGS_F0) {
            s->status |= MZM_SPRITE_STATUS_Y_FLIP;
            s->y -= BLOCK_SIZE;
        }
        s->pose = HOPPER_POSE_IDLE;
    }
    s->work0 = 0;
    int16_t head = dess ? DESSGEEGA_HEAD_HITBOX : SIDEHOPPER_HEAD_HITBOX;
    uint8_t head_draw = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
    uint8_t feet_draw = HALF_BLOCK_SIZE / PIXEL_SIZE;
    if (s->status & MZM_SPRITE_STATUS_Y_FLIP) {
        s->draw_top = feet_draw;
        s->draw_bottom = head_draw;
        s->hitbox_top = 0;
        s->hitbox_bottom = head;
    } else {
        s->draw_top = head_draw;
        s->draw_bottom = feet_draw;
        s->hitbox_top = -head;
        s->hitbox_bottom = 0;
    }
    s->draw_horizontal = (dess ? BLOCK_SIZE + HALF_BLOCK_SIZE
                               : BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
    s->hitbox_left = (int16_t)-(dess ? BLOCK_SIZE : BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
    s->hitbox_right = dess ? BLOCK_SIZE : BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    ai_set_anim(s, MZM_SIDEHOPPER_IDLE);
    s->samus_collision = SSC_HURTS_SAMUS;
    s->health = c->tables->health[s->sprite_id];
    ai_random_x_direction(c);
}

/* Both sides of the feet in the air (the ground variants start falling). */
static bool hopper_unsupported(Ctx *c) {
    MzmSprite *s = c->s;
    return ai_collision_at(c, s->y, (uint16_t)(s->x + s->hitbox_right)) == MZM_COLLISION_AIR &&
           ai_collision_at(c, s->y, (uint16_t)(s->x + s->hitbox_left)) == MZM_COLLISION_AIR;
}

/* *JumpWarningGround / *JumpWarningCeiling */
static void hopper_jump_warning(Ctx *c) {
    MzmSprite *s = c->s;
    if (!(s->status & MZM_SPRITE_STATUS_Y_FLIP) &&
        ai_collision_at(c, s->y, s->x) == MZM_COLLISION_AIR && hopper_unsupported(c)) {
        hopper_falling_init(s);
        return;
    }
    if (ai_animation_ending(c, 1)) hopper_jumping_init(c);
}

/* The vertical probe of a landing: on the ground any collision, on the
 * ceiling the low collision bits, with the block below the top. */
static bool hopper_lands_at(Ctx *c, uint16_t x, bool ceiling, uint16_t *top) {
    *top = ai_check_vertical_slopes(c, c->s->y, x);
    return ceiling ? (c->vertical & MZM_COLLISION_FLAGS_F) != 0 : c->vertical != MZM_COLLISION_AIR;
}

/* *JumpingGround / *JumpingCeiling: a pixel a frame across (a pixel and a
 * subpixel while rising), bouncing off walls; the jump table entry changes
 * every four frames. Rising into solid makes it fall; descending onto its
 * centre or either side lands it. */
static void hopper_jumping(Ctx *c) {
    MzmSprite *s = c->s;
    bool dess = is_dessgeega(s), ceiling = s->status & MZM_SPRITE_STATUS_Y_FLIP;
    const int16_t *table = s->work2 ? (dess ? c->tables->dessgeega_low : c->tables->sidehopper_low)
                                    : (dess ? c->tables->dessgeega_high : c->tables->sidehopper_high);
    int movement = table[s->work3 / 4];
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    int colliding = 0;
    uint16_t probe_y = ceiling ? (uint16_t)(s->y + QUARTER_BLOCK_SIZE)
                               : (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
    uint16_t probe_x = right ? (uint16_t)(s->x + s->hitbox_right + PIXEL_SIZE)
                             : (uint16_t)(s->x + s->hitbox_left - PIXEL_SIZE);
    int step = movement > 0 ? PIXEL_SIZE : PIXEL_SIZE + 1;
    if (ai_collision_at(c, probe_y, probe_x) == MZM_COLLISION_SOLID) {
        colliding++;
        s->x = (uint16_t)(s->x + (right ? -(PIXEL_SIZE + PIXEL_SIZE / 2) : PIXEL_SIZE + PIXEL_SIZE / 2));
    } else {
        s->x = (uint16_t)(s->x + (right ? step : -step));
    }
    s->y = (uint16_t)(s->y + (ceiling ? -movement : movement));
    if (s->work3 < HOPPER_JUMP_VELOCITY_FRAMES) s->work3++;
    bool descending = ceiling ? movement >= 0 : movement > 0;
    if (descending) {
        if (colliding) s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
        uint16_t top, land = 0;
        if (hopper_lands_at(c, s->x, ceiling, &top)) {
            s->y = ceiling ? (uint16_t)(top + BLOCK_SIZE) : top;
            hopper_landing_init(s);
            return;
        }
        if (colliding) return;
        if (hopper_lands_at(c, (uint16_t)(s->x + s->hitbox_right), ceiling, &top) ||
            hopper_lands_at(c, (uint16_t)(s->x + s->hitbox_left), ceiling, &top))
            land = 1;
        if (land) {
            s->y = ceiling ? (uint16_t)(top + BLOCK_SIZE) : top;
            hopper_landing_init(s);
        }
        return;
    }
    /* Rising: the head corner on the facing side against solid. */
    uint16_t head_y = ceiling ? (uint16_t)(s->y + s->hitbox_bottom) : (uint16_t)(s->y + s->hitbox_top);
    uint16_t head_x = s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? (uint16_t)(s->x + s->hitbox_right)
                                                                 : (uint16_t)(s->x + s->hitbox_left);
    if (ai_collision_at(c, head_y, head_x) == MZM_COLLISION_SOLID) {
        colliding++;
        s->x = (uint16_t)(s->x + (s->status & MZM_SPRITE_STATUS_FACING_RIGHT
                                      ? -(PIXEL_SIZE + PIXEL_SIZE / 2) : PIXEL_SIZE + PIXEL_SIZE / 2));
        hopper_falling_init(s);
    }
    if (colliding) s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
}

/* *FallingGround / *FallingCeiling: sSpritesFallingSpeed (ceiling:
 * sSpritesFallingCeilingSpeed) until the centre or a side lands. */
static void hopper_falling(Ctx *c) {
    MzmSprite *s = c->s;
    bool ceiling = s->status & MZM_SPRITE_STATUS_Y_FLIP;
    uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
    bool colliding = c->vertical != MZM_COLLISION_AIR;
    if (!colliding) {
        top = ai_check_vertical_slopes(c, s->y, (uint16_t)(s->x + s->hitbox_right));
        colliding = c->vertical != MZM_COLLISION_AIR;
    }
    if (!colliding) {
        top = ai_check_vertical_slopes(c, s->y, (uint16_t)(s->x + s->hitbox_left));
        colliding = c->vertical != MZM_COLLISION_AIR;
    }
    if (colliding) {
        s->y = ceiling ? (uint16_t)(top + BLOCK_SIZE) : top;
        hopper_landing_init(s);
        return;
    }
    const int16_t *speeds = ceiling ? c->tables->falling_ceiling : c->tables->falling;
    uint8_t offset = s->work3;
    int16_t movement = offset < 16 ? speeds[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        s->y = (uint16_t)(s->y + speeds[offset > 0 ? offset - 1 : 0]);
    } else {
        s->work3++;
        s->y = (uint16_t)(s->y + movement);
    }
}

/* *IdleGround / *IdleCeiling (the head shake / scream sounds are not
 * played). */
static void hopper_idle(Ctx *c) {
    MzmSprite *s = c->s;
    if (hopper_check_samus(c)) {
        hopper_jump_warning_init(s);
        return;
    }
    if (!(s->status & MZM_SPRITE_STATUS_Y_FLIP) && hopper_unsupported(c)) {
        hopper_falling_init(s);
        return;
    }
    if (ai_animation_ending(c, 1)) {
        if (s->work0++ == s->work1) hopper_jump_warning_init(s);
        else if (s->anim == MZM_SIDEHOPPER_SHAKING_HEAD) hopper_idle_sound(s);
    }
}

/* DessgeegaLongBeamDetectSamus: hidden from projectiles and Samus until she
 * is on screen within 12 blocks vertically and 3.75 horizontally, then it
 * drops with four debris (the screen shake and sound are not ported). */
static void dessgeega_long_beam_detect(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 1;
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    if (!(s->status & MZM_SPRITE_STATUS_ONSCREEN) ||
        !ai_samus_near_left_right(c, DESSGEEGA_LONG_BEAM_Y_RANGE, DESSGEEGA_LONG_BEAM_X_RANGE))
        return;
    s->pose = DESSGEEGA_POSE_LONG_BEAM_SPAWNING;
    ai_set_anim(s, MZM_DESSGEEGA_JUMPING);
    s->work3 = 0;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    uint16_t y = s->y, x = s->x;
    ai_debris_init(c, 5, (uint16_t)(y + THREE_QUARTER_BLOCK_SIZE), (uint16_t)(x + BLOCK_SIZE + QUARTER_BLOCK_SIZE));
    ai_debris_init(c, 7, (uint16_t)(y + QUARTER_BLOCK_SIZE),
                (uint16_t)(x + 3 * EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2));
    ai_debris_init(c, 8, (uint16_t)(y + BLOCK_SIZE),
                (uint16_t)(x - (BLOCK_SIZE + 3 * EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2)));
    ai_debris_init(c, 6, (uint16_t)(y + HALF_BLOCK_SIZE), (uint16_t)(x - QUARTER_BLOCK_SIZE));
}

/* DessgeegaLongBeamSpawning: down 24 subpixels a frame; the first floor it
 * meets loses three blocks (with dust and debris), the next lands it. */
static void dessgeega_long_beam_spawning(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
    if (c->vertical == MZM_COLLISION_AIR) {
        s->y += DESSGEEGA_SPAWNING_SPEED;
        return;
    }
    if (s->work3) {
        s->y = top;
        hopper_landing_init(s);
        return;
    }
    s->work3++;
    uint16_t y = s->y, x = s->x;
    hook_remove_solid(c, y, x);
    hook_remove_solid(c, y, (uint16_t)(x + BLOCK_SIZE));
    hook_remove_solid(c, y, (uint16_t)(x - BLOCK_SIZE));
    ai_particle_set(c, (uint16_t)(y - EIGHTH_BLOCK_SIZE),
                 (uint16_t)(x + 3 * QUARTER_BLOCK_SIZE + PIXEL_SIZE), MZM_PE_TWO_MEDIUM_DUST);
    ai_particle_set(c, (uint16_t)(y + EIGHTH_BLOCK_SIZE),
                 (uint16_t)(x - (3 * QUARTER_BLOCK_SIZE + PIXEL_SIZE)), MZM_PE_MEDIUM_DUST);
    ai_debris_init(c, 0x11, (uint16_t)(y - BLOCK_SIZE), x);
    ai_debris_init(c, 0x12, (uint16_t)(y - HALF_BLOCK_SIZE), (uint16_t)(x + BLOCK_SIZE - PIXEL_SIZE / 2));
    ai_debris_init(c, 0x13, (uint16_t)(y - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                (uint16_t)(x - (BLOCK_SIZE + HALF_BLOCK_SIZE - PIXEL_SIZE)));
    ai_debris_init(c, 4, (uint16_t)(y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE)),
                (uint16_t)(x + HALF_BLOCK_SIZE - PIXEL_SIZE / 2));
}

/* SidehopperDeath / DessgeegaDeath. Every ground Dessgeega (not only the
 * Long Beam one) sets the Long Beam event and unlocks the doors: a bug of
 * the game, kept. */
static void hopper_death(Ctx *c) {
    MzmSprite *s = c->s;
    bool dess = is_dessgeega(s), ceiling = s->status & MZM_SPRITE_STATUS_Y_FLIP;
    uint16_t offset = dess ? THREE_QUARTER_BLOCK_SIZE : THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE;
    if (dess && !ceiling) {
        hook_set_event(c, MZM_EVENT_LONG_BEAM_DESSGEEGA_KILLED);
        hook_doors(c, -20);         /* -ONE_THIRD_SECOND */
    }
    ai_sprite_death(c, ceiling ? (uint16_t)(s->y + offset) : (uint16_t)(s->y - offset), s->x,
                 MZM_PE_SPRITE_EXPLOSION_SINGLE_THEN_BIG);
}

/* Sidehopper / Dessgeega: the AIs. Freezing is not ported. */
static void hopper(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: hopper_init(c); break;
    case DESSGEEGA_POSE_LONG_BEAM_DETECT:
        if (is_dessgeega(s)) dessgeega_long_beam_detect(c);
        else hopper_death(c);
        break;
    case DESSGEEGA_POSE_LONG_BEAM_SPAWNING:
        if (is_dessgeega(s)) dessgeega_long_beam_spawning(c);
        else hopper_death(c);
        break;
    case HOPPER_POSE_JUMP_WARNING_INIT:
        hopper_jump_warning_init(s);
        /* fallthrough */
    case HOPPER_POSE_JUMP_WARNING: hopper_jump_warning(c); break;
    case HOPPER_POSE_JUMPING: hopper_jumping(c); break;
    case HOPPER_POSE_LANDING:
        if (ai_animation_ending(c, 1)) hopper_idle_init(c);
        break;
    case HOPPER_POSE_IDLE: hopper_idle(c); break;
    case HOPPER_POSE_FALLING: hopper_falling(c); break;
    default: hopper_death(c); break;
    }
}

/* Viola poses (viola.c): the surface it crawls on. */
enum {
    VIOLA_POSE_ON_FLOOR = 0x9,      /* VIOLA_POSE_MOVE_RIGHT */
    VIOLA_POSE_FALLING_INIT = 0x1E,
    VIOLA_POSE_FALLING = 0x1F,
    VIOLA_POSE_ON_CEILING = 0x23,   /* VIOLA_POSE_MOVE_LEFT */
    VIOLA_POSE_ON_LEFT_WALL = 0x25, /* VIOLA_POSE_MOVE_DOWN */
    VIOLA_POSE_ON_RIGHT_WALL = 0x27,    /* VIOLA_POSE_MOVE_UP */
    VIOLA_SIZE = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
};

/* ViolaInit: half a block up, then onto the floor, ceiling, left or right
 * wall found around it (or removed); orange Violas move twice as fast. */
static void viola_init(Ctx *c) {
    MzmSprite *s = c->s;
    ai_random_x_direction(c);
    s->y -= HALF_BLOCK_SIZE;
    if (ai_collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE), s->x) & MZM_COLLISION_FLAGS_F0) {
        s->y += EIGHTH_BLOCK_SIZE;
        s->pose = VIOLA_POSE_ON_FLOOR;
    } else if (ai_collision_at(c, (uint16_t)(s->y - (HALF_BLOCK_SIZE + PIXEL_SIZE)), s->x) &
               MZM_COLLISION_FLAGS_F0) {
        s->y -= EIGHTH_BLOCK_SIZE;
        s->pose = VIOLA_POSE_ON_CEILING;
    } else if (ai_collision_at(c, s->y, (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE))) &
               MZM_COLLISION_FLAGS_F0) {
        s->x -= EIGHTH_BLOCK_SIZE;
        s->pose = VIOLA_POSE_ON_LEFT_WALL;
    } else if (ai_collision_at(c, s->y, (uint16_t)(s->x + HALF_BLOCK_SIZE)) & MZM_COLLISION_FLAGS_F0) {
        s->x += EIGHTH_BLOCK_SIZE;
        s->pose = VIOLA_POSE_ON_RIGHT_WALL;
    } else {
        s->status = 0;
        return;
    }
    s->samus_collision = SSC_HURTS_SAMUS;
    ai_set_anim(s, MZM_VIOLA_MOVING);
    s->health = c->tables->health[s->sprite_id];
    s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    s->hitbox_top = s->hitbox_left = -(QUARTER_BLOCK_SIZE + PIXEL_SIZE);
    s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE + PIXEL_SIZE;
    s->work2 = s->sprite_id == MZM_PSPRITE_VIOLA_ORANGE ? PIXEL_SIZE : PIXEL_SIZE / 2;
}

/* ViolaMoveRight: on the floor, following slopes. */
static void viola_on_floor(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t v = s->work2, y = (uint16_t)(s->y + VIOLA_SIZE), x = s->x;
    if (ai_collision_at(c, y, (uint16_t)(x + VIOLA_SIZE + PIXEL_SIZE)) == MZM_COLLISION_AIR &&
        ai_collision_at(c, y, (uint16_t)(x - HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR) {
        s->pose = VIOLA_POSE_FALLING_INIT;
        return;
    }
    uint16_t top = ai_check_vertical(c, (uint16_t)(y - PIXEL_SIZE), x);
    if ((c->vertical & MZM_COLLISION_FLAGS_F) >= MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
        s->y = (uint16_t)(top - VIOLA_SIZE);
    } else {
        top = ai_check_vertical(c, y, x);
        if ((c->vertical & MZM_COLLISION_FLAGS_F) >= MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE) {
            s->y = (uint16_t)(top - VIOLA_SIZE);
        } else {
            top = ai_check_vertical(c, (uint16_t)(y + PIXEL_SIZE), x);
            if (c->vertical != MZM_COLLISION_AIR) s->y = (uint16_t)(top - VIOLA_SIZE);
        }
    }
    y = (uint16_t)(s->y + VIOLA_SIZE);
    if (c->vertical != MZM_COLLISION_AIR && !(c->vertical & MZM_COLLISION_FLAGS_F0)) {
        /* On a slope: straight on. */
        if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) s->x += v;
        else s->x -= v;
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        if (ai_collision_at(c, y, (uint16_t)(x - VIOLA_SIZE)) == MZM_COLLISION_AIR) {
            if (ai_collision_at(c, y, (uint16_t)(x + VIOLA_SIZE)) == MZM_COLLISION_AIR) {
                s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
                s->pose = VIOLA_POSE_ON_LEFT_WALL;
            } else {
                s->x += v;
            }
        } else if (ai_collision_at(c, (uint16_t)(y - PIXEL_SIZE), (uint16_t)(x + VIOLA_SIZE)) ==
                   MZM_COLLISION_SOLID) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = VIOLA_POSE_ON_RIGHT_WALL;
        } else {
            s->x += v;
        }
    } else {
        if (ai_collision_at(c, y, (uint16_t)(x + (VIOLA_SIZE - PIXEL_SIZE))) == MZM_COLLISION_AIR) {
            if (ai_collision_at(c, y, (uint16_t)(x - (VIOLA_SIZE + PIXEL_SIZE))) == MZM_COLLISION_AIR) {
                s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
                s->pose = VIOLA_POSE_ON_RIGHT_WALL;
            } else {
                s->x -= v;
            }
        } else if (ai_collision_at(c, (uint16_t)(y - PIXEL_SIZE),
                                (uint16_t)(x - (VIOLA_SIZE + PIXEL_SIZE))) == MZM_COLLISION_SOLID) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = VIOLA_POSE_ON_LEFT_WALL;
        } else {
            s->x -= v;
        }
    }
}

/* ViolaMoveLeft: on the ceiling, snapped under its block. */
static void viola_on_ceiling(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t block_y = s->y &= BLOCK_POSITION_FLAG;
    s->y += VIOLA_SIZE;
    uint16_t v = s->work2, y = (uint16_t)(block_y - PIXEL_SIZE), x = s->x;
    if (ai_collision_at(c, y, (uint16_t)(x + VIOLA_SIZE + PIXEL_SIZE)) == MZM_COLLISION_AIR &&
        ai_collision_at(c, y, (uint16_t)(x - HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR) {
        s->pose = VIOLA_POSE_FALLING_INIT;
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        if (!(ai_collision_at(c, y, (uint16_t)(x - VIOLA_SIZE)) & MZM_COLLISION_FLAGS_F)) {
            if (ai_collision_at(c, y, (uint16_t)(x + VIOLA_SIZE)) == MZM_COLLISION_AIR) {
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
                s->pose = VIOLA_POSE_ON_LEFT_WALL;
            } else {
                s->x += v;
            }
        } else if (ai_collision_at(c, (uint16_t)(y + PIXEL_SIZE), (uint16_t)(x + VIOLA_SIZE)) ==
                   MZM_COLLISION_SOLID) {
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = VIOLA_POSE_ON_RIGHT_WALL;
        } else {
            s->x += v;
        }
    } else {
        if (!(ai_collision_at(c, y, (uint16_t)(x + (VIOLA_SIZE - PIXEL_SIZE))) & MZM_COLLISION_FLAGS_F)) {
            if (ai_collision_at(c, y, (uint16_t)(x - (VIOLA_SIZE + PIXEL_SIZE))) == MZM_COLLISION_AIR) {
                s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
                s->pose = VIOLA_POSE_ON_RIGHT_WALL;
            } else {
                s->x -= v;
            }
        } else if (ai_collision_at(c, (uint16_t)(y + PIXEL_SIZE),
                                (uint16_t)(x - (VIOLA_SIZE + PIXEL_SIZE))) == MZM_COLLISION_SOLID) {
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = VIOLA_POSE_ON_LEFT_WALL;
        } else {
            s->x -= v;
        }
    }
}

/* ViolaMoveDown / ViolaMoveUp: on a left or right wall, snapped beside its
 * block (facing right means moving down). */
static void viola_on_wall(Ctx *c, bool right_wall) {
    MzmSprite *s = c->s;
    uint16_t block_x = s->x &= BLOCK_POSITION_FLAG;
    s->x += right_wall ? HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE : VIOLA_SIZE;
    uint16_t v = s->work2, y = s->y;
    uint16_t x = right_wall ? (uint16_t)(block_x + BLOCK_SIZE) : (uint16_t)(block_x - PIXEL_SIZE);
    int16_t inward = right_wall ? -PIXEL_SIZE : PIXEL_SIZE;
    uint16_t above = right_wall ? HALF_BLOCK_SIZE : VIOLA_SIZE + EIGHTH_BLOCK_SIZE;
    if (ai_collision_at(c, (uint16_t)(y + VIOLA_SIZE + PIXEL_SIZE), x) == MZM_COLLISION_AIR &&
        ai_collision_at(c, (uint16_t)(y - above), x) == MZM_COLLISION_AIR) {
        s->pose = VIOLA_POSE_FALLING_INIT;
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
        if (!(ai_collision_at(c, (uint16_t)(y - VIOLA_SIZE), x) & MZM_COLLISION_FLAGS_F0)) {
            if (ai_collision_at(c, (uint16_t)(y + VIOLA_SIZE), x) == MZM_COLLISION_AIR) {
                if (right_wall) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
                else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
                s->pose = VIOLA_POSE_ON_CEILING;
            } else {
                s->y += v;
            }
        } else if (ai_collision_at(c, (uint16_t)(y + VIOLA_SIZE), (uint16_t)(x + inward)) ==
                   MZM_COLLISION_SOLID) {
            if (right_wall) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            else s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = VIOLA_POSE_ON_FLOOR;
        } else {
            s->y += v;
        }
    } else {
        if (!(ai_collision_at(c, (uint16_t)(y + (VIOLA_SIZE - PIXEL_SIZE)), x) & MZM_COLLISION_FLAGS_F0)) {
            if (ai_collision_at(c, (uint16_t)(y - (VIOLA_SIZE + PIXEL_SIZE)), x) == MZM_COLLISION_AIR) {
                if (right_wall) s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
                else s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
                s->pose = VIOLA_POSE_ON_FLOOR;
            } else {
                s->y -= v;
            }
        } else if (ai_collision_at(c, (uint16_t)(y - (VIOLA_SIZE + PIXEL_SIZE)), (uint16_t)(x + inward)) ==
                   MZM_COLLISION_SOLID) {
            if (right_wall) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
            else s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            s->pose = VIOLA_POSE_ON_CEILING;
        } else {
            s->y -= v;
        }
    }
}

/* ViolaFalling: sSpritesFallingSpeed until it lands on the floor. */
static void viola_falling(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t top = ai_check_vertical_slopes(c, (uint16_t)(s->y + VIOLA_SIZE), s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = (uint16_t)(top - VIOLA_SIZE);
        s->pose = VIOLA_POSE_ON_FLOOR;
        ai_random_x_direction(c);
        return;
    }
    uint8_t offset = s->work3;
    int16_t movement = offset < 16 ? c->tables->falling[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        s->y = (uint16_t)(s->y + c->tables->falling[offset > 0 ? offset - 1 : 0]);
    } else {
        s->work3 = (uint8_t)(offset + 1);
        s->y = (uint16_t)(s->y + movement);
    }
}

/* Viola: the AI (its damaged sound in mzm_sprite_update). Freezing is not
 * ported. */
static void viola(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: viola_init(c); break;
    case VIOLA_POSE_ON_FLOOR: viola_on_floor(c); break;
    case VIOLA_POSE_ON_CEILING: viola_on_ceiling(c); break;
    case VIOLA_POSE_ON_LEFT_WALL: viola_on_wall(c, false); break;
    case VIOLA_POSE_ON_RIGHT_WALL: viola_on_wall(c, true); break;
    case VIOLA_POSE_FALLING_INIT:
        s->pose = VIOLA_POSE_FALLING;
        s->work3 = 0;
        /* fallthrough */
    case VIOLA_POSE_FALLING: viola_falling(c); break;
    default: ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM); break;
    }
}

/* Sova (sova.c): a crawler like the Zoomer, but its ceiling state is work2
 * (no Y flip: it has its own tables), its speed follows the animation frame
 * and its turns and probes use slightly different distances. */
static void sova_set_x_flip_by_facing(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
}

/* SovaCheckCollidingWithAir */
static bool sova_in_air(const Ctx *c) {
    const MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        uint16_t x = s->status & MZM_SPRITE_STATUS_X_FLIP ? s->x : (uint16_t)(s->x - PIXEL_SIZE);
        return ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), x) == MZM_COLLISION_AIR &&
               ai_collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE), x) == MZM_COLLISION_AIR;
    }
    uint16_t y = s->work2 ? (uint16_t)(s->y - PIXEL_SIZE) : s->y;
    return ai_collision_at(c, y, (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR &&
           ai_collision_at(c, y, (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR;
}

/* SovaUpdateHitbox */
static void sova_hitbox(MzmSprite *s) {
    const int16_t top = THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE, bottom = PIXEL_SIZE;
    const int16_t side = HALF_BLOCK_SIZE - PIXEL_SIZE;
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        s->hitbox_top = -side;
        s->hitbox_bottom = side;
        s->hitbox_left = s->status & MZM_SPRITE_STATUS_X_FLIP ? -top : -bottom;
        s->hitbox_right = s->status & MZM_SPRITE_STATUS_X_FLIP ? bottom : top;
    } else {
        s->hitbox_top = s->work2 ? -bottom : -top;
        s->hitbox_bottom = s->work2 ? top : bottom;
        s->hitbox_left = -side;
        s->hitbox_right = side;
    }
}

/* SovaSetCrawlingOam */
static void sova_crawling_oam(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        ai_set_anim(s, s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? MZM_SOVA_WALKING_ON_RIGHT
                                                               : MZM_SOVA_WALKING_ON_LEFT);
    } else {
        ai_set_anim(s, s->work2 ? MZM_SOVA_WALKING_ON_CEILING : MZM_SOVA_ON_GROUND);
        sova_set_x_flip_by_facing(s);
    }
}

/* SovaInit */
static void sova_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->work2 = 0;
    s->pose = ZOOMER_POSE_IDLE;
    ai_random_x_direction(c);
    if (ai_collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    } else if (ai_collision_at(c, (uint16_t)(s->y - (BLOCK_SIZE + PIXEL_SIZE)), s->x) &
               MZM_COLLISION_FLAGS_F0) {
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        s->y -= BLOCK_SIZE;
        s->work2 = 1;
    } else if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                            (uint16_t)(s->x - (HALF_BLOCK_SIZE + PIXEL_SIZE))) &
               MZM_COLLISION_FLAGS_F0) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        s->y -= HALF_BLOCK_SIZE;
        s->x -= HALF_BLOCK_SIZE;
    } else if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE),
                            (uint16_t)(s->x + HALF_BLOCK_SIZE)) & MZM_COLLISION_FLAGS_F0) {
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN | MZM_SPRITE_STATUS_X_FLIP;
        s->y -= HALF_BLOCK_SIZE;
        s->x += HALF_BLOCK_SIZE;
    } else {
        s->status = 0;
        return;
    }
    if (!(s->status & MZM_SPRITE_STATUS_FACING_DOWN)) sova_set_x_flip_by_facing(s);
    s->samus_collision = SSC_HURTS_SAMUS;
    sova_crawling_oam(s);
    sova_hitbox(s);
    s->health = c->tables->health[s->sprite_id];
    s->draw_top = s->draw_bottom = s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
    if (s->sprite_id == MZM_PSPRITE_SOVA_ORANGE) s->absolute_palette_row = s->palette_row = 1;
}

/* SovaMove */
static void sova_move(Ctx *c) {
    MzmSprite *s = c->s;
    static const uint8_t frame_speeds[6] = {0, 0, 1, 2, 3, 1};
    uint16_t speed = s->frame < 6 ? frame_speeds[s->frame] : 0;
    if (s->sprite_id == MZM_PSPRITE_SOVA_ORANGE && speed) speed++;
    bool turning = false, right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    const uint16_t reach = HALF_BLOCK_SIZE - PIXEL_SIZE;
    if (sova_in_air(c)) {
        s->pose = ZOOMER_POSE_FALLING_INIT;
        return;
    }
    /* SpriteUtilShouldFall: only during a screen shake, not ported. */
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            if (right) {
                if (!(ai_collision_at(c, s->y, s->x) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, true, TURN_BOTTOM_LEFT_EDGE);
                else if (ai_collision_at(c, (uint16_t)(s->y + reach), (uint16_t)(s->x - PIXEL_SIZE)) ==
                         MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_RIGHT_CORNER);
                else
                    s->y += speed;
            } else {
                if (!(ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), s->x) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, true, TURN_BOTTOM_RIGHT_CORNER);
                else if (ai_collision_at(c, (uint16_t)(s->y - reach), (uint16_t)(s->x - PIXEL_SIZE)) ==
                         MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_LEFT_EDGE);
                else
                    s->y -= speed;
            }
        } else {
            if (right) {
                if (!(ai_collision_at(c, s->y, (uint16_t)(s->x - PIXEL_SIZE)) & MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, false, TURN_TOP_LEFT_CORNER);
                else if (ai_collision_at(c, (uint16_t)(s->y + reach), s->x) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, true, TURN_TOP_LEFT_EDGE);
                else
                    s->y += speed;
            } else {
                if (!(ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), (uint16_t)(s->x - PIXEL_SIZE)) &
                      MZM_COLLISION_FLAGS_F0))
                    turning = zoomer_turn(s, false, TURN_TOP_LEFT_EDGE);
                else if (ai_collision_at(c, (uint16_t)(s->y - reach), s->x) == MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, true, TURN_TOP_LEFT_CORNER);
                else
                    s->y -= speed;
            }
        }
    } else if (s->work2) {
        if (right) {
            if (!(ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), s->x) & MZM_COLLISION_FLAGS_F))
                turning = zoomer_turn(s, false, TURN_BOTTOM_RIGHT_EDGE);
            else if (ai_collision_at(c, s->y, (uint16_t)(s->x + reach)) == MZM_COLLISION_SOLID)
                turning = zoomer_turn(s, true, TURN_TOP_RIGHT_CORNER);
            else
                s->x += speed;
        } else {
            if (!(ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), (uint16_t)(s->x - PIXEL_SIZE)) &
                  MZM_COLLISION_FLAGS_F))
                turning = zoomer_turn(s, false, TURN_TOP_RIGHT_CORNER);
            else if (ai_collision_at(c, s->y, (uint16_t)(s->x - reach)) == MZM_COLLISION_SOLID)
                turning = zoomer_turn(s, true, TURN_BOTTOM_RIGHT_EDGE);
            else
                s->x -= speed;
        }
    } else {
        ai_align_on_slope(c);
        if (c->vertical == MZM_COLLISION_AIR || (c->vertical & MZM_COLLISION_FLAGS_F0)) {
            if (s->anim != MZM_SOVA_ON_GROUND) {
                ai_set_anim(s, MZM_SOVA_ON_GROUND);
                sova_set_x_flip_by_facing(s);
            }
            if (right) {
                if (ai_collision_at(c, s->y, s->x) == MZM_COLLISION_AIR)
                    turning = zoomer_turn(s, true, TURN_BOTTOM_LEFT_CORNER);
                else if (ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), (uint16_t)(s->x + reach)) ==
                         MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_TOP_RIGHT_EDGE);
                else
                    s->x += speed;
            } else {
                if (ai_collision_at(c, s->y, (uint16_t)(s->x - PIXEL_SIZE)) == MZM_COLLISION_AIR)
                    turning = zoomer_turn(s, true, TURN_TOP_RIGHT_EDGE);
                else if (ai_collision_at(c, (uint16_t)(s->y - PIXEL_SIZE), (uint16_t)(s->x - reach)) ==
                         MZM_COLLISION_SOLID)
                    turning = zoomer_turn(s, false, TURN_BOTTOM_LEFT_CORNER);
                else
                    s->x -= speed;
            }
        } else {
            /* On a slope: going right, a left slope uses the right slope
             * table at full speed, any other the left one at two thirds;
             * going left, the reverse. */
            bool left_slope = c->vertical == MZM_COLLISION_LEFT_SLIGHT_FLOOR_SLOPE ||
                              c->vertical == MZM_COLLISION_LEFT_STEEP_FLOOR_SLOPE;
            bool right_slope = c->vertical == MZM_COLLISION_RIGHT_SLIGHT_FLOOR_SLOPE ||
                               c->vertical == MZM_COLLISION_RIGHT_STEEP_FLOOR_SLOPE;
            bool full = right ? left_slope : right_slope;
            uint8_t anim = full ? MZM_SOVA_WALKING_ON_RIGHT_SLOPE : MZM_SOVA_WALKING_ON_LEFT_SLOPE;
            if (s->anim != anim) {
                ai_set_anim(s, anim);
                if (right) s->status |= MZM_SPRITE_STATUS_X_FLIP;
                else s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
            }
            uint16_t step = full ? speed : (uint16_t)(speed * 2 / 3);
            if (right) s->x += step;
            else s->x -= step;
        }
    }
    if (turning) s->pose = ZOOMER_POSE_TURNING_AROUND_INIT;
}

/* SovaTurningAroundInit: the corner table, flipped for the turn. */
static void sova_turning_init(MzmSprite *s) {
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    s->pose = ZOOMER_POSE_TURNING_AROUND;
    s->adc = 0;
    s->frame = 0;
    static const struct { uint8_t if_right, otherwise; bool x_flip, y_flip; } turns[8] = {
        [TURN_BOTTOM_LEFT_CORNER] = {MZM_SOVA_TURNING_CORNER_RIGHT, MZM_SOVA_TURNING_EDGE_LEFT, false, false},
        [TURN_TOP_RIGHT_EDGE] = {MZM_SOVA_TURNING_CORNER_RIGHT, MZM_SOVA_TURNING_EDGE_LEFT, true, false},
        [TURN_BOTTOM_RIGHT_EDGE] = {MZM_SOVA_TURNING_EDGE_LEFT, MZM_SOVA_TURNING_CORNER_RIGHT, false, true},
        [TURN_TOP_RIGHT_CORNER] = {MZM_SOVA_TURNING_EDGE_LEFT, MZM_SOVA_TURNING_CORNER_RIGHT, true, true},
        [TURN_TOP_LEFT_EDGE] = {MZM_SOVA_TURNING_CORNER_LEFT, MZM_SOVA_TURNING_EDGE_RIGHT, false, false},
        [TURN_BOTTOM_RIGHT_CORNER] = {MZM_SOVA_TURNING_EDGE_RIGHT, MZM_SOVA_TURNING_CORNER_LEFT, true, false},
        [TURN_TOP_LEFT_CORNER] = {MZM_SOVA_TURNING_CORNER_LEFT, MZM_SOVA_TURNING_EDGE_RIGHT, false, true},
        [TURN_BOTTOM_LEFT_EDGE] = {MZM_SOVA_TURNING_EDGE_RIGHT, MZM_SOVA_TURNING_CORNER_LEFT, true, true},
    };
    if (s->work1 >= 8) {
        s->status = 0;
        return;
    }
    s->anim = right ? turns[s->work1].if_right : turns[s->work1].otherwise;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP);
    if (turns[s->work1].x_flip) s->status |= MZM_SPRITE_STATUS_X_FLIP;
    if (turns[s->work1].y_flip) s->status |= MZM_SPRITE_STATUS_Y_FLIP;
}

/* SovaTurningAround: onto the new surface once the turn table ends. */
static void sova_turning(Ctx *c) {
    MzmSprite *s = c->s;
    if (!ai_animation_ending(c, 1)) return;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    s->pose = ZOOMER_POSE_IDLE;
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_Y_FLIP;
    s->work2 = 0;
    const uint16_t near = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE, far = HALF_BLOCK_SIZE - PIXEL_SIZE;
    switch (s->work1) {
    case TURN_BOTTOM_LEFT_CORNER:
        if (!right) {
            s->y -= near;
            s->x &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_RIGHT_EDGE:
        if (!right) {
            s->y -= far;
            s->x &= BLOCK_POSITION_FLAG;
            s->x += BLOCK_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_BOTTOM_RIGHT_EDGE:
        if (right) {
            s->y += near;
            s->x &= BLOCK_POSITION_FLAG;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_RIGHT_CORNER:
        if (right) {
            s->y += far;
            s->x &= BLOCK_POSITION_FLAG;
            s->x += BLOCK_SIZE;
        }
        s->status |= MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_LEFT_EDGE:
        if (right) {
            s->x += far;
            s->y &= BLOCK_POSITION_FLAG;
            s->y += BLOCK_SIZE;
            s->status |= MZM_SPRITE_STATUS_X_FLIP;
        } else {
            s->y &= BLOCK_POSITION_FLAG;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_BOTTOM_RIGHT_CORNER:
        if (!right) {
            s->x -= far;
            s->y &= BLOCK_POSITION_FLAG;
            s->y += BLOCK_SIZE;
            s->status |= MZM_SPRITE_STATUS_X_FLIP;
        } else {
            s->y &= BLOCK_POSITION_FLAG;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        break;
    case TURN_TOP_LEFT_CORNER:
        if (right) {
            s->y &= BLOCK_POSITION_FLAG;
            s->x += near;
            s->status |= MZM_SPRITE_STATUS_X_FLIP;
        } else {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        s->work2 = 1;
        break;
    case TURN_BOTTOM_LEFT_EDGE:
        if (!right) {
            s->y &= BLOCK_POSITION_FLAG;
            s->x -= far;
            s->status |= MZM_SPRITE_STATUS_X_FLIP;
        } else {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        }
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
        s->work2 = 1;
        break;
    default:
        s->status = 0;
        return;
    }
    sova_crawling_oam(s);
    sova_hitbox(s);
}

/* SovaLandingInit: the falling table again, a new random direction. */
static void sova_landing_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->pose = ZOOMER_POSE_LANDING;
    ai_set_anim(s, MZM_SOVA_FALLING);
    ai_random_x_direction(c);
    sova_set_x_flip_by_facing(s);
}

/* SovaFallingInit */
static void sova_falling_init(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) s->x -= HALF_BLOCK_SIZE;
        else s->x += HALF_BLOCK_SIZE;
    } else if (s->work2) {
        s->y += HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    }
    s->pose = ZOOMER_POSE_FALLING;
    s->work3 = 0;
    s->work2 = 0;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_X_FLIP | MZM_SPRITE_STATUS_Y_FLIP |
                             MZM_SPRITE_STATUS_FACING_DOWN);
    sova_hitbox(s);
    ai_set_anim(s, MZM_SOVA_FALLING);
}

/* SovaFalling (the splash of SpriteUtilCheckInRoomEffect is not ported) */
static void sova_falling(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t offset = s->work3;
    int16_t movement = offset < 16 ? c->tables->falling[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        s->y = (uint16_t)(s->y + c->tables->falling[offset > 0 ? offset - 1 : 0]);
    } else {
        s->work3++;
        s->y = (uint16_t)(s->y + movement);
    }
    uint16_t top = ai_check_vertical_slopes(c, s->y, s->x);
    if (c->vertical != MZM_COLLISION_AIR) {
        s->y = top;
        sova_landing_init(c);
    }
}

/* Sova: the AI (its damaged sound in mzm_sprite_update). Freezing is not
 * ported. */
static void sova(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED: sova_init(c); break;
    case ZOOMER_POSE_IDLE_INIT:
        sova_crawling_oam(s);
        s->pose = ZOOMER_POSE_IDLE;
        /* fallthrough */
    case ZOOMER_POSE_IDLE: sova_move(c); break;
    case ZOOMER_POSE_TURNING_AROUND_INIT:
        sova_turning_init(s);
        if (!s->status) break;
        /* fallthrough */
    case ZOOMER_POSE_TURNING_AROUND: sova_turning(c); break;
    case ZOOMER_POSE_LANDING_INIT:
        sova_landing_init(c);
        /* fallthrough */
    case ZOOMER_POSE_LANDING:
        if (sova_in_air(c)) s->pose = ZOOMER_POSE_FALLING_INIT;
        else if (ai_animation_ending(c, 2)) s->pose = ZOOMER_POSE_IDLE_INIT;
        break;
    case ZOOMER_POSE_FALLING_INIT:
        sova_falling_init(s);
        /* fallthrough */
    case ZOOMER_POSE_FALLING: sova_falling(c); break;
    default: {
        /* SovaDeath */
        uint16_t y = s->y, x = s->x;
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN)
            x = s->status & MZM_SPRITE_STATUS_X_FLIP ? (uint16_t)(x - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE))
                                                     : (uint16_t)(x + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        else
            y = s->work2 ? (uint16_t)(y + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)
                         : (uint16_t)(y - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
        ai_sprite_death(c, y, x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        break;
    }
    }
}

/* SpriteSpawnDropFollowers: a primary sprite owned by `ram_slot` in the
 * first free slot (NULL without one). */
MzmSprite *ai_spawn_drop_follower(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot,
                                      uint8_t ram_slot, uint16_t y, uint16_t x, uint16_t status) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS) continue;
        memset(n, 0, sizeof *n);
        n->status = status | MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                    MZM_SPRITE_STATUS_NOT_DRAWN;
        n->gfx_slot = gfx_slot;
        n->sprite_id = sprite_id;
        n->y = y;
        n->x = x;
        n->room_slot = part;
        n->pose = SPRITE_POSE_UNINITIALIZED;
        n->ignore_samus_timer = 1;
        n->ram_slot = ram_slot;
        return n;
    }
    return NULL;
}

/* SpriteUtilCheckHasDrops: a drop owned by this sprite's slot exists. */
bool ai_has_drops(const Ctx *c) {
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS && n->ram_slot == c->s->ram_slot &&
            n->samus_collision >= SSC_SMALL_ENERGY_DROP)
            return true;
    }
    return false;
}

/* SpriteUtilCountChildPrimarySprites */
static int count_children(const Ctx *c, uint8_t sprite_id) {
    int count = 0;
    for (int i = 0; c->f->sprites && i < c->f->sprite_count; ++i) {
        const MzmSprite *n = &c->f->sprites[i];
        if (n->status & MZM_SPRITE_STATUS_EXISTS && !n->secondary && n->sprite_id == sprite_id &&
            n->ram_slot == c->s->ram_slot)
            ++count;
    }
    return count;
}

/* SpriteUtilCheckSamusNearSpriteAboveBelow == NSAB_ABOVE: within the x
 * range, and the sprite below Samus's middle within the y range. */
bool ai_samus_above(const Ctx *c, uint16_t y_range, uint16_t x_range) {
    uint16_t samus_y = (uint16_t)(c->f->samus_y + c->f->samus_hitbox_top / 2);
    uint16_t samus_x = c->f->samus_x, y = c->s->y, x = c->s->x;
    if (x > samus_x ? x - samus_x >= x_range : samus_x - x >= x_range) return false;
    return y > samus_y && y - samus_y < y_range;
}

/* Zeb (zeb.c) and Geega (geega.c), the pipe bugs: hidden in their pipe
 * until Samus is above it, beside it and within five blocks, they rise an
 * eighth of a block a frame (harmful after two frames) up to about her
 * height, then fly toward her at 12 subpixels a frame through walls until
 * 16 blocks past her, and respawn in the pipe a second later; a pipe waits
 * while a drop of its own exists. A Geega leader rises with a follower
 * spawned two blocks and a half below it and waits for it to be gone; a
 * Zebbo leader (zebbo.c, otherwise a Geega) spawns two followers, one block
 * and a half and two blocks and a half below it. */
enum {
    PIPE_BUG_POSE_IDLE_INIT = 0x8,
    PIPE_BUG_POSE_IDLE = 0x9,
    PIPE_BUG_POSE_GOING_UP = 0x23,
    PIPE_BUG_POSE_MOVING = 0x25,
};

static bool is_follower(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_GEEGA_FOLLOWER ||
           s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER;
}

static bool is_zebbo(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN || s->sprite_id == MZM_PSPRITE_ZEBBO_YELLOW ||
           s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_LEADER ||
           s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER;
}

static bool is_geega(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_GEEGA || s->sprite_id == MZM_PSPRITE_GEEGA_WHITE ||
           s->sprite_id == MZM_PSPRITE_GEEGA_LEADER || s->sprite_id == MZM_PSPRITE_GEEGA_FOLLOWER;
}

/* ZebIdleInit / GeegaIdleInit */
static void pipe_bug_idle_init(MzmSprite *s) {
    s->samus_collision = SSC_NONE;
    s->pose = PIPE_BUG_POSE_IDLE;
    ai_set_anim(s, MZM_PIPE_BUG_IDLE);
    s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
}

/* ZebRespawn / GeegaRespawn (a Geega follower is removed instead) */
static void pipe_bug_respawn(Ctx *c) {
    MzmSprite *s = c->s;
    if (is_follower(s)) {
        s->status = 0;
        return;
    }
    s->y = s->y_spawn;
    s->x = s->x_spawn;
    pipe_bug_idle_init(s);
    s->work1 = 60;                  /* CONVERT_SECONDS(1.f) */
    s->health = c->tables->health[s->sprite_id];
    s->isft = 0;
    s->palette_row = 0;
    s->absolute_palette_row = 0;
    s->ignore_samus_timer = 1;
    s->freeze_timer = 0;
}

static void pipe_bug_idle(Ctx *c) {
    MzmSprite *s = c->s;
    if (is_follower(s)) {
        s->pose = PIPE_BUG_POSE_GOING_UP;
        s->status &= (uint16_t)~(MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
        return;
    }
    if (s->sprite_id == MZM_PSPRITE_GEEGA_LEADER && count_children(c, MZM_PSPRITE_GEEGA_FOLLOWER))
        return;
    if (s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_LEADER &&
        count_children(c, MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER))
        return;
    if (ai_has_drops(c)) return;
    if (s->work1) {
        s->work1--;
        return;
    }
    uint16_t samus_y = c->f->samus_y, samus_x = c->f->samus_x;
    if (samus_y > s->y - (HALF_BLOCK_SIZE - PIXEL_SIZE / 2)) return;
    uint16_t dx = s->x > samus_x ? (uint16_t)(s->x - samus_x) : (uint16_t)(samus_x - s->x);
    if (dx <= HALF_BLOCK_SIZE + PIXEL_SIZE) return;
    if (!ai_samus_above(c, BLOCK_SIZE * 5, BLOCK_SIZE * 5)) return;
    s->scaling = samus_y;
    s->pose = PIPE_BUG_POSE_GOING_UP;
    s->work0 = 2;
    s->status &= (uint16_t)~(MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES);
    /* SpriteUtilMakeSpriteFaceSamusXFlip */
    if (s->x > samus_x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
    else s->status |= MZM_SPRITE_STATUS_X_FLIP;
    ai_sound_onscreen(s, is_zebbo(s) ? MZM_SOUND_ZEBBO_RISING :
                         is_geega(s) ? MZM_SOUND_GEEGA_RISING : MZM_SOUND_ZEB_RISING);
    if (s->sprite_id == MZM_PSPRITE_GEEGA_LEADER) {
        MzmSprite *follower = ai_spawn_drop_follower(
            c, MZM_PSPRITE_GEEGA_FOLLOWER, s->room_slot, s->gfx_slot, s->ram_slot,
            (uint16_t)(s->y + BLOCK_SIZE * 2 + HALF_BLOCK_SIZE), (uint16_t)(s->x - HALF_BLOCK_SIZE),
            s->status & MZM_SPRITE_STATUS_X_FLIP);
        if (follower) {
            follower->scaling = s->scaling;
            follower->work0 = 18;   /* CONVERT_SECONDS(.3f) */
        }
    } else if (s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_LEADER) {
        /* The second follower only spawns after the first. */
        MzmSprite *follower = ai_spawn_drop_follower(
            c, MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER, s->room_slot, s->gfx_slot, s->ram_slot,
            (uint16_t)(s->y + BLOCK_SIZE + HALF_BLOCK_SIZE), (uint16_t)(s->x - HALF_BLOCK_SIZE),
            s->status & MZM_SPRITE_STATUS_X_FLIP);
        if (follower) {
            follower->scaling = s->scaling;
            follower->work0 = 10;   /* CONVERT_SECONDS(1.f / 6) */
            follower = ai_spawn_drop_follower(
                c, MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER, s->room_slot, s->gfx_slot, s->ram_slot,
                (uint16_t)(s->y + BLOCK_SIZE * 2 + HALF_BLOCK_SIZE),
                (uint16_t)(s->x - HALF_BLOCK_SIZE), s->status & MZM_SPRITE_STATUS_X_FLIP);
            if (follower) {
                follower->scaling = s->scaling;
                follower->work0 = 18;   /* CONVERT_SECONDS(0.3f) */
            }
        }
    }
}

static void pipe_bug_going_up(Ctx *c) {
    MzmSprite *s = c->s;
    s->y -= EIGHTH_BLOCK_SIZE;
    if (s->work0) {
        if (--s->work0 == 0) s->samus_collision = SSC_HURTS_SAMUS;
        return;
    }
    uint16_t samus_y = c->f->samus_y;
    uint16_t target = s->scaling < samus_y && samus_y > s->y_spawn - BLOCK_SIZE * 2 ? s->scaling
                                                                                     : samus_y;
    if (target - (BLOCK_SIZE + HALF_BLOCK_SIZE + PIXEL_SIZE) > s->y) {
        s->pose = PIPE_BUG_POSE_MOVING;
        s->work0 = 10;              /* CONVERT_SECONDS(1.f / 6) */
        ai_set_anim(s, MZM_PIPE_BUG_MOVING);
    }
}

static void pipe_bug_move(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t moving = is_zebbo(s) ? MZM_SOUND_ZEBBO_MOVING :
                      is_geega(s) ? MZM_SOUND_GEEGA_MOVING : MZM_SOUND_ZEB_MOVING;
    if (s->work0) {
        if (--s->work0 == 0) {
            ai_sound_onscreen(s, moving);
            s->work1 = 0;
        }
        return;
    }
    s->work1++;
    int samus_x = c->f->samus_x, x = s->x;
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        if (x - samus_x > BLOCK_SIZE * 16 || x & 0x8000) {
            pipe_bug_respawn(c);
            return;
        }
        s->x += EIGHTH_BLOCK_SIZE + PIXEL_SIZE;
    } else {
        if (samus_x - x > BLOCK_SIZE * 16 || x & 0x8000) {
            pipe_bug_respawn(c);
            return;
        }
        s->x -= EIGHTH_BLOCK_SIZE + PIXEL_SIZE;
    }
    /* CONVERT_SECONDS(.25f) + DELTA_TIME */
    if ((s->work1 & 15) == 0) ai_sound_onscreen(s, moving);
}

/* Zeb / Geega / Zebbo: the AIs. Freezing is not ported. */
static void pipe_bug(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* ZebInit / GeegaInit / ZebboInit */
        if (is_zebbo(s)) {
            s->hitbox_top = s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
            s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
            s->draw_top = s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        } else if (is_geega(s)) {
            s->hitbox_top = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
            s->hitbox_bottom = HALF_BLOCK_SIZE - PIXEL_SIZE;
            s->hitbox_left = -(HALF_BLOCK_SIZE - EIGHTH_BLOCK_SIZE);
            s->hitbox_right = HALF_BLOCK_SIZE - EIGHTH_BLOCK_SIZE;
            s->draw_top = s->draw_bottom = (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) / PIXEL_SIZE;
            s->draw_horizontal = THREE_QUARTER_BLOCK_SIZE / PIXEL_SIZE;
        } else {
            s->hitbox_top = -EIGHTH_BLOCK_SIZE;
            s->hitbox_bottom = QUARTER_BLOCK_SIZE + PIXEL_SIZE;
            s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE + PIXEL_SIZE);
            s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE + PIXEL_SIZE;
            s->draw_top = s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        }
        s->work1 = 1;
        s->health = c->tables->health[s->sprite_id];
        s->y -= HALF_BLOCK_SIZE;
        s->x += HALF_BLOCK_SIZE;
        s->y_spawn = s->y;
        s->x_spawn = s->x;
        /* fallthrough */
    case PIPE_BUG_POSE_IDLE_INIT:
        pipe_bug_idle_init(s);
        /* fallthrough */
    case PIPE_BUG_POSE_IDLE: pipe_bug_idle(c); break;
    case PIPE_BUG_POSE_GOING_UP: pipe_bug_going_up(c); break;
    case PIPE_BUG_POSE_MOVING: pipe_bug_move(c); break;
    default:
        ai_sprite_death_typed(c, DEATH_RESPAWNING, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        pipe_bug_respawn(c);
        break;
    }
}

/* Reo (reo.c) */
enum {
    REO_POSE_IDLE_INIT = 0x8,
    REO_POSE_IDLE = 0x9,
    REO_POSE_MOVING_INIT = 0x22,
    REO_POSE_MOVING = 0x23,
};

/* ReoIdle: hovering with sReoIdleYMovement / sReoIdleXMovement from an RNG
 * offset until Samus's middle is within 8 blocks vertically and 7
 * horizontally. */
static void reo_idle(Ctx *c) {
    MzmSprite *s = c->s;
    uint8_t offset = s->work3;
    int16_t movement = offset < 65 ? c->tables->reo_idle_y[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = c->tables->reo_idle_y[0];
        offset = 0;
    }
    s->work3 = (uint8_t)(offset + 1);
    s->y = (uint16_t)(s->y + movement);
    offset = s->work2;
    movement = offset < 81 ? c->tables->reo_idle_x[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = c->tables->reo_idle_x[0];
        offset = 0;
    }
    s->work2 = (uint8_t)(offset + 1);
    s->x = (uint16_t)(s->x + movement);
    if (ai_samus_near_left_right(c, BLOCK_SIZE * 8, BLOCK_SIZE * 7)) s->pose = REO_POSE_MOVING_INIT;
}

/* ReoMovingInit: facing Samus, diving if above her head. */
static void reo_moving_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->work1 = 0;
    s->work2 = 1;
    s->work0 = 0;
    s->work3 = 1;
    s->x_spawn = c->rng & 3;        /* MOD_AND(gSpriteRng, SPRITE_RNG_PROB(.25f)) */
    s->pose = REO_POSE_MOVING;
    s->scaling = 32;                /* CONVERT_SECONDS(.5f + 1.f / 30), the sound timer */
    ai_set_anim(s, MZM_REO_MOVING);
    if (s->x > c->f->samus_x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
    if (s->y > c->f->samus_y + c->f->samus_hitbox_top)
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    else
        s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
}

/* One axis of ReoMove: accelerate (a speed unit is a quarter subpixel) up
 * to the cap toward the target, then decelerate past it and turn. */
static void reo_axis(uint16_t *position, uint8_t *braking, uint8_t *speed, uint16_t *status,
                     uint16_t flag, uint16_t target, uint8_t cap) {
    bool positive = *status & flag;
    if (*braking == 0) {
        bool passed = positive ? *position > target - PIXEL_SIZE : *position < target + PIXEL_SIZE;
        if (passed) {
            *braking = *speed;
        } else {
            if (*speed < cap) ++*speed;
            *position = (uint16_t)(*position + (positive ? *speed / 4 : -(*speed / 4)));
        }
    } else if (--*braking != 0) {
        *position = (uint16_t)(*position + (positive ? *braking / 4 : -(*braking / 4)));
    } else {
        *status ^= flag;
        *speed = 1;
    }
}

/* ReoMove: pushing the next overlapping Reo of its kind apart, turning at
 * walls, and homing on a point 72 subpixels above Samus's feet (the splash
 * effects and the moving sound are not ported). */
static void reo_move(Ctx *c) {
    MzmSprite *s = c->s;
    const int offset = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    for (int i = s->ram_slot + 1; c->f->sprites && i < c->f->sprite_count; ++i) {
        MzmSprite *o = &c->f->sprites[i];
        if (!(o->status & MZM_SPRITE_STATUS_EXISTS) || o->secondary || o->sprite_id != s->sprite_id)
            continue;
        if (!(s->y + offset > o->y - offset && s->y - offset < o->y + offset &&
              s->x + offset > o->x - offset && s->x - offset < o->x + offset))
            continue;
        if (!o->freeze_timer) {
            if (s->y > o->y) {
                if (ai_collision_at(c, (uint16_t)(o->y - HALF_BLOCK_SIZE), o->x) == MZM_COLLISION_AIR)
                    o->y -= PIXEL_SIZE;
            } else if (ai_collision_at(c, (uint16_t)(o->y + HALF_BLOCK_SIZE), o->x) ==
                       MZM_COLLISION_AIR) {
                o->y += PIXEL_SIZE;
            }
            if (s->x > o->x) {
                if (ai_collision_at(c, o->y, (uint16_t)(o->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_AIR)
                    o->x -= PIXEL_SIZE;
            } else if (ai_collision_at(c, o->y, (uint16_t)(o->x + HALF_BLOCK_SIZE)) ==
                       MZM_COLLISION_AIR) {
                o->x += PIXEL_SIZE;
            }
        }
        break;
    }
    uint16_t ahead = s->status & MZM_SPRITE_STATUS_FACING_RIGHT ? (uint16_t)(s->x + HALF_BLOCK_SIZE)
                                                                : (uint16_t)(s->x - HALF_BLOCK_SIZE);
    if (ai_collision_at(c, s->y, ahead) != MZM_COLLISION_AIR) {
        s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
        s->work1 = 0;
        s->work2 = 1;
    }
    uint16_t below = s->status & MZM_SPRITE_STATUS_FACING_DOWN ? (uint16_t)(s->y + HALF_BLOCK_SIZE)
                                                               : (uint16_t)(s->y - HALF_BLOCK_SIZE);
    if (ai_collision_at(c, below, s->x) != MZM_COLLISION_AIR) {
        s->status ^= MZM_SPRITE_STATUS_FACING_DOWN;
        s->work0 = 0;
        s->work3 = 1;
    }
    bool purple = s->sprite_id == MZM_PSPRITE_REO_PURPLE_WINGS;
    uint16_t target_y = (uint16_t)(c->f->samus_y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
    reo_axis(&s->x, &s->work1, &s->work2, &s->status, MZM_SPRITE_STATUS_FACING_RIGHT,
             c->f->samus_x, purple ? BLOCK_SIZE : QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
    reo_axis(&s->y, &s->work0, &s->work3, &s->status, MZM_SPRITE_STATUS_FACING_DOWN, target_y,
             purple ? HALF_BLOCK_SIZE : QUARTER_BLOCK_SIZE);
    if (--s->scaling == 0) {
        s->scaling = 32;
        ai_sound_onscreen_once(s, MZM_SOUND_REO_MOVING);
    }
}

/* Reo: the AI. Freezing is not ported. */
static void reo(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* ReoInit */
        s->draw_top = s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = HALF_BLOCK_SIZE;
        s->hitbox_left = -(BLOCK_SIZE - EIGHTH_BLOCK_SIZE);
        s->hitbox_right = BLOCK_SIZE - EIGHTH_BLOCK_SIZE;
        ai_set_anim(s, MZM_REO_IDLE);
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS;
        ai_random_x_direction(c);
        if (c->rng > 8) s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
        s->pose = REO_POSE_IDLE_INIT;
        break;
    case REO_POSE_IDLE_INIT:
        s->pose = REO_POSE_IDLE;
        s->work2 = (uint8_t)(c->rng * 4);
        s->work3 = s->work2;
        break;
    case REO_POSE_IDLE: reo_idle(c); break;
    case REO_POSE_MOVING_INIT:
        reo_moving_init(c);
        /* fallthrough */
    case REO_POSE_MOVING: reo_move(c); break;
    default: ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_BIG); break;
    }
}

/* Mella (mella.c) and Holtz (holtz.c): ceiling swoopers. SAMUS_COLLIDING
 * is their "going down" flag, as in the game. */
enum {
    SWOOPER_POSE_IDLE_INIT = 0x8,
    SWOOPER_POSE_IDLE = 0x9,
    SWOOPER_POSE_WARNING_INIT = 0x32,   /* MELLA_POSE_MOVING_INIT */
    SWOOPER_POSE_WARNING = 0x33,        /* MELLA_POSE_DELAY_BEOFRE_GOING_DOWN */
    SWOOPER_POSE_GOING_DOWN = 0x35,
    SWOOPER_POSE_SLIDING = 0x37,        /* MELLA_POSE_GOING_UP */
    HOLTZ_POSE_GOING_UP = 0x39,
    HOLTZ_POSE_BACK_TO_CEILING = 0x3B,
};

/* MellaYMovement / HoltzYMovement: down until a floor a block below its
 * centre or either side; up until both blocks on one side of the top are
 * solid. */
static bool swooper_y_movement(Ctx *c, uint16_t movement, uint16_t height) {
    MzmSprite *s = c->s;
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        uint16_t y = (uint16_t)(s->y + BLOCK_SIZE);
        if (ai_collision_at(c, y, s->x) == MZM_COLLISION_SOLID ||
            ai_collision_at(c, y, (uint16_t)(s->x - THREE_QUARTER_BLOCK_SIZE)) == MZM_COLLISION_SOLID ||
            ai_collision_at(c, y, (uint16_t)(s->x + THREE_QUARTER_BLOCK_SIZE)) == MZM_COLLISION_SOLID)
            return true;
        s->y += movement;
        return false;
    }
    uint16_t y = (uint16_t)(s->y - height);
    if (ai_collision_at(c, y, (uint16_t)(s->x - THREE_QUARTER_BLOCK_SIZE)) == MZM_COLLISION_SOLID &&
        ai_collision_at(c, y, (uint16_t)(s->x - (BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE))) ==
            MZM_COLLISION_SOLID)
        return true;
    if (ai_collision_at(c, y, (uint16_t)(s->x + THREE_QUARTER_BLOCK_SIZE)) == MZM_COLLISION_SOLID &&
        ai_collision_at(c, y, (uint16_t)(s->x + BLOCK_SIZE + THREE_QUARTER_BLOCK_SIZE)) ==
            MZM_COLLISION_SOLID)
        return true;
    s->y -= movement;
    return false;
}

/* MellaXMovement / HoltzXMovement: blocked by solid a block ahead, half a
 * block above or below its centre. */
static void swooper_x_movement(Ctx *c, uint16_t movement) {
    MzmSprite *s = c->s;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    uint16_t x = right ? (uint16_t)(s->x + BLOCK_SIZE) : (uint16_t)(s->x - BLOCK_SIZE);
    if (ai_collision_at(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), x) == MZM_COLLISION_SOLID ||
        ai_collision_at(c, (uint16_t)(s->y + HALF_BLOCK_SIZE), x) == MZM_COLLISION_SOLID)
        return;
    if (right) s->x += movement;
    else s->x -= movement;
}

/* A cycling idle table (SHORT_MAX restarts it). */
static void cycle_table(uint8_t *offset_field, uint16_t *position, const int16_t *table, int size) {
    uint8_t offset = *offset_field;
    int16_t movement = offset < size ? table[offset] : SHORT_MAX;
    if (movement == SHORT_MAX) {
        movement = table[0];
        offset = 0;
    }
    *offset_field = (uint8_t)(offset + 1);
    *position = (uint16_t)(*position + movement);
}

/* Samus low enough (her feet 72 subpixels or more below its centre) and
 * within the ranges. */
static bool swooper_detects(const Ctx *c, uint16_t x_range) {
    return c->f->samus_y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE) >= c->s->y &&
           ai_samus_near_left_right(c, BLOCK_SIZE * 8, x_range);
}

void ai_face_samus(const Ctx *c) {
    if (c->s->x > c->f->samus_x) c->s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else c->s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
}

/* Mella: hovers under its ceiling with its idle tables, then swoops down
 * toward Samus (within 8 blocks vertically, 3 across) after a 7-frame
 * delay, accelerating from the ROM tables, and climbs back. */
static void mella(Ctx *c) {
    MzmSprite *s = c->s;
    const int16_t *t;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        s->draw_top = s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = -QUARTER_BLOCK_SIZE;
        s->hitbox_bottom = QUARTER_BLOCK_SIZE;
        s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_right = HALF_BLOCK_SIZE;
        ai_set_anim(s, MZM_MELLA_IDLE);
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS;
        s->pose = SWOOPER_POSE_IDLE_INIT;
        break;
    case SWOOPER_POSE_IDLE_INIT:
        s->pose = SWOOPER_POSE_IDLE;
        ai_set_anim(s, MZM_MELLA_IDLE);
        s->work3 = (uint8_t)(c->rng * 4);
        s->work2 = (uint8_t)(c->rng * 2);
        break;
    case SWOOPER_POSE_IDLE:
        cycle_table(&s->work3, &s->y, c->tables->mella_idle_y, 65);
        cycle_table(&s->work2, &s->x, c->tables->mella_idle_x, 49);
        if (swooper_detects(c, BLOCK_SIZE * 3)) s->pose = SWOOPER_POSE_WARNING_INIT;
        break;
    case SWOOPER_POSE_WARNING_INIT:
        s->pose = SWOOPER_POSE_WARNING;
        ai_set_anim(s, MZM_MELLA_MOVING);
        s->work0 = 7;               /* CONVERT_SECONDS(.1f) + 1 */
        /* fallthrough */
    case SWOOPER_POSE_WARNING:      /* MellaDelayBeforeGoingDown */
        if (s->frame == 0 && s->adc == 1) ai_sound_onscreen_once(s, MZM_SOUND_MELLA_MOVING);
        if (--s->work0 == 0) {
            s->pose = SWOOPER_POSE_GOING_DOWN;
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            ai_face_samus(c);
            s->work2 = 0;
            s->work3 = 0;
        }
        break;
    case SWOOPER_POSE_GOING_DOWN:
        if (s->frame == 0 && s->adc == 1) ai_sound_onscreen_once(s, MZM_SOUND_MELLA_MOVING);
        if (s->work3 < 6 * 8 - 1) s->work3++;
        if (s->work2 < 7 * 8 - 1) s->work2++;
        t = c->tables->mella_x;
        swooper_x_movement(c, (uint16_t)t[s->work2 / 8]);
        if (swooper_y_movement(c, (uint16_t)c->tables->mella_down[s->work3 / 8], HALF_BLOCK_SIZE)) {
            s->pose = SWOOPER_POSE_SLIDING;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            s->work3 = 0;
        }
        break;
    case SWOOPER_POSE_SLIDING:      /* MellaGoingUp */
        if (s->frame == 0 && s->adc == 1) ai_sound_onscreen_once(s, MZM_SOUND_MELLA_MOVING);
        if (s->work3 < 7 * 8 - 1) s->work3++;
        if (s->work2 > 0x18) s->work2--;
        swooper_x_movement(c, (uint16_t)c->tables->mella_x[s->work2 / 8]);
        if (swooper_y_movement(c, (uint16_t)c->tables->mella_up[s->work3 / 8], HALF_BLOCK_SIZE)) {
            s->y = (uint16_t)((s->y & BLOCK_POSITION_FLAG) + HALF_BLOCK_SIZE);
            s->pose = SWOOPER_POSE_IDLE_INIT;
        }
        break;
    default: ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL); break;
    }
}

/* Holtz: hovers under its ceiling, warns when Samus is low and within 8
 * blocks vertically and 5 across, swoops down at 3 pixels a frame with a
 * growing horizontal speed, slides along the floor, climbs back slowing
 * down and bonks on its ceiling. */
static void holtz(Ctx *c) {
    MzmSprite *s = c->s;
    const uint16_t height = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        ai_face_samus(c);
        s->draw_top = THREE_QUARTER_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = HALF_BLOCK_SIZE;
        s->hitbox_left = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        ai_set_anim(s, MZM_HOLTZ_IDLE);
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS;
        s->pose = SWOOPER_POSE_IDLE_INIT;
        s->work3 = 0;
        break;
    case SWOOPER_POSE_IDLE_INIT:
        s->pose = SWOOPER_POSE_IDLE;
        ai_set_anim(s, c->f->samus_y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE) < s->y ? MZM_HOLTZ_IDLE_AWARE
                                                                           : MZM_HOLTZ_IDLE);
        break;
    case SWOOPER_POSE_IDLE:
        cycle_table(&s->work3, &s->y, c->tables->holtz_idle_y, 65);
        if (swooper_detects(c, BLOCK_SIZE * 5)) s->pose = SWOOPER_POSE_WARNING_INIT;
        break;
    case SWOOPER_POSE_WARNING_INIT:
        s->pose = SWOOPER_POSE_WARNING;
        ai_set_anim(s, MZM_HOLTZ_WARNING);
        /* fallthrough */
    case SWOOPER_POSE_WARNING:
        cycle_table(&s->work3, &s->y, c->tables->holtz_idle_y, 65);
        if (ai_animation_ending(c, 1)) {
            s->pose = SWOOPER_POSE_GOING_DOWN;
            ai_set_anim(s, MZM_HOLTZ_GOING_DOWN);
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            ai_face_samus(c);
            s->work0 = 0;
            s->work2 = PIXEL_SIZE / 2;
            ai_sound_onscreen_once(s, MZM_SOUND_HOLTZ_GOING_DOWN);
        }
        break;
    case SWOOPER_POSE_GOING_DOWN:
        s->work0++;
        if (s->work2 < PIXEL_SIZE * 3 && (s->work0 & 1) == 0) s->work2++;
        swooper_x_movement(c, s->work2);
        if (swooper_y_movement(c, 3 * PIXEL_SIZE, height)) {
            s->pose = SWOOPER_POSE_SLIDING;
            ai_set_anim(s, MZM_HOLTZ_IDLE_AWARE);
        }
        break;
    case SWOOPER_POSE_SLIDING:
        swooper_x_movement(c, s->work2);
        if (ai_animation_ending(c, 1)) {
            s->pose = HOLTZ_POSE_GOING_UP;
            ai_set_anim(s, MZM_HOLTZ_GOING_UP);
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            s->work0 = 0;
        }
        break;
    case HOLTZ_POSE_GOING_UP:
        if ((s->frame == 0 || s->frame == 3) && s->adc == 1)
            ai_sound_onscreen_once(s, MZM_SOUND_HOLTZ_GOING_UP);
        s->work0++;
        if (s->work2 > PIXEL_SIZE / 2 && (s->work0 & 1) == 0) s->work2--;
        swooper_x_movement(c, s->work2);
        if (swooper_y_movement(c, PIXEL_SIZE * 3, height)) {
            s->y &= BLOCK_POSITION_FLAG;
            s->y += height;
            s->pose = HOLTZ_POSE_BACK_TO_CEILING;
            ai_set_anim(s, MZM_HOLTZ_BONKING_ON_CEILING);
            s->work3 = 0;
        }
        break;
    case HOLTZ_POSE_BACK_TO_CEILING:
        cycle_table(&s->work3, &s->y, c->tables->holtz_idle_y, 65);
        if (ai_animation_ending(c, 2)) s->pose = SWOOPER_POSE_IDLE_INIT;
        break;
    default: ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM); break;
    }
}

/* Multiviola: bounces off every surface at 3 subpixels a frame on both
 * axes, its animation starting at twice its slot (MultiviolaMove). */
static void multiviola(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_bottom = THREE_QUARTER_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = HALF_BLOCK_SIZE;
        ai_set_anim(s, MZM_MULTIVIOLA_MOVING);
        s->frame = (uint8_t)(s->ram_slot * 2);
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS;
        if (s->x > c->f->samus_x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        else s->status |= MZM_SPRITE_STATUS_X_FLIP;
        s->pose = 9;
        return;
    }
    if (s->pose != 9) {
        ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_BIG);
        return;
    }
    const uint16_t step = PIXEL_SIZE - 1;
    bool bouncing = false;
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        if (ai_collision_at(c, s->y, (uint16_t)(s->x + s->hitbox_right)) == MZM_COLLISION_AIR) {
            s->x += step;
        } else {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
            bouncing = true;
        }
    } else {
        if (ai_collision_at(c, s->y, (uint16_t)(s->x + s->hitbox_left)) == MZM_COLLISION_AIR) {
            s->x -= step;
        } else {
            s->status |= MZM_SPRITE_STATUS_X_FLIP;
            bouncing = true;
        }
    }
    if (s->status & MZM_SPRITE_STATUS_FACING_DOWN) {
        if (ai_collision_at(c, (uint16_t)(s->y + s->hitbox_top), s->x) == MZM_COLLISION_AIR) {
            s->y -= step;
        } else {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
            bouncing = true;
        }
    } else {
        if (ai_collision_at(c, (uint16_t)(s->y + s->hitbox_bottom), s->x) == MZM_COLLISION_AIR) {
            s->y += step;
        } else {
            s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
            bouncing = true;
        }
    }
    if (bouncing) ai_sound_onscreen_once(s, MZM_SOUND_MULTIVIOLA_BOUNCING);
}

/* Sqrt (BIOS SWI 0x08): the integer square root, rounded down. */
static uint16_t bios_sqrt(uint32_t number) {
    uint32_t root = 0, bit = 1u << 30;
    while (bit > number) bit >>= 2;
    while (bit) {
        if (number >= root + bit) {
            number -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (uint16_t)root;
}

/* Rinka (rinka.c, orange and green; the Mother Brain ones are not ported):
 * a ring born at its spawn block that, once on screen and vulnerable,
 * flies in a straight line toward where Samus was (its distance from the
 * spawn point grows by 4 subpixels a frame, green 5) and respawns 16 blocks
 * away from her. */
enum { RINKA_POSE_SPAWNING_INIT = 0x8, RINKA_POSE_SPAWNING = 0x9, RINKA_POSE_MOVING = 0x23 };

static bool is_green_rinka(const MzmSprite *s) {
    return s->status & MZM_SPRITE_STATUS_MOSAIC;
}

/* RinkaSpawningInit (the green one is flagged MOSAIC, as in the game) */
static void rinka_spawning_init(MzmSprite *s) {
    s->pose = RINKA_POSE_SPAWNING;
    s->frame = 0;
    s->adc = 0;
    if (s->sprite_id == MZM_PSPRITE_RINKA_GREEN) {
        s->status |= MZM_SPRITE_STATUS_MOSAIC;
        s->anim = MZM_RINKA_GREEN_SPAWNING;
    } else {
        s->anim = MZM_RINKA_ORANGE_SPAWNING;
    }
}

/* RinkaRespawn: back to its spawn block, hidden, after 20 (green) or 60
 * frames. */
static void rinka_respawn(Ctx *c) {
    MzmSprite *s = c->s;
    s->y = (uint16_t)(s->work3 * BLOCK_SIZE + HALF_BLOCK_SIZE);
    s->x = (uint16_t)(s->work2 * BLOCK_SIZE + HALF_BLOCK_SIZE);
    rinka_spawning_init(s);
    s->health = c->tables->health[s->sprite_id];
    s->isft = 0;
    s->palette_row = 0;
    s->absolute_palette_row = 0;
    s->freeze_timer = 0;
    s->work1 = is_green_rinka(s) ? 20 : 60;
    s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->ignore_samus_timer = 1;
}

/* RinkaSpawning */
static void rinka_spawning(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->work1) {
        s->ignore_samus_timer = 1;
        s->work1--;
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_NOT_DRAWN) {
        s->ignore_samus_timer = 1;
        if (s->status & MZM_SPRITE_STATUS_ONSCREEN) {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
            s->frame = 0;
            s->adc = 0;
            s->work0 = 11;          /* CONVERT_SECONDS(1.f / 6 + 1.f / 60) */
        }
        return;
    }
    if (s->work0) {
        s->ignore_samus_timer = 1;
        if (--s->work0 == 0) s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        return;
    }
    if (!ai_animation_ending(c, 1)) return;
    s->anim = s->sprite_id == MZM_PSPRITE_RINKA_GREEN ? MZM_RINKA_GREEN_MOVING
                                                       : MZM_RINKA_ORANGE_MOVING;
    s->pose = RINKA_POSE_MOVING;
    s->frame = 0;
    s->adc = 0;
    s->scaling = 0;
    uint16_t samus_y = (uint16_t)(c->f->samus_y - (BLOCK_SIZE - PIXEL_SIZE)), samus_x = c->f->samus_x;
    s->y_spawn = samus_y;           /* the destination */
    s->x_spawn = samus_x;
    if (samus_y < s->y) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_DOWN;
    else s->status |= MZM_SPRITE_STATUS_FACING_DOWN;
    if (samus_x < s->x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_FACING_RIGHT;
    else s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
}

/* RinkaMove: from its spawn point along the unit vector toward the
 * destination (10-bit fixed point), placed scaling times 4 (green: 5)
 * subpixels from the spawn point; respawn beyond 16 blocks from Samus or
 * past the room origin. */
static void rinka_move(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t velocity = is_green_rinka(s) ? PIXEL_SIZE + 1 : PIXEL_SIZE;
    uint16_t acceleration = ++s->scaling;
    velocity = (uint16_t)(velocity * acceleration);
    uint32_t spawn_y = (uint32_t)s->work3 * BLOCK_SIZE + HALF_BLOCK_SIZE;
    uint32_t spawn_x = (uint32_t)s->work2 * BLOCK_SIZE + HALF_BLOCK_SIZE;
    bool down = s->status & MZM_SPRITE_STATUS_FACING_DOWN;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    uint16_t dy = down ? (uint16_t)(s->y_spawn - spawn_y) : (uint16_t)(spawn_y - s->y_spawn);
    uint16_t dx = right ? (uint16_t)(s->x_spawn - spawn_x) : (uint16_t)(spawn_x - s->x_spawn);
    int32_t total = bios_sqrt((uint32_t)dx * dx + (uint32_t)dy * dy);
    if (total != 0) {
        int32_t oy = (velocity * ((int32_t)(dy << 10) / total)) >> 10;
        int32_t ox = (velocity * ((int32_t)(dx << 10) / total)) >> 10;
        s->y = (uint16_t)(down ? spawn_y + oy : spawn_y - oy);
        s->x = (uint16_t)(right ? spawn_x + ox : spawn_x - ox);
    }
    bool respawn = false;
    uint16_t samus_x = c->f->samus_x, samus_y = c->f->samus_y;
    if (s->x & 0x8000) respawn = true;
    else if ((s->x > samus_x ? s->x - samus_x : samus_x - s->x) > BLOCK_SIZE * 16) respawn = true;
    if (s->y & 0x8000) respawn = true;
    else if ((s->y > samus_y ? s->y - samus_y : samus_y - s->y) > BLOCK_SIZE * 16) respawn = true;
    if (respawn) rinka_respawn(c);
}

/* Rinka: the AI. Freezing is not ported. */
static void rinka(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* RinkaInit: hitbox 20 subpixels (SUB_PIXEL_TO_PIXEL of 80) */
        s->hitbox_top = s->hitbox_left = -(BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_bottom = s->hitbox_right = (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = EIGHTH_BLOCK_SIZE;
        s->health = c->tables->health[s->sprite_id];
        s->y -= HALF_BLOCK_SIZE;
        s->work3 = (uint8_t)((uint16_t)(s->y - HALF_BLOCK_SIZE) / BLOCK_SIZE);
        s->work2 = (uint8_t)((uint16_t)(s->x - HALF_BLOCK_SIZE) / BLOCK_SIZE);
        s->samus_collision = SSC_HURTS_SAMUS;
        s->work1 = 0;
        /* fallthrough */
    case RINKA_POSE_SPAWNING_INIT:
        rinka_spawning_init(s);
        /* fallthrough */
    case RINKA_POSE_SPAWNING: rinka_spawning(c); break;
    case RINKA_POSE_MOVING: rinka_move(c); break;
    default:
        ai_sprite_death_typed(c, ai_has_drops(c) ? DEATH_NO_DEATH_OR_RESPAWNING_ALREADY_HAS_DROP
                                           : DEATH_RESPAWNING,
                           s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM);
        rinka_respawn(c);
        break;
    }
}

/* EnemyDropInit */
static void enemy_drop_init(MzmSprite *s) {
    s->ignore_samus_timer = 20;     /* ONE_THIRD_SECOND */
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->pose = ENEMY_DROP_POSE_IDLE;
    s->y_spawn = 200;               /* CONVERT_SECONDS(3.f) + ONE_THIRD_SECOND */
    s->x_spawn = 0;
    s->adc = 0;
    s->frame = 0;
    s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
    switch (s->sprite_id) {
    case MZM_PSPRITE_POWER_BOMB_DROP:
        s->anim = MZM_DROP_POWER_BOMB;
        s->samus_collision = SSC_POWER_BOMB_DROP;
        break;
    case MZM_PSPRITE_SUPER_MISSILE_DROP:
        s->anim = MZM_DROP_SUPER_MISSILE;
        s->samus_collision = SSC_SUPER_MISSILE_DROP;
        break;
    case MZM_PSPRITE_MISSILE_DROP:
        s->anim = MZM_DROP_MISSILE;
        s->samus_collision = SSC_MISSILE_DROP;
        break;
    case MZM_PSPRITE_LARGE_ENERGY_DROP:
        s->anim = MZM_DROP_LARGE_ENERGY;
        s->samus_collision = SSC_LARGE_ENERGY_DROP;
        break;
    default:                        /* PSPRITE_MULTIPLE_LARGE_ENERGY is not ported */
        s->anim = MZM_DROP_SMALL_ENERGY;
        s->samus_collision = SSC_SMALL_ENERGY_DROP;
        break;
    }
    s->hitbox_top = -(HALF_BLOCK_SIZE + PIXEL_SIZE);
    s->hitbox_bottom = HALF_BLOCK_SIZE + PIXEL_SIZE;
    s->hitbox_left = -HALF_BLOCK_SIZE;
    s->hitbox_right = HALF_BLOCK_SIZE;
}

static void refill(int *current, int maximum, int amount) {
    *current += amount;
    if (*current > maximum) *current = maximum;
}

/* EnemyDropIdle: shown once the ignore timer is nearly over, collected on
 * contact with its sound, blinking during its last 80 frames and gone
 * after 200 frames counted every other frame. */
static void enemy_drop_idle(Ctx *c) {
    MzmSprite *s = c->s;
    MzmEquipment *e = c->f->equipment;
    if (s->ignore_samus_timer) {
        if (s->ignore_samus_timer < 2) s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        return;
    }
    if (s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
        switch (s->samus_collision) {
        case SSC_SMALL_ENERGY_DROP:
            refill(&e->energy, e->max_energy, 5);
            mzm_sfx_play(MZM_SOUND_GETTING_SMALL_ENERGY_DROP);
            break;
        case SSC_LARGE_ENERGY_DROP:
            refill(&e->energy, e->max_energy, 20);
            mzm_sfx_play(MZM_SOUND_GETTING_LARGE_ENERGY_DROP);
            break;
        case SSC_MISSILE_DROP:
            refill(&e->missiles, e->max_missiles, 2);
            mzm_sfx_play(MZM_SOUND_GETTING_MISSILE_DROP);
            break;
        case SSC_SUPER_MISSILE_DROP:
            refill(&e->super_missiles, e->max_super_missiles, 2);
            mzm_sfx_play(MZM_SOUND_GETTING_SUPER_MISSILE_DROP);
            break;
        default: break;             /* power bombs: not ported */
        }
        s->status = 0;
        return;
    }
    if (s->x_spawn & 1) {
        uint8_t timer = (uint8_t)--s->y_spawn;
        if (timer == 0) s->status = 0;
        else if (timer < 80) s->status ^= MZM_SPRITE_STATUS_NOT_DRAWN;
    }
}

/* EnemyDrop */
static void enemy_drop(Ctx *c) {
    if (c->s->pose == SPRITE_POSE_UNINITIALIZED) enemy_drop_init(c->s);
    else if (c->s->pose == ENEMY_DROP_POSE_IDLE) enemy_drop_idle(c);
    c->s->x_spawn++;
}

static bool is_drop(uint8_t sprite_id) {
    return sprite_id >= MZM_PSPRITE_LARGE_ENERGY_DROP && sprite_id <= MZM_PSPRITE_POWER_BOMB_DROP;
}

int mzm_sprite_family(uint8_t sprite_id, bool secondary) {
    if (secondary) {
        if (sprite_id == MZM_SSPRITE_ZEELA_EYES) return MZM_FAMILY_ZEELA;
        if (sprite_id == MZM_SSPRITE_SPACE_PIRATE_LASER) return MZM_FAMILY_SPACE_PIRATE;
        if (sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2 ||
            sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_PROJECTILE)
            return MZM_FAMILY_CHOZODIA_SECURITY;
        return sprite_id == MZM_SSPRITE_SKREE_EXPLOSION ||
               sprite_id == MZM_SSPRITE_BLUE_SKREE_EXPLOSION ? MZM_FAMILY_SKREE : -1;
    }
    switch (sprite_id) {
    case MZM_PSPRITE_ZOOMER_YELLOW:
    case MZM_PSPRITE_ZOOMER_RED: return MZM_FAMILY_ZOOMER;
    case MZM_PSPRITE_ZEELA:
    case MZM_PSPRITE_ZEELA_RED: return MZM_FAMILY_ZEELA;
    case MZM_PSPRITE_RIPPER_BROWN:
    case MZM_PSPRITE_RIPPER_PURPLE: return MZM_FAMILY_RIPPER;
    case MZM_PSPRITE_SKREE_GREEN:
    case MZM_PSPRITE_SKREE_BLUE: return MZM_FAMILY_SKREE;
    case MZM_PSPRITE_WAVER: return MZM_FAMILY_WAVER;
    case MZM_PSPRITE_SIDEHOPPER: return MZM_FAMILY_SIDEHOPPER;
    case MZM_PSPRITE_VIOLA_BLUE:
    case MZM_PSPRITE_VIOLA_ORANGE: return MZM_FAMILY_VIOLA;
    case MZM_PSPRITE_SOVA_PURPLE:
    case MZM_PSPRITE_SOVA_ORANGE: return MZM_FAMILY_SOVA;
    case MZM_PSPRITE_ZEB:
    case MZM_PSPRITE_ZEB_BLUE: return MZM_FAMILY_ZEB;
    case MZM_PSPRITE_GEEGA:
    case MZM_PSPRITE_GEEGA_WHITE:
    case MZM_PSPRITE_GEEGA_LEADER:
    case MZM_PSPRITE_GEEGA_FOLLOWER: return MZM_FAMILY_GEEGA;
    case MZM_PSPRITE_ZEBBO_GREEN:
    case MZM_PSPRITE_ZEBBO_YELLOW:
    case MZM_PSPRITE_ZEBBO_GREEN_LEADER:
    case MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER: return MZM_FAMILY_ZEBBO;
    case MZM_PSPRITE_REO_GREEN_WINGS:
    case MZM_PSPRITE_REO_PURPLE_WINGS: return MZM_FAMILY_REO;
    case MZM_PSPRITE_MELLA: return MZM_FAMILY_MELLA;
    case MZM_PSPRITE_HOLTZ: return MZM_FAMILY_HOLTZ;
    case MZM_PSPRITE_MULTIVIOLA: return MZM_FAMILY_MULTIVIOLA;
    case MZM_PSPRITE_RINKA_ORANGE:
    case MZM_PSPRITE_RINKA_GREEN: return MZM_FAMILY_RINKA;
    case MZM_PSPRITE_DESSGEEGA:
    case MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM: return MZM_FAMILY_DESSGEEGA;
    case MZM_PSPRITE_SPACE_PIRATE:
    case MZM_PSPRITE_SPACE_PIRATE_WAITING1:
    case MZM_PSPRITE_SPACE_PIRATE_WAITING2:
    case MZM_PSPRITE_SPACE_PIRATE_WAITING3:
    case MZM_PSPRITE_SPACE_PIRATE2: return MZM_FAMILY_SPACE_PIRATE;
    case MZM_PSPRITE_SECURITY_LASER_VERTICAL:
    case MZM_PSPRITE_SECURITY_LASER_HORIZONTAL:
    case MZM_PSPRITE_SECURITY_LASER_VERTICAL2:
    case MZM_PSPRITE_SECURITY_LASER_HORIZONTAL2:
    case MZM_PSPRITE_SEARCHLIGHT_EYE:
    case MZM_PSPRITE_SEARCHLIGHT_EYE2:
    case MZM_PSPRITE_SEARCHLIGHT:
    case MZM_PSPRITE_SEARCHLIGHT2:
    case MZM_PSPRITE_SEARCHLIGHT3:
    case MZM_PSPRITE_SEARCHLIGHT4: return MZM_FAMILY_CHOZODIA_SECURITY;
    default: return is_drop(sprite_id) ? MZM_FAMILY_DROP : -1;
    }
}

/* SPRITE_CLEAR_AND_SET_ISFT (bit 7 is the power bomb stun). */
static void set_isft(MzmSprite *s, uint8_t value) {
    s->isft = (uint8_t)((s->isft & 0x80) | value);
}

/* SpriteUtilCheckObjectsTouching */
static bool touching(uint16_t o1_top, uint16_t o1_bottom, uint16_t o1_left, uint16_t o1_right,
                     uint16_t o2_top, uint16_t o2_bottom, uint16_t o2_left, uint16_t o2_right) {
    return o2_bottom >= o1_top && o2_top < o1_bottom && o2_right >= o1_left && o2_left < o1_right;
}

/* SpriteUtilSpriteTakeDamageFromSamusContact without the speed booster,
 * shinespark and charged pseudo screw (not ported on Samus): true when the
 * screw attack destroyed the sprite (DCT_SCREW_ATTACK). */
/* GET_PSPRITE_ / GET_SSPRITE_ stats */
static uint8_t weakness_of(const MzmSprite *s, const MzmSpriteTables *tables) {
    return s->secondary ? tables->secondary_weakness[s->sprite_id] : tables->weakness[s->sprite_id];
}

static uint16_t damage_of(const MzmSprite *s, const MzmSpriteTables *tables) {
    return s->secondary ? tables->secondary_damage[s->sprite_id] : tables->damage[s->sprite_id];
}

static bool contact_kills(MzmSprite *s, const MzmSpriteSamus *samus,
                          const MzmSpriteTables *tables) {
    if (samus->suitless || !samus->screw_attacking) return false;
    /* Sprites immune to projectiles take no contact damage either. */
    if (s->immune_to_projectiles) return false;
    /* Sprites ignoring projectiles take no contact damage. */
    if (s->status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES) return false;
    if (weakness_of(s, tables) & MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK) {
        s->health = 0;
        s->freeze_timer = 0;
        s->palette_row = 0;
        s->pose = SPRITE_POSE_SCREW_ATTACK_DESTROYED;
        s->ignore_samus_timer = 1;
        set_isft(s, ISFT_SCREW_ATTACK);
        s->damaged = true;
        return true;
    }
    if ((s->isft & 0x7F) < ISFT_SOLID_HIT) set_isft(s, ISFT_SOLID_HIT);
    return false;
}

void mzm_sprites_samus_collision(MzmSprite *sprites, int count, MzmSpriteSamus *samus,
                                 const MzmSpriteTables *tables) {
    uint16_t samus_top = (uint16_t)(samus->y + samus->hitbox_top);
    uint16_t samus_bottom = (uint16_t)(samus->y + samus->hitbox_bottom);
    uint16_t samus_left = (uint16_t)(samus->x + samus->hitbox_left);
    uint16_t samus_right = (uint16_t)(samus->x + samus->hitbox_right);
    const uint16_t check = MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN;
    bool ignore = false;            /* gIgnoreSamusAndSpriteCollision */
    for (int i = 0; i < count && !ignore; ++i) {
        MzmSprite *s = &sprites[i];
        if ((s->status & check) != check || s->ignore_samus_timer) continue;
        uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
        uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
        if (!touching(samus_top, samus_bottom, samus_left, samus_right, top, bottom, left, right))
            continue;
        uint16_t center_x = (uint16_t)(left + (uint16_t)(right - left) / 2);
        bool on_left = center_x >= samus->previous_x;
        /* Frozen sprites are solid: not ported (no ice beam). */
        if (s->freeze_timer) continue;
        switch (s->samus_collision) {
        case SSC_NONE: break;
        case SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT:
        case SSC_SPACE_PIRATE_LASER:
            s->pose = SPRITE_POSE_STOPPED;
            /* fallthrough */
        case SSC_HURTS_SAMUS:
        case SSC_SPACE_PIRATE:      /* same as the hurt code */
            if (contact_kills(s, samus, tables)) break;
            if (!samus->invincible && samus->hurt) {
                samus->hurt(samus->context, damage_of(s, tables), on_left);
                samus->invincible = true;
            }
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        case SSC_CHECK_COLLIDING:
        case SSC_SMALL_ENERGY_DROP:
        case SSC_LARGE_ENERGY_DROP:
        case SSC_MISSILE_DROP:
        case SSC_SUPER_MISSILE_DROP:
        case SSC_POWER_BOMB_DROP:
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            ignore = true;
            break;
        default:                    /* no other collision is ported */
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        }
    }
    for (int i = 0; i < count; ++i)
        if (sprites[i].status & MZM_SPRITE_STATUS_EXISTS && sprites[i].ignore_samus_timer)
            sprites[i].ignore_samus_timer--;
}

/* ProjectileDealDamage */
static void deal_damage(MzmSprite *s, uint16_t damage) {
    if (s->health > damage) {
        s->health -= damage;
    } else {
        s->health = 0;
        s->freeze_timer = 0;
        s->palette_row = 0;
        s->pose = SPRITE_POSE_DESTROYED;
        s->ignore_samus_timer = 1;
    }
    set_isft(s, ISFT_DAMAGED);
    s->damaged = true;
}

/* ProjectileHitSolidSprite */
static void hit_solid(MzmSprite *s) {
    if ((s->isft & 0x7F) < ISFT_SOLID_HIT) set_isft(s, ISFT_SOLID_HIT);
}

void mzm_sprites_projectile_hits(MzmSprite *sprites, int count, MzmWeapons *weapons,
                                 const MzmSpriteTables *tables, MzmEffects *effects) {
    /* The sprites are visited by draw order, then slot; every ported sprite
     * has the default draw order, so slot order. A sprite's hitbox is read
     * once, before the projectiles that hit it. */
    for (int i = 0; i < count; ++i) {
        MzmSprite *s = &sprites[i];
        if ((s->status & (MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_IGNORE_PROJECTILES)) !=
                MZM_SPRITE_STATUS_EXISTS || s->health == 0)
            continue;
        uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
        uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
        uint8_t weakness = weakness_of(s, tables);
        for (int p = 0; p < MZM_MAX_PROJECTILES; ++p) {
            MzmProjectile *proj = &weapons->list[p];
            if (!proj->active) continue;
            if (proj->type == MZM_PROJECTILE_BOMB) {
                /* ProjectileBombHitSprite: the bomb's hitbox, no particle,
                 * and the bomb stays. */
                if (!proj->hits_sprites) continue;
                uint16_t by = (uint16_t)proj->y, bx = (uint16_t)proj->x;
                if (!touching(top, bottom, left, right, (uint16_t)(by - (BLOCK_SIZE - PIXEL_SIZE)),
                              (uint16_t)(by + THREE_QUARTER_BLOCK_SIZE),
                              (uint16_t)(bx - THREE_QUARTER_BLOCK_SIZE),
                              (uint16_t)(bx + THREE_QUARTER_BLOCK_SIZE)))
                    continue;
                if (s->immune_to_projectiles) {
                    if ((s->isft & 0x7F) < 2) set_isft(s, 2);
                } else if (weakness & MZM_WEAKNESS_BEAM_BOMBS) {
                    deal_damage(s, BOMB_DAMAGE);
                } else {
                    hit_solid(s);
                }
                continue;
            }
            /* ProjectileProcessNormalBeam/Missile: +-EIGHTH_BLOCK_SIZE;
             * ProjectileProcessSuperMissile: +-(QUARTER_BLOCK_SIZE - PIXEL_SIZE). */
            int half = proj->type == MZM_PROJECTILE_SUPER_MISSILE ? QUARTER_BLOCK_SIZE - PIXEL_SIZE
                                                                  : EIGHTH_BLOCK_SIZE;
            uint16_t py = (uint16_t)proj->y, px = (uint16_t)proj->x;
            if (!touching(top, bottom, left, right, (uint16_t)(py - half), (uint16_t)(py + half),
                          (uint16_t)(px - half), (uint16_t)(px + half)))
                continue;
            /* The particle at the projectile: its impact, or the
             * invincible impact when the sprite is not weak to it. */
            uint8_t effect = MZM_PE_HITTING_SOMETHING_INVINCIBLE;
            if (s->immune_to_projectiles) {
                /* ProjectileHitSpriteImmuneToProjectiles (missiles would
                 * tumble: not ported). */
                if ((s->isft & 0x7F) < 2) set_isft(s, 2);
                if (effects) mzm_particle_set(effects, py, px, effect);
                proj->active = false;
                continue;
            }
            switch (proj->type) {
            case MZM_PROJECTILE_BEAM:   /* ProjectileHitSprite */
                if (weakness & MZM_WEAKNESS_BEAM_BOMBS) {
                    deal_damage(s, NORMAL_BEAM_DAMAGE);
                    effect = MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM;
                } else {
                    hit_solid(s);
                }
                break;
            case MZM_PROJECTILE_MISSILE:    /* ProjectileMissileHitSprite */
                if (weakness & MZM_WEAKNESS_MISSILES) {
                    deal_damage(s, MISSILE_DAMAGE);
                    effect = MZM_PE_HITTING_SOMETHING_WITH_MISSILE;
                } else {
                    hit_solid(s);   /* ProjectileStartTumblingMissile: not ported */
                }
                break;
            case MZM_PROJECTILE_BOMB:
                break;
            case MZM_PROJECTILE_SUPER_MISSILE:
                if (weakness & (MZM_WEAKNESS_MISSILES | MZM_WEAKNESS_SUPER_MISSILES)) {
                    deal_damage(s, SUPER_MISSILE_DAMAGE);
                    effect = MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE;
                } else {
                    hit_solid(s);
                }
                break;
            }
            if (effects) mzm_particle_set(effects, py, px, effect);
            proj->active = false;
        }
    }
    for (int p = 0; p < MZM_MAX_PROJECTILES; ++p) weapons->list[p].hits_sprites = false;
}

bool mzm_sprite_supported(uint8_t sprite_id) {
    return mzm_sprite_family(sprite_id, false) >= 0;
}

uint8_t mzm_sprite_graphics_owner(const MzmSprite *sprite) {
    if (!sprite->secondary) return sprite->sprite_id;
    if (sprite->sprite_id == MZM_SSPRITE_ZEELA_EYES) return MZM_PSPRITE_ZEELA_RED;
    if (sprite->sprite_id == MZM_SSPRITE_SPACE_PIRATE_LASER) return MZM_PSPRITE_SPACE_PIRATE;
    if (sprite->sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2 ||
        sprite->sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_PROJECTILE)
        return MZM_PSPRITE_SEARCHLIGHT_EYE;
    return sprite->sprite_id == MZM_SSPRITE_BLUE_SKREE_EXPLOSION ? MZM_PSPRITE_SKREE_BLUE
                                                                  : MZM_PSPRITE_SKREE_GREEN;
}

const char *mzm_sprite_anim_name(const MzmSprite *sprite) {
    int family = mzm_sprite_family(sprite->sprite_id, sprite->secondary);
    if (family < 0 || sprite->anim >= mzm_sprite_families[family].count) return NULL;
    return mzm_sprite_families[family].anims[sprite->anim];
}

void mzm_sprite_init_primary(MzmSprite *sprite, uint8_t sprite_id, uint8_t gfx_slot,
                             uint16_t y_block, uint16_t x_block, uint8_t room_slot,
                             uint8_t ram_slot) {
    memset(sprite, 0, sizeof *sprite);
    sprite->status = MZM_SPRITE_STATUS_EXISTS;
    sprite->sprite_id = sprite_id;
    sprite->gfx_slot = gfx_slot;
    sprite->room_slot = room_slot;
    sprite->ram_slot = ram_slot;
    /* Bottom middle of the block. */
    sprite->y = (uint16_t)(y_block * BLOCK_SIZE + BLOCK_SIZE);
    sprite->x = (uint16_t)(x_block * BLOCK_SIZE + HALF_BLOCK_SIZE);
    sprite->pose = SPRITE_POSE_UNINITIALIZED;
    sprite->ignore_samus_timer = 1;
}

/* SpriteUpdateAnimation */
static void update_animation(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) return;
    s->adc++;
    if (timer(c, s->anim, s->frame) < s->adc) {
        s->adc = 1;
        s->frame++;
        if (timer(c, s->anim, s->frame) == 0) s->frame = 0;
    }
}

/* SpriteCheckOnScreen (sprites without an absolute position). */
static void check_on_screen(const Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t bg_y = (uint16_t)(c->world->camera_y + HALF_SCREEN_BLOCKS_SUB_PIXEL);
    uint16_t sprite_y = (uint16_t)(s->y + HALF_SCREEN_BLOCKS_SUB_PIXEL);
    uint16_t bottom = (uint16_t)(bg_y - s->draw_bottom * PIXEL_SIZE);
    uint16_t top = (uint16_t)(bg_y + s->draw_top * PIXEL_SIZE + SCREEN_SIZE_Y_SUB_PIXEL);
    uint16_t bg_x = (uint16_t)(c->world->camera_x + HALF_SCREEN_BLOCKS_SUB_PIXEL);
    uint16_t sprite_x = (uint16_t)(s->x + HALF_SCREEN_BLOCKS_SUB_PIXEL);
    uint16_t left = (uint16_t)(bg_x - s->draw_horizontal * PIXEL_SIZE);
    uint16_t right = (uint16_t)(bg_x + s->draw_horizontal * PIXEL_SIZE + SCREEN_SIZE_X_SUB_PIXEL);
    if (left < sprite_x && sprite_x < right && bottom < sprite_y && sprite_y < top) {
        s->status |= MZM_SPRITE_STATUS_ONSCREEN;
        return;
    }
    s->status &= (uint16_t)~MZM_SPRITE_STATUS_ONSCREEN;
    if (s->kill_off_screen) {
        /* Removed beyond 9 blocks above or left of the screen, 9 below and
         * 14 right of it (the native ranges). */
        bg_y = (uint16_t)(c->world->camera_y + BLOCK_SIZE * 10);
        sprite_y = (uint16_t)(s->y + BLOCK_SIZE * 10);
        bottom = (uint16_t)(bg_y - BLOCK_SIZE * 9);
        top = (uint16_t)(bg_y + BLOCK_SIZE * 19);
        bg_x = (uint16_t)(c->world->camera_x + BLOCK_SIZE * 10);
        sprite_x = (uint16_t)(s->x + BLOCK_SIZE * 10);
        left = (uint16_t)(bg_x - BLOCK_SIZE * 9);
        right = (uint16_t)(bg_x + BLOCK_SIZE * 24);
        if (left >= sprite_x || sprite_x >= right || bottom >= sprite_y || sprite_y >= top)
            s->status = 0;
    }
}

/* The SP_DAMAGED sound of each primary AI (none for Rinkas, the Space
 * Pirates and the security systems). */
static uint16_t damaged_sound(int family) {
    switch (family) {
    case MZM_FAMILY_ZOOMER: return MZM_SOUND_ZOOMER_DAMAGED;
    case MZM_FAMILY_ZEELA: return MZM_SOUND_ZEELA_DAMAGED;
    case MZM_FAMILY_RIPPER: return MZM_SOUND_RIPPER_DAMAGED;
    case MZM_FAMILY_WAVER: return MZM_SOUND_WAVER_DAMAGED;
    case MZM_FAMILY_SIDEHOPPER: return MZM_SOUND_SIDEHOPPER_DAMAGED;
    case MZM_FAMILY_DESSGEEGA: return MZM_SOUND_DESSGEEGA_DAMAGED;
    case MZM_FAMILY_VIOLA: return MZM_SOUND_VIOLA_DAMAGED;
    case MZM_FAMILY_SOVA: return MZM_SOUND_SOVA_DAMAGED;
    case MZM_FAMILY_ZEB: return MZM_SOUND_ZEB_DAMAGED;
    case MZM_FAMILY_GEEGA: return MZM_SOUND_GEEGA_DAMAGED;
    case MZM_FAMILY_ZEBBO: return MZM_SOUND_ZEBBO_DAMAGED;
    case MZM_FAMILY_REO: return MZM_SOUND_REO_DAMAGED;
    case MZM_FAMILY_MELLA: return MZM_SOUND_MELLA_DAMAGED;
    case MZM_FAMILY_HOLTZ: return MZM_SOUND_HOLTZ_DAMAGED;
    case MZM_FAMILY_MULTIVIOLA: return MZM_SOUND_MULTIVIOLA_DAMAGED;
    case MZM_FAMILY_SKREE: return MZM_SOUND_SKREE_DAMAGED;
    default: return 0;
    }
}

void mzm_sprite_update(MzmSprite *sprite, uint8_t slot, const MzmSpriteFrame *frame) {
    int family = mzm_sprite_family(sprite->sprite_id, sprite->secondary);
    /* A family without frame durations (not exported) is not updated. */
    if (!(sprite->status & MZM_SPRITE_STATUS_EXISTS) || family < 0 || !frame->kinds[family])
        return;
    /* The Space Pirates and the security systems need the game globals. */
    if ((family == MZM_FAMILY_SPACE_PIRATE || family == MZM_FAMILY_CHOZODIA_SECURITY) &&
        !frame->globals)
        return;
    Ctx c = {sprite, frame, frame->world, frame->tables, frame->kinds[family],
             0, MZM_COLLISION_AIR, frame->frame8};
    /* gSpriteRng = sSpriteRandomNumberTable[(frame8 + slot + frame16 / 16 +
     * x + y) % 32] */
    c.rng = c.tables->random[((uint32_t)frame->frame8 + slot + frame->frame16 / 16u + sprite->x +
                              sprite->y) % 32u];
    /* SpriteUtilUpdateStunTimer: the stun palette flash is not drawn yet. */
    if (sprite->isft & 0x7F) sprite->isft--;
    /* The first lines of each primary AI with a damaged sound. */
    uint16_t damaged = sprite->secondary ? 0 : damaged_sound(family);
    if (damaged && sprite->damaged) {
        sprite->damaged = false;
        if (sprite->status & MZM_SPRITE_STATUS_ONSCREEN) mzm_sfx_play_not_playing(damaged);
    }
    switch (family) {
    case MZM_FAMILY_ZOOMER:
    case MZM_FAMILY_ZEELA:
        if (sprite->secondary) zeela_eyes(&c);
        else zoomer(&c);
        break;
    case MZM_FAMILY_RIPPER: ripper(&c); break;
    case MZM_FAMILY_WAVER: waver(&c); break;
    case MZM_FAMILY_SIDEHOPPER:
    case MZM_FAMILY_DESSGEEGA: hopper(&c); break;
    case MZM_FAMILY_VIOLA: viola(&c); break;
    case MZM_FAMILY_SOVA: sova(&c); break;
    case MZM_FAMILY_ZEB:
    case MZM_FAMILY_GEEGA:
    case MZM_FAMILY_ZEBBO: pipe_bug(&c); break;
    case MZM_FAMILY_REO: reo(&c); break;
    case MZM_FAMILY_MELLA: mella(&c); break;
    case MZM_FAMILY_HOLTZ: holtz(&c); break;
    case MZM_FAMILY_MULTIVIOLA: multiviola(&c); break;
    case MZM_FAMILY_RINKA: rinka(&c); break;
    case MZM_FAMILY_SPACE_PIRATE:
        if (sprite->secondary) ai_space_pirate_laser(&c);
        else ai_space_pirate(&c);
        break;
    case MZM_FAMILY_CHOZODIA_SECURITY: ai_chozodia_security(&c); break;
    case MZM_FAMILY_SKREE:
        if (sprite->secondary) skree_explosion(&c);
        else skree(&c);
        break;
    default: enemy_drop(&c); break;
    }
    if (sprite->status & MZM_SPRITE_STATUS_EXISTS) {
        /* A sprite that just became a drop finishes the frame on its
         * previous OAM (pOam is only set by EnemyDropInit), so `c.kind`
         * stays the one it started the frame with. */
        update_animation(&c);
        check_on_screen(&c);
    }
}

const char *const mzm_debris_table_names[MZM_DEBRIS_TABLES] = {
    "sDebrisBouncingVelocity", "sDebris11YVelocity", "sDebris12YVelocity",
    "sDebris13YVelocity", "sDebris4YVelocity", "sDebris5YVelocity", "sDebris6YVelocity",
    "sDebris7YVelocity", "sDebris8YVelocity",
};

enum { SPRITE_DEBRIS_BOUNCING = 0x80 };

void mzm_sprite_debris_init(MzmEffects *effects, uint8_t type, uint16_t y, uint16_t x) {
    MzmSpriteDebris *d = NULL;
    for (int i = 0; i < MZM_MAX_SPRITE_DEBRIS && !d; ++i)
        if (!effects->debris[i].exists) d = &effects->debris[i];
    /* With every slot taken the game writes past gSpriteDebris; the port
     * drops the piece. */
    if (!d) return;
    d->exists = true;
    d->y = y;
    d->x = x;
    d->frame = 0;
    d->adc = 0;
    d->type = type;
    d->frame_counter = 0;
    d->offset = 0;
}

/* A falling arc: on landing (after `min_frames` for the half arcs) it
 * bounces with the given direction. */
static void debris_fall(Ctx *c, MzmSpriteDebris *d, const int16_t *velocity, unsigned offset,
                        int dx, uint8_t bounce, uint8_t min_frames) {
    d->y = (uint16_t)(d->y + velocity[offset]);
    d->x = (uint16_t)(d->x + dx);
    uint16_t top = ai_check_vertical_slopes(c, d->y, d->x);
    /* SpriteDebrisSetSplash: room liquids are not ported. */
    if (c->vertical != MZM_COLLISION_AIR && d->frame_counter >= min_frames) {
        d->y = top;
        d->type = (uint8_t)(SPRITE_DEBRIS_BOUNCING | bounce);
        d->offset = 0;
    }
}

/* SpriteDebrisProcess */
static void debris_process(Ctx *c, MzmSpriteDebris *d) {
    const int16_t (*v)[9] = c->tables->debris;
    if (++d->frame_counter == 0) {
        d->exists = false;
        return;
    }
    unsigned offset = d->offset / 2u;
    if (offset > 8) offset = 8;
    switch (d->type) {
    case 1:
        if (d->frame_counter <= 8) return;  /* CONVERT_SECONDS(2.f / 15) */
        /* fallthrough */
    case 0x11: debris_fall(c, d, v[MZM_DEBRIS_11], offset, -1, 1, 0); break;
    case 2:
        if (d->frame_counter <= 5) return;  /* CONVERT_SECONDS(1.f / 12) */
        /* fallthrough */
    case 0x12: debris_fall(c, d, v[MZM_DEBRIS_12], offset, 1, 2, 0); break;
    case 3:
        if (d->frame_counter <= 2) return;  /* CONVERT_SECONDS(1.f / 30) */
        /* fallthrough */
    case 0x13: debris_fall(c, d, v[MZM_DEBRIS_13], offset, 2, 3, 0); break;
    case 4: debris_fall(c, d, v[MZM_DEBRIS_4], offset, -2, 4, 0); break;
    case 5: debris_fall(c, d, v[MZM_DEBRIS_5], offset, -1, 1, 20); break;
    case 6: debris_fall(c, d, v[MZM_DEBRIS_6], offset, 1, 2, 20); break;
    case 7: debris_fall(c, d, v[MZM_DEBRIS_7], offset, 1, 3, 20); break;
    case 8: debris_fall(c, d, v[MZM_DEBRIS_8], offset, -1, 4, 20); break;
    case SPRITE_DEBRIS_BOUNCING | 1:
    case SPRITE_DEBRIS_BOUNCING | 2:
    case SPRITE_DEBRIS_BOUNCING | 3:
    case SPRITE_DEBRIS_BOUNCING | 4: {
        static const int8_t dx[5] = {0, -1, 1, 2, -2};
        d->y = (uint16_t)(d->y + v[MZM_DEBRIS_BOUNCING][offset]);
        d->x = (uint16_t)(d->x + dx[d->type & 7]);
        break;
    }
    default:
        d->exists = false;
        break;
    }
    d->offset++;
}

void mzm_sprite_debris_process(MzmEffects *effects, const MzmSpriteWorld *world,
                               const MzmSpriteTables *tables, const MzmEffectKinds *kinds) {
    Ctx c = {NULL, NULL, world, tables, NULL, 0, MZM_COLLISION_AIR, 0};
    for (int i = 0; i < MZM_MAX_SPRITE_DEBRIS; ++i) {
        MzmSpriteDebris *d = &effects->debris[i];
        if (!d->exists) continue;
        debris_process(&c, d);
        /* SpriteDebrisProcessAll animates even a piece that just ended. */
        d->adc++;
        uint8_t current = d->frame < kinds->debris_count ? kinds->debris_timers[d->frame] : 0;
        if (current < d->adc) {
            d->adc = 1;
            d->frame++;
            if (d->frame >= kinds->debris_count || kinds->debris_timers[d->frame] == 0) d->frame = 0;
        }
    }
    /* SpriteDebrisDraw: pieces outside the vertical range are removed. */
    for (int i = 0; i < MZM_MAX_SPRITE_DEBRIS; ++i) {
        MzmSpriteDebris *d = &effects->debris[i];
        if (!d->exists || d->frame_counter == 0) continue;
        int camera = world->camera_y, y = d->y + BLOCK_SIZE * 4;
        if (camera + BLOCK_SIZE * 3 > y || camera + BLOCK_SIZE * 15 < y) d->exists = false;
    }
}
