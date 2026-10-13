/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Samus controller, transliterated from the pinned mzm
 * decompilation's src/samus.c. Comments name the routine each function
 * reproduces. Inside an update the position is native (the public `y` minus
 * one); positions wrap as the native u16 fields do. */
#include "mzm_samus.h"
#include "mzm_sfx.h"

#include <stdio.h>
#include <string.h>

/* Native key bits (include/gba/keys.h), kept by currentSlope and
 * lastWallTouchedMidAir. */
enum { NK_RIGHT = 1u << 4, NK_LEFT = 1u << 5, NK_UP = 1u << 6, NK_DOWN = 1u << 7 };
/* SlopeType */
enum { SLOPE_NONE = 0, SLOPE_STEEP = 1, SLOPE_SLIGHT = 2 };
/* HorizontalMovingDir / VerticalMovingDir */
enum { HDMOVING_NONE, HDMOVING_LEFT, HDMOVING_RIGHT };
enum { VDMOVING_NONE, VDMOVING_UP, VDMOVING_DOWN };
/* SamusCollisionDetection */
enum {
    DETECT_NONE = 0,
    DETECT_LEFT_MOST = 1 << 0,
    DETECT_MIDDLE_LEFT = 1 << 1,
    DETECT_MIDDLE_RIGHT = 1 << 2,
    DETECT_RIGHT_MOST = 1 << 3,
    DETECT_SLOPE = 1 << 7,
    DETECT_MIDDLE = DETECT_MIDDLE_LEFT | DETECT_MIDDLE_RIGHT,
};
/* ClipdataType and CollisionResult values the probes compare. */
enum {
    CLIP_AIR, CLIP_SOLID, CLIP_LEFT_STEEP, CLIP_RIGHT_STEEP, CLIP_LEFT_UPPER_SLIGHT,
    CLIP_LEFT_LOWER_SLIGHT, CLIP_RIGHT_LOWER_SLIGHT, CLIP_RIGHT_UPPER_SLIGHT,
    CLIP_PASS_THROUGH_BOTTOM = 12,
    COLLISION_PASS_THROUGH_BOTTOM = 16,
};
/* SamusAnimState */
enum { ANIM_NONE, ANIM_SUB_ENDED, ANIM_ENDED };
/* WantingEffect */
enum {
    WANTING_RUNNING_EFFECT, WANTING_RUNNING_EFFECT_, WANTING_LANDING_EFFECT,
    WANTING_GOING_OUT_OF_LIQUID_EFFECT, WANTING_RUNNING_OUT_OF_LIQUID_EFFECT,
    WANTING_BREATHING_BUBBLES, WANTING_SKIDDING_EFFECT, WANTING_RUNNING_ON_WET_GROUND
};
/* gScrewSpeedAnimation.flag */
enum { SCREW_SPEED_FLAG_NONE, SCREW_SPEED_FLAG_ACTIVE, SCREW_SPEED_FLAG_STORING_SHINESPARK = 8 };
/* CHARGE_BEAM_THRESHOLD (unused here: the weapons own the charge). */
#define SUB_PIXEL_POSITION_FLAG 63
#define BLOCK_POSITION_FLAG 0xFFC0
#define U16(v) ((int32_t)(uint16_t)(v))

/* sSamusBlockHitboxData */
static const MzmBlockHitbox block_hitboxes[] = {
    [MZM_HITBOX_STANDING] = {-(MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                             -(MZM_BLOCK_SIZE * 2 - MZM_PIXEL_SIZE),
                             MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE},
    [MZM_HITBOX_CROUCHED] = {-(MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                             -(MZM_BLOCK_SIZE + MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                             MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE},
    [MZM_HITBOX_MORPHED] = {-(MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                            -(MZM_BLOCK_SIZE - MZM_PIXEL_SIZE),
                            MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE},
    /* The zipline poses use the standing block hitbox. */
    [MZM_HITBOX_ZIPLINE] = {-(MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                            -(MZM_BLOCK_SIZE * 2 - MZM_PIXEL_SIZE),
                            MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE},
};
/* sSamusBlockHitboxData_Above */
static const int16_t above_left = -(MZM_EIGHTH_BLOCK_SIZE + MZM_PIXEL_SIZE / 2);
static const int16_t above_right = MZM_EIGHTH_BLOCK_SIZE + MZM_PIXEL_SIZE / 2;
#define STANDING_TOP (block_hitboxes[MZM_HITBOX_STANDING].top)
#define STANDING_LEFT (block_hitboxes[MZM_HITBOX_STANDING].left)
#define STANDING_RIGHT (block_hitboxes[MZM_HITBOX_STANDING].right)

typedef struct {
    MzmHitboxType hitbox;           /* SCDF_HITBOX */
    MzmHitboxType block_hitbox;     /* SCDF_BLOCK_HITBOX */
    MzmStandingStatus standing;     /* SCDF_STANDING_STATUS */
    const char *name;
} PoseInfo;

/* sSamusCollisionData */
static const PoseInfo pose_info[MZM_POSE_COUNT] = {
    [MZM_POSE_RUNNING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_RUNNING"},
    [MZM_POSE_STANDING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_STANDING"},
    [MZM_POSE_TURNING_AROUND] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_TURNING_AROUND"},
    [MZM_POSE_SHOOTING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_SHOOTING"},
    [MZM_POSE_CROUCHING] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_GROUND, "SPOSE_CROUCHING"},
    [MZM_POSE_TURNING_AROUND_AND_CROUCHING] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_GROUND, "SPOSE_TURNING_AROUND_AND_CROUCHING"},
    [MZM_POSE_SHOOTING_AND_CROUCHING] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_GROUND, "SPOSE_SHOOTING_AND_CROUCHING"},
    [MZM_POSE_SKIDDING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_SKIDDING"},
    [MZM_POSE_MIDAIR] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_MIDAIR, "SPOSE_MIDAIR"},
    [MZM_POSE_TURNING_AROUND_MIDAIR] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_MIDAIR, "SPOSE_TURNING_AROUND_MIDAIR"},
    [MZM_POSE_LANDING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_LANDING"},
    [MZM_POSE_STARTING_SPIN_JUMP] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR, "SPOSE_STARTING_SPIN_JUMP"},
    [MZM_POSE_SPINNING] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR, "SPOSE_SPINNING"},
    [MZM_POSE_STARTING_WALL_JUMP] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR, "SPOSE_STARTING_WALL_JUMP"},
    [MZM_POSE_SPACE_JUMPING] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR, "SPOSE_SPACE_JUMPING"},
    [MZM_POSE_SCREW_ATTACKING] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR, "SPOSE_SCREW_ATTACKING"},
    [MZM_POSE_MORPHING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_MORPHING"},
    [MZM_POSE_MORPH_BALL] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_MORPH_BALL"},
    [MZM_POSE_ROLLING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_ROLLING"},
    [MZM_POSE_UNMORPHING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_UNMORPHING"},
    [MZM_POSE_MORPH_BALL_MIDAIR] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_MIDAIR, "SPOSE_MORPH_BALL_MIDAIR"},
    [MZM_POSE_HANGING_ON_LEDGE] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_HANGING, "SPOSE_HANGING_ON_LEDGE"},
    [MZM_POSE_TURNING_TO_AIM_WHILE_HANGING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_HANGING, "SPOSE_TURNING_TO_AIM_WHILE_HANGING"},
    [MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_HANGING, "SPOSE_HIDING_ARM_CANNON_WHILE_HANGING"},
    [MZM_POSE_AIMING_WHILE_HANGING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_HANGING, "SPOSE_AIMING_WHILE_HANGING"},
    [MZM_POSE_SHOOTING_WHILE_HANGING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_HANGING, "SPOSE_SHOOTING_WHILE_HANGING"},
    [MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_FORCED, "SPOSE_PULLING_YOURSELF_UP_FROM_HANGING"},
    [MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_PULLING_YOURSELF_FORWARD_FROM_HANGING"},
    [MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_FORCED, "SPOSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL"},
    [MZM_POSE_USING_AN_ELEVATOR] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_FORCED, "SPOSE_USING_AN_ELEVATOR"},
    [MZM_POSE_FACING_THE_FOREGROUND] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_FACING_THE_FOREGROUND"},
    [MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_TURNING_FROM_FACING_THE_FOREGROUND"},
    [MZM_POSE_GRABBED_BY_CHOZO_STATUE] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_FORCED, "SPOSE_GRABBED_BY_CHOZO_STATUE"},
    [MZM_POSE_DELAY_BEFORE_SHINESPARKING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_MIDAIR, "SPOSE_DELAY_BEFORE_SHINESPARKING"},
    [MZM_POSE_SHINESPARKING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_MIDAIR, "SPOSE_SHINESPARKING"},
    [MZM_POSE_SHINESPARK_COLLISION] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_FORCED, "SPOSE_SHINESPARK_COLLISION"},
    [MZM_POSE_DELAY_AFTER_SHINESPARKING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_FORCED, "SPOSE_DELAY_AFTER_SHINESPARKING"},
    [MZM_POSE_DELAY_BEFORE_BALLSPARKING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_MIDAIR, "SPOSE_DELAY_BEFORE_BALLSPARKING"},
    [MZM_POSE_BALLSPARKING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_MIDAIR, "SPOSE_BALLSPARKING"},
    [MZM_POSE_BALLSPARK_COLLISION] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_FORCED, "SPOSE_BALLSPARK_COLLISION"},
    [MZM_POSE_ON_ZIPLINE] = {MZM_HITBOX_ZIPLINE, MZM_HITBOX_STANDING, MZM_STANDING_NOT_IN_CONTROL, "SPOSE_ON_ZIPLINE"},
    [MZM_POSE_SHOOTING_ON_ZIPLINE] = {MZM_HITBOX_ZIPLINE, MZM_HITBOX_STANDING, MZM_STANDING_NOT_IN_CONTROL, "SPOSE_SHOOTING_ON_ZIPLINE"},
    [MZM_POSE_TURNING_ON_ZIPLINE] = {MZM_HITBOX_ZIPLINE, MZM_HITBOX_STANDING, MZM_STANDING_NOT_IN_CONTROL, "SPOSE_TURNING_ON_ZIPLINE"},
    [MZM_POSE_MORPH_BALL_ON_ZIPLINE] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_NOT_IN_CONTROL, "SPOSE_MORPH_BALL_ON_ZIPLINE"},
    [MZM_POSE_SAVING_LOADING_GAME] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_FORCED, "SPOSE_SAVING_LOADING_GAME"},
    [MZM_POSE_DOWNLOADING_MAP_DATA] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_DOWNLOADING_MAP_DATA"},
    [MZM_POSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA"},
    [MZM_POSE_GETTING_HURT] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_MIDAIR, "SPOSE_GETTING_HURT"},
    [MZM_POSE_GETTING_KNOCKED_BACK] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_MIDAIR, "SPOSE_GETTING_KNOCKED_BACK"},
    [MZM_POSE_GETTING_HURT_IN_MORPH_BALL] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_MIDAIR, "SPOSE_GETTING_HURT_IN_MORPH_BALL"},
    [MZM_POSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_MIDAIR, "SPOSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL"},
    [MZM_POSE_DYING] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_FORCED, "SPOSE_DYING"},
    [MZM_POSE_CROUCHING_TO_CRAWL] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_CROUCHING_TO_CRAWL"},
    [MZM_POSE_CRAWLING_STOPPED] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_CRAWLING_STOPPED"},
    [MZM_POSE_STARTING_TO_CRAWL] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_STARTING_TO_CRAWL"},
    [MZM_POSE_CRAWLING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_CRAWLING"},
    [MZM_POSE_UNCROUCHING_FROM_CRAWLING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_UNCROUCHING_FROM_CRAWLING"},
    [MZM_POSE_TURNING_AROUND_WHILE_CRAWLING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_TURNING_AROUND_WHILE_CRAWLING"},
    [MZM_POSE_SHOOTING_WHILE_CRAWLING] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_SHOOTING_WHILE_CRAWLING"},
    [MZM_POSE_UNCROUCHING_SUITLESS] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_UNCROUCHING_SUITLESS"},
    [MZM_POSE_CROUCHING_SUITLESS] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_GROUND, "SPOSE_CROUCHING_SUITLESS"},
    [MZM_POSE_GRABBING_A_LEDGE_SUITLESS] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_HANGING, "SPOSE_GRABBING_A_LEDGE_SUITLESS"},
    [MZM_POSE_FACING_THE_BACKGROUND_SUITLESS] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_FACING_THE_BACKGROUND_SUITLESS"},
    [MZM_POSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "SPOSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS"},
    [MZM_POSE_ACTIVATING_ZIPLINES] = {MZM_HITBOX_MORPHED, MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "SPOSE_ACTIVATING_ZIPLINES"},
    [MZM_POSE_IN_ESCAPE_SHIP] = {MZM_HITBOX_STANDING, MZM_HITBOX_STANDING, MZM_STANDING_FORCED, "SPOSE_IN_ESCAPE_SHIP"},
    [MZM_POSE_TURNING_TO_ENTER_ESCAPE_SHIP] = {MZM_HITBOX_CROUCHED, MZM_HITBOX_CROUCHED, MZM_STANDING_FORCED, "SPOSE_TURNING_TO_ENTER_ESCAPE_SHIP"},
};

/* sSamusPullingSelfUpVelocity */
static const uint16_t pulling_up_velocity[4] = {
    MZM_PIXEL_SIZE * 6, MZM_PIXEL_SIZE * 2, MZM_PIXEL_SIZE * 1, 0
};

const MzmBlockHitbox *mzm_block_hitbox(MzmHitboxType type) {
    return &block_hitboxes[type <= MZM_HITBOX_ZIPLINE ? type : MZM_HITBOX_STANDING];
}

MzmHitboxType mzm_pose_hitbox(MzmPose pose) {
    return pose < MZM_POSE_COUNT ? pose_info[pose].block_hitbox : MZM_HITBOX_STANDING;
}

MzmStandingStatus mzm_pose_standing(MzmPose pose) {
    return pose < MZM_POSE_COUNT ? pose_info[pose].standing : MZM_STANDING_FORCED;
}

const char *mzm_pose_name(MzmPose pose) {
    return pose < MZM_POSE_COUNT ? pose_info[pose].name : "SPOSE_NONE";
}

bool mzm_pose_is_spinning(MzmPose pose) {
    return pose == MZM_POSE_STARTING_SPIN_JUMP || pose == MZM_POSE_SPINNING ||
           pose == MZM_POSE_SPACE_JUMPING || pose == MZM_POSE_SCREW_ATTACKING;
}

bool mzm_pose_is_morphed(MzmPose pose) {
    return mzm_pose_hitbox(pose) == MZM_HITBOX_MORPHED;
}

/* SamusApplyXAcceleration on a velocity. */
int16_t mzm_apply_x_acceleration(int16_t velocity, int facing,
                                 int acceleration, int cap) {
    int value = velocity;
    if (facing > 0) {
        value = (int16_t)(acceleration + (uint16_t)velocity);
        if (value > cap) value = cap;
    } else {
        value = (int16_t)(velocity - acceleration);
        if (value < -cap) value = -cap;
    }
    return (int16_t)value;
}

/* The Y half of SamusUpdateVelocityPosition with normal (non-liquid) physics. */
int32_t mzm_integrate_y(int16_t *y_velocity) {
    int velocity;
    if (*y_velocity > MZM_Y_POSITIVE_VELOCITY_CAP)
        velocity = MZM_Y_POSITIVE_VELOCITY_CAP >> 3;
    else if (*y_velocity < -MZM_Y_NEGATIVE_VELOCITY_CAP)
        velocity = -MZM_Y_NEGATIVE_VELOCITY_CAP >> 3;
    else
        velocity = *y_velocity >> 3;
    if (*y_velocity >= -231) *y_velocity = (int16_t)(*y_velocity - MZM_Y_ACCELERATION);
    return -velocity;
}

/* Jump velocity selection shared by SamusSetMidAir's jump branches. */
int16_t mzm_jump_velocity(const MzmEquipment *equipment) {
    if (equipment->suit == MZM_SUIT_SUITLESS) return MZM_SUITLESS_JUMP_VELOCITY;
    if (equipment->items & MZM_ITEM_HIGH_JUMP) return MZM_HIGH_JUMP_VELOCITY;
    return MZM_LOW_JUMP_VELOCITY;
}

/* Suit reduction and death rule from SpriteUtilTakeDamageFromSprite at the
 * normal difficulty. Energy equal to the damage is lethal, as in the source. */
bool mzm_equipment_damage(MzmEquipment *equipment, int damage) {
    const uint32_t both = MZM_ITEM_VARIA_SUIT | MZM_ITEM_GRAVITY_SUIT;
    if (damage < 0) damage = 0;
    if ((equipment->items & both) == both) damage /= 2;
    else if (equipment->items & MZM_ITEM_GRAVITY_SUIT) damage = damage * 7 / 10;
    else if (equipment->items & MZM_ITEM_VARIA_SUIT) damage = damage * 8 / 10;
    if (damage == 0) damage = 1;
    if (equipment->energy > damage) {
        equipment->energy -= damage;
        return true;
    }
    equipment->energy = 0;
    return false;
}

/* SamusInit (the parts that are not the game state's). */
void mzm_samus_init(MzmSamus *samus, int32_t x, int32_t y, int facing) {
    memset(samus, 0, sizeof *samus);
    samus->pose = MZM_POSE_STANDING;
    samus->standing = MZM_STANDING_GROUND;
    samus->x = x;
    samus->y = y;
    samus->facing = facing < 0 ? -1 : 1;
    samus->aim = MZM_AIM_FORWARD;
    samus->copy = samus->data;
    samus->copy_y = y;
    samus->prev_x = x;
    samus->prev_y = y - 1;
}

void mzm_samus_box(const MzmSamus *samus, float *x, float *y,
                   float *w, float *h) {
    const MzmBlockHitbox *b = mzm_block_hitbox(mzm_pose_hitbox(samus->pose));
    *x = (float)(samus->x + b->left) / (float)MZM_SUBPIXELS_PER_PIXEL;
    *y = (float)(samus->y + b->top) / (float)MZM_SUBPIXELS_PER_PIXEL;
    *w = (float)(b->right - b->left) / (float)MZM_SUBPIXELS_PER_PIXEL;
    *h = (float)(-b->top) / (float)MZM_SUBPIXELS_PER_PIXEL;
}

/* ---- Frame context ---- */

typedef struct {
    MzmSamus *s;
    const MzmInput *in;
    const MzmEquipment *eq;
    const MzmCollision *c;
    const MzmAnimationSource *a;
    bool lethal;                    /* gEquipment.currentEnergy == 0 for a hurt request */
} Ctx;

static const MzmEquipment no_equipment = {0};
static const MzmInput no_input = {0};

/* gButtonInput, gChangedInput */
static bool held(const Ctx *c, uint16_t keys) { return (c->in->held & keys) != 0; }
static bool pressed(const Ctx *c, uint16_t keys) { return (c->in->pressed & keys) != 0; }
static bool suitless(const Ctx *c) { return c->eq->suit == MZM_SUIT_SUITLESS; }
static bool has(const Ctx *c, uint32_t item) { return (c->eq->items & item) != 0; }
/* pData->direction and OPPOSITE_DIRECTION(pData->direction) as input keys. */
static uint16_t dir_key(const MzmSamus *s) { return s->facing > 0 ? MZM_KEY_RIGHT : MZM_KEY_LEFT; }
static uint16_t opp_key(const MzmSamus *s) { return s->facing > 0 ? MZM_KEY_LEFT : MZM_KEY_RIGHT; }
/* pData->direction as native key bits (slopes, lastWallTouchedMidAir). */
static uint16_t dir_bits(const MzmSamus *s) { return s->facing > 0 ? NK_RIGHT : NK_LEFT; }
/* pData->direction ^= KEY_RIGHT | KEY_LEFT */
static void flip(MzmSamus *s) { s->facing = -s->facing; }
static void face_bits(MzmSamus *s, uint16_t bits) { s->facing = bits & NK_RIGHT ? 1 : -1; }

/* ClipdataProcessForSamus */
static uint32_t clip(const Ctx *c, int32_t y, int32_t x) {
    uint16_t yy = (uint16_t)y, xx = (uint16_t)x;
    if (c->c->clipdata) return c->c->clipdata(c->c->context, yy, xx);
    if (c->c->solid_point && c->c->solid_point(c->c->context, xx, yy, 0))
        return CLIP_SOLID | MZM_SAMUS_CLIP_SOLID_FLAG;
    if (!c->c->solid_point && c->c->blocked &&
        c->c->blocked(c->c->context, (float)(xx >> 2), (float)(yy >> 2), 1.f, 1.f))
        return CLIP_SOLID | MZM_SAMUS_CLIP_SOLID_FLAG;
    return CLIP_AIR;
}

/* ClipdataCheckCurrentAffectingAtPosition */
static uint32_t affecting(const Ctx *c, int32_t y, int32_t x) {
    return c->c->affecting ? c->c->affecting(c->c->context, (uint16_t)y, (uint16_t)x) : 0;
}

/* ClipdataCheckGroundEffect */
static uint8_t ground_effect(const Ctx *c, int32_t y, int32_t x) {
    return c->c->ground_effect ? c->c->ground_effect(c->c->context, (uint16_t)y, (uint16_t)x)
                               : MZM_GROUND_EFFECT_NONE;
}

static bool is_floor_slope(uint32_t clipdata) {
    switch (clipdata & 0xFF) {
    case CLIP_LEFT_STEEP:
    case CLIP_RIGHT_STEEP:
    case CLIP_LEFT_UPPER_SLIGHT:
    case CLIP_LEFT_LOWER_SLIGHT:
    case CLIP_RIGHT_LOWER_SLIGHT:
    case CLIP_RIGHT_UPPER_SLIGHT:
        return true;
    default:
        return false;
    }
}

/* SamusCheckCollisionAtPosition */
static uint8_t collision_at(const Ctx *c, int32_t x_position, int32_t y_position,
                            uint16_t *px, uint16_t *py, uint16_t *pslope) {
    uint16_t x = (uint16_t)x_position, y = (uint16_t)y_position;
    uint32_t clipdata = clip(c, y, x);
    uint8_t collision = clipdata & MZM_SAMUS_CLIP_SOLID_FLAG ? CLIP_SOLID : CLIP_AIR;
    uint16_t nx, ny, slope;
    const uint16_t sub = SUB_PIXEL_POSITION_FLAG;
    switch (clipdata & 0xFF) {
    case CLIP_RIGHT_STEEP:
        ny = (uint16_t)(((y & BLOCK_POSITION_FLAG) + sub) - (x & sub));
        nx = (uint16_t)(((x & BLOCK_POSITION_FLAG) + sub) - (y & sub));
        slope = SLOPE_STEEP | NK_RIGHT;
        break;
    case CLIP_RIGHT_LOWER_SLIGHT:
        ny = (uint16_t)((y & BLOCK_POSITION_FLAG) + sub - ((x & sub) >> 1));
        nx = (uint16_t)((x & BLOCK_POSITION_FLAG) - ((y & sub) * 2 - (sub - 1 + MZM_BLOCK_SIZE)));
        slope = SLOPE_SLIGHT | NK_RIGHT;
        break;
    case CLIP_RIGHT_UPPER_SLIGHT:
        ny = (uint16_t)((y & BLOCK_POSITION_FLAG) + (sub - MZM_HALF_BLOCK_SIZE) - ((x & sub) >> 1));
        nx = (uint16_t)((x & BLOCK_POSITION_FLAG) - ((y & sub) * 2 - sub + 1));
        slope = SLOPE_SLIGHT | NK_RIGHT;
        break;
    case CLIP_LEFT_STEEP:
        ny = (uint16_t)((y & BLOCK_POSITION_FLAG) | (x & sub));
        nx = (uint16_t)((x & BLOCK_POSITION_FLAG) | (y & sub));
        slope = SLOPE_STEEP | NK_LEFT;
        break;
    case CLIP_LEFT_LOWER_SLIGHT:
        ny = (uint16_t)((y & BLOCK_POSITION_FLAG) | (((x & sub) >> 1) + (sub - MZM_HALF_BLOCK_SIZE)));
        nx = (uint16_t)((x & BLOCK_POSITION_FLAG) + ((y & sub) * 2 - sub));
        slope = SLOPE_SLIGHT | NK_LEFT;
        break;
    case CLIP_LEFT_UPPER_SLIGHT:
        ny = (uint16_t)((y & BLOCK_POSITION_FLAG) | ((x & sub) >> 1));
        nx = (uint16_t)((x & BLOCK_POSITION_FLAG) + (y & sub) * 2);
        slope = SLOPE_SLIGHT | NK_LEFT;
        break;
    case CLIP_PASS_THROUGH_BOTTOM:
        collision = COLLISION_PASS_THROUGH_BOTTOM;
        /* fallthrough */
    default:
        ny = y & BLOCK_POSITION_FLAG;
        nx = x & BLOCK_POSITION_FLAG;
        slope = SLOPE_NONE;
        break;
    }
    *py = ny;
    *px = nx;
    *pslope = slope;
    return collision;
}

/* SamusCheckScrewSpeedboosterAffectingEnvironment */
static void screw_speed_environment(const Ctx *c) {
    const MzmSamus *s = c->s;
    if (!c->c->destruct) return;
    int action = 0;
    if (s->pose == MZM_POSE_SCREW_ATTACKING) action = MZM_SAMUS_DESTRUCT_SCREW;
    else if (s->pose == MZM_POSE_STARTING_SPIN_JUMP && has(c, MZM_ITEM_SCREW_ATTACK))
        action = MZM_SAMUS_DESTRUCT_SCREW;
    if (s->speedboosting) action += MZM_SAMUS_DESTRUCT_SPEED;
    if (action == 0) return;
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    uint16_t left = (uint16_t)(s->x + b->left), right = (uint16_t)(s->x + b->right);
    uint16_t top = (uint16_t)(s->y + b->top), bottom = (uint16_t)s->y;
    bool below = b->top < -MZM_BLOCK_SIZE;
    void *ctx = c->c->context;
    c->c->destruct(ctx, left, top, action);
    c->c->destruct(ctx, right, top, action);
    if (s->h_moving == HDMOVING_RIGHT) {
        if (below) c->c->destruct(ctx, right, (uint16_t)(top + MZM_BLOCK_SIZE), action);
        c->c->destruct(ctx, right, bottom, action);
    } else {
        if (below) c->c->destruct(ctx, left, (uint16_t)(top + MZM_BLOCK_SIZE), action);
        c->c->destruct(ctx, left, bottom, action);
    }
    if (mzm_pose_standing(s->pose) == MZM_STANDING_GROUND) {
        bottom++;
        action = MZM_SAMUS_DESTRUCT_SPEED_ON_GROUND;
    }
    c->c->destruct(ctx, left, bottom, action);
    c->c->destruct(ctx, right, bottom, action);
}

/* The x position against the block a side probe hit (unk_5604 and
 * SamusCheckTopSideCollisionMidAir). */
static uint16_t side_clamp(const MzmSamus *s) {
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    if (s->h_moving == HDMOVING_LEFT) return (uint16_t)((s->x & BLOCK_POSITION_FLAG) - b->left);
    return (uint16_t)((s->x & BLOCK_POSITION_FLAG) - b->right + SUB_PIXEL_POSITION_FLAG);
}

/* unk_5604: the front probes at the head (and the block below it for the
 * tall hitboxes), slopes excluded. */
static uint8_t side_probe(const Ctx *c, int32_t x, uint16_t *position) {
    const MzmSamus *s = c->s;
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    uint8_t result = DETECT_NONE;
    *position = side_clamp(s);
    uint16_t y = (uint16_t)(s->y + b->top);
    uint32_t clipdata = clip(c, y, x);
    if (!is_floor_slope(clipdata) && clipdata & MZM_SAMUS_CLIP_SOLID_FLAG)
        result += DETECT_LEFT_MOST;
    if (b->top >= -MZM_BLOCK_SIZE) return result;
    clipdata = clip(c, y + MZM_BLOCK_SIZE, x);
    if (!is_floor_slope(clipdata) && clipdata & MZM_SAMUS_CLIP_SOLID_FLAG)
        result += DETECT_MIDDLE_LEFT;
    return result;
}

/* SamusCheckTopSideCollisionMidAir */
static uint8_t top_side_midair(const Ctx *c, int32_t x, uint16_t *position) {
    const MzmSamus *s = c->s;
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    uint8_t result = DETECT_NONE;
    *position = side_clamp(s);
    uint16_t y = (uint16_t)s->y;
    uint32_t clipdata = clip(c, y, x);
    if (clipdata & MZM_SAMUS_CLIP_SOLID_FLAG) {
        if (is_floor_slope(clipdata)) result = DETECT_SLOPE;
        else result += DETECT_LEFT_MOST;
    }
    if (result == DETECT_LEFT_MOST && is_floor_slope(clip(c, s->y, s->x)))
        result = DETECT_LEFT_MOST | DETECT_SLOPE;
    if (b->top < -MZM_BLOCK_SIZE) {
        clipdata = clip(c, y - MZM_BLOCK_SIZE, x);
        if (!is_floor_slope(clipdata) && clipdata & MZM_SAMUS_CLIP_SOLID_FLAG)
            result += DETECT_MIDDLE_LEFT;
    }
    return result;
}

/* SamusCheckWalkingOnSlope */
static uint8_t walking_on_slope(const Ctx *c, int32_t x) {
    uint8_t result = DETECT_NONE;
    uint32_t clipdata = clip(c, c->s->y, x);
    if (clipdata & MZM_SAMUS_CLIP_SOLID_FLAG) {
        if (is_floor_slope(clipdata)) result = DETECT_SLOPE;
        else result += DETECT_LEFT_MOST;
    }
    if (result == DETECT_LEFT_MOST && is_floor_slope(clip(c, c->s->y, c->s->x)))
        result = DETECT_LEFT_MOST | DETECT_SLOPE;
    return result;
}

/* SamusCheckCollisionAbove: the four points of a top (the hitbox's sides
 * and sSamusBlockHitboxData_Above). */
static uint8_t collision_above(const Ctx *c, int16_t hitbox) {
    const MzmSamus *s = c->s;
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    uint8_t result = DETECT_NONE;
    uint16_t y = (uint16_t)(s->y + hitbox);
    if (clip(c, y, s->x + b->left) & MZM_SAMUS_CLIP_SOLID_FLAG) result += DETECT_LEFT_MOST;
    if (clip(c, y, s->x + above_left) & MZM_SAMUS_CLIP_SOLID_FLAG) result += DETECT_MIDDLE_LEFT;
    if (clip(c, y, s->x + above_right) & MZM_SAMUS_CLIP_SOLID_FLAG) result += DETECT_MIDDLE_RIGHT;
    if (clip(c, y, s->x + b->right) & MZM_SAMUS_CLIP_SOLID_FLAG) result += DETECT_RIGHT_MOST;
    return result;
}

static void set_pose(Ctx *c, MzmPose pose);

/* SamusCheckWalkingSidesCollision */
static MzmPose walking_sides(Ctx *c) {
    MzmSamus *s = c->s;
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    uint16_t slope, front, back;
    uint16_t nx, ny, nslope;
    if (s->h_moving == HDMOVING_LEFT) {
        slope = NK_LEFT;
        front = (uint16_t)b->left;
        back = (uint16_t)b->right;
    } else {
        slope = NK_RIGHT;
        front = (uint16_t)b->right;
        back = (uint16_t)b->left;
    }
    uint8_t result = side_probe(c, s->x + (int16_t)front, &nx);
    if (result != DETECT_NONE) {
        s->x = nx;
        s->touching_side++;
    }
    s->copy.timer = result;
    if (s->current_slope == 0) {
        result = walking_on_slope(c, s->x + (int16_t)front);
        if (result == DETECT_SLOPE) {
            collision_at(c, s->x + (int16_t)front, s->y, &nx, &ny, &nslope);
            s->y = ny;
            s->current_slope = nslope;
            s->standing = MZM_STANDING_GROUND;
        } else if (result == (DETECT_SLOPE | DETECT_LEFT_MOST)) {
            collision_at(c, s->x + (int16_t)front, s->y - MZM_BLOCK_SIZE, &nx, &ny, &nslope);
            s->y = ny;
            if (nslope == 0) s->y = U16(ny + SUB_PIXEL_POSITION_FLAG);
            s->current_slope = nslope;
            s->standing = MZM_STANDING_GROUND;
        } else {
            if (result != DETECT_NONE) {
                s->x = nx;
                s->touching_side++;
            }
            if (s->standing == MZM_STANDING_ENEMY) return MZM_POSE_NONE;
            if (collision_at(c, s->x + (int16_t)front, s->y + 1, &nx, &ny, &nslope) == CLIP_AIR) {
                uint8_t back_result = collision_at(c, s->x + (int16_t)back, s->y + 1, &nx, &ny, &nslope);
                if (nslope == 0) {
                    if (back_result == CLIP_AIR) return MZM_POSE_MID_AIR_REQUEST;
                } else {
                    s->y = ny;
                    s->current_slope = nslope;
                }
            }
            s->standing = MZM_STANDING_GROUND;
        }
    } else if (s->current_slope & slope) {
        result = walking_on_slope(c, s->x + (int16_t)front);
        if (result == DETECT_SLOPE) {
            collision_at(c, s->x + (int16_t)front, s->y, &nx, &ny, &nslope);
            s->y = ny;
            s->current_slope = nslope;
            s->standing = MZM_STANDING_GROUND;
        } else if (result != DETECT_NONE) {
            collision_at(c, s->x + (int16_t)front, s->y - MZM_BLOCK_SIZE, &nx, &ny, &nslope);
            s->y = ny;
            if (nslope == 0) s->y = U16(ny + (MZM_BLOCK_SIZE - 1));
            s->current_slope = nslope;
            s->standing = MZM_STANDING_GROUND;
        }
    } else {
        result = collision_at(c, s->x + (int16_t)back, s->y, &nx, &ny, &nslope);
        if (nslope != 0) {
            s->y = ny;
            return MZM_POSE_NONE;
        }
        if (result == DETECT_NONE) {
            result = collision_at(c, s->x + (int16_t)back, s->y + MZM_BLOCK_SIZE, &nx, &ny, &nslope);
            s->current_slope = nslope;
            if (nslope != 0) {
                s->y = ny;
                return MZM_POSE_NONE;
            }
            if (result != DETECT_NONE) {
                s->x = side_clamp(s);
                s->y = U16(ny - 1);
            }
        }
    }
    return MZM_POSE_NONE;
}

/* unk_5AD8: walking off a ledge in the middle of a block drops Samus at
 * the edge of her block. */
static MzmPose walking_off_ledge(Ctx *c) {
    MzmSamus *s = c->s;
    uint16_t nx, ny, nslope;
    if ((s->x & BLOCK_POSITION_FLAG) == (s->prev_x & BLOCK_POSITION_FLAG) && s->current_slope == 0 &&
        s->standing == MZM_STANDING_GROUND &&
        collision_at(c, s->x, s->y + 1, &nx, &ny, &nslope) == CLIP_AIR && nslope == 0) {
        if (s->h_moving == HDMOVING_RIGHT) {
            if ((s->x & SUB_PIXEL_POSITION_FLAG) > 0x1D && (s->prev_x & SUB_PIXEL_POSITION_FLAG) < 0x1F) {
                s->x = (s->x & BLOCK_POSITION_FLAG) + 0x1E;
                return MZM_POSE_MID_AIR_REQUEST;
            }
        } else if (s->h_moving == HDMOVING_LEFT) {
            if ((s->x & SUB_PIXEL_POSITION_FLAG) < 0x22 && (s->prev_x & SUB_PIXEL_POSITION_FLAG) > 0x20) {
                s->x = (s->x & BLOCK_POSITION_FLAG) + 0x21;
                return MZM_POSE_MID_AIR_REQUEST;
            }
        }
    }
    return MZM_POSE_NONE;
}

/* SamusCheckStandingOnGroundCollision */
static MzmPose standing_on_ground(Ctx *c) {
    MzmSamus *s = c->s;
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    uint16_t nx, ny, nslope;
    uint8_t above = collision_above(c, b->top);
    if (above == DETECT_LEFT_MOST || above == (DETECT_LEFT_MOST | DETECT_MIDDLE_LEFT))
        s->x = U16((s->x & BLOCK_POSITION_FLAG) - b->left);
    else if (above == DETECT_RIGHT_MOST || above == (DETECT_RIGHT_MOST | DETECT_MIDDLE_RIGHT))
        s->x = U16((s->x & BLOCK_POSITION_FLAG) - b->right + SUB_PIXEL_POSITION_FLAG);
    if (s->standing != MZM_STANDING_ENEMY &&
        collision_at(c, s->x + b->left, s->y + 1, &nx, &ny, &nslope) == CLIP_AIR &&
        collision_at(c, s->x + b->right, s->y + 1, &nx, &ny, &nslope) == CLIP_AIR)
        return MZM_POSE_MID_AIR_REQUEST;
    return MZM_POSE_NONE;
}

/* SamusCheckLandingCollision */
static MzmPose landing_collision(Ctx *c) {
    MzmSamus *s = c->s;
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    int16_t h = s->h_moving == HDMOVING_LEFT ? b->left : b->right;
    uint16_t lx, ly, lslope, rx, ry, rslope;
    if (side_probe(c, s->x + h, &lx) != DETECT_NONE) {
        s->x = lx;
        s->touching_side++;
    }
    if (s->standing == MZM_STANDING_ENEMY) return MZM_POSE_NONE;
    uint16_t block_y = s->y & BLOCK_POSITION_FLAG, prev_block_y = s->prev_y & BLOCK_POSITION_FLAG;
    uint8_t left = collision_at(c, s->x + b->left, s->y, &lx, &ly, &lslope);
    uint8_t right = collision_at(c, s->x + b->right, s->y, &rx, &ry, &rslope);
    if (block_y > prev_block_y) {
        if (left != CLIP_AIR) {
            if (lslope != SLOPE_NONE) {
                if (right != CLIP_AIR) {
                    s->y = U16(ry - 1);
                } else {
                    s->current_slope = lslope;
                    s->y = ly;
                }
            } else {
                collision_at(c, s->x + b->left, s->y - MZM_BLOCK_SIZE, &lx, &ly, &lslope);
                s->y = ly;
                if (lslope == SLOPE_NONE) s->y = U16(ly + SUB_PIXEL_POSITION_FLAG);
                s->current_slope = lslope;
            }
        } else if (right != CLIP_AIR) {
            if (rslope != SLOPE_NONE) {
                s->current_slope = rslope;
                s->y = ry;
            } else {
                collision_at(c, s->x + b->right, s->y - MZM_BLOCK_SIZE, &lx, &ly, &lslope);
                s->y = ly;
                if (lslope == SLOPE_NONE) s->y = U16(ly + SUB_PIXEL_POSITION_FLAG);
                s->current_slope = lslope;
            }
        } else {
            return MZM_POSE_NONE;
        }
        return MZM_POSE_LANDING_REQUEST;
    }
    if (left != CLIP_AIR) {
        if (lslope != SLOPE_NONE) {
            s->current_slope = lslope;
            s->y = ly;
            return MZM_POSE_LANDING_REQUEST;
        }
        s->x = U16((s->x & BLOCK_POSITION_FLAG) - b->left);
        s->touching_side++;
    } else if (right != CLIP_AIR) {
        if (rslope != SLOPE_NONE) {
            s->current_slope = rslope;
            s->y = ry;
            return MZM_POSE_LANDING_REQUEST;
        }
        s->x = U16((s->x & BLOCK_POSITION_FLAG) - b->right + SUB_PIXEL_POSITION_FLAG);
        s->touching_side++;
    }
    return MZM_POSE_NONE;
}

/* SamusCheckTopCollision */
static MzmPose top_collision(Ctx *c) {
    MzmSamus *s = c->s;
    const MzmBlockHitbox *b = mzm_block_hitbox(s->hitbox_type);
    int16_t h = s->h_moving == HDMOVING_LEFT ? b->left : b->right;
    uint16_t nx, ny, nslope;
    uint8_t result = top_side_midair(c, s->x + h, &nx);
    if (result & DETECT_SLOPE) {
        if (result & DETECT_LEFT_MOST) s->x = nx;
        collision_at(c, s->x + h, s->y, &nx, &ny, &nslope);
        s->y = ny;
        s->current_slope = nslope;
        return MZM_POSE_LANDING_REQUEST;
    }
    if (result != DETECT_NONE) {
        s->x = nx;
        s->touching_side++;
    }
    result = collision_above(c, b->top);
    if (result == DETECT_LEFT_MOST) {
        s->x = U16((s->x & BLOCK_POSITION_FLAG) - b->left);
        s->prev_x = s->x;
    } else if (result == DETECT_RIGHT_MOST) {
        s->x = U16((s->x & BLOCK_POSITION_FLAG) - b->right + SUB_PIXEL_POSITION_FLAG);
        s->prev_x = s->x;
    } else if (result & DETECT_MIDDLE) {
        uint16_t top = (uint16_t)(s->y + b->top);
        s->y = U16((top & BLOCK_POSITION_FLAG) - b->top + MZM_BLOCK_SIZE);
        s->y_velocity = 0;
        s->touching_top++;
    }
    return MZM_POSE_NONE;
}

/* SamusCheckCollisions */
static void check_collisions(Ctx *c, MzmStandingStatus physics_standing) {
    MzmSamus *s = c->s;
    if (physics_standing > MZM_STANDING_NOT_IN_CONTROL) return;
    screw_speed_environment(c);
    MzmPose pose = MZM_POSE_NONE;
    if (s->v_moving == VDMOVING_UP) {
        pose = top_collision(c);
    } else if (s->v_moving == VDMOVING_DOWN) {
        pose = landing_collision(c);
    } else if (s->h_moving != HDMOVING_NONE) {
        if (physics_standing == MZM_STANDING_MIDAIR) {
            pose = landing_collision(c);
        } else {
            pose = walking_sides(c);
            if (pose == MZM_POSE_NONE) pose = walking_off_ledge(c);
        }
    } else if (physics_standing == MZM_STANDING_GROUND) {
        pose = standing_on_ground(c);
    }
    /* The Power Grip ledge grab. */
    if (pose == MZM_POSE_NONE && has(c, MZM_ITEM_POWER_GRIP) && held(c, dir_key(s)) &&
        s->y_velocity < 1) {
        int32_t x_offset = s->facing > 0 ? MZM_HALF_BLOCK_SIZE - 1 : -(MZM_HALF_BLOCK_SIZE - 1);
        if (clip(c, s->y + MZM_HALF_BLOCK_SIZE, s->x) == CLIP_AIR) {
            uint32_t prevent = clip(c, s->y - (MZM_BLOCK_SIZE * 2 + MZM_HALF_BLOCK_SIZE), s->x) &
                               MZM_SAMUS_CLIP_SOLID_FLAG;
            uint32_t grabbing = clip(c, s->y - (MZM_BLOCK_SIZE + MZM_HALF_BLOCK_SIZE +
                                                MZM_EIGHTH_BLOCK_SIZE), s->x + x_offset) &
                                MZM_SAMUS_CLIP_SOLID_FLAG;
            uint32_t above = clip(c, s->y - MZM_BLOCK_SIZE * 2, s->x + x_offset) &
                             MZM_SAMUS_CLIP_SOLID_FLAG;
            switch (s->pose) {
            case MZM_POSE_MIDAIR:
            case MZM_POSE_STARTING_SPIN_JUMP:
            case MZM_POSE_SPINNING:
            case MZM_POSE_STARTING_WALL_JUMP:
            case MZM_POSE_SPACE_JUMPING:
            case MZM_POSE_SCREW_ATTACKING: {
                uint32_t movement = affecting(c, s->y - (MZM_BLOCK_SIZE + MZM_HALF_BLOCK_SIZE),
                                              s->x + x_offset) >> 16;
                if (!prevent && grabbing && movement != MZM_SAMUS_MOVEMENT_NON_POWER_GRIP && !above) {
                    if (suitless(c)) {
                        pose = MZM_POSE_GRABBING_A_LEDGE_SUITLESS;
                        mzm_sfx_play(MZM_SOUND_SUITLESS_GRAB_LEDGE);
                    } else {
                        pose = MZM_POSE_HANGING_ON_LEDGE;
                        mzm_sfx_play(s->slowed ? MZM_SOUND_WATER_MOVEMENT_WALL_JUMP
                                               : MZM_SOUND_GRAB_LEDGE);
                    }
                    s->grabbed_ledge = true;
                }
                break;
            }
            default:
                break;
            }
        }
    }
    if (s->touching_side) {
        s->x_velocity = 0;
        switch (s->pose) {
        case MZM_POSE_RUNNING:
            if (suitless(c) && s->copy.timer == 1) pose = MZM_POSE_CROUCHING_TO_CRAWL;
            break;
        case MZM_POSE_SPINNING:
        case MZM_POSE_SCREW_ATTACKING:
            s->walljump_timer = MZM_WALL_JUMP_TICKS;
            s->last_wall = dir_bits(s) ^ (NK_RIGHT | NK_LEFT);
            break;
        case MZM_POSE_ON_ZIPLINE:
        case MZM_POSE_SHOOTING_ON_ZIPLINE:
        case MZM_POSE_TURNING_ON_ZIPLINE:
        case MZM_POSE_MORPH_BALL_ON_ZIPLINE:
            pose = MZM_POSE_MID_AIR_REQUEST;
            break;
        default:
            break;
        }
    }
    if (pose == MZM_POSE_MID_AIR_REQUEST) s->y = U16(s->y + 1);
    if (pose != MZM_POSE_NONE) set_pose(c, pose);
}

/* SamusCheckSetEnvironmentalEffect: the sounds of the requests (the effect
 * sprites themselves, gSamusEnvironmentalEffects, are not ported). */
static void environmental_effect(Ctx *c, int request) {
    MzmSamus *s = c->s;
    uint8_t ground;
    switch (request) {
    case WANTING_RUNNING_EFFECT:
    case WANTING_RUNNING_EFFECT_: {
        int32_t next_x = s->facing > 0 ? s->x + MZM_PIXEL_SIZE : s->x - MZM_PIXEL_SIZE;
        ground = ground_effect(c, s->y + 1, next_x);
        bool first = request == WANTING_RUNNING_EFFECT;
        if (ground == MZM_GROUND_EFFECT_WET_GROUND) {
            if (suitless(c))
                mzm_sfx_play(first ? MZM_SOUND_SUITLESS_FOOTSTEPS_WET_GROUND_1
                                   : MZM_SOUND_SUITLESS_FOOTSTEPS_WET_GROUND_2);
            else
                mzm_sfx_play(first ? MZM_SOUND_FOOTSTEPS_WET_GROUND_1
                                   : MZM_SOUND_FOOTSTEPS_WET_GROUND_2);
        } else if (ground == MZM_GROUND_EFFECT_DUSTY_GROUND ||
                   ground == MZM_GROUND_EFFECT_VERY_DUSTY_GROUND) {
            mzm_sfx_play(first ? MZM_SOUND_FOOTSTEPS_DUSTY_GROUND_1
                               : MZM_SOUND_FOOTSTEPS_DUSTY_GROUND_2);
        }
        break;
    }
    case WANTING_LANDING_EFFECT:
        ground = ground_effect(c, s->y + 1, s->x);
        if (ground == MZM_GROUND_EFFECT_WET_GROUND) {
            mzm_sfx_play(suitless(c) ? MZM_SOUND_SUITLESS_LANDING_ON_WET_GROUND
                                     : MZM_SOUND_GOING_IN_LIQUID);
        } else if (ground == MZM_GROUND_EFFECT_BUBBLY_GROUND) {
            /* No sound. */
        } else if (ground == MZM_GROUND_EFFECT_DUSTY_GROUND ||
                   ground == MZM_GROUND_EFFECT_VERY_DUSTY_GROUND) {
            mzm_sfx_play(MZM_SOUND_LANDING_ON_DUSTY_GROUND);
        } else if (s->slowed) {
            mzm_sfx_play(MZM_SOUND_WATER_MOVEMENT_WALL_JUMP);
        } else if (s->copy.last_wall != 0) {
            mzm_sfx_play(MZM_SOUND_MORPH_BALL_LANDING);
        } else {
            mzm_sfx_play(suitless(c) ? MZM_SOUND_SUITLESS_LANDING : MZM_SOUND_LANDING);
        }
        break;
    default:
        /* Skidding and wet ground effects only spawn effect sprites. */
        break;
    }
}

static bool jump_pressed(const Ctx *c) { return pressed(c, MZM_KEY_A); }

/* SamusSetMidAir */
static void set_midair(Ctx *c, const MzmSamusData *copy) {
    MzmSamus *s = c->s;
    s->x_velocity = copy->x_velocity;
    s->aim = copy->aim;
    s->speedboosting = copy->speedboosting;
    switch (copy->pose) {
    case MZM_POSE_RUNNING:
        if (copy->forced != MZM_FORCED_JUMP) {
            s->pose = MZM_POSE_MIDAIR;
            break;
        }
        s->pose = MZM_POSE_STARTING_SPIN_JUMP;
        s->y_velocity = mzm_jump_velocity(c->eq);
        break;
    case MZM_POSE_STARTING_SPIN_JUMP:
    case MZM_POSE_SPINNING:
    case MZM_POSE_SPACE_JUMPING:
    case MZM_POSE_SCREW_ATTACKING:
        s->pose = MZM_POSE_MIDAIR;
        s->x_velocity = 0;
        if (copy->forced == MZM_FORCED_CARRY) s->y_velocity = copy->y_velocity;
        if (collision_above(c, STANDING_TOP) != DETECT_NONE)
            s->y = U16(s->y + MZM_HALF_BLOCK_SIZE);
        break;
    case MZM_POSE_STARTING_WALL_JUMP:
        if (copy->forced == MZM_FORCED_FALL) {
            s->pose = MZM_POSE_MIDAIR;
            break;
        }
        s->pose = MZM_POSE_SPINNING;
        if (copy->forced == MZM_FORCED_JUMP)
            s->y_velocity = has(c, MZM_ITEM_HIGH_JUMP) ? MZM_HIGH_JUMP_VELOCITY : MZM_LOW_JUMP_VELOCITY;
        if (s->slowed) {
            s->y_velocity = MZM_HIGH_JUMP_VELOCITY / 2;
            mzm_sfx_play(MZM_SOUND_WATER_MOVEMENT_WALL_JUMP);
        } else {
            mzm_sfx_play(suitless(c) ? MZM_SOUND_SUITLESS_WALL_JUMP : MZM_SOUND_WALL_JUMP);
        }
        break;
    case MZM_POSE_MORPH_BALL_MIDAIR:
        if (copy->forced == MZM_FORCED_BOMB_JUMP_RIGHT) {
            s->facing = 1;
            s->x_velocity = MZM_BOMB_BOUNCE_X_VELOCITY;
            s->y_velocity = MZM_BOMB_BOUNCE_Y_VELOCITY;
            s->forced = MZM_FORCED_JUMP;
        } else if (copy->forced == MZM_FORCED_BOMB_JUMP_UP) {
            s->x_velocity = 0;
            s->y_velocity = MZM_BOMB_BOUNCE_Y_VELOCITY;
            s->forced = MZM_FORCED_JUMP;
        } else if (copy->forced == MZM_FORCED_BOMB_JUMP_LEFT) {
            s->facing = -1;
            s->x_velocity = -MZM_BOMB_BOUNCE_X_VELOCITY;
            s->y_velocity = MZM_BOMB_BOUNCE_Y_VELOCITY;
            s->forced = MZM_FORCED_JUMP;
        }
        break;
    case MZM_POSE_MORPH_BALL:
    case MZM_POSE_ROLLING:
        /* The morph ball spin carries on. */
        s->anim_frame = copy->anim_frame;
        s->anim_counter = copy->anim_counter;
        /* fallthrough */
    case MZM_POSE_MORPHING:
        if (copy->forced == MZM_FORCED_BOMB_JUMP_RIGHT) {
            s->facing = 1;
            s->x_velocity = MZM_BOMB_BOUNCE_X_VELOCITY;
            s->y_velocity = MZM_BOMB_BOUNCE_Y_VELOCITY;
            s->forced = MZM_FORCED_JUMP;
        } else if (copy->forced == MZM_FORCED_BOMB_JUMP_UP) {
            s->x_velocity = 0;
            s->y_velocity = MZM_BOMB_BOUNCE_Y_VELOCITY;
            s->forced = MZM_FORCED_JUMP;
        } else if (copy->forced == MZM_FORCED_BOMB_JUMP_LEFT) {
            s->facing = -1;
            s->x_velocity = -MZM_BOMB_BOUNCE_X_VELOCITY;
            s->y_velocity = MZM_BOMB_BOUNCE_Y_VELOCITY;
            s->forced = MZM_FORCED_JUMP;
        } else {
            /* A morph ball jump: half the X velocity (DIV_SHIFT). */
            s->x_velocity = (int16_t)(s->x_velocity >> 1);
            if (copy->forced == MZM_FORCED_JUMP) s->y_velocity = MZM_MORPH_BALL_JUMP_VELOCITY;
        }
        /* fallthrough */
    case MZM_POSE_DELAY_BEFORE_BALLSPARKING:
    case MZM_POSE_BALLSPARK_COLLISION:
    case MZM_POSE_MORPH_BALL_ON_ZIPLINE:
        s->pose = MZM_POSE_MORPH_BALL_MIDAIR;
        break;
    case MZM_POSE_CROUCHING:
    case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
    case MZM_POSE_SHOOTING_AND_CROUCHING:
        if (collision_above(c, STANDING_TOP) != DETECT_NONE) s->y = U16(s->y + MZM_HALF_BLOCK_SIZE);
        /* fallthrough */
    default:
        s->pose = MZM_POSE_MIDAIR;
        if (copy->forced == MZM_FORCED_JUMP) {
            s->pose = held(c, MZM_KEY_RIGHT | MZM_KEY_LEFT) ? MZM_POSE_STARTING_SPIN_JUMP
                                                            : MZM_POSE_MIDAIR;
            s->y_velocity = mzm_jump_velocity(c->eq);
        } else if (copy->forced == MZM_FORCED_CARRY) {
            s->y_velocity = copy->y_velocity;
        }
        break;
    }
    if (copy->forced == MZM_FORCED_JUMP) {
        if (s->pose == MZM_POSE_MIDAIR) {
            if (!s->slowed) {
                if (s->y_velocity == MZM_LOW_JUMP_VELOCITY) mzm_sfx_play(MZM_SOUND_LOW_JUMP);
                else if (s->y_velocity == MZM_HIGH_JUMP_VELOCITY) mzm_sfx_play(MZM_SOUND_HIGH_JUMP);
                else if (s->y_velocity == MZM_SUITLESS_JUMP_VELOCITY)
                    mzm_sfx_play(MZM_SOUND_SUITLESS_JUMP);
            } else {
                mzm_sfx_play(MZM_SOUND_WATER_MOVEMENT_GRABBING_LEDGE);
            }
        } else if (s->pose == MZM_POSE_MORPH_BALL_MIDAIR) {
            mzm_sfx_play(s->slowed ? MZM_SOUND_WATER_MOVEMENT_GRABBING_LEDGE
                                   : MZM_SOUND_MORPH_BALL_JUMP);
        }
    }
}

/* SamusSetLandingPose */
static void set_landing(Ctx *c, MzmSamusData *copy) {
    MzmSamus *s = c->s;
    copy->last_wall = 0;
    switch (copy->pose) {
    case MZM_POSE_MIDAIR:
        if (collision_above(c, STANDING_TOP) != DETECT_NONE) {
            s->pose = MZM_POSE_CROUCHING;
        } else if (copy->x_velocity == 0) {
            s->pose = MZM_POSE_LANDING;
        } else if (copy->speedboosting) {
            /* Intended to keep the speed booster; the low velocity cancels
             * it right away. */
            s->pose = MZM_POSE_RUNNING;
            s->speedboosting = true;
        } else {
            s->pose = MZM_POSE_STANDING;
        }
        break;
    case MZM_POSE_MORPH_BALL_MIDAIR:
        copy->last_wall++;
        if (held(c, MZM_KEY_A) && has(c, MZM_ITEM_HIGH_JUMP)) {
            if (!(collision_above(c, STANDING_TOP) & DETECT_MIDDLE))
                s->forced = MZM_FORCED_BOUNCE_BEFORE_JUMP;
        } else if (copy->y_velocity < -8 * (MZM_QUARTER_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE) &&
                   !s->slowed &&
                   ground_effect(c, s->y + 1, s->x) != MZM_GROUND_EFFECT_VERY_DUSTY_GROUND) {
            /* The morph ball bounce. */
            s->forced = MZM_FORCED_BOUNCE_AFTER_FALL;
            s->y_velocity = 50;     /* SUB_PIXEL_TO_VELOCITY(PIXEL_SIZE + PIXEL_SIZE / 2 + 1 / 4.f) */
            break;
        }
        /* fallthrough */
    case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
    case MZM_POSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL:
        s->pose = MZM_POSE_MORPH_BALL;
        break;
    case MZM_POSE_SHINESPARKING:
    case MZM_POSE_BALLSPARKING:
        if (held(c, dir_key(s))) {
            /* The shinespark carries into a run. */
            s->x_velocity = s->facing > 0 ? MZM_X_SPEEDBOOST_VELOCITY_CAP : -MZM_X_SPEEDBOOST_VELOCITY_CAP;
            if (copy->pose == MZM_POSE_SHINESPARKING) {
                s->pose = MZM_POSE_RUNNING;
            } else {
                s->pose = MZM_POSE_ROLLING;
                s->shinespark_timer = 6;
            }
            s->speedboosting = true;
            s->timer = 160;
            mzm_sfx_play(MZM_SOUND_SPEEDBOOSTER_START);
        } else {
            s->shake_x = 30;        /* ScreenShakeStartHorizontal(CONVERT_SECONDS(.5f), 1) */
            s->pose = copy->pose == MZM_POSE_SHINESPARKING ? MZM_POSE_SHINESPARK_COLLISION
                                                           : MZM_POSE_BALLSPARK_COLLISION;
            s->forced = copy->forced;
            s->anim_frame = 1;
            mzm_sfx_play(MZM_SOUND_SHINESPARK_COLLISION);
        }
        break;
    default:
        if (collision_above(c, STANDING_TOP) != DETECT_NONE) s->pose = MZM_POSE_CROUCHING;
        else if (copy->x_velocity == 0) s->pose = MZM_POSE_LANDING;
        else s->pose = MZM_POSE_STANDING;
        break;
    }
    s->aim = copy->aim;
    switch (s->pose) {
    case MZM_POSE_LANDING:
        if (s->has_new_projectile) s->anim_frame = 1;
        if (copy->aim == MZM_AIM_DOWN) s->aim = MZM_AIM_DIAGONAL_DOWN;
        break;
    case MZM_POSE_RUNNING:
        if (copy->aim == MZM_AIM_UP) s->aim = MZM_AIM_DIAGONAL_UP;
        /* fallthrough */
    case MZM_POSE_STANDING:
        if (copy->aim == MZM_AIM_DOWN) s->aim = MZM_AIM_DIAGONAL_DOWN;
        break;
    default:
        break;
    }
    environmental_effect(c, WANTING_LANDING_EFFECT);
}

/* SamusChangeToHurtPose */
static void change_to_hurt(Ctx *c, const MzmSamusData *copy) {
    MzmSamus *s = c->s;
    if (!c->lethal) {
        uint8_t collision = DETECT_NONE;
        switch (copy->pose) {
        case MZM_POSE_MORPHING:
        case MZM_POSE_MORPH_BALL:
        case MZM_POSE_ROLLING:
        case MZM_POSE_MORPH_BALL_MIDAIR:
        case MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL:
        case MZM_POSE_DELAY_BEFORE_BALLSPARKING:
        case MZM_POSE_BALLSPARK_COLLISION:
        case MZM_POSE_MORPH_BALL_ON_ZIPLINE:
        case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
        case MZM_POSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL:
            s->pose = MZM_POSE_GETTING_HURT_IN_MORPH_BALL;
            break;
        case MZM_POSE_CRAWLING_STOPPED:
        case MZM_POSE_STARTING_TO_CRAWL:
        case MZM_POSE_CRAWLING:
        case MZM_POSE_TURNING_AROUND_WHILE_CRAWLING:
        case MZM_POSE_SHOOTING_WHILE_CRAWLING:
            s->pose = MZM_POSE_CRAWLING_STOPPED;
            s->x_velocity = 0;
            break;
        default:
            collision = collision_above(c, STANDING_TOP);
            s->pose = MZM_POSE_GETTING_HURT;
            break;
        }
        if (collision != DETECT_NONE) s->y = s->y | SUB_PIXEL_POSITION_FLAG;
        if (copy->forced == MZM_FORCED_DAMAGED_BY_RUINS_TEST) s->forced = 1;
        s->y_velocity = 8 * (MZM_QUARTER_BLOCK_SIZE - MZM_PIXEL_SIZE / 2);
        if (copy->standing == MZM_STANDING_MIDAIR) s->y_velocity = 8 * (MZM_EIGHTH_BLOCK_SIZE - 1);
        s->aim = copy->aim;
        mzm_sfx_play(MZM_SOUND_HURT);
    } else {
        /* gDisableScrolling, the black monochrome fading and
         * SUB_GAME_MODE_DYING are the game state's. */
        s->pose = MZM_POSE_DYING;
        int16_t x_velocity = (int16_t)(s->camera_x + 240 * 2 - s->x);
        s->x_velocity = (int16_t)(x_velocity >> 1);     /* DIV_SHIFT */
        int16_t y_velocity = (int16_t)(s->camera_y + 160 * 2 + 80 - s->y);
        s->y_velocity = (int16_t)(y_velocity >> 4);
    }
    s->invincibility = MZM_INVINCIBILITY_TICKS;
    s->shinespark_timer = 0;
    s->standing = MZM_STANDING_MIDAIR;
    s->has_new_projectile = false;  /* pWeapon->newProjectile, for the rest of the frame */
}

/* SamusChangeToKnockbackPose */
static void change_to_knockback(Ctx *c, const MzmSamusData *copy) {
    MzmSamus *s = c->s;
    uint8_t collision = DETECT_NONE;
    switch (copy->pose) {
    case MZM_POSE_MORPHING:
    case MZM_POSE_MORPH_BALL:
    case MZM_POSE_ROLLING:
    case MZM_POSE_MORPH_BALL_MIDAIR:
    case MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL:
    case MZM_POSE_DELAY_BEFORE_BALLSPARKING:
    case MZM_POSE_BALLSPARK_COLLISION:
    case MZM_POSE_MORPH_BALL_ON_ZIPLINE:
    case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
        s->pose = MZM_POSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL;
        break;
    case MZM_POSE_CRAWLING_STOPPED:
    case MZM_POSE_STARTING_TO_CRAWL:
    case MZM_POSE_CRAWLING:
    case MZM_POSE_TURNING_AROUND_WHILE_CRAWLING:
    case MZM_POSE_SHOOTING_WHILE_CRAWLING:
        s->pose = MZM_POSE_CRAWLING_STOPPED;
        break;
    case MZM_POSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL:
        break;
    default:
        collision = collision_above(c, STANDING_TOP);
        s->pose = MZM_POSE_GETTING_KNOCKED_BACK;
        break;
    }
    if (collision != DETECT_NONE) s->y = s->y | SUB_PIXEL_POSITION_FLAG;
    s->y_velocity = 8 * (MZM_QUARTER_BLOCK_SIZE - MZM_PIXEL_SIZE / 2);
    if (copy->standing == MZM_STANDING_MIDAIR) s->y_velocity = 8 * (MZM_EIGHTH_BLOCK_SIZE - 1);
    s->aim = copy->aim;
    s->shinespark_timer = 0;
    s->standing = MZM_STANDING_MIDAIR;
    s->has_new_projectile = false;
}

/* SamusCheckCarryFromCopy */
static void carry_from_copy(Ctx *c, const MzmSamusData *copy) {
    MzmSamus *s = c->s;
    switch (s->pose) {
    case MZM_POSE_RUNNING:
        s->aim = held(c, MZM_KEY_L | MZM_KEY_R) ? copy->aim : MZM_AIM_NONE;
        break;
    case MZM_POSE_STANDING:
        s->aim = copy->aim;
        /* No up aim for 6 frames after uncrouching. */
        if (copy->pose == MZM_POSE_CROUCHING || copy->pose == MZM_POSE_SHOOTING_AND_CROUCHING)
            s->timer = 6;
        break;
    case MZM_POSE_CROUCHING:
        s->aim = copy->aim;
        s->anim_frame = 1;
        if (s->aim != MZM_AIM_FORWARD && s->aim != MZM_AIM_DIAGONAL_UP &&
            s->aim != MZM_AIM_DIAGONAL_DOWN)
            s->aim = MZM_AIM_FORWARD;
        break;
    case MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING:
    case MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL:
        /* The timer holds the slowed flag. */
        s->timer = s->slowed;
        if (s->slowed) mzm_sfx_play(MZM_SOUND_WATER_MOVEMENT_GRABBING_LEDGE);
        else mzm_sfx_play(suitless(c) ? MZM_SOUND_SUITLESS_GRABBING_LEDGE : MZM_SOUND_GRABBING_LEDGE);
        /* fallthrough */
    case MZM_POSE_MORPH_BALL:
        if (copy->speedboosting) s->timer = 6;
        s->diagonal_aim = MZM_DIAG_AIM_NONE;
        break;
    case MZM_POSE_SKIDDING:
        s->x_velocity = copy->x_velocity;
        s->diagonal_aim = MZM_DIAG_AIM_NONE;
        environmental_effect(c, WANTING_SKIDDING_EFFECT);
        mzm_sfx_play(MZM_SOUND_SKIDDING);
        break;
    case MZM_POSE_DELAY_BEFORE_SHINESPARKING:
        s->shinespark_timer = 50;
        mzm_sfx_play(MZM_SOUND_SHINESPARKING);
        break;
    case MZM_POSE_SHINESPARKING:
    case MZM_POSE_BALLSPARKING:
        if (copy->forced == MZM_FORCED_LAUNCHED_BY_CANNON) {
            s->y_velocity = MZM_SHINESPARK_VELOCITY;
        } else if (held(c, MZM_KEY_L)) {
            s->forced = MZM_FORCED_DIAGONAL_SHINESPARK;
            s->x_velocity = s->facing > 0 ? MZM_DIAGONAL_SHINESPARK_X_VELOCITY
                                          : -MZM_DIAGONAL_SHINESPARK_X_VELOCITY;
            s->y_velocity = MZM_DIAGONAL_SHINESPARK_Y_VELOCITY;
        } else if (held(c, dir_key(s))) {
            if (held(c, MZM_KEY_UP)) {
                s->forced = MZM_FORCED_DIAGONAL_SHINESPARK;
                s->x_velocity = s->facing > 0 ? MZM_DIAGONAL_SHINESPARK_X_VELOCITY
                                              : -MZM_DIAGONAL_SHINESPARK_X_VELOCITY;
                s->y_velocity = MZM_DIAGONAL_SHINESPARK_Y_VELOCITY;
            } else {
                s->forced += MZM_FORCED_SIDEWARDS_SHINESPARK;
                s->x_velocity = s->facing > 0 ? MZM_SHINESPARK_VELOCITY : -MZM_SHINESPARK_VELOCITY;
            }
        } else {
            s->y_velocity = MZM_SHINESPARK_VELOCITY;
        }
        s->speedboosting++;
        s->shinespark_timer = 0;
        s->diagonal_aim = MZM_DIAG_AIM_NONE;
        s->screw_speed_flag = SCREW_SPEED_FLAG_ACTIVE;
        break;
    case MZM_POSE_SHINESPARK_COLLISION:
        s->forced = copy->forced;
        s->anim_frame = 1;
        /* fallthrough */
    case MZM_POSE_BALLSPARK_COLLISION:
        s->invincibility = MZM_INVINCIBILITY_TICKS;
        mzm_sfx_play(MZM_SOUND_SHINESPARK_COLLISION);
        break;
    case MZM_POSE_DELAY_BEFORE_BALLSPARKING:
        if (copy->forced == MZM_FORCED_LAUNCHED_BY_CANNON) s->forced = MZM_FORCED_LAUNCHED_BY_CANNON;
        else s->screw_speed_flag = SCREW_SPEED_FLAG_ACTIVE;
        mzm_sfx_play(MZM_SOUND_BALLSPARKING);
        break;
    case MZM_POSE_HANGING_ON_LEDGE:
    case MZM_POSE_GRABBING_A_LEDGE_SUITLESS:
        /* The feet an eighth of a block below the grabbed block's top; the
         * echo reset (gSamusEcho) is the runtime's. */
        if ((s->y & SUB_PIXEL_POSITION_FLAG) < SUB_PIXEL_POSITION_FLAG / 2)
            s->y = (s->y & BLOCK_POSITION_FLAG) + MZM_EIGHTH_BLOCK_SIZE;
        else
            s->y = (s->y & BLOCK_POSITION_FLAG) + MZM_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE;
        s->y = U16(s->y);
        s->diagonal_aim = MZM_DIAG_AIM_NONE;
        break;
    case MZM_POSE_USING_AN_ELEVATOR:
        s->elevator_direction = copy->elevator_direction;
        s->y_velocity = s->elevator_direction & MZM_KEY_UP ? MZM_ELEVATOR_SPEED : -MZM_ELEVATOR_SPEED;
        mzm_sfx_play(MZM_SOUND_ELEVATOR);
        break;
    case MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND:
        /* SAMUS_CARRY_ELEVATOR_DIR */
        s->elevator_direction = copy->elevator_direction;
        s->diagonal_aim = MZM_DIAG_AIM_NONE;
        break;
    case MZM_POSE_ON_ZIPLINE:
        if (copy->pose != MZM_POSE_SHOOTING_ON_ZIPLINE && copy->pose != MZM_POSE_TURNING_ON_ZIPLINE) {
            s->diagonal_aim = MZM_DIAG_AIM_NONE;
            mzm_sfx_play(suitless(c) ? MZM_SOUND_SUITLESS_GRAB_LEDGE : MZM_SOUND_GRAB_LEDGE);
        } else {
            s->aim = copy->aim;
        }
        break;
    case MZM_POSE_SAVING_LOADING_GAME:
        s->last_wall = copy->last_wall;
        /* fallthrough */
    case MZM_POSE_DOWNLOADING_MAP_DATA:
        s->timer = 1;
        /* fallthrough */
    case MZM_POSE_MORPH_BALL_MIDAIR:
    case MZM_POSE_FACING_THE_FOREGROUND:
    case MZM_POSE_CROUCHING_TO_CRAWL:
        s->diagonal_aim = MZM_DIAG_AIM_NONE;
        break;
    case MZM_POSE_CRAWLING_STOPPED:
        if (copy->pose == MZM_POSE_SHOOTING_WHILE_CRAWLING) s->forced = MZM_FORCED_CRAWLING_ARM_CANNON_UP;
        break;
    case MZM_POSE_FACING_THE_BACKGROUND_SUITLESS:
        s->last_wall = 1;
        s->aim = copy->aim;
        break;
    case MZM_POSE_TURNING_TO_ENTER_ESCAPE_SHIP:
        s->turning = true;
        break;
    case MZM_POSE_TURNING_AROUND_MIDAIR:
        s->y_velocity = copy->y_velocity;
        /* fallthrough */
    case MZM_POSE_TURNING_AROUND:
    case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
    case MZM_POSE_TURNING_ON_ZIPLINE:
    case MZM_POSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA:
    case MZM_POSE_TURNING_AROUND_WHILE_CRAWLING:
        s->turning = true;
        s->aim = copy->aim;
        break;
    case MZM_POSE_IN_ESCAPE_SHIP:
        break;
    default:
        s->aim = copy->aim;
        break;
    }
}

/* SamusCopyData */
static void copy_data(MzmSamus *s) {
    s->copy = s->data;
    if (s->turning) {
        flip(s);
        s->turning = false;
    }
    s->aim = MZM_AIM_FORWARD;
    s->forced = 0;
    s->speedboosting = 0;
    s->walljump_timer = 0;
    s->timer = 0;
    s->last_wall = 0;
    s->elevator_direction = 0;
    s->x_velocity = 0;
    s->y_velocity = 0;
    s->anim_counter = 0;
    s->anim_frame = 0;
    if (s->shinespark_timer != MZM_SHINESPARK_TIMER_MAX) s->screw_speed_flag = SCREW_SPEED_FLAG_NONE;
}

/* SamusSetPose */
static void set_pose(Ctx *c, MzmPose pose) {
    MzmSamus *s = c->s;
    if (pose == MZM_POSE_KNOCKBACK_REQUEST || pose == MZM_POSE_HURT_REQUEST) s->turning = false;
    copy_data(s);
    MzmSamusData *copy = &s->copy;
    if (copy->aim == MZM_AIM_NONE) copy->aim = MZM_AIM_FORWARD;
    switch (copy->pose) {
    case MZM_POSE_DELAY_BEFORE_SHINESPARKING:
        if (pose != MZM_POSE_SHINESPARKING) mzm_sfx_stop(MZM_SOUND_SHINESPARKING);
        break;
    case MZM_POSE_SHINESPARKING:
        if (pose != MZM_POSE_SHINESPARK_COLLISION) mzm_sfx_stop(MZM_SOUND_SHINESPARKING);
        break;
    case MZM_POSE_DELAY_BEFORE_BALLSPARKING:
        if (pose != MZM_POSE_BALLSPARKING) mzm_sfx_stop(MZM_SOUND_BALLSPARKING);
        break;
    case MZM_POSE_BALLSPARKING:
        if (pose != MZM_POSE_BALLSPARK_COLLISION) mzm_sfx_stop(MZM_SOUND_BALLSPARKING);
        break;
    default:
        break;
    }
    switch (pose) {
    case MZM_POSE_MID_AIR_REQUEST: set_midair(c, copy); break;
    case MZM_POSE_LANDING_REQUEST: set_landing(c, copy); break;
    case MZM_POSE_HURT_REQUEST: change_to_hurt(c, copy); break;
    case MZM_POSE_KNOCKBACK_REQUEST: change_to_knockback(c, copy); break;
    default:
        s->pose = pose;
        carry_from_copy(c, copy);
        break;
    }
}

/* SamusUpdatePhysics (no liquid: gEquipment.grabbedByMetroid and the
 * hazards of ClipdataCheckCurrentAffectingAtPosition). */
typedef struct {
    int16_t x_acceleration, x_velocity_cap, midair_x_velocity_cap, midair_x_acceleration;
    int16_t midair_morphed_x_velocity_cap, y_acceleration, positive_y_cap, negative_y_cap;
} Physics;

static Physics update_physics(Ctx *c) {
    MzmSamus *s = c->s;
    int32_t y;
    switch (s->pose) {
    case MZM_POSE_HANGING_ON_LEDGE:
    case MZM_POSE_TURNING_TO_AIM_WHILE_HANGING:
    case MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING:
    case MZM_POSE_AIMING_WHILE_HANGING:
    case MZM_POSE_SHOOTING_WHILE_HANGING:
    case MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING:
    case MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING:
    case MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL:
    case MZM_POSE_GRABBING_A_LEDGE_SUITLESS:
        y = s->y - MZM_QUARTER_BLOCK_SIZE;
        break;
    default:
        y = s->y;
        break;
    }
    s->keep_vertical = false;
    uint8_t hazard = affecting(c, y, s->x) & 0xFF;
    bool slowed = false;
    /* HAZARD_TYPE_WATER .. HAZARD_TYPE_STRONG_ACID */
    if (hazard >= 1 && hazard <= 4 && !has(c, MZM_ITEM_GRAVITY_SUIT)) slowed = true;
    s->slowed = slowed;
    Physics p;
    if (slowed) {
        p = (Physics){MZM_X_ACCELERATION / 2, MZM_X_VELOCITY_CAP / 2, MZM_X_MIDAIR_VELOCITY_CAP / 2,
                      MZM_X_MIDAIR_ACCELERATION / 2, MZM_X_MIDAIR_MORPHED_VELOCITY_CAP / 2,
                      MZM_Y_ACCELERATION / 2, MZM_Y_SLOWED_VELOCITY_CAP, MZM_Y_SLOWED_VELOCITY_CAP};
        if (s->speedboosting) s->speedboosting = false;
    } else {
        p = (Physics){MZM_X_ACCELERATION, MZM_X_VELOCITY_CAP, MZM_X_MIDAIR_VELOCITY_CAP,
                      MZM_X_MIDAIR_ACCELERATION, MZM_X_MIDAIR_MORPHED_VELOCITY_CAP,
                      MZM_Y_ACCELERATION, MZM_Y_POSITIVE_VELOCITY_CAP, MZM_Y_NEGATIVE_VELOCITY_CAP};
        if (s->speedboosting) {
            p.midair_x_velocity_cap = MZM_X_SPEEDBOOST_VELOCITY_CAP;
            p.x_velocity_cap = MZM_X_SPEEDBOOST_VELOCITY_CAP;
        }
    }
    if (s->standing == MZM_STANDING_MIDAIR || s->standing == MZM_STANDING_ENEMY)
        s->current_slope = SLOPE_NONE;
    return p;
}

/* SamusChangeVelocityOnSlope */
static int16_t velocity_on_slope(const MzmSamus *s) {
    int32_t velocity = s->x_velocity;
    if (dir_bits(s) & s->current_slope) {
        if (s->current_slope & SLOPE_STEEP) velocity = (int16_t)(velocity * 3 / 5);
        else velocity = (int16_t)(velocity * 4 / 5);
    } else {
        if (velocity < -0xA0) velocity = -0xA0;
        else if (velocity > 0xA0) velocity = 0xA0;
    }
    return (int16_t)velocity;
}

/* SamusApplyXAcceleration */
static void apply_x_acceleration(MzmSamus *s, int acceleration, int cap) {
    s->x_velocity = mzm_apply_x_acceleration(s->x_velocity, s->facing, (int16_t)acceleration,
                                             (int16_t)cap);
}

/* SamusAimCannon */
static void aim_cannon(Ctx *c) {
    MzmSamus *s = c->s;
    if (held(c, MZM_KEY_L)) {
        switch (s->pose) {
        case MZM_POSE_RUNNING:
        case MZM_POSE_STANDING:
        case MZM_POSE_SHOOTING:
        case MZM_POSE_CROUCHING:
        case MZM_POSE_SHOOTING_AND_CROUCHING:
        case MZM_POSE_MIDAIR:
        case MZM_POSE_LANDING:
        case MZM_POSE_STARTING_SPIN_JUMP:
        case MZM_POSE_SPINNING:
        case MZM_POSE_SPACE_JUMPING:
        case MZM_POSE_SCREW_ATTACKING:
        case MZM_POSE_AIMING_WHILE_HANGING:
        case MZM_POSE_UNCROUCHING_SUITLESS:
        case MZM_POSE_CROUCHING_SUITLESS:
            if (held(c, MZM_KEY_DOWN)) {
                s->aim = MZM_AIM_DIAGONAL_DOWN;
                s->diagonal_aim = MZM_DIAG_AIM_DOWN;
            } else if (s->diagonal_aim == MZM_DIAG_AIM_NONE || s->diagonal_aim == MZM_DIAG_AIM_UP ||
                       held(c, MZM_KEY_UP)) {
                s->aim = MZM_AIM_DIAGONAL_UP;
                s->diagonal_aim = MZM_DIAG_AIM_UP;
            }
            break;
        case MZM_POSE_ON_ZIPLINE:
            s->aim = MZM_AIM_DIAGONAL_DOWN;
            s->diagonal_aim = MZM_DIAG_AIM_DOWN;
            break;
        default:
            break;
        }
        return;
    }
    switch (s->pose) {
    case MZM_POSE_RUNNING:
        if (held(c, MZM_KEY_UP)) s->aim = MZM_AIM_DIAGONAL_UP;
        else if (held(c, MZM_KEY_DOWN)) s->aim = MZM_AIM_DIAGONAL_DOWN;
        else if (s->aim <= MZM_AIM_DOWN) s->aim = MZM_AIM_FORWARD;
        if (!suitless(c) && s->aim == MZM_AIM_NONE &&
            (c->in->weapon_highlighted != 0 || c->in->charge_counter != 0))
            s->aim = MZM_AIM_FORWARD;
        break;
    case MZM_POSE_STANDING:
    case MZM_POSE_SHOOTING:
    case MZM_POSE_LANDING:
    case MZM_POSE_UNCROUCHING_SUITLESS:
        s->aim = s->timer == 0 && held(c, MZM_KEY_UP) ? MZM_AIM_UP : MZM_AIM_FORWARD;
        break;
    case MZM_POSE_CROUCHING:
    case MZM_POSE_SHOOTING_AND_CROUCHING:
    case MZM_POSE_CROUCHING_SUITLESS:
        s->aim = MZM_AIM_FORWARD;
        break;
    case MZM_POSE_MIDAIR:
    case MZM_POSE_STARTING_SPIN_JUMP:
    case MZM_POSE_SPINNING:
    case MZM_POSE_SPACE_JUMPING:
    case MZM_POSE_SCREW_ATTACKING:
    case MZM_POSE_AIMING_WHILE_HANGING:
        if (held(c, MZM_KEY_UP))
            s->aim = held(c, dir_key(s)) ? MZM_AIM_DIAGONAL_UP : MZM_AIM_UP;
        else if (held(c, MZM_KEY_DOWN))
            s->aim = held(c, dir_key(s)) ? MZM_AIM_DIAGONAL_DOWN : MZM_AIM_DOWN;
        else if (held(c, dir_key(s)))
            s->aim = MZM_AIM_FORWARD;
        else if (s->aim != MZM_AIM_UP && s->aim != MZM_AIM_DOWN)
            s->aim = MZM_AIM_FORWARD;
        break;
    case MZM_POSE_ON_ZIPLINE:
        if (held(c, MZM_KEY_DOWN))
            s->aim = held(c, dir_key(s)) ? MZM_AIM_DIAGONAL_DOWN : MZM_AIM_DOWN;
        else if (held(c, dir_key(s)) || s->aim != MZM_AIM_DOWN)
            s->aim = MZM_AIM_FORWARD;
        break;
    default:
        break;
    }
    s->diagonal_aim = MZM_DIAG_AIM_NONE;
}

/* SamusSetSpinningPose */
static void set_spinning_pose(Ctx *c) {
    MzmSamus *s = c->s;
    switch (s->pose) {
    case MZM_POSE_SPINNING:
        if (s->slowed) break;
        if (has(c, MZM_ITEM_SCREW_ATTACK)) s->pose = MZM_POSE_SCREW_ATTACKING;
        else if (has(c, MZM_ITEM_SPACE_JUMP)) s->pose = MZM_POSE_SPACE_JUMPING;
        break;
    case MZM_POSE_SPACE_JUMPING:
        if (has(c, MZM_ITEM_SCREW_ATTACK)) {
            s->pose = MZM_POSE_SCREW_ATTACKING;
        } else if (!has(c, MZM_ITEM_SPACE_JUMP) || s->slowed) {
            s->pose = MZM_POSE_SPINNING;
            s->anim_frame = 0;
        }
        break;
    case MZM_POSE_SCREW_ATTACKING:
        if (s->slowed) {
            s->pose = MZM_POSE_SPINNING;
            s->anim_frame = 0;
        } else if (!has(c, MZM_ITEM_SCREW_ATTACK)) {
            s->pose = has(c, MZM_ITEM_SPACE_JUMP) ? MZM_POSE_SPACE_JUMPING : MZM_POSE_SPINNING;
            s->anim_frame = 0;
        }
        s->screw_speed_flag = SCREW_SPEED_FLAG_NONE;
        break;
    default:
        break;
    }
}

/* SamusCheckShinesparking */
static void check_shinesparking(MzmSamus *s) {
    if (s->pose != MZM_POSE_DYING) {
        if (s->pose == MZM_POSE_SHINESPARKING || s->pose == MZM_POSE_BALLSPARKING) {
            s->speedboosting = true;
        } else if ((uint16_t)(s->x_velocity + 8 * (MZM_QUARTER_BLOCK_SIZE + MZM_PIXEL_SIZE) - 1) >=
                   8 * (MZM_HALF_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE) - 1) {
            if (!s->speedboosting && s->pose != MZM_POSE_SKIDDING) {
                s->speedboosting++;
                mzm_sfx_play(MZM_SOUND_SPEEDBOOSTER_START);
            }
        } else if (s->speedboosting) {
            s->speedboosting = false;
        }
    } else {
        s->speedboosting = false;
    }
    if (!s->speedboosting) mzm_sfx_stop(MZM_SOUND_SPEEDBOOSTER_START);
}

/* ---- Animations ---- */

static const char *body_family(const MzmEquipment *eq) {
    return eq->suit == MZM_SUIT_SUITLESS ? "Suitless" :
           eq->suit == MZM_SUIT_NORMAL ? "PowerSuit" : "FullSuit";
}

/* The durations of a library animation; 0 frames when unavailable. */
static int durations(const Ctx *c, const char *key, uint8_t *out) {
    if (!c->a || !c->a->durations) return 0;
    return c->a->durations(c->a->context, key, out, 256);
}

/* One step of a body animation (SamusUpdateAnimation's frame logic) on the
 * named table; `slowed` doubles each frame's time. */
static int anim_step(Ctx *c, const char *key, bool slowed) {
    MzmSamus *s = c->s;
    uint8_t d[256];
    int count = durations(c, key, d);
    if (count <= 0) {
        s->anim_frame = 0;
        s->anim_counter = 0;
        return ANIM_ENDED;
    }
    unsigned timer = s->anim_frame < count ? d[s->anim_frame] : 0;
    if (slowed) timer *= 2;
    if (s->anim_counter >= timer) {
        s->anim_counter = 0;
        s->anim_frame++;
        return s->anim_frame < count ? ANIM_SUB_ENDED : ANIM_ENDED;
    }
    return ANIM_NONE;
}

/* SamusUpdateAnimation: sSamusAnimPointers_PowerSuit[pose][0] (or the
 * suitless table). */
static int update_animation(Ctx *c, bool slowed) {
    char key[128];
    snprintf(key, sizeof key, "%s/pose/%s/right", suitless(c) ? "Suitless" : "PowerSuit",
             mzm_pose_name(c->s->pose));
    return anim_step(c, key, slowed);
}

/* ---- Pose handlers ---- */

/* The shinespark start of the ground poses: A without a direction, with
 * half a block of room above the head (Samus rises by it). */
static bool ground_spark(Ctx *c) {
    MzmSamus *s = c->s;
    if (held(c, MZM_KEY_RIGHT | MZM_KEY_LEFT) || !pressed(c, MZM_KEY_A) || !s->shinespark_timer)
        return false;
    if (collision_above(c, STANDING_TOP - MZM_HALF_BLOCK_SIZE) != DETECT_NONE) return false;
    s->y = U16(s->y - MZM_HALF_BLOCK_SIZE);
    return true;
}

/* SamusCheckJumping */
static bool check_jumping(Ctx *c) {
    if (!jump_pressed(c)) return false;
    c->s->forced = MZM_FORCED_JUMP;
    return true;
}

/* SamusRunning */
static MzmPose pose_running(Ctx *c, const Physics *p) {
    MzmSamus *s = c->s;
    if (jump_pressed(c)) {
        s->forced = MZM_FORCED_JUMP;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    int cap = p->x_velocity_cap;
    if (has(c, MZM_ITEM_SPEED_BOOSTER) && !s->slowed) {
        if (s->timer >= 140) cap = 8 * (MZM_QUARTER_BLOCK_SIZE + MZM_PIXEL_SIZE);
        else if (s->timer >= 120) cap = 140;   /* SUB_PIXEL_TO_VELOCITY(16 + 1 + 1 / 2.f) */
        uint16_t velocity = (uint16_t)(s->x_velocity + 8 * (MZM_PIXEL_SIZE * 3) - 1);
        if (velocity < 8 * (MZM_QUARTER_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE) - 1) s->timer = 0;
        else if (s->timer < 160) s->timer++;
    } else {
        s->timer = 0;
    }
    if (held(c, dir_key(s))) {
        apply_x_acceleration(s, p->x_acceleration, cap);
        aim_cannon(c);
        return MZM_POSE_NONE;
    }
    if (s->speedboosting) return MZM_POSE_SKIDDING;
    if (s->has_new_projectile) return MZM_POSE_SHOOTING;
    return held(c, opp_key(s)) ? MZM_POSE_TURNING_AROUND : MZM_POSE_STANDING;
}

/* SamusRunningGfx: sSamusAnimPointers_PowerSuit_Running(_Speedboosting)[0][0]. */
static MzmPose gfx_running(Ctx *c) {
    MzmSamus *s = c->s;
    const char *key;
    if (!s->speedboosting) {
        key = "PowerSuit/Running/ACD_FORWARD/right";
        s->screw_speed_flag = SCREW_SPEED_FLAG_NONE;
    } else {
        key = "PowerSuit/Running_Speedboosting/ACD_FORWARD/right";
        s->screw_speed_flag = SCREW_SPEED_FLAG_ACTIVE;
    }
    uint8_t d[256];
    int count = durations(c, key, d);
    unsigned timer = count > 0 && s->anim_frame < count ? d[s->anim_frame] : 0;
    if (s->slowed) timer *= 2;
    if (s->anim_counter >= timer) {
        s->anim_counter = 0;
        s->anim_frame++;
        if (s->anim_frame >= count) s->anim_frame = 0;
        switch (s->anim_frame) {
        case 1:
            if (s->slowed) mzm_sfx_play(MZM_SOUND_WATER_MOVEMENT_RUNNING);
            else mzm_sfx_play(suitless(c) ? MZM_SOUND_SUITLESS_FOOTSTEPS_1 : MZM_SOUND_FOOTSTEPS_1);
            break;
        case 2: environmental_effect(c, WANTING_RUNNING_EFFECT); break;
        case 7: environmental_effect(c, WANTING_RUNNING_EFFECT_); break;
        case 6:
            if (s->slowed) mzm_sfx_play(MZM_SOUND_WATER_MOVEMENT_RUNNING);
            else mzm_sfx_play(suitless(c) ? MZM_SOUND_SUITLESS_FOOTSTEPS_2 : MZM_SOUND_FOOTSTEPS_2);
            break;
        default: break;
        }
    }
    return MZM_POSE_NONE;
}

/* SamusStanding */
static MzmPose pose_standing(Ctx *c) {
    MzmSamus *s = c->s;
    if (ground_spark(c)) return MZM_POSE_DELAY_BEFORE_SHINESPARKING;
    if (check_jumping(c)) return MZM_POSE_MID_AIR_REQUEST;
    if (held(c, dir_key(s))) return MZM_POSE_RUNNING;
    if (s->has_new_projectile) return MZM_POSE_SHOOTING;
    if (held(c, opp_key(s))) return MZM_POSE_TURNING_AROUND;
    if (pressed(c, MZM_KEY_DOWN)) {
        if (affecting(c, s->y + MZM_PIXEL_SIZE, s->x) >> 16 == MZM_SAMUS_MOVEMENT_ELEVATOR_DOWN_BLOCK) {
            s->x = U16((s->x & BLOCK_POSITION_FLAG) + MZM_HALF_BLOCK_SIZE);
            s->elevator_direction = MZM_KEY_DOWN;
            return MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND;
        }
        if (s->diagonal_aim == MZM_DIAG_AIM_NONE || s->aim == MZM_AIM_DIAGONAL_DOWN) {
            if (!suitless(c)) mzm_sfx_play(MZM_SOUND_CROUCHING);
            if (s->has_new_projectile) return MZM_POSE_SHOOTING_AND_CROUCHING;
            return suitless(c) ? MZM_POSE_CROUCHING_SUITLESS : MZM_POSE_CROUCHING;
        }
    } else if (pressed(c, MZM_KEY_UP)) {
        if (affecting(c, s->y + MZM_PIXEL_SIZE, s->x) >> 16 == MZM_SAMUS_MOVEMENT_ELEVATOR_UP_BLOCK) {
            s->x = U16((s->x & BLOCK_POSITION_FLAG) + MZM_HALF_BLOCK_SIZE);
            s->elevator_direction = MZM_KEY_UP;
            return MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND;
        }
    }
    if (s->timer) s->timer--;
    aim_cannon(c);
    return MZM_POSE_NONE;
}

/* SamusStandingGfx: a looping animation. */
static MzmPose gfx_loop(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) c->s->anim_frame = 0;
    return MZM_POSE_NONE;
}

/* SamusTurningAround */
static MzmPose pose_turning_around(Ctx *c) {
    MzmSamus *s = c->s;
    if (ground_spark(c)) return MZM_POSE_DELAY_BEFORE_SHINESPARKING;
    if (check_jumping(c)) return MZM_POSE_MID_AIR_REQUEST;
    if (pressed(c, MZM_KEY_DOWN) &&
        (s->diagonal_aim == MZM_DIAG_AIM_NONE || s->aim == MZM_AIM_DIAGONAL_DOWN)) {
        /* Not returned: the crouched turn continues the animation. */
        s->pose = MZM_POSE_TURNING_AROUND_AND_CROUCHING;
        if (!suitless(c)) mzm_sfx_play(MZM_SOUND_CROUCHING);
    }
    if (s->has_new_projectile)
        return s->pose == MZM_POSE_TURNING_AROUND_AND_CROUCHING ? MZM_POSE_SHOOTING_AND_CROUCHING
                                                                : MZM_POSE_SHOOTING;
    return MZM_POSE_NONE;
}

/* SamusTurningAroundGfx */
static MzmPose gfx_turning_around(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) {
        if (held(c, opp_key(c->s))) return MZM_POSE_RUNNING;
        return suitless(c) ? MZM_POSE_UNCROUCHING_SUITLESS : MZM_POSE_STANDING;
    }
    return MZM_POSE_NONE;
}

/* SamusShootingGfx */
static MzmPose gfx_shooting(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_STANDING : MZM_POSE_NONE;
}

/* The x position SamusCrouching and SamusTurningAroundAndCrouching clamp
 * to against a block at the head's edge (with the native RIGHT_MOST bug:
 * the left hitbox offset). */
static uint16_t crouch_clamp(const MzmSamus *s, uint8_t collision) {
    if (collision == DETECT_LEFT_MOST) return (uint16_t)((s->x & BLOCK_POSITION_FLAG) - STANDING_LEFT);
    if (collision == DETECT_RIGHT_MOST)
        return (uint16_t)((s->x & BLOCK_POSITION_FLAG) - STANDING_LEFT + SUB_PIXEL_POSITION_FLAG);
    return (uint16_t)s->x;
}

static bool clamp_edge(uint8_t collision) {
    return collision == DETECT_LEFT_MOST || collision == DETECT_RIGHT_MOST;
}

/* SamusCrouching */
static MzmPose pose_crouching(Ctx *c) {
    MzmSamus *s = c->s;
    if (ground_spark(c)) return MZM_POSE_DELAY_BEFORE_SHINESPARKING;
    uint8_t collision = collision_above(c, STANDING_TOP);
    uint16_t x = crouch_clamp(s, collision);
    if (check_jumping(c) && !(collision & DETECT_MIDDLE)) {
        if (clamp_edge(collision)) s->x = x;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (pressed(c, MZM_KEY_UP) && !(collision & DETECT_MIDDLE) &&
        (s->diagonal_aim == MZM_DIAG_AIM_NONE || s->aim == MZM_AIM_DIAGONAL_UP)) {
        if (clamp_edge(collision)) s->x = x;
        s->last_wall = 6;           /* a leftover */
        return suitless(c) ? MZM_POSE_UNCROUCHING_SUITLESS : MZM_POSE_STANDING;
    }
    if (pressed(c, MZM_KEY_DOWN) && has(c, MZM_ITEM_MORPH_BALL) &&
        (s->diagonal_aim == MZM_DIAG_AIM_NONE || s->aim == MZM_AIM_DIAGONAL_DOWN)) {
        mzm_sfx_play(MZM_SOUND_MORPHING);
        return MZM_POSE_MORPHING;
    }
    aim_cannon(c);
    if (s->aim == MZM_AIM_UP) s->aim = MZM_AIM_FORWARD;
    if (s->has_new_projectile) return MZM_POSE_SHOOTING_AND_CROUCHING;
    if (held(c, opp_key(s))) return MZM_POSE_TURNING_AROUND_AND_CROUCHING;
    if (held(c, dir_key(s))) {
        if (!(collision & DETECT_MIDDLE) && s->timer++ > 5) {
            if (clamp_edge(collision)) s->x = x;
            return suitless(c) ? MZM_POSE_UNCROUCHING_SUITLESS : MZM_POSE_STANDING;
        }
    } else {
        s->timer = 0;
    }
    return MZM_POSE_NONE;
}

/* SamusTurningAroundAndCrouching */
static MzmPose pose_turning_around_and_crouching(Ctx *c) {
    MzmSamus *s = c->s;
    if (ground_spark(c)) return MZM_POSE_DELAY_BEFORE_SHINESPARKING;
    uint8_t collision = collision_above(c, STANDING_TOP);
    uint16_t x = crouch_clamp(s, collision);
    if (check_jumping(c) && !(collision & DETECT_MIDDLE)) {
        if (clamp_edge(collision)) s->x = x;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (pressed(c, MZM_KEY_UP) && !(collision & DETECT_MIDDLE) &&
        (s->diagonal_aim == MZM_DIAG_AIM_NONE || s->aim == MZM_AIM_DIAGONAL_UP)) {
        if (clamp_edge(collision)) s->x = x;
        /* Not returned: the standing turn continues the animation. */
        s->pose = MZM_POSE_TURNING_AROUND;
    }
    if (s->has_new_projectile)
        return s->pose == MZM_POSE_TURNING_AROUND ? MZM_POSE_SHOOTING : MZM_POSE_SHOOTING_AND_CROUCHING;
    return MZM_POSE_NONE;
}

/* SamusTurningAroundAndCrouchingGfx */
static MzmPose gfx_turning_around_and_crouching(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED)
        return suitless(c) ? MZM_POSE_CROUCHING_SUITLESS : MZM_POSE_CROUCHING;
    return MZM_POSE_NONE;
}

/* SamusShootingAndCrouchingGfx */
static MzmPose gfx_shooting_and_crouching(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_CROUCHING : MZM_POSE_NONE;
}

/* SamusSkidding */
static MzmPose pose_skidding(Ctx *c) {
    MzmSamus *s = c->s;
    if (jump_pressed(c)) {
        s->forced = MZM_FORCED_JUMP;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (held(c, dir_key(s))) return MZM_POSE_RUNNING;
    if ((c->in->held & (MZM_KEY_RIGHT | MZM_KEY_LEFT | MZM_KEY_UP | MZM_KEY_DOWN)) == MZM_KEY_DOWN) {
        s->shinespark_timer = MZM_SHINESPARK_TIMER_MAX;
        s->screw_speed_flag = SCREW_SPEED_FLAG_STORING_SHINESPARK;
        return suitless(c) ? MZM_POSE_CROUCHING_SUITLESS : MZM_POSE_CROUCHING;
    }
    /* SUB_PIXEL_TO_VELOCITY(ONE_SUB_PIXEL + ONE_SUB_PIXEL / 4.f) */
    if (s->facing > 0) {
        s->x_velocity = (int16_t)(s->x_velocity - 10);
        if (s->x_velocity <= 0) return MZM_POSE_STANDING;
    } else {
        s->x_velocity = (int16_t)(s->x_velocity + 10);
        if (s->x_velocity >= 0) return MZM_POSE_STANDING;
    }
    return MZM_POSE_NONE;
}

/* SamusMidAir */
static MzmPose pose_midair(Ctx *c, const Physics *p) {
    MzmSamus *s = c->s;
    if (pressed(c, MZM_KEY_DOWN) && s->aim == MZM_AIM_DOWN && has(c, MZM_ITEM_MORPH_BALL)) {
        mzm_sfx_play(MZM_SOUND_MORPHING);
        return MZM_POSE_MORPH_BALL_MIDAIR;
    }
    if (jump_pressed(c)) {
        if (s->shinespark_timer) return MZM_POSE_DELAY_BEFORE_SHINESPARKING;
        if (!held(c, MZM_KEY_UP | MZM_KEY_DOWN)) {
            s->pose = MZM_POSE_SPINNING;
            s->anim_frame = 0;
            s->anim_counter = 0;
            return MZM_POSE_NONE;
        }
    }
    bool turning = false;
    int acceleration = p->midair_x_acceleration;
    if (held(c, dir_key(s))) {
        apply_x_acceleration(s, acceleration, p->midair_x_velocity_cap);
    } else if (held(c, opp_key(s))) {
        turning = true;
    } else if (s->x_velocity > 0) {
        s->x_velocity = (int16_t)(s->x_velocity - acceleration);
        if (s->x_velocity < 0) s->x_velocity = 0;
    } else if (s->x_velocity < 0) {
        s->x_velocity = (int16_t)(s->x_velocity + acceleration);
        if (s->x_velocity > 0) s->x_velocity = 0;
    }
    aim_cannon(c);
    if (turning) return MZM_POSE_TURNING_AROUND_MIDAIR;
    if (!held(c, MZM_KEY_A) && s->y_velocity > 0) s->y_velocity = 0;
    return MZM_POSE_NONE;
}

/* SamusMidAirGfx: rising holds the first legs, falling starts at frame 2,
 * the last frame is held. */
static MzmPose gfx_midair(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->y_velocity >= 0) {
        if (s->anim_frame == 2) s->anim_counter = 0;
    } else if (s->anim_frame < 2) {
        s->anim_frame = 2;
    }
    if (update_animation(c, false) == ANIM_ENDED) s->anim_frame = 4;
    return MZM_POSE_NONE;
}

/* SamusTurningAroundMidAir */
static MzmPose pose_turning_around_midair(Ctx *c) {
    MzmSamus *s = c->s;
    if (jump_pressed(c)) {
        if (s->shinespark_timer) return MZM_POSE_DELAY_BEFORE_SHINESPARKING;
        if (!held(c, MZM_KEY_UP | MZM_KEY_DOWN)) {
            s->pose = MZM_POSE_SPINNING;
            flip(s);
            s->anim_frame = 0;
            s->anim_counter = 0;
            s->turning = false;
            return MZM_POSE_NONE;
        }
    }
    if (s->has_new_projectile) {
        s->forced = MZM_FORCED_CARRY;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (!held(c, MZM_KEY_A) && s->y_velocity > 0) s->y_velocity = 0;
    return MZM_POSE_NONE;
}

/* SamusTurningAroundMidAirGfx */
static MzmPose gfx_turning_around_midair(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) {
        c->s->forced = MZM_FORCED_CARRY;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    return MZM_POSE_NONE;
}

/* SamusStartingSpinJumpGfx */
static MzmPose gfx_starting_spin_jump(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) {
        c->s->pose = MZM_POSE_SPINNING;
        c->s->anim_frame = 0;
    }
    return MZM_POSE_NONE;
}

/* SamusSpinning */
static MzmPose pose_spinning(Ctx *c, const Physics *p) {
    MzmSamus *s = c->s;
    if (s->has_new_projectile) {
        s->forced = MZM_FORCED_FALL;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (!held(c, MZM_KEY_RIGHT | MZM_KEY_LEFT) && held(c, MZM_KEY_UP | MZM_KEY_DOWN)) {
        s->forced = MZM_FORCED_CARRY;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    aim_cannon(c);
    int acceleration = p->midair_x_acceleration;
    if (has(c, MZM_ITEM_SPACE_JUMP) && !s->slowed) {
        if (jump_pressed(c) && s->y_velocity <= -8 * MZM_EIGHTH_BLOCK_SIZE) {
            s->y_velocity = has(c, MZM_ITEM_HIGH_JUMP) ? MZM_HIGH_JUMP_VELOCITY : MZM_LOW_JUMP_VELOCITY;
            return MZM_POSE_NONE;
        }
    } else if (s->walljump_timer) {
        s->walljump_timer--;
        if (dir_bits(s) & s->last_wall) {
            if (jump_pressed(c)) {
                int32_t x_offset = s->last_wall & NK_RIGHT ? -(MZM_HALF_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE)
                                                           : MZM_HALF_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE;
                if (clip(c, s->y, s->x + x_offset) & MZM_SAMUS_CLIP_SOLID_FLAG) {
                    face_bits(s, s->last_wall);
                    return MZM_POSE_STARTING_WALL_JUMP;
                }
            }
            acceleration = 1;
        }
    }
    if (held(c, opp_key(s))) {
        flip(s);
        s->x_velocity = 0;
    } else {
        apply_x_acceleration(s, acceleration, p->midair_x_velocity_cap);
    }
    if (!held(c, MZM_KEY_A) && s->y_velocity > 0) s->y_velocity = 0;
    return MZM_POSE_NONE;
}

/* SamusSpinningGfx */
static MzmPose gfx_spinning(Ctx *c) {
    MzmSamus *s = c->s;
    if (update_animation(c, s->slowed) == ANIM_ENDED) s->anim_frame = 0;
    if (s->anim_frame == 0 && s->anim_counter == 1) {
        if (s->slowed) mzm_sfx_play(MZM_SOUND_WATER_SPIN_JUMP);
        else mzm_sfx_play(suitless(c) ? MZM_SOUND_SUITLESS_SPIN_JUMP : MZM_SOUND_SPIN_JUMP);
    }
    return MZM_POSE_NONE;
}

/* SamusStartingWallJump */
static MzmPose pose_starting_wall_jump(Ctx *c) {
    if (c->s->has_new_projectile ||
        (!held(c, MZM_KEY_RIGHT | MZM_KEY_LEFT) && held(c, MZM_KEY_UP | MZM_KEY_DOWN))) {
        c->s->forced = MZM_FORCED_FALL;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    return MZM_POSE_NONE;
}

/* SamusStartingWallJumpGfx */
static MzmPose gfx_starting_wall_jump(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) {
        c->s->forced = MZM_FORCED_JUMP;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    return MZM_POSE_NONE;
}

/* SamusSpaceJumpingGfx */
static MzmPose gfx_space_jumping(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->anim_counter == 1 && s->anim_frame == 0) mzm_sfx_play(MZM_SOUND_SPACE_JUMP);
    if (update_animation(c, s->slowed) == ANIM_ENDED) s->anim_frame = 0;
    return MZM_POSE_NONE;
}

/* SamusScrewAttackingGfx: sSamusAnimPointers_PowerSuit_ScrewAttacking. */
static MzmPose gfx_screw_attacking(Ctx *c) {
    MzmSamus *s = c->s;
    bool space = has(c, MZM_ITEM_SPACE_JUMP);
    if (s->anim_counter == 1 && s->anim_frame == 0)
        mzm_sfx_play(space ? MZM_SOUND_SPACE_SCREW_ATTACK : MZM_SOUND_SCREW_ATTACK);
    if (anim_step(c, space ? "PowerSuit/ScrewAttacking/TRUE/right"
                           : "PowerSuit/ScrewAttacking/FALSE/right", s->slowed) == ANIM_ENDED)
        s->anim_frame = 0;
    s->screw_speed_flag = SCREW_SPEED_FLAG_ACTIVE;
    return MZM_POSE_NONE;
}

/* SamusMorphing */
static MzmPose pose_morphing(Ctx *c) {
    if (pressed(c, MZM_KEY_UP)) c->s->pose = MZM_POSE_UNMORPHING;
    return MZM_POSE_NONE;
}

/* SamusMorphingGfx */
static MzmPose gfx_morphing(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_MORPH_BALL : MZM_POSE_NONE;
}

/* SamusMorphball */
static MzmPose pose_morph_ball(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->forced > MZM_FORCED_BOUNCE_BEFORE_JUMP + 1) {
        s->forced = MZM_FORCED_JUMP;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (s->forced >= MZM_FORCED_BOUNCE_BEFORE_JUMP) {
        s->forced++;
        return MZM_POSE_NONE;
    }
    if (jump_pressed(c) && has(c, MZM_ITEM_HIGH_JUMP) && s->shinespark_timer &&
        collision_above(c, block_hitboxes[MZM_HITBOX_MORPHED].top - MZM_BLOCK_SIZE) == DETECT_NONE) {
        s->y = U16(s->y - MZM_HALF_BLOCK_SIZE);
        return MZM_POSE_DELAY_BEFORE_BALLSPARKING;
    }
    if (check_jumping(c)) {
        if (s->forced != MZM_FORCED_JUMP) return MZM_POSE_MID_AIR_REQUEST;
        if (has(c, MZM_ITEM_HIGH_JUMP)) return MZM_POSE_MID_AIR_REQUEST;
        s->forced = 0;
    }
    if (held(c, MZM_KEY_RIGHT | MZM_KEY_LEFT)) {
        s->facing = held(c, MZM_KEY_RIGHT) ? 1 : -1;
        return MZM_POSE_ROLLING;
    }
    if (pressed(c, MZM_KEY_UP)) {
        uint8_t collision = collision_above(c, STANDING_TOP);
        /* The smooth unmorph when the head is barely in a block. */
        if (collision == DETECT_LEFT_MOST) {
            uint16_t x = (uint16_t)(s->x + STANDING_LEFT);
            s->x = U16((x & BLOCK_POSITION_FLAG) - STANDING_LEFT + MZM_BLOCK_SIZE);
            s->prev_x = s->x;
            collision = DETECT_NONE;
        } else if (collision == DETECT_RIGHT_MOST) {
            uint16_t x = (uint16_t)(s->x + STANDING_RIGHT);
            s->x = U16((x & BLOCK_POSITION_FLAG) - STANDING_RIGHT - 1);
            s->prev_x = s->x;
            collision = DETECT_NONE;
        }
        if (collision == DETECT_NONE) {
            mzm_sfx_play(MZM_SOUND_UNMORPHING);
            return MZM_POSE_UNMORPHING;
        }
    }
    if (s->timer) {
        s->timer--;
        if ((c->in->held & (MZM_KEY_RIGHT | MZM_KEY_LEFT | MZM_KEY_UP | MZM_KEY_DOWN)) == MZM_KEY_DOWN) {
            s->shinespark_timer = MZM_SHINESPARK_TIMER_MAX;
            s->timer = 0;
            s->screw_speed_flag = SCREW_SPEED_FLAG_STORING_SHINESPARK;
        }
    }
    return MZM_POSE_NONE;
}

/* SamusRolling */
static MzmPose pose_rolling(Ctx *c, const Physics *p) {
    MzmSamus *s = c->s;
    if (jump_pressed(c) && has(c, MZM_ITEM_HIGH_JUMP)) {
        s->forced = MZM_FORCED_JUMP;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (collision_above(c, STANDING_TOP) == DETECT_NONE && pressed(c, MZM_KEY_UP)) {
        mzm_sfx_play(MZM_SOUND_UNMORPHING);
        return MZM_POSE_UNMORPHING;
    }
    if (held(c, dir_key(s))) {
        int cap = p->x_velocity_cap;
        if (s->speedboosting) {
            cap = MZM_X_SPEEDBOOST_VELOCITY_CAP;
            s->shinespark_timer = 6;
        }
        apply_x_acceleration(s, p->x_acceleration, cap);
        return MZM_POSE_NONE;
    }
    if (held(c, opp_key(s))) s->turning = true;
    return MZM_POSE_MORPH_BALL;
}

/* SamusRollingGfx (the wet ground effect sprites are not ported). */
static MzmPose gfx_rolling(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) c->s->anim_frame = 0;
    return MZM_POSE_NONE;
}

/* SamusUnmorphing */
static MzmPose pose_unmorphing(Ctx *c) {
    if (collision_above(c, STANDING_TOP) == DETECT_NONE) {
        if (jump_pressed(c)) {
            c->s->forced = MZM_FORCED_JUMP;
            return MZM_POSE_MID_AIR_REQUEST;
        }
        if (!pressed(c, MZM_KEY_DOWN)) return MZM_POSE_NONE;
    }
    c->s->pose = MZM_POSE_MORPHING;
    return MZM_POSE_NONE;
}

/* SamusUnmorphingGfx */
static MzmPose gfx_unmorphing(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) {
        c->s->unmorph_palette_timer = 15;
        return MZM_POSE_CROUCHING;
    }
    return MZM_POSE_NONE;
}

/* SamusMorphballMidAir */
static MzmPose pose_morph_ball_midair(Ctx *c, const Physics *p) {
    MzmSamus *s = c->s;
    if (pressed(c, MZM_KEY_UP) && collision_above(c, STANDING_TOP) == DETECT_NONE) {
        mzm_sfx_play(MZM_SOUND_UNMORPHING);
        s->unmorph_palette_timer = 15;
        return MZM_POSE_MIDAIR;
    }
    if (s->forced == 0) {
        if (!held(c, MZM_KEY_A) && s->y_velocity > 0) s->y_velocity = 0;
    } else if (s->y_velocity < 8 - 1) {
        s->forced = 0;
    }
    if ((s->y_velocity >= 0 && s->x_velocity != 0) || held(c, dir_key(s))) {
        apply_x_acceleration(s, p->midair_x_acceleration, p->midair_morphed_x_velocity_cap);
    } else {
        if (held(c, opp_key(s))) flip(s);
        s->x_velocity = 0;
    }
    return MZM_POSE_NONE;
}

/* The two blocks above a hanging Samus: the one above the grabbed block
 * (blockAbove) and the one above her (sideBlock). `x_offset` points into
 * the grabbed block. */
static void hanging_blocks(const Ctx *c, int32_t x_offset, bool *block_above, bool *side_block) {
    const MzmSamus *s = c->s;
    int32_t y = s->y - (MZM_BLOCK_SIZE * 3 + MZM_QUARTER_BLOCK_SIZE);
    *block_above = clip(c, y, s->x + x_offset) & MZM_SAMUS_CLIP_SOLID_FLAG;
    *side_block = clip(c, y, s->x) & MZM_SAMUS_CLIP_SOLID_FLAG;
}

/* SamusHangingOnLedge */
static MzmPose pose_hanging_on_ledge(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->shake_x >= 30) {
        s->forced = MZM_FORCED_FALL;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    bool block_above, side_block;
    hanging_blocks(c, s->facing > 0 ? MZM_HALF_BLOCK_SIZE : -MZM_HALF_BLOCK_SIZE, &block_above,
                   &side_block);
    if (jump_pressed(c)) {
        if (held(c, dir_key(s))) {
            if (!block_above && !side_block) return MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING;
            if (has(c, MZM_ITEM_MORPH_BALL)) return MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL;
        }
        if (held(c, opp_key(s))) {
            flip(s);
            s->forced = MZM_FORCED_JUMP;
        } else if (held(c, MZM_KEY_DOWN)) {
            s->forced = MZM_FORCED_FALL;
        } else {
            s->forced = MZM_FORCED_CARRY;
            s->y_velocity = 8 * (MZM_QUARTER_BLOCK_SIZE + MZM_PIXEL_SIZE / 2);
        }
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (s->has_new_projectile) {
        flip(s);
        s->aim = held(c, MZM_KEY_DOWN) ? MZM_AIM_DOWN : MZM_AIM_UP;
        return MZM_POSE_SHOOTING_WHILE_HANGING;
    }
    if (!suitless(c) && c->in->charge_counter != 0) {
        flip(s);
        s->aim = held(c, MZM_KEY_DOWN) ? MZM_AIM_DOWN : MZM_AIM_UP;
        return MZM_POSE_AIMING_WHILE_HANGING;
    }
    if (held(c, MZM_KEY_L | MZM_KEY_UP | MZM_KEY_DOWN) || held(c, opp_key(s))) {
        flip(s);
        return MZM_POSE_TURNING_TO_AIM_WHILE_HANGING;
    }
    return MZM_POSE_NONE;
}

/* SamusHangingOnLedgeGfx */
static MzmPose gfx_hanging_on_ledge(Ctx *c) {
    if (update_animation(c, c->s->slowed) == ANIM_ENDED) c->s->anim_frame = 0;
    return MZM_POSE_NONE;
}

/* SamusTurningToAimWhileHanging (also hiding the arm cannon and shooting) */
static MzmPose pose_turning_to_aim_while_hanging(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->shake_x >= 30) {
        s->forced = MZM_FORCED_FALL;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    bool block_above, side_block;
    /* Samus faces away from the grabbed block. */
    hanging_blocks(c, s->facing < 0 ? MZM_HALF_BLOCK_SIZE : -MZM_HALF_BLOCK_SIZE, &block_above,
                   &side_block);
    if (jump_pressed(c)) {
        if (held(c, opp_key(s))) {
            if (!block_above && !side_block) {
                flip(s);
                return MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING;
            }
            if (has(c, MZM_ITEM_MORPH_BALL)) {
                flip(s);
                return MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL;
            }
        }
        s->forced = MZM_FORCED_JUMP;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    if (s->has_new_projectile) return MZM_POSE_SHOOTING_WHILE_HANGING;
    return MZM_POSE_NONE;
}

/* SamusTurningToAimWhileHangingGfx (also shooting while hanging) */
static MzmPose gfx_turning_to_aim_while_hanging(Ctx *c) {
    if (update_animation(c, c->s->slowed) == ANIM_ENDED) return MZM_POSE_AIMING_WHILE_HANGING;
    return MZM_POSE_NONE;
}

/* SamusHidingArmCannonWhileHangingGfx */
static MzmPose gfx_hiding_arm_cannon_while_hanging(Ctx *c) {
    if (update_animation(c, c->s->slowed) == ANIM_ENDED) {
        flip(c->s);
        return MZM_POSE_HANGING_ON_LEDGE;
    }
    return MZM_POSE_NONE;
}

/* SamusAimingWhileHanging: holding Up (without the outward direction) for
 * ten frames climbs the ledge, as A with the inward direction does. */
static MzmPose pose_aiming_while_hanging(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->shake_x >= 30) {
        s->forced = MZM_FORCED_FALL;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    aim_cannon(c);
    if (s->has_new_projectile) return MZM_POSE_SHOOTING_WHILE_HANGING;
    bool block_above, side_block;
    hanging_blocks(c, s->facing < 0 ? MZM_HALF_BLOCK_SIZE : -MZM_HALF_BLOCK_SIZE, &block_above,
                   &side_block);
    if (jump_pressed(c)) {
        if (held(c, opp_key(s))) {
            if (!block_above && !side_block) {
                flip(s);
                return MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING;
            }
            if (has(c, MZM_ITEM_MORPH_BALL)) {
                flip(s);
                return MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL;
            }
        }
        s->forced = held(c, dir_key(s)) ? MZM_FORCED_JUMP : MZM_FORCED_FALL;
        return MZM_POSE_MID_AIR_REQUEST;
    }
    bool aiming_up = false;
    if (!held(c, MZM_KEY_L)) {
        if (pressed(c, opp_key(s))) {
            if (suitless(c) || c->in->charge_counter == 0)
                return MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING;
            s->aim = MZM_AIM_UP;
        }
        if (held(c, MZM_KEY_UP) && !held(c, dir_key(s))) aiming_up = true;
    }
    if (aiming_up) {
        if (s->timer++ > 9) {
            if (!block_above && !side_block) {
                flip(s);
                return MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING;
            }
            if (has(c, MZM_ITEM_MORPH_BALL)) {
                flip(s);
                return MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL;
            }
            s->timer = 0;
        }
    } else {
        s->timer = 0;
    }
    return MZM_POSE_NONE;
}

/* SamusPullingSelfUp */
static MzmPose pose_pulling_self_up(Ctx *c) {
    MzmSamus *s = c->s;
    uint16_t offset = pulling_up_velocity[s->anim_frame < 4 ? s->anim_frame : 3];
    if (s->timer) offset /= 2;      /* the timer holds the slowed flag */
    s->y = U16(s->y - offset);
    return MZM_POSE_NONE;
}

/* SamusPullingSelfUpGfx */
static MzmPose gfx_pulling_self_up(Ctx *c) {
    MzmSamus *s = c->s;
    if (update_animation(c, s->timer) == ANIM_ENDED) {
        s->y = U16((s->y & BLOCK_POSITION_FLAG) - 1);
        return MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING;
    }
    return MZM_POSE_NONE;
}

/* SamusPullingSelfForward */
static MzmPose pose_pulling_self_forward(Ctx *c) {
    MzmSamus *s = c->s;
    s->x = U16(s->x + (s->facing > 0 ? MZM_PIXEL_SIZE : -MZM_PIXEL_SIZE));
    return MZM_POSE_NONE;
}

/* SamusPullingSelfForwardGfx */
static MzmPose gfx_pulling_self_forward(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED)
        return suitless(c) ? MZM_POSE_UNCROUCHING_SUITLESS : MZM_POSE_STANDING;
    return MZM_POSE_NONE;
}

/* SamusPullingSelfIntoMorphballTunnelGfx */
static MzmPose gfx_pulling_self_into_tunnel(Ctx *c) {
    MzmSamus *s = c->s;
    if (update_animation(c, s->timer) == ANIM_ENDED) {
        s->x = U16(s->x + (s->facing > 0 ? MZM_PIXEL_SIZE + MZM_PIXEL_SIZE / 2
                                         : -(MZM_PIXEL_SIZE + MZM_PIXEL_SIZE / 2)));
        s->y = U16((s->y & BLOCK_POSITION_FLAG) - 1);
        mzm_sfx_play(MZM_SOUND_MORPHING);
        return MZM_POSE_MORPH_BALL;
    }
    return MZM_POSE_NONE;
}

/* SamusUsingAnElevator */
static MzmPose pose_using_an_elevator(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->anim_frame != 0) return MZM_POSE_NONE;
    bool stop = false;
    s->y = U16(s->y - s->y_velocity);
    uint32_t current = affecting(c, s->y, s->x) >> 16;
    if (s->elevator_direction & MZM_KEY_UP) {
        uint32_t previous = affecting(c, s->prev_y, s->x) >> 16;
        if (current != MZM_SAMUS_MOVEMENT_ELEVATOR_DOWN_BLOCK &&
            previous == MZM_SAMUS_MOVEMENT_ELEVATOR_DOWN_BLOCK) {
            s->y = U16((int16_t)(s->y | SUB_PIXEL_POSITION_FLAG));
            stop = true;
        }
    } else if (s->elevator_direction & MZM_KEY_DOWN) {
        if (current == MZM_SAMUS_MOVEMENT_ELEVATOR_UP_BLOCK) {
            s->y = U16((int16_t)((s->y & BLOCK_POSITION_FLAG) - 1));
            stop = true;
        }
    }
    if (stop) {
        s->anim_frame++;
        s->anim_counter = 0;
        s->y_velocity = 0;
        mzm_sfx_fade(MZM_SOUND_ELEVATOR, 10);
    }
    return MZM_POSE_NONE;
}

/* SamusUsingAnElevatorGfx: sSamusAnimPointers_PowerSuit_UsingAnElevator. */
static MzmPose gfx_using_an_elevator(Ctx *c) {
    MzmSamus *s = c->s;
    uint8_t d[256];
    int count = durations(c, s->elevator_direction & MZM_KEY_DOWN
                                 ? "PowerSuit/UsingAnElevator/FALSE/right"
                                 : "PowerSuit/UsingAnElevator/TRUE/right", d);
    unsigned timer = s->anim_frame < count ? d[s->anim_frame] : 0;
    if (s->anim_counter >= timer) {
        if (s->anim_frame != 0) {
            s->anim_counter = 0;
            s->anim_frame++;
            timer = s->anim_frame < count ? d[s->anim_frame] : 0;
        }
        if (timer == 0) return MZM_POSE_FACING_THE_FOREGROUND;
    }
    return MZM_POSE_NONE;
}

/* SamusFacingTheForeground (and SamusFacingTheBackground with its pose) */
static MzmPose pose_facing(Ctx *c, MzmPose turning) {
    MzmSamus *s = c->s;
    uint16_t direction = c->in->held & (MZM_KEY_RIGHT | MZM_KEY_LEFT);
    if (direction != 0 && s->last_wall == 0) {
        s->facing = direction & MZM_KEY_RIGHT ? 1 : -1;
        return turning;
    }
    return MZM_POSE_NONE;
}

/* SamusTurningFromFacingForegroundGfx */
static MzmPose gfx_turning_from_facing_foreground(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->anim_counter >= 2) {
        if (s->elevator_direction != 0) return MZM_POSE_USING_AN_ELEVATOR;
        if (s->timer) return MZM_POSE_SAVING_LOADING_GAME;
        return suitless(c) ? MZM_POSE_UNCROUCHING_SUITLESS : MZM_POSE_STANDING;
    }
    return MZM_POSE_NONE;
}

/* SamusDelayBeforeShinesparkingGfx */
static MzmPose gfx_delay_before_shinesparking(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) {
        if (held(c, opp_key(c->s))) c->s->turning = true;
        return MZM_POSE_SHINESPARKING;
    }
    return MZM_POSE_NONE;
}

/* SamusShinesparking (also ballsparking) */
static MzmPose pose_shinesparking(Ctx *c) {
    MzmSamus *s = c->s;
    bool hitting_side = false, stop = false;
    if (s->forced == MZM_FORCED_UPWARDS_SHINESPARK) {
        if (s->y_velocity == 0) stop = true;
    } else if (s->forced != MZM_FORCED_SIDEWARDS_SHINESPARK) {
        if (s->y_velocity != 0) {
            if (s->x_velocity == 0) hitting_side = true;
        } else {
            stop = true;
        }
    } else if (s->x_velocity == 0) {
        hitting_side = true;
    }
    if (stop) {
        s->shake_y = 30;            /* ScreenShakeStartVertical(CONVERT_SECONDS(.5f), 1) */
        return (MzmPose)(s->pose + 1);
    }
    if (hitting_side) {
        int32_t x_offset = s->facing > 0 ? MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE
                                         : -(MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE);
        uint16_t nx, ny, nslope;
        if (collision_at(c, s->x + x_offset, s->y + 1, &nx, &ny, &nslope) != CLIP_AIR &&
            nslope != SLOPE_NONE) {
            /* A slope: the landing code carries the shinespark. */
            s->current_slope = nslope;
            s->y = ny;
            s->keep_vertical = true;
            return MZM_POSE_LANDING_REQUEST;
        }
        s->shake_x = 30;
        return (MzmPose)(s->pose + 1);
    }
    return MZM_POSE_NONE;
}

/* SamusShinesparkingGfx: sSamusAnimPointers_PowerSuit_Shinesparking. */
static MzmPose gfx_shinesparking(Ctx *c) {
    MzmSamus *s = c->s;
    const char *key;
    uint8_t loop;
    if (s->forced == MZM_FORCED_UPWARDS_SHINESPARK) {
        key = "PowerSuit/Shinesparking/FORCED_MOVEMENT_UPWARDS_SHINESPARK/right";
        loop = 1;
    } else {
        key = "PowerSuit/Shinesparking/FORCED_MOVEMENT_SIDEWARDS_SHINESPARK/right";
        loop = 2;
    }
    if (anim_step(c, key, false) == ANIM_ENDED) s->anim_frame = loop;
    return MZM_POSE_NONE;
}

/* SamusShinesparkCollisionGfx: CONVERT_SECONDS(.25f + 1.f / 60) */
static MzmPose gfx_shinespark_collision(Ctx *c) {
    return c->s->anim_counter > 16 ? MZM_POSE_DELAY_AFTER_SHINESPARKING : MZM_POSE_NONE;
}

/* SamusDelayAfterShinesparkingGfx */
static MzmPose gfx_delay_after_shinesparking(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_MID_AIR_REQUEST : MZM_POSE_NONE;
}

/* SamusDelayBeforeBallsparking */
static MzmPose pose_delay_before_ballsparking(Ctx *c) {
    if (held(c, opp_key(c->s))) flip(c->s);
    return MZM_POSE_NONE;
}

/* SamusDelayBeforeBallsparkingGfx */
static MzmPose gfx_delay_before_ballsparking(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->timer >= 60) return MZM_POSE_BALLSPARKING;
    if (s->timer == 0 && s->forced != MZM_FORCED_LAUNCHED_BY_CANNON)
        s->y = U16(s->y - MZM_HALF_BLOCK_SIZE);
    s->timer++;
    if (update_animation(c, false) == ANIM_ENDED) s->anim_frame = 0;
    return MZM_POSE_NONE;
}

/* SamusBallsparkingGfx */
static MzmPose gfx_ballsparking(Ctx *c) {
    return gfx_loop(c);
}

/* SamusBallsparkCollisionGfx */
static MzmPose gfx_ballspark_collision(Ctx *c) {
    return c->s->anim_counter > 16 ? MZM_POSE_MID_AIR_REQUEST : MZM_POSE_NONE;
}

/* SamusOnZipline (also shooting and turning on a zipline) */
static MzmPose pose_on_zipline(Ctx *c) {
    MzmSamus *s = c->s;
    if (jump_pressed(c)) return MZM_POSE_MID_AIR_REQUEST;
    if (s->has_new_projectile) return MZM_POSE_SHOOTING_ON_ZIPLINE;
    if (s->pose != MZM_POSE_ON_ZIPLINE) return MZM_POSE_NONE;
    if (!held(c, opp_key(s))) {
        aim_cannon(c);
        return MZM_POSE_NONE;
    }
    return MZM_POSE_TURNING_ON_ZIPLINE;
}

/* SamusShootingOnZiplineGfx */
static MzmPose gfx_shooting_on_zipline(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_ON_ZIPLINE : MZM_POSE_NONE;
}

/* SamusMorphballOnZipline */
static MzmPose pose_morph_ball_on_zipline(Ctx *c) {
    if (jump_pressed(c)) return MZM_POSE_MID_AIR_REQUEST;
    uint16_t direction = c->in->held & (MZM_KEY_RIGHT | MZM_KEY_LEFT);
    if (direction != 0) c->s->facing = direction & MZM_KEY_RIGHT ? 1 : -1;
    return MZM_POSE_NONE;
}

/* SamusSavingLoadingGame */
static MzmPose pose_saving_loading_game(Ctx *c) {
    if (c->s->timer != 0) {
        c->s->anim_frame = 0;
        c->s->anim_counter = 0;
    }
    return MZM_POSE_NONE;
}

/* SamusTurningAroundToDownloadMapDataGfx */
static MzmPose gfx_turning_around_to_download(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_DOWNLOADING_MAP_DATA : MZM_POSE_NONE;
}

/* SamusGettingHurt */
static MzmPose pose_getting_hurt(Ctx *c, Physics *p) {
    MzmSamus *s = c->s;
    uint8_t forced = 0;
    if (s->forced != 0 && (uint16_t)(s->y_velocity + 7) < 15 && s->last_wall++ < 10) {
        p->y_acceleration = 0;
        forced = 1;
    }
    if (forced == 0 && s->timer++ > 12 && s->y_velocity < -8 * (MZM_PIXEL_SIZE / 2)) {
        s->forced = forced;
        return s->pose == MZM_POSE_GETTING_HURT ? MZM_POSE_MIDAIR : MZM_POSE_MORPH_BALL_MIDAIR;
    }
    return MZM_POSE_NONE;
}

/* SamusGettingHurtGfx: alternates between the last frames. */
static MzmPose gfx_getting_hurt(Ctx *c) {
    if (update_animation(c, false) == ANIM_ENDED) c->s->anim_frame--;
    return MZM_POSE_NONE;
}

/* SamusGettingKnockedBack */
static MzmPose pose_getting_knocked_back(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->timer >= 13) {
        if (s->y_velocity >= -8 * MZM_PIXEL_SIZE) return MZM_POSE_NONE;
        return s->pose == MZM_POSE_GETTING_KNOCKED_BACK ? MZM_POSE_MIDAIR : MZM_POSE_MORPH_BALL_MIDAIR;
    }
    s->timer++;
    return MZM_POSE_NONE;
}

/* SamusDying: the music stops, the dying sound plays and Samus drifts to
 * the screen's centre (the fading and the game over are the game state's). */
static MzmPose pose_dying(Ctx *c) {
    MzmSamus *s = c->s;
    if (s->last_wall == 0) {
        s->last_wall++;
        mzm_sfx_stop_all();
    } else if (s->last_wall == 1) {
        s->last_wall++;
        mzm_sfx_play(MZM_SOUND_DYING);
    }
    s->invincibility = 64;
    if (s->x_velocity != 0 || s->y_velocity != 0) {
        uint32_t dst_x = (uint32_t)(s->camera_x + 120 * 4);
        uint32_t dst_y = (uint32_t)(s->camera_y + (80 + MZM_QUARTER_BLOCK_SIZE + MZM_PIXEL_SIZE) * 4);
        if (s->x_velocity > 0) {
            if ((uint32_t)s->x >= dst_x) s->x_velocity = 0;
        } else if (s->x_velocity < 0) {
            if ((uint32_t)s->x <= dst_x) s->x_velocity = 0;
        }
        if (s->y_velocity > 0) {
            if ((uint32_t)s->y >= dst_y) s->y_velocity = 0;
        } else if (s->y_velocity < 0) {
            if ((uint32_t)s->y <= dst_y) s->y_velocity = 0;
        }
        s->y = U16(s->y + s->y_velocity);
    }
    if (s->anim_frame > 23) {
        switch (s->timer++) {
        case 4:
        case 12:
        case 20:
        case 28:
        case 36:
        case 44:
        case 52:
        case 80:
            s->walljump_timer++;
            break;
        case 120:
            s->game_over = true;
            break;
        default:
            break;
        }
    }
    return MZM_POSE_NONE;
}

/* SamusCrouchingToCrawlGfx */
static MzmPose gfx_crouching_to_crawl(Ctx *c) {
    MzmSamus *s = c->s;
    if (update_animation(c, false) == ANIM_ENDED) {
        s->x = U16(s->x + (s->facing > 0 ? MZM_PIXEL_SIZE : -MZM_PIXEL_SIZE));
        return MZM_POSE_STARTING_TO_CRAWL;
    }
    return MZM_POSE_NONE;
}

/* SamusCrawlingStopped */
static MzmPose pose_crawling_stopped(Ctx *c) {
    MzmSamus *s = c->s;
    s->x_velocity = 0;
    if (collision_above(c, STANDING_TOP) == DETECT_NONE) return MZM_POSE_UNCROUCHING_FROM_CRAWLING;
    if (s->has_new_projectile) return MZM_POSE_SHOOTING_WHILE_CRAWLING;
    if (held(c, dir_key(s))) return MZM_POSE_CRAWLING;
    if (held(c, opp_key(s))) return MZM_POSE_TURNING_AROUND_WHILE_CRAWLING;
    return MZM_POSE_NONE;
}

/* SamusStartingToCrawlGfx (also shooting while crawling) */
static MzmPose gfx_starting_to_crawl(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_CRAWLING_STOPPED : MZM_POSE_NONE;
}

/* SamusCrawling */
static MzmPose pose_crawling(Ctx *c, const Physics *p) {
    MzmSamus *s = c->s;
    if (collision_above(c, STANDING_TOP) == DETECT_NONE) return MZM_POSE_UNCROUCHING_FROM_CRAWLING;
    if (s->has_new_projectile) return MZM_POSE_SHOOTING_WHILE_CRAWLING;
    if (held(c, dir_key(s))) {
        apply_x_acceleration(s, p->x_acceleration, MZM_HALF_BLOCK_SIZE);
        return MZM_POSE_NONE;
    }
    if (held(c, opp_key(s))) s->turning = true;
    return MZM_POSE_CRAWLING_STOPPED;
}

/* SamusCrawlingGfx */
static MzmPose gfx_crawling(Ctx *c) {
    MzmSamus *s = c->s;
    if (update_animation(c, false) == ANIM_ENDED) s->anim_frame = 0;
    if (s->anim_counter == 1 && s->anim_frame == 0) mzm_sfx_play(MZM_SOUND_SUITLESS_CRAWL);
    return MZM_POSE_NONE;
}

/* SamusTurningAroundWhileCrawling */
static MzmPose pose_turning_around_while_crawling(Ctx *c) {
    return c->s->has_new_projectile ? MZM_POSE_SHOOTING_WHILE_CRAWLING : MZM_POSE_NONE;
}

/* SamusTurningAroundWhileCrawlingGfx */
static MzmPose gfx_turning_around_while_crawling(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_STARTING_TO_CRAWL : MZM_POSE_NONE;
}

/* SamusGrabbingALedgeSuitlessGfx */
static MzmPose gfx_grabbing_a_ledge_suitless(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_HANGING_ON_LEDGE : MZM_POSE_NONE;
}

/* SamusTurningFromFacingTheBackgroundGfx (without BUGFIX) */
static MzmPose gfx_turning_from_facing_background(Ctx *c) {
    MzmSamus *s = c->s;
    if (update_animation(c, false) == ANIM_ENDED) {
        if (held(c, opp_key(s))) return MZM_POSE_RUNNING;
        if (s->last_wall != 0) return MZM_POSE_FACING_THE_BACKGROUND_SUITLESS;
        return suitless(c) ? MZM_POSE_UNCROUCHING_SUITLESS : MZM_POSE_STANDING;
    }
    return MZM_POSE_NONE;
}

/* SamusTurningToEnterEscapeShipGfx */
static MzmPose gfx_turning_to_enter_escape_ship(Ctx *c) {
    return update_animation(c, false) == ANIM_ENDED ? MZM_POSE_IN_ESCAPE_SHIP : MZM_POSE_NONE;
}

/* sSamusPoseFunctionPointers */
static MzmPose pose_handler(Ctx *c, Physics *p) {
    switch (c->s->pose) {
    case MZM_POSE_RUNNING: return pose_running(c, p);
    case MZM_POSE_STANDING:
    case MZM_POSE_SHOOTING:
    case MZM_POSE_LANDING:
    case MZM_POSE_UNCROUCHING_SUITLESS: return pose_standing(c);
    case MZM_POSE_TURNING_AROUND: return pose_turning_around(c);
    case MZM_POSE_CROUCHING:
    case MZM_POSE_SHOOTING_AND_CROUCHING:
    case MZM_POSE_CROUCHING_SUITLESS: return pose_crouching(c);
    case MZM_POSE_TURNING_AROUND_AND_CROUCHING: return pose_turning_around_and_crouching(c);
    case MZM_POSE_SKIDDING: return pose_skidding(c);
    case MZM_POSE_MIDAIR: return pose_midair(c, p);
    case MZM_POSE_TURNING_AROUND_MIDAIR: return pose_turning_around_midair(c);
    case MZM_POSE_STARTING_SPIN_JUMP:
    case MZM_POSE_SPINNING:
    case MZM_POSE_SPACE_JUMPING:
    case MZM_POSE_SCREW_ATTACKING: return pose_spinning(c, p);
    case MZM_POSE_STARTING_WALL_JUMP: return pose_starting_wall_jump(c);
    case MZM_POSE_MORPHING: return pose_morphing(c);
    case MZM_POSE_MORPH_BALL: return pose_morph_ball(c);
    case MZM_POSE_ROLLING: return pose_rolling(c, p);
    case MZM_POSE_UNMORPHING: return pose_unmorphing(c);
    case MZM_POSE_MORPH_BALL_MIDAIR: return pose_morph_ball_midair(c, p);
    case MZM_POSE_HANGING_ON_LEDGE:
    case MZM_POSE_GRABBING_A_LEDGE_SUITLESS: return pose_hanging_on_ledge(c);
    case MZM_POSE_TURNING_TO_AIM_WHILE_HANGING:
    case MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING:
    case MZM_POSE_SHOOTING_WHILE_HANGING: return pose_turning_to_aim_while_hanging(c);
    case MZM_POSE_AIMING_WHILE_HANGING: return pose_aiming_while_hanging(c);
    case MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING:
    case MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL: return pose_pulling_self_up(c);
    case MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING: return pose_pulling_self_forward(c);
    case MZM_POSE_USING_AN_ELEVATOR: return pose_using_an_elevator(c);
    case MZM_POSE_FACING_THE_FOREGROUND:
        return pose_facing(c, MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND);
    case MZM_POSE_SHINESPARKING:
    case MZM_POSE_BALLSPARKING: return pose_shinesparking(c);
    case MZM_POSE_DELAY_BEFORE_BALLSPARKING: return pose_delay_before_ballsparking(c);
    case MZM_POSE_ON_ZIPLINE:
    case MZM_POSE_SHOOTING_ON_ZIPLINE:
    case MZM_POSE_TURNING_ON_ZIPLINE: return pose_on_zipline(c);
    case MZM_POSE_MORPH_BALL_ON_ZIPLINE: return pose_morph_ball_on_zipline(c);
    case MZM_POSE_SAVING_LOADING_GAME:
    case MZM_POSE_DOWNLOADING_MAP_DATA: return pose_saving_loading_game(c);
    case MZM_POSE_GETTING_HURT:
    case MZM_POSE_GETTING_HURT_IN_MORPH_BALL: return pose_getting_hurt(c, p);
    case MZM_POSE_GETTING_KNOCKED_BACK:
    case MZM_POSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL: return pose_getting_knocked_back(c);
    case MZM_POSE_DYING: return pose_dying(c);
    case MZM_POSE_CRAWLING_STOPPED:
    case MZM_POSE_STARTING_TO_CRAWL: return pose_crawling_stopped(c);
    case MZM_POSE_CRAWLING: return pose_crawling(c, p);
    case MZM_POSE_TURNING_AROUND_WHILE_CRAWLING: return pose_turning_around_while_crawling(c);
    case MZM_POSE_FACING_THE_BACKGROUND_SUITLESS:
        return pose_facing(c, MZM_POSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS);
    default: return MZM_POSE_NONE;  /* SamusInactivity */
    }
}

/* sSamusPoseGfxFunctionPointers */
static MzmPose pose_graphics(Ctx *c) {
    switch (c->s->pose) {
    case MZM_POSE_RUNNING: return gfx_running(c);
    case MZM_POSE_STANDING:
    case MZM_POSE_CROUCHING:
    case MZM_POSE_MORPH_BALL:
    case MZM_POSE_MORPH_BALL_MIDAIR:
    case MZM_POSE_AIMING_WHILE_HANGING:
    case MZM_POSE_FACING_THE_FOREGROUND:
    case MZM_POSE_GRABBED_BY_CHOZO_STATUE:
    case MZM_POSE_MORPH_BALL_ON_ZIPLINE:
    case MZM_POSE_SAVING_LOADING_GAME:
    case MZM_POSE_DOWNLOADING_MAP_DATA:
    case MZM_POSE_CRAWLING_STOPPED:
    case MZM_POSE_ACTIVATING_ZIPLINES: return gfx_loop(c);
    case MZM_POSE_TURNING_AROUND: return gfx_turning_around(c);
    case MZM_POSE_SHOOTING:
    case MZM_POSE_LANDING:
    case MZM_POSE_UNCROUCHING_SUITLESS: return gfx_shooting(c);
    case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
    case MZM_POSE_UNCROUCHING_FROM_CRAWLING: return gfx_turning_around_and_crouching(c);
    case MZM_POSE_SHOOTING_AND_CROUCHING:
    case MZM_POSE_CROUCHING_SUITLESS: return gfx_shooting_and_crouching(c);
    case MZM_POSE_SKIDDING:
    case MZM_POSE_GETTING_HURT:
    case MZM_POSE_GETTING_KNOCKED_BACK:
    case MZM_POSE_DYING: return gfx_getting_hurt(c);
    case MZM_POSE_MIDAIR: return gfx_midair(c);
    case MZM_POSE_TURNING_AROUND_MIDAIR: return gfx_turning_around_midair(c);
    case MZM_POSE_STARTING_SPIN_JUMP: return gfx_starting_spin_jump(c);
    case MZM_POSE_SPINNING: return gfx_spinning(c);
    case MZM_POSE_STARTING_WALL_JUMP: return gfx_starting_wall_jump(c);
    case MZM_POSE_SPACE_JUMPING: return gfx_space_jumping(c);
    case MZM_POSE_SCREW_ATTACKING: return gfx_screw_attacking(c);
    case MZM_POSE_MORPHING: return gfx_morphing(c);
    case MZM_POSE_ROLLING: return gfx_rolling(c);
    case MZM_POSE_UNMORPHING: return gfx_unmorphing(c);
    case MZM_POSE_HANGING_ON_LEDGE: return gfx_hanging_on_ledge(c);
    case MZM_POSE_TURNING_TO_AIM_WHILE_HANGING:
    case MZM_POSE_SHOOTING_WHILE_HANGING: return gfx_turning_to_aim_while_hanging(c);
    case MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING: return gfx_hiding_arm_cannon_while_hanging(c);
    case MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING: return gfx_pulling_self_up(c);
    case MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING: return gfx_pulling_self_forward(c);
    case MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL: return gfx_pulling_self_into_tunnel(c);
    case MZM_POSE_USING_AN_ELEVATOR: return gfx_using_an_elevator(c);
    case MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND: return gfx_turning_from_facing_foreground(c);
    case MZM_POSE_DELAY_BEFORE_SHINESPARKING: return gfx_delay_before_shinesparking(c);
    case MZM_POSE_SHINESPARKING: return gfx_shinesparking(c);
    case MZM_POSE_SHINESPARK_COLLISION: return gfx_shinespark_collision(c);
    case MZM_POSE_DELAY_AFTER_SHINESPARKING: return gfx_delay_after_shinesparking(c);
    case MZM_POSE_DELAY_BEFORE_BALLSPARKING: return gfx_delay_before_ballsparking(c);
    case MZM_POSE_BALLSPARKING: return gfx_ballsparking(c);
    case MZM_POSE_BALLSPARK_COLLISION: return gfx_ballspark_collision(c);
    case MZM_POSE_SHOOTING_ON_ZIPLINE:
    case MZM_POSE_TURNING_ON_ZIPLINE: return gfx_shooting_on_zipline(c);
    case MZM_POSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA: return gfx_turning_around_to_download(c);
    case MZM_POSE_CROUCHING_TO_CRAWL: return gfx_crouching_to_crawl(c);
    case MZM_POSE_STARTING_TO_CRAWL:
    case MZM_POSE_SHOOTING_WHILE_CRAWLING: return gfx_starting_to_crawl(c);
    case MZM_POSE_CRAWLING: return gfx_crawling(c);
    case MZM_POSE_TURNING_AROUND_WHILE_CRAWLING: return gfx_turning_around_while_crawling(c);
    case MZM_POSE_GRABBING_A_LEDGE_SUITLESS: return gfx_grabbing_a_ledge_suitless(c);
    case MZM_POSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS:
        return gfx_turning_from_facing_background(c);
    case MZM_POSE_TURNING_TO_ENTER_ESCAPE_SHIP: return gfx_turning_to_enter_escape_ship(c);
    default: return MZM_POSE_NONE;  /* SamusInactivity */
    }
}

/* SamusTakeHazardDamage: true when Samus dies or the cold knocks her
 * back. */
static bool take_hazard_damage(Ctx *c, MzmEquipment *equipment) {
    MzmSamus *s = c->s;
    int32_t y;
    switch (s->pose) {
    case MZM_POSE_GRABBED_BY_CHOZO_STATUE:
    case MZM_POSE_DYING:
        return false;
    case MZM_POSE_HANGING_ON_LEDGE:
    case MZM_POSE_TURNING_TO_AIM_WHILE_HANGING:
    case MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING:
    case MZM_POSE_AIMING_WHILE_HANGING:
    case MZM_POSE_SHOOTING_WHILE_HANGING:
    case MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING:
    case MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING:
    case MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL:
        y = s->y - MZM_QUARTER_BLOCK_SIZE;
        break;
    default:
        y = s->y;
        break;
    }
    s->hazard_damage_timer++;
    bool damaged = false, knockback = false;
    enum { NONE, LIQUID, ROOM = 15 } type = NONE;
    uint8_t hazard = affecting(c, y, s->x) & 0xFF;
    uint8_t timer = s->hazard_damage_timer;
    if (has(c, MZM_ITEM_GRAVITY_SUIT)) {
        if (hazard == MZM_HAZARD_STRONG_ACID) {
            damaged = true;
            if (timer > 3) type = LIQUID;
        }
    } else if (has(c, MZM_ITEM_VARIA_SUIT)) {
        if (hazard == MZM_HAZARD_STRONG_ACID) {
            damaged = true;
            if (timer > 1) type = LIQUID;
        } else if (hazard == MZM_HAZARD_LAVA) {
            damaged = true;
            if (timer > 4) type = LIQUID;
        }
    } else if (hazard == MZM_HAZARD_STRONG_ACID) {
        damaged = true;
        type = LIQUID;
    } else if (hazard == MZM_HAZARD_COLD_KNOCKBACK) {
        damaged = true;
        if (timer > 3) type = ROOM;
        if (s->hazard_knockback_timer++ > 87) {
            s->hazard_knockback_timer = 0;
            knockback = true;
        }
    } else if (hazard == MZM_HAZARD_LAVA) {
        damaged = true;
        if (timer > 2) type = LIQUID;
    } else if (hazard == MZM_HAZARD_WEAK_ACID) {
        damaged = true;
        if (timer > 7) type = LIQUID;
    } else if (hazard == MZM_HAZARD_HEAT || hazard == MZM_HAZARD_COLD) {
        damaged = true;
        if (timer > 5) type = ROOM;
    }
    if (!damaged) {
        s->hazard_damage_timer = 0;
        s->hazard_knockback_timer = 0;
        s->hazard_palette_timer = 0;
    } else {
        switch (s->hazard_palette_timer++) {
        case 1:
        case 33:
            if (hazard == MZM_HAZARD_STRONG_ACID || hazard == MZM_HAZARD_LAVA ||
                hazard == MZM_HAZARD_WEAK_ACID)
                mzm_sfx_play(MZM_SOUND_LIQUID_DAMAGE_SUBMERGED);
            break;
        case 64:
            s->hazard_palette_timer = 0;
            break;
        default:
            break;
        }
        if (type != NONE) {
            if (equipment && equipment->energy > 0) equipment->energy--;
            s->hazard_damage_timer = 0;
        }
        if (type == ROOM) mzm_sfx_play(MZM_SOUND_ROOM_DAMAGE);
    }
    return (equipment && equipment->energy == 0) || knockback;
}

/* SamusExecutePoseHandler. The weapon cooldown, SamusSetHighlightedWeapon
 * and SamusCheckNewProjectile belong to the weapons (mzm_projectiles.c),
 * which run before and hand over hasNewProjectile, weaponHighlighted and
 * chargeCounter through the input. */
static MzmPose execute_pose_handler(Ctx *c, Physics *p, MzmEquipment *equipment) {
    MzmSamus *s = c->s;
    if (take_hazard_damage(c, equipment)) {
        /* Dead (SamusChangeToHurtPose's dying branch) or knocked back by
         * the cold (a hurt request with energy left). */
        c->lethal = !equipment || equipment->energy == 0;
        return MZM_POSE_HURT_REQUEST;
    }
    /* The speed booster charge sound of a stored shinespark. */
    if (s->shinespark_timer != 0 && s->pose != MZM_POSE_DELAY_BEFORE_SHINESPARKING &&
        s->pose != MZM_POSE_DELAY_BEFORE_BALLSPARKING) {
        if (s->shinespark_timer % 16 == 4) mzm_sfx_play(MZM_SOUND_SPEEDBOOSTER_CHARGE);
        s->shinespark_timer--;
    }
    set_spinning_pose(c);
    /* SamusCheckFireBeamMissile / SamusCheckFirePistol: firing without an
     * aim aims forward. */
    s->has_new_projectile = c->in->new_projectile;
    if (s->has_new_projectile && s->aim == MZM_AIM_NONE) s->aim = MZM_AIM_FORWARD;
    MzmPose pose = pose_handler(c, p);
    if (pose == MZM_POSE_NONE) pose = pose_graphics(c);
    check_shinesparking(s);
    return pose;
}

/* SamusUpdateVelocityPosition */
static void update_velocity_position(Ctx *c, const Physics *p) {
    MzmSamus *s = c->s;
    s->hitbox_type = mzm_pose_hitbox(s->pose);
    int32_t velocity;
    switch (s->pose) {
    case MZM_POSE_MIDAIR:
    case MZM_POSE_TURNING_AROUND_MIDAIR:
    case MZM_POSE_SPINNING:
    case MZM_POSE_STARTING_SPIN_JUMP:
    case MZM_POSE_SPACE_JUMPING:
    case MZM_POSE_SCREW_ATTACKING:
    case MZM_POSE_MORPH_BALL_MIDAIR:
    case MZM_POSE_GETTING_HURT:
    case MZM_POSE_GETTING_KNOCKED_BACK:
    case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
    case MZM_POSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL:
        /* DIV_SHIFT: arithmetic shifts. */
        if (s->y_velocity > p->positive_y_cap) velocity = p->positive_y_cap >> 3;
        else if (s->y_velocity < -p->negative_y_cap) velocity = -p->negative_y_cap >> 3;
        else velocity = s->y_velocity >> 3;
        if (s->y_velocity >= -231) s->y_velocity = (int16_t)(s->y_velocity - p->y_acceleration);
        s->y = U16(s->y - velocity);
        break;
    case MZM_POSE_SHINESPARKING:
    case MZM_POSE_BALLSPARKING:
        if (!(s->forced & MZM_FORCED_SIDEWARDS_SHINESPARK)) s->y = U16(s->y - (s->y_velocity >> 3));
        break;
    default:
        break;
    }
    if (s->standing == MZM_STANDING_GROUND) {
        velocity = velocity_on_slope(s) >> 3;
        if (s->pose == MZM_POSE_RUNNING) {
            if (s->facing > 0) {
                if (velocity < 0) velocity = 0;
            } else if (velocity > 0) {
                velocity = 0;
            }
        }
    } else {
        velocity = s->x_velocity >> 3;
    }
    s->x = U16(s->x + velocity);
}

/* SamusUpdateDrawDistanceAndStandingStatus */
static void update_standing(MzmSamus *s) {
    if (s->standing != MZM_STANDING_ENEMY) s->standing = mzm_pose_standing(s->pose);
}

/* SamusUpdateHitboxMovingDirection */
static void update_hitbox_moving_direction(Ctx *c) {
    MzmSamus *s = c->s;
    s->touching_side = 0;
    s->touching_top = 0;
    s->h_moving = HDMOVING_NONE;
    s->v_moving = VDMOVING_NONE;
    if (s->x > s->prev_x) s->h_moving = HDMOVING_RIGHT;
    else if (s->x < s->prev_x) s->h_moving = HDMOVING_LEFT;
    if (!s->keep_vertical) {
        if (s->y > s->prev_y) s->v_moving = VDMOVING_DOWN;
        else if (s->y < s->prev_y) s->v_moving = VDMOVING_UP;
    }
    MzmStandingStatus standing = mzm_pose_standing(s->pose);
    if (standing == MZM_STANDING_NOT_IN_CONTROL) s->v_moving = VDMOVING_DOWN;
    check_collisions(c, standing);
    update_standing(s);
}

/* Native positions inside the controller. */
static void enter(MzmSamus *s) {
    s->y = U16(s->y - 1);
    s->copy.y = U16(s->copy_y - 1);
    if (s->on_enemy) s->standing = MZM_STANDING_ENEMY;
    else if (s->standing == MZM_STANDING_ENEMY) s->standing = mzm_pose_standing(s->pose);
}

static void leave(MzmSamus *s) {
    s->copy_y = s->copy.y + 1;
    s->y = s->y + 1;
    s->on_enemy = s->standing == MZM_STANDING_ENEMY;
}

void mzm_samus_update(MzmSamus *samus, const MzmInput *input,
                      MzmEquipment *equipment,
                      const MzmCollision *collision,
                      const MzmAnimationSource *animation) {
    Ctx c = {samus, input ? input : &no_input, equipment ? equipment : &no_equipment, collision,
             animation, false};
    enter(samus);
    samus->grabbed_ledge = false;
    /* gPreviousXPosition / gPreviousYPosition */
    samus->prev_x = samus->x;
    samus->prev_y = samus->y;
    /* SamusUpdate */
    samus->anim_counter++;
    if (samus->invincibility) samus->invincibility--;
    if (samus->unmorph_palette_timer) samus->unmorph_palette_timer--;
    Physics p = update_physics(&c);
    MzmPose pose = execute_pose_handler(&c, &p, equipment);
    if (pose != MZM_POSE_NONE) set_pose(&c, pose);
    update_standing(samus);
    update_velocity_position(&c, &p);
    update_hitbox_moving_direction(&c);
    /* ScreenShakeUpdateHorizontal / Vertical */
    if (samus->shake_x) samus->shake_x--;
    if (samus->shake_y) samus->shake_y--;
    leave(samus);
}

void mzm_samus_hurt(MzmSamus *samus, const MzmCollision *collision, bool lethal) {
    Ctx c = {samus, &no_input, &no_equipment, collision, NULL, lethal};
    enter(samus);
    set_pose(&c, MZM_POSE_HURT_REQUEST);
    leave(samus);
}

void mzm_samus_knockback(MzmSamus *samus, const MzmCollision *collision) {
    Ctx c = {samus, &no_input, &no_equipment, collision, NULL, false};
    enter(samus);
    set_pose(&c, MZM_POSE_KNOCKBACK_REQUEST);
    leave(samus);
}

bool mzm_samus_bomb_bounce_allowed(const MzmSamus *samus, int direction) {
    if (samus->slowed) return false;
    if (direction % MZM_BOMB_JUMP_ABOVE < MZM_FORCED_BOMB_JUMP_RIGHT) return false;
    switch (samus->pose) {
    case MZM_POSE_MORPHING:
    case MZM_POSE_MORPH_BALL:
    case MZM_POSE_ROLLING:
        return true;
    case MZM_POSE_MORPH_BALL_MIDAIR:
        return samus->y_velocity <= 0 && !(direction & MZM_BOMB_JUMP_ABOVE);
    default:
        return false;
    }
}

void mzm_samus_chozo_grab(MzmSamus *samus, bool grabbed, const MzmEquipment *equipment,
                          const MzmCollision *collision) {
    Ctx c = {samus, &no_input, equipment ? equipment : &no_equipment, collision, NULL, false};
    enter(samus);
    set_pose(&c, grabbed ? MZM_POSE_GRABBED_BY_CHOZO_STATUE : MZM_POSE_MORPH_BALL);
    leave(samus);
}

void mzm_samus_sprite_set_pose(MzmSamus *samus, MzmPose pose, const MzmEquipment *equipment,
                               const MzmCollision *collision) {
    Ctx c = {samus, &no_input, equipment ? equipment : &no_equipment, collision, NULL, false};
    enter(samus);
    set_pose(&c, pose);
    leave(samus);
}

void mzm_samus_land_on_sprite(MzmSamus *samus, const MzmInput *input,
                              const MzmEquipment *equipment, const MzmCollision *collision) {
    Ctx c = {samus, input ? input : &no_input, equipment ? equipment : &no_equipment, collision,
             NULL, false};
    enter(samus);
    set_pose(&c, MZM_POSE_LANDING_REQUEST);
    leave(samus);
}

/* SamusBombBounce */
bool mzm_samus_bomb_bounce(MzmSamus *samus, const MzmEquipment *equipment,
                           const MzmCollision *collision, int direction) {
    if (!mzm_samus_bomb_bounce_allowed(samus, direction)) return false;
    Ctx c = {samus, &no_input, equipment ? equipment : &no_equipment, collision, NULL, false};
    enter(samus);
    samus->forced = (uint8_t)(direction % MZM_BOMB_JUMP_ABOVE);
    set_pose(&c, MZM_POSE_MID_AIR_REQUEST);
    leave(samus);
    return true;
}

bool mzm_samus_armed_graphics(uint8_t weapon_highlighted) {
    return (weapon_highlighted & (MZM_WH_MISSILE | MZM_WH_SUPER_MISSILE)) != 0;
}

static const char *const acd_names[] = {
    "ACD_FORWARD", "ACD_DIAGONALLY_UP", "ACD_DIAGONALLY_DOWN", "ACD_UP", "ACD_DOWN", "ACD_NONE",
};

/* SamusUpdateGraphicsOam's body animation pointer. */
void mzm_samus_graphics_key(const MzmSamus *samus, const MzmEquipment *equipment,
                            uint8_t weapon_highlighted, char *key, size_t size) {
    const MzmEquipment *eq = equipment ? equipment : &no_equipment;
    const char *family = body_family(eq);
    /* The tables with no suitless version: the full suit's. */
    const char *suited = eq->suit == MZM_SUIT_NORMAL ? "PowerSuit" : "FullSuit";
    const char *side = samus->facing > 0 ? "right" : "left";
    MzmPose pose = samus->pose;
    unsigned acd = samus->aim <= MZM_AIM_NONE ? samus->aim : MZM_AIM_FORWARD;
    const char *table = NULL, *selector = acd_names[acd];
    if (pose == MZM_POSE_DOWNLOADING_MAP_DATA && samus->timer != 0) pose = MZM_POSE_STANDING;
    switch (pose) {
    case MZM_POSE_RUNNING:
        /* ACD_NONE - 2 */
        table = eq->suit != MZM_SUIT_SUITLESS && samus->speedboosting ? "Running_Speedboosting"
                                                                       : "Running";
        break;
    case MZM_POSE_STANDING: table = "Standing"; break;
    case MZM_POSE_TURNING_AROUND: table = "TurningAround"; break;
    case MZM_POSE_SHOOTING: table = "Shooting"; break;
    case MZM_POSE_CROUCHING: table = "Crouching"; break;
    case MZM_POSE_TURNING_AROUND_AND_CROUCHING: table = "TurningAroundAndCrouching"; break;
    case MZM_POSE_SHOOTING_AND_CROUCHING: table = "ShootingAndCrouching"; break;
    case MZM_POSE_MIDAIR: table = "MidAir"; break;
    case MZM_POSE_TURNING_AROUND_MIDAIR: table = "TurningAroundMidAir"; break;
    case MZM_POSE_LANDING: table = "Landing"; break;
    case MZM_POSE_AIMING_WHILE_HANGING: table = "AimingWhileHanging"; break;
    case MZM_POSE_SHOOTING_WHILE_HANGING: table = "ShootingWhileHanging"; break;
    case MZM_POSE_SKIDDING:
        family = suited;
        table = "Skidding";
        selector = mzm_samus_armed_graphics(weapon_highlighted) ? "TRUE" : "FALSE";
        break;
    case MZM_POSE_SCREW_ATTACKING:
        family = suited;
        table = "ScrewAttacking";
        selector = eq->items & MZM_ITEM_SPACE_JUMP ? "TRUE" : "FALSE";
        break;
    case MZM_POSE_USING_AN_ELEVATOR:
        family = suited;
        table = "UsingAnElevator";
        selector = acd + (samus->elevator_direction & MZM_KEY_UP ? 1 : 0) ? "TRUE" : "FALSE";
        break;
    case MZM_POSE_SHINESPARKING:
    case MZM_POSE_SHINESPARK_COLLISION:
        family = suited;
        table = "Shinesparking";
        selector = samus->forced == MZM_FORCED_SIDEWARDS_SHINESPARK ? "FORCED_MOVEMENT_SIDEWARDS_SHINESPARK" :
                   samus->forced == MZM_FORCED_DIAGONAL_SHINESPARK ? "FORCED_MOVEMENT_DIAGONAL_SHINESPARK" :
                   "FORCED_MOVEMENT_UPWARDS_SHINESPARK";
        break;
    case MZM_POSE_ON_ZIPLINE: family = suited; table = "OnZipline"; break;
    case MZM_POSE_SHOOTING_ON_ZIPLINE: family = suited; table = "ShootingOnZipline"; break;
    case MZM_POSE_TURNING_ON_ZIPLINE: family = suited; table = "TurningOnZipline"; break;
    case MZM_POSE_CRAWLING_STOPPED:
        family = "Suitless";
        table = "CrawlingStopped";
        selector = samus->forced ? "TRUE" : "FALSE";
        break;
    case MZM_POSE_UNCROUCHING_SUITLESS: family = "Suitless"; table = "UncrouchingSuitless"; break;
    case MZM_POSE_CROUCHING_SUITLESS: family = "Suitless"; table = "CrouchingSuitless"; break;
    default:
        /* sSamusAnimPointers_*[pose] (the saving pose's alternate frames,
         * sSamusAnim_256484 and its kin, are not exported). */
        break;
    }
    if (table) snprintf(key, size, "%s/%s/%s/%s", family, table, selector, side);
    else snprintf(key, size, "%s/pose/%s/%s", family, mzm_pose_name(pose), side);
}
