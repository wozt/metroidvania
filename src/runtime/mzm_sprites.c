/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission sprites; comments name the pinned decompilation's routines. */
#include "mzm_sprites.h"
#include "mzm_sprite_ai.h"
#include "mzm_blocks.h"
#include "mzm_screen_shake.h"
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
    "EnemyDropOam_MultipleLargeEnergySlot1", "EnemyDropOam_MultipleLargeEnergySlot2",
    "EnemyDropOam_MultipleLargeEnergySlot3",
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
const char *const mzm_gamet_anim_names[MZM_PIPE_BUG_ANIMS] = {"GametOam_Idle", "GametOam_Moving"};
static const char *const gamet_sprites[MZM_PIPE_BUG_ANIMS] = {"GAMET_BLUE_SINGLE", "GAMET_BLUE_SINGLE"};
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
const char *const mzm_geruta_anim_names[MZM_GERUTA_ANIMS] = {
    "GerutaOam_Idle", "GerutaOam_Warning", "GerutaOam_Launching", "GerutaOam_GoingDown",
    "GerutaOam_Bouncing", "GerutaOam_GoingUp", "GerutaOam_BouncingOnCeiling",
};
static const char *const geruta_sprites[MZM_GERUTA_ANIMS] = {
    "GERUTA_RED", "GERUTA_RED", "GERUTA_RED", "GERUTA_RED", "GERUTA_RED", "GERUTA_RED",
    "GERUTA_RED",
};
const char *const mzm_ripper2_anim_names[MZM_RIPPER2_ANIMS] = {
    "Ripper2Oam_Moving", "Ripper2Oam_TurningAround", "Ripper2Oam_TurningAroundPart2",
};
static const char *const ripper2_sprites[MZM_RIPPER2_ANIMS] = {"RIPPERII", "RIPPERII", "RIPPERII"};
const char *const mzm_item_sprite_anim_names[MZM_ITEM_SPRITE_ANIMS] = {
    "MorphBallOam_Idle", "MorphBallOutsideOam_Idle", "PowerGripOam_Idle", "PowerGripGlowOam_Idle",
    "ChargeBeamOam_Spawning", "ChargeBeamOam_Visible", "ChargeBeamGlowOam_Idle",
};
static const char *const item_sprites[MZM_ITEM_SPRITE_ANIMS] = {
    "MORPH_BALL", "MORPH_BALL", "POWER_GRIP", "POWER_GRIP", "CHARGE_BEAM", "CHARGE_BEAM",
    "CHARGE_BEAM",
};
const char *const mzm_gadget_anim_names[MZM_GADGET_ANIMS] = {
    "ElevatorPadOam_Idle", "ElevatorPadOam_Moving",
    "WaterDropOam_Spawning", "WaterDropOam_Falling", "WaterDropOam_Splashing",
    "SteamOam_VerticalLarge", "SteamOam_VerticalSmall", "SteamOam_HorizontalLarge",
    "SteamOam_HorizontalSmall", "SteamDiagonalOam_Large", "SteamDiagonalOam_Small",
    "GeronOam_Idle", "GeronOam_Shaking", "GeronOam_GettingDestroyed", "GeronOam_Destroyed",
    "GeronNorfairOam_Idle",
    "UnknownItemBlockOam_Idle", "UnknownItemBlockOam_Activated", "UnknownItemBlockOam_Exploding",
    "UnknownItemBlockLightOam_Idle",
    "MapStationOam_Idle", "MapStationOam_GrabbingSamus", "MapStationOam_Downloading",
    "MapStationOam_Retracting", "MapStationOam_Inactive", "MapStationPartOam_BackScreenIdle",
    "MapStationPartOam_BackScreenDownloading", "MapStationPartOam_BackScreenDownloaded",
    "MapStationPartOam_FrontScreenIdle", "MapStationPartOam_FrontScreenDownloading",
    "MapStationPartOam_FrontScreenDownloaded",
    "SavePlatformOam_Idle",
    "SavePlatformOam_Opening",
    "SavePlatformOam_Opened",
    "SavePlatformOam_Saving",
    "SavePlatformOam_OpenedOff",
    "SavePlatformOam_Closing",
    "SavePlatformOam_IdleOff",
    "SavePlatformPartOam_TopIdle",
    "SavePlatformPartOam_TopExtending",
    "SavePlatformPartOam_TopExtended",
    "SavePlatformPartOam_TopRetracting",
    "SavePlatformPartOam_TubeSpawning",
    "SavePlatformPartOam_TubeShadow",
    "SavePlatformPartOam_TubeIdle",
    "SavePlatformPartOam_TubeDespawning",
    "SavePlatformPartOam_Ray",
    "GunshipOam_Idle",
    "GunshipOam_Refilling",
    "GunshipOam_Flying",
    "GunshipPartOam_EntranceFrontClosed",
    "GunshipPartOam_EntranceFrontOpening",
    "GunshipPartOam_EntranceFrontOpened",
    "GunshipPartOam_EntranceFrontClosing",
    "GunshipPartOam_EntranceBackClosed",
    "GunshipPartOam_EntranceBackOpening",
    "GunshipPartOam_EntranceBackOpened",
    "GunshipPartOam_EntranceBackClosing",
    "GunshipPartOam_Platform",
    "GunshipPartOam_FlamesHorizontal",
    "GunshipPartOam_FlamesVertical",
    "AreaBannerOam_SpawnMiddle",
    "AreaBannerOam_StaticMiddle",
    "AreaBannerOam_RemovingMiddle",
    "AreaBannerOam_SpawnBottom",
    "AreaBannerOam_StaticBottom",
    "AreaBannerOam_RemovingBottom",
    "MorphBallLauncherOam_Idle",
    "MorphBallLauncherOam_Launching",
    "MorphBallLauncherPartOam_Back",
    "MorphBallLauncherPartOam_Energy",
    "ZiplineOam_OffIdle",
    "ZiplineOam_OffGrabbingSamus",
    "ZiplineOam_OffSamusGrabbed",
    "ZiplineOam_OffReleasingSamus",
    "ZiplineOam_OnIdle",
    "ZiplineOam_OnGrabbingSamus",
    "ZiplineOam_OnSamusGrabbed",
    "ZiplineOam_OnReleasingSamus",
    "ZiplineButtonOam_OffIdle",
    "ZiplineButtonOam_OnIdle",
    "ZiplineButtonOam_Active",
    "ZiplineGeneratorOam_Deactivated",
    "ZiplineGeneratorOam_Activating",
    "ZiplineGeneratorOam_Activated",
    "ZiplineGeneratorPartOam_ConductorDeactivated",
    "ZiplineGeneratorPartOam_ConductorActivating",
    "ZiplineGeneratorPartOam_ConductorActivated",
    "ZiplineGeneratorPartOam_MorphSymbolActivating",
    "ZiplineGeneratorPartOam_MorphSymbolActivated",
    "ZiplineGeneratorPartOam_ElectricityActivating",
    "SecurityGateOam_Opened",
    "SecurityGateOam_Closing",
    "SecurityGateOam_Closed",
    "SecurityGateOam_Opening",
    "PistonOam_Idle",
    "PistonOam_Opening",
    "PistonOam_Opened",
    "GlassTubeOam_Intact",
    "GlassTubeOam_Cracking",
    "GlassTubeOam_Breaking",
    "GlassTubeOam_Broken",
    "RisingChozoPillarPlatformOam_NoShadowSpawning",
    "RisingChozoPillarPlatformOam_NoShadowSpawned",
    "RisingChozoPillarPlatformOam_ShadowSpawning",
    "RisingChozoPillarPlatformOam_ShadowSpawned",
    "RisingChozoPillarPlatformShadowOam",
    "SavePlatformChozodiaOam_Idle",
    "SavePlatformChozodiaOam_Opening",
    "SavePlatformChozodiaOam_Opened",
    "SavePlatformChozodiaOam_Saving",
    "SavePlatformChozodiaOam_OpenedOff",
    "SavePlatformChozodiaOam_Closing",
    "SavePlatformChozodiaOam_IdleOff",
    "SavePlatformChozodiaPartOam_TopIdle",
    "SavePlatformChozodiaPartOam_TopExtending",
    "SavePlatformChozodiaPartOam_TopExtended",
    "SavePlatformChozodiaPartOam_TopRetracting",
    "SavePlatformChozodiaPartOam_TubeSpawning",
    "SavePlatformChozodiaPartOam_TubeShadow",
    "SavePlatformChozodiaPartOam_TubeIdle",
    "SavePlatformChozodiaPartOam_TubeDespawning",
    "SavePlatformChozodiaPartOam_Ray",
    "SavePlatformChozodiaPartOam_RefillLight",
};
static const char *const gadget_sprites[MZM_GADGET_ANIMS] = {
    "ELEVATOR_PAD", "ELEVATOR_PAD", "WATER_DROP", "WATER_DROP", "WATER_DROP",
    "STEAM_LARGE", "STEAM_LARGE", "STEAM_LARGE", "STEAM_LARGE", "STEAM_LARGE", "STEAM_LARGE",
    "GERON_BRINSTAR_ROOM_15", "GERON_BRINSTAR_ROOM_15", "GERON_BRINSTAR_ROOM_15",
    "GERON_BRINSTAR_ROOM_15", "GERON_NORFAIR",
    "PLASMA_BEAM_BLOCK", "PLASMA_BEAM_BLOCK", "PLASMA_BEAM_BLOCK", "PLASMA_BEAM_BLOCK",
    "MAP_STATION", "MAP_STATION", "MAP_STATION", "MAP_STATION", "MAP_STATION", "MAP_STATION",
    "MAP_STATION", "MAP_STATION", "MAP_STATION", "MAP_STATION", "MAP_STATION",
    "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM", "SAVE_PLATFORM",
    "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP", "GUNSHIP",
    "AREA_BANNER", "AREA_BANNER", "AREA_BANNER", "AREA_BANNER", "AREA_BANNER", "AREA_BANNER",
    "MORPH_BALL_LAUNCHER", "MORPH_BALL_LAUNCHER", "MORPH_BALL_LAUNCHER", "MORPH_BALL_LAUNCHER",
    "ZIPLINE", "ZIPLINE", "ZIPLINE", "ZIPLINE", "ZIPLINE", "ZIPLINE", "ZIPLINE", "ZIPLINE",
    "ZIPLINE_BUTTON", "ZIPLINE_BUTTON", "ZIPLINE_BUTTON",
    "ZIPLINE_GENERATOR", "ZIPLINE_GENERATOR", "ZIPLINE_GENERATOR", "ZIPLINE_GENERATOR",
    "ZIPLINE_GENERATOR", "ZIPLINE_GENERATOR", "ZIPLINE_GENERATOR", "ZIPLINE_GENERATOR",
    "ZIPLINE_GENERATOR",
    "SECURITY_GATE_DEFAULT_OPEN", "SECURITY_GATE_DEFAULT_OPEN", "SECURITY_GATE_DEFAULT_OPEN",
    "SECURITY_GATE_DEFAULT_OPEN",
    "PISTON", "PISTON", "PISTON", "GLASS_TUBE", "GLASS_TUBE", "GLASS_TUBE", "GLASS_TUBE",
    "RISING_CHOZO_PILLAR", "RISING_CHOZO_PILLAR", "RISING_CHOZO_PILLAR", "RISING_CHOZO_PILLAR",
    "RISING_CHOZO_PILLAR",
    "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA", "SAVE_PLATFORM_CHOZODIA",
};
const char *const mzm_dragon_anim_names[MZM_DRAGON_ANIMS] = {
    "DragonOam_Idle", "DragonOam_Warning", "DragonOam_Spitting", "DragonOam_TurningAround",
    "DragonOam_TurningAroundPart2", "DragonFireballOam_Moving", "DragonFireballOam_Exploding",
};
static const char *const dragon_sprites[MZM_DRAGON_ANIMS] = {
    "DRAGON", "DRAGON", "DRAGON", "DRAGON", "DRAGON", "DRAGON", "DRAGON",
};
const char *const mzm_parasite_anim_names[MZM_PARASITE_ANIMS] = {
    "ParasiteOam_Idle", "ParasiteOam_LandingAfterFalling", "ParasiteOam_Landing",
    "ParasiteOam_Tumbling", "ParasiteOam_TurningBackOnFeet", "ParasiteOam_TurningAround",
    "ParasiteOam_JumpingUp", "ParasiteOam_Attached", "ParasiteOam_Expelled", "ParasiteOam_Dying",
};
static const char *const parasite_sprites[MZM_PARASITE_ANIMS] = {
    "PARASITE", "PARASITE", "PARASITE", "PARASITE", "PARASITE",
    "PARASITE", "PARASITE", "PARASITE", "PARASITE", "PARASITE",
};
const char *const mzm_atomic_anim_names[MZM_ATOMIC_ANIMS] = {
    "AtomicOam_Idle", "AtomicElectricityOam_Charging", "AtomicElectricityOam_MovingVertical",
    "AtomicElectricityOam_MovingDiagonal", "AtomicElectricityOam_MovingHorizontal",
    "AtomicElectricityOam_ExplodingNonDiagonal", "AtomicElectricityOam_ExplodingDiagonal",
};
static const char *const atomic_sprites[MZM_ATOMIC_ANIMS] = {
    "ATOMIC", "ATOMIC", "ATOMIC", "ATOMIC", "ATOMIC", "ATOMIC", "ATOMIC",
};
const char *const mzm_polyp_anim_names[MZM_POLYP_ANIMS] = {
    "PolypOam_Idle", "PolypOam_Warning", "PolypOam_Spitting", "PolypOam_AfterSpitting",
    "PolypOam_Retracting", "PolypProjectileOam_Right", "PolypProjectileOam_Left",
    "PolypProjectileOam_Exploding",
};
static const char *const polyp_sprites[MZM_POLYP_ANIMS] = {
    "POLYP", "POLYP", "POLYP", "POLYP", "POLYP", "POLYP", "POLYP", "POLYP",
};
const char *const mzm_worker_robot_anim_names[MZM_WORKER_ROBOT_ANIMS] = {
    "WorkerRobotOam_Walking", "WorkerRobotOam_WalkingBackwards", "WorkerRobotOam_Standing",
    "WorkerRobotOam_BackToSleep", "WorkerRobotOam_Sleeping", "WorkerRobotOam_GoingToSleep",
    "WorkerRobotOam_TurningAround", "WorkerRobotOam_WakingUp",
};
static const char *const worker_robot_sprites[MZM_WORKER_ROBOT_ANIMS] = {
    "WORKER_ROBOT", "WORKER_ROBOT", "WORKER_ROBOT", "WORKER_ROBOT",
    "WORKER_ROBOT", "WORKER_ROBOT", "WORKER_ROBOT", "WORKER_ROBOT",
};
const char *const mzm_kraid_anim_names[MZM_KRAID_ANIMS] = {
    "KraidOam_MouthClosed",
    "KraidOam_MouthClosedBlink",
    "KraidOam_OpeningMouth",
    "KraidOam_MouthOpened",
    "KraidOam_Rising",
    "KraidOam_ClosingMouth",
    "KraidPartOam_2cac5c",
    "KraidPartOam_LeftArmIdle",
    "KraidPartOam_LeftArmDying",
    "KraidPartOam_LeftArmThrowingNails",
    "KraidPartOam_2cadc4",
    "KraidPartOam_RightArmIdle",
    "KraidPartOam_RightArmAttacking",
    "KraidPartOam_RightArmDying",
    "KraidPartOam_LeftFeetRising",
    "KraidPartOam_LeftFeetIdle1",
    "KraidPartOam_LeftFeetMovingRight",
    "KraidPartOam_LeftFeetIdle2",
    "KraidPartOam_LeftFeetMovedRight",
    "KraidPartOam_LeftFeetMovingLeft",
    "KraidPartOam_LeftFeetMovedLeft",
    "KraidPartOam_RightFeetRising",
    "KraidPartOam_RightFeetIdle1",
    "KraidPartOam_RightFeetMovedRight",
    "KraidPartOam_RightFeetIdle2",
    "KraidPartOam_RightFeetMovingRight",
    "KraidPartOam_RightFeetMovedLeft",
    "KraidPartOam_RightFeetMovingLeft",
    "KraidPartOam_TopHoleLeft",
    "KraidPartOam_TopHoleRight",
    "KraidPartOam_MiddleHoleLeft",
    "KraidPartOam_MiddleHoleRight",
    "KraidPartOam_BottomHoleLeft",
    "KraidPartOam_BottomHoleRight",
    "KraidNailOam",
    "KraidOam_2cb29c",
    "KraidOam_2cb2ac",
    "KraidSpikeOam",
};
static const char *const kraid_sprites[MZM_KRAID_ANIMS] = {
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
    "KRAID",
};
const char *const mzm_kraid_multi_names[MZM_KRAID_MULTI_COUNT] = {
    "sKraidMultiSpriteData_Rising", "sKraidMultiSpriteData_Standing",
    "sKraidMultiSpriteData_StandingBetweenSteps", "sKraidMultiSpriteData_MovingLeftFeetToRight",
    "sKraidMultiSpriteData_MovingRightFeetToRight", "sKraidMultiSpriteData_MovingLeftFeetToLeft",
    "sKraidMultiSpriteData_MovingRightFeetToLeft", "sKraidMultiSpriteData_Dying1",
    "sKraidMultiSpriteData_Dying2",
};
const char *const mzm_ridley_anim_names[MZM_RIDLEY_ANIMS] = {
    "RidleyOam_Idle",
    "RidleyOam_SpittingFireballs",
    "RidleyOam_TurningAroundFirstPart",
    "RidleyOam_TurningAroundSecondPart",
    "RidleyPartOam_HeadIdle",
    "RidleyPartOam_OpeningMouth",
    "RidleyPartOam_MouthOpened",
    "RidleyPartOam_HeadDying",
    "RidleyPartOam_HeadTurningAround",
    "RidleyPartOam_ClawIdle",
    "RidleyPartOam_ClawSpittingFireballs",
    "RidleyPartOam_ClawTurningAroundFirstPart",
    "RidleyPartOam_ClawTurningAroundSecondPart",
    "RidleyPartOam_ClawCarryingSamus",
    "RidleyPartOam_ClawLiftingSamus",
    "RidleyPartOam_ClawSamusLifted",
    "RidleyPartOam_ClawReleasingSamus",
    "RidleyPartOam_LeftWingIdle",
    "RidleyPartOam_RightWingIdle",
    "RidleyPartOam_LeftWing_Unused",
    "RidleyPartOam_RightWing_Unused",
    "RidleyPartOam_LeftWingSpittingFireballs",
    "RidleyPartOam_RightWingSpittingFireballs",
    "RidleyTailOam_Part",
    "RidleyTailOam_TipPointingDown",
    "RidleyTailOam_TipPointingUp",
    "RidleyTailOam_TipPointingDiagonallyDownRight",
    "RidleyTailOam_TipPointingDiagonallyUpRight",
    "RidleyTailOam_TipPointingDiagonallyDownLeft",
    "RidleyTailOam_TipPointingDiagonallyUpLeft",
    "RidleyOam_Square",
    "RidleyFireballOam_Small",
    "RidleyFireballOam_Big",
};
static const char *const ridley_sprites[MZM_RIDLEY_ANIMS] = {
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
    "RIDLEY",
};
const char *const mzm_ridley_multi_names[MZM_RIDLEY_MULTI_COUNT] = {
    "sRidleyMultiSpriteData_Idle",
    "sRidleyMultiSpriteData_SpittingFireballs",
    "sRidleyMultiSpriteData_TurningAroundFirstPart",
    "sRidleyMultiSpriteData_TurningAroundSecondPart",
    "sRidleyTailMultiSpriteData_Idle",
    "sRidleyTailMultiSpriteData_Dying",
    "sRidleyTailMultiSpriteData_MoveToAttack",
    "sRidleyTailMultiSpriteData_SettingUpDiagonalTailAttack",
    "sRidleyTailMultiSpriteData_ChargingDiagonalTailAttack",
    "sRidleyTailMultiSpriteData_DiagonalTailAttack",
    "sRidleyTailMultiSpriteData_ChargingVerticalTailAttack",
    "sRidleyTailMultiSpriteData_VerticalTailAttack",
    "sRidleyTailMultiSpriteData_LastVerticalAttack",
    "sRidleyTailMultiSpriteData_BackToIdle",
};
const char *const mzm_imago_larva_anim_names[MZM_IMAGO_LARVA_ANIMS] = {
    "ImagoLarvaPartOam_ShellAttacking",
    "ImagoLarvaPartOam_ShellIdle",
    "ImagoLarvaPartOam_ShellRetreating",
    "ImagoLarvaPartOam_ShellDying",
    "ImagoLarvaPartOam_ClawsAttacking",
    "ImagoLarvaPartOam_ClawsIdle",
    "ImagoLarvaPartOam_ClawsRetreating",
    "ImagoLarvaPartOam_ClawsTakingDamage",
    "ImagoLarvaPartOam_LeftDotAppearing",
    "ImagoLarvaPartOam_MiddleDotAppearing",
    "ImagoLarvaPartOam_RightDotAppearing",
    "ImagoLarvaPartOam_LeftDotVisible",
    "ImagoLarvaPartOam_MiddleDotVisible",
    "ImagoLarvaPartOam_RightDotVisible",
    "ImagoLarvaPartOam_LeftDotDisappearing",
    "ImagoLarvaPartOam_MiddleDotDisappearing",
    "ImagoLarvaPartOam_RightDotDisappearing",
    "ImagoLarvaPartOam_ShellWarning",
    "ImagoLarvaOam_Idle",
    "ImagoLarvaPartOam_ClawsWarningFirstPart",
    "ImagoLarvaPartOam_ClawsWarningSecondPart",
    "ImagoLarvaOam_Warning",
    "ImagoLarvaPartOam_ShellTakingDamage",
};
static const char *const imago_larva_sprites[MZM_IMAGO_LARVA_ANIMS] = {
    "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT",
    "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT",
    "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT",
    "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT",
    "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT",
    "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT", "IMAGO_LARVA_RIGHT",
};
const char *const
mzm_imago_larva_right_side_anim_names[MZM_IMAGO_LARVA_RIGHT_SIDE_ANIMS] = {
    "ImagoLarvaRightSideOam",
};
static const char *const
imago_larva_right_side_sprites[MZM_IMAGO_LARVA_RIGHT_SIDE_ANIMS] = {
    "IMAGO_LARVA_RIGHT_SIDE",
};
const char *const mzm_imago_cocoon_anim_names[MZM_IMAGO_COCOON_ANIMS] = {
    "ImagoCocoonOam_CeilingVineMotionless",
    "ImagoCocoonOam_CeilingVineMoving",
    "ImagoCocoonOam_CeilingVineBroken",
    "ImagoCocoonOam_DecorativeVinesLeft",
    "ImagoCocoonOam_DecorativeVinesRight",
    "ImagoCocoonVineOam_LeftMiddle",
    "ImagoCocoonVineOam_RightMiddle",
    "ImagoCocoonVineOam_LeftRight",
    "ImagoCocoonVineOam_RightRight",
    "ImagoCocoonVineOam_LeftLeft",
    "ImagoCocoonVineOam_RightLeft",
    "ImagoCocoonOam_Idle",
    "ImagoCocoonOam_2e0bb0",
    "ImagoCocoonOam_Static",
    "ImagoCocoonSporeOam_Spawning",
    "ImagoCocoonSporeOam_Nest",
    "ImagoCocoonSporeOam_Moving",
    "ImagoCocoonSporeOam_Exploding",
    "WingedRipperOam_Moving",
    "WingedRipperOam_TurningAround",
    "DefeatedImagoCocoonOam",
};
static const char *const imago_cocoon_sprites[MZM_IMAGO_COCOON_ANIMS] = {
    "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON",
    "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON",
    "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON",
    "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON",
    "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON", "IMAGO_COCOON",
    "IMAGO_COCOON",
};
const char *const mzm_imago_multi_names[MZM_IMAGO_MULTI_COUNT] = {
    "sImagoLarvaMultiSpriteData_Attacking",
    "sImagoLarvaMultiSpriteData_Idle",
    "sImagoLarvaMultiSpriteData_Retreating",
    "sImagoLarvaMultiSpriteData_Dying",
    "sImagoLarvaMultiSpriteData_Warning",
    "sImagoLarvaMultiSpriteData_TakingDamage",
};
const char *const mzm_imago_cocoon_multi_names[MZM_IMAGO_COCOON_MULTI_COUNT] = {
    "sImagoCocoonMultiSpriteData_Idle",
    "sImagoCocoonMultiSpriteData_Dying",
};
const char *const mzm_mecha_ridley_multi_names[MZM_MECHA_RIDLEY_MULTI_COUNT] = {
    "sMechaRidleyMultiSpriteData_CrawlingForwardLow",
    "sMechaRidleyMultiSpriteData_StandingLow",
    "sMechaRidleyMultiSpriteData_StandingMiddle",
    "sMechaRidleyMultiSpriteData_StandingHigh",
    "sMechaRidleyMultiSpriteData_LowToMiddle",
    "sMechaRidleyMultiSpriteData_MiddleToHigh",
    "sMechaRidleyMultiSpriteData_HighToMiddle",
    "sMechaRidleyMultiSpriteData_MiddleToLow",
    "sMechaRidleyMultiSpriteData_LowToHigh",
    "sMechaRidleyMultiSpriteData_HighToLow",
    "sMechaRidleyMultiSpriteData_CrawlingBackwardsLow",
    "sMechaRidleyMultiSpriteData_OpeningMouthHigh",
    "sMechaRidleyMultiSpriteData_SpittingFireballsHigh",
    "sMechaRidleyMultiSpriteData_ClosingMouthHigh",
    "sMechaRidleyMultiSpriteData_DyingStandingLow",
    "sMechaRidleyMultiSpriteData_DyingStandingMiddle",
    "sMechaRidleyMultiSpriteData_DyingStandingHigh",
    "sMechaRidleyMultiSpriteData_Dying",
};
const char *const mzm_mecha_ridley_anim_names[MZM_MECHA_RIDLEY_ANIMS] = {
    "MechaRidleyPartOam_Tail", "MechaRidleyPartOam_Ring",
    "MechaRidleyPartOam_Cover", "MechaRidleyPartOam_Torso", "MechaRidleyOam_Idle",
    "MechaRidleyPartOam_HeadIdle", "MechaRidleyPartOam_HeadOpeningMouth",
    "MechaRidleyPartOam_HeadSpittingFireballs", "MechaRidleyPartOam_HeadClosingMouth",
    "MechaRidleyPartOam_EyeIdle", "MechaRidleyPartOam_EyeGlowing",
    "MechaRidleyPartOam_EyeShootingLaserForward",
    "MechaRidleyPartOam_EyeShootingLaserSlightlyDown",
    "MechaRidleyPartOam_EyeShootingLaserDown",
    "MechaRidleyPartOam_EyeShootingLaserSlightlyUp",
    "MechaRidleyPartOam_EyeShootingLaserUp", "MechaRidleyPartOam_EyeInactive",
    "MechaRidleyPartOam_LeftArmCrawlingForward",
    "MechaRidleyPartOam_RightArmCrawlingForward",
    "MechaRidleyPartOam_LeftArmHoldingUp", "MechaRidleyPartOam_RightArmHoldingUp",
    "MechaRidleyPartOam_LeftArmHeldUp", "MechaRidleyPartOam_RightArmHeldUp",
    "MechaRidleyPartOam_LeftArmLayingDown", "MechaRidleyPartOam_RightArmLayingDown",
    "MechaRidleyPartOam_LeftArmTrembling", "MechaRidleyPartOam_RightArmTrembling",
    "MechaRidleyPartOam_LeftArmIdle", "MechaRidleyPartOam_RightArmIdle",
    "MechaRidleyPartOam_LeftArmSwinging", "MechaRidleyPartOam_NeckLow",
    "MechaRidleyPartOam_NeckMiddle", "MechaRidleyPartOam_NeckHigh",
    "MechaRidleyPartOam_NeckRotateLow", "MechaRidleyPartOam_NeckRotateMiddle",
    "MechaRidleyPartOam_NeckRotateHigh", "MechaRidleyPartOam_NeckLowToMiddle",
    "MechaRidleyPartOam_NeckMiddleToHigh", "MechaRidleyPartOam_NeckHighToMiddle",
    "MechaRidleyPartOam_NeckMiddleToLow", "MechaRidleyPartOam_NeckLowToHigh",
    "MechaRidleyPartOam_NeckHighToLow", "MechaRidleyPartOam_NeckLow_2",
    "MechaRidleyPartOam_NeckMiddle_2", "MechaRidleyPartOam_NeckHigh_2",
    "MechaRidleyPartOam_MissileLauncherClosed",
    "MechaRidleyPartOam_MissileLauncherOpening",
    "MechaRidleyPartOam_MissileLauncherOpened",
    "MechaRidleyPartOam_MissileLauncherClosing", "MechaRidleyMissileOam",
    "MechaRidleyFireballOam", "MechaRidleyLaserOam_Forward",
    "MechaRidleyLaserOam_SlightlyDown", "MechaRidleyLaserOam_Down",
    "MechaRidleyLaserOam_SlightlyUp", "MechaRidleyLaserOam_Up",
    "MechaRidleyPartOam_LeftArmDying", "MechaRidleyPartOam_RightArmDying",
    "MechaRidleyPartOam_NeckDying", "MechaRidleyPartOam_LeftArmCrawlingBackwards",
    "MechaRidleyPartOam_RightArmCrawlingBackwards",
    "MechaRidleyPartOam_LeftArmSwingingAtGround",
    "MechaRidleyPartOam_LeftArmSwingingAtCloseGround",
    "MechaRidleyPartOam_EyeDying",
};
const char *const mzm_hive_anim_names[MZM_HIVE_ANIMS] = {
    "MellowOam_Idle", "MellowOam_SamusDetected", "HiveOam_Idle", "HiveRootsOam_Idle",
    "HiveOam_Phase2", "HiveOam_Phase3", "HiveOam_Dying",
};
static const char *const hive_sprites[MZM_HIVE_ANIMS] = {
    "HIVE", "HIVE", "HIVE", "HIVE", "HIVE", "HIVE", "HIVE",
};
const char *const mzm_baristute_anim_names[MZM_BARISTUTE_ANIMS] = {
    "BaristuteOam_Idle", "BaristuteOam_Warning", "BaristuteOam_Jumping", "BaristuteOam_Landing",
    "BaristuteOam_WalkingSlow", "BaristuteOam_WalkingFast", "BaristuteOam_WalkingVeryFast",
    "BaristuteOam_WalkingExtremelyFast",
};
static const char *const baristute_sprites[MZM_BARISTUTE_ANIMS] = {
    "BARISTUTE", "BARISTUTE", "BARISTUTE", "BARISTUTE",
    "BARISTUTE", "BARISTUTE", "BARISTUTE", "BARISTUTE",
};
const char *const mzm_tangle_vine_anim_names[MZM_TANGLE_VINE_ANIMS] = {
    "TangleVineTallOam_OnGround", "TangleVineMediumOam_OnGround", "TangleVineShortOam_OnGround",
    "TangleVineCurvedOam_OnGround", "TangleVineTallOam_OnCeiling", "TangleVineMediumOam_OnCeiling",
    "TangleVineShortOam_OnCeiling", "TangleVineCurvedOam_OnCeiling", "TangleVineGerutaPartOam_Grip",
    "TangleVineGerutaOam_Root", "TangleVineGerutaOam_Full", "TangleVineRedGerutaOam",
    "TangleVineGerutaPartOam_Geruta", "TangleVineLarvaRightOam", "TangleVineLarvaLeftOam",
};
static const char *const tangle_vine_sprites[MZM_TANGLE_VINE_ANIMS] = {
    "TANGLE_VINE_TALL", "TANGLE_VINE_TALL", "TANGLE_VINE_TALL", "TANGLE_VINE_TALL",
    "TANGLE_VINE_TALL", "TANGLE_VINE_TALL", "TANGLE_VINE_TALL", "TANGLE_VINE_TALL",
    "TANGLE_VINE_TALL", "TANGLE_VINE_TALL", "TANGLE_VINE_TALL", "TANGLE_VINE_TALL",
    "TANGLE_VINE_TALL", "TANGLE_VINE_TALL", "TANGLE_VINE_TALL",
};
const char *const mzm_ruins_test_anim_names[MZM_RUINS_TEST_ANIMS] = {
    "RuinsTestSamusReflectionOam",
    "RuinsTestGhostOutlineOam_Spawning",
    "RuinsTestGhostOam_NotMoving",
    "RuinsTestGhostOam_Moving",
    "RuinsTestGhostOam_SymbolPlaced",
    "RuinsTestSymbolOam_FourSymbols",
    "RuinsTestSymbolOam_Merging",
    "RuinsTestSymbolOam_Disappearing",
    "RuinsTestOam_TurningIntoReflection",
    "RuinsTestReflectionCoverOam",
    "RuinsTestOam_Symbol",
    "RuinsTestShootableSymbolOam_Spawning",
    "RuinsTestShootableSymbolOam_Spawned",
    "RuinsTestShootableSymbolOam_Despawning",
    "RuinsTestShootableSymbolOam_TurningIntoSymbol",
    "RuinsTestGhostOam_SymbolShot",
    "RuinsTestGhostOam_SymbolPlacing",
    "RuinsTestOam_Despawn",
    "RuinsTestOam_Unused",
    "RuinsTestOam_Spawning",
    "RuinsTestLightningOam_InAir",
    "RuinsTestLightningOam_GoingOnGround",
    "RuinsTestLightningOam_OnGroundHorizontal",
    "RuinsTestLightningOam_OnGroundVertical",
    "RuinsTestGhostOutlineOam_ShootingGroundLightning",
};
static const char *const ruins_test_sprites[MZM_RUINS_TEST_ANIMS] = {
    "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST", "RUINS_TEST",
};
const char *const mzm_chozodia_scripted_anim_names[MZM_CHOZODIA_SCRIPTED_ANIMS] = {
    "FallingChozoPillarOam_Falling", "FakePowerBombOam_Idle", "SpacePirateCarryingPowerBombOam",
};
static const char *const chozodia_scripted_sprites[MZM_CHOZODIA_SCRIPTED_ANIMS] = {
    "FALLING_CHOZO_PILLAR", "FAKE_POWER_BOMB", "SPACE_PIRATE",
};
const char *const mzm_elevator_statue_anim_names[MZM_ELEVATOR_STATUE_ANIMS] = {
    "KraidElevatorStatueOam_Idle", "KraidElevatorStatueOam_Falling", "ElevatorStatueDebrisOam_Kraid", "KraidElevatorStatueOam_Fallen", "RidleyElevatorStatueOam_Idle", "RidleyElevatorStatueOam_Falling", "ElevatorStatueDebrisOam_Ridley", "RidleyElevatorStatueOam_Fallen", "ElevatorStatueDebrisOam_Dust",
};
static const char *const elevator_statue_sprites[MZM_ELEVATOR_STATUE_ANIMS] = {
    "KRAID_ELEVATOR_STATUE", "KRAID_ELEVATOR_STATUE", "KRAID_ELEVATOR_STATUE", "KRAID_ELEVATOR_STATUE", "KRAID_ELEVATOR_STATUE", "KRAID_ELEVATOR_STATUE", "KRAID_ELEVATOR_STATUE", "KRAID_ELEVATOR_STATUE", "KRAID_ELEVATOR_STATUE",
};
const char *const mzm_escape_ship_anim_names[MZM_ESCAPE_SHIP_ANIMS] = {
    "EscapeShipOam_Idle", "EscapeShipOam_Opening", "EscapeShipOam_Closing", "EscapeShipOam_Flying",
    "EscapeShipPartOam_Top", "EscapeShipPartOam_Flames", "EscapeShipPartOam_Tail",
    "EscapeShipPartOam_TailMoving", "EscapeGateOam_Closed", "EscapeGateOam_Opening", "EscapeGateOam_Opened",
};
static const char *const escape_ship_sprites[MZM_ESCAPE_SHIP_ANIMS] = {
    "ESCAPE_SHIP", "ESCAPE_SHIP", "ESCAPE_SHIP", "ESCAPE_SHIP", "ESCAPE_SHIP", "ESCAPE_SHIP",
    "ESCAPE_SHIP", "ESCAPE_SHIP", "ESCAPE_GATE1", "ESCAPE_GATE1", "ESCAPE_GATE1",
};
const char *const mzm_imago_anim_names[MZM_IMAGO_ANIMS] = {
    "ImagoPartOam_BodyIdle",
    "ImagoPartOam_BodyGrowling",
    "ImagoOam_BrokenStinger",
    "ImagoPartOam_LeftWingIdle",
    "ImagoPartOam_LeftWingShootingNeedles",
    "ImagoPartOam_LeftWingDying",
    "ImagoPartOam_RightWingIdle",
    "ImagoPartOam_RightWingShootingNeedles",
    "ImagoPartOam_RightWingDying",
    "ImagoPartOam_Core",
    "ImagoOam_Flying",
    "ImagoOam_ShootingNeedles",
    "ImagoOam_RechargingNeedles",
    "ImagoDamagedStingerOam",
    "ImagoNeedleOam",
    "ImagoDamagedStingerOam_Unused",
    "ImagoEggOam_Breaking",
    "ImagoEggOam_Standing",
};
static const char *const imago_sprites[MZM_IMAGO_ANIMS] = {
    "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO", "IMAGO",
};
const char *const mzm_imago_adult_multi_names[MZM_IMAGO_ADULT_MULTI_COUNT] = {
    "sImagoMultiSpriteData_Idle", "sImagoMultiSpriteData_ShootingNeedles", "sImagoMultiSpriteData_RechargingNeedles", "sImagoMultiSpriteData_Dying",
};
static const char *const mecha_ridley_sprites[MZM_MECHA_RIDLEY_ANIMS] = {
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY", "MECHA_RIDLEY", "MECHA_RIDLEY",
    "MECHA_RIDLEY",
};
const char *const mzm_mother_brain_anim_names[MZM_MOTHER_BRAIN_ANIMS] = {
    "MotherBrainOam_Idle", "MotherBrainOam_ChargingBeam", "MotherBrainPartOam_EyeClosed",
    "MotherBrainPartOam_2fa934", "MotherBrainPartOam_EyeOpening", "MotherBrainPartOam_2fa984",
    "MotherBrainPartOam_EyeDying", "MotherBrainPartOam_Bottom", "MotherBrainPartOam_BeamSpawning",
    "MotherBrainBeamOam_Moving", "MotherBrainPartOam_EyeClosing", "MotherBrainGlassBreakingOam_Breaking",
    "MotherBrainBlockOam",
};

const char *const mzm_deorem_anim_names[MZM_DEOREM_ANIMS] = {
    "DeoremSegmentOam_Middle", "DeoremSegmentOam_Junction", "DeoremSegmentOam_Tail",
    "DeoremThornOam_Idle", "DeoremEyeOam_Idle", "DeoremEyeOam_Pulsing",
    "DeoremOam_ClosedSlow", "DeoremOam_Opening", "DeoremOam_OpenedSlow",
    "DeoremOam_Closing", "DeoremOam_Warning", "DeoremOam_GoingDown",
    "DeoremOam_OpenedFast", "DeoremOam_ClosedFast", "DeoremOam_Dying",
};
static const char *const deorem_sprites[MZM_DEOREM_ANIMS] = {
    "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION",
    "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION",
    "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION",
    "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION",
    "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION", "DEOREM_FIRST_LOCATION",
};
const char *const mzm_acid_worm_anim_names[MZM_ACID_WORM_ANIMS] = {
    "AcidWormOam_Idle", "AcidWormOam_Moving", "AcidWormOam_Spitting",
    "AcidWormOam_MouthClosed", "AcidWormPartOam_AroundMouth",
    "AcidWormPartOam_WeakPoint", "AcidWormPartOam_Unused",
    "AcidWormPartOam_BelowWeakPoint", "AcidWormPartOam_AboveSegments",
    "AcidWormPartOam_Segment", "AcidWormPartOam_Unused2",
    "AcidWormSpitOam_Moving", "AcidWormSpitOam_Exploding",
    "AcidWormOam_Warning", "AcidWormOam_SpawnOnTop",
};
static const char *const acid_worm_sprites[MZM_ACID_WORM_ANIMS] = {
    "ACID_WORM", "ACID_WORM", "ACID_WORM", "ACID_WORM", "ACID_WORM",
    "ACID_WORM", "ACID_WORM", "ACID_WORM", "ACID_WORM", "ACID_WORM",
    "ACID_WORM", "ACID_WORM", "ACID_WORM", "ACID_WORM", "ACID_WORM",
};
const char *const mzm_gadora_anim_names[MZM_GADORA_ANIMS] = {
    "GadoraOam_EyeClosed", "GadoraOam_Warning", "GadoraOam_OpeningEye",
    "GadoraBeamOam_Moving", "GadoraOam_EyeMoving", "GadoraOam_ClosingEye",
    "GadoraOam_Death", "GadoraOam_EyeOpened", "GadoraOam_BeforeClosing",
};
static const char *const gadora_sprites[MZM_GADORA_ANIMS] = {
    "GADORA_KRAID", "GADORA_KRAID", "GADORA_KRAID", "GADORA_KRAID",
    "GADORA_KRAID", "GADORA_KRAID", "GADORA_KRAID", "GADORA_KRAID",
    "GADORA_KRAID",
};
static const char *const mother_brain_sprites[MZM_MOTHER_BRAIN_ANIMS] = {
    "MOTHER_BRAIN", "MOTHER_BRAIN", "MOTHER_BRAIN", "MOTHER_BRAIN", "MOTHER_BRAIN", "MOTHER_BRAIN",
    "MOTHER_BRAIN", "MOTHER_BRAIN", "MOTHER_BRAIN", "MOTHER_BRAIN", "MOTHER_BRAIN", "MOTHER_BRAIN",
    "CANNON",
};
const char *const mzm_metroid_anim_names[MZM_METROID_ANIMS] = {
    "MetroidOam_Spawning", "MetroidOam_Moving", "MetroidOam_SamusGrabbed", "MetroidShellOam_Idle",
};
static const char *const metroid_sprites[MZM_METROID_ANIMS] = {
    "METROID", "METROID", "METROID", "METROID",
};
const char *const mzm_tourian_anim_names[MZM_TOURIAN_ANIMS] = {
    "ZebetiteOam_Phase1",
    "ZebetiteOam_Phase2",
    "ZebetiteOam_Phase4",
    "ZebetiteOam_Phase6",
    "CannonOam_AimingLeft",
    "CannonOam_ShootingLeft",
    "CannonOam_LeftToDownLeftTransition",
    "CannonOam_AimingDownLeft",
    "CannonOam_ShootingDownLeft",
    "CannonOam_DownLeftToDownTransition",
    "CannonOam_AimingDown",
    "CannonOam_ShootingDown",
    "CannonOam_DownToDownRightTransition",
    "CannonOam_AimingDownRight",
    "CannonOam_ShootingDownRight",
    "CannonOam_DownRightToRightTransition",
    "CannonOam_AimingRight",
    "CannonOam_ShootingRight",
    "CannonBulletOam_Left",
    "CannonBulletOam_DownLeft",
    "CannonBulletOam_Down",
    "CannonBulletOam_DownRight",
    "CannonBulletOam_Right",
};
static const char *const tourian_sprites[MZM_TOURIAN_ANIMS] = {
    "ZEBETITE_ONE_AND_THREE",
    "ZEBETITE_ONE_AND_THREE",
    "ZEBETITE_ONE_AND_THREE",
    "ZEBETITE_ONE_AND_THREE",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
    "CANNON",
};
const char *const mzm_skultera_anim_names[MZM_SKULTERA_ANIMS] = {
    "SkulteraOam_Idle", "SkulteraOam_TurningAround", "SkulteraOam_ChasingSamus",
};
static const char *const skultera_sprites[MZM_SKULTERA_ANIMS] = {"SKULTERA", "SKULTERA", "SKULTERA"};
const char *const mzm_squeept_anim_names[MZM_SQUEEPT_ANIMS] = {
    "SqueeptOam_GoingUp", "SqueeptOam_TurningAround", "SqueeptOam_GoingDown",
};
static const char *const squeept_sprites[MZM_SQUEEPT_ANIMS] = {"SQUEEPT", "SQUEEPT", "SQUEEPT"};
static const char *const multiviola_sprites[MZM_MULTIVIOLA_ANIMS] = {"MULTIVIOLA"};
const char *const mzm_rinka_anim_names[MZM_RINKA_ANIMS] = {
    "RinkaOrangeOam_Spawning", "RinkaOrangeOam_Moving", "RinkaGreenOam_Spawning",
    "RinkaGreenOam_Moving", "RinkaMotherBrainOam_Spawning", "RinkaMotherBrainOam_Moving",
};
static const char *const rinka_sprites[MZM_RINKA_ANIMS] = {
    "RINKA_ORANGE", "RINKA_ORANGE", "RINKA_GREEN", "RINKA_GREEN", "CANNON", "CANNON",
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
    "POWER_BOMB_DROP", "MULTIPLE_LARGE_ENERGY", "MULTIPLE_LARGE_ENERGY",
    "MULTIPLE_LARGE_ENERGY",
};

/* Every Chozo statue table has the same durations in each statue's graphics. */
static const char *const chozo_statue_sprites[MZM_CHOZO_ANIMS] = {
    "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG",
    "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG",
    "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG",
    "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG", "CHOZO_STATUE_LONG",
    "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM",
    "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM",
    "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM",
    "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM",
    "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM", "CHOZO_STATUE_PLASMA_BEAM",
    "CHOZO_STATUE_PLASMA_BEAM",
};

const char *const mzm_boss_statue_anim_names[MZM_BOSS_STATUE_ANIMS] = {
    "KraidStatueOam_Idle", "KraidStatueOam_Activating", "KraidStatueOam_Activated",
    "KraidStatueOam_Opening", "KraidStatueOam_Opened", "RidleyStatueOam_Idle",
    "RidleyStatueOam_Activating", "RidleyStatueOam_Activated",
    "RidleyStatueOam_Opening", "RidleyStatueOam_Opened",
};
static const char *const boss_statue_sprites[MZM_BOSS_STATUE_ANIMS] = {
    "KRAID_STATUE", "KRAID_STATUE", "KRAID_STATUE", "KRAID_STATUE", "KRAID_STATUE",
    "RIDLEY_STATUE", "RIDLEY_STATUE", "RIDLEY_STATUE", "RIDLEY_STATUE", "RIDLEY_STATUE",
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
    [MZM_FAMILY_GAMET] = {MZM_PIPE_BUG_ANIMS, mzm_gamet_anim_names, gamet_sprites},
    [MZM_FAMILY_ITEM_SPRITE] = {MZM_ITEM_SPRITE_ANIMS, mzm_item_sprite_anim_names, item_sprites},
    [MZM_FAMILY_SKULTERA] = {MZM_SKULTERA_ANIMS, mzm_skultera_anim_names, skultera_sprites},
    [MZM_FAMILY_DRAGON] = {MZM_DRAGON_ANIMS, mzm_dragon_anim_names, dragon_sprites},
    [MZM_FAMILY_GADGET] = {MZM_GADGET_ANIMS, mzm_gadget_anim_names, gadget_sprites},
    [MZM_FAMILY_PARASITE] = {MZM_PARASITE_ANIMS, mzm_parasite_anim_names, parasite_sprites},
    [MZM_FAMILY_ATOMIC] = {MZM_ATOMIC_ANIMS, mzm_atomic_anim_names, atomic_sprites},
    [MZM_FAMILY_POLYP] = {MZM_POLYP_ANIMS, mzm_polyp_anim_names, polyp_sprites},
    [MZM_FAMILY_TOURIAN] = {MZM_TOURIAN_ANIMS, mzm_tourian_anim_names, tourian_sprites},
    [MZM_FAMILY_WORKER_ROBOT] = {MZM_WORKER_ROBOT_ANIMS, mzm_worker_robot_anim_names,
                                 worker_robot_sprites},
    [MZM_FAMILY_METROID] = {MZM_METROID_ANIMS, mzm_metroid_anim_names, metroid_sprites},
    [MZM_FAMILY_KRAID] = {MZM_KRAID_ANIMS, mzm_kraid_anim_names, kraid_sprites},
    [MZM_FAMILY_RIDLEY] = {MZM_RIDLEY_ANIMS, mzm_ridley_anim_names, ridley_sprites},
    [MZM_FAMILY_MOTHER_BRAIN] = {MZM_MOTHER_BRAIN_ANIMS, mzm_mother_brain_anim_names, mother_brain_sprites},
    [MZM_FAMILY_REO] = {MZM_REO_ANIMS, mzm_reo_anim_names, reo_sprites},
    [MZM_FAMILY_MELLA] = {MZM_MELLA_ANIMS, mzm_mella_anim_names, mella_sprites},
    [MZM_FAMILY_HOLTZ] = {MZM_HOLTZ_ANIMS, mzm_holtz_anim_names, holtz_sprites},
    [MZM_FAMILY_GERUTA] = {MZM_GERUTA_ANIMS, mzm_geruta_anim_names, geruta_sprites},
    [MZM_FAMILY_RIPPER2] = {MZM_RIPPER2_ANIMS, mzm_ripper2_anim_names, ripper2_sprites},
    [MZM_FAMILY_SQUEEPT] = {MZM_SQUEEPT_ANIMS, mzm_squeept_anim_names, squeept_sprites},
    [MZM_FAMILY_MULTIVIOLA] = {MZM_MULTIVIOLA_ANIMS, mzm_multiviola_anim_names,
                               multiviola_sprites},
    [MZM_FAMILY_RINKA] = {MZM_RINKA_ANIMS, mzm_rinka_anim_names, rinka_sprites},
    [MZM_FAMILY_SPACE_PIRATE] = {MZM_PIRATE_ANIMS, mzm_space_pirate_anim_names,
                                 space_pirate_sprites},
    [MZM_FAMILY_CHOZODIA_SECURITY] = {MZM_SECURITY_ANIMS, mzm_chozodia_security_anim_names,
                                      chozodia_security_sprites},
    [MZM_FAMILY_CHOZO_STATUE] = {MZM_CHOZO_ANIMS, mzm_chozo_statue_anim_names,
                                 chozo_statue_sprites},
    [MZM_FAMILY_BOSS_STATUE] = {MZM_BOSS_STATUE_ANIMS, mzm_boss_statue_anim_names,
                                boss_statue_sprites},
    [MZM_FAMILY_DEOREM] = {MZM_DEOREM_ANIMS, mzm_deorem_anim_names, deorem_sprites},
    [MZM_FAMILY_ACID_WORM] = {MZM_ACID_WORM_ANIMS, mzm_acid_worm_anim_names,
                              acid_worm_sprites},
    [MZM_FAMILY_GADORA] = {MZM_GADORA_ANIMS, mzm_gadora_anim_names, gadora_sprites},
    [MZM_FAMILY_IMAGO_LARVA] = {MZM_IMAGO_LARVA_ANIMS, mzm_imago_larva_anim_names,
                                 imago_larva_sprites},
    [MZM_FAMILY_IMAGO_LARVA_RIGHT_SIDE] = {
        MZM_IMAGO_LARVA_RIGHT_SIDE_ANIMS, mzm_imago_larva_right_side_anim_names,
        imago_larva_right_side_sprites},
    [MZM_FAMILY_IMAGO_COCOON] = {MZM_IMAGO_COCOON_ANIMS, mzm_imago_cocoon_anim_names,
                                 imago_cocoon_sprites},
    [MZM_FAMILY_MECHA_RIDLEY] = {MZM_MECHA_RIDLEY_ANIMS, mzm_mecha_ridley_anim_names,
                                 mecha_ridley_sprites},
    [MZM_FAMILY_HIVE] = {MZM_HIVE_ANIMS, mzm_hive_anim_names, hive_sprites},
    [MZM_FAMILY_BARISTUTE] = {MZM_BARISTUTE_ANIMS, mzm_baristute_anim_names, baristute_sprites},
    [MZM_FAMILY_TANGLE_VINE] = {MZM_TANGLE_VINE_ANIMS, mzm_tangle_vine_anim_names, tangle_vine_sprites},
    [MZM_FAMILY_RUINS_TEST] = {MZM_RUINS_TEST_ANIMS, mzm_ruins_test_anim_names, ruins_test_sprites},
    [MZM_FAMILY_CHOZODIA_SCRIPTED] = {MZM_CHOZODIA_SCRIPTED_ANIMS, mzm_chozodia_scripted_anim_names,
                                      chozodia_scripted_sprites},
    [MZM_FAMILY_ELEVATOR_STATUE] = {MZM_ELEVATOR_STATUE_ANIMS, mzm_elevator_statue_anim_names,
                                    elevator_statue_sprites},
    [MZM_FAMILY_IMAGO] = {MZM_IMAGO_ANIMS, mzm_imago_anim_names, imago_sprites},
    [MZM_FAMILY_ESCAPE_SHIP] = {MZM_ESCAPE_SHIP_ANIMS, mzm_escape_ship_anim_names, escape_ship_sprites},
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

/* SpriteUtilShouldFall: on screen during any screen shake. */
static bool should_fall(const MzmSprite *s) {
    return s->status & MZM_SPRITE_STATUS_ONSCREEN &&
           (mzm_screen_shake.y.timer != 0 || mzm_screen_shake.x.timer != 0);
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
    /* SpriteUtilShouldFall: a crawler on a ceiling or wall drops during a
     * screen shake, and none crawls. */
    if (should_fall(s)) {
        if (s->status & (MZM_SPRITE_STATUS_Y_FLIP | MZM_SPRITE_STATUS_FACING_DOWN))
            s->pose = ZOOMER_POSE_FALLING_INIT;
        return;
    }
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
    const uint16_t *probs = c->s->secondary ? c->tables->secondary_drops[c->s->sprite_id]
                                            : c->tables->drops[c->s->sprite_id];
    bool full_life = e->energy == e->max_energy;
    uint16_t rng = (uint16_t)(c->rng * 256);
    rng = (uint16_t)(c->frame8 + c->f->frame16 + rng);
    rng %= SPRITE_DROP_MAX_PROB;
    if (rng == 0) rng = 1;          /* not REGION_US_BETA */
    /* Ammunition drops, power bombs first. */
    static const uint8_t ammo_drops[3] = {
        MZM_PSPRITE_POWER_BOMB_DROP, MZM_PSPRITE_SUPER_MISSILE_DROP, MZM_PSPRITE_MISSILE_DROP,
    };
    int current[3] = {e->power_bombs, e->super_missiles, e->missiles};
    int maximum[3] = {e->max_power_bombs, e->max_super_missiles, e->max_missiles};
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
    ai_sprite_death_full(c, type, y, x, true, effect);
}

void ai_sprite_death_full(Ctx *c, int type, uint16_t y, uint16_t x, bool play_sound, uint8_t effect) {
    MzmSprite *s = c->s;
    if (s->pose == SPRITE_POSE_SCREW_ATTACK_DESTROYED) {
        ai_particle_set(c, y, x, MZM_PE_SCREW_ATTACK_DESTROYED);
        random_debris_3(c, y, x);
        mzm_sfx_play(MZM_SOUND_SCREW_ATTACK_DESTROYED);
    } else if (s->pose == SPRITE_POSE_PSEUDO_SCREW_DESTROYED) {
        ai_particle_set(c, y, x, MZM_PE_PSEUDO_SCREW_DESTROYED);
        random_debris_3(c, y, x);
        mzm_sfx_play(MZM_SOUND_PSEUDO_SCREW_DESTROYED);
    } else if (s->pose == SPRITE_POSE_SHINESPARK_DESTROYED) {
        ai_particle_set(c, y, x, MZM_PE_SHINESPARK_DESTROYED);
        random_debris_3(c, y, x);
        mzm_sfx_play(MZM_SOUND_SHINESPARK_DESTROYED);
    } else if (s->pose == SPRITE_POSE_SPEEDBOOSTER_DESTROYED) {
        ai_particle_set(c, y, x, MZM_PE_SPEEDBOOSTER_DESTROYED);
        random_debris_3(c, y, x);
        mzm_sfx_play(MZM_SOUND_SPEEDBOOSTER_DESTROYED);
    } else {
        ai_particle_set(c, y, x, effect);
        switch (play_sound ? effect : 0) {
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

/* Zoomer: the AI (its damaged sound in mzm_sprite_update). */
static void zoomer(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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

/* Ripper2: the Ripper's AI with its own tables, a wider hitbox, two
 * pixels a frame and a turning sound. */
static void ripper2(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    /* X_COLLISION_CHECK_OFFSET */
    const uint16_t offset = THREE_QUARTER_BLOCK_SIZE - PIXEL_SIZE;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* Ripper2Init */
        s->hitbox_top = -(HALF_BLOCK_SIZE + PIXEL_SIZE);
        s->hitbox_bottom = EIGHTH_BLOCK_SIZE;
        s->hitbox_left = -(HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->draw_top = BLOCK_SIZE / PIXEL_SIZE;
        s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->draw_horizontal = (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        ai_set_anim(s, MZM_RIPPER2_MOVING);
        s->samus_collision = SSC_HURTS_SAMUS;
        s->health = c->tables->health[s->sprite_id];
        s->y -= EIGHTH_BLOCK_SIZE;
        random_x_flip(c);
        s->pose = RIPPER_POSE_MOVING_INIT;
        break;
    case RIPPER_POSE_MOVING_INIT:
        s->pose = RIPPER_POSE_MOVING;
        ai_set_anim(s, MZM_RIPPER2_MOVING);
        /* fallthrough */
    case RIPPER_POSE_MOVING: {
        /* Ripper2Move */
        uint16_t y = (uint16_t)(s->y - QUARTER_BLOCK_SIZE);
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            if (ai_collision_at(c, y, (uint16_t)(s->x + offset)) != MZM_COLLISION_SOLID)
                s->x += EIGHTH_BLOCK_SIZE;
            else
                s->pose = RIPPER_POSE_TURNING_AROUND_INIT;
        } else {
            if (ai_collision_at(c, y, (uint16_t)(s->x - offset)) == MZM_COLLISION_SOLID)
                s->pose = RIPPER_POSE_TURNING_AROUND_INIT;
            else
                s->x -= EIGHTH_BLOCK_SIZE;
        }
        break;
    }
    case RIPPER_POSE_TURNING_AROUND_INIT:
        s->pose = RIPPER_POSE_TURNING_AROUND_FIRST_PART;
        ai_set_anim(s, MZM_RIPPER2_TURNING_AROUND);
        ai_sound_onscreen_once(s, MZM_SOUND_RIPPER2_TURNING_AROUND);
        /* fallthrough */
    case RIPPER_POSE_TURNING_AROUND_FIRST_PART:
        if (ai_animation_ending(c, 1)) {
            s->status ^= MZM_SPRITE_STATUS_X_FLIP;
            s->pose = RIPPER_POSE_TURNING_AROUND_SECOND_PART;
            ai_set_anim(s, MZM_RIPPER2_TURNING_AROUND_PART2);
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
int ai_spawn_primary(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot, uint16_t y,
                     uint16_t x, uint16_t status) {
    for (int slot = 0; c->f->sprites && slot < c->f->sprite_count; ++slot) {
        MzmSprite *n = &c->f->sprites[slot];
        if (n->status & MZM_SPRITE_STATUS_EXISTS) continue;
        memset(n, 0, sizeof *n);
        n->status = status | MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_ONSCREEN |
                    MZM_SPRITE_STATUS_NOT_DRAWN;
        n->gfx_slot = gfx_slot;
        n->sprite_id = sprite_id;
        n->y = y;
        n->x = x;
        n->room_slot = part;
        n->draw_order = 4;
        n->pose = SPRITE_POSE_UNINITIALIZED;
        n->ignore_samus_timer = 1;
        n->ram_slot = (uint8_t)slot;
        return slot;
    }
    return -1;
}

int ai_spawn_secondary(Ctx *c, uint8_t sprite_id, uint8_t part, uint8_t gfx_slot,
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
        n->draw_order = 4;
        n->owner = c->s->secondary ? c->s->owner : c->s->sprite_id;
        return i;
    }
    return -1;
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
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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
 * toward Samus (SpriteUtilMakeSpriteFaceSamusXFlip). The damaged sound is
 * not ported. */
static void waver(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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

bool ai_refill(int *current, int max, int divisor, int cap) {
    if (*current >= max) return false;
    int step = (max - *current) / divisor;
    if (step == 0) step = 1;
    else if (step >= cap) step = cap;
    *current += step;
    if (*current > max) *current = max;
    return true;
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

static void boss_statue_apply_caa(const Ctx *c, uint16_t y, uint16_t x, uint8_t caa) {
    const MzmSpriteHooks *h = c->f->hooks;
    if (h && h->apply_caa) h->apply_caa(h->context, y, x, caa);
}

/* BossStatusSetWallBehindSamusCollision: the temporary six-block wall at
 * the right edge of the statue room. */
static void boss_statue_wall(Ctx *c, uint8_t caa) {
    uint16_t y = (uint16_t)(c->s->y - HALF_BLOCK_SIZE);
    uint16_t x = (uint16_t)(c->s->x + HALF_BLOCK_SIZE + BLOCK_SIZE * 8);
    for (unsigned row = 0; row < 6; ++row)
        boss_statue_apply_caa(c, (uint16_t)(y - BLOCK_SIZE * row), x, caa);
}

/* KraidStatueInsideChangeCcaa / RidleyStatueInsideChangeCcaa. */
static void boss_statue_inside(Ctx *c, bool kraid, uint8_t caa) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
    uint16_t x = (uint16_t)(s->x + (kraid ? HALF_BLOCK_SIZE : -HALF_BLOCK_SIZE));
    if (kraid) {
        for (unsigned column = 2; column <= 5; ++column)
            boss_statue_apply_caa(c, (uint16_t)(y - BLOCK_SIZE),
                                  (uint16_t)(x + BLOCK_SIZE * column), caa);
        for (unsigned column = 2; column <= 4; ++column)
            boss_statue_apply_caa(c, (uint16_t)(y - BLOCK_SIZE * 2),
                                  (uint16_t)(x + BLOCK_SIZE * column), caa);
        boss_statue_apply_caa(c, (uint16_t)(y - BLOCK_SIZE * 3),
                              (uint16_t)(x + BLOCK_SIZE * 3), caa);
    } else {
        for (unsigned row = 1; row <= 2; ++row) {
            boss_statue_apply_caa(c, (uint16_t)(y - BLOCK_SIZE * row), x, caa);
            boss_statue_apply_caa(c, (uint16_t)(y - BLOCK_SIZE * row),
                                  (uint16_t)(x - BLOCK_SIZE), caa);
        }
    }
}

/* KraidStatueHorizontalLignThreeChangeCcaa / RidleyStatueChangeThreeCcaa. */
static void boss_statue_opened_floor(Ctx *c, bool kraid) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE - BLOCK_SIZE * 4);
    uint16_t x = (uint16_t)(s->x + (kraid ? HALF_BLOCK_SIZE : -HALF_BLOCK_SIZE));
    for (unsigned column = 2; column <= 4; ++column) {
        uint16_t point = kraid ? (uint16_t)(x + BLOCK_SIZE * column)
                               : (uint16_t)(x - BLOCK_SIZE * column);
        boss_statue_apply_caa(c, y, point, MZM_CAA_MAKE_NON_POWER_GRIP);
    }
}

static void boss_statue_opened_init(Ctx *c, bool kraid) {
    ai_set_anim(c->s, kraid ? MZM_BOSS_STATUE_KRAID_OPENED
                            : MZM_BOSS_STATUE_RIDLEY_OPENED);
    c->s->pose = 0xF;              /* BOSS_STATUE_POSE_IDLE */
    boss_statue_opened_floor(c, kraid);
}

/* The common debris stream of KraidStatueOpening / RidleyStatueOpening. */
static void boss_statue_debris(Ctx *c, bool kraid) {
    MzmSprite *s = c->s;
    uint8_t r2 = ++s->work2;
    uint8_t r1 = s->work1++;
    if ((r1 & 31) == 0) mzm_screen_shake_start_vertical(10, 0x81);
    /* The native routine reads work1 after incrementing it. */
    r1 = s->work1;
    uint16_t y = (uint16_t)(c->world->camera_y - BLOCK_SIZE);
    uint16_t x = (uint16_t)(c->world->camera_x +
                            (kraid ? BLOCK_SIZE * 9 - QUARTER_BLOCK_SIZE
                                   : BLOCK_SIZE * 6 + QUARTER_BLOCK_SIZE));
    uint8_t rng = c->rng;
    if ((r2 & 31) == 0) {
        if (r2 & 32) {
            ai_debris_init(c, 5, y, (uint16_t)(x + BLOCK_SIZE * 2 - EIGHTH_BLOCK_SIZE -
                                                rng * QUARTER_BLOCK_SIZE));
            ai_debris_init(c, 8, y, (uint16_t)(x - (BLOCK_SIZE * 6 + QUARTER_BLOCK_SIZE) +
                                                rng * EIGHTH_BLOCK_SIZE));
        } else {
            ai_debris_init(c, 7, y, (uint16_t)(x - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) -
                                                rng * QUARTER_BLOCK_SIZE));
            ai_debris_init(c, 5, y, (uint16_t)(x - (BLOCK_SIZE * 4 + EIGHTH_BLOCK_SIZE * 5 + PIXEL_SIZE) +
                                                rng * EIGHTH_BLOCK_SIZE));
        }
    }
    if ((r1 & 15) == 0) {
        if (rng > 7) {
            ai_debris_init(c, 8, y, (uint16_t)(x - (BLOCK_SIZE * 7 + PIXEL_SIZE / 2) +
                                                rng * HALF_BLOCK_SIZE));
            ai_debris_init(c, 6, y, (uint16_t)(x + BLOCK_SIZE * 9 + QUARTER_BLOCK_SIZE -
                                                PIXEL_SIZE / 2 - rng * HALF_BLOCK_SIZE));
        } else {
            ai_debris_init(c, 6, y, (uint16_t)(x + BLOCK_SIZE * 6 - PIXEL_SIZE -
                                                rng * HALF_BLOCK_SIZE));
            ai_debris_init(c, 8, y, (uint16_t)(x - (BLOCK_SIZE * 10 - EIGHTH_BLOCK_SIZE) +
                                                rng * EIGHTH_BLOCK_SIZE));
        }
    }
}

static void boss_statue_opening(Ctx *c, bool kraid) {
    MzmSprite *s = c->s;
    if (c->f->samus_x > s->x + BLOCK_SIZE * 8 + HALF_BLOCK_SIZE)
        boss_statue_wall(c, MZM_CAA_REMOVE_SOLID);

    if (!kraid && s->work0) {
        if (s->frame == 0 && s->adc == 1)
            mzm_sfx_play(MZM_SOUND_BOSS_STATUES_EYE_TURNING_ON);
        if (--s->work0 == 0) {
            ai_set_anim(s, MZM_BOSS_STATUE_RIDLEY_OPENING);
            s->work1 = s->work2 = 0;
            mzm_sfx_play(MZM_SOUND_BOSS_STATUES_RIDLEY_STATUE_OPENING);
        }
        return;
    }

    if (s->frame == 29 && s->adc == 1)
        boss_statue_inside(c, kraid, MZM_CAA_REMOVE_SOLID);
    if (ai_animation_ending(c, 1)) {
        boss_statue_opened_init(c, kraid);
        hook_set_event(c, kraid ? MZM_EVENT_KRAID_STATUE_OPENED
                                : MZM_EVENT_RIDLEY_STATUE_OPENED);
        if (!kraid || !ai_check_event(c, MZM_EVENT_RIDLEY_KILLED) ||
            ai_check_event(c, MZM_EVENT_RIDLEY_STATUE_OPENED)) {
            hook_doors(c, -20);    /* ONE_THIRD_SECOND */
            boss_statue_wall(c, MZM_CAA_REMOVE_SOLID);
        }
        return;
    }
    if (s->frame > 8 && s->frame < 40) boss_statue_debris(c, kraid);
}

/* KraidStatue / RidleyStatue, including the complete post-boss opening and
 * its persistent collision changes. */
static void boss_statue(Ctx *c) {
    MzmSprite *s = c->s;
    bool kraid = s->sprite_id == MZM_PSPRITE_KRAID_STATUE;
    s->ignore_samus_timer = 1;
    if (s->pose == SPRITE_POSE_UNINITIALIZED) {
        s->x = (uint16_t)(s->x + (kraid ? -HALF_BLOCK_SIZE : HALF_BLOCK_SIZE));
        s->draw_top = kraid ? 96 : 88;
        s->draw_bottom = 0;
        s->draw_horizontal = 112;
        s->hitbox_top = s->hitbox_bottom = 0;
        s->hitbox_left = s->hitbox_right = 0;
        s->samus_collision = SSC_NONE;
        s->draw_order = 1;
        if (kraid && !ai_check_event(c, MZM_EVENT_VIEWED_STATUE_ROOM))
            hook_set_event(c, MZM_EVENT_VIEWED_STATUE_ROOM);
        uint8_t opened = kraid ? MZM_EVENT_KRAID_STATUE_OPENED
                               : MZM_EVENT_RIDLEY_STATUE_OPENED;
        uint8_t killed = kraid ? MZM_EVENT_KRAID_KILLED : MZM_EVENT_RIDLEY_KILLED;
        if (ai_check_event(c, opened)) {
            boss_statue_opened_init(c, kraid);
        } else {
            if (ai_check_event(c, killed)) {
                ai_set_anim(s, kraid ? MZM_BOSS_STATUE_KRAID_ACTIVATED
                                     : MZM_BOSS_STATUE_RIDLEY_ACTIVATED);
                s->pose = 0x9;     /* BOSS_STATUE_POSE_CHECK_START_OPEN */
                hook_doors(c, 1);
            } else {
                ai_set_anim(s, kraid ? MZM_BOSS_STATUE_KRAID_IDLE
                                     : MZM_BOSS_STATUE_RIDLEY_IDLE);
                s->pose = 0xF;
            }
            boss_statue_inside(c, kraid, MZM_CAA_MAKE_NON_POWER_GRIP);
        }
    } else if (s->pose == 0x9) {
        if (s->frame == 0 && s->adc == 1)
            mzm_sfx_play(MZM_SOUND_BOSS_STATUES_EYE_TURNING_ON);
        uint16_t sprite_x = (uint16_t)((s->x + HALF_BLOCK_SIZE) / PIXEL_SIZE);
        uint16_t bg_x = (uint16_t)(c->world->camera_x / PIXEL_SIZE);
        uint16_t distance = (uint16_t)(sprite_x - bg_x);
        /* The gameplay viewport keeps the native 240-pixel trigger, while
         * room_runtime's optional 320-pixel-wide view centres it at 160. */
        if (distance == SCREEN_SIZE_X_SUB_PIXEL / PIXEL_SIZE / 2 || distance == 160) {
            s->pose = 0xB;         /* BOSS_STATUE_POSE_OPENING */
            boss_statue_wall(c, MZM_CAA_MAKE_NON_POWER_GRIP);
            if (kraid) {
                ai_set_anim(s, MZM_BOSS_STATUE_KRAID_OPENING);
                s->work1 = s->work2 = 0;
                mzm_sfx_play(MZM_SOUND_BOSS_STATUES_KRAID_STATUE_OPENING);
            } else {
                s->work0 = 60;     /* CONVERT_SECONDS(1.f) */
            }
        }
    } else if (s->pose == 0xB) {
        boss_statue_opening(c, kraid);
    }
    if (ai_samus_nslr(c, BLOCK_SIZE * 10, BLOCK_SIZE * 13) != NSLR_OUT_OF_RANGE)
        s->status |= MZM_SPRITE_STATUS_HIGH_PRIORITY;
    else
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_HIGH_PRIORITY;

    /* KraidStatue moves every live projectile to the final native OAM pass.
     * The flag remains on that projectile until its slot is initialized for
     * a new shot, matching PROJ_STATUS_LOW_OAM_PRIORITY. */
    if (kraid && c->f->weapons) {
        for (int i = 0; i < MZM_MAX_PROJECTILES; ++i)
            if (c->f->weapons->list[i].active)
                c->f->weapons->list[i].low_oam_priority = true;
    }

    /* KraidStatue owns the statue room's camera lock. */
    if (kraid && c->f->globals) {
        MzmSpriteGlobals *g = c->f->globals;
        bool both_open = ai_check_event(c, MZM_EVENT_KRAID_STATUE_OPENED) &&
                         ai_check_event(c, MZM_EVENT_RIDLEY_STATUE_OPENED);
        uint8_t near = ai_samus_nslr(c, BLOCK_SIZE * 10,
                                     both_open ? BLOCK_SIZE * 6 + QUARTER_BLOCK_SIZE
                                               : BLOCK_SIZE * 8 - QUARTER_BLOCK_SIZE);
        bool lock = both_open ? near != NSLR_OUT_OF_RANGE : near == NSLR_RIGHT;
        g->lock_screen = lock;
        if (lock) {
            g->lock_y = s->y;
            g->lock_x = (uint16_t)(s->x + HALF_BLOCK_SIZE);
        }
    }
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
 * drops with four debris, a screen shake and its sound. */
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
    mzm_screen_shake_start_vertical(10, 0x81);  /* CONVERT_SECONDS(1.f / 6) */
    uint16_t y = s->y, x = s->x;
    ai_debris_init(c, 5, (uint16_t)(y + THREE_QUARTER_BLOCK_SIZE), (uint16_t)(x + BLOCK_SIZE + QUARTER_BLOCK_SIZE));
    ai_debris_init(c, 7, (uint16_t)(y + QUARTER_BLOCK_SIZE),
                (uint16_t)(x + 3 * EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2));
    ai_debris_init(c, 8, (uint16_t)(y + BLOCK_SIZE),
                (uint16_t)(x - (BLOCK_SIZE + 3 * EIGHTH_BLOCK_SIZE + PIXEL_SIZE / 2)));
    ai_debris_init(c, 6, (uint16_t)(y + HALF_BLOCK_SIZE), (uint16_t)(x - QUARTER_BLOCK_SIZE));
    mzm_sfx_play(MZM_SOUND_DESSGEEGA_LONG_BEAM_SPAWNING);
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
        mzm_screen_shake_start_vertical(10, 0x81);
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
    mzm_sfx_play(MZM_SOUND_DESSGEEGA_DESTROYING_FLOOR);
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

/* Sidehopper / Dessgeega: the AIs. */
static void hopper(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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
    /* SpriteUtilShouldFall */
    if (should_fall(s)) {
        if (s->status & MZM_SPRITE_STATUS_FACING_DOWN || s->work2 != 0)
            s->pose = ZOOMER_POSE_FALLING_INIT;
        return;
    }
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
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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
 * and a half and two blocks and a half below it, and so does a blue Gamet
 * leader (gamet.c). Their bgPriority changes (2 hidden, 1 moving) are not
 * modelled: sprites share one drawing priority. */
enum {
    PIPE_BUG_POSE_IDLE_INIT = 0x8,
    PIPE_BUG_POSE_IDLE = 0x9,
    PIPE_BUG_POSE_GOING_UP = 0x23,
    PIPE_BUG_POSE_MOVING = 0x25,
};

static bool is_follower(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_GEEGA_FOLLOWER ||
           s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER ||
           s->sprite_id == MZM_PSPRITE_GAMET_BLUE_FOLLOWER;
}

static bool is_zebbo(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN || s->sprite_id == MZM_PSPRITE_ZEBBO_YELLOW ||
           s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_LEADER ||
           s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER;
}

static bool is_gamet(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_GAMET_BLUE_SINGLE ||
           s->sprite_id == MZM_PSPRITE_GAMET_RED_SINGLE ||
           s->sprite_id == MZM_PSPRITE_GAMET_BLUE_LEADER ||
           s->sprite_id == MZM_PSPRITE_GAMET_BLUE_FOLLOWER;
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
    if (s->sprite_id == MZM_PSPRITE_GAMET_BLUE_LEADER &&
        count_children(c, MZM_PSPRITE_GAMET_BLUE_FOLLOWER))
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
                         is_geega(s) ? MZM_SOUND_GEEGA_RISING :
                         is_gamet(s) ? MZM_SOUND_GAMET_RISING : MZM_SOUND_ZEB_RISING);
    if (s->sprite_id == MZM_PSPRITE_GEEGA_LEADER) {
        MzmSprite *follower = ai_spawn_drop_follower(
            c, MZM_PSPRITE_GEEGA_FOLLOWER, s->room_slot, s->gfx_slot, s->ram_slot,
            (uint16_t)(s->y + BLOCK_SIZE * 2 + HALF_BLOCK_SIZE), (uint16_t)(s->x - HALF_BLOCK_SIZE),
            s->status & MZM_SPRITE_STATUS_X_FLIP);
        if (follower) {
            follower->scaling = s->scaling;
            follower->work0 = 18;   /* CONVERT_SECONDS(.3f) */
        }
    } else if (s->sprite_id == MZM_PSPRITE_ZEBBO_GREEN_LEADER ||
               s->sprite_id == MZM_PSPRITE_GAMET_BLUE_LEADER) {
        /* The second follower only spawns after the first. */
        uint8_t id = s->sprite_id == MZM_PSPRITE_GAMET_BLUE_LEADER ? MZM_PSPRITE_GAMET_BLUE_FOLLOWER
                                                                   : MZM_PSPRITE_ZEBBO_GREEN_FOLLOWER;
        MzmSprite *follower = ai_spawn_drop_follower(
            c, id, s->room_slot, s->gfx_slot, s->ram_slot,
            (uint16_t)(s->y + BLOCK_SIZE + HALF_BLOCK_SIZE), (uint16_t)(s->x - HALF_BLOCK_SIZE),
            s->status & MZM_SPRITE_STATUS_X_FLIP);
        if (follower) {
            follower->scaling = s->scaling;
            follower->work0 = 10;   /* CONVERT_SECONDS(1.f / 6) */
            follower = ai_spawn_drop_follower(
                c, id, s->room_slot, s->gfx_slot, s->ram_slot,
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
                      is_geega(s) ? MZM_SOUND_GEEGA_MOVING :
                      is_gamet(s) ? MZM_SOUND_GAMET_MOVING : MZM_SOUND_ZEB_MOVING;
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

/* Zeb / Geega / Zebbo: the AIs. */
static void pipe_bug(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* ZebInit / GeegaInit / ZebboInit / GametInit */
        if (is_gamet(s)) {
            s->hitbox_top = -(HALF_BLOCK_SIZE - PIXEL_SIZE);
            s->hitbox_bottom = HALF_BLOCK_SIZE - PIXEL_SIZE;
            s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
            s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
            s->draw_top = s->draw_bottom = HALF_BLOCK_SIZE / PIXEL_SIZE;
            s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        } else if (is_zebbo(s)) {
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

/* Reo: the AI. */
static void reo(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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

/* SpriteUtilMakeSpriteFaceSamusRotation: turns `rotation` (u8, 256 a
 * turn) by PI / 64 toward the nearest of eight directions to the target. */
uint8_t ai_face_target_rotation(uint8_t rotation, uint16_t target_y, uint16_t target_x,
                                uint16_t sprite_y, uint16_t sprite_x) {
    enum { PI_ = 128, PI2 = 64, PI4 = 32, PI34 = 96, STEP = 2 };
    int32_t r = rotation, dy = (int16_t)target_y, dx = (int16_t)target_x;
    int32_t y = (int16_t)sprite_y, x = (int16_t)sprite_x, target;
    bool column = x - BLOCK_SIZE < dx && x + BLOCK_SIZE > dx;
    if (dy < y) {
        if (column) target = PI_ + PI2;
        else if (dx > x) target = y - dy < BLOCK_SIZE ? 0 : PI_ + PI34;
        else target = y - dy < BLOCK_SIZE ? PI_ : PI_ + PI4;
    } else {
        if (column) target = PI2;
        else if (dx > x) target = dy - y < BLOCK_SIZE ? 0 : PI4;
        else target = dy - y < BLOCK_SIZE ? PI_ : PI34;
    }
    switch (target) {
    case 0:
        if ((uint16_t)(r - 1) < PI_ - 1) r -= STEP;
        else if (r >= PI_) r += STEP;
        break;
    case PI4: case PI2: case PI34:
        if ((uint16_t)(r - target - 1) < PI_ - 1) r -= STEP;
        else if ((uint16_t)(r - target) >= PI_) r += STEP;
        break;
    case PI_:
        if ((uint16_t)(r - 1) < PI_ - 1) r += STEP;
        else if (r > PI_) r -= STEP;
        break;
    default:                                       /* PI + PI_4, PI_2, PI_3_4 */
        if ((uint16_t)(r - (target - PI_) - 1) < PI_ - 1) r += STEP;
        else if ((uint16_t)(r - (target - PI_) - 1) >= PI_) r -= STEP;
        break;
    }
    return (uint8_t)r;
}

/* One axis of SpriteUtilMoveSpriteTowardsSamus: `brake` (work1 / work0)
 * and `speed` (work2 / work3); true when the direction flips. */
static bool move_towards_axis(uint16_t *position, bool positive, uint16_t target,
                              uint8_t *brake, uint8_t *speed, uint8_t cap, uint8_t divisor) {
    if (positive) {
        if (*brake == 0) {
            if (*position > target - PIXEL_SIZE) {
                *brake = *speed;
            } else {
                if (*speed < cap) ++*speed;
                *position = (uint16_t)(*position + (*speed >> divisor));
            }
        } else if (--*brake != 0) {
            *position = (uint16_t)(*position + (*brake >> divisor));
        } else {
            return true;
        }
        return false;
    }
    uint8_t step;
    if (*brake == 0) {
        if (*position < target + PIXEL_SIZE) {
            *brake = *speed;
            return false;
        }
        if (*speed < cap) ++*speed;
        step = (uint8_t)(*speed >> divisor);
    } else if (--*brake != 0) {
        step = (uint8_t)(*brake >> divisor);
    } else {
        return true;
    }
    uint32_t next = (uint32_t)(uint16_t)(*position - step);
    if (next & 0x8000) {
        *brake = 0;
        return true;
    }
    *position = (uint16_t)next;
    return false;
}

/* SpriteUtilMoveSpriteTowardsSamus: accelerates toward the target on each
 * axis, brakes past it and turns around (work0-3 are the native ones). */
void ai_move_towards_target(Ctx *c, uint16_t target_y, uint16_t target_x, uint8_t y_speed,
                            uint8_t x_speed, uint8_t divisor) {
    MzmSprite *s = c->s;
    if (move_towards_axis(&s->x, s->status & MZM_SPRITE_STATUS_FACING_RIGHT, target_x,
                          &s->work1, &s->work2, x_speed, divisor)) {
        s->status ^= MZM_SPRITE_STATUS_FACING_RIGHT;
        s->work2 = 1;
    }
    if (move_towards_axis(&s->y, s->status & MZM_SPRITE_STATUS_FACING_DOWN, target_y,
                          &s->work0, &s->work3, y_speed, divisor)) {
        s->status ^= MZM_SPRITE_STATUS_FACING_DOWN;
        s->work3 = 1;
    }
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
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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

/* Geruta poses (geruta.c). */
enum {
    GERUTA_POSE_IDLE_INIT = 0x8,
    GERUTA_POSE_IDLE = 0x9,
    GERUTA_POSE_WARNING_INIT = 0x32,
    GERUTA_POSE_WARNING = 0x33,
    GERUTA_POSE_LAUNCHING = 0x35,
    GERUTA_POSE_GOING_DOWN = 0x37,
    GERUTA_POSE_BOUNCING = 0x39,
    GERUTA_POSE_GOING_UP = 0x3B,
    GERUTA_POSE_BOUNCING_ON_CEILING = 0x3D,
};

/* Geruta: waits on its ceiling (attacking at once without one), warns when
 * Samus is low and within 5 blocks, launches, dives at 3 pixels a frame
 * drifting a pixel a frame toward her, bounces off the floor and climbs
 * back (GerutaXMovement and GerutaYMovement are the Holtz's movements; the
 * splash of SpriteUtilCheckInRoomEffect is not ported). */
static void geruta(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    const uint16_t height = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* GerutaInit */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        ai_face_samus(c);
        s->draw_top = s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -HALF_BLOCK_SIZE;
        s->hitbox_bottom = HALF_BLOCK_SIZE;
        s->hitbox_left = -THREE_QUARTER_BLOCK_SIZE;
        s->hitbox_right = THREE_QUARTER_BLOCK_SIZE;
        ai_set_anim(s, MZM_GERUTA_IDLE);
        s->health = c->tables->health[s->sprite_id];
        s->samus_collision = SSC_HURTS_SAMUS;
        s->y -= 3 * PIXEL_SIZE;
        s->pose = GERUTA_POSE_IDLE_INIT;
        break;
    case GERUTA_POSE_IDLE_INIT:
        s->pose = GERUTA_POSE_IDLE;
        ai_set_anim(s, MZM_GERUTA_IDLE);
        /* fallthrough */
    case GERUTA_POSE_IDLE: {
        /* GerutaDetectSamus */
        uint16_t y = (uint16_t)(s->y - BLOCK_SIZE);
        if (ai_collision_at(c, y, (uint16_t)(s->x - THREE_QUARTER_BLOCK_SIZE)) != MZM_COLLISION_SOLID &&
            ai_collision_at(c, y, (uint16_t)(s->x + THREE_QUARTER_BLOCK_SIZE)) != MZM_COLLISION_SOLID) {
            ai_face_samus(c);
            s->pose = GERUTA_POSE_WARNING_INIT;
        } else if (c->f->samus_y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE) >= s->y) {
            uint8_t nslr = ai_samus_nslr(c, BLOCK_SIZE * 5, BLOCK_SIZE * 5);
            if (nslr == NSLR_RIGHT || nslr == NSLR_LEFT) s->pose = GERUTA_POSE_WARNING_INIT;
        }
        break;
    }
    case GERUTA_POSE_WARNING_INIT:
        s->pose = GERUTA_POSE_WARNING;
        ai_set_anim(s, MZM_GERUTA_WARNING);
        ai_sound_onscreen(s, MZM_SOUND_GERUTA_WARNING);
        /* fallthrough */
    case GERUTA_POSE_WARNING:
        if (ai_animation_ending(c, 1)) {
            s->pose = GERUTA_POSE_LAUNCHING;
            ai_set_anim(s, MZM_GERUTA_LAUNCHING);
        }
        break;
    case GERUTA_POSE_LAUNCHING:
        if (ai_animation_ending(c, 1)) {
            s->pose = GERUTA_POSE_GOING_DOWN;
            ai_set_anim(s, MZM_GERUTA_GOING_DOWN);
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            ai_face_samus(c);
            ai_sound_onscreen_once(s, MZM_SOUND_GERUTA_GOING_DOWN);
        }
        break;
    case GERUTA_POSE_GOING_DOWN:
        swooper_x_movement(c, PIXEL_SIZE);
        if (swooper_y_movement(c, 3 * PIXEL_SIZE, height)) {
            s->pose = GERUTA_POSE_BOUNCING;
            ai_set_anim(s, MZM_GERUTA_BOUNCING);
        }
        break;
    case GERUTA_POSE_BOUNCING:
        if (ai_animation_ending(c, 1)) {
            s->pose = GERUTA_POSE_GOING_UP;
            ai_set_anim(s, MZM_GERUTA_GOING_UP);
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            ai_sound_onscreen_once(s, MZM_SOUND_GERUTA_GOING_UP);
        }
        break;
    case GERUTA_POSE_GOING_UP:
        swooper_x_movement(c, PIXEL_SIZE);
        if (swooper_y_movement(c, 3 * PIXEL_SIZE, height)) {
            s->y &= BLOCK_POSITION_FLAG;
            s->y += THREE_QUARTER_BLOCK_SIZE + PIXEL_SIZE;
            s->pose = GERUTA_POSE_BOUNCING_ON_CEILING;
            ai_set_anim(s, MZM_GERUTA_BOUNCING_ON_CEILING);
        }
        break;
    case GERUTA_POSE_BOUNCING_ON_CEILING:
        if (ai_animation_ending(c, 2)) s->pose = GERUTA_POSE_IDLE_INIT;
        break;
    default: ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM); break;
    }
}

/* SpriteUtilCheckOutOfRoomEffect and SpriteUtilCheckInRoomEffect: whether a
 * vertical move crossed the room effect's surface upward or downward (their
 * splash particles are not ported). */
bool ai_left_room_effect(const Ctx *c, uint16_t old_y, uint16_t y) {
    uint16_t effect = c->world->effect_y;
    return old_y > effect && y <= effect;
}
bool ai_entered_room_effect(const Ctx *c, uint16_t old_y, uint16_t y) {
    uint16_t effect = c->world->effect_y;
    return old_y < effect && y >= effect;
}

/* gCurrentAffectingClipdata.hazard after a ClipdataProcess at a point. */
uint8_t ai_hazard_at(const Ctx *c, uint16_t y, uint16_t x) {
    return c->world->hazard ? c->world->hazard(c->world->context, y, x) : 0;
}

/* Skultera poses (skultera.c). */
enum {
    SKULTERA_POSE_IDLE_INIT = 0x8,
    SKULTERA_POSE_IDLE = 0x9,
    SKULTERA_POSE_TURNING_AROUND = 0xA,
    SKULTERA_POSE_CHECK_TURNING_AROUND_ENDED = 0xB,
    SKULTERA_POSE_CHASING_SAMUS_INIT = 0x34,
    SKULTERA_POSE_CHASING_SAMUS = 0x35,
    SKULTERA_HEAD_HITBOX = HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    SKULTERA_TAIL_HITBOX = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE,
    SKULTERA_TERRITORY_RANGE = BLOCK_SIZE * 8,
};

/* SkulteraSetSidesHitbox: the head ahead, the tail behind. */
static void skultera_sides(MzmSprite *s) {
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        s->hitbox_left = -SKULTERA_TAIL_HITBOX;
        s->hitbox_right = SKULTERA_HEAD_HITBOX;
    } else {
        s->hitbox_left = -SKULTERA_HEAD_HITBOX;
        s->hitbox_right = SKULTERA_TAIL_HITBOX;
    }
}

/* SkulteraXMovement: true against a wall. */
static bool skultera_x_movement(Ctx *c, uint16_t movement) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
    if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
        if (ai_collision_at(c, y, (uint16_t)(s->x + THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)) !=
            MZM_COLLISION_SOLID) {
            s->x = (uint16_t)(s->x + movement);
            return false;
        }
        return true;
    }
    if (ai_collision_at(c, y, (uint16_t)(s->x - (THREE_QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE))) !=
        MZM_COLLISION_SOLID) {
        s->x = (uint16_t)(s->x - movement);
        return false;
    }
    return true;
}

/* SkulteraIdleInit */
static void skultera_idle_init(MzmSprite *s) {
    s->x_spawn = s->x;
    s->pose = SKULTERA_POSE_IDLE;
    ai_set_anim(s, MZM_SKULTERA_IDLE);
    s->work0 = 3;                   /* CONVERT_SECONDS(.05f) */
}

/* The Samus checks of SkulteraMoving and SkulteraChasingSamus: none within
 * a block across and ten up or down; else her side within 3 blocks up or
 * down and 5 across. */
static uint8_t skultera_nslr(const Ctx *c) {
    if (ai_samus_nslr(c, BLOCK_SIZE * 10, BLOCK_SIZE) != NSLR_OUT_OF_RANGE) return 0xFF;
    return ai_samus_nslr(c, BLOCK_SIZE * 3, BLOCK_SIZE * 5);
}

/* Skultera: patrols its water 8 blocks either side of where it last turned
 * a pixel every 3 frames, chases Samus every 2 frames (rising only in water,
 * half a pixel a frame) and turns toward her. */
static void skultera(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    bool flipped = s->status & MZM_SPRITE_STATUS_X_FLIP;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* SkulteraInit */
        random_x_flip(c);
        s->draw_top = BLOCK_SIZE * 2 / PIXEL_SIZE;
        s->draw_bottom = 0;
        s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        s->hitbox_top = -(BLOCK_SIZE + HALF_BLOCK_SIZE);
        s->hitbox_bottom = 0;
        skultera_sides(s);
        s->pose = SKULTERA_POSE_IDLE_INIT;
        s->samus_collision = SSC_HURTS_SAMUS;
        ai_set_anim(s, MZM_SKULTERA_IDLE);
        s->health = c->tables->health[s->sprite_id];
        break;
    case SKULTERA_POSE_IDLE_INIT:
        skultera_idle_init(s);
        break;
    case SKULTERA_POSE_IDLE: {
        /* SkulteraMoving */
        if (--s->work0 == 0) {
            if (skultera_x_movement(c, PIXEL_SIZE)) {
                s->pose = SKULTERA_POSE_TURNING_AROUND;
            } else {
                if (flipped) {
                    if (s->x_spawn + SKULTERA_TERRITORY_RANGE < s->x) s->pose = SKULTERA_POSE_TURNING_AROUND;
                } else if (s->x_spawn - SKULTERA_TERRITORY_RANGE > s->x) {
                    s->pose = SKULTERA_POSE_TURNING_AROUND;
                }
                s->work0 = 3;
            }
        }
        uint8_t nslr = skultera_nslr(c);
        if (nslr == 0xFF) break;
        if (flipped ? nslr == NSLR_RIGHT : nslr == NSLR_LEFT) s->pose = SKULTERA_POSE_CHASING_SAMUS_INIT;
        else if (flipped ? nslr == NSLR_LEFT : nslr == NSLR_RIGHT) s->pose = SKULTERA_POSE_TURNING_AROUND;
        break;
    }
    case SKULTERA_POSE_TURNING_AROUND:
        /* SkulteraTurningAroundInit */
        s->pose = SKULTERA_POSE_CHECK_TURNING_AROUND_ENDED;
        ai_set_anim(s, MZM_SKULTERA_TURNING_AROUND);
        ai_sound_onscreen_once(s, MZM_SOUND_SKULTERA_TURNING_AROUND);
        /* fallthrough */
    case SKULTERA_POSE_CHECK_TURNING_AROUND_ENDED:
        if (ai_animation_ending(c, 1)) {
            skultera_idle_init(s);
            s->status ^= MZM_SPRITE_STATUS_X_FLIP;
            skultera_sides(s);
        }
        break;
    case SKULTERA_POSE_CHASING_SAMUS_INIT:
        s->pose = SKULTERA_POSE_CHASING_SAMUS;
        ai_set_anim(s, MZM_SKULTERA_CHASING_SAMUS);
        /* fallthrough */
    case SKULTERA_POSE_CHASING_SAMUS: {
        /* SkulteraChasingSamus */
        uint16_t samus_y = (uint16_t)(c->f->samus_y + c->f->samus_hitbox_top / 2);
        uint16_t y = (uint16_t)(s->y - HALF_BLOCK_SIZE);
        if ((uint16_t)(y - BLOCK_SIZE) > samus_y) {
            uint16_t up = (uint16_t)(s->y - (BLOCK_SIZE + HALF_BLOCK_SIZE));
            if (ai_collision_at(c, up, s->x) == MZM_COLLISION_AIR &&
                ai_hazard_at(c, up, s->x) == 1 /* HAZARD_TYPE_WATER */)
                s->y = (uint16_t)(s->y - PIXEL_SIZE / 2);
        } else if ((uint16_t)(y + BLOCK_SIZE) < samus_y) {
            if (ai_collision_at(c, s->y, s->x) == MZM_COLLISION_AIR)
                s->y = (uint16_t)(s->y + PIXEL_SIZE / 2);
        }
        if (--s->work0 == 0) {
            if (skultera_x_movement(c, PIXEL_SIZE)) s->pose = SKULTERA_POSE_TURNING_AROUND;
            else s->work0 = 2;      /* CONVERT_SECONDS(1.f / 30) */
        }
        uint8_t nslr = skultera_nslr(c);
        if (nslr == 0xFF) break;
        if (flipped ? nslr != NSLR_RIGHT : nslr != NSLR_LEFT)
            s->pose = (flipped ? nslr == NSLR_LEFT : nslr == NSLR_RIGHT) ? SKULTERA_POSE_TURNING_AROUND
                                                                         : SKULTERA_POSE_IDLE_INIT;
        break;
    }
    default:
        ai_sprite_death(c, (uint16_t)(s->y - HALF_BLOCK_SIZE), s->x, MZM_PE_SPRITE_EXPLOSION_BIG);
        break;
    }
}

/* Dragon poses (dragon.c). */
enum {
    DRAGON_POSE_IDLE_INIT = 0x8,
    DRAGON_POSE_IDLE = 0x9,
    DRAGON_POSE_TURN_AROUND_INIT = 0xA,
    DRAGON_POSE_TURN_AROUND_FIRST_HALF = 0xB,
    DRAGON_POSE_TURN_AROUND_SECOND_HALF = 0xC,
    DRAGON_POSE_WARNING_INIT = 0x34,
    DRAGON_POSE_WARNING = 0x35,
    DRAGON_POSE_SPIT = 0x37,
    DRAGON_FIREBALL_POSE_MOVE = 0x9,
    DRAGON_FIREBALL_POSE_EXPLODING = 0x43,
    /* STATUS_RISING: the facing down flag means rising. */
    DRAGON_STATUS_RISING = MZM_SPRITE_STATUS_FACING_DOWN,
};

/* DragonYMovement: up to two blocks above its spawn, back down to it; the
 * splashes are not ported. */
static void dragon_y_movement(MzmSprite *s) {
    uint16_t spawn = s->y_spawn;
    if (s->status & DRAGON_STATUS_RISING) {
        if ((uint16_t)(spawn - (BLOCK_SIZE * 2 - ONE_SUB_PIXEL)) < s->y) s->y -= PIXEL_SIZE / 2;
    } else if ((uint16_t)(spawn - PIXEL_SIZE / 2) > s->y) {
        s->y += PIXEL_SIZE / 2;
    }
}

/* DragonIdleInit */
static void dragon_idle_init(MzmSprite *s) {
    s->pose = DRAGON_POSE_IDLE;
    ai_set_anim(s, MZM_DRAGON_IDLE);
    s->work0 = 100;                 /* CONVERT_SECONDS(1.f) + TWO_THIRD_SECOND */
}

/* Dragon: rises from its lava when Samus is above it within 6 blocks,
 * turns toward her and spits a fireball every 100 frames (after a
 * half-second warning), the spit point clear. */
static void dragon(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* DragonInit */
        s->hitbox_top = -BLOCK_SIZE;
        s->hitbox_bottom = BLOCK_SIZE + QUARTER_BLOCK_SIZE;
        s->hitbox_left = -(QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE);
        s->hitbox_right = QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
        s->draw_top = s->draw_bottom = s->draw_horizontal = (BLOCK_SIZE + HALF_BLOCK_SIZE) / PIXEL_SIZE;
        ai_set_anim(s, MZM_DRAGON_IDLE);
        s->samus_collision = SSC_HURTS_SAMUS;
        s->health = c->tables->health[s->sprite_id];
        /* SpriteUtilMakeSpriteFaceSamusXFlip */
        if (s->x > c->f->samus_x) s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        else s->status |= MZM_SPRITE_STATUS_X_FLIP;
        s->pose = DRAGON_POSE_IDLE_INIT;
        s->y_spawn = s->y;
        break;
    case DRAGON_POSE_IDLE_INIT:
        dragon_idle_init(s);
        /* fallthrough */
    case DRAGON_POSE_IDLE: {
        /* DragonIdle */
        if (s->work0) s->work0--;
        dragon_y_movement(s);
        s->status &= (uint16_t)~DRAGON_STATUS_RISING;
        if (c->f->samus_y > s->y) break;
        uint8_t nslr = ai_samus_nslr(c, BLOCK_SIZE * 6, BLOCK_SIZE * 6);
        if (nslr != NSLR_OUT_OF_RANGE) s->status |= DRAGON_STATUS_RISING;
        bool flipped = s->status & MZM_SPRITE_STATUS_X_FLIP;
        if (nslr == NSLR_RIGHT) {
            if (!flipped) {
                s->pose = DRAGON_POSE_TURN_AROUND_INIT;
                break;
            }
        } else if (nslr == NSLR_LEFT) {
            if (flipped) {
                s->pose = DRAGON_POSE_TURN_AROUND_INIT;
                break;
            }
        } else {
            break;
        }
        if (s->work0 == 0) {
            /* The block two above and one ahead (not where the fireball
             * spawns, as in the game). */
            uint16_t y = (uint16_t)(s->y - (BLOCK_SIZE * 2 + EIGHTH_BLOCK_SIZE));
            uint16_t x = flipped ? (uint16_t)(s->x + BLOCK_SIZE + EIGHTH_BLOCK_SIZE)
                                 : (uint16_t)(s->x - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
            if (ai_collision_at(c, y, x) == MZM_COLLISION_AIR) s->pose = DRAGON_POSE_WARNING_INIT;
        }
        break;
    }
    case DRAGON_POSE_TURN_AROUND_INIT:
        s->pose = DRAGON_POSE_TURN_AROUND_FIRST_HALF;
        ai_set_anim(s, MZM_DRAGON_TURNING_AROUND);
        /* fallthrough */
    case DRAGON_POSE_TURN_AROUND_FIRST_HALF:
        dragon_y_movement(s);
        if (ai_animation_ending(c, 1)) {
            s->pose = DRAGON_POSE_TURN_AROUND_SECOND_HALF;
            ai_set_anim(s, MZM_DRAGON_TURNING_AROUND_PART2);
            s->status ^= MZM_SPRITE_STATUS_X_FLIP;
        }
        break;
    case DRAGON_POSE_TURN_AROUND_SECOND_HALF:
        dragon_y_movement(s);
        if (ai_animation_ending(c, 2)) s->pose = DRAGON_POSE_WARNING_INIT;
        break;
    case DRAGON_POSE_WARNING_INIT:
        s->pose = DRAGON_POSE_WARNING;
        ai_set_anim(s, MZM_DRAGON_WARNING);
        s->work0 = 30;              /* CONVERT_SECONDS(.5f) */
        /* fallthrough */
    case DRAGON_POSE_WARNING:
        dragon_y_movement(s);
        if (--s->work0 == 0) {
            s->pose = DRAGON_POSE_SPIT;
            ai_set_anim(s, MZM_DRAGON_SPITTING);
            s->work0 = 8;           /* CONVERT_SECONDS(.1f) + 2 * DELTA_TIME */
        }
        break;
    case DRAGON_POSE_SPIT:
        if (--s->work0 == 0) {
            dragon_idle_init(s);
            uint16_t y = (uint16_t)(s->y - (BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
            bool flipped = s->status & MZM_SPRITE_STATUS_X_FLIP;
            uint16_t x = flipped ? (uint16_t)(s->x + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE)
                                 : (uint16_t)(s->x - (HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE));
            ai_spawn_secondary(c, MZM_SSPRITE_DRAGON_FIREBALL, 0, s->gfx_slot, y, x,
                               s->status & MZM_SPRITE_STATUS_X_FLIP);
        }
        break;
    default: ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_BIG); break;
    }
}

/* DragonFireball: an arc from sDragonFireballYMovement at 3 pixels a frame,
 * rotated with sDragonFireballOamRotation, until the lava's surface or a
 * block. */
static void dragon_fireball(Ctx *c) {
    MzmSprite *s = c->s;
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* DragonFireballInit (bgPriority is not modelled) */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        s->kill_off_screen = true;
        s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_top = s->hitbox_left = -QUARTER_BLOCK_SIZE;
        s->hitbox_bottom = s->hitbox_right = QUARTER_BLOCK_SIZE;
        ai_set_anim(s, MZM_DRAGON_FIREBALL_MOVING);
        s->pose = DRAGON_POSE_IDLE;
        s->samus_collision = SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT;
        s->draw_order = 3;
        s->health = c->tables->secondary_health[s->sprite_id];
        if (s->status & MZM_SPRITE_STATUS_X_FLIP) {
            s->status |= MZM_SPRITE_STATUS_FACING_RIGHT;
            s->rotation = 64;       /* PI_2 */
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_X_FLIP;
        } else {
            s->rotation = 0;
        }
        s->status |= MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
        s->work3 = 0;
        mzm_sfx_play(MZM_SOUND_DRAGON_SPITTING);
        /* fallthrough */
    case DRAGON_FIREBALL_POSE_MOVE: {
        /* DragonFireballMove */
        uint16_t old_y = s->y;
        uint8_t offset = s->work3;
        int16_t y_movement = offset < 40 ? c->tables->dragon_fireball_y[offset] : SHORT_MAX;
        int16_t rotation = offset < 40 ? c->tables->dragon_fireball_rotation[offset] : 0;
        if (y_movement == SHORT_MAX) {
            uint8_t last = offset > 0 && offset <= 40 ? (uint8_t)(offset - 1) : 0;
            rotation = c->tables->dragon_fireball_rotation[last];
            s->y = (uint16_t)(s->y + c->tables->dragon_fireball_y[last]);
        } else {
            s->work3++;
            s->y = (uint16_t)(s->y + y_movement);
        }
        if (s->status & MZM_SPRITE_STATUS_FACING_RIGHT) {
            s->x = (uint16_t)(s->x + PIXEL_SIZE * 3);
            s->rotation = (uint16_t)((rotation + 64) & 0xFF);
        } else {
            s->x = (uint16_t)(s->x - PIXEL_SIZE * 3);
            s->rotation = (uint16_t)(-rotation & 0xFF);
        }
        if (ai_entered_room_effect(c, old_y, s->y)) {
            s->pose = SPRITE_POSE_STOPPED;
            break;
        }
        if (ai_collision_at(c, s->y, s->x) & 0xF0) s->pose = SPRITE_POSE_STOPPED;
        break;
    }
    case SPRITE_POSE_STOPPED:
        /* DragonFireballExplodingInit */
        s->ignore_samus_timer = 1;
        ai_set_anim(s, MZM_DRAGON_FIREBALL_EXPLODING);
        s->pose = DRAGON_FIREBALL_POSE_EXPLODING;
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_ROTATION_SCALING_SINGLE;
        s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        break;
    case DRAGON_FIREBALL_POSE_EXPLODING:
        s->ignore_samus_timer = 1;
        if (ai_animation_ending(c, 1)) s->status = 0;
        break;
    default: ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL); break;
    }
}

/* Squeept poses (squeept.c). */
enum {
    SQUEEPT_POSE_IDLE_INIT = 0xE,
    SQUEEPT_POSE_IDLE = 0xF,
    SQUEEPT_POSE_GOING_UP = 0x35,
    SQUEEPT_POSE_TURNING_AROUND = 0x37,
    SQUEEPT_POSE_GOING_DOWN = 0x39,
};

/* SqueeptGoingUpInit, SqueeptTurningAroundInit, SqueeptGoingDownInit */
static void squeept_set(MzmSprite *s, int16_t top, int16_t bottom, uint8_t anim) {
    s->hitbox_top = top;
    s->hitbox_bottom = bottom;
    ai_set_anim(s, anim);
}

/* SqueeptIdleInit: hidden, half a second before it looks for Samus. */
static void squeept_idle_init(MzmSprite *s) {
    s->status |= MZM_SPRITE_STATUS_NOT_DRAWN;
    s->pose = SQUEEPT_POSE_IDLE;
    squeept_set(s, -(HALF_BLOCK_SIZE + PIXEL_SIZE * 3), HALF_BLOCK_SIZE, MZM_SQUEEPT_GOING_UP);
    s->work0 = 30;
}

/* Squeept: hidden in its lava until Samus is above it (within 8 blocks up
 * and 6 across), it jumps with sSqueeptRisingMovement, turns at the top of
 * the table or at a ceiling once 2.5 blocks above its spawn, falls with
 * sSqueeptFallingMovement and hides again at its spawn height. */
static void squeept(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
    if ((s->isft & 0x7F) > STUN_THRESHOLD && s->pose < SPRITE_POSE_DESTROYED) {
        if (s->adc) s->adc--;
        return;
    }
    switch (s->pose) {
    case SPRITE_POSE_UNINITIALIZED:
        /* SqueeptInit */
        s->draw_top = s->draw_bottom = (BLOCK_SIZE + QUARTER_BLOCK_SIZE) / PIXEL_SIZE;
        s->draw_horizontal = BLOCK_SIZE / PIXEL_SIZE;
        s->hitbox_left = -HALF_BLOCK_SIZE;
        s->hitbox_right = HALF_BLOCK_SIZE;
        s->samus_collision = SSC_HURTS_SAMUS;
        s->health = c->tables->health[s->sprite_id];
        s->y += PIXEL_SIZE;
        s->y_spawn = s->y;
        /* fallthrough */
    case SQUEEPT_POSE_IDLE_INIT:
        squeept_idle_init(s);
        /* fallthrough */
    case SQUEEPT_POSE_IDLE:
        /* SqueeptIdle */
        if (s->work0 != 0) {
            s->work0--;
            break;
        }
        if (ai_samus_above(c, BLOCK_SIZE * 8, BLOCK_SIZE * 6)) {
            s->pose = SQUEEPT_POSE_GOING_UP;
            s->work3 = 0;
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_NOT_DRAWN;
        }
        break;
    case SQUEEPT_POSE_GOING_UP: {
        /* SqueeptGoUp */
        uint16_t old_y = s->y;
        if (s->y_spawn - (BLOCK_SIZE * 2 + HALF_BLOCK_SIZE) > old_y) {
            uint16_t y = (uint16_t)(s->y - (BLOCK_SIZE - QUARTER_BLOCK_SIZE));
            if (ai_collision_at(c, y, (uint16_t)(s->x - HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID ||
                ai_collision_at(c, y, (uint16_t)(s->x + HALF_BLOCK_SIZE)) == MZM_COLLISION_SOLID) {
                squeept_set(s, -QUARTER_BLOCK_SIZE, HALF_BLOCK_SIZE, MZM_SQUEEPT_TURNING_AROUND);
                s->pose = SQUEEPT_POSE_TURNING_AROUND;
                break;
            }
        }
        int16_t movement = s->work3 < 65 ? c->tables->squeept_rising[s->work3] : SHORT_MAX;
        if (movement == SHORT_MAX) {
            squeept_set(s, -QUARTER_BLOCK_SIZE, HALF_BLOCK_SIZE, MZM_SQUEEPT_TURNING_AROUND);
            s->pose = SQUEEPT_POSE_TURNING_AROUND;
        } else {
            s->work3++;
            s->y = (uint16_t)(s->y + movement);
        }
        if (ai_left_room_effect(c, old_y, s->y)) ai_sound_onscreen(s, MZM_SOUND_SQUEEPT_LEAVING_LAVA);
        break;
    }
    case SQUEEPT_POSE_TURNING_AROUND:
        if (ai_animation_ending(c, 1)) {
            squeept_set(s, 0, HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE, MZM_SQUEEPT_GOING_DOWN);
            s->pose = SQUEEPT_POSE_GOING_DOWN;
            s->work3 = 0;
        }
        break;
    case SQUEEPT_POSE_GOING_DOWN: {
        /* SqueeptGoDown */
        uint16_t old_y = s->y;
        uint8_t offset = s->work3;
        int16_t movement = offset < 16 ? c->tables->squeept_falling[offset] : SHORT_MAX;
        if (movement == SHORT_MAX) {
            s->y = (uint16_t)(s->y + c->tables->squeept_falling[offset > 0 && offset <= 16 ? offset - 1 : 0]);
        } else {
            s->work3++;
            s->y = (uint16_t)(s->y + movement);
        }
        if (ai_entered_room_effect(c, old_y, s->y)) ai_sound_onscreen(s, MZM_SOUND_SQUEEPT_ENTERING_LAVA);
        if (s->y_spawn < s->y) {
            s->y = s->y_spawn;
            squeept_idle_init(s);
        }
        break;
    }
    default: ai_sprite_death(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_MEDIUM); break;
    }
}

/* Multiviola: bounces off every surface at 3 subpixels a frame on both
 * axes, its animation starting at twice its slot (MultiviolaMove). */
static void multiviola(Ctx *c) {
    MzmSprite *s = c->s;
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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
uint16_t mzm_bios_sqrt(uint32_t number) {
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

/* Rinka and RinkaMotherBrain (rinka.c): a ring born at its spawn block
 * that, once on screen and vulnerable, flies in a straight line toward
 * where Samus was (its distance from the spawn point grows by 4 subpixels a
 * frame, green 5) and respawns 16 blocks away from her. Mother Brain's
 * Rinkas (Tourian) also have a second spawn point their room's Samus
 * position selects, wait off screen, and vanish once she is dead. */
enum { RINKA_POSE_SPAWNING_INIT = 0x8, RINKA_POSE_SPAWNING = 0x9, RINKA_POSE_MOVING = 0x23 };

static void drop_samus(MzmSprite *s);

static bool is_green_rinka(const MzmSprite *s) {
    return s->status & MZM_SPRITE_STATUS_MOSAIC;
}

static bool is_mother_brain_rinka(const MzmSprite *s) {
    return s->sprite_id != MZM_PSPRITE_RINKA_ORANGE && s->sprite_id != MZM_PSPRITE_RINKA_GREEN;
}

/* RinkaSpawningInit (the green one is flagged MOSAIC, as in the game) and
 * RinkaMotherBrainSpawningInit */
static void rinka_spawning_init(MzmSprite *s) {
    s->pose = RINKA_POSE_SPAWNING;
    s->frame = 0;
    s->adc = 0;
    if (is_mother_brain_rinka(s)) {
        s->anim = MZM_RINKA_MOTHER_BRAIN_SPAWNING;
    } else if (s->sprite_id == MZM_PSPRITE_RINKA_GREEN) {
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
    s->frozen_row_offset = 0;
    s->work1 = is_green_rinka(s) ? 20 : 60;
    s->status |= MZM_SPRITE_STATUS_NOT_DRAWN | MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->ignore_samus_timer = 1;
}

/* The Mother Brain Rinkas' second spawn point: Samus farther left than
 * `blocks` from the first moves it `shift` blocks left (and a block `dy`
 * down); SAMUS_COLLIDING marks it. */
static const struct { uint8_t blocks, shift; int8_t dy; } rinka_second_spawn[6] = {
    {0, 0, 0}, {20, 40, 1}, {17, 34, -1}, {12, 24, 0}, {15, 30, 0}, {10, 20, 0},
};

static int rinka_mother_brain_index(const MzmSprite *s) {
    return s->sprite_id == MZM_PSPRITE_RINKA_MOTHER_BRAIN ? 0 : s->sprite_id - MZM_PSPRITE_RINKA_MOTHER_BRAIN2 + 1;
}

/* RinkaMotherBrainRespawn */
static void rinka_mother_brain_respawn(Ctx *c) {
    MzmSprite *s = c->s;
    uint16_t y = (uint16_t)(s->work3 * BLOCK_SIZE + HALF_BLOCK_SIZE);
    uint16_t x = (uint16_t)(s->work2 * BLOCK_SIZE + HALF_BLOCK_SIZE);
    uint16_t samus_x = c->f->samus_x;
    int i = rinka_mother_brain_index(s);
    if (i > 0) {
        if (samus_x < x && x - samus_x > rinka_second_spawn[i].blocks * BLOCK_SIZE) {
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            y = (uint16_t)(y + rinka_second_spawn[i].dy * BLOCK_SIZE);
            x = (uint16_t)(x - rinka_second_spawn[i].shift * BLOCK_SIZE);
        } else {
            s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_COLLIDING;
        }
    }
    s->y = y;
    s->x = x;
    rinka_spawning_init(s);
    s->health = c->tables->health[s->sprite_id];
    s->isft = 0;
    s->palette_row = 0;
    s->frozen_row_offset = 1;
    s->absolute_palette_row = 0;
    s->freeze_timer = 0;
    s->work1 = 60;                  /* CONVERT_SECONDS(1.f) */
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
        } else if (is_mother_brain_rinka(s)) {
            rinka_mother_brain_respawn(c);
        }
        return;
    }
    if (s->work0) {
        s->ignore_samus_timer = 1;
        if (--s->work0 == 0) s->status &= (uint16_t)~MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
        return;
    }
    if (!ai_animation_ending(c, 1)) return;
    s->anim = is_mother_brain_rinka(s) ? MZM_RINKA_MOTHER_BRAIN_MOVING
              : s->sprite_id == MZM_PSPRITE_RINKA_GREEN ? MZM_RINKA_GREEN_MOVING
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
    if (is_mother_brain_rinka(s)) {
        /* RinkaMotherBrainMove: from the second spawn point when it is the
         * one in use (the u8 block coordinates, the u16 positions). */
        uint8_t tile_y = s->work3, tile_x = s->work2;
        int i = rinka_mother_brain_index(s);
        if (i > 0 && s->status & MZM_SPRITE_STATUS_SAMUS_COLLIDING) {
            tile_x = (uint8_t)(tile_x - rinka_second_spawn[i].shift);
            tile_y = (uint8_t)(tile_y + rinka_second_spawn[i].dy);
        }
        spawn_y = (uint16_t)(tile_y * BLOCK_SIZE + HALF_BLOCK_SIZE);
        spawn_x = (uint16_t)(tile_x * BLOCK_SIZE + HALF_BLOCK_SIZE);
    }
    bool down = s->status & MZM_SPRITE_STATUS_FACING_DOWN;
    bool right = s->status & MZM_SPRITE_STATUS_FACING_RIGHT;
    uint16_t dy = down ? (uint16_t)(s->y_spawn - spawn_y) : (uint16_t)(spawn_y - s->y_spawn);
    uint16_t dx = right ? (uint16_t)(s->x_spawn - spawn_x) : (uint16_t)(spawn_x - s->x_spawn);
    int32_t total = mzm_bios_sqrt((uint32_t)dx * dx + (uint32_t)dy * dy);
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
    if (respawn) {
        if (is_mother_brain_rinka(s)) rinka_mother_brain_respawn(c);
        else rinka_respawn(c);
    }
}

/* Rinka and RinkaMotherBrain: the AIs. */
static void rinka(Ctx *c) {
    MzmSprite *s = c->s;
    bool mother_brain = is_mother_brain_rinka(s);
    if (mother_brain && ai_check_event(c, MZM_EVENT_MOTHER_BRAIN_KILLED)) {
        if ((s->status & (MZM_SPRITE_STATUS_ONSCREEN | MZM_SPRITE_STATUS_NOT_DRAWN)) ==
            MZM_SPRITE_STATUS_ONSCREEN)
            ai_particle_set(c, s->y, s->x, MZM_PE_SPRITE_EXPLOSION_SMALL);
        drop_samus(s);
        s->status = 0;
        return;
    }
    if (s->freeze_timer) {
        ai_update_freeze_timer(c);
        return;
    }
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
        s->draw_order = 3;
        s->work1 = 0;
        if (mother_brain) s->frozen_row_offset = 1;
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
        if (mother_brain) rinka_mother_brain_respawn(c);
        else rinka_respawn(c);
        break;
    }
}

/* EnemyDropInit */
static void enemy_drop_init(Ctx *c) {
    MzmSprite *s = c->s;
    s->ignore_samus_timer = 20;     /* ONE_THIRD_SECOND */
    s->status |= MZM_SPRITE_STATUS_IGNORE_PROJECTILES;
    s->pose = ENEMY_DROP_POSE_IDLE;
    s->y_spawn = 200;               /* CONVERT_SECONDS(3.f) + ONE_THIRD_SECOND */
    s->x_spawn = 0;
    s->adc = 0;
    s->frame = 0;
    s->draw_top = s->draw_bottom = s->draw_horizontal = HALF_BLOCK_SIZE / PIXEL_SIZE;
    if (c->world) s->bg_priority_override = (uint8_t)(c->world->bg1_priority + 1);
    s->draw_order = 1;
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
    case MZM_PSPRITE_MULTIPLE_LARGE_ENERGY:
        s->anim = s->room_slot == 1 ? MZM_DROP_MULTIPLE_LARGE_ENERGY_SLOT1
                                    : s->room_slot == 2 ? MZM_DROP_MULTIPLE_LARGE_ENERGY_SLOT2
                                                       : MZM_DROP_MULTIPLE_LARGE_ENERGY_SLOT3;
        s->samus_collision = SSC_MULTIPLE_LARGE_ENERGY_DROP;
        break;
    default:
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
        case SSC_POWER_BOMB_DROP:
            refill(&e->power_bombs, e->max_power_bombs, 1);     /* POWER_BOMB_DROP_REFILL */
            mzm_sfx_play(MZM_SOUND_GETTING_POWER_BOMB_DROP);
            break;
        case SSC_MULTIPLE_LARGE_ENERGY_DROP:
            refill(&e->energy, e->max_energy, 60);       /* LARGE_ENERGY_DROP_REFILL * 3 */
            mzm_sfx_play(MZM_SOUND_GETTING_LARGE_ENERGY_DROP);
            break;
        default: break;
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
    if (c->s->pose == SPRITE_POSE_UNINITIALIZED) enemy_drop_init(c);
    else if (c->s->pose == ENEMY_DROP_POSE_IDLE) enemy_drop_idle(c);
    c->s->x_spawn++;
}

static bool is_drop(uint8_t sprite_id) {
    return (sprite_id >= MZM_PSPRITE_LARGE_ENERGY_DROP &&
            sprite_id <= MZM_PSPRITE_POWER_BOMB_DROP) ||
           sprite_id == MZM_PSPRITE_MULTIPLE_LARGE_ENERGY;
}

int mzm_sprite_family(uint8_t sprite_id, bool secondary) {
    if (secondary) {
        if (sprite_id == MZM_SSPRITE_ZEELA_EYES) return MZM_FAMILY_ZEELA;
        if (sprite_id == MZM_SSPRITE_DRAGON_FIREBALL) return MZM_FAMILY_DRAGON;
        if (sprite_id == MZM_SSPRITE_UNKNOWN_ITEM_BLOCK_LIGHT || sprite_id == MZM_SSPRITE_MAP_STATION_PART ||
            sprite_id == MZM_SSPRITE_SAVE_PLATFORM_PART || sprite_id == MZM_SSPRITE_GUNSHIP_PART ||
            sprite_id == MZM_SSPRITE_MORPH_BALL_LAUNCHER_PART ||
            sprite_id == MZM_SSPRITE_ZIPLINE_GENERATOR_PART ||
            sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM ||
            sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM_SHADOW ||
            sprite_id == MZM_SSPRITE_CHOZODIA_SAVE_PLATFORM_PART)
            return MZM_FAMILY_GADGET;
        if (sprite_id == MZM_SSPRITE_MORPH_BALL_OUTSIDE || sprite_id == MZM_SSPRITE_POWER_GRIP_GLOW ||
            sprite_id == MZM_SSPRITE_CHARGE_BEAM_GLOW)
            return MZM_FAMILY_ITEM_SPRITE;
        if (sprite_id == MZM_SSPRITE_SPACE_PIRATE_LASER) return MZM_FAMILY_SPACE_PIRATE;
        if (sprite_id == MZM_SSPRITE_ATOMIC_ELECTRICITY) return MZM_FAMILY_ATOMIC;
        if (sprite_id == MZM_SSPRITE_POLYP_PROJECTILE) return MZM_FAMILY_POLYP;
        if (sprite_id == MZM_SSPRITE_METROID_SHELL) return MZM_FAMILY_METROID;
        if (sprite_id == MZM_SSPRITE_KRAID_PART || sprite_id == MZM_SSPRITE_KRAID_SPIKE ||
            sprite_id == MZM_SSPRITE_KRAID_NAIL)
            return MZM_FAMILY_KRAID;
        if (sprite_id == MZM_SSPRITE_RIDLEY_PART || sprite_id == MZM_SSPRITE_RIDLEY_TAIL ||
            sprite_id == MZM_SSPRITE_RIDLEY_FIREBALL || sprite_id == MZM_SSPRITE_RIDLEY_BIG_FIREBALL)
            return MZM_FAMILY_RIDLEY;
        if (sprite_id == MZM_SSPRITE_MOTHER_BRAIN_PART || sprite_id == MZM_SSPRITE_MOTHER_BRAIN_BEAM ||
            sprite_id == MZM_SSPRITE_MOTHER_BRAIN_BLOCK || sprite_id == MZM_SSPRITE_MOTHER_BRAIN_GLASS_BREAKING)
            return MZM_FAMILY_MOTHER_BRAIN;
        if (sprite_id == MZM_SSPRITE_DEOREM_SEGMENT || sprite_id == MZM_SSPRITE_DEOREM_EYE ||
            sprite_id == MZM_SSPRITE_DEOREM_THORN)
            return MZM_FAMILY_DEOREM;
        if (sprite_id == MZM_SSPRITE_ACID_WORM_PART ||
            sprite_id == MZM_SSPRITE_ACID_WORM_SPIT)
            return MZM_FAMILY_ACID_WORM;
        if (sprite_id == MZM_SSPRITE_GADORA_EYE || sprite_id == MZM_SSPRITE_GADORA_BEAM)
            return MZM_FAMILY_GADORA;
        if (sprite_id == MZM_SSPRITE_IMAGO_LARVA_PART) return MZM_FAMILY_IMAGO_LARVA;
        if (sprite_id == MZM_SSPRITE_WINGED_RIPPER ||
            sprite_id == MZM_SSPRITE_IMAGO_COCOON_VINE ||
            sprite_id == MZM_SSPRITE_IMAGO_COCOON_SPORE)
            return MZM_FAMILY_IMAGO_COCOON;
        if (sprite_id == MZM_SSPRITE_DEFEATED_IMAGO_COCOON ||
            sprite_id == MZM_SSPRITE_IMAGO_CEILING_VINE)
            return MZM_FAMILY_IMAGO_COCOON;
        if (sprite_id == MZM_SSPRITE_HIVE_ROOTS) return MZM_FAMILY_HIVE;
        if (sprite_id == MZM_SSPRITE_TANGLE_VINE_GERUTA_PART) return MZM_FAMILY_TANGLE_VINE;
        if (sprite_id == MZM_SSPRITE_ELEVATOR_STATUE_DEBRIS) return MZM_FAMILY_ELEVATOR_STATUE;
        if (sprite_id == MZM_SSPRITE_ESCAPE_SHIP_PART) return MZM_FAMILY_ESCAPE_SHIP;
        if (sprite_id == MZM_SSPRITE_IMAGO_PART || sprite_id == MZM_SSPRITE_IMAGO_NEEDLE ||
            sprite_id == MZM_SSPRITE_IMAGO_DAMAGED_STINGER || sprite_id == MZM_SSPRITE_IMAGO_EGG)
            return MZM_FAMILY_IMAGO;
        if (sprite_id >= MZM_SSPRITE_RUINS_TEST_SYMBOL && sprite_id <= MZM_SSPRITE_RUINS_TEST_LIGHTNING)
            return MZM_FAMILY_RUINS_TEST;
        if (sprite_id == MZM_SSPRITE_MECHA_RIDLEY_PART ||
            sprite_id == MZM_SSPRITE_MECHA_RIDLEY_LASER ||
            sprite_id == MZM_SSPRITE_MECHA_RIDLEY_MISSILE ||
            sprite_id == MZM_SSPRITE_MECHA_RIDLEY_FIREBALL)
            return MZM_FAMILY_MECHA_RIDLEY;
        if (sprite_id == MZM_SSPRITE_CANNON_BULLET) return MZM_FAMILY_TOURIAN;
        if (sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_BEAM2 ||
            sprite_id == MZM_SSPRITE_SEARCHLIGHT_EYE_PROJECTILE)
            return MZM_FAMILY_CHOZODIA_SECURITY;
        if (sprite_id == MZM_SSPRITE_CHOZO_BALL || sprite_id == MZM_SSPRITE_CHOZO_STATUE_PART ||
            sprite_id == MZM_SSPRITE_CHOZO_STATUE_REFILL ||
            sprite_id == MZM_SSPRITE_CHOZO_STATUE_MOVEMENT ||
            sprite_id == MZM_SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_PART ||
            sprite_id == MZM_SSPRITE_UNKNOWN_ITEM_CHOZO_STATUE_REFILL)
            return MZM_FAMILY_CHOZO_STATUE;
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
    case MZM_PSPRITE_GAMET_BLUE_SINGLE:
    case MZM_PSPRITE_GAMET_RED_SINGLE:
    case MZM_PSPRITE_GAMET_BLUE_LEADER:
    case MZM_PSPRITE_GAMET_BLUE_FOLLOWER: return MZM_FAMILY_GAMET;
    case MZM_PSPRITE_REO_GREEN_WINGS:
    case MZM_PSPRITE_REO_PURPLE_WINGS: return MZM_FAMILY_REO;
    case MZM_PSPRITE_MELLA: return MZM_FAMILY_MELLA;
    case MZM_PSPRITE_HOLTZ: return MZM_FAMILY_HOLTZ;
    case MZM_PSPRITE_GERUTA_RED:
    case MZM_PSPRITE_GERUTA_GREEN: return MZM_FAMILY_GERUTA;
    case MZM_PSPRITE_RIPPERII: return MZM_FAMILY_RIPPER2;
    case MZM_PSPRITE_SQUEEPT: return MZM_FAMILY_SQUEEPT;
    case MZM_PSPRITE_SKULTERA: return MZM_FAMILY_SKULTERA;
    case MZM_PSPRITE_DRAGON: return MZM_FAMILY_DRAGON;
    case MZM_PSPRITE_ELEVATOR_PAD: case MZM_PSPRITE_WATER_DROP: case MZM_PSPRITE_STEAM_LARGE:
    case MZM_PSPRITE_STEAM_SMALL: case MZM_PSPRITE_STEAM_LARGE_DIAGONAL_UP:
    case MZM_PSPRITE_STEAM_SMALL_DIAGONAL_UP: case MZM_PSPRITE_STEAM_LARGE_DIAGONAL_DOWN:
    case MZM_PSPRITE_STEAM_SMALL_DIAGONAL_DOWN: case MZM_PSPRITE_GERON_BRINSTAR_ROOM_15:
    case MZM_PSPRITE_GERON_BRINSTAR_ROOM_1C: case MZM_PSPRITE_GERON_VARIA1:
    case MZM_PSPRITE_GERON_VARIA2: case MZM_PSPRITE_GERON_VARIA3: case MZM_PSPRITE_GERON_NORFAIR:
    case MZM_PSPRITE_PLASMA_BEAM_BLOCK: case MZM_PSPRITE_GRAVITY_SUIT_BLOCK:
    case MZM_PSPRITE_SPACE_JUMP_BLOCK: case MZM_PSPRITE_MAP_STATION: case MZM_PSPRITE_SAVE_PLATFORM:
    case MZM_PSPRITE_GUNSHIP: case MZM_PSPRITE_AREA_BANNER: case MZM_PSPRITE_MORPH_BALL_LAUNCHER:
    case MZM_PSPRITE_ZIPLINE: case MZM_PSPRITE_ZIPLINE_BUTTON: case MZM_PSPRITE_ZIPLINE_GENERATOR:
    case MZM_PSPRITE_SECURITY_GATE_DEFAULT_OPEN: case MZM_PSPRITE_SECURITY_GATE_DEFAULT_CLOSED:
    case MZM_PSPRITE_PISTON: case MZM_PSPRITE_GLASS_TUBE: case MZM_PSPRITE_RISING_CHOZO_PILLAR:
    case MZM_PSPRITE_SAVE_PLATFORM_CHOZODIA:
        return MZM_FAMILY_GADGET;
    case MZM_PSPRITE_PARASITE: case MZM_PSPRITE_PARASITE_MULTIPLE: return MZM_FAMILY_PARASITE;
    case MZM_PSPRITE_ATOMIC: return MZM_FAMILY_ATOMIC;
    case MZM_PSPRITE_POLYP: return MZM_FAMILY_POLYP;
    case MZM_PSPRITE_WORKER_ROBOT: return MZM_FAMILY_WORKER_ROBOT;
    case MZM_PSPRITE_METROID: case MZM_PSPRITE_FROZEN_METROID: return MZM_FAMILY_METROID;
    case MZM_PSPRITE_KRAID: return MZM_FAMILY_KRAID;
    case MZM_PSPRITE_RIDLEY: return MZM_FAMILY_RIDLEY;
    case MZM_PSPRITE_MOTHER_BRAIN: return MZM_FAMILY_MOTHER_BRAIN;
    case MZM_PSPRITE_DEOREM_FIRST_LOCATION:
    case MZM_PSPRITE_DEOREM_SECOND_LOCATION: return MZM_FAMILY_DEOREM;
    case MZM_PSPRITE_ACID_WORM: return MZM_FAMILY_ACID_WORM;
    case MZM_PSPRITE_GADORA_KRAID: case MZM_PSPRITE_GADORA_RIDLEY:
        return MZM_FAMILY_GADORA;
    case MZM_PSPRITE_IMAGO_LARVA_RIGHT: case MZM_PSPRITE_IMAGO_LARVA_LEFT:
        return MZM_FAMILY_IMAGO_LARVA;
    case MZM_PSPRITE_IMAGO_LARVA_RIGHT_SIDE:
        return MZM_FAMILY_IMAGO_LARVA_RIGHT_SIDE;
    case MZM_PSPRITE_IMAGO_COCOON:
    case MZM_PSPRITE_IMAGO_COCOON_AFTER_FIGHT:
    case MZM_PSPRITE_DISCOVERED_IMAGO_PASSAGE_EVENT_TRIGGER:
        return MZM_FAMILY_IMAGO_COCOON;
    case MZM_PSPRITE_MECHA_RIDLEY: return MZM_FAMILY_MECHA_RIDLEY;
    case MZM_PSPRITE_HIVE: case MZM_PSPRITE_MELLOW: case MZM_PSPRITE_MELLOW_SWARM:
    case MZM_PSPRITE_MELLOW_SWARM_HEALTH_BASED:
        return MZM_FAMILY_HIVE;
    case MZM_PSPRITE_BARISTUTE: case MZM_PSPRITE_BARISTUTE_KRAID_UPPER:
    case MZM_PSPRITE_BARISTUTE_KRAID_LOWER:
        return MZM_FAMILY_BARISTUTE;
    case MZM_PSPRITE_TANGLE_VINE_TALL: case MZM_PSPRITE_TANGLE_VINE_MEDIUM:
    case MZM_PSPRITE_TANGLE_VINE_CURVED: case MZM_PSPRITE_TANGLE_VINE_SHORT:
    case MZM_PSPRITE_TANGLE_VINE_RED_GERUTA: case MZM_PSPRITE_TANGLE_VINE_GERUTA:
    case MZM_PSPRITE_TANGLE_VINE_LARVA_RIGHT: case MZM_PSPRITE_TANGLE_VINE_LARVA_LEFT:
        return MZM_FAMILY_TANGLE_VINE;
    case MZM_PSPRITE_RUINS_TEST: return MZM_FAMILY_RUINS_TEST;
    case MZM_PSPRITE_FALLING_CHOZO_PILLAR: case MZM_PSPRITE_FAKE_POWER_BOMB:
    case MZM_PSPRITE_FAKE_POWER_BOMB_EVENT_TRIGGER: case MZM_PSPRITE_SPACE_PIRATE_CARRYING_POWER_BOMB:
        return MZM_FAMILY_CHOZODIA_SCRIPTED;
    case MZM_PSPRITE_KRAID_ELEVATOR_STATUE: case MZM_PSPRITE_RIDLEY_ELEVATOR_STATUE:
        return MZM_FAMILY_ELEVATOR_STATUE;
    case MZM_PSPRITE_IMAGO: return MZM_FAMILY_IMAGO;
    case MZM_PSPRITE_ESCAPE_SHIP: case MZM_PSPRITE_ESCAPE_GATE1: case MZM_PSPRITE_ESCAPE_GATE2:
        return MZM_FAMILY_ESCAPE_SHIP;
    case MZM_PSPRITE_KRAID_STATUE:
    case MZM_PSPRITE_RIDLEY_STATUE: return MZM_FAMILY_BOSS_STATUE;
    case MZM_PSPRITE_CANNON: case MZM_PSPRITE_CANNON2: case MZM_PSPRITE_CANNON3:
    case MZM_PSPRITE_ZEBETITE_ONE_AND_THREE: case MZM_PSPRITE_ZEBETITE_TWO_AND_FOUR:
        return MZM_FAMILY_TOURIAN;
    case MZM_PSPRITE_MORPH_BALL:
    case MZM_PSPRITE_POWER_GRIP:
    case MZM_PSPRITE_CHARGE_BEAM: return MZM_FAMILY_ITEM_SPRITE;
    case MZM_PSPRITE_MULTIVIOLA: return MZM_FAMILY_MULTIVIOLA;
    case MZM_PSPRITE_RINKA_ORANGE:
    case MZM_PSPRITE_RINKA_GREEN:
    case MZM_PSPRITE_RINKA_MOTHER_BRAIN: case MZM_PSPRITE_RINKA_MOTHER_BRAIN2:
    case MZM_PSPRITE_RINKA_MOTHER_BRAIN3: case MZM_PSPRITE_RINKA_MOTHER_BRAIN4:
    case MZM_PSPRITE_RINKA_MOTHER_BRAIN5: case MZM_PSPRITE_RINKA_MOTHER_BRAIN6:
        return MZM_FAMILY_RINKA;
    case MZM_PSPRITE_DESSGEEGA:
    case MZM_PSPRITE_DESSGEEGA_AFTER_LONG_BEAM: return MZM_FAMILY_DESSGEEGA;
    case MZM_PSPRITE_SPACE_PIRATE:
    case MZM_PSPRITE_SPACE_PIRATE_WAITING1:
    case MZM_PSPRITE_SPACE_PIRATE_WAITING2:
    case MZM_PSPRITE_SPACE_PIRATE_WAITING3:
    case MZM_PSPRITE_SPACE_PIRATE2: return MZM_FAMILY_SPACE_PIRATE;
    case MZM_PSPRITE_BLACK_SPACE_PIRATE:
    case MZM_PSPRITE_ESCAPE_SHIP_SPACE_PIRATE: return MZM_FAMILY_SPACE_PIRATE;
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
    /* The item and hint statues (not the unknown item ones). */
    case MZM_PSPRITE_CHOZO_STATUE_LONG_HINT:
    case MZM_PSPRITE_CHOZO_STATUE_LONG:
    case MZM_PSPRITE_CHOZO_STATUE_ICE_HINT:
    case MZM_PSPRITE_CHOZO_STATUE_ICE:
    case MZM_PSPRITE_CHOZO_STATUE_WAVE_HINT:
    case MZM_PSPRITE_CHOZO_STATUE_WAVE:
    case MZM_PSPRITE_CHOZO_STATUE_BOMB_HINT:
    case MZM_PSPRITE_CHOZO_STATUE_BOMB:
    case MZM_PSPRITE_CHOZO_STATUE_SPEEDBOOSTER_HINT:
    case MZM_PSPRITE_CHOZO_STATUE_SPEEDBOOSTER:
    case MZM_PSPRITE_CHOZO_STATUE_HIGH_JUMP_HINT:
    case MZM_PSPRITE_CHOZO_STATUE_HIGH_JUMP:
    case MZM_PSPRITE_CHOZO_STATUE_SCREW_HINT:
    case MZM_PSPRITE_CHOZO_STATUE_SCREW:
    case MZM_PSPRITE_CHOZO_STATUE_VARIA_HINT:
    case MZM_PSPRITE_CHOZO_STATUE_VARIA:
    case MZM_PSPRITE_CHOZO_STATUE_GRAVITY:
    case MZM_PSPRITE_CHOZO_STATUE_SPACE_JUMP:
    case MZM_PSPRITE_CHOZO_STATUE_PLASMA_BEAM: return MZM_FAMILY_CHOZO_STATUE;
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

static void deal_damage(MzmSprite *s, uint16_t damage);

/* The kills' "standing on it: Samus to STANDING_MIDAIR" (applied by
 * mzm_sprites_samus_dropped). */
static void drop_samus(MzmSprite *s) {
    if (s->standing_on != MZM_STANDING_ON_SPRITE_OFF) s->standing_on = MZM_STANDING_ON_SPRITE_DROPPED;
}

bool mzm_sprites_samus_dropped(MzmSprite *sprites, int count) {
    bool dropped = false;
    for (int i = 0; i < count; ++i) {
        if (sprites[i].standing_on != MZM_STANDING_ON_SPRITE_DROPPED) continue;
        sprites[i].standing_on = MZM_STANDING_ON_SPRITE_OFF;
        dropped = true;
    }
    return dropped;
}

/* SpriteUtilCheckCollisionAtPosition (gPreviousCollisionCheck) for the
 * solid sprite rules. */
static uint8_t samus_check_collision(const MzmSpriteWorld *world, uint16_t y, uint16_t x) {
    Ctx c = {0};
    c.world = world;
    return ai_collision_at(&c, y, x);
}

/* SpriteUtilCheckStopSamusAgainstSolidSpriteLeft */
static uint8_t stop_samus_left(MzmSpriteSamus *samus, uint16_t samus_y, uint16_t sprite_x) {
    uint16_t x = (uint16_t)(sprite_x - samus->hitbox_right);
    uint8_t collision = samus_check_collision(samus->world, samus_y, (uint16_t)(x + samus->hitbox_left));
    if (collision != MZM_COLLISION_AIR) return collision;
    collision = samus_check_collision(samus->world, (uint16_t)(samus_y - BLOCK_SIZE),
                                      (uint16_t)(x + samus->hitbox_left));
    if (collision == MZM_COLLISION_AIR || samus->morphed) {
        samus->x = x;
        if (samus->facing > 0 && samus->has_energy) samus->stop_x = true;
    }
    return collision;     /* gPreviousCollisionCheck */
}

/* SpriteUtilCheckStopSamusAgainstSolidSpriteRight */
static uint8_t stop_samus_right(MzmSpriteSamus *samus, uint16_t samus_y, uint16_t sprite_x) {
    uint16_t x = (uint16_t)(sprite_x - samus->hitbox_left);
    uint8_t collision = samus_check_collision(samus->world, samus_y, (uint16_t)(x + samus->hitbox_right));
    if (collision != MZM_COLLISION_AIR) return collision;
    collision = samus_check_collision(samus->world, (uint16_t)(samus_y - BLOCK_SIZE),
                                      (uint16_t)(x + samus->hitbox_right));
    if (collision == MZM_COLLISION_AIR || samus->morphed) {
        samus->x = (uint16_t)(x + ONE_SUB_PIXEL);
        if (samus->facing < 0 && samus->has_energy) samus->stop_x = true;
    }
    return collision;     /* gPreviousCollisionCheck */
}

/* SpriteUtilSamusStandingOnSprite */
static void samus_standing_on_sprite(MzmSprite *s, MzmSpriteStanding *st) {
    if (s->status & MZM_SPRITE_STATUS_SAMUS_ON_TOP) {
        if (!st->on_enemy) {
            if (st->midair) st->landing_request = true;
            st->on_enemy = true;
            st->midair = false;
        }
        /* Set again by next frame's collision while Samus stays on top. */
        s->status &= (uint16_t)~MZM_SPRITE_STATUS_SAMUS_ON_TOP;
        return;
    }
    switch (s->standing_on) {
    case MZM_STANDING_ON_SPRITE_ON:
        s->standing_on = MZM_STANDING_ON_SPRITE_RELEASING;
        if (st->copy_y <= st->y) {
            st->y = (uint16_t)(s->y + s->hitbox_top + ONE_SUB_PIXEL);
            st->y_set = true;
        }
        break;
    case MZM_STANDING_ON_SPRITE_RELEASING:
        s->standing_on = MZM_STANDING_ON_SPRITE_OFF;
        if (st->on_enemy) {
            st->on_enemy = false;
            st->midair = true;
        }
        break;
    }
}

/* SpriteUtilUpdateFreezeTimer: down every other frame; the animation timer
 * steps back once it ends; the last 45 ticks flicker the frozen palette. */
void ai_update_freeze_timer(Ctx *c) {
    MzmSprite *s = c->s;
    if (c->frame8 & 1) s->freeze_timer--;
    if (s->freeze_timer == 0) s->adc--;
    if (s->freeze_timer <= 45)
        s->palette_row = s->freeze_timer & 1 ? (uint8_t)(15 - s->gfx_slot - s->frozen_row_offset)
                                             : s->absolute_palette_row;
}

/* ProjectileFreezeSprite */
static void freeze_sprite(MzmSprite *s, uint8_t timer) {
    s->freeze_timer = timer;
    s->palette_row = (uint8_t)(15 - s->gfx_slot - s->frozen_row_offset);
    mzm_sfx_play_not_playing(MZM_SOUND_FREEZING_SPRITE);
}

/* ParasiteCount: more than three Parasites hold Samus. */
static bool parasite_count(const MzmSprite *sprites, int count) {
    int held = 0;
    for (int i = 0; i < count; ++i)
        if (sprites[i].status & MZM_SPRITE_STATUS_EXISTS && sprites[i].samus_collision == SSC_PARASITE &&
            sprites[i].pose == 0x43 && ++held > 3)      /* PARASITE_POSE_SAMUS_GRABBED */
            return true;
    return false;
}

/* SpriteUtilCheckOnZipline */
static bool on_zipline(uint8_t pose) {
    return pose == MZM_POSE_ON_ZIPLINE || pose == MZM_POSE_SHOOTING_ON_ZIPLINE ||
           pose == MZM_POSE_TURNING_ON_ZIPLINE || pose == MZM_POSE_MORPH_BALL_ON_ZIPLINE;
}

static bool contact_kills(MzmSprite *s, const MzmSpriteSamus *samus,
                          const MzmSpriteTables *tables) {
    if (!samus->suitless && !samus->speedboosting && !samus->screw_attacking &&
        samus->pseudo_screw_damage && !s->immune_to_projectiles &&
        !(s->status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES)) {
        /* The pseudo screw: the charge is spent on a weak sprite; Samus is
         * still hurt when it survives. */
        if (weakness_of(s, tables) & (MZM_WEAKNESS_CHARGE_BEAM_PISTOL | MZM_WEAKNESS_BEAM_BOMBS)) {
            if (samus->charge_counter) *samus->charge_counter = 0;
            deal_damage(s, samus->pseudo_screw_damage);
            if (s->health == 0) {
                s->pose = SPRITE_POSE_PSEUDO_SCREW_DESTROYED;
                return true;
            }
            s->ignore_samus_timer = 0;
        }
        return false;
    }
    if (samus->suitless || !(samus->screw_attacking || samus->speedboosting)) return false;
    /* Sprites immune to projectiles take no contact damage either. */
    if (s->immune_to_projectiles) return false;
    /* Sprites ignoring projectiles take no contact damage. */
    if (s->status & MZM_SPRITE_STATUS_IGNORE_PROJECTILES) return false;
    if (weakness_of(s, tables) & MZM_WEAKNESS_SPEEDBOOSTER_SCREW_ATTACK) {
        s->health = 0;
        s->freeze_timer = 0;
        s->palette_row = 0;
        drop_samus(s);
        /* Speed boosting takes precedence over the screw attack. */
        s->pose = !samus->speedboosting ? SPRITE_POSE_SCREW_ATTACK_DESTROYED
                  : samus->shinesparking ? SPRITE_POSE_SHINESPARK_DESTROYED
                                         : SPRITE_POSE_SPEEDBOOSTER_DESTROYED;
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
    const uint16_t samus_y = samus->y, samus_x = samus->x;
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
        uint16_t center_y = (uint16_t)(top + (uint16_t)(bottom - top) / 2);
        bool on_left = center_x >= samus->previous_x;
        /* SPRITE_COLLISION_FLAG_ON_BOTTOM: the hitbox's centre above her. */
        bool on_bottom = center_y + PIXEL_SIZE < samus_top;
        /* A frozen sprite is solid: after the contact damage (no kill),
         * Samus falls from a zipline, or stands on it, is pushed below it
         * or is stopped at its sides. The SSC_SOLID sprites share the
         * rules; SSC_SOLID_CHECK_COLLIDING also flags the contact and ends
         * the pass. */
        bool solid = s->freeze_timer || s->samus_collision == SSC_SOLID ||
                     s->samus_collision == SSC_SOLID_CHECK_COLLIDING;
        if (!s->freeze_timer && s->samus_collision == SSC_SOLID_CHECK_COLLIDING) {
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            ignore = true;
        }
        if (solid) {
            if (samus->pulling_up || contact_kills(s, samus, tables) || !samus->world) continue;
            if (on_zipline(samus->pose)) {
                samus->set_pose = MZM_POSE_MID_AIR_REQUEST;
                samus->pose = MZM_POSE_MID_AIR_REQUEST;
            } else if (samus_y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) < top) {
                if (samus_check_collision(samus->world,
                                          (uint16_t)(top + ONE_SUB_PIXEL + samus->hitbox_top),
                                          samus_x) == MZM_COLLISION_AIR &&
                    samus->y_velocity <= 0) {
                    samus->y = (uint16_t)(top + ONE_SUB_PIXEL);
                    s->status |= MZM_SPRITE_STATUS_SAMUS_ON_TOP;
                    s->standing_on = MZM_STANDING_ON_SPRITE_ON;
                }
            } else if (samus_top + QUARTER_BLOCK_SIZE > bottom) {
                if (samus_check_collision(samus->world, (uint16_t)(bottom - samus->hitbox_top),
                                          samus_x) == MZM_COLLISION_AIR) {
                    samus->y = (uint16_t)(bottom - samus->hitbox_top);
                    if (samus->y_velocity > 0 && samus->has_energy) samus->stop_y = true;
                }
            } else if (samus_x < left || samus_x > right) {
                if (on_left) stop_samus_left(samus, samus_y, left);
                else stop_samus_right(samus, samus_y, right);
            }
            continue;
        }
        switch (s->samus_collision) {
        case SSC_NONE: break;
        case SSC_CAN_STAND_ON_TOP:
            /* Only Samus standing on the sprite, no other collision. */
            if (!samus->pulling_up && !contact_kills(s, samus, tables) && samus->world &&
                samus_y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) < top &&
                samus_check_collision(samus->world, (uint16_t)(top + ONE_SUB_PIXEL + samus->hitbox_top),
                                      samus_x) == MZM_COLLISION_AIR &&
                samus->y_velocity <= 0) {
                samus->y = (uint16_t)(top + ONE_SUB_PIXEL);
                s->status |= MZM_SPRITE_STATUS_SAMUS_ON_TOP;
                s->standing_on = MZM_STANDING_ON_SPRITE_ON;
            }
            break;
        case SSC_SOLID_SIDEWAYS:
            /* Samus stopped at its sides (the Zebetites). */
            if (contact_kills(s, samus, tables) || !samus->world) break;
            if (on_left) stop_samus_left(samus, samus_y, left);
            else stop_samus_right(samus, samus_y, right);
            break;
        case SSC_HURTS_KNOCKBACK_IF_INVINCIBLE: {
            /* Kraid's body: Samus stopped at its side, hurt, or knocked back
             * while invincible when nothing is above her. */
            if (contact_kills(s, samus, tables) || !samus->world) break;
            uint8_t previous = on_left ? stop_samus_left(samus, samus_y, left)
                                       : stop_samus_right(samus, samus_y, right);
            if (!samus->invincible) {
                /* The knockback of `hurt` ignores the wall the native one
                 * checks (gPreviousCollisionCheck). */
                if (samus->hurt) {
                    samus->hurt(samus->context, damage_of(s, tables), on_left);
                    samus->invincible = true;
                }
            } else if (previous == MZM_COLLISION_AIR &&
                       samus_check_collision(samus->world,
                                             (uint16_t)(samus_y + ONE_SUB_PIXEL + samus->hitbox_top),
                                             samus_x) == MZM_COLLISION_AIR &&
                       samus->knockback) {
                samus->big_knockback = true;
                samus->knockback(samus->context, on_left);
                samus->big_knockback = false;
            }
            ignore = true;
            break;
        }
        case SSC_HURTS_RIGHT_CAN_STAND_ON_TOP:
            /* Kraid's spikes: they hurt from the right; Samus stands on
             * them otherwise. */
            if (samus_x > right) {
                if (!samus->invincible && samus->hurt) {
                    samus->hurt(samus->context, damage_of(s, tables), on_left);
                    samus->invincible = true;
                }
                s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
                ignore = true;
            } else if (samus_y - (QUARTER_BLOCK_SIZE + EIGHTH_BLOCK_SIZE) < top && !samus->pulling_up &&
                       samus->invincibility_timer < 38 && samus->world &&
                       samus_check_collision(samus->world, (uint16_t)(top + ONE_SUB_PIXEL + samus->hitbox_top),
                                             samus_x) == MZM_COLLISION_AIR &&
                       samus->y_velocity <= 0) {
                samus->y = (uint16_t)(top + ONE_SUB_PIXEL);
                s->status |= MZM_SPRITE_STATUS_SAMUS_ON_TOP;
                s->standing_on = MZM_STANDING_ON_SPRITE_ON;
            }
            break;
        case SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT_NO_KNOCKBACK:
            /* Ridley's fireballs on Samus held: damage without knockback,
             * even while invincible, then 8 invincible frames. */
            s->pose = SPRITE_POSE_STOPPED;
            if (samus->metroid_drain) samus->metroid_drain(samus->context, damage_of(s, tables), 8);
            samus->invincible = true;
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        case SSC_RIDLEY_CLAW: {
            /* RidleyCheckGrabbing: Samus under the claw with room for her
             * where it would carry her is grabbed; otherwise it hurts. */
            bool grabbing = false;
            if (on_bottom && samus->world) {
                uint16_t gy = (uint16_t)(s->y + BLOCK_SIZE * 4);
                uint16_t gx = samus_x > s->x ? (uint16_t)(s->x + (BLOCK_SIZE * 3 - QUARTER_BLOCK_SIZE))
                                             : (uint16_t)(s->x - (BLOCK_SIZE * 3 - QUARTER_BLOCK_SIZE));
                grabbing = samus_check_collision(samus->world, gy, (uint16_t)(gx + samus->hitbox_left)) ==
                               MZM_COLLISION_AIR &&
                           samus_check_collision(samus->world, gy, (uint16_t)(gx + samus->hitbox_right)) ==
                               MZM_COLLISION_AIR;
            }
            if (grabbing) {
                s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
                s->samus_collision = SSC_NONE;
                samus->set_pose = MZM_POSE_MIDAIR;
                samus->pose = MZM_POSE_MIDAIR;
                mzm_sfx_play(MZM_SOUND_RIDLEY_GRABBING_SAMUS);
            } else {
                /* (the hurt's knockback velocity the native leaves alone) */
                if (!samus->invincible && !contact_kills(s, samus, tables) && samus->hurt) {
                    samus->hurt(samus->context, damage_of(s, tables), on_left);
                    samus->invincible = true;
                }
                s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            }
            ignore = true;
            break;
        }
        case SSC_MECHA_RIDLEY:
            /* The native boss doubles contact damage at 100% completion.
             * Completion percentage is not exposed to this shared contact
             * pass yet, so the ordinary route uses its sprite stat damage. */
            if (!samus->invincible && !contact_kills(s, samus, tables) && samus->hurt) {
                samus->hurt(samus->context, damage_of(s, tables), on_left);
                samus->invincible = true;
            }
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        case SSC_KNOCKS_BACK_SAMUS:
            /* A knockback only, no damage. */
            if (contact_kills(s, samus, tables)) break;
            if (samus->knockback) samus->knockback(samus->context, on_left);
            ignore = true;
            break;
        case SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT_BIG_KNOCKBACK:
            s->pose = SPRITE_POSE_STOPPED;
            /* fallthrough */
        case SSC_HURTS_BIG_KNOCKBACK:
            /* The hurt code with a quarter-block knockback (Imago). */
            if (contact_kills(s, samus, tables)) break;
            if (!samus->invincible && samus->hurt) {
                samus->big_knockback = true;
                samus->hurt(samus->context, damage_of(s, tables), on_left);
                samus->big_knockback = false;
                samus->invincible = true;
            }
            s->ignore_samus_timer = 15;                /* CONVERT_SECONDS(.25f) */
            ignore = true;
            break;
        case SSC_HURTS_SAMUS_A_LOT:
            /* The hurt code with five times the damage and a bigger
             * knockback. */
            if (contact_kills(s, samus, tables)) break;
            if (!samus->invincible && samus->hurt) {
                samus->hurts_a_lot = true;
                samus->hurt(samus->context, (uint16_t)(damage_of(s, tables) * 5), on_left);
                samus->hurts_a_lot = false;
                samus->invincible = true;
            }
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        case SSC_HURTS_SAMUS_STOP_DIES_WHEN_HIT:
        case SSC_SPACE_PIRATE_LASER:
            s->pose = SPRITE_POSE_STOPPED;
            /* fallthrough */
        case SSC_HURTS_SAMUS_IGNORE_INVINCIBILITY:
            /* Mother Brain's beam: it hurts even an invincible Samus. */
            if (contact_kills(s, samus, tables)) break;
            if (samus->hurt) {
                samus->hurt(samus->context, damage_of(s, tables), on_left);
                samus->invincible = true;
            }
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        case SSC_HURTS_SAMUS_SOLID: {
            /* The hives: Samus stopped at their side, and hurt. */
            if (contact_kills(s, samus, tables) || !samus->world) break;
            if (on_left) stop_samus_left(samus, samus_y, left);
            else stop_samus_right(samus, samus_y, right);
            if (!samus->invincible) {
                if (samus->hurt) {
                    samus->hurt(samus->context, damage_of(s, tables), on_left);
                    samus->invincible = true;
                }
                ignore = true;
            }
            break;
        }
        case SSC_HURTS_SAMUS:
        case SSC_MELLOW:
        case SSC_SPACE_PIRATE:      /* same as the hurt code */
            if (contact_kills(s, samus, tables)) break;
            if (!samus->invincible && samus->hurt) {
                samus->hurt(samus->context, damage_of(s, tables), on_left);
                samus->invincible = true;
            }
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        case SSC_ZIPLINE:
            /* Grabbed from below while mid-air or spinning (hanging under
             * it), or in morph ball mid-air or ballsparking (inside it). */
            if (on_bottom) {
                uint8_t pose = MZM_POSE_NONE;
                uint16_t offset = 0;
                switch (samus->pose) {
                case MZM_POSE_SPINNING:
                case MZM_POSE_MIDAIR:
                    pose = MZM_POSE_ON_ZIPLINE;
                    offset = BLOCK_SIZE * 2 + HALF_BLOCK_SIZE + EIGHTH_BLOCK_SIZE;
                    break;
                case MZM_POSE_BALLSPARKING:
                    mzm_sfx_stop(MZM_SOUND_BALLSPARKING);
                    /* fallthrough */
                case MZM_POSE_MORPH_BALL_MIDAIR:
                    pose = MZM_POSE_MORPH_BALL_ON_ZIPLINE;
                    offset = BLOCK_SIZE - PIXEL_SIZE;
                    break;
                default:
                    break;
                }
                if (pose != MZM_POSE_NONE) {
                    samus->y = (uint16_t)(s->y + offset);
                    samus->x = s->x;
                    samus->previous_x = samus->x;
                    samus->set_pose = pose;
                    samus->pose = pose;
                    s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
                }
            }
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            ignore = true;
            break;
        case SSC_PARASITE:
            /* Leeching once more than three Parasites hold her, without
             * the Varia or Gravity Suit; other contacts go on this frame. */
            s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            if (!samus->invincible && !samus->suited && parasite_count(sprites, count) && samus->leech &&
                samus->leech(samus->context)) {
                samus->invincible = true;
                mzm_sfx_play_not_playing(MZM_SOUND_BUG_LEECHING);
                if (samus->leech_count && ++*samus->leech_count % 4 == 0) mzm_sfx_play(MZM_SOUND_HURT);
            }
            s->ignore_samus_timer = IGNORE_SAMUS_AFTER_HIT;
            break;
        case SSC_METROID:
            /* Samus below its middle: it grabs her; it drains her when she
             * is not invincible. */
            if (samus_y > (uint16_t)(s->y + HALF_BLOCK_SIZE)) s->status |= MZM_SPRITE_STATUS_SAMUS_COLLIDING;
            if (!samus->invincible && samus->metroid_drain) {
                samus->metroid_drain(samus->context, damage_of(s, tables), 0);
                samus->invincible = true;
            }
            break;
        case SSC_CHECK_COLLIDING:
        case SSC_SMALL_ENERGY_DROP:
        case SSC_LARGE_ENERGY_DROP:
        case SSC_MISSILE_DROP:
        case SSC_SUPER_MISSILE_DROP:
        case SSC_POWER_BOMB_DROP:
        case SSC_MULTIPLE_LARGE_ENERGY_DROP:
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
        drop_samus(s);
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

/* ProjectileIceBeamDealDamage: the freeze timer when the sprite survives. */
static uint8_t ice_damage(MzmSprite *s, uint16_t damage) {
    uint8_t timer = 0;
    if (s->health > damage) {
        s->health -= damage;
        timer = SPRITE_FREEZE_TIMER;
    } else {
        s->health = 0;
        s->freeze_timer = 0;
        s->palette_row = 0;
        drop_samus(s);
        s->pose = SPRITE_POSE_DESTROYED;
        s->ignore_samus_timer = 1;
    }
    set_isft(s, ISFT_DAMAGED);
    s->damaged = true;
    return timer;
}

/* ProjectileIceBeamHittingSprite / ProjectileChargedIceBeamHittingSprite:
 * the impact particle (or the invincible one), then the freezing particle
 * when the sprite freezes. A weak sprite freezes when gFrameCounter8Bit and
 * its freeze flag share no bit (charged: always). */
static void ice_hit(MzmSprite *s, uint8_t weakness, uint8_t freeze_flag, uint8_t frame8,
                    uint16_t damage, bool charged, MzmEffects *effects, uint16_t y, uint16_t x,
                    uint8_t effect) {
    uint8_t timer = 0;
    uint8_t weak = charged ? MZM_WEAKNESS_CHARGE_BEAM_PISTOL | MZM_WEAKNESS_BEAM_BOMBS
                           : MZM_WEAKNESS_BEAM_BOMBS;
    if (weakness & weak) {
        if (weakness & MZM_WEAKNESS_CAN_BE_FROZEN && (charged || !(frame8 & freeze_flag)))
            timer = ice_damage(s, damage);
        else
            deal_damage(s, damage);
    } else if (weakness & MZM_WEAKNESS_CAN_BE_FROZEN) {
        timer = SPRITE_FREEZE_TIMER;
    } else {
        hit_solid(s);
        effect = MZM_PE_HITTING_SOMETHING_INVINCIBLE;
    }
    if (effects) mzm_particle_set(effects, y, x, effect);
    if (timer) {
        freeze_sprite(s, timer);
        if (effects)
            mzm_particle_set(effects, y, x, charged ? MZM_PE_FREEZING_SPRITE_WITH_CHARGED_ICE
                                                    : MZM_PE_FREEZING_SPRITE_WITH_ICE);
    }
}

/* ProjectilePowerBombDealDamage: once per power bomb (the stunned flag). */
static void power_bomb_damage(MzmSprite *s, uint8_t weakness) {
    uint8_t isft;
    if (s->immune_to_projectiles) {
        if ((s->isft & 0x7F) < 2) set_isft(s, 2);
        return;
    }
    if (s->solid_for_projectiles) {
        hit_solid(s);
        return;
    }
    if (weakness & MZM_WEAKNESS_POWER_BOMB) {
        if (s->health > POWER_BOMB_DAMAGE) {
            s->health -= POWER_BOMB_DAMAGE;
        } else {
            s->health = 0;
            s->freeze_timer = 0;
            s->palette_row = 0;
            drop_samus(s);
            s->pose = SPRITE_POSE_DESTROYED;
            s->ignore_samus_timer = 1;
        }
        isft = ISFT_DAMAGED;
    } else {
        isft = ISFT_SOLID_HIT;
    }
    if ((s->isft & 0x7F) < isft) s->isft = (uint8_t)(isft | ISFT_POWER_BOMB_STUNNED);
    else s->isft |= ISFT_POWER_BOMB_STUNNED;
    s->damaged = true;
}

/* The particle a projectile sets on a sprite solid for projectiles. */
static uint8_t solid_hit_effect(uint8_t type, uint32_t items) {
    switch (type) {
    case MZM_PROJECTILE_LONG_BEAM:
    case MZM_PROJECTILE_CHARGED_LONG_BEAM:
    case MZM_PROJECTILE_CHARGED_PISTOL: return MZM_PE_HITTING_SOMETHING_WITH_LONG_BEAM;
    case MZM_PROJECTILE_ICE_BEAM:
    case MZM_PROJECTILE_CHARGED_ICE_BEAM: return MZM_PE_HITTING_SOMETHING_WITH_ICE_BEAM;
    case MZM_PROJECTILE_WAVE_BEAM:
    case MZM_PROJECTILE_CHARGED_WAVE_BEAM:
        return items & MZM_ITEM_ICE_BEAM ? MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM_NO_PLASMA
                                         : MZM_PE_HITTING_SOMETHING_WITH_WAVE_BEAM;
    case MZM_PROJECTILE_PLASMA_BEAM:
    case MZM_PROJECTILE_CHARGED_PLASMA_BEAM:
        return items & MZM_ITEM_ICE_BEAM ? MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM
                                         : MZM_PE_HITTING_SOMETHING_WITH_PLASMA_BEAM;
    case MZM_PROJECTILE_MISSILE: return MZM_PE_HITTING_SOMETHING_WITH_MISSILE;
    case MZM_PROJECTILE_SUPER_MISSILE: return MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE;
    default: return MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM;
    }
}

/* What one projectile does to the sprite it touches (the hit routines of
 * ProjectileCheckHittingSprite), the projectile at (py, px). */
static void projectile_hit(MzmSprite *s, MzmProjectile *proj, MzmWeapons *weapons,
                           const MzmSpriteTables *tables, MzmEffects *effects, uint8_t weakness,
                           uint16_t py, uint16_t px) {
    /* The uncharged pistol always has the immune-projectile result.
     * A charged pistol instead paralyses a Space Pirate for one
     * second, regardless of its normal projectile properties. */
    if (proj->type == MZM_PROJECTILE_PISTOL) {
        if ((s->isft & 0x7F) < 2) set_isft(s, 2);
        if (effects) mzm_particle_set(effects, py, px, MZM_PE_HITTING_SOMETHING_INVINCIBLE);
        proj->active = false;
        return;
    }
    if (proj->type == MZM_PROJECTILE_CHARGED_PISTOL &&
        s->samus_collision == SSC_SPACE_PIRATE) {
        s->standing_on = MZM_STANDING_ON_SPRITE_OFF;
        s->freeze_timer = 60;
        s->palette_row = 1;
        s->absolute_palette_row = 1;
        if (effects)
            mzm_particle_set(effects, py, px, MZM_PE_HITTING_SOMETHING_WITH_LONG_BEAM);
        proj->active = false;
        return;
    }
    /* The Plasma Beam goes through the sprites it hits. */
    bool plasma = proj->type == MZM_PROJECTILE_PLASMA_BEAM ||
                  proj->type == MZM_PROJECTILE_CHARGED_PLASMA_BEAM;
    /* The particle at the projectile: its impact, or the
     * invincible impact when the sprite is not weak to it. */
    uint8_t effect = MZM_PE_HITTING_SOMETHING_INVINCIBLE;
    bool missile = proj->type == MZM_PROJECTILE_MISSILE ||
                   proj->type == MZM_PROJECTILE_SUPER_MISSILE;
    if (s->solid_for_projectiles) {
        /* The SP_SOLID_FOR_PROJECTILES case of each hit routine:
         * the projectile's own impact particle, the projectile
         * removed (the Plasma Beam goes through). */
        hit_solid(s);
        if (effects) mzm_particle_set(effects, py, px, solid_hit_effect(proj->type, weapons->items));
        if (!plasma) proj->active = false;
        return;
    }
    if (s->immune_to_projectiles) {
        /* ProjectileHitSpriteImmuneToProjectiles; missiles tumble. */
        if ((s->isft & 0x7F) < 2) set_isft(s, 2);
        if (effects) mzm_particle_set(effects, py, px, effect);
        if (missile) mzm_projectile_start_tumbling(proj, s->x);
        else if (!plasma) proj->active = false;
        return;
    }
    uint8_t freeze_flag = s->secondary ? tables->secondary_freeze[s->sprite_id]
                                       : tables->freeze[s->sprite_id];
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
            hit_solid(s);
            if (effects) mzm_particle_set(effects, py, px, effect);
            mzm_projectile_start_tumbling(proj, s->x);
            return;
        }
        break;
    case MZM_PROJECTILE_LONG_BEAM:  /* ProjectileHitSprite, LONG_BEAM_DAMAGE */
        if (weakness & MZM_WEAKNESS_BEAM_BOMBS) {
            deal_damage(s, LONG_BEAM_DAMAGE);
            effect = MZM_PE_HITTING_SOMETHING_WITH_LONG_BEAM;
        } else {
            hit_solid(s);
        }
        break;
    case MZM_PROJECTILE_CHARGED_BEAM:
    case MZM_PROJECTILE_CHARGED_LONG_BEAM:
    case MZM_PROJECTILE_CHARGED_PISTOL: {
        /* ProjectileNonIceChargedHitSprite */
        bool is_long = proj->type == MZM_PROJECTILE_CHARGED_LONG_BEAM;
        if (weakness & (MZM_WEAKNESS_CHARGE_BEAM_PISTOL | MZM_WEAKNESS_BEAM_BOMBS)) {
            uint16_t damage = proj->type == MZM_PROJECTILE_CHARGED_PISTOL ?
                CHARGED_PISTOL_DAMAGE : is_long ? CHARGED_LONG_BEAM_DAMAGE :
                CHARGED_NORMAL_BEAM_DAMAGE;
            deal_damage(s, damage);
            effect = is_long || proj->type == MZM_PROJECTILE_CHARGED_PISTOL ?
                MZM_PE_HITTING_SOMETHING_WITH_LONG_BEAM :
                MZM_PE_HITTING_SOMETHING_WITH_NORMAL_BEAM;
        } else {
            hit_solid(s);
        }
        break;
    }
    case MZM_PROJECTILE_ICE_BEAM:
    case MZM_PROJECTILE_CHARGED_ICE_BEAM: {
        bool charged = proj->type == MZM_PROJECTILE_CHARGED_ICE_BEAM;
        bool is_long = weapons->items & MZM_ITEM_LONG_BEAM;
        uint16_t damage = charged ? (is_long ? CHARGED_ICE_LONG_BEAM_DAMAGE : CHARGED_ICE_BEAM_DAMAGE)
                                  : (is_long ? ICE_LONG_BEAM_DAMAGE : ICE_BEAM_DAMAGE);
        ice_hit(s, weakness, freeze_flag, weapons->frame8, damage, charged, effects, py, px,
                MZM_PE_HITTING_SOMETHING_WITH_ICE_BEAM);
        proj->active = false;
        return;
    }
    case MZM_PROJECTILE_WAVE_BEAM:
    case MZM_PROJECTILE_CHARGED_WAVE_BEAM:
    case MZM_PROJECTILE_PLASMA_BEAM:
    case MZM_PROJECTILE_CHARGED_PLASMA_BEAM: {
        /* The Wave and Plasma cases of ProjectileCheckHittingSprite:
         * WAVE_* / PLASMA_* damage (3, one more per other beam;
         * charged 12, four more per other beam), ice hits with the
         * Ice Beam (the full beam particles), else ProjectileHitSprite
         * or ProjectileNonIceChargedHitSprite. */
        bool charged = proj->type == MZM_PROJECTILE_CHARGED_WAVE_BEAM ||
                       proj->type == MZM_PROJECTILE_CHARGED_PLASMA_BEAM;
        uint32_t items = weapons->items;
        int extra = (items & MZM_ITEM_LONG_BEAM ? 1 : 0) + (items & MZM_ITEM_ICE_BEAM ? 1 : 0) +
                    (plasma && items & MZM_ITEM_WAVE_BEAM ? 1 : 0);
        uint16_t damage = (uint16_t)(charged ? 12 + 4 * extra : 3 + extra);
        if (items & MZM_ITEM_ICE_BEAM) {
            ice_hit(s, weakness, freeze_flag, weapons->frame8, damage, charged, effects, py, px,
                    plasma ? MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM
                           : MZM_PE_HITTING_SOMETHING_WITH_FULL_BEAM_NO_PLASMA);
            if (!plasma) proj->active = false;
            return;
        }
        if (weakness & (charged ? MZM_WEAKNESS_CHARGE_BEAM_PISTOL | MZM_WEAKNESS_BEAM_BOMBS
                                : MZM_WEAKNESS_BEAM_BOMBS)) {
            deal_damage(s, damage);
            effect = plasma ? MZM_PE_HITTING_SOMETHING_WITH_PLASMA_BEAM
                            : MZM_PE_HITTING_SOMETHING_WITH_WAVE_BEAM;
        } else {
            hit_solid(s);
        }
        if (effects) mzm_particle_set(effects, py, px, effect);
        if (!plasma) proj->active = false;
        return;
    }
    case MZM_PROJECTILE_BOMB:
    case MZM_PROJECTILE_POWER_BOMB:
    case MZM_PROJECTILE_PISTOL:
        break;
    case MZM_PROJECTILE_SUPER_MISSILE:
        if (weakness & (MZM_WEAKNESS_MISSILES | MZM_WEAKNESS_SUPER_MISSILES)) {
            deal_damage(s, SUPER_MISSILE_DAMAGE);
            effect = MZM_PE_HITTING_SOMETHING_WITH_SUPER_MISSILE;
        } else {
            hit_solid(s);
            if (effects) mzm_particle_set(effects, py, px, effect);
            mzm_projectile_start_tumbling(proj, s->x);
            return;
        }
        break;
    }
    if (effects) mzm_particle_set(effects, py, px, effect);
    proj->active = false;
}

/* BlackSpacePirateProjectileCollision: a sprite ignoring the common pass
 * tests the projectiles itself, by their position inside its hitbox. The
 * Plasma Beam (removed here) and missiles hit as usual; every other beam is
 * absorbed with the invincible particle. */
void ai_projectile_collision_self(Ctx *c) {
    MzmWeapons *weapons = c->f->weapons;
    MzmSprite *s = c->s;
    if (!weapons) return;
    uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
    uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
    for (int p = 0; p < MZM_MAX_PROJECTILES; ++p) {
        MzmProjectile *proj = &weapons->list[p];
        if (!proj->active || proj->stage == MZM_STAGE_TUMBLING || proj->stage <= MZM_STAGE_SPAWNING ||
            proj->type == MZM_PROJECTILE_BOMB || proj->type == MZM_PROJECTILE_POWER_BOMB)
            continue;
        uint16_t py = (uint16_t)proj->y, px = (uint16_t)proj->x;
        if (!(px > left && px < right && py > top && py < bottom)) continue;
        switch (proj->type) {
        case MZM_PROJECTILE_PLASMA_BEAM:
        case MZM_PROJECTILE_CHARGED_PLASMA_BEAM:
            projectile_hit(s, proj, weapons, c->tables, c->f->effects, weakness_of(s, c->tables), py, px);
            proj->active = false;
            break;
        case MZM_PROJECTILE_MISSILE:
        case MZM_PROJECTILE_SUPER_MISSILE:
            projectile_hit(s, proj, weapons, c->tables, c->f->effects, weakness_of(s, c->tables), py, px);
            break;
        default:
            proj->active = false;
            if (c->f->effects) mzm_particle_set(c->f->effects, py, px, MZM_PE_HITTING_SOMETHING_INVINCIBLE);
            break;
        }
    }
}

void mzm_sprites_projectile_hits(MzmSprite *sprites, int count, MzmWeapons *weapons,
                                 const MzmSpriteTables *tables, MzmEffects *effects) {
    /* ProjectileProcessPowerBomb clears every sprite's stunned flag. */
    if (weapons->power_bomb_set)
        for (int i = 0; i < count; ++i)
            if (sprites[i].status & MZM_SPRITE_STATUS_EXISTS)
                sprites[i].isft &= (uint8_t)~ISFT_POWER_BOMB_STUNNED;
    /* The power bomb explosion's pass of ProjectileCheckHittingSprite (a
     * power bomb was placed, so the maximum is not zero). */
    const MzmPowerBomb *pb = &weapons->power_bomb;
    if (pb->state != MZM_PB_NONE) {
        uint16_t top = (uint16_t)(pb->y + pb->hitbox_top), bottom = (uint16_t)(pb->y + pb->hitbox_bottom);
        uint16_t left = (uint16_t)(pb->x + pb->hitbox_left), right = (uint16_t)(pb->x + pb->hitbox_right);
        for (int i = 0; i < count; ++i) {
            MzmSprite *s = &sprites[i];
            if ((s->status & (MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_IGNORE_PROJECTILES)) !=
                    MZM_SPRITE_STATUS_EXISTS || s->health == 0 || s->isft & ISFT_POWER_BOMB_STUNNED)
                continue;
            if (touching((uint16_t)(s->y + s->hitbox_top), (uint16_t)(s->y + s->hitbox_bottom),
                         (uint16_t)(s->x + s->hitbox_left), (uint16_t)(s->x + s->hitbox_right),
                         top, bottom, left, right))
                power_bomb_damage(s, weakness_of(s, tables));
        }
    }
    /* gSpriteDrawOrder: the sprites that can be hit, chosen once before the
     * pass, are visited by draw order (1 to 16), then slot. A sprite's
     * hitbox is read once, before the projectiles that hit it. */
    enum { MAX_ORDERED = 64 };
    uint8_t order[MAX_ORDERED];
    int ordered = count < MAX_ORDERED ? count : MAX_ORDERED;
    for (int i = 0; i < ordered; ++i) {
        const MzmSprite *s = &sprites[i];
        order[i] = (s->status & (MZM_SPRITE_STATUS_EXISTS | MZM_SPRITE_STATUS_IGNORE_PROJECTILES)) ==
                           MZM_SPRITE_STATUS_EXISTS && s->health != 0
                       ? s->draw_order : 0;
    }
    for (int visit = 0; visit < 16 * ordered; ++visit) {
        int i = visit % ordered;
        if (order[i] != visit / ordered + 1) continue;
        MzmSprite *s = &sprites[i];
        uint16_t top = (uint16_t)(s->y + s->hitbox_top), bottom = (uint16_t)(s->y + s->hitbox_bottom);
        uint16_t left = (uint16_t)(s->x + s->hitbox_left), right = (uint16_t)(s->x + s->hitbox_right);
        uint8_t weakness = weakness_of(s, tables);
        for (int p = 0; p < MZM_MAX_PROJECTILES; ++p) {
            MzmProjectile *proj = &weapons->list[p];
            /* A tumbling missile no longer has PROJ_STATUS_CAN_AFFECT_ENVIRONMENT. */
            if (!proj->active || proj->type == MZM_PROJECTILE_POWER_BOMB ||
                proj->stage == MZM_STAGE_TUMBLING)
                continue;
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
                } else if (s->solid_for_projectiles) {
                    hit_solid(s);
                } else if (weakness & MZM_WEAKNESS_BEAM_BOMBS) {
                    deal_damage(s, BOMB_DAMAGE);
                } else {
                    hit_solid(s);
                }
                continue;
            }
            /* The hitboxes set by each Projectile*Process* routine. */
            int16_t ptop, pbottom, pleft, pright;
            mzm_projectile_hitbox(proj->type, proj->direction, weapons->items, &ptop, &pbottom,
                                  &pleft, &pright);
            uint16_t py = (uint16_t)proj->y, px = (uint16_t)proj->x;
            if (!touching(top, bottom, left, right, (uint16_t)(py + ptop), (uint16_t)(py + pbottom),
                          (uint16_t)(px + pleft), (uint16_t)(px + pright)))
                continue;
            projectile_hit(s, proj, weapons, tables, effects, weakness, py, px);
        }
    }
    for (int p = 0; p < MZM_MAX_PROJECTILES; ++p) weapons->list[p].hits_sprites = false;
}

bool mzm_sprite_supported(uint8_t sprite_id) {
    return mzm_sprite_family(sprite_id, false) >= 0;
}

uint8_t mzm_sprite_graphics_owner(const MzmSprite *sprite) {
    if (!sprite->secondary) {
        /* The Tourian defences' second ids share their first's graphics. */
        if (sprite->sprite_id == MZM_PSPRITE_ZEBETITE_TWO_AND_FOUR) return MZM_PSPRITE_ZEBETITE_ONE_AND_THREE;
        if (sprite->sprite_id == MZM_PSPRITE_FROZEN_METROID) return MZM_PSPRITE_METROID;
        if (sprite->sprite_id == MZM_PSPRITE_CANNON2 || sprite->sprite_id == MZM_PSPRITE_CANNON3)
            return MZM_PSPRITE_CANNON;
        if (sprite->sprite_id == MZM_PSPRITE_DEOREM_SECOND_LOCATION)
            return MZM_PSPRITE_DEOREM_FIRST_LOCATION;
        /* sHiveGfx: the Mellows and their swarms. */
        if (mzm_sprite_family(sprite->sprite_id, false) == MZM_FAMILY_HIVE) return MZM_PSPRITE_HIVE;
        if (mzm_sprite_family(sprite->sprite_id, false) == MZM_FAMILY_BARISTUTE) return MZM_PSPRITE_BARISTUTE;
        /* sSpacePirateGfx; the event trigger is never drawn. */
        if (sprite->sprite_id == MZM_PSPRITE_SPACE_PIRATE_CARRYING_POWER_BOMB ||
            sprite->sprite_id == MZM_PSPRITE_BLACK_SPACE_PIRATE ||
            sprite->sprite_id == MZM_PSPRITE_ESCAPE_SHIP_SPACE_PIRATE)
            return MZM_PSPRITE_SPACE_PIRATE;
        /* sElevatorStatuesGfx */
        if (sprite->sprite_id == MZM_PSPRITE_RIDLEY_ELEVATOR_STATUE) return MZM_PSPRITE_KRAID_ELEVATOR_STATUE;
        /* sEscapeGateAndTimerGfx, exported with the first gate. */
        if (sprite->sprite_id == MZM_PSPRITE_ESCAPE_GATE2) return MZM_PSPRITE_ESCAPE_GATE1;
        if (mzm_sprite_family(sprite->sprite_id, false) == MZM_FAMILY_TANGLE_VINE)
            return MZM_PSPRITE_TANGLE_VINE_TALL;
        /* sRinkaZebetiteAndCannonGfx, exported with the cannon's tables. */
        if (mzm_sprite_family(sprite->sprite_id, false) == MZM_FAMILY_RINKA &&
            sprite->sprite_id != MZM_PSPRITE_RINKA_ORANGE && sprite->sprite_id != MZM_PSPRITE_RINKA_GREEN)
            return MZM_PSPRITE_CANNON;
        return sprite->sprite_id;
    }
    if (sprite->sprite_id == MZM_SSPRITE_ZEELA_EYES) return MZM_PSPRITE_ZEELA_RED;
    if (sprite->sprite_id == MZM_SSPRITE_MORPH_BALL_OUTSIDE) return MZM_PSPRITE_MORPH_BALL;
    if (sprite->sprite_id == MZM_SSPRITE_DRAGON_FIREBALL) return MZM_PSPRITE_DRAGON;
    if (sprite->sprite_id == MZM_SSPRITE_POWER_GRIP_GLOW) return MZM_PSPRITE_POWER_GRIP;
    if (sprite->sprite_id == MZM_SSPRITE_CHARGE_BEAM_GLOW) return MZM_PSPRITE_CHARGE_BEAM;
    if (sprite->sprite_id == MZM_SSPRITE_SPACE_PIRATE_LASER) return MZM_PSPRITE_SPACE_PIRATE;
    if (sprite->sprite_id == MZM_SSPRITE_ATOMIC_ELECTRICITY) return MZM_PSPRITE_ATOMIC;
    if (sprite->sprite_id == MZM_SSPRITE_POLYP_PROJECTILE) return MZM_PSPRITE_POLYP;
    if (sprite->sprite_id == MZM_SSPRITE_CANNON_BULLET) return MZM_PSPRITE_CANNON;
    if (sprite->sprite_id == MZM_SSPRITE_METROID_SHELL) return MZM_PSPRITE_METROID;
    if (sprite->sprite_id == MZM_SSPRITE_KRAID_PART || sprite->sprite_id == MZM_SSPRITE_KRAID_SPIKE ||
        sprite->sprite_id == MZM_SSPRITE_KRAID_NAIL)
        return MZM_PSPRITE_KRAID;
    if (mzm_sprite_family(sprite->sprite_id, true) == MZM_FAMILY_RIDLEY) return MZM_PSPRITE_RIDLEY;
    /* The block uses SPRITE_GFX_SLOT_SPECIAL (the cannons' graphics). */
    if (sprite->sprite_id == MZM_SSPRITE_MOTHER_BRAIN_BLOCK) return MZM_PSPRITE_CANNON;
    if (mzm_sprite_family(sprite->sprite_id, true) == MZM_FAMILY_MOTHER_BRAIN) return MZM_PSPRITE_MOTHER_BRAIN;
    if (mzm_sprite_family(sprite->sprite_id, true) == MZM_FAMILY_DEOREM)
        return MZM_PSPRITE_DEOREM_FIRST_LOCATION;
    if (mzm_sprite_family(sprite->sprite_id, true) == MZM_FAMILY_ACID_WORM)
        return MZM_PSPRITE_ACID_WORM;
    if (mzm_sprite_family(sprite->sprite_id, true) == MZM_FAMILY_GADORA)
        return sprite->absolute_palette_row == 1 ? MZM_PSPRITE_GADORA_RIDLEY :
                                                   MZM_PSPRITE_GADORA_KRAID;
    if (sprite->sprite_id == MZM_SSPRITE_IMAGO_LARVA_PART) return sprite->owner;
    if (sprite->sprite_id == MZM_SSPRITE_DEFEATED_IMAGO_COCOON ||
        sprite->sprite_id == MZM_SSPRITE_IMAGO_CEILING_VINE ||
        sprite->sprite_id == MZM_SSPRITE_WINGED_RIPPER ||
        sprite->sprite_id == MZM_SSPRITE_IMAGO_COCOON_VINE ||
        sprite->sprite_id == MZM_SSPRITE_IMAGO_COCOON_SPORE)
        return MZM_PSPRITE_IMAGO_COCOON;
    if (mzm_sprite_family(sprite->sprite_id, true) == MZM_FAMILY_MECHA_RIDLEY)
        return MZM_PSPRITE_MECHA_RIDLEY;
    /* The Chozo statue parts and ball, the map station's screens and the
     * unknown item block's light use their spawner's graphics. */
    if (mzm_sprite_family(sprite->sprite_id, true) == MZM_FAMILY_CHOZO_STATUE ||
        sprite->sprite_id == MZM_SSPRITE_MAP_STATION_PART || sprite->sprite_id == MZM_SSPRITE_SAVE_PLATFORM_PART ||
        sprite->sprite_id == MZM_SSPRITE_GUNSHIP_PART || sprite->sprite_id == MZM_SSPRITE_MORPH_BALL_LAUNCHER_PART ||
        sprite->sprite_id == MZM_SSPRITE_ESCAPE_SHIP_PART ||
        sprite->sprite_id == MZM_SSPRITE_ZIPLINE_GENERATOR_PART ||
        sprite->sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM ||
        sprite->sprite_id == MZM_SSPRITE_CHOZO_PILLAR_PLATFORM_SHADOW ||
        sprite->sprite_id == MZM_SSPRITE_CHOZODIA_SAVE_PLATFORM_PART ||
        sprite->sprite_id == MZM_SSPRITE_UNKNOWN_ITEM_BLOCK_LIGHT)
        return sprite->owner;
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

uint8_t mzm_sprite_bg_priority(const MzmSprite *sprite) {
    if (sprite->bg_priority_override)
        return (uint8_t)(sprite->bg_priority_override - 1);
    return sprite->low_bg_priority ? 3 : 2;
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
    sprite->draw_order = 4;
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
    if (s->absolute) return;
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
    case MZM_FAMILY_GAMET: return MZM_SOUND_GAMET_DAMAGED;
    case MZM_FAMILY_REO: return MZM_SOUND_REO_DAMAGED;
    case MZM_FAMILY_MELLA: return MZM_SOUND_MELLA_DAMAGED;
    case MZM_FAMILY_HOLTZ: return MZM_SOUND_HOLTZ_DAMAGED;
    case MZM_FAMILY_MULTIVIOLA: return MZM_SOUND_MULTIVIOLA_DAMAGED;
    case MZM_FAMILY_SKREE: return MZM_SOUND_SKREE_DAMAGED;
    case MZM_FAMILY_GERUTA: return MZM_SOUND_GERUTA_DAMAGED;
    case MZM_FAMILY_RIPPER2: return MZM_SOUND_RIPPER2_DAMAGED;
    case MZM_FAMILY_SQUEEPT: return MZM_SOUND_SQUEEPT_DAMAGED;
    case MZM_FAMILY_SKULTERA: return MZM_SOUND_SKULTERA_DAMAGED;
    case MZM_FAMILY_DRAGON: return MZM_SOUND_DRAGON_DAMAGED;
    case MZM_FAMILY_ATOMIC: return MZM_SOUND_ATOMIC_DAMAGED;
    default: return 0;
    }
}

void mzm_sprite_update(MzmSprite *sprite, uint8_t slot, const MzmSpriteFrame *frame) {
    int family = mzm_sprite_family(sprite->sprite_id, sprite->secondary);
    /* A family without frame durations (not exported) is not updated. */
    if (!(sprite->status & MZM_SPRITE_STATUS_EXISTS) || family < 0 || !frame->kinds[family])
        return;
    /* The Space Pirates and the security systems need the game globals. */
    if ((family == MZM_FAMILY_SPACE_PIRATE || family == MZM_FAMILY_CHOZODIA_SECURITY ||
         family == MZM_FAMILY_CHOZO_STATUE || family == MZM_FAMILY_IMAGO_LARVA ||
         family == MZM_FAMILY_IMAGO_COCOON) &&
        !frame->globals)
        return;
    Ctx c = {sprite, frame, frame->world, frame->tables, frame->kinds[family],
             0, MZM_COLLISION_AIR, frame->frame8};
    if (frame->stopped && sprite->pose != SPRITE_POSE_UNINITIALIZED && !sprite->always_active) {
        check_on_screen(&c);
        return;
    }
    /* gSpriteRng = sSpriteRandomNumberTable[(frame8 + slot + frame16 / 16 +
     * x + y) % 32] */
    c.rng = c.tables->random[((uint32_t)frame->frame8 + slot + frame->frame16 / 16u + sprite->x +
                              sprite->y) % 32u];
    /* SpriteUtilUpdateStunTimer: the stun row every other 4 frames, then
     * back to the frozen or the sprite's own row. */
    if (sprite->isft & 0x7F) {
        uint8_t isft = --sprite->isft;
        if (!(isft & 3) && sprite->health) {
            if (isft & 4) sprite->palette_row = (uint8_t)(14 - sprite->gfx_slot - sprite->frozen_row_offset);
            else if (sprite->freeze_timer)
                sprite->palette_row = (uint8_t)(15 - sprite->gfx_slot - sprite->frozen_row_offset);
            else sprite->palette_row = sprite->absolute_palette_row;
        }
    }
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
    case MZM_FAMILY_ZEBBO:
    case MZM_FAMILY_GAMET: pipe_bug(&c); break;
    case MZM_FAMILY_REO: reo(&c); break;
    case MZM_FAMILY_MELLA: mella(&c); break;
    case MZM_FAMILY_HOLTZ: holtz(&c); break;
    case MZM_FAMILY_MULTIVIOLA: multiviola(&c); break;
    case MZM_FAMILY_GERUTA: geruta(&c); break;
    case MZM_FAMILY_RIPPER2: ripper2(&c); break;
    case MZM_FAMILY_SQUEEPT: squeept(&c); break;
    case MZM_FAMILY_ITEM_SPRITE: ai_item_sprite(&c); break;
    case MZM_FAMILY_SKULTERA: skultera(&c); break;
    case MZM_FAMILY_GADGET: ai_gadget(&c); break;
    case MZM_FAMILY_DRAGON:
        if (sprite->secondary) dragon_fireball(&c);
        else dragon(&c);
        break;
    case MZM_FAMILY_RINKA: rinka(&c); break;
    case MZM_FAMILY_SPACE_PIRATE:
        if (sprite->secondary) ai_space_pirate_laser(&c);
        else if (sprite->sprite_id == MZM_PSPRITE_BLACK_SPACE_PIRATE) ai_black_space_pirate(&c);
        else ai_space_pirate(&c);
        break;
    case MZM_FAMILY_CHOZODIA_SECURITY: ai_chozodia_security(&c); break;
    case MZM_FAMILY_PARASITE: ai_parasite(&c); break;
    case MZM_FAMILY_ATOMIC: ai_atomic(&c); break;
    case MZM_FAMILY_POLYP: ai_polyp(&c); break;
    case MZM_FAMILY_TOURIAN: ai_tourian(&c); break;
    case MZM_FAMILY_WORKER_ROBOT: ai_worker_robot(&c); break;
    case MZM_FAMILY_METROID: ai_metroid(&c); break;
    case MZM_FAMILY_KRAID: ai_kraid(&c); break;
    case MZM_FAMILY_RIDLEY: ai_ridley(&c); break;
    case MZM_FAMILY_MOTHER_BRAIN: ai_mother_brain(&c); break;
    case MZM_FAMILY_DEOREM: ai_deorem(&c); break;
    case MZM_FAMILY_ACID_WORM: ai_acid_worm(&c); break;
    case MZM_FAMILY_GADORA: ai_gadora(&c); break;
    case MZM_FAMILY_IMAGO_LARVA: ai_imago_larva(&c); break;
    case MZM_FAMILY_IMAGO_LARVA_RIGHT_SIDE: ai_imago_larva_right_side(&c); break;
    case MZM_FAMILY_IMAGO_COCOON: ai_imago_cocoon(&c); break;
    case MZM_FAMILY_MECHA_RIDLEY: ai_mecha_ridley(&c); break;
    case MZM_FAMILY_HIVE: ai_hive(&c); break;
    case MZM_FAMILY_BARISTUTE: ai_baristute(&c); break;
    case MZM_FAMILY_TANGLE_VINE: ai_tangle_vine(&c); break;
    case MZM_FAMILY_RUINS_TEST: ai_ruins_test(&c); break;
    case MZM_FAMILY_CHOZODIA_SCRIPTED: ai_chozodia_scripted(&c); break;
    case MZM_FAMILY_ELEVATOR_STATUE: ai_elevator_statue(&c); break;
    case MZM_FAMILY_IMAGO: ai_imago(&c); break;
    case MZM_FAMILY_ESCAPE_SHIP: ai_escape_ship(&c); break;
    case MZM_FAMILY_BOSS_STATUE: boss_statue(&c); break;
    case MZM_FAMILY_CHOZO_STATUE: ai_chozo_statue(&c); break;
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
        if (frame->standing) samus_standing_on_sprite(sprite, frame->standing);
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
