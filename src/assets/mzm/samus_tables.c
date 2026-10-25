/* SPDX-License-Identifier: GPL-3.0-only */
/* The Samus graphics pointer tables of the Zero Mission decompilation
 * (third_party/mzm/src/data/samus/samus_animation_pointers.c), each row's
 * designated selector and its right/left symbols in source order, and the
 * projectile FrameData tables of src/data/projectile_data.c, excluding
 * particles and unused data. Rows without a symbol are omitted, as the
 * native selection never reads them. Addresses come from the ELF. */
#include "samus_tables.h"

static const FusionMzmSamusTableRow rows_0[] = {
    {"SPOSE_RUNNING", 2, {
        "sSamusAnim_PowerSuit_Right_Running",
        "sSamusAnim_PowerSuit_Left_Running"}},
    {"SPOSE_STANDING", 2, {
        "sSamusAnim_PowerSuit_Right_Standing",
        "sSamusAnim_PowerSuit_Left_Standing"}},
    {"SPOSE_TURNING_AROUND", 2, {
        "sSamusAnim_PowerSuit_Right_TurningAround",
        "sSamusAnim_PowerSuit_Left_TurningAround"}},
    {"SPOSE_SHOOTING", 2, {
        "sSamusAnim_PowerSuit_Right_Shooting",
        "sSamusAnim_PowerSuit_Left_Shooting"}},
    {"SPOSE_CROUCHING", 2, {
        "sSamusAnim_PowerSuit_Right_Crouching",
        "sSamusAnim_PowerSuit_Left_Crouching"}},
    {"SPOSE_TURNING_AROUND_AND_CROUCHING", 2, {
        "sSamusAnim_PowerSuit_Right_TurningAroundAndCrouching",
        "sSamusAnim_PowerSuit_Left_TurningAroundAndCrouching"}},
    {"SPOSE_SHOOTING_AND_CROUCHING", 2, {
        "sSamusAnim_PowerSuit_Right_ShootingAndCrouching",
        "sSamusAnim_PowerSuit_Left_ShootingAndCrouching"}},
    {"SPOSE_SKIDDING", 2, {
        "sSamusAnim_PowerSuit_Right_Skidding",
        "sSamusAnim_PowerSuit_Left_Skidding"}},
    {"SPOSE_MIDAIR", 2, {
        "sSamusAnim_PowerSuit_Right_MidAir",
        "sSamusAnim_PowerSuit_Left_MidAir"}},
    {"SPOSE_TURNING_AROUND_MIDAIR", 2, {
        "sSamusAnim_PowerSuit_Right_TurningAroundMidAir",
        "sSamusAnim_PowerSuit_Left_TurningAroundMidAir"}},
    {"SPOSE_LANDING", 2, {
        "sSamusAnim_PowerSuit_Right_Landing",
        "sSamusAnim_PowerSuit_Left_Landing"}},
    {"SPOSE_STARTING_SPIN_JUMP", 2, {
        "sSamusAnim_PowerSuit_Right_StartingSpinJump",
        "sSamusAnim_PowerSuit_Left_StartingSpinJump"}},
    {"SPOSE_SPINNING", 2, {
        "sSamusAnim_PowerSuit_Right_Spinning",
        "sSamusAnim_PowerSuit_Left_Spinning"}},
    {"SPOSE_STARTING_WALL_JUMP", 2, {
        "sSamusAnim_PowerSuit_Right_StartingWallJump",
        "sSamusAnim_PowerSuit_Left_StartingWallJump"}},
    {"SPOSE_SPACE_JUMPING", 2, {
        "sSamusAnim_PowerSuit_Right_SpaceJumping",
        "sSamusAnim_PowerSuit_Left_SpaceJumping"}},
    {"SPOSE_SCREW_ATTACKING", 2, {
        "sSamusAnim_PowerSuit_Right_ScrewAttacking",
        "sSamusAnim_PowerSuit_Left_ScrewAttacking"}},
    {"SPOSE_MORPHING", 2, {
        "sSamusAnim_PowerSuit_Right_Morphing",
        "sSamusAnim_PowerSuit_Left_Morphing"}},
    {"SPOSE_MORPH_BALL", 2, {
        "sSamusAnim_PowerSuit_Right_Morphball",
        "sSamusAnim_PowerSuit_Left_Morphball"}},
    {"SPOSE_ROLLING", 2, {
        "sSamusAnim_PowerSuit_Right_Morphball",
        "sSamusAnim_PowerSuit_Left_Morphball"}},
    {"SPOSE_UNMORPHING", 2, {
        "sSamusAnim_PowerSuit_Right_Unmorphing",
        "sSamusAnim_PowerSuit_Left_Unmorphing"}},
    {"SPOSE_MORPH_BALL_MIDAIR", 2, {
        "sSamusAnim_PowerSuit_Right_Morphball",
        "sSamusAnim_PowerSuit_Left_Morphball"}},
    {"SPOSE_HANGING_ON_LEDGE", 2, {
        "sSamusAnim_PowerSuit_Right_HangingOnLedge",
        "sSamusAnim_PowerSuit_Left_HangingOnLedge"}},
    {"SPOSE_TURNING_TO_AIM_WHILE_HANGING", 2, {
        "sSamusAnim_PowerSuit_Right_TurningToAimWhileHanging",
        "sSamusAnim_PowerSuit_Left_TurningToAimWhileHanging"}},
    {"SPOSE_HIDING_ARM_CANNON_WHILE_HANGING", 2, {
        "sSamusAnim_PowerSuit_Right_TurningToAimWhileHanging",
        "sSamusAnim_PowerSuit_Left_TurningToAimWhileHanging"}},
    {"SPOSE_AIMING_WHILE_HANGING", 2, {
        "sSamusAnim_PowerSuit_Right_AimingWhileHanging",
        "sSamusAnim_PowerSuit_Left_AimingWhileHanging"}},
    {"SPOSE_SHOOTING_WHILE_HANGING", 2, {
        "sSamusAnim_PowerSuit_Right_ShootingWhileHanging",
        "sSamusAnim_PowerSuit_Left_ShootingWhileHanging"}},
    {"SPOSE_PULLING_YOURSELF_UP_FROM_HANGING", 2, {
        "sSamusAnim_PowerSuit_Right_PullingYourselfUpFromHanging",
        "sSamusAnim_PowerSuit_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING", 2, {
        "sSamusAnim_PowerSuit_Right_PullingYourselfForwardFromHanging",
        "sSamusAnim_PowerSuit_Left_PullingYourselfForwardFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL", 2, {
        "sSamusAnim_PowerSuit_Right_PullingYourselfUpFromHanging",
        "sSamusAnim_PowerSuit_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_USING_AN_ELEVATOR", 2, {
        "sSamusAnim_PowerSuit_UsingAnElevator",
        "sSamusAnim_PowerSuit_UsingAnElevator"}},
    {"SPOSE_FACING_THE_FOREGROUND", 2, {
        "sSamusAnim_PowerSuit_FacingTheForeground",
        "sSamusAnim_PowerSuit_FacingTheForeground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_FOREGROUND", 2, {
        "sSamusAnim_PowerSuit_Right_TurningFromFacingTheForeground",
        "sSamusAnim_PowerSuit_Left_TurningFromFacingTheForeground"}},
    {"SPOSE_GRABBED_BY_CHOZO_STATUE", 2, {
        "sSamusAnim_PowerSuit_MorphballMotionless",
        "sSamusAnim_PowerSuit_MorphballMotionless"}},
    {"SPOSE_DELAY_BEFORE_SHINESPARKING", 2, {
        "sSamusAnim_PowerSuit_Right_DelayBeforeShinesparking",
        "sSamusAnim_PowerSuit_Left_DelayBeforeShinesparking"}},
    {"SPOSE_SHINESPARKING", 2, {
        "sSamusAnim_PowerSuit_Right_Shinesparking",
        "sSamusAnim_PowerSuit_Left_Shinesparking"}},
    {"SPOSE_SHINESPARK_COLLISION", 2, {
        "sSamusAnim_PowerSuit_Right_Shinesparking",
        "sSamusAnim_PowerSuit_Left_Shinesparking"}},
    {"SPOSE_DELAY_AFTER_SHINESPARKING", 2, {
        "sSamusAnim_PowerSuit_Right_DelayAfterShinesparking",
        "sSamusAnim_PowerSuit_Left_DelayAfterShinesparking"}},
    {"SPOSE_DELAY_BEFORE_BALLSPARKING", 2, {
        "sSamusAnim_PowerSuit_MorphballMotionless",
        "sSamusAnim_PowerSuit_MorphballMotionless"}},
    {"SPOSE_BALLSPARKING", 2, {
        "sSamusAnim_PowerSuit_Right_Ballsparking",
        "sSamusAnim_PowerSuit_Left_Ballsparking"}},
    {"SPOSE_BALLSPARK_COLLISION", 2, {
        "sSamusAnim_PowerSuit_Right_Morphball",
        "sSamusAnim_PowerSuit_Left_Morphball"}},
    {"SPOSE_ON_ZIPLINE", 2, {
        "sSamusAnim_PowerSuit_Right_OnZipline",
        "sSamusAnim_PowerSuit_Left_OnZipline"}},
    {"SPOSE_SHOOTING_ON_ZIPLINE", 2, {
        "sSamusAnim_PowerSuit_Right_ShootingOnZipline",
        "sSamusAnim_PowerSuit_Left_ShootingOnZipline"}},
    {"SPOSE_TURNING_ON_ZIPLINE", 2, {
        "sSamusAnim_PowerSuit_Right_TurningOnZipline",
        "sSamusAnim_PowerSuit_Left_TurningOnZipline"}},
    {"SPOSE_MORPH_BALL_ON_ZIPLINE", 2, {
        "sSamusAnim_PowerSuit_MorphballMotionless",
        "sSamusAnim_PowerSuit_MorphballMotionless"}},
    {"SPOSE_SAVING_LOADING_GAME", 2, {
        "sSamusAnim_PowerSuit_SavingLoadingGame",
        "sSamusAnim_PowerSuit_SavingLoadingGame"}},
    {"SPOSE_DOWNLOADING_MAP_DATA", 2, {
        "sSamusAnim_PowerSuit_DownloadingMapData",
        "sSamusAnim_PowerSuit_DownloadingMapData"}},
    {"SPOSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA", 2, {
        "sSamusAnim_PowerSuit_Right_TurningAround",
        "sSamusAnim_PowerSuit_Left_TurningAround"}},
    {"SPOSE_GETTING_HURT", 2, {
        "sSamusAnim_PowerSuit_Right_GettingKnockedBack",
        "sSamusAnim_PowerSuit_Left_GettingKnockedBack"}},
    {"SPOSE_GETTING_KNOCKED_BACK", 2, {
        "sSamusAnim_PowerSuit_Right_GettingKnockedBack",
        "sSamusAnim_PowerSuit_Left_GettingKnockedBack"}},
    {"SPOSE_GETTING_HURT_IN_MORPH_BALL", 2, {
        "sSamusAnim_PowerSuit_Right_Morphball",
        "sSamusAnim_PowerSuit_Left_Morphball"}},
    {"SPOSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL", 2, {
        "sSamusAnim_PowerSuit_Right_Morphball",
        "sSamusAnim_PowerSuit_Left_Morphball"}},
    {"SPOSE_DYING", 2, {
        "sSamusAnim_PowerSuit_Right_Dying",
        "sSamusAnim_PowerSuit_Left_Dying"}},
    {"SPOSE_CROUCHING_TO_CRAWL", 2, {
        "sSamusAnim_Suitless_Right_CrouchingToCrawl",
        "sSamusAnim_Suitless_Left_CrouchingToCrawl"}},
    {"SPOSE_CRAWLING_STOPPED", 2, {
        "sSamusAnim_Suitless_Right_CrawlingStopped",
        "sSamusAnim_Suitless_Left_CrawlingStopped"}},
    {"SPOSE_STARTING_TO_CRAWL", 2, {
        "sSamusAnim_Suitless_Right_StartingToCrawl",
        "sSamusAnim_Suitless_Left_StartingToCrawl"}},
    {"SPOSE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_Crawling",
        "sSamusAnim_Suitless_Left_Crawling"}},
    {"SPOSE_UNCROUCHING_FROM_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_UncrouchingFromCrawling",
        "sSamusAnim_Suitless_Left_UncrouchingFromCrawling"}},
    {"SPOSE_TURNING_AROUND_WHILE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_TurningAroundWhileCrawling",
        "sSamusAnim_Suitless_Left_TurningAroundWhileCrawling"}},
    {"SPOSE_SHOOTING_WHILE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_ShootingWhileCrawling",
        "sSamusAnim_Suitless_Left_ShootingWhileCrawling"}},
    {"SPOSE_UNCROUCHING_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_UncrouchingSuitless",
        "sSamusAnim_Suitless_Left_UncrouchingSuitless"}},
    {"SPOSE_CROUCHING_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_CrouchingSuitless",
        "sSamusAnim_Suitless_Left_CrouchingSuitless"}},
    {"SPOSE_GRABBING_A_LEDGE_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_HangingOnLedgeSuitless",
        "sSamusAnim_Suitless_Left_HangingOnLedgeSuitless"}},
    {"SPOSE_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sSamusAnim_Suitless_FacingTheBackground",
        "sSamusAnim_Suitless_FacingTheBackground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_TurningFromFacingTheBackground",
        "sSamusAnim_Suitless_Left_TurningFromFacingTheBackground"}},
    {"SPOSE_ACTIVATING_ZIPLINES", 2, {
        "sSamusAnim_PowerSuit_MorphballMotionless",
        "sSamusAnim_PowerSuit_MorphballMotionless"}},
    {"SPOSE_IN_ESCAPE_SHIP", 2, {
        "sSamusAnim_PowerSuit_Right_Crouching",
        "sSamusAnim_PowerSuit_Left_Crouching"}},
    {"SPOSE_TURNING_TO_ENTER_ESCAPE_SHIP", 2, {
        "sSamusAnim_PowerSuit_Right_TurningAround",
        "sSamusAnim_PowerSuit_Left_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_1[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_Forward_Running",
        "sSamusAnim_PowerSuit_Left_Forward_Running"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_Running",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_Running"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_Running",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_Running"}},
    {"ACD_NONE", 2, {
        "sSamusAnim_PowerSuit_Right_Running",
        "sSamusAnim_PowerSuit_Left_Running"}},
};
static const FusionMzmSamusTableRow rows_2[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_Forward_Running_Speedboosting",
        "sSamusAnim_PowerSuit_Left_Forward_Running_Speedboosting"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_Running_Speedboosting",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_Running_Speedboosting"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_Running_Speedboosting",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_Running_Speedboosting"}},
    {"ACD_NONE", 2, {
        "sSamusAnim_PowerSuit_Right_Running_Speedboosting",
        "sSamusAnim_PowerSuit_Left_Running_Speedboosting"}},
};
static const FusionMzmSamusTableRow rows_3[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_Standing",
        "sSamusAnim_PowerSuit_Left_Standing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_Standing",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_Standing",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_Standing"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_Up_Standing",
        "sSamusAnim_PowerSuit_Left_Up_Standing"}},
};
static const FusionMzmSamusTableRow rows_4[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_TurningAround",
        "sSamusAnim_PowerSuit_Left_TurningAround"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_TurningAround",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_TurningAround"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_TurningAround",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_TurningAround"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_Up_TurningAround",
        "sSamusAnim_PowerSuit_Left_Up_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_5[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_Shooting",
        "sSamusAnim_PowerSuit_Left_Shooting"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_Shooting",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_Shooting"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_Shooting",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_Shooting"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_Up_Shooting",
        "sSamusAnim_PowerSuit_Left_Up_Shooting"}},
};
static const FusionMzmSamusTableRow rows_6[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_Crouching",
        "sSamusAnim_PowerSuit_Left_Crouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_Crouching",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_Crouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_Crouching",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_Crouching"}},
};
static const FusionMzmSamusTableRow rows_7[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_TurningAroundAndCrouching",
        "sSamusAnim_PowerSuit_Left_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_TurningAroundAndCrouching",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_TurningAroundAndCrouching",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_TurningAroundAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_8[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_ShootingAndCrouching",
        "sSamusAnim_PowerSuit_Left_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_ShootingAndCrouching",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_ShootingAndCrouching",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_ShootingAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_9[] = {
    {"FALSE", 2, {
        "sSamusAnim_PowerSuit_Right_Skidding",
        "sSamusAnim_PowerSuit_Left_Skidding"}},
    {"TRUE", 2, {
        "sSamusAnim_PowerSuit_Right_Armed_Skidding",
        "sSamusAnim_PowerSuit_Left_Armed_Skidding"}},
};
static const FusionMzmSamusTableRow rows_10[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_MidAir",
        "sSamusAnim_PowerSuit_Left_MidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_MidAir",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_MidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_MidAir",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_MidAir"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_Up_MidAir",
        "sSamusAnim_PowerSuit_Left_Up_MidAir"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_Down_MidAir",
        "sSamusAnim_PowerSuit_Left_Down_MidAir"}},
};
static const FusionMzmSamusTableRow rows_11[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_TurningAroundMidAir",
        "sSamusAnim_PowerSuit_Left_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_TurningAroundMidAir",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_TurningAroundMidAir",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_TurningAroundMidAir"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_Up_TurningAroundMidAir",
        "sSamusAnim_PowerSuit_Left_Up_TurningAroundMidAir"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_Down_TurningAroundMidAir",
        "sSamusAnim_PowerSuit_Left_Down_TurningAroundMidAir"}},
};
static const FusionMzmSamusTableRow rows_12[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_Landing",
        "sSamusAnim_PowerSuit_Left_Landing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_Landing",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_Landing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_Landing",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_Landing"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_Up_Landing",
        "sSamusAnim_PowerSuit_Left_Up_Landing"}},
};
static const FusionMzmSamusTableRow rows_13[] = {
    {"FALSE", 2, {
        "sSamusAnim_PowerSuit_Right_ScrewAttacking",
        "sSamusAnim_PowerSuit_Left_ScrewAttacking"}},
    {"TRUE", 2, {
        "sSamusAnim_PowerSuit_Right_SpaceJumping",
        "sSamusAnim_PowerSuit_Left_SpaceJumping"}},
};
static const FusionMzmSamusTableRow rows_14[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_AimingWhileHanging",
        "sSamusAnim_PowerSuit_Left_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_AimingWhileHanging",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_AimingWhileHanging",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_AimingWhileHanging"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_Up_AimingWhileHanging",
        "sSamusAnim_PowerSuit_Left_Up_AimingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_Down_AimingWhileHanging",
        "sSamusAnim_PowerSuit_Left_Down_AimingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_15[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_ShootingWhileHanging",
        "sSamusAnim_PowerSuit_Left_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalUp_ShootingWhileHanging",
        "sSamusAnim_PowerSuit_Left_DiagonalUp_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_ShootingWhileHanging",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_ShootingWhileHanging"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_Up_ShootingWhileHanging",
        "sSamusAnim_PowerSuit_Left_Up_ShootingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_Down_ShootingWhileHanging",
        "sSamusAnim_PowerSuit_Left_Down_ShootingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_16[] = {
    {"FALSE", 2, {
        "sSamusAnim_PowerSuit_UsingAnElevator",
        "sSamusAnim_PowerSuit_UsingAnElevator"}},
    {"TRUE", 2, {
        "sSamusAnim_PowerSuit_GoingUp_UsingAnElevator",
        "sSamusAnim_PowerSuit_GoingUp_UsingAnElevator"}},
};
static const FusionMzmSamusTableRow rows_17[] = {
    {"FORCED_MOVEMENT_UPWARDS_SHINESPARK", 2, {
        "sSamusAnim_PowerSuit_Right_Shinesparking",
        "sSamusAnim_PowerSuit_Left_Shinesparking"}},
    {"FORCED_MOVEMENT_SIDEWARDS_SHINESPARK", 2, {
        "sSamusAnim_PowerSuit_Right_Sidewards_Shinesparking",
        "sSamusAnim_PowerSuit_Left_Sidewards_Shinesparking"}},
    {"FORCED_MOVEMENT_DIAGONAL_SHINESPARK", 2, {
        "sSamusAnim_PowerSuit_Right_Sidewards_Shinesparking",
        "sSamusAnim_PowerSuit_Left_Sidewards_Shinesparking"}},
};
static const FusionMzmSamusTableRow rows_18[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_OnZipline",
        "sSamusAnim_PowerSuit_Left_OnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_OnZipline",
        "sSamusAnim_PowerSuit_Left_OnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_OnZipline",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_OnZipline"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_OnZipline",
        "sSamusAnim_PowerSuit_Left_OnZipline"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_Down_OnZipline",
        "sSamusAnim_PowerSuit_Left_Down_OnZipline"}},
};
static const FusionMzmSamusTableRow rows_19[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_ShootingOnZipline",
        "sSamusAnim_PowerSuit_Left_ShootingOnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_ShootingOnZipline",
        "sSamusAnim_PowerSuit_Left_ShootingOnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_ShootingOnZipline",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_ShootingOnZipline"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_ShootingOnZipline",
        "sSamusAnim_PowerSuit_Left_ShootingOnZipline"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_Down_ShootingOnZipline",
        "sSamusAnim_PowerSuit_Left_Down_ShootingOnZipline"}},
};
static const FusionMzmSamusTableRow rows_20[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_PowerSuit_Right_TurningOnZipline",
        "sSamusAnim_PowerSuit_Left_TurningOnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_PowerSuit_Right_TurningOnZipline",
        "sSamusAnim_PowerSuit_Left_TurningOnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_DiagonalDown_TurningOnZipline",
        "sSamusAnim_PowerSuit_Left_DiagonalDown_TurningOnZipline"}},
    {"ACD_UP", 2, {
        "sSamusAnim_PowerSuit_Right_TurningOnZipline",
        "sSamusAnim_PowerSuit_Left_TurningOnZipline"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_PowerSuit_Right_Down_TurningOnZipline",
        "sSamusAnim_PowerSuit_Left_Down_TurningOnZipline"}},
};
static const FusionMzmSamusTableRow rows_21[] = {
    {"SPOSE_RUNNING", 2, {
        "sArmCannonAnim_Suit_Right_None_Running",
        "sArmCannonAnim_Suit_Left_None_Running"}},
    {"SPOSE_STANDING", 2, {
        "sArmCannonAnim_Suit_Right_Standing",
        "sArmCannonAnim_Suit_Left_Standing"}},
    {"SPOSE_TURNING_AROUND", 2, {
        "sArmCannonAnim_Suit_Right_TurningAround",
        "sArmCannonAnim_Suit_Left_TurningAround"}},
    {"SPOSE_SHOOTING", 2, {
        "sArmCannonAnim_Suit_Right_Shooting",
        "sArmCannonAnim_Suit_Left_Shooting"}},
    {"SPOSE_CROUCHING", 2, {
        "sArmCannonAnim_Suit_Right_Crouching",
        "sArmCannonAnim_Suit_Left_Crouching"}},
    {"SPOSE_TURNING_AROUND_AND_CROUCHING", 2, {
        "sArmCannonAnim_Suit_Right_TurningAroundAndCrouching",
        "sArmCannonAnim_Suit_Left_TurningAroundAndCrouching"}},
    {"SPOSE_SHOOTING_AND_CROUCHING", 2, {
        "sArmCannonAnim_Suit_Right_ShootingAndCrouching",
        "sArmCannonAnim_Suit_Left_ShootingAndCrouching"}},
    {"SPOSE_SKIDDING", 2, {
        "sArmCannonAnim_Suit_Right_Skidding",
        "sArmCannonAnim_Suit_Left_Skidding"}},
    {"SPOSE_MIDAIR", 2, {
        "sArmCannonAnim_Suit_Right_MidAir",
        "sArmCannonAnim_Suit_Left_MidAir"}},
    {"SPOSE_TURNING_AROUND_MIDAIR", 2, {
        "sArmCannonAnim_Suit_Right_TurningAroundMidAir",
        "sArmCannonAnim_Suit_Left_TurningAroundMidAir"}},
    {"SPOSE_LANDING", 2, {
        "sArmCannonAnim_Suit_Right_Landing",
        "sArmCannonAnim_Suit_Left_Landing"}},
    {"SPOSE_STARTING_SPIN_JUMP", 2, {
        "sArmCannonAnim_Suit_Right_StartingSpinJump",
        "sArmCannonAnim_Suit_Left_StartingSpinJump"}},
    {"SPOSE_SPINNING", 2, {
        "sArmCannonAnim_Suit_Right_Spinning",
        "sArmCannonAnim_Suit_Left_Spinning"}},
    {"SPOSE_STARTING_WALL_JUMP", 2, {
        "sArmCannonAnim_Suit_Right_StartingWallJump",
        "sArmCannonAnim_Suit_Left_StartingWallJump"}},
    {"SPOSE_SPACE_JUMPING", 2, {
        "sArmCannonAnim_Suit_Right_SpaceJumping",
        "sArmCannonAnim_Suit_Left_SpaceJumping"}},
    {"SPOSE_SCREW_ATTACKING", 2, {
        "sArmCannonAnim_Suit_Right_ScrewAttacking",
        "sArmCannonAnim_Suit_Left_ScrewAttacking"}},
    {"SPOSE_MORPHING", 2, {
        "sArmCannonAnim_Suit_Right_Morphing",
        "sArmCannonAnim_Suit_Left_Morphing"}},
    {"SPOSE_MORPH_BALL", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_ROLLING", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_UNMORPHING", 2, {
        "sArmCannonAnim_Suit_Right_Unmorphing",
        "sArmCannonAnim_Suit_Left_Unmorphing"}},
    {"SPOSE_MORPH_BALL_MIDAIR", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_HANGING_ON_LEDGE", 2, {
        "sArmCannonAnim_Suit_Right_HangingOnLedge",
        "sArmCannonAnim_Suit_Left_HangingOnLedge"}},
    {"SPOSE_TURNING_TO_AIM_WHILE_HANGING", 2, {
        "sArmCannonAnim_Suit_Right_TurningToAimWhileHanging",
        "sArmCannonAnim_Suit_Left_TurningToAimWhileHanging"}},
    {"SPOSE_HIDING_ARM_CANNON_WHILE_HANGING", 2, {
        "sArmCannonAnim_Suit_Right_TurningToAimWhileHanging",
        "sArmCannonAnim_Suit_Left_TurningToAimWhileHanging"}},
    {"SPOSE_AIMING_WHILE_HANGING", 2, {
        "sArmCannonAnim_Suit_Right_AimingWhileHanging",
        "sArmCannonAnim_Suit_Left_AimingWhileHanging"}},
    {"SPOSE_SHOOTING_WHILE_HANGING", 2, {
        "sArmCannonAnim_Suit_Right_ShootingWhileHanging",
        "sArmCannonAnim_Suit_Left_ShootingWhileHanging"}},
    {"SPOSE_PULLING_YOURSELF_UP_FROM_HANGING", 2, {
        "sArmCannonAnim_Suit_Right_PullingYourselfUpFromHanging",
        "sArmCannonAnim_Suit_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING", 2, {
        "sArmCannonAnim_Suit_Right_PullingYourselfForwardFromHanging",
        "sArmCannonAnim_Suit_Left_PullingYourselfForwardFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL", 2, {
        "sArmCannonAnim_Suit_Right_PullingYourselfUpFromHanging",
        "sArmCannonAnim_Suit_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_USING_AN_ELEVATOR", 2, {
        "sArmCannonAnim_Suit_UsingAnElevator",
        "sArmCannonAnim_Suit_UsingAnElevator"}},
    {"SPOSE_FACING_THE_FOREGROUND", 2, {
        "sArmCannonAnim_Suit_FacingTheForeground",
        "sArmCannonAnim_Suit_FacingTheForeground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_FOREGROUND", 2, {
        "sArmCannonAnim_Suit_Right_TurningFromFacingTheForeground",
        "sArmCannonAnim_Suit_Left_TurningFromFacingTheForeground"}},
    {"SPOSE_GRABBED_BY_CHOZO_STATUE", 2, {
        "sArmCannonAnim_Suit_MorphballMotionless",
        "sArmCannonAnim_Suit_MorphballMotionless"}},
    {"SPOSE_DELAY_BEFORE_SHINESPARKING", 2, {
        "sArmCannonAnim_Suit_Right_DelayBeforeShinesparking",
        "sArmCannonAnim_Suit_Left_DelayBeforeShinesparking"}},
    {"SPOSE_SHINESPARKING", 2, {
        "sArmCannonAnim_Suit_Right_Shinesparking",
        "sArmCannonAnim_Suit_Left_Shinesparking"}},
    {"SPOSE_SHINESPARK_COLLISION", 2, {
        "sArmCannonAnim_Suit_Right_Shinesparking",
        "sArmCannonAnim_Suit_Left_Shinesparking"}},
    {"SPOSE_DELAY_AFTER_SHINESPARKING", 2, {
        "sArmCannonAnim_Suit_Right_DelayAfterShinesparking",
        "sArmCannonAnim_Suit_Left_DelayAfterShinesparking"}},
    {"SPOSE_DELAY_BEFORE_BALLSPARKING", 2, {
        "sArmCannonAnim_Suit_MorphballMotionless",
        "sArmCannonAnim_Suit_MorphballMotionless"}},
    {"SPOSE_BALLSPARKING", 2, {
        "sArmCannonAnim_Suit_Right_Ballsparking",
        "sArmCannonAnim_Suit_Left_Ballsparking"}},
    {"SPOSE_BALLSPARK_COLLISION", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_ON_ZIPLINE", 2, {
        "sArmCannonAnim_Suit_Right_OnZipline",
        "sArmCannonAnim_Suit_Left_OnZipline"}},
    {"SPOSE_SHOOTING_ON_ZIPLINE", 2, {
        "sArmCannonAnim_Suit_Right_ShootingOnZipline",
        "sArmCannonAnim_Suit_Left_ShootingOnZipline"}},
    {"SPOSE_TURNING_ON_ZIPLINE", 2, {
        "sArmCannonAnim_Suit_Right_TurningOnZipline",
        "sArmCannonAnim_Suit_Left_TurningOnZipline"}},
    {"SPOSE_MORPH_BALL_ON_ZIPLINE", 2, {
        "sArmCannonAnim_Suit_MorphballMotionless",
        "sArmCannonAnim_Suit_MorphballMotionless"}},
    {"SPOSE_SAVING_LOADING_GAME", 2, {
        "sArmCannonAnim_Suit_SavingLoadingGame",
        "sArmCannonAnim_Suit_SavingLoadingGame"}},
    {"SPOSE_DOWNLOADING_MAP_DATA", 2, {
        "sArmCannonAnim_Suit_DownloadingMapData",
        "sArmCannonAnim_Suit_DownloadingMapData"}},
    {"SPOSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA", 2, {
        "sArmCannonAnim_Suit_Right_TurningAround",
        "sArmCannonAnim_Suit_Left_TurningAround"}},
    {"SPOSE_GETTING_HURT", 2, {
        "sArmCannonAnim_Suit_Right_GettingHurt",
        "sArmCannonAnim_Suit_Left_GettingHurt"}},
    {"SPOSE_GETTING_KNOCKED_BACK", 2, {
        "sArmCannonAnim_Suit_Right_GettingHurt",
        "sArmCannonAnim_Suit_Left_GettingHurt"}},
    {"SPOSE_GETTING_HURT_IN_MORPH_BALL", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_DYING", 2, {
        "sArmCannonAnim_Suit_Dying",
        "sArmCannonAnim_Suit_Dying"}},
    {"SPOSE_CROUCHING_TO_CRAWL", 2, {
        "sArmCannonAnim_Suitless_Right_CrouchingToCrawl",
        "sArmCannonAnim_Suitless_Left_CrouchingToCrawl"}},
    {"SPOSE_CRAWLING_STOPPED", 2, {
        "sArmCannonAnim_Suitless_Right_CrawlingStopped",
        "sArmCannonAnim_Suitless_Left_CrawlingStopped"}},
    {"SPOSE_STARTING_TO_CRAWL", 2, {
        "sArmCannonAnim_Suitless_Right_StartingToCrawl",
        "sArmCannonAnim_Suitless_Left_StartingToCrawl"}},
    {"SPOSE_CRAWLING", 2, {
        "sArmCannonAnim_Suitless_Right_Crawling",
        "sArmCannonAnim_Suitless_Left_Crawling"}},
    {"SPOSE_UNCROUCHING_FROM_CRAWLING", 2, {
        "sArmCannonAnim_Suitless_Right_UncrouchingFromCrawling",
        "sArmCannonAnim_Suitless_Left_UncrouchingFromCrawling"}},
    {"SPOSE_TURNING_AROUND_WHILE_CRAWLING", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAroundWhileCrawling",
        "sArmCannonAnim_Suitless_Left_TurningAroundWhileCrawling"}},
    {"SPOSE_SHOOTING_WHILE_CRAWLING", 2, {
        "sArmCannonAnim_Suitless_Right_ShootingWhileCrawling",
        "sArmCannonAnim_Suitless_Left_ShootingWhileCrawling"}},
    {"SPOSE_UNCROUCHING_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_Right_UncrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_UncrouchingSuitless"}},
    {"SPOSE_CROUCHING_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_Right_CrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_CrouchingSuitless"}},
    {"SPOSE_GRABBING_A_LEDGE_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_Right_HangingOnLedgeSuitless",
        "sArmCannonAnim_Suitless_Left_HangingOnLedgeSuitless"}},
    {"SPOSE_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_FacingTheBackground",
        "sArmCannonAnim_Suitless_FacingTheBackground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_Right_TurningFromFacingTheBackground",
        "sArmCannonAnim_Suitless_Left_TurningFromFacingTheBackground"}},
    {"SPOSE_ACTIVATING_ZIPLINES", 2, {
        "sArmCannonAnim_Suit_MorphballMotionless",
        "sArmCannonAnim_Suit_MorphballMotionless"}},
    {"SPOSE_IN_ESCAPE_SHIP", 2, {
        "sArmCannonAnim_Suit_Right_Crouching",
        "sArmCannonAnim_Suit_Left_Crouching"}},
    {"SPOSE_TURNING_TO_ENTER_ESCAPE_SHIP", 2, {
        "sArmCannonAnim_Suit_Right_TurningAround",
        "sArmCannonAnim_Suit_Left_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_22[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_Forward_Running",
        "sArmCannonAnim_Suit_Left_Forward_Running"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_Running",
        "sArmCannonAnim_Suit_Left_DiagonalUp_Running"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_Running",
        "sArmCannonAnim_Suit_Left_DiagonalDown_Running"}},
    {"ACD_NONE", 2, {
        "sArmCannonAnim_Suit_Right_None_Running",
        "sArmCannonAnim_Suit_Left_None_Running"}},
};
static const FusionMzmSamusTableRow rows_23[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_Forward_Running_Speedboosting",
        "sArmCannonAnim_Suit_Left_Forward_Running_Speedboosting"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_Running_Speedboosting",
        "sArmCannonAnim_Suit_Left_DiagonalUp_Running_Speedboosting"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_Running_Speedboosting",
        "sArmCannonAnim_Suit_Left_DiagonalDown_Running_Speedboosting"}},
    {"ACD_NONE", 2, {
        "sArmCannonAnim_Suit_Right_None_Running_Speedboosting",
        "sArmCannonAnim_Suit_Left_None_Running_Speedboosting"}},
};
static const FusionMzmSamusTableRow rows_24[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_Standing",
        "sArmCannonAnim_Suit_Left_Standing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_Standing",
        "sArmCannonAnim_Suit_Left_DiagonalUp_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_Standing",
        "sArmCannonAnim_Suit_Left_DiagonalDown_Standing"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_Up_Standing",
        "sArmCannonAnim_Suit_Left_Up_Standing"}},
};
static const FusionMzmSamusTableRow rows_25[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_TurningAround",
        "sArmCannonAnim_Suit_Left_TurningAround"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_TurningAround",
        "sArmCannonAnim_Suit_Left_DiagonalUp_TurningAround"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_TurningAround",
        "sArmCannonAnim_Suit_Left_DiagonalDown_TurningAround"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_Up_TurningAround",
        "sArmCannonAnim_Suit_Left_Up_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_26[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_Shooting",
        "sArmCannonAnim_Suit_Left_Shooting"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_Shooting",
        "sArmCannonAnim_Suit_Left_DiagonalUp_Shooting"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_Shooting",
        "sArmCannonAnim_Suit_Left_DiagonalDown_Shooting"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_Up_Shooting",
        "sArmCannonAnim_Suit_Left_Up_Shooting"}},
};
static const FusionMzmSamusTableRow rows_27[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_Crouching",
        "sArmCannonAnim_Suit_Left_Crouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_Crouching",
        "sArmCannonAnim_Suit_Left_DiagonalUp_Crouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_Crouching",
        "sArmCannonAnim_Suit_Left_DiagonalDown_Crouching"}},
};
static const FusionMzmSamusTableRow rows_28[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_TurningAroundAndCrouching",
        "sArmCannonAnim_Suit_Left_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_TurningAroundAndCrouching",
        "sArmCannonAnim_Suit_Left_DiagonalUp_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_TurningAroundAndCrouching",
        "sArmCannonAnim_Suit_Left_DiagonalDown_TurningAroundAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_29[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_ShootingAndCrouching",
        "sArmCannonAnim_Suit_Left_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_ShootingAndCrouching",
        "sArmCannonAnim_Suit_Left_DiagonalUp_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_ShootingAndCrouching",
        "sArmCannonAnim_Suit_Left_DiagonalDown_ShootingAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_30[] = {
    {"FALSE", 2, {
        "sArmCannonAnim_Suit_Right_Skidding",
        "sArmCannonAnim_Suit_Left_Skidding"}},
    {"TRUE", 2, {
        "sArmCannonAnim_Suit_Right_Armed_Skidding",
        "sArmCannonAnim_Suit_Left_Armed_Skidding"}},
};
static const FusionMzmSamusTableRow rows_31[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_MidAir",
        "sArmCannonAnim_Suit_Left_MidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_MidAir",
        "sArmCannonAnim_Suit_Left_DiagonalUp_MidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_MidAir",
        "sArmCannonAnim_Suit_Left_DiagonalDown_MidAir"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_Up_MidAir",
        "sArmCannonAnim_Suit_Left_Up_MidAir"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_Down_MidAir",
        "sArmCannonAnim_Suit_Left_Down_MidAir"}},
};
static const FusionMzmSamusTableRow rows_32[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_TurningAroundMidAir",
        "sArmCannonAnim_Suit_Left_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_TurningAroundMidAir",
        "sArmCannonAnim_Suit_Left_DiagonalUp_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_TurningAroundMidAir",
        "sArmCannonAnim_Suit_Left_DiagonalDown_TurningAroundMidAir"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_Up_TurningAroundMidAir",
        "sArmCannonAnim_Suit_Left_Up_TurningAroundMidAir"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_Down_TurningAroundMidAir",
        "sArmCannonAnim_Suit_Left_Down_TurningAroundMidAir"}},
};
static const FusionMzmSamusTableRow rows_33[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_Landing",
        "sArmCannonAnim_Suit_Left_Landing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_Landing",
        "sArmCannonAnim_Suit_Left_DiagonalUp_Landing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_Landing",
        "sArmCannonAnim_Suit_Left_DiagonalDown_Landing"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_Up_Landing",
        "sArmCannonAnim_Suit_Left_Up_Landing"}},
};
static const FusionMzmSamusTableRow rows_34[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_AimingWhileHanging",
        "sArmCannonAnim_Suit_Left_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_AimingWhileHanging",
        "sArmCannonAnim_Suit_Left_DiagonalUp_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_AimingWhileHanging",
        "sArmCannonAnim_Suit_Left_DiagonalDown_AimingWhileHanging"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_Up_AimingWhileHanging",
        "sArmCannonAnim_Suit_Left_Up_AimingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_Down_AimingWhileHanging",
        "sArmCannonAnim_Suit_Left_Down_AimingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_35[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_ShootingWhileHanging",
        "sArmCannonAnim_Suit_Left_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalUp_ShootingWhileHanging",
        "sArmCannonAnim_Suit_Left_DiagonalUp_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_ShootingWhileHanging",
        "sArmCannonAnim_Suit_Left_DiagonalDown_ShootingWhileHanging"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_Up_ShootingWhileHanging",
        "sArmCannonAnim_Suit_Left_Up_ShootingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_Down_ShootingWhileHanging",
        "sArmCannonAnim_Suit_Left_Down_ShootingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_36[] = {
    {"FALSE", 2, {
        "sArmCannonAnim_Suit_UsingAnElevator",
        "sArmCannonAnim_Suit_UsingAnElevator"}},
    {"TRUE", 2, {
        "sArmCannonAnim_Suit_GoingUp_UsingAnElevator",
        "sArmCannonAnim_Suit_GoingUp_UsingAnElevator"}},
};
static const FusionMzmSamusTableRow rows_37[] = {
    {"FORCED_MOVEMENT_UPWARDS_SHINESPARK", 2, {
        "sArmCannonAnim_Suit_Right_Shinesparking",
        "sArmCannonAnim_Suit_Left_Shinesparking"}},
    {"FORCED_MOVEMENT_SIDEWARDS_SHINESPARK", 2, {
        "sArmCannonAnim_Suit_Right_Sidewards_Shinesparking",
        "sArmCannonAnim_Suit_Left_Sidewards_Shinesparking"}},
    {"FORCED_MOVEMENT_DIAGONAL_SHINESPARK", 2, {
        "sArmCannonAnim_Suit_Right_Sidewards_Shinesparking",
        "sArmCannonAnim_Suit_Left_Sidewards_Shinesparking"}},
};
static const FusionMzmSamusTableRow rows_38[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_OnZipline",
        "sArmCannonAnim_Suit_Left_OnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_OnZipline",
        "sArmCannonAnim_Suit_Left_OnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_OnZipline",
        "sArmCannonAnim_Suit_Left_DiagonalDown_OnZipline"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_OnZipline",
        "sArmCannonAnim_Suit_Left_OnZipline"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_Down_OnZipline",
        "sArmCannonAnim_Suit_Left_Down_OnZipline"}},
};
static const FusionMzmSamusTableRow rows_39[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_ShootingOnZipline",
        "sArmCannonAnim_Suit_Left_ShootingOnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_ShootingOnZipline",
        "sArmCannonAnim_Suit_Left_ShootingOnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_ShootingOnZipline",
        "sArmCannonAnim_Suit_Left_DiagonalDown_ShootingOnZipline"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_ShootingOnZipline",
        "sArmCannonAnim_Suit_Left_ShootingOnZipline"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_Down_ShootingOnZipline",
        "sArmCannonAnim_Suit_Left_Down_ShootingOnZipline"}},
};
static const FusionMzmSamusTableRow rows_40[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suit_Right_TurningOnZipline",
        "sArmCannonAnim_Suit_Left_TurningOnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suit_Right_TurningOnZipline",
        "sArmCannonAnim_Suit_Left_TurningOnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_DiagonalDown_TurningOnZipline",
        "sArmCannonAnim_Suit_Left_DiagonalDown_TurningOnZipline"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suit_Right_TurningOnZipline",
        "sArmCannonAnim_Suit_Left_TurningOnZipline"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suit_Right_Down_TurningOnZipline",
        "sArmCannonAnim_Suit_Left_Down_TurningOnZipline"}},
};
static const FusionMzmSamusTableRow rows_41[] = {
    {"SPOSE_RUNNING", 2, {
        "sSamusAnim_FullSuit_Right_Running",
        "sSamusAnim_FullSuit_Left_Running"}},
    {"SPOSE_STANDING", 2, {
        "sSamusAnim_FullSuit_Right_Standing",
        "sSamusAnim_FullSuit_Left_Standing"}},
    {"SPOSE_TURNING_AROUND", 2, {
        "sSamusAnim_FullSuit_Right_TurningAround",
        "sSamusAnim_FullSuit_Left_TurningAround"}},
    {"SPOSE_SHOOTING", 2, {
        "sSamusAnim_FullSuit_Right_Shooting",
        "sSamusAnim_FullSuit_Left_Shooting"}},
    {"SPOSE_CROUCHING", 2, {
        "sSamusAnim_FullSuit_Right_Crouching",
        "sSamusAnim_FullSuit_Left_Crouching"}},
    {"SPOSE_TURNING_AROUND_AND_CROUCHING", 2, {
        "sSamusAnim_FullSuit_Right_TurningAroundAndCrouching",
        "sSamusAnim_FullSuit_Left_TurningAroundAndCrouching"}},
    {"SPOSE_SHOOTING_AND_CROUCHING", 2, {
        "sSamusAnim_FullSuit_Right_ShootingAndCrouching",
        "sSamusAnim_FullSuit_Left_ShootingAndCrouching"}},
    {"SPOSE_SKIDDING", 2, {
        "sSamusAnim_FullSuit_Right_Skidding",
        "sSamusAnim_FullSuit_Left_Skidding"}},
    {"SPOSE_MIDAIR", 2, {
        "sSamusAnim_FullSuit_Right_MidAir",
        "sSamusAnim_FullSuit_Left_MidAir"}},
    {"SPOSE_TURNING_AROUND_MIDAIR", 2, {
        "sSamusAnim_FullSuit_Right_TurningAroundMidAir",
        "sSamusAnim_FullSuit_Left_TurningAroundMidAir"}},
    {"SPOSE_LANDING", 2, {
        "sSamusAnim_FullSuit_Right_Landing",
        "sSamusAnim_FullSuit_Left_Landing"}},
    {"SPOSE_STARTING_SPIN_JUMP", 2, {
        "sSamusAnim_FullSuit_Right_StartingSpinJump",
        "sSamusAnim_FullSuit_Left_StartingSpinJump"}},
    {"SPOSE_SPINNING", 2, {
        "sSamusAnim_FullSuit_Right_Spinning",
        "sSamusAnim_FullSuit_Left_Spinning"}},
    {"SPOSE_STARTING_WALL_JUMP", 2, {
        "sSamusAnim_FullSuit_Right_StartingWallJump",
        "sSamusAnim_FullSuit_Left_StartingWallJump"}},
    {"SPOSE_SPACE_JUMPING", 2, {
        "sSamusAnim_FullSuit_Right_SpaceJumping",
        "sSamusAnim_FullSuit_Left_SpaceJumping"}},
    {"SPOSE_SCREW_ATTACKING", 2, {
        "sSamusAnim_FullSuit_Right_ScrewAttacking",
        "sSamusAnim_FullSuit_Left_ScrewAttacking"}},
    {"SPOSE_MORPHING", 2, {
        "sSamusAnim_FullSuit_Right_Morphing",
        "sSamusAnim_FullSuit_Left_Morphing"}},
    {"SPOSE_MORPH_BALL", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_ROLLING", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_UNMORPHING", 2, {
        "sSamusAnim_FullSuit_Right_Unmorphing",
        "sSamusAnim_FullSuit_Left_Unmorphing"}},
    {"SPOSE_MORPH_BALL_MIDAIR", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_HANGING_ON_LEDGE", 2, {
        "sSamusAnim_FullSuit_Right_HangingOnLedge",
        "sSamusAnim_FullSuit_Left_HangingOnLedge"}},
    {"SPOSE_TURNING_TO_AIM_WHILE_HANGING", 2, {
        "sSamusAnim_FullSuit_Right_TurningToAimWhileHanging",
        "sSamusAnim_FullSuit_Left_TurningToAimWhileHanging"}},
    {"SPOSE_HIDING_ARM_CANNON_WHILE_HANGING", 2, {
        "sSamusAnim_FullSuit_Right_TurningToAimWhileHanging",
        "sSamusAnim_FullSuit_Left_TurningToAimWhileHanging"}},
    {"SPOSE_AIMING_WHILE_HANGING", 2, {
        "sSamusAnim_FullSuit_Right_AimingWhileHanging",
        "sSamusAnim_FullSuit_Left_AimingWhileHanging"}},
    {"SPOSE_SHOOTING_WHILE_HANGING", 2, {
        "sSamusAnim_FullSuit_Right_ShootingWhileHanging",
        "sSamusAnim_FullSuit_Left_ShootingWhileHanging"}},
    {"SPOSE_PULLING_YOURSELF_UP_FROM_HANGING", 2, {
        "sSamusAnim_FullSuit_Right_PullingYourselfUpFromHanging",
        "sSamusAnim_FullSuit_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING", 2, {
        "sSamusAnim_FullSuit_Right_PullingYourselfForwardFromHanging",
        "sSamusAnim_FullSuit_Left_PullingYourselfForwardFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL", 2, {
        "sSamusAnim_FullSuit_Right_PullingYourselfUpFromHanging",
        "sSamusAnim_FullSuit_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_USING_AN_ELEVATOR", 2, {
        "sSamusAnim_FullSuit_UsingAnElevator",
        "sSamusAnim_FullSuit_UsingAnElevator"}},
    {"SPOSE_FACING_THE_FOREGROUND", 2, {
        "sSamusAnim_FullSuit_FacingTheForeground",
        "sSamusAnim_FullSuit_FacingTheForeground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_FOREGROUND", 2, {
        "sSamusAnim_FullSuit_Right_TurningFromFacingTheForeground",
        "sSamusAnim_FullSuit_Left_TurningFromFacingTheForeground"}},
    {"SPOSE_GRABBED_BY_CHOZO_STATUE", 2, {
        "sSamusAnim_FullSuit_MorphballMotionless",
        "sSamusAnim_FullSuit_MorphballMotionless"}},
    {"SPOSE_DELAY_BEFORE_SHINESPARKING", 2, {
        "sSamusAnim_FullSuit_Right_DelayBeforeShinesparking",
        "sSamusAnim_FullSuit_Left_DelayBeforeShinesparking"}},
    {"SPOSE_SHINESPARKING", 2, {
        "sSamusAnim_FullSuit_Right_Shinesparking",
        "sSamusAnim_FullSuit_Left_Shinesparking"}},
    {"SPOSE_SHINESPARK_COLLISION", 2, {
        "sSamusAnim_FullSuit_Right_Shinesparking",
        "sSamusAnim_FullSuit_Left_Shinesparking"}},
    {"SPOSE_DELAY_AFTER_SHINESPARKING", 2, {
        "sSamusAnim_FullSuit_Right_DelayAfterShinesparking",
        "sSamusAnim_FullSuit_Left_DelayAfterShinesparking"}},
    {"SPOSE_DELAY_BEFORE_BALLSPARKING", 2, {
        "sSamusAnim_FullSuit_MorphballMotionless",
        "sSamusAnim_FullSuit_MorphballMotionless"}},
    {"SPOSE_BALLSPARKING", 2, {
        "sSamusAnim_FullSuit_Right_Ballsparking",
        "sSamusAnim_FullSuit_Left_Ballsparking"}},
    {"SPOSE_BALLSPARK_COLLISION", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_ON_ZIPLINE", 2, {
        "sSamusAnim_FullSuit_Right_OnZipline",
        "sSamusAnim_FullSuit_Left_OnZipline"}},
    {"SPOSE_SHOOTING_ON_ZIPLINE", 2, {
        "sSamusAnim_FullSuit_Right_ShootingOnZipline",
        "sSamusAnim_FullSuit_Left_ShootingOnZipline"}},
    {"SPOSE_TURNING_ON_ZIPLINE", 2, {
        "sSamusAnim_FullSuit_Right_TurningOnZipline",
        "sSamusAnim_FullSuit_Left_TurningOnZipline"}},
    {"SPOSE_MORPH_BALL_ON_ZIPLINE", 2, {
        "sSamusAnim_FullSuit_MorphballMotionless",
        "sSamusAnim_FullSuit_MorphballMotionless"}},
    {"SPOSE_SAVING_LOADING_GAME", 2, {
        "sSamusAnim_FullSuit_SavingLoadingGame",
        "sSamusAnim_FullSuit_SavingLoadingGame"}},
    {"SPOSE_DOWNLOADING_MAP_DATA", 2, {
        "sSamusAnim_FullSuit_DownloadingMapData",
        "sSamusAnim_FullSuit_DownloadingMapData"}},
    {"SPOSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA", 2, {
        "sSamusAnim_FullSuit_Right_TurningAround",
        "sSamusAnim_FullSuit_Left_TurningAround"}},
    {"SPOSE_GETTING_HURT", 2, {
        "sSamusAnim_FullSuit_Right_GettingKnockedBack",
        "sSamusAnim_FullSuit_Left_GettingKnockedBack"}},
    {"SPOSE_GETTING_KNOCKED_BACK", 2, {
        "sSamusAnim_FullSuit_Right_GettingKnockedBack",
        "sSamusAnim_FullSuit_Left_GettingKnockedBack"}},
    {"SPOSE_GETTING_HURT_IN_MORPH_BALL", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_DYING", 2, {
        "sSamusAnim_FullSuit_Right_Dying",
        "sSamusAnim_FullSuit_Left_Dying"}},
    {"SPOSE_CROUCHING_TO_CRAWL", 2, {
        "sSamusAnim_Suitless_Right_CrouchingToCrawl",
        "sSamusAnim_Suitless_Left_CrouchingToCrawl"}},
    {"SPOSE_CRAWLING_STOPPED", 2, {
        "sSamusAnim_Suitless_Right_CrawlingStopped",
        "sSamusAnim_Suitless_Left_CrawlingStopped"}},
    {"SPOSE_STARTING_TO_CRAWL", 2, {
        "sSamusAnim_Suitless_Right_StartingToCrawl",
        "sSamusAnim_Suitless_Left_StartingToCrawl"}},
    {"SPOSE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_Crawling",
        "sSamusAnim_Suitless_Left_Crawling"}},
    {"SPOSE_UNCROUCHING_FROM_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_UncrouchingFromCrawling",
        "sSamusAnim_Suitless_Left_UncrouchingFromCrawling"}},
    {"SPOSE_TURNING_AROUND_WHILE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_TurningAroundWhileCrawling",
        "sSamusAnim_Suitless_Left_TurningAroundWhileCrawling"}},
    {"SPOSE_SHOOTING_WHILE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_ShootingWhileCrawling",
        "sSamusAnim_Suitless_Left_ShootingWhileCrawling"}},
    {"SPOSE_UNCROUCHING_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_UncrouchingSuitless",
        "sSamusAnim_Suitless_Left_UncrouchingSuitless"}},
    {"SPOSE_CROUCHING_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_CrouchingSuitless",
        "sSamusAnim_Suitless_Left_CrouchingSuitless"}},
    {"SPOSE_GRABBING_A_LEDGE_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_HangingOnLedgeSuitless",
        "sSamusAnim_Suitless_Left_HangingOnLedgeSuitless"}},
    {"SPOSE_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sSamusAnim_Suitless_FacingTheBackground",
        "sSamusAnim_Suitless_FacingTheBackground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_TurningFromFacingTheBackground",
        "sSamusAnim_Suitless_Left_TurningFromFacingTheBackground"}},
    {"SPOSE_ACTIVATING_ZIPLINES", 2, {
        "sSamusAnim_FullSuit_MorphballMotionless",
        "sSamusAnim_FullSuit_MorphballMotionless"}},
    {"SPOSE_IN_ESCAPE_SHIP", 2, {
        "sSamusAnim_FullSuit_Right_Crouching",
        "sSamusAnim_FullSuit_Left_Crouching"}},
    {"SPOSE_TURNING_TO_ENTER_ESCAPE_SHIP", 2, {
        "sSamusAnim_FullSuit_Right_TurningAround",
        "sSamusAnim_FullSuit_Left_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_42[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_Forward_Running",
        "sSamusAnim_FullSuit_Left_Forward_Running"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_Running",
        "sSamusAnim_FullSuit_Left_DiagonalUp_Running"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_Running",
        "sSamusAnim_FullSuit_Left_DiagonalDown_Running"}},
    {"ACD_NONE", 2, {
        "sSamusAnim_FullSuit_Right_Running",
        "sSamusAnim_FullSuit_Left_Running"}},
};
static const FusionMzmSamusTableRow rows_43[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_Forward_Running_Speedboosting",
        "sSamusAnim_FullSuit_Left_Forward_Running_Speedboosting"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_Running_Speedboosting",
        "sSamusAnim_FullSuit_Left_DiagonalUp_Running_Speedboosting"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_Running_Speedboosting",
        "sSamusAnim_FullSuit_Left_DiagonalDown_Running_Speedboosting"}},
    {"ACD_NONE", 2, {
        "sSamusAnim_FullSuit_Right_Running_Speedboosting",
        "sSamusAnim_FullSuit_Left_Running_Speedboosting"}},
};
static const FusionMzmSamusTableRow rows_44[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_Standing",
        "sSamusAnim_FullSuit_Left_Standing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_Standing",
        "sSamusAnim_FullSuit_Left_DiagonalUp_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_Standing",
        "sSamusAnim_FullSuit_Left_DiagonalDown_Standing"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_Up_Standing",
        "sSamusAnim_FullSuit_Left_Up_Standing"}},
};
static const FusionMzmSamusTableRow rows_45[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_TurningAround",
        "sSamusAnim_FullSuit_Left_TurningAround"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_TurningAround",
        "sSamusAnim_FullSuit_Left_DiagonalUp_TurningAround"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_TurningAround",
        "sSamusAnim_FullSuit_Left_DiagonalDown_TurningAround"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_Up_TurningAround",
        "sSamusAnim_FullSuit_Left_Up_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_46[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_Shooting",
        "sSamusAnim_FullSuit_Left_Shooting"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_Shooting",
        "sSamusAnim_FullSuit_Left_DiagonalUp_Shooting"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_Shooting",
        "sSamusAnim_FullSuit_Left_DiagonalDown_Shooting"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_Up_Shooting",
        "sSamusAnim_FullSuit_Left_Up_Shooting"}},
};
static const FusionMzmSamusTableRow rows_47[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_Crouching",
        "sSamusAnim_FullSuit_Left_Crouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_Crouching",
        "sSamusAnim_FullSuit_Left_DiagonalUp_Crouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_Crouching",
        "sSamusAnim_FullSuit_Left_DiagonalDown_Crouching"}},
};
static const FusionMzmSamusTableRow rows_48[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_TurningAroundAndCrouching",
        "sSamusAnim_FullSuit_Left_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_TurningAroundAndCrouching",
        "sSamusAnim_FullSuit_Left_DiagonalUp_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_TurningAroundAndCrouching",
        "sSamusAnim_FullSuit_Left_DiagonalDown_TurningAroundAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_49[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_ShootingAndCrouching",
        "sSamusAnim_FullSuit_Left_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_ShootingAndCrouching",
        "sSamusAnim_FullSuit_Left_DiagonalUp_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_ShootingAndCrouching",
        "sSamusAnim_FullSuit_Left_DiagonalDown_ShootingAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_50[] = {
    {"FALSE", 2, {
        "sSamusAnim_FullSuit_Right_Skidding",
        "sSamusAnim_FullSuit_Left_Skidding"}},
    {"TRUE", 2, {
        "sSamusAnim_FullSuit_Right_Armed_Skidding",
        "sSamusAnim_FullSuit_Left_Armed_Skidding"}},
};
static const FusionMzmSamusTableRow rows_51[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_MidAir",
        "sSamusAnim_FullSuit_Left_MidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_MidAir",
        "sSamusAnim_FullSuit_Left_DiagonalUp_MidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_MidAir",
        "sSamusAnim_FullSuit_Left_DiagonalDown_MidAir"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_Up_MidAir",
        "sSamusAnim_FullSuit_Left_Up_MidAir"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_Down_MidAir",
        "sSamusAnim_FullSuit_Left_Down_MidAir"}},
};
static const FusionMzmSamusTableRow rows_52[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_TurningAroundMidAir",
        "sSamusAnim_FullSuit_Left_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_TurningAroundMidAir",
        "sSamusAnim_FullSuit_Left_DiagonalUp_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_TurningAroundMidAir",
        "sSamusAnim_FullSuit_Left_DiagonalDown_TurningAroundMidAir"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_Up_TurningAroundMidAir",
        "sSamusAnim_FullSuit_Left_Up_TurningAroundMidAir"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_Down_TurningAroundMidAir",
        "sSamusAnim_FullSuit_Left_Down_TurningAroundMidAir"}},
};
static const FusionMzmSamusTableRow rows_53[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_Landing",
        "sSamusAnim_FullSuit_Left_Landing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_Landing",
        "sSamusAnim_FullSuit_Left_DiagonalUp_Landing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_Landing",
        "sSamusAnim_FullSuit_Left_DiagonalDown_Landing"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_Up_Landing",
        "sSamusAnim_FullSuit_Left_Up_Landing"}},
};
static const FusionMzmSamusTableRow rows_54[] = {
    {"FALSE", 2, {
        "sSamusAnim_FullSuit_Right_ScrewAttacking",
        "sSamusAnim_FullSuit_Left_ScrewAttacking"}},
    {"TRUE", 2, {
        "sSamusAnim_FullSuit_Right_SpaceJumping",
        "sSamusAnim_FullSuit_Left_SpaceJumping"}},
};
static const FusionMzmSamusTableRow rows_55[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_AimingWhileHanging",
        "sSamusAnim_FullSuit_Left_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_AimingWhileHanging",
        "sSamusAnim_FullSuit_Left_DiagonalUp_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_AimingWhileHanging",
        "sSamusAnim_FullSuit_Left_DiagonalDown_AimingWhileHanging"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_Up_AimingWhileHanging",
        "sSamusAnim_FullSuit_Left_Up_AimingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_Down_AimingWhileHanging",
        "sSamusAnim_FullSuit_Left_Down_AimingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_56[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_ShootingWhileHanging",
        "sSamusAnim_FullSuit_Left_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalUp_ShootingWhileHanging",
        "sSamusAnim_FullSuit_Left_DiagonalUp_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_ShootingWhileHanging",
        "sSamusAnim_FullSuit_Left_DiagonalDown_ShootingWhileHanging"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_Up_ShootingWhileHanging",
        "sSamusAnim_FullSuit_Left_Up_ShootingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_Down_ShootingWhileHanging",
        "sSamusAnim_FullSuit_Left_Down_ShootingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_57[] = {
    {"FALSE", 2, {
        "sSamusAnim_FullSuit_UsingAnElevator",
        "sSamusAnim_FullSuit_UsingAnElevator"}},
    {"TRUE", 2, {
        "sSamusAnim_FullSuit_GoingUp_UsingAnElevator",
        "sSamusAnim_FullSuit_GoingUp_UsingAnElevator"}},
};
static const FusionMzmSamusTableRow rows_58[] = {
    {"FORCED_MOVEMENT_UPWARDS_SHINESPARK", 2, {
        "sSamusAnim_FullSuit_Right_Shinesparking",
        "sSamusAnim_FullSuit_Left_Shinesparking"}},
    {"FORCED_MOVEMENT_SIDEWARDS_SHINESPARK", 2, {
        "sSamusAnim_FullSuit_Right_Sidewards_Shinesparking",
        "sSamusAnim_FullSuit_Left_Sidewards_Shinesparking"}},
    {"FORCED_MOVEMENT_DIAGONAL_SHINESPARK", 2, {
        "sSamusAnim_FullSuit_Right_Sidewards_Shinesparking",
        "sSamusAnim_FullSuit_Left_Sidewards_Shinesparking"}},
};
static const FusionMzmSamusTableRow rows_59[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_OnZipline",
        "sSamusAnim_FullSuit_Left_OnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_OnZipline",
        "sSamusAnim_FullSuit_Left_OnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_OnZipline",
        "sSamusAnim_FullSuit_Left_DiagonalDown_OnZipline"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_OnZipline",
        "sSamusAnim_FullSuit_Left_OnZipline"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_Down_OnZipline",
        "sSamusAnim_FullSuit_Left_Down_OnZipline"}},
};
static const FusionMzmSamusTableRow rows_60[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_ShootingOnZipline",
        "sSamusAnim_FullSuit_Left_ShootingOnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_ShootingOnZipline",
        "sSamusAnim_FullSuit_Left_ShootingOnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_ShootingOnZipline",
        "sSamusAnim_FullSuit_Left_DiagonalDown_ShootingOnZipline"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_ShootingOnZipline",
        "sSamusAnim_FullSuit_Left_ShootingOnZipline"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_Down_ShootingOnZipline",
        "sSamusAnim_FullSuit_Left_Down_ShootingOnZipline"}},
};
static const FusionMzmSamusTableRow rows_61[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_FullSuit_Right_TurningOnZipline",
        "sSamusAnim_FullSuit_Left_TurningOnZipline"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_FullSuit_Right_TurningOnZipline",
        "sSamusAnim_FullSuit_Left_TurningOnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_DiagonalDown_TurningOnZipline",
        "sSamusAnim_FullSuit_Left_DiagonalDown_TurningOnZipline"}},
    {"ACD_UP", 2, {
        "sSamusAnim_FullSuit_Right_TurningOnZipline",
        "sSamusAnim_FullSuit_Left_TurningOnZipline"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_FullSuit_Right_Down_TurningOnZipline",
        "sSamusAnim_FullSuit_Left_Down_TurningOnZipline"}},
};
static const FusionMzmSamusTableRow rows_62[] = {
    {"SPOSE_RUNNING", 2, {
        "sSamusAnim_Suitless_Right_Running",
        "sSamusAnim_Suitless_Left_Running"}},
    {"SPOSE_STANDING", 2, {
        "sSamusAnim_Suitless_Right_Standing",
        "sSamusAnim_Suitless_Left_Standing"}},
    {"SPOSE_TURNING_AROUND", 2, {
        "sSamusAnim_Suitless_Right_TurningAround",
        "sSamusAnim_Suitless_Left_TurningAround"}},
    {"SPOSE_SHOOTING", 2, {
        "sSamusAnim_Suitless_Right_Shooting",
        "sSamusAnim_Suitless_Left_Shooting"}},
    {"SPOSE_CROUCHING", 2, {
        "sSamusAnim_Suitless_Right_Crouching",
        "sSamusAnim_Suitless_Left_Crouching"}},
    {"SPOSE_TURNING_AROUND_AND_CROUCHING", 2, {
        "sSamusAnim_Suitless_Right_TurningAroundAndCrouching",
        "sSamusAnim_Suitless_Left_TurningAroundAndCrouching"}},
    {"SPOSE_SHOOTING_AND_CROUCHING", 2, {
        "sSamusAnim_Suitless_Right_ShootingAndCrouching",
        "sSamusAnim_Suitless_Left_ShootingAndCrouching"}},
    {"SPOSE_SKIDDING", 2, {
        "sSamusAnim_FullSuit_Right_Skidding",
        "sSamusAnim_FullSuit_Left_Skidding"}},
    {"SPOSE_MIDAIR", 2, {
        "sSamusAnim_Suitless_Right_MidAir",
        "sSamusAnim_Suitless_Left_MidAir"}},
    {"SPOSE_TURNING_AROUND_MIDAIR", 2, {
        "sSamusAnim_FullSuit_Right_TurningAroundMidAir",
        "sSamusAnim_FullSuit_Left_TurningAroundMidAir"}},
    {"SPOSE_LANDING", 2, {
        "sSamusAnim_Suitless_Right_Landing",
        "sSamusAnim_Suitless_Left_Landing"}},
    {"SPOSE_STARTING_SPIN_JUMP", 2, {
        "sSamusAnim_Suitless_Right_StartingSpinJump",
        "sSamusAnim_Suitless_Left_StartingSpinJump"}},
    {"SPOSE_SPINNING", 2, {
        "sSamusAnim_Suitless_Right_Spinning",
        "sSamusAnim_Suitless_Left_Spinning"}},
    {"SPOSE_STARTING_WALL_JUMP", 2, {
        "sSamusAnim_Suitless_Left_StartingWallJump",
        "sSamusAnim_Suitless_Right_StartingWallJump"}},
    {"SPOSE_SPACE_JUMPING", 2, {
        "sSamusAnim_FullSuit_Right_SpaceJumping",
        "sSamusAnim_FullSuit_Left_SpaceJumping"}},
    {"SPOSE_SCREW_ATTACKING", 2, {
        "sSamusAnim_FullSuit_Right_ScrewAttacking",
        "sSamusAnim_FullSuit_Left_ScrewAttacking"}},
    {"SPOSE_MORPHING", 2, {
        "sSamusAnim_FullSuit_Right_Morphing",
        "sSamusAnim_FullSuit_Left_Morphing"}},
    {"SPOSE_MORPH_BALL", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_ROLLING", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_UNMORPHING", 2, {
        "sSamusAnim_FullSuit_Right_Unmorphing",
        "sSamusAnim_FullSuit_Left_Unmorphing"}},
    {"SPOSE_MORPH_BALL_MIDAIR", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_HANGING_ON_LEDGE", 2, {
        "sSamusAnim_Suitless_Right_HangingOnLedge",
        "sSamusAnim_Suitless_Left_HangingOnLedge"}},
    {"SPOSE_TURNING_TO_AIM_WHILE_HANGING", 2, {
        "sSamusAnim_Suitless_Right_TurningToAimWhileHanging",
        "sSamusAnim_Suitless_Left_TurningToAimWhileHanging"}},
    {"SPOSE_HIDING_ARM_CANNON_WHILE_HANGING", 2, {
        "sSamusAnim_Suitless_Right_TurningToAimWhileHanging",
        "sSamusAnim_Suitless_Left_TurningToAimWhileHanging"}},
    {"SPOSE_AIMING_WHILE_HANGING", 2, {
        "sSamusAnim_Suitless_Right_AimingWhileHanging",
        "sSamusAnim_Suitless_Left_AimingWhileHanging"}},
    {"SPOSE_SHOOTING_WHILE_HANGING", 2, {
        "sSamusAnim_Suitless_Right_ShootingWhileHanging",
        "sSamusAnim_Suitless_Left_ShootingWhileHanging"}},
    {"SPOSE_PULLING_YOURSELF_UP_FROM_HANGING", 2, {
        "sSamusAnim_Suitless_Right_PullingYourselfUpFromHanging",
        "sSamusAnim_Suitless_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING", 2, {
        "sSamusAnim_Suitless_Right_PullingYourselfForwardFromHanging",
        "sSamusAnim_Suitless_Left_PullingYourselfForwardFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL", 2, {
        "sSamusAnim_Suitless_Right_PullingYourselfUpFromHanging",
        "sSamusAnim_Suitless_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_USING_AN_ELEVATOR", 2, {
        "sSamusAnim_FullSuit_UsingAnElevator",
        "sSamusAnim_FullSuit_UsingAnElevator"}},
    {"SPOSE_FACING_THE_FOREGROUND", 2, {
        "sSamusAnim_Suitless_FacingTheForeground",
        "sSamusAnim_Suitless_FacingTheForeground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_FOREGROUND", 2, {
        "sSamusAnim_Suitless_Right_TurningFromFacingTheForeground",
        "sSamusAnim_Suitless_Left_TurningFromFacingTheForeground"}},
    {"SPOSE_GRABBED_BY_CHOZO_STATUE", 2, {
        "sSamusAnim_FullSuit_MorphballMotionless",
        "sSamusAnim_FullSuit_MorphballMotionless"}},
    {"SPOSE_DELAY_BEFORE_SHINESPARKING", 2, {
        "sSamusAnim_FullSuit_Right_DelayBeforeShinesparking",
        "sSamusAnim_FullSuit_Left_DelayBeforeShinesparking"}},
    {"SPOSE_SHINESPARKING", 2, {
        "sSamusAnim_FullSuit_Right_Shinesparking",
        "sSamusAnim_FullSuit_Left_Shinesparking"}},
    {"SPOSE_SHINESPARK_COLLISION", 2, {
        "sSamusAnim_FullSuit_Right_Shinesparking",
        "sSamusAnim_FullSuit_Left_Shinesparking"}},
    {"SPOSE_DELAY_AFTER_SHINESPARKING", 2, {
        "sSamusAnim_FullSuit_Right_DelayAfterShinesparking",
        "sSamusAnim_FullSuit_Left_DelayAfterShinesparking"}},
    {"SPOSE_DELAY_BEFORE_BALLSPARKING", 2, {
        "sSamusAnim_FullSuit_MorphballMotionless",
        "sSamusAnim_FullSuit_MorphballMotionless"}},
    {"SPOSE_BALLSPARKING", 2, {
        "sSamusAnim_FullSuit_Right_Ballsparking",
        "sSamusAnim_FullSuit_Left_Ballsparking"}},
    {"SPOSE_BALLSPARK_COLLISION", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_ON_ZIPLINE", 2, {
        "sSamusAnim_FullSuit_Right_OnZipline",
        "sSamusAnim_FullSuit_Left_OnZipline"}},
    {"SPOSE_SHOOTING_ON_ZIPLINE", 2, {
        "sSamusAnim_FullSuit_Right_ShootingOnZipline",
        "sSamusAnim_FullSuit_Left_ShootingOnZipline"}},
    {"SPOSE_TURNING_ON_ZIPLINE", 2, {
        "sSamusAnim_FullSuit_Right_TurningOnZipline",
        "sSamusAnim_FullSuit_Left_TurningOnZipline"}},
    {"SPOSE_MORPH_BALL_ON_ZIPLINE", 2, {
        "sSamusAnim_FullSuit_MorphballMotionless",
        "sSamusAnim_FullSuit_MorphballMotionless"}},
    {"SPOSE_SAVING_LOADING_GAME", 2, {
        "sSamusAnim_Suitless_SavingLoadingGame",
        "sSamusAnim_Suitless_SavingLoadingGame"}},
    {"SPOSE_DOWNLOADING_MAP_DATA", 2, {
        "sSamusAnim_Suitless_DownloadingMapData",
        "sSamusAnim_Suitless_DownloadingMapData"}},
    {"SPOSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA", 2, {
        "sSamusAnim_Suitless_Right_TurningAround",
        "sSamusAnim_Suitless_Left_TurningAround"}},
    {"SPOSE_GETTING_HURT", 2, {
        "sSamusAnim_Suitless_Right_GettingKnockedBack",
        "sSamusAnim_Suitless_Left_GettingKnockedBack"}},
    {"SPOSE_GETTING_KNOCKED_BACK", 2, {
        "sSamusAnim_Suitless_Right_GettingKnockedBack",
        "sSamusAnim_Suitless_Left_GettingKnockedBack"}},
    {"SPOSE_GETTING_HURT_IN_MORPH_BALL", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL", 2, {
        "sSamusAnim_FullSuit_Right_Morphball",
        "sSamusAnim_FullSuit_Left_Morphball"}},
    {"SPOSE_DYING", 2, {
        "sSamusAnim_Suitless_Right_Dying",
        "sSamusAnim_Suitless_Left_Dying"}},
    {"SPOSE_CROUCHING_TO_CRAWL", 2, {
        "sSamusAnim_Suitless_Right_CrouchingToCrawl",
        "sSamusAnim_Suitless_Left_CrouchingToCrawl"}},
    {"SPOSE_CRAWLING_STOPPED", 2, {
        "sSamusAnim_Suitless_Right_CrawlingStopped",
        "sSamusAnim_Suitless_Left_CrawlingStopped"}},
    {"SPOSE_STARTING_TO_CRAWL", 2, {
        "sSamusAnim_Suitless_Right_StartingToCrawl",
        "sSamusAnim_Suitless_Left_StartingToCrawl"}},
    {"SPOSE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_Crawling",
        "sSamusAnim_Suitless_Left_Crawling"}},
    {"SPOSE_UNCROUCHING_FROM_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_UncrouchingFromCrawling",
        "sSamusAnim_Suitless_Left_UncrouchingFromCrawling"}},
    {"SPOSE_TURNING_AROUND_WHILE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_TurningAroundWhileCrawling",
        "sSamusAnim_Suitless_Left_TurningAroundWhileCrawling"}},
    {"SPOSE_SHOOTING_WHILE_CRAWLING", 2, {
        "sSamusAnim_Suitless_Right_ShootingWhileCrawling",
        "sSamusAnim_Suitless_Left_ShootingWhileCrawling"}},
    {"SPOSE_UNCROUCHING_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_UncrouchingSuitless",
        "sSamusAnim_Suitless_Left_UncrouchingSuitless"}},
    {"SPOSE_CROUCHING_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_CrouchingSuitless",
        "sSamusAnim_Suitless_Left_CrouchingSuitless"}},
    {"SPOSE_GRABBING_A_LEDGE_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_HangingOnLedgeSuitless",
        "sSamusAnim_Suitless_Left_HangingOnLedgeSuitless"}},
    {"SPOSE_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sSamusAnim_Suitless_FacingTheBackground",
        "sSamusAnim_Suitless_FacingTheBackground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sSamusAnim_Suitless_Right_TurningFromFacingTheBackground",
        "sSamusAnim_Suitless_Left_TurningFromFacingTheBackground"}},
    {"SPOSE_ACTIVATING_ZIPLINES", 2, {
        "sSamusAnim_FullSuit_MorphballMotionless",
        "sSamusAnim_FullSuit_MorphballMotionless"}},
    {"SPOSE_IN_ESCAPE_SHIP", 2, {
        "sSamusAnim_FullSuit_Right_Crouching",
        "sSamusAnim_FullSuit_Left_Crouching"}},
    {"SPOSE_TURNING_TO_ENTER_ESCAPE_SHIP", 2, {
        "sSamusAnim_FullSuit_Right_TurningAround",
        "sSamusAnim_FullSuit_Left_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_63[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_Forward_Running",
        "sSamusAnim_Suitless_Left_Forward_Running"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_Running",
        "sSamusAnim_Suitless_Left_DiagonalUp_Running"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_Running",
        "sSamusAnim_Suitless_Left_DiagonalDown_Running"}},
    {"ACD_NONE", 2, {
        "sSamusAnim_Suitless_Right_Running",
        "sSamusAnim_Suitless_Left_Running"}},
};
static const FusionMzmSamusTableRow rows_64[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_Standing",
        "sSamusAnim_Suitless_Left_Standing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_Standing",
        "sSamusAnim_Suitless_Left_DiagonalUp_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_Standing",
        "sSamusAnim_Suitless_Left_DiagonalDown_Standing"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_Standing",
        "sSamusAnim_Suitless_Left_Up_Standing"}},
};
static const FusionMzmSamusTableRow rows_65[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_TurningAround",
        "sSamusAnim_Suitless_Left_TurningAround"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_TurningAround",
        "sSamusAnim_Suitless_Left_DiagonalUp_TurningAround"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_TurningAround",
        "sSamusAnim_Suitless_Left_DiagonalDown_TurningAround"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_TurningAround",
        "sSamusAnim_Suitless_Left_Up_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_66[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_Shooting",
        "sSamusAnim_Suitless_Left_Shooting"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_Shooting",
        "sSamusAnim_Suitless_Left_DiagonalUp_Shooting"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_Shooting",
        "sSamusAnim_Suitless_Left_DiagonalDown_Shooting"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_Shooting",
        "sSamusAnim_Suitless_Left_Up_Shooting"}},
};
static const FusionMzmSamusTableRow rows_67[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_Crouching",
        "sSamusAnim_Suitless_Left_Crouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_Crouching",
        "sSamusAnim_Suitless_Left_DiagonalUp_Crouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_Crouching",
        "sSamusAnim_Suitless_Left_DiagonalDown_Crouching"}},
};
static const FusionMzmSamusTableRow rows_68[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_TurningAroundAndCrouching",
        "sSamusAnim_Suitless_Left_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_TurningAroundAndCrouching",
        "sSamusAnim_Suitless_Left_DiagonalUp_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_TurningAroundAndCrouching",
        "sSamusAnim_Suitless_Left_DiagonalDown_TurningAroundAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_69[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_ShootingAndCrouching",
        "sSamusAnim_Suitless_Left_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_ShootingAndCrouching",
        "sSamusAnim_Suitless_Left_DiagonalUp_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_ShootingAndCrouching",
        "sSamusAnim_Suitless_Left_DiagonalDown_ShootingAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_70[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_MidAir",
        "sSamusAnim_Suitless_Left_MidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_MidAir",
        "sSamusAnim_Suitless_Left_DiagonalUp_MidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_MidAir",
        "sSamusAnim_Suitless_Left_DiagonalDown_MidAir"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_MidAir",
        "sSamusAnim_Suitless_Left_Up_MidAir"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_Suitless_Right_Down_MidAir",
        "sSamusAnim_Suitless_Left_Down_MidAir"}},
};
static const FusionMzmSamusTableRow rows_71[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_TurningAroundMidAir",
        "sSamusAnim_Suitless_Left_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_TurningAroundMidAir",
        "sSamusAnim_Suitless_Left_DiagonalUp_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_TurningAroundMidAir",
        "sSamusAnim_Suitless_Left_DiagonalDown_TurningAroundMidAir"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_TurningAroundMidAir",
        "sSamusAnim_Suitless_Left_Up_TurningAroundMidAir"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_Suitless_Right_Down_TurningAroundMidAir",
        "sSamusAnim_Suitless_Left_Down_TurningAroundMidAir"}},
};
static const FusionMzmSamusTableRow rows_72[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_Landing",
        "sSamusAnim_Suitless_Left_Landing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_Landing",
        "sSamusAnim_Suitless_Left_DiagonalUp_Landing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_Landing",
        "sSamusAnim_Suitless_Left_DiagonalDown_Landing"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_Landing",
        "sSamusAnim_Suitless_Left_Up_Landing"}},
};
static const FusionMzmSamusTableRow rows_73[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_AimingWhileHanging",
        "sSamusAnim_Suitless_Left_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_AimingWhileHanging",
        "sSamusAnim_Suitless_Left_DiagonalUp_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_AimingWhileHanging",
        "sSamusAnim_Suitless_Left_DiagonalDown_AimingWhileHanging"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_AimingWhileHanging",
        "sSamusAnim_Suitless_Left_Up_AimingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_Suitless_Right_Down_AimingWhileHanging",
        "sSamusAnim_Suitless_Left_Down_AimingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_74[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_ShootingWhileHanging",
        "sSamusAnim_Suitless_Left_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_ShootingWhileHanging",
        "sSamusAnim_Suitless_Left_DiagonalUp_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_ShootingWhileHanging",
        "sSamusAnim_Suitless_Left_DiagonalDown_ShootingWhileHanging"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_ShootingWhileHanging",
        "sSamusAnim_Suitless_Left_Up_ShootingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sSamusAnim_Suitless_Right_Down_ShootingWhileHanging",
        "sSamusAnim_Suitless_Left_Down_ShootingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_75[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_UncrouchingSuitless",
        "sSamusAnim_Suitless_Left_UncrouchingSuitless"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_UncrouchingSuitless",
        "sSamusAnim_Suitless_Left_DiagonalUp_UncrouchingSuitless"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_UncrouchingSuitless",
        "sSamusAnim_Suitless_Left_DiagonalDown_UncrouchingSuitless"}},
    {"ACD_UP", 2, {
        "sSamusAnim_Suitless_Right_Up_UncrouchingSuitless",
        "sSamusAnim_Suitless_Left_Up_UncrouchingSuitless"}},
};
static const FusionMzmSamusTableRow rows_76[] = {
    {"ACD_FORWARD", 2, {
        "sSamusAnim_Suitless_Right_CrouchingSuitless",
        "sSamusAnim_Suitless_Left_CrouchingSuitless"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sSamusAnim_Suitless_Right_DiagonalUp_CrouchingSuitless",
        "sSamusAnim_Suitless_Left_DiagonalUp_CrouchingSuitless"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sSamusAnim_Suitless_Right_DiagonalDown_CrouchingSuitless",
        "sSamusAnim_Suitless_Left_DiagonalDown_CrouchingSuitless"}},
};
static const FusionMzmSamusTableRow rows_77[] = {
    {"FALSE", 2, {
        "sSamusAnim_Suitless_Right_CrawlingStopped",
        "sSamusAnim_Suitless_Left_CrawlingStopped"}},
    {"TRUE", 2, {
        "sSamusAnim_Suitless_Right_PistolUp_CrawlingStopped",
        "sSamusAnim_Suitless_Left_PistolUp_CrawlingStopped"}},
};
static const FusionMzmSamusTableRow rows_78[] = {
    {"SPOSE_RUNNING", 2, {
        "sArmCannonAnim_Suitless_Right_None_Running",
        "sArmCannonAnim_Suitless_Left_None_Running"}},
    {"SPOSE_STANDING", 2, {
        "sArmCannonAnim_Suitless_Right_Standing",
        "sArmCannonAnim_Suitless_Left_Standing"}},
    {"SPOSE_TURNING_AROUND", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAround",
        "sArmCannonAnim_Suitless_Left_TurningAround"}},
    {"SPOSE_SHOOTING", 2, {
        "sArmCannonAnim_Suitless_Right_Shooting",
        "sArmCannonAnim_Suitless_Left_Shooting"}},
    {"SPOSE_CROUCHING", 2, {
        "sArmCannonAnim_Suitless_Right_Crouching",
        "sArmCannonAnim_Suitless_Left_Crouching"}},
    {"SPOSE_TURNING_AROUND_AND_CROUCHING", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAroundAndCrouching",
        "sArmCannonAnim_Suitless_Left_TurningAroundAndCrouching"}},
    {"SPOSE_SHOOTING_AND_CROUCHING", 2, {
        "sArmCannonAnim_Suitless_Right_ShootingAndCrouching",
        "sArmCannonAnim_Suitless_Left_ShootingAndCrouching"}},
    {"SPOSE_SKIDDING", 2, {
        "sArmCannonAnim_Suit_Right_Skidding",
        "sArmCannonAnim_Suit_Left_Skidding"}},
    {"SPOSE_MIDAIR", 2, {
        "sArmCannonAnim_Suitless_Right_MidAir",
        "sArmCannonAnim_Suitless_Left_MidAir"}},
    {"SPOSE_TURNING_AROUND_MIDAIR", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAroundMidAir",
        "sArmCannonAnim_Suitless_Left_TurningAroundMidAir"}},
    {"SPOSE_LANDING", 2, {
        "sArmCannonAnim_Suitless_Right_Landing",
        "sArmCannonAnim_Suitless_Left_Landing"}},
    {"SPOSE_STARTING_SPIN_JUMP", 2, {
        "sArmCannonAnim_Suitless_Right_StartingSpinJump",
        "sArmCannonAnim_Suitless_Left_StartingSpinJump"}},
    {"SPOSE_SPINNING", 2, {
        "sArmCannonAnim_Suitless_Right_Spinning",
        "sArmCannonAnim_Suitless_Left_Spinning"}},
    {"SPOSE_STARTING_WALL_JUMP", 2, {
        "sArmCannonAnim_Suitless_Left_StartingWallJump",
        "sArmCannonAnim_Suitless_Right_StartingWallJump"}},
    {"SPOSE_SPACE_JUMPING", 2, {
        "sArmCannonAnim_Suit_Right_SpaceJumping",
        "sArmCannonAnim_Suit_Left_SpaceJumping"}},
    {"SPOSE_SCREW_ATTACKING", 2, {
        "sArmCannonAnim_Suit_Right_ScrewAttacking",
        "sArmCannonAnim_Suit_Left_ScrewAttacking"}},
    {"SPOSE_MORPHING", 2, {
        "sArmCannonAnim_Suit_Right_Morphing",
        "sArmCannonAnim_Suit_Left_Morphing"}},
    {"SPOSE_MORPH_BALL", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_ROLLING", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_UNMORPHING", 2, {
        "sArmCannonAnim_Suit_Right_Unmorphing",
        "sArmCannonAnim_Suit_Left_Unmorphing"}},
    {"SPOSE_MORPH_BALL_MIDAIR", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_HANGING_ON_LEDGE", 2, {
        "sArmCannonAnim_Suitless_Right_HangingOnLedge",
        "sArmCannonAnim_Suitless_Left_HangingOnLedge"}},
    {"SPOSE_TURNING_TO_AIM_WHILE_HANGING", 2, {
        "sArmCannonAnim_Suitless_Right_TurningToAimWhileHanging",
        "sArmCannonAnim_Suitless_Left_TurningToAimWhileHanging"}},
    {"SPOSE_HIDING_ARM_CANNON_WHILE_HANGING", 2, {
        "sArmCannonAnim_Suitless_Right_TurningToAimWhileHanging",
        "sArmCannonAnim_Suitless_Left_TurningToAimWhileHanging"}},
    {"SPOSE_AIMING_WHILE_HANGING", 2, {
        "sArmCannonAnim_Suitless_Right_AimingWhileHanging",
        "sArmCannonAnim_Suitless_Left_AimingWhileHanging"}},
    {"SPOSE_SHOOTING_WHILE_HANGING", 2, {
        "sArmCannonAnim_Suitless_Right_ShootingWhileHanging",
        "sArmCannonAnim_Suitless_Left_ShootingWhileHanging"}},
    {"SPOSE_PULLING_YOURSELF_UP_FROM_HANGING", 2, {
        "sArmCannonAnim_Suitless_Right_PullingYourselfUpFromHanging",
        "sArmCannonAnim_Suitless_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING", 2, {
        "sArmCannonAnim_Suitless_Right_PullingYourselfForwardFromHanging",
        "sArmCannonAnim_Suitless_Left_PullingYourselfForwardFromHanging"}},
    {"SPOSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL", 2, {
        "sArmCannonAnim_Suitless_Right_PullingYourselfUpFromHanging",
        "sArmCannonAnim_Suitless_Left_PullingYourselfUpFromHanging"}},
    {"SPOSE_USING_AN_ELEVATOR", 2, {
        "sArmCannonAnim_Suit_UsingAnElevator",
        "sArmCannonAnim_Suit_UsingAnElevator"}},
    {"SPOSE_FACING_THE_FOREGROUND", 2, {
        "sArmCannonAnim_Suitless_FacingTheForeground",
        "sArmCannonAnim_Suitless_FacingTheForeground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_FOREGROUND", 2, {
        "sArmCannonAnim_Suitless_Right_TurningFromFacingTheForeground",
        "sArmCannonAnim_Suitless_Left_TurningFromFacingTheForeground"}},
    {"SPOSE_GRABBED_BY_CHOZO_STATUE", 2, {
        "sArmCannonAnim_Suit_MorphballMotionless",
        "sArmCannonAnim_Suit_MorphballMotionless"}},
    {"SPOSE_DELAY_BEFORE_SHINESPARKING", 2, {
        "sArmCannonAnim_Suit_Right_DelayBeforeShinesparking",
        "sArmCannonAnim_Suit_Left_DelayBeforeShinesparking"}},
    {"SPOSE_SHINESPARKING", 2, {
        "sArmCannonAnim_Suit_Right_Shinesparking",
        "sArmCannonAnim_Suit_Left_Shinesparking"}},
    {"SPOSE_SHINESPARK_COLLISION", 2, {
        "sArmCannonAnim_Suit_Right_Shinesparking",
        "sArmCannonAnim_Suit_Left_Shinesparking"}},
    {"SPOSE_DELAY_AFTER_SHINESPARKING", 2, {
        "sArmCannonAnim_Suit_Right_DelayAfterShinesparking",
        "sArmCannonAnim_Suit_Left_DelayAfterShinesparking"}},
    {"SPOSE_DELAY_BEFORE_BALLSPARKING", 2, {
        "sArmCannonAnim_Suit_MorphballMotionless",
        "sArmCannonAnim_Suit_MorphballMotionless"}},
    {"SPOSE_BALLSPARKING", 2, {
        "sArmCannonAnim_Suit_Right_Ballsparking",
        "sArmCannonAnim_Suit_Left_Ballsparking"}},
    {"SPOSE_BALLSPARK_COLLISION", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_ON_ZIPLINE", 2, {
        "sArmCannonAnim_Suit_Right_OnZipline",
        "sArmCannonAnim_Suit_Left_OnZipline"}},
    {"SPOSE_SHOOTING_ON_ZIPLINE", 2, {
        "sArmCannonAnim_Suit_Right_ShootingOnZipline",
        "sArmCannonAnim_Suit_Left_ShootingOnZipline"}},
    {"SPOSE_TURNING_ON_ZIPLINE", 2, {
        "sArmCannonAnim_Suit_Right_TurningOnZipline",
        "sArmCannonAnim_Suit_Left_TurningOnZipline"}},
    {"SPOSE_MORPH_BALL_ON_ZIPLINE", 2, {
        "sArmCannonAnim_Suit_MorphballMotionless",
        "sArmCannonAnim_Suit_MorphballMotionless"}},
    {"SPOSE_SAVING_LOADING_GAME", 2, {
        "sArmCannonAnim_Suitless_SavingLoadingGame",
        "sArmCannonAnim_Suitless_SavingLoadingGame"}},
    {"SPOSE_DOWNLOADING_MAP_DATA", 2, {
        "sArmCannonAnim_Suitless_DownloadingMapData",
        "sArmCannonAnim_Suitless_DownloadingMapData"}},
    {"SPOSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAround",
        "sArmCannonAnim_Suitless_Left_TurningAround"}},
    {"SPOSE_GETTING_HURT", 2, {
        "sArmCannonAnim_Suit_Right_GettingHurt",
        "sArmCannonAnim_Suit_Left_GettingHurt"}},
    {"SPOSE_GETTING_KNOCKED_BACK", 2, {
        "sArmCannonAnim_Suit_Right_GettingHurt",
        "sArmCannonAnim_Suit_Left_GettingHurt"}},
    {"SPOSE_GETTING_HURT_IN_MORPH_BALL", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL", 2, {
        "sArmCannonAnim_Suit_Right_Morphball",
        "sArmCannonAnim_Suit_Left_Morphball"}},
    {"SPOSE_DYING", 2, {
        "sArmCannonAnim_Suit_Dying",
        "sArmCannonAnim_Suit_Dying"}},
    {"SPOSE_CROUCHING_TO_CRAWL", 2, {
        "sArmCannonAnim_Suitless_Right_CrouchingToCrawl",
        "sArmCannonAnim_Suitless_Left_CrouchingToCrawl"}},
    {"SPOSE_CRAWLING_STOPPED", 2, {
        "sArmCannonAnim_Suitless_Right_CrawlingStopped",
        "sArmCannonAnim_Suitless_Left_CrawlingStopped"}},
    {"SPOSE_STARTING_TO_CRAWL", 2, {
        "sArmCannonAnim_Suitless_Right_StartingToCrawl",
        "sArmCannonAnim_Suitless_Left_StartingToCrawl"}},
    {"SPOSE_CRAWLING", 2, {
        "sArmCannonAnim_Suitless_Right_Crawling",
        "sArmCannonAnim_Suitless_Left_Crawling"}},
    {"SPOSE_UNCROUCHING_FROM_CRAWLING", 2, {
        "sArmCannonAnim_Suitless_Right_UncrouchingFromCrawling",
        "sArmCannonAnim_Suitless_Left_UncrouchingFromCrawling"}},
    {"SPOSE_TURNING_AROUND_WHILE_CRAWLING", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAroundWhileCrawling",
        "sArmCannonAnim_Suitless_Left_TurningAroundWhileCrawling"}},
    {"SPOSE_SHOOTING_WHILE_CRAWLING", 2, {
        "sArmCannonAnim_Suitless_Right_ShootingWhileCrawling",
        "sArmCannonAnim_Suitless_Left_ShootingWhileCrawling"}},
    {"SPOSE_UNCROUCHING_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_Right_UncrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_UncrouchingSuitless"}},
    {"SPOSE_CROUCHING_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_Right_CrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_CrouchingSuitless"}},
    {"SPOSE_GRABBING_A_LEDGE_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_Right_HangingOnLedgeSuitless",
        "sArmCannonAnim_Suitless_Left_HangingOnLedgeSuitless"}},
    {"SPOSE_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_FacingTheBackground",
        "sArmCannonAnim_Suitless_FacingTheBackground"}},
    {"SPOSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS", 2, {
        "sArmCannonAnim_Suitless_Right_TurningFromFacingTheBackground",
        "sArmCannonAnim_Suitless_Left_TurningFromFacingTheBackground"}},
    {"SPOSE_ACTIVATING_ZIPLINES", 2, {
        "sArmCannonAnim_Suit_MorphballMotionless",
        "sArmCannonAnim_Suit_MorphballMotionless"}},
    {"SPOSE_IN_ESCAPE_SHIP", 2, {
        "sArmCannonAnim_Suit_Right_Crouching",
        "sArmCannonAnim_Suit_Left_Crouching"}},
    {"SPOSE_TURNING_TO_ENTER_ESCAPE_SHIP", 2, {
        "sArmCannonAnim_Suit_Right_TurningAround",
        "sArmCannonAnim_Suit_Left_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_79[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_Forward_Running",
        "sArmCannonAnim_Suitless_Left_Forward_Running"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_Running",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_Running"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_Running",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_Running"}},
    {"ACD_NONE", 2, {
        "sArmCannonAnim_Suitless_Right_None_Running",
        "sArmCannonAnim_Suitless_Left_None_Running"}},
};
static const FusionMzmSamusTableRow rows_80[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_Standing",
        "sArmCannonAnim_Suitless_Left_Standing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_Standing",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_Standing",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_Standing"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_Standing",
        "sArmCannonAnim_Suitless_Left_Up_Standing"}},
};
static const FusionMzmSamusTableRow rows_81[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAround",
        "sArmCannonAnim_Suitless_Left_TurningAround"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_TurningAround",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_TurningAround"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_TurningAround",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_TurningAround"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_TurningAround",
        "sArmCannonAnim_Suitless_Left_Up_TurningAround"}},
};
static const FusionMzmSamusTableRow rows_82[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_Shooting",
        "sArmCannonAnim_Suitless_Left_Shooting"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_Shooting",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_Shooting"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_Shooting",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_Shooting"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_Shooting",
        "sArmCannonAnim_Suitless_Left_Up_Shooting"}},
};
static const FusionMzmSamusTableRow rows_83[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_Crouching",
        "sArmCannonAnim_Suitless_Left_Crouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_Crouching",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_Crouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_Crouching",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_Crouching"}},
};
static const FusionMzmSamusTableRow rows_84[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAroundAndCrouching",
        "sArmCannonAnim_Suitless_Left_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_TurningAroundAndCrouching",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_TurningAroundAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_TurningAroundAndCrouching",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_TurningAroundAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_85[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_ShootingAndCrouching",
        "sArmCannonAnim_Suitless_Left_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_ShootingAndCrouching",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_ShootingAndCrouching"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_ShootingAndCrouching",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_ShootingAndCrouching"}},
};
static const FusionMzmSamusTableRow rows_86[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_MidAir",
        "sArmCannonAnim_Suitless_Left_MidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_MidAir",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_MidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_MidAir",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_MidAir"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_MidAir",
        "sArmCannonAnim_Suitless_Left_Up_MidAir"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_Down_MidAir",
        "sArmCannonAnim_Suitless_Left_Down_MidAir"}},
};
static const FusionMzmSamusTableRow rows_87[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_TurningAroundMidAir",
        "sArmCannonAnim_Suitless_Left_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_TurningAroundMidAir",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_TurningAroundMidAir"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_TurningAroundMidAir",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_TurningAroundMidAir"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_TurningAroundMidAir",
        "sArmCannonAnim_Suitless_Left_Up_TurningAroundMidAir"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_Down_TurningAroundMidAir",
        "sArmCannonAnim_Suitless_Left_Down_TurningAroundMidAir"}},
};
static const FusionMzmSamusTableRow rows_88[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_Landing",
        "sArmCannonAnim_Suitless_Left_Landing"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_Landing",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_Landing"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_Landing",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_Landing"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_Landing",
        "sArmCannonAnim_Suitless_Left_Up_Landing"}},
};
static const FusionMzmSamusTableRow rows_89[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_AimingWhileHanging",
        "sArmCannonAnim_Suitless_Left_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_AimingWhileHanging",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_AimingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_AimingWhileHanging",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_AimingWhileHanging"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_AimingWhileHanging",
        "sArmCannonAnim_Suitless_Left_Up_AimingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_Down_AimingWhileHanging",
        "sArmCannonAnim_Suitless_Left_Down_AimingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_90[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_ShootingWhileHanging",
        "sArmCannonAnim_Suitless_Left_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_ShootingWhileHanging",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_ShootingWhileHanging"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_ShootingWhileHanging",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_ShootingWhileHanging"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_ShootingWhileHanging",
        "sArmCannonAnim_Suitless_Left_Up_ShootingWhileHanging"}},
    {"ACD_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_Down_ShootingWhileHanging",
        "sArmCannonAnim_Suitless_Left_Down_ShootingWhileHanging"}},
};
static const FusionMzmSamusTableRow rows_91[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_UncrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_UncrouchingSuitless"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_UncrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_UncrouchingSuitless"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_UncrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_UncrouchingSuitless"}},
    {"ACD_UP", 2, {
        "sArmCannonAnim_Suitless_Right_Up_UncrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_Up_UncrouchingSuitless"}},
};
static const FusionMzmSamusTableRow rows_92[] = {
    {"ACD_FORWARD", 2, {
        "sArmCannonAnim_Suitless_Right_CrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_CrouchingSuitless"}},
    {"ACD_DIAGONALLY_UP", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalUp_CrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_DiagonalUp_CrouchingSuitless"}},
    {"ACD_DIAGONALLY_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_DiagonalDown_CrouchingSuitless",
        "sArmCannonAnim_Suitless_Left_DiagonalDown_CrouchingSuitless"}},
};
static const FusionMzmSamusTableRow rows_93[] = {
    {"FORCED_MOVEMENT_CRAWLING_ARM_CANNON_DOWN", 2, {
        "sArmCannonAnim_Suitless_Right_CrawlingStopped",
        "sArmCannonAnim_Suitless_Left_CrawlingStopped"}},
    {"FORCED_MOVEMENT_CRAWLING_ARM_CANNON_UP", 2, {
        "sArmCannonAnim_Suitless_Right_PistolUp_CrawlingStopped",
        "sArmCannonAnim_Suitless_Left_PistolUp_CrawlingStopped"}},
};
static const FusionMzmSamusTableRow rows_94[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Standing"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Standing"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Standing"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Standing"}},
};
static const FusionMzmSamusTableRow rows_95[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Standing"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Standing"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Standing"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Standing"}},
};
static const FusionMzmSamusTableRow rows_96[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Armed_Standing"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Armed_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Armed_Standing"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Right_Armed_Default"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Right_Armed_Default"}},
};
static const FusionMzmSamusTableRow rows_97[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Armed_Standing"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Armed_Standing"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Armed_Standing"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Standing"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Armed_Down"}},
};
static const FusionMzmSamusTableRow rows_98[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Right_Default"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Right_Default"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Right_Default"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Standing"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Standing"}},
};
static const FusionMzmSamusTableRow rows_99[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Right_Default"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Right_Default"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Right_Default"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Standing"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Standing"}},
};
static const FusionMzmSamusTableRow rows_100[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Left_Default"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Left_Default"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Left_Default"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Left_Default"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Left_Default"}},
};
static const FusionMzmSamusTableRow rows_101[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Left_Default"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Left_Default"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Left_Default"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Left_Default"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Left_Default"}},
};
static const FusionMzmSamusTableRow rows_102[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Right_Armed_Default"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Right_Armed_Default"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Right_Armed_Default"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Right_Armed_Default"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Right_Armed_Default"}},
};
static const FusionMzmSamusTableRow rows_103[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Right_Armed_Default"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Right_Armed_Default"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Right_Armed_Default"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Standing"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Armed_Down"}},
};
static const FusionMzmSamusTableRow rows_104[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Left_Armed_Default"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Left_Armed_Default"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Left_Armed_Default"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Left_Armed_Default"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Left_Armed_Default"}},
};
static const FusionMzmSamusTableRow rows_105[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Left_Armed_Default"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Left_Armed_Default"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Left_Armed_Default"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Left_Armed_Default"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Left_Armed_Default"}},
};
static const FusionMzmSamusTableRow rows_106[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Left_Hanging"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Left_Hanging"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Left_Hanging"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Left_Hanging"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Left_Hanging"}},
};
static const FusionMzmSamusTableRow rows_107[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Left_Hanging"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Left_Hanging"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Left_Hanging"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Left_Hanging"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Left_Hanging"}},
};
static const FusionMzmSamusTableRow rows_108[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Right_Hanging"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Right_OnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Right_Hanging"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Right_OnZipline"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Right_Hanging"}},
};
static const FusionMzmSamusTableRow rows_109[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Right_Hanging"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Right_OnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Right_Hanging"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Right_OnZipline"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Right_Hanging"}},
};
static const FusionMzmSamusTableRow rows_110[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Left_Armed_Hanging"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Left_Armed_Hanging"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Left_Armed_Hanging"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Left_Armed_Hanging"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Left_Armed_Hanging"}},
};
static const FusionMzmSamusTableRow rows_111[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Left_Armed_Hanging"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Left_Armed_Hanging"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Left_Armed_Hanging"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Left_Armed_Hanging"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Left_Armed_Hanging"}},
};
static const FusionMzmSamusTableRow rows_112[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Right_Armed_Hanging"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Right_Armed_Hanging"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Right_Armed_Hanging"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Right_Armed_Hanging"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Right_Armed_Hanging"}},
};
static const FusionMzmSamusTableRow rows_113[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Right_Armed_Hanging"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Right_Armed_Hanging"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Right_Armed_Hanging"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Right_Armed_Hanging"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Right_Armed_Hanging"}},
};
static const FusionMzmSamusTableRow rows_114[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Right_OnZipline"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Right_OnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Right_OnZipline"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Right_OnZipline"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Right_OnZipline"}},
};
static const FusionMzmSamusTableRow rows_115[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Right_OnZipline"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Right_OnZipline"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Right_OnZipline"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Right_OnZipline"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Right_OnZipline"}},
};
static const FusionMzmSamusTableRow rows_116[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Upper_Forward_Right_Armed_OnZipline"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Upper_DiagonalUp_Right_Armed_Hanging"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Upper_DiagonalDown_Right_Armed_OnZipline"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Upper_Up_Right_Armed_Hanging"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Upper_Down_Right_Armed_OnZipline"}},
};
static const FusionMzmSamusTableRow rows_117[] = {
    {"ACD_FORWARD", 1, {
        "sArmCannonGfx_Lower_Forward_Right_Armed_OnZipline"}},
    {"ACD_DIAGONALLY_UP", 1, {
        "sArmCannonGfx_Lower_DiagonalUp_Right_Armed_Hanging"}},
    {"ACD_DIAGONALLY_DOWN", 1, {
        "sArmCannonGfx_Lower_DiagonalDown_Right_Armed_OnZipline"}},
    {"ACD_UP", 1, {
        "sArmCannonGfx_Lower_Up_Right_Armed_Hanging"}},
    {"ACD_DOWN", 1, {
        "sArmCannonGfx_Lower_Down_Right_Armed_OnZipline"}},
};

const FusionMzmSamusTable fusion_mzm_samus_tables[] = {
    {"sSamusAnimPointers_PowerSuit", rows_0, 67},
    {"sSamusAnimPointers_PowerSuit_Running", rows_1, 4},
    {"sSamusAnimPointers_PowerSuit_Running_Speedboosting", rows_2, 4},
    {"sSamusAnimPointers_PowerSuit_Standing", rows_3, 4},
    {"sSamusAnimPointers_PowerSuit_TurningAround", rows_4, 4},
    {"sSamusAnimPointers_PowerSuit_Shooting", rows_5, 4},
    {"sSamusAnimPointers_PowerSuit_Crouching", rows_6, 3},
    {"sSamusAnimPointers_PowerSuit_TurningAroundAndCrouching", rows_7, 3},
    {"sSamusAnimPointers_PowerSuit_ShootingAndCrouching", rows_8, 3},
    {"sSamusAnimPointers_PowerSuit_Skidding", rows_9, 2},
    {"sSamusAnimPointers_PowerSuit_MidAir", rows_10, 5},
    {"sSamusAnimPointers_PowerSuit_TurningAroundMidAir", rows_11, 5},
    {"sSamusAnimPointers_PowerSuit_Landing", rows_12, 4},
    {"sSamusAnimPointers_PowerSuit_ScrewAttacking", rows_13, 2},
    {"sSamusAnimPointers_PowerSuit_AimingWhileHanging", rows_14, 5},
    {"sSamusAnimPointers_PowerSuit_ShootingWhileHanging", rows_15, 5},
    {"sSamusAnimPointers_PowerSuit_UsingAnElevator", rows_16, 2},
    {"sSamusAnimPointers_PowerSuit_Shinesparking", rows_17, 3},
    {"sSamusAnimPointers_PowerSuit_OnZipline", rows_18, 5},
    {"sSamusAnimPointers_PowerSuit_ShootingOnZipline", rows_19, 5},
    {"sSamusAnimPointers_PowerSuit_TurningOnZipline", rows_20, 5},
    {"sArmCannonAnimPointers_Suit_All", rows_21, 67},
    {"sArmCannonAnimPointers_Suit_Running", rows_22, 4},
    {"sArmCannonAnimPointers_Suit_Running_Speedboosting", rows_23, 4},
    {"sArmCannonAnimPointers_Suit_Standing", rows_24, 4},
    {"sArmCannonAnimPointers_Suit_TurningAround", rows_25, 4},
    {"sArmCannonAnimPointers_Suit_Shooting", rows_26, 4},
    {"sArmCannonAnimPointers_Suit_Crouching", rows_27, 3},
    {"sArmCannonAnimPointers_Suit_TurningAroundAndCrouching", rows_28, 3},
    {"sArmCannonAnimPointers_Suit_ShootingAndCrouching", rows_29, 3},
    {"sArmCannonAnimPointers_Suit_Skidding", rows_30, 2},
    {"sArmCannonAnimPointers_Suit_MidAir", rows_31, 5},
    {"sArmCannonAnimPointers_Suit_TurningAroundMidAir", rows_32, 5},
    {"sArmCannonAnimPointers_Suit_Landing", rows_33, 4},
    {"sArmCannonAnimPointers_Suit_AimingWhileHanging", rows_34, 5},
    {"sArmCannonAnimPointers_Suit_ShootingWhileHanging", rows_35, 5},
    {"sArmCannonAnimPointers_Suit_UsingAnElevator", rows_36, 2},
    {"sArmCannonAnimPointers_Suit_Shinesparking", rows_37, 3},
    {"sArmCannonAnimPointers_Suit_OnZipline", rows_38, 5},
    {"sArmCannonAnimPointers_Suit_ShootingOnZipline", rows_39, 5},
    {"sArmCannonAnimPointers_Suit_TurningOnZipline", rows_40, 5},
    {"sSamusAnimPointers_FullSuit", rows_41, 67},
    {"sSamusAnimPointers_FullSuit_Running", rows_42, 4},
    {"sSamusAnimPointers_FullSuit_Running_Speedboosting", rows_43, 4},
    {"sSamusAnimPointers_FullSuit_Standing", rows_44, 4},
    {"sSamusAnimPointers_FullSuit_TurningAround", rows_45, 4},
    {"sSamusAnimPointers_FullSuit_Shooting", rows_46, 4},
    {"sSamusAnimPointers_FullSuit_Crouching", rows_47, 3},
    {"sSamusAnimPointers_FullSuit_TurningAroundAndCrouching", rows_48, 3},
    {"sSamusAnimPointers_FullSuit_ShootingAndCrouching", rows_49, 3},
    {"sSamusAnimPointers_FullSuit_Skidding", rows_50, 2},
    {"sSamusAnimPointers_FullSuit_MidAir", rows_51, 5},
    {"sSamusAnimPointers_FullSuit_TurningAroundMidAir", rows_52, 5},
    {"sSamusAnimPointers_FullSuit_Landing", rows_53, 4},
    {"sSamusAnimPointers_FullSuit_ScrewAttacking", rows_54, 2},
    {"sSamusAnimPointers_FullSuit_AimingWhileHanging", rows_55, 5},
    {"sSamusAnimPointers_FullSuit_ShootingWhileHanging", rows_56, 5},
    {"sSamusAnimPointers_FullSuit_UsingAnElevator", rows_57, 2},
    {"sSamusAnimPointers_FullSuit_Shinesparking", rows_58, 3},
    {"sSamusAnimPointers_FullSuit_OnZipline", rows_59, 5},
    {"sSamusAnimPointers_FullSuit_ShootingOnZipline", rows_60, 5},
    {"sSamusAnimPointers_FullSuit_TurningOnZipline", rows_61, 5},
    {"sSamusAnimPointers_Suitless", rows_62, 67},
    {"sSamusAnimPointers_Suitless_Running", rows_63, 4},
    {"sSamusAnimPointers_Suitless_Standing", rows_64, 4},
    {"sSamusAnimPointers_Suitless_TurningAround", rows_65, 4},
    {"sSamusAnimPointers_Suitless_Shooting", rows_66, 4},
    {"sSamusAnimPointers_Suitless_Crouching", rows_67, 3},
    {"sSamusAnimPointers_Suitless_TurningAroundAndCrouching", rows_68, 3},
    {"sSamusAnimPointers_Suitless_ShootingAndCrouching", rows_69, 3},
    {"sSamusAnimPointers_Suitless_MidAir", rows_70, 5},
    {"sSamusAnimPointers_Suitless_TurningAroundMidAir", rows_71, 5},
    {"sSamusAnimPointers_Suitless_Landing", rows_72, 4},
    {"sSamusAnimPointers_Suitless_AimingWhileHanging", rows_73, 5},
    {"sSamusAnimPointers_Suitless_ShootingWhileHanging", rows_74, 5},
    {"sSamusAnimPointers_Suitless_UncrouchingSuitless", rows_75, 4},
    {"sSamusAnimPointers_Suitless_CrouchingSuitless", rows_76, 3},
    {"sSamusAnimPointers_Suitless_CrawlingStopped", rows_77, 2},
    {"sArmCannonAnimPointers_Suitless_All", rows_78, 67},
    {"sArmCannonAnimPointers_Suitless_Running", rows_79, 4},
    {"sArmCannonAnimPointers_Suitless_Standing", rows_80, 4},
    {"sArmCannonAnimPointers_Suitless_TurningAround", rows_81, 4},
    {"sArmCannonAnimPointers_Suitless_Shooting", rows_82, 4},
    {"sArmCannonAnimPointers_Suitless_Crouching", rows_83, 3},
    {"sArmCannonAnimPointers_Suitless_TurningAroundAndCrouching", rows_84, 3},
    {"sArmCannonAnimPointers_Suitless_ShootingAndCrouching", rows_85, 3},
    {"sArmCannonAnimPointers_Suitless_MidAir", rows_86, 5},
    {"sArmCannonAnimPointers_Suitless_TurningAroundMidAir", rows_87, 5},
    {"sArmCannonAnimPointers_Suitless_Landing", rows_88, 4},
    {"sArmCannonAnimPointers_Suitless_AimingWhileHanging", rows_89, 5},
    {"sArmCannonAnimPointers_Suitless_ShootingWhileHanging", rows_90, 5},
    {"sArmCannonAnimPointers_Suitless_UncrouchingSuitless", rows_91, 4},
    {"sArmCannonAnimPointers_Suitless_CrouchingSuitless", rows_92, 3},
    {"sArmCannonAnimPointers_Suitless_CrawlingStopped", rows_93, 2},
    {"sArmCannonGfxPointers_Upper_Standing", rows_94, 5},
    {"sArmCannonGfxPointers_Lower_Standing", rows_95, 5},
    {"sArmCannonGfxPointers_Upper_Armed_Standing", rows_96, 5},
    {"sArmCannonGfxPointers_Lower_Armed_Standing", rows_97, 5},
    {"sArmCannonGfxPointers_Upper_Right_Default", rows_98, 5},
    {"sArmCannonGfxPointers_Lower_Right_Default", rows_99, 5},
    {"sArmCannonGfxPointers_Upper_Left_Default", rows_100, 5},
    {"sArmCannonGfxPointers_Lower_Left_Default", rows_101, 5},
    {"sArmCannonGfxPointers_Upper_Right_Armed_Default", rows_102, 5},
    {"sArmCannonGfxPointers_Lower_Right_Armed_Default", rows_103, 5},
    {"sArmCannonGfxPointers_Upper_Left_Armed_Default", rows_104, 5},
    {"sArmCannonGfxPointers_Lower_Left_Armed_Default", rows_105, 5},
    {"sArmCannonGfxPointers_Upper_Left_Hanging", rows_106, 5},
    {"sArmCannonGfxPointers_Lower_Left_Hanging", rows_107, 5},
    {"sArmCannonGfxPointers_Upper_Right_Hanging", rows_108, 5},
    {"sArmCannonGfxPointers_Lower_Right_Hanging", rows_109, 5},
    {"sArmCannonGfxPointers_Upper_Left_Armed_Hanging", rows_110, 5},
    {"sArmCannonGfxPointers_Lower_Left_Armed_Hanging", rows_111, 5},
    {"sArmCannonGfxPointers_Upper_Right_Armed_Hanging", rows_112, 5},
    {"sArmCannonGfxPointers_Lower_Right_Armed_Hanging", rows_113, 5},
    {"sArmCannonGfxPointers_Upper_Right_OnZipline", rows_114, 5},
    {"sArmCannonGfxPointers_Lower_Right_OnZipline", rows_115, 5},
    {"sArmCannonGfxPointers_Upper_Right_Armed_OnZipline", rows_116, 5},
    {"sArmCannonGfxPointers_Lower_Right_Armed_OnZipline", rows_117, 5},
};
const size_t fusion_mzm_samus_table_count =
    sizeof fusion_mzm_samus_tables / sizeof fusion_mzm_samus_tables[0];

const char *const fusion_mzm_projectile_frame_tables[] = {
    "sBombOam_Slow",
    "sBombOam_Fast",
    "sPowerBombOam_Slow",
    "sPowerBombOam_Fast",
    "sMissileOam_Horizontal",
    "sMissileOam_Diagonal",
    "sMissileOam_Vertical",
    "sSuperMissileOam_Horizontal",
    "sSuperMissileOam_Diagonal",
    "sSuperMissileOam_Vertical",
    "sMissileOam_Tumbling",
    "sSuperMissileOam_Tumbling",
    "sNormalBeamOam_Horizontal",
    "sNormalBeamOam_Diagonal",
    "sNormalBeamOam_Vertical",
    "sChargedNormalBeamOam_Horizontal",
    "sChargedNormalBeamOam_Diagonal",
    "sChargedNormalBeamOam_Vertical",
    "sLongBeamOam_Horizontal",
    "sLongBeamOam_Diagonal",
    "sLongBeamOam_Vertical",
    "sChargedLongBeamOam_Horizontal",
    "sChargedLongBeamOam_Diagonal",
    "sChargedLongBeamOam_Vertical",
    "sIceBeamOam_Horizontal",
    "sIceBeamOam_Diagonal",
    "sIceBeamOam_Vertical",
    "sChargedIceBeamOam_Horizontal",
    "sChargedIceBeamOam_Diagonal",
    "sChargedIceBeamOam_Vertical",
    "sWaveBeamOam_Horizontal",
    "sWaveBeamOam_Diagonal",
    "sWaveBeamOam_Vertical",
    "sChargedWaveBeamOam_Horizontal",
    "sChargedWaveBeamOam_Diagonal",
    "sChargedWaveBeamOam_Vertical",
    "sPlasmaBeamOam_Horizontal_NoWave",
    "sPlasmaBeamOam_Diagonal_NoWave",
    "sPlasmaBeamOam_Vertical_NoWave",
    "sChargedPlasmaBeamOam_Horizontal_NoWave",
    "sChargedPlasmaBeamOam_Diagonal_NoWave",
    "sChargedPlasmaBeamOam_Vertical_NoWave",
    "sPlasmaBeamOam_Horizontal_Wave",
    "sPlasmaBeamOam_Diagonal_Wave",
    "sPlasmaBeamOam_Vertical_Wave",
    "sChargedPlasmaBeamOam_Horizontal_Wave",
    "sChargedPlasmaBeamOam_Diagonal_Wave",
    "sChargedPlasmaBeamOam_Vertical_Wave",
    "sPistolOam_Horizontal",
    "sPistolOam_Diagonal",
    "sPistolOam_Vertical",
    "sChargedPistolOam_Horizontal",
    "sChargedPistolOam_Diagonal",
    "sChargedPistolOam_Vertical",
};
const size_t fusion_mzm_projectile_frame_table_count =
    sizeof fusion_mzm_projectile_frame_tables /
    sizeof fusion_mzm_projectile_frame_tables[0];
