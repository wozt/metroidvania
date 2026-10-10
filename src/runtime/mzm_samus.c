/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Samus pose controller. Function names in comments refer to
 * the pinned mzm decompilation that each routine reproduces. */
#include "mzm_samus.h"

#include <stddef.h>

static const MzmBlockHitbox block_hitboxes[] = {
    /* sSamusBlockHitboxData */
    [MZM_HITBOX_STANDING] = {-(MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                             -(MZM_BLOCK_SIZE * 2 - MZM_PIXEL_SIZE),
                             MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE},
    [MZM_HITBOX_CROUCHED] = {-(MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                             -(MZM_BLOCK_SIZE + MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                             MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE},
    [MZM_HITBOX_MORPHED] = {-(MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE),
                            -(MZM_BLOCK_SIZE - MZM_PIXEL_SIZE),
                            MZM_HALF_BLOCK_SIZE - MZM_PIXEL_SIZE},
};

typedef struct {
    MzmHitboxType hitbox;
    MzmStandingStatus standing;
    const char *name;
} PoseInfo;

/* sSamusCollisionData rows for the implemented poses. */
static const PoseInfo pose_info[MZM_POSE_COUNT] = {
    [MZM_POSE_STANDING] = {MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "standing"},
    [MZM_POSE_RUNNING] = {MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "running"},
    [MZM_POSE_TURNING_AROUND] = {MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "turning"},
    [MZM_POSE_SHOOTING] = {MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "shooting"},
    [MZM_POSE_CROUCHING] = {MZM_HITBOX_CROUCHED, MZM_STANDING_GROUND, "crouching"},
    [MZM_POSE_TURNING_AROUND_AND_CROUCHING] = {MZM_HITBOX_CROUCHED,
        MZM_STANDING_GROUND, "turning-crouched"},
    [MZM_POSE_SHOOTING_AND_CROUCHING] = {MZM_HITBOX_CROUCHED,
        MZM_STANDING_GROUND, "shooting-crouched"},
    [MZM_POSE_MIDAIR] = {MZM_HITBOX_STANDING, MZM_STANDING_MIDAIR, "midair"},
    [MZM_POSE_TURNING_AROUND_MIDAIR] = {MZM_HITBOX_STANDING,
        MZM_STANDING_MIDAIR, "turning-midair"},
    [MZM_POSE_LANDING] = {MZM_HITBOX_STANDING, MZM_STANDING_GROUND, "landing"},
    [MZM_POSE_STARTING_SPIN_JUMP] = {MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR,
        "starting-spin"},
    [MZM_POSE_SPINNING] = {MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR, "spinning"},
    [MZM_POSE_STARTING_WALL_JUMP] = {MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR,
        "starting-wall-jump"},
    [MZM_POSE_SPACE_JUMPING] = {MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR,
        "space-jumping"},
    [MZM_POSE_SCREW_ATTACKING] = {MZM_HITBOX_CROUCHED, MZM_STANDING_MIDAIR,
        "screw-attacking"},
    [MZM_POSE_MORPHING] = {MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "morphing"},
    [MZM_POSE_MORPH_BALL] = {MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "morph-ball"},
    [MZM_POSE_ROLLING] = {MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "rolling"},
    [MZM_POSE_UNMORPHING] = {MZM_HITBOX_MORPHED, MZM_STANDING_GROUND, "unmorphing"},
    [MZM_POSE_MORPH_BALL_MIDAIR] = {MZM_HITBOX_MORPHED, MZM_STANDING_MIDAIR,
        "morph-ball-midair"},
    [MZM_POSE_HANGING_ON_LEDGE] = {MZM_HITBOX_STANDING, MZM_STANDING_HANGING,
        "hanging"},
    [MZM_POSE_PULLING_UP] = {MZM_HITBOX_STANDING, MZM_STANDING_FORCED,
        "pulling-up"},
    [MZM_POSE_PULLING_FORWARD] = {MZM_HITBOX_STANDING, MZM_STANDING_GROUND,
        "pulling-forward"},
    [MZM_POSE_GETTING_HURT] = {MZM_HITBOX_STANDING, MZM_STANDING_MIDAIR, "hurt"},
    [MZM_POSE_GETTING_HURT_IN_MORPH_BALL] = {MZM_HITBOX_MORPHED,
        MZM_STANDING_MIDAIR, "hurt-morphed"},
    [MZM_POSE_DYING] = {MZM_HITBOX_STANDING, MZM_STANDING_FORCED, "dying"},
};

/* sSamusPullingSelfUpVelocity, subpixels per frame for each animation frame. */
static const uint8_t pulling_up_velocity[4] = {
    MZM_PIXEL_SIZE * 6, MZM_PIXEL_SIZE * 2, MZM_PIXEL_SIZE * 1, 0
};

const MzmBlockHitbox *mzm_block_hitbox(MzmHitboxType type) {
    return &block_hitboxes[type];
}

MzmHitboxType mzm_pose_hitbox(MzmPose pose) {
    return pose < MZM_POSE_COUNT ? pose_info[pose].hitbox : MZM_HITBOX_STANDING;
}

MzmStandingStatus mzm_pose_standing(MzmPose pose) {
    return pose < MZM_POSE_COUNT ? pose_info[pose].standing : MZM_STANDING_FORCED;
}

const char *mzm_pose_name(MzmPose pose) {
    return pose < MZM_POSE_COUNT ? pose_info[pose].name : "unknown";
}

bool mzm_pose_is_spinning(MzmPose pose) {
    return pose == MZM_POSE_STARTING_SPIN_JUMP || pose == MZM_POSE_SPINNING ||
           pose == MZM_POSE_SPACE_JUMPING || pose == MZM_POSE_SCREW_ATTACKING;
}

bool mzm_pose_is_morphed(MzmPose pose) {
    return mzm_pose_hitbox(pose) == MZM_HITBOX_MORPHED;
}

/* SamusApplyXAcceleration */
int16_t mzm_apply_x_acceleration(int16_t velocity, int facing,
                                 int acceleration, int cap) {
    int value = velocity;
    if (facing > 0) {
        value += acceleration;
        if (value > cap) value = cap;
    } else {
        value -= acceleration;
        if (value < -cap) value = -cap;
    }
    return (int16_t)value;
}

/* Y half of SamusUpdateVelocityPosition with normal (non-liquid) physics. */
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

void mzm_samus_init(MzmSamus *samus, int32_t x, int32_t y, int facing) {
    *samus = (MzmSamus){0};
    samus->pose = MZM_POSE_STANDING;
    samus->x = x;
    samus->y = y;
    samus->facing = facing < 0 ? -1 : 1;
}

static void hitbox_box(MzmHitboxType type, int32_t x, int32_t y,
                       float *bx, float *by, float *bw, float *bh) {
    const MzmBlockHitbox *h = &block_hitboxes[type];
    *bx = (float)(x + h->left) / (float)MZM_SUBPIXELS_PER_PIXEL;
    *by = (float)(y + h->top) / (float)MZM_SUBPIXELS_PER_PIXEL;
    *bw = (float)(h->right - h->left) / (float)MZM_SUBPIXELS_PER_PIXEL;
    *bh = (float)(-h->top) / (float)MZM_SUBPIXELS_PER_PIXEL;
}

void mzm_samus_box(const MzmSamus *samus, float *x, float *y,
                   float *w, float *h) {
    hitbox_box(mzm_pose_hitbox(samus->pose), samus->x, samus->y, x, y, w, h);
}

static bool box_blocked(const MzmCollision *c, MzmHitboxType type,
                        int32_t x, int32_t y) {
    float bx, by, bw, bh;
    hitbox_box(type, x, y, &bx, &by, &bw, &bh);
    return c->blocked(c->context, bx, by, bw, bh);
}

static bool box_near_slope(const MzmCollision *c, MzmHitboxType type,
                           int32_t x, int32_t y) {
    if (!c->slope_near) return false;
    float bx, by, bw, bh;
    hitbox_box(type, x, y, &bx, &by, &bw, &bh);
    return c->slope_near(c->context, bx, by, bw, bh);
}

/* ClipdataProcessForSamus solid test at one subpixel position. */
static bool point_solid(const MzmCollision *c, int32_t x, int32_t y) {
    if (c->solid_point) return c->solid_point(c->context, x, y, 0);
    return c->blocked(c->context, (float)(x >> 2), (float)(y >> 2), 1.f, 1.f);
}

/* Standing-height clearance (SamusCheckCollisionAbove with the standing
 * block hitbox top). The original's corner nudges are not reproduced. */
static bool standing_clear(const MzmCollision *c, const MzmSamus *s) {
    return !box_blocked(c, MZM_HITBOX_STANDING, s->x, s->y);
}

typedef struct {
    MzmSamus *s;
    const MzmInput *in;
    const MzmEquipment *eq;
    const MzmCollision *c;
    const MzmAnimationSource *a;
} Ctx;

enum { NEXT_NONE = -1, REQUEST_MIDAIR = MZM_POSE_COUNT, REQUEST_LANDING };

static bool held(const Ctx *x, uint16_t keys) { return (x->in->held & keys) != 0; }
static bool pressed(const Ctx *x, uint16_t keys) { return (x->in->pressed & keys) != 0; }
static uint16_t forward_key(const MzmSamus *s) {
    return s->facing > 0 ? MZM_KEY_RIGHT : MZM_KEY_LEFT;
}
static uint16_t backward_key(const MzmSamus *s) {
    return s->facing > 0 ? MZM_KEY_LEFT : MZM_KEY_RIGHT;
}

typedef enum { ANIM_NONE, ANIM_SUB_ENDED, ANIM_ENDED } AnimState;

/* SamusUpdateAnimation */
static AnimState update_animation(Ctx *x) {
    MzmSamus *s = x->s;
    uint8_t durations[256];
    int count = x->a && x->a->durations ?
        x->a->durations(x->a->context, s, durations, 256) : 0;
    if (count <= 0) {
        s->anim_frame = 0;
        s->anim_counter = 0;
        return ANIM_ENDED;
    }
    if (s->anim_frame >= count) s->anim_frame = (uint8_t)(count - 1);
    if (s->anim_counter >= durations[s->anim_frame]) {
        s->anim_counter = 0;
        s->anim_frame++;
        return s->anim_frame >= count ? ANIM_ENDED : ANIM_SUB_ENDED;
    }
    return ANIM_NONE;
}

static void loop_animation(Ctx *x) {
    if (update_animation(x) == ANIM_ENDED) x->s->anim_frame = 0;
}

/* SamusAimCannon, reduced to the aim directions the registry exposes. */
static void aim_cannon(Ctx *x, bool allow_down) {
    MzmSamus *s = x->s;
    if (held(x, MZM_KEY_L)) {
        if (pressed(x, MZM_KEY_DOWN)) s->aim = MZM_AIM_DIAGONAL_DOWN;
        else if (pressed(x, MZM_KEY_UP) || (s->aim != MZM_AIM_DIAGONAL_UP &&
                                            s->aim != MZM_AIM_DIAGONAL_DOWN))
            s->aim = MZM_AIM_DIAGONAL_UP;
    } else if (held(x, MZM_KEY_UP)) {
        s->aim = MZM_AIM_UP;
    } else if (allow_down && held(x, MZM_KEY_DOWN)) {
        s->aim = MZM_AIM_DOWN;
    } else {
        s->aim = MZM_AIM_FORWARD;
    }
}

/* SamusCopyData: snapshot, apply a pending turn and reset transient fields. */
static MzmSamus copy_data(MzmSamus *s) {
    MzmSamus copy = *s;
    if (s->turning) {
        s->facing = -s->facing;
        s->turning = false;
    }
    s->aim = MZM_AIM_FORWARD;
    s->forced = MZM_FORCED_NONE;
    s->walljump_timer = 0;
    s->timer = 0;
    s->last_wall = 0;
    s->x_velocity = 0;
    s->y_velocity = 0;
    s->anim_counter = 0;
    s->anim_frame = 0;
    return copy;
}

/* Keep a resized hitbox out of a ceiling when a taller midair pose starts. */
static void settle_midair_hitbox(Ctx *x) {
    MzmSamus *s = x->s;
    MzmHitboxType type = mzm_pose_hitbox(s->pose);
    for (int step = 0; step < MZM_HALF_BLOCK_SIZE &&
         box_blocked(x->c, type, s->x, s->y); ++step) s->y++;
}

/* SamusSetPose default branch plus SamusCheckCarryFromCopy. */
static void set_pose(Ctx *x, MzmPose pose) {
    MzmSamus *s = x->s;
    MzmSamus copy = copy_data(s);
    s->pose = pose;
    switch (pose) {
        case MZM_POSE_RUNNING:
            s->aim = held(x, MZM_KEY_L) ? copy.aim : MZM_AIM_FORWARD;
            break;
        case MZM_POSE_STANDING:
            s->aim = copy.aim;
            if (copy.pose == MZM_POSE_CROUCHING ||
                copy.pose == MZM_POSE_SHOOTING_AND_CROUCHING) s->timer = 6;
            break;
        case MZM_POSE_CROUCHING:
            s->aim = copy.aim;
            s->anim_frame = 1;
            if (s->aim != MZM_AIM_FORWARD && s->aim != MZM_AIM_DIAGONAL_UP &&
                s->aim != MZM_AIM_DIAGONAL_DOWN) s->aim = MZM_AIM_FORWARD;
            break;
        case MZM_POSE_HANGING_ON_LEDGE: {
            /* Align the feet below the grabbed block. The runtime's feet sit
             * exactly on block boundaries, one subpixel lower than native. */
            int32_t native = s->y - 1;
            int32_t block = native & ~(MZM_BLOCK_SIZE - 1);
            if ((native & (MZM_BLOCK_SIZE - 1)) < (MZM_BLOCK_SIZE - 1) / 2)
                s->y = block + MZM_EIGHTH_BLOCK_SIZE + 1;
            else
                s->y = block + MZM_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE + 1;
            s->grabbed_ledge = true;
            break;
        }
        case MZM_POSE_TURNING_AROUND_MIDAIR:
            s->y_velocity = copy.y_velocity;
            s->turning = true;
            s->aim = copy.aim;
            break;
        case MZM_POSE_TURNING_AROUND:
        case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
            s->turning = true;
            s->aim = copy.aim;
            break;
        default:
            s->aim = copy.aim;
            break;
    }
    if (mzm_pose_standing(pose) == MZM_STANDING_MIDAIR) settle_midair_hitbox(x);
}

/* SamusSetMidAir */
static void request_midair(Ctx *x) {
    MzmSamus *s = x->s;
    MzmSamus copy = copy_data(s);
    s->x_velocity = copy.x_velocity;
    s->aim = copy.aim;
    switch (copy.pose) {
        case MZM_POSE_RUNNING:
            if (copy.forced != MZM_FORCED_JUMP) {
                s->pose = MZM_POSE_MIDAIR;
                break;
            }
            s->pose = MZM_POSE_STARTING_SPIN_JUMP;
            s->y_velocity = mzm_jump_velocity(x->eq);
            break;
        case MZM_POSE_STARTING_SPIN_JUMP:
        case MZM_POSE_SPINNING:
        case MZM_POSE_SPACE_JUMPING:
        case MZM_POSE_SCREW_ATTACKING:
            /* Spin break: cancel horizontal momentum. */
            s->pose = MZM_POSE_MIDAIR;
            s->x_velocity = 0;
            if (copy.forced == MZM_FORCED_CARRY) s->y_velocity = copy.y_velocity;
            if (!standing_clear(x->c, s)) s->y += MZM_HALF_BLOCK_SIZE;
            break;
        case MZM_POSE_STARTING_WALL_JUMP:
            if (copy.forced == MZM_FORCED_FALL) {
                s->pose = MZM_POSE_MIDAIR;
                break;
            }
            s->pose = MZM_POSE_SPINNING;
            if (copy.forced == MZM_FORCED_JUMP)
                s->y_velocity = (x->eq->items & MZM_ITEM_HIGH_JUMP) ?
                    MZM_HIGH_JUMP_VELOCITY : MZM_LOW_JUMP_VELOCITY;
            break;
        case MZM_POSE_MORPH_BALL_MIDAIR:
            s->pose = MZM_POSE_MORPH_BALL_MIDAIR;
            break;
        case MZM_POSE_MORPH_BALL:
        case MZM_POSE_ROLLING:
            s->anim_frame = copy.anim_frame;
            s->anim_counter = copy.anim_counter;
            /* fall through */
        case MZM_POSE_MORPHING:
            s->x_velocity = (int16_t)(s->x_velocity >> 1);
            if (copy.forced == MZM_FORCED_JUMP)
                s->y_velocity = MZM_MORPH_BALL_JUMP_VELOCITY;
            s->pose = MZM_POSE_MORPH_BALL_MIDAIR;
            break;
        case MZM_POSE_CROUCHING:
        case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
        case MZM_POSE_SHOOTING_AND_CROUCHING:
            if (!standing_clear(x->c, s)) s->y += MZM_HALF_BLOCK_SIZE;
            /* fall through */
        default:
            s->pose = MZM_POSE_MIDAIR;
            if (copy.forced == MZM_FORCED_JUMP) {
                if (held(x, MZM_KEY_RIGHT | MZM_KEY_LEFT))
                    s->pose = MZM_POSE_STARTING_SPIN_JUMP;
                s->y_velocity = mzm_jump_velocity(x->eq);
            } else if (copy.forced == MZM_FORCED_CARRY) {
                s->y_velocity = copy.y_velocity;
            }
            break;
    }
    settle_midair_hitbox(x);
}

/* SamusSetLandingPose */
static void request_landing(Ctx *x) {
    MzmSamus *s = x->s;
    MzmSamus copy = copy_data(s);
    switch (copy.pose) {
        case MZM_POSE_MORPH_BALL_MIDAIR:
            if (held(x, MZM_KEY_A) && (x->eq->items & MZM_ITEM_HIGH_JUMP)) {
                if (standing_clear(x->c, s))
                    s->forced = MZM_FORCED_BOUNCE_BEFORE_JUMP;
            } else if (copy.y_velocity < -(8 * (MZM_QUARTER_BLOCK_SIZE +
                                                MZM_EIGHTH_BLOCK_SIZE))) {
                /* Morph Ball bounce after a fast fall; the pose stays midair. */
                s->forced = MZM_FORCED_BOUNCE_AFTER_FALL;
                s->y_velocity = 50;
                break;
            }
            /* fall through */
        case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
            s->pose = MZM_POSE_MORPH_BALL;
            break;
        default:
            if (!standing_clear(x->c, s)) s->pose = MZM_POSE_CROUCHING;
            else if (copy.x_velocity == 0) s->pose = MZM_POSE_LANDING;
            else s->pose = MZM_POSE_STANDING;
            break;
    }
    s->aim = copy.aim;
    if ((s->pose == MZM_POSE_LANDING || s->pose == MZM_POSE_STANDING) &&
        copy.aim == MZM_AIM_DOWN) s->aim = MZM_AIM_DIAGONAL_DOWN;
}

static void apply_next(Ctx *x, int next) {
    if (next == NEXT_NONE) return;
    if (next == REQUEST_MIDAIR) request_midair(x);
    else if (next == REQUEST_LANDING) request_landing(x);
    else set_pose(x, (MzmPose)next);
}

/* SamusSetSpinningPose */
static void set_spinning_pose(Ctx *x) {
    MzmSamus *s = x->s;
    bool space = (x->eq->items & MZM_ITEM_SPACE_JUMP) != 0;
    bool screw = (x->eq->items & MZM_ITEM_SCREW_ATTACK) != 0;
    switch (s->pose) {
        case MZM_POSE_SPINNING:
            if (screw) s->pose = MZM_POSE_SCREW_ATTACKING;
            else if (space) s->pose = MZM_POSE_SPACE_JUMPING;
            break;
        case MZM_POSE_SPACE_JUMPING:
            if (screw) {
                s->pose = MZM_POSE_SCREW_ATTACKING;
            } else if (!space) {
                s->pose = MZM_POSE_SPINNING;
                s->anim_frame = 0;
            }
            break;
        case MZM_POSE_SCREW_ATTACKING:
            if (!screw) {
                s->pose = space ? MZM_POSE_SPACE_JUMPING : MZM_POSE_SPINNING;
                s->anim_frame = 0;
            }
            break;
        default:
            break;
    }
}

static int jump_request(Ctx *x) {
    x->s->forced = MZM_FORCED_JUMP;
    return REQUEST_MIDAIR;
}

/* SamusStanding, also used by the shooting and landing poses. */
static int pose_standing(Ctx *x) {
    MzmSamus *s = x->s;
    if (pressed(x, MZM_KEY_A)) return jump_request(x);
    if (held(x, forward_key(s))) return MZM_POSE_RUNNING;
    if (x->in->new_projectile) return MZM_POSE_SHOOTING;
    if (held(x, backward_key(s))) return MZM_POSE_TURNING_AROUND;
    if (pressed(x, MZM_KEY_DOWN) &&
        (!held(x, MZM_KEY_L) || s->aim == MZM_AIM_DIAGONAL_DOWN)) {
        if (x->in->new_projectile) return MZM_POSE_SHOOTING_AND_CROUCHING;
        return MZM_POSE_CROUCHING;
    }
    if (s->timer) s->timer--;
    aim_cannon(x, false);
    return NEXT_NONE;
}

/* SamusRunning without the Speed Booster charge. */
static int pose_running(Ctx *x) {
    MzmSamus *s = x->s;
    if (pressed(x, MZM_KEY_A)) return jump_request(x);
    if (held(x, forward_key(s))) {
        s->x_velocity = mzm_apply_x_acceleration(
            s->x_velocity, s->facing, MZM_X_ACCELERATION, MZM_X_VELOCITY_CAP);
        aim_cannon(x, false);
        return NEXT_NONE;
    }
    if (x->in->new_projectile) return MZM_POSE_SHOOTING;
    if (!held(x, backward_key(s))) return MZM_POSE_STANDING;
    return MZM_POSE_TURNING_AROUND;
}

/* SamusTurningAround */
static int pose_turning(Ctx *x) {
    MzmSamus *s = x->s;
    if (pressed(x, MZM_KEY_A)) return jump_request(x);
    if (pressed(x, MZM_KEY_DOWN) &&
        (!held(x, MZM_KEY_L) || s->aim == MZM_AIM_DIAGONAL_DOWN))
        s->pose = MZM_POSE_TURNING_AROUND_AND_CROUCHING;
    if (x->in->new_projectile)
        return s->pose == MZM_POSE_TURNING_AROUND_AND_CROUCHING ?
            MZM_POSE_SHOOTING_AND_CROUCHING : MZM_POSE_SHOOTING;
    return NEXT_NONE;
}

/* SamusCrouching, also used for crouched turning and shooting here. */
static int pose_crouching(Ctx *x) {
    MzmSamus *s = x->s;
    bool clear = standing_clear(x->c, s);
    if (pressed(x, MZM_KEY_A) && clear) return jump_request(x);
    if (pressed(x, MZM_KEY_UP) && clear &&
        (!held(x, MZM_KEY_L) || s->aim == MZM_AIM_DIAGONAL_UP))
        return MZM_POSE_STANDING;
    if (pressed(x, MZM_KEY_DOWN) && (x->eq->items & MZM_ITEM_MORPH_BALL) &&
        (!held(x, MZM_KEY_L) || s->aim == MZM_AIM_DIAGONAL_DOWN))
        return MZM_POSE_MORPHING;
    if (s->pose != MZM_POSE_CROUCHING) return NEXT_NONE;
    aim_cannon(x, false);
    if (s->aim == MZM_AIM_UP) s->aim = MZM_AIM_FORWARD;
    if (x->in->new_projectile) return MZM_POSE_SHOOTING_AND_CROUCHING;
    if (held(x, backward_key(s))) return MZM_POSE_TURNING_AROUND_AND_CROUCHING;
    if (held(x, forward_key(s))) {
        if (clear && s->timer++ > 5) return MZM_POSE_STANDING;
    } else {
        s->timer = 0;
    }
    return NEXT_NONE;
}

/* SamusMidAir */
static int pose_midair(Ctx *x) {
    MzmSamus *s = x->s;
    if (pressed(x, MZM_KEY_DOWN) && s->aim == MZM_AIM_DOWN &&
        (x->eq->items & MZM_ITEM_MORPH_BALL)) {
        /* The original returns this pose through SamusSetPose. */
        return MZM_POSE_MORPH_BALL_MIDAIR;
    }
    if (pressed(x, MZM_KEY_A) && !held(x, MZM_KEY_UP | MZM_KEY_DOWN)) {
        s->pose = MZM_POSE_SPINNING;
        s->anim_frame = 0;
        s->anim_counter = 0;
        return NEXT_NONE;
    }
    bool turning = false;
    if (held(x, forward_key(s))) {
        s->x_velocity = mzm_apply_x_acceleration(
            s->x_velocity, s->facing, MZM_X_MIDAIR_ACCELERATION,
            MZM_X_MIDAIR_VELOCITY_CAP);
    } else if (held(x, backward_key(s))) {
        turning = true;
    } else if (s->x_velocity > 0) {
        s->x_velocity = (int16_t)(s->x_velocity - MZM_X_MIDAIR_ACCELERATION);
        if (s->x_velocity < 0) s->x_velocity = 0;
    } else if (s->x_velocity < 0) {
        s->x_velocity = (int16_t)(s->x_velocity + MZM_X_MIDAIR_ACCELERATION);
        if (s->x_velocity > 0) s->x_velocity = 0;
    }
    aim_cannon(x, true);
    if (turning) return MZM_POSE_TURNING_AROUND_MIDAIR;
    if (!held(x, MZM_KEY_A) && s->y_velocity > 0) s->y_velocity = 0;
    return NEXT_NONE;
}

/* SamusTurningAroundMidAir */
static int pose_turning_midair(Ctx *x) {
    MzmSamus *s = x->s;
    if (pressed(x, MZM_KEY_A) && !held(x, MZM_KEY_UP | MZM_KEY_DOWN)) {
        s->pose = MZM_POSE_SPINNING;
        s->facing = -s->facing;
        s->anim_frame = 0;
        s->anim_counter = 0;
        s->turning = false;
        return NEXT_NONE;
    }
    if (x->in->new_projectile) {
        s->forced = MZM_FORCED_CARRY;
        return REQUEST_MIDAIR;
    }
    if (!held(x, MZM_KEY_A) && s->y_velocity > 0) s->y_velocity = 0;
    return NEXT_NONE;
}

/* SamusSpinning, shared by the starting, Space Jump and Screw Attack poses. */
static int pose_spinning(Ctx *x) {
    MzmSamus *s = x->s;
    if (x->in->new_projectile) {
        s->forced = MZM_FORCED_FALL;
        return REQUEST_MIDAIR;
    }
    if (!held(x, MZM_KEY_RIGHT | MZM_KEY_LEFT) &&
        held(x, MZM_KEY_UP | MZM_KEY_DOWN)) {
        s->forced = MZM_FORCED_CARRY;
        return REQUEST_MIDAIR;
    }
    aim_cannon(x, true);
    int acceleration = MZM_X_MIDAIR_ACCELERATION;
    if (x->eq->items & MZM_ITEM_SPACE_JUMP) {
        if (pressed(x, MZM_KEY_A) && s->y_velocity <= -8 * MZM_EIGHTH_BLOCK_SIZE) {
            s->y_velocity = (x->eq->items & MZM_ITEM_HIGH_JUMP) ?
                MZM_HIGH_JUMP_VELOCITY : MZM_LOW_JUMP_VELOCITY;
            return NEXT_NONE;
        }
    } else if (s->walljump_timer) {
        s->walljump_timer--;
        if (s->facing == s->last_wall) {
            if (pressed(x, MZM_KEY_A)) {
                int32_t offset = s->last_wall > 0 ?
                    -(MZM_HALF_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE) :
                    MZM_HALF_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE;
                if (point_solid(x->c, s->x + offset, s->y - 1)) {
                    s->facing = s->last_wall;
                    return MZM_POSE_STARTING_WALL_JUMP;
                }
            }
            acceleration = 1;
        }
    }
    if (held(x, backward_key(s))) {
        s->facing = -s->facing;
        s->x_velocity = 0;
    } else {
        s->x_velocity = mzm_apply_x_acceleration(
            s->x_velocity, s->facing, acceleration, MZM_X_MIDAIR_VELOCITY_CAP);
    }
    if (!held(x, MZM_KEY_A) && s->y_velocity > 0) s->y_velocity = 0;
    return NEXT_NONE;
}

/* SamusStartingWallJump */
static int pose_starting_wall_jump(Ctx *x) {
    if (x->in->new_projectile ||
        (!held(x, MZM_KEY_RIGHT | MZM_KEY_LEFT) &&
         held(x, MZM_KEY_UP | MZM_KEY_DOWN))) {
        x->s->forced = MZM_FORCED_FALL;
        return REQUEST_MIDAIR;
    }
    return NEXT_NONE;
}

/* SamusMorphing */
static int pose_morphing(Ctx *x) {
    if (pressed(x, MZM_KEY_UP)) x->s->pose = MZM_POSE_UNMORPHING;
    return NEXT_NONE;
}

/* SamusMorphball without bombs or ballspark storage. */
static int pose_morph_ball(Ctx *x) {
    MzmSamus *s = x->s;
    if (s->forced == MZM_FORCED_BOUNCE_BEFORE_JUMP) {
        /* The original counts this field up twice before jumping. */
        if (s->timer > 1) {
            s->forced = MZM_FORCED_JUMP;
            return REQUEST_MIDAIR;
        }
        s->timer++;
        return NEXT_NONE;
    }
    if (pressed(x, MZM_KEY_A)) {
        s->forced = MZM_FORCED_JUMP;
        if (x->eq->items & MZM_ITEM_HIGH_JUMP) return REQUEST_MIDAIR;
        s->forced = MZM_FORCED_NONE;
    }
    if (held(x, MZM_KEY_RIGHT | MZM_KEY_LEFT)) {
        s->facing = held(x, MZM_KEY_RIGHT) ? 1 : -1;
        return MZM_POSE_ROLLING;
    }
    if (pressed(x, MZM_KEY_UP) && standing_clear(x->c, s))
        return MZM_POSE_UNMORPHING;
    return NEXT_NONE;
}

/* SamusRolling */
static int pose_rolling(Ctx *x) {
    MzmSamus *s = x->s;
    if (pressed(x, MZM_KEY_A) && (x->eq->items & MZM_ITEM_HIGH_JUMP))
        return jump_request(x);
    if (standing_clear(x->c, s) && pressed(x, MZM_KEY_UP))
        return MZM_POSE_UNMORPHING;
    if (held(x, forward_key(s))) {
        s->x_velocity = mzm_apply_x_acceleration(
            s->x_velocity, s->facing, MZM_X_ACCELERATION, MZM_X_VELOCITY_CAP);
        return NEXT_NONE;
    }
    if (held(x, backward_key(s))) s->turning = true;
    return MZM_POSE_MORPH_BALL;
}

/* SamusUnmorphing */
static int pose_unmorphing(Ctx *x) {
    if (standing_clear(x->c, x->s)) {
        if (pressed(x, MZM_KEY_A)) return jump_request(x);
        if (!pressed(x, MZM_KEY_DOWN)) return NEXT_NONE;
    }
    x->s->pose = MZM_POSE_MORPHING;
    return NEXT_NONE;
}

/* SamusMorphballMidAir */
static int pose_morph_midair(Ctx *x) {
    MzmSamus *s = x->s;
    if (pressed(x, MZM_KEY_UP) && standing_clear(x->c, s)) {
        s->unmorph_palette_timer = 15;
        return MZM_POSE_MIDAIR;
    }
    if (s->forced == MZM_FORCED_NONE) {
        if (!held(x, MZM_KEY_A) && s->y_velocity > 0) s->y_velocity = 0;
    } else if (s->y_velocity < 8 - 1) {
        s->forced = MZM_FORCED_NONE;
    }
    if ((s->y_velocity >= 0 && s->x_velocity != 0) || held(x, forward_key(s))) {
        s->x_velocity = mzm_apply_x_acceleration(
            s->x_velocity, s->facing, MZM_X_MIDAIR_ACCELERATION,
            MZM_X_MIDAIR_MORPHED_VELOCITY_CAP);
    } else {
        if (held(x, backward_key(s))) s->facing = -s->facing;
        s->x_velocity = 0;
    }
    return NEXT_NONE;
}

/* SamusHangingOnLedge without aiming or the Morph Ball tunnel pull. */
static int pose_hanging(Ctx *x) {
    MzmSamus *s = x->s;
    if (!pressed(x, MZM_KEY_A)) return NEXT_NONE;
    int32_t grabbed_x = s->x + s->facing * MZM_HALF_BLOCK_SIZE;
    int32_t probe_y = s->y - 1 - (MZM_BLOCK_SIZE * 3 + MZM_QUARTER_BLOCK_SIZE);
    bool block_above = point_solid(x->c, grabbed_x, probe_y);
    bool side_block = point_solid(x->c, s->x, probe_y);
    if (held(x, forward_key(s)) && !block_above && !side_block)
        return MZM_POSE_PULLING_UP;
    if (held(x, backward_key(s))) {
        s->facing = -s->facing;
        s->forced = MZM_FORCED_JUMP;
    } else if (held(x, MZM_KEY_DOWN)) {
        s->forced = MZM_FORCED_FALL;
    } else {
        s->forced = MZM_FORCED_CARRY;
        s->y_velocity = 8 * (MZM_QUARTER_BLOCK_SIZE + MZM_PIXEL_SIZE / 2);
    }
    return REQUEST_MIDAIR;
}

/* SamusPullingSelfUp */
static int pose_pulling_up(Ctx *x) {
    MzmSamus *s = x->s;
    s->y -= pulling_up_velocity[s->anim_frame < 4 ? s->anim_frame : 3];
    return NEXT_NONE;
}

/* SamusPullingSelfForward; refuses to enter verified solid geometry. */
static int pose_pulling_forward(Ctx *x) {
    MzmSamus *s = x->s;
    int32_t next = s->x + s->facing * MZM_PIXEL_SIZE;
    if (!box_blocked(x->c, MZM_HITBOX_STANDING, next, s->y)) s->x = next;
    return NEXT_NONE;
}

/* SamusGettingHurt */
static int pose_hurt(Ctx *x) {
    MzmSamus *s = x->s;
    if (s->timer++ > 12 && s->y_velocity < -8 * (MZM_PIXEL_SIZE / 2))
        return s->pose == MZM_POSE_GETTING_HURT ?
            MZM_POSE_MIDAIR : MZM_POSE_MORPH_BALL_MIDAIR;
    return NEXT_NONE;
}

static int pose_handler(Ctx *x) {
    switch (x->s->pose) {
        case MZM_POSE_STANDING:
        case MZM_POSE_SHOOTING:
        case MZM_POSE_LANDING:
            return pose_standing(x);
        case MZM_POSE_RUNNING: return pose_running(x);
        case MZM_POSE_TURNING_AROUND: return pose_turning(x);
        case MZM_POSE_CROUCHING:
        case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
        case MZM_POSE_SHOOTING_AND_CROUCHING:
            return pose_crouching(x);
        case MZM_POSE_MIDAIR: return pose_midair(x);
        case MZM_POSE_TURNING_AROUND_MIDAIR: return pose_turning_midair(x);
        case MZM_POSE_STARTING_SPIN_JUMP:
        case MZM_POSE_SPINNING:
        case MZM_POSE_SPACE_JUMPING:
        case MZM_POSE_SCREW_ATTACKING:
            return pose_spinning(x);
        case MZM_POSE_STARTING_WALL_JUMP: return pose_starting_wall_jump(x);
        case MZM_POSE_MORPHING: return pose_morphing(x);
        case MZM_POSE_MORPH_BALL: return pose_morph_ball(x);
        case MZM_POSE_ROLLING: return pose_rolling(x);
        case MZM_POSE_UNMORPHING: return pose_unmorphing(x);
        case MZM_POSE_MORPH_BALL_MIDAIR: return pose_morph_midair(x);
        case MZM_POSE_HANGING_ON_LEDGE: return pose_hanging(x);
        case MZM_POSE_PULLING_UP: return pose_pulling_up(x);
        case MZM_POSE_PULLING_FORWARD: return pose_pulling_forward(x);
        case MZM_POSE_GETTING_HURT:
        case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
            return pose_hurt(x);
        case MZM_POSE_DYING:
            x->s->x_velocity = 0;
            x->s->y_velocity = 0;
            return NEXT_NONE;
        case MZM_POSE_COUNT: break;
    }
    return NEXT_NONE;
}

/* The *Gfx pose loops: advance the animation and resolve transitions. */
static int pose_graphics(Ctx *x) {
    MzmSamus *s = x->s;
    switch (s->pose) {
        case MZM_POSE_TURNING_AROUND:
            if (update_animation(x) == ANIM_ENDED)
                return held(x, backward_key(s)) ?
                    MZM_POSE_RUNNING : MZM_POSE_STANDING;
            return NEXT_NONE;
        case MZM_POSE_TURNING_AROUND_AND_CROUCHING:
        case MZM_POSE_SHOOTING_AND_CROUCHING:
            if (update_animation(x) == ANIM_ENDED) return MZM_POSE_CROUCHING;
            return NEXT_NONE;
        case MZM_POSE_SHOOTING:
        case MZM_POSE_LANDING:
            if (update_animation(x) == ANIM_ENDED) return MZM_POSE_STANDING;
            return NEXT_NONE;
        case MZM_POSE_MIDAIR: {
            /* SamusMidAirGfx: rising loops the first legs, falling starts at
             * frame 2, and the final frame is held. */
            if (s->y_velocity >= 0) {
                if (s->anim_frame == 2) s->anim_counter = 0;
            } else if (s->anim_frame < 2) {
                s->anim_frame = 2;
            }
            if (update_animation(x) == ANIM_ENDED) s->anim_frame = 4;
            return NEXT_NONE;
        }
        case MZM_POSE_TURNING_AROUND_MIDAIR:
            if (update_animation(x) == ANIM_ENDED) {
                s->forced = MZM_FORCED_CARRY;
                return REQUEST_MIDAIR;
            }
            return NEXT_NONE;
        case MZM_POSE_STARTING_SPIN_JUMP:
            if (update_animation(x) == ANIM_ENDED) {
                s->pose = MZM_POSE_SPINNING;
                s->anim_frame = 0;
            }
            return NEXT_NONE;
        case MZM_POSE_STARTING_WALL_JUMP:
            if (update_animation(x) == ANIM_ENDED) {
                s->forced = MZM_FORCED_JUMP;
                return REQUEST_MIDAIR;
            }
            return NEXT_NONE;
        case MZM_POSE_MORPHING:
            if (update_animation(x) == ANIM_ENDED) return MZM_POSE_MORPH_BALL;
            return NEXT_NONE;
        case MZM_POSE_UNMORPHING:
            if (update_animation(x) == ANIM_ENDED) {
                s->unmorph_palette_timer = 15;
                return MZM_POSE_CROUCHING;
            }
            return NEXT_NONE;
        case MZM_POSE_PULLING_UP:
            if (update_animation(x) == ANIM_ENDED) {
                /* Native feet end one subpixel above the block; the
                 * runtime's boundary convention stores that as the edge. */
                s->y = (s->y - 1) & ~(MZM_BLOCK_SIZE - 1);
                return MZM_POSE_PULLING_FORWARD;
            }
            return NEXT_NONE;
        case MZM_POSE_PULLING_FORWARD:
            if (update_animation(x) == ANIM_ENDED) return MZM_POSE_STANDING;
            return NEXT_NONE;
        case MZM_POSE_GETTING_HURT:
        case MZM_POSE_DYING:
            if (update_animation(x) == ANIM_ENDED && s->anim_frame > 0)
                s->anim_frame--;
            return NEXT_NONE;
        case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
            return NEXT_NONE;
        default:
            loop_animation(x);
            return NEXT_NONE;
    }
}

static bool airborne_physics(MzmPose pose) {
    switch (pose) {
        case MZM_POSE_MIDAIR:
        case MZM_POSE_TURNING_AROUND_MIDAIR:
        case MZM_POSE_STARTING_SPIN_JUMP:
        case MZM_POSE_SPINNING:
        case MZM_POSE_SPACE_JUMPING:
        case MZM_POSE_SCREW_ATTACKING:
        case MZM_POSE_MORPH_BALL_MIDAIR:
        case MZM_POSE_GETTING_HURT:
        case MZM_POSE_GETTING_HURT_IN_MORPH_BALL:
            return true;
        default:
            return false;
    }
}

/* Native ledge probe from SamusCheckCollisions (Power Grip branch). */
static bool ledge_grab_allowed(Ctx *x) {
    MzmSamus *s = x->s;
    if (!(x->eq->items & MZM_ITEM_POWER_GRIP) || !held(x, forward_key(s)) ||
        s->y_velocity >= 1) return false;
    switch (s->pose) {
        case MZM_POSE_MIDAIR:
        case MZM_POSE_STARTING_SPIN_JUMP:
        case MZM_POSE_SPINNING:
        case MZM_POSE_STARTING_WALL_JUMP:
        case MZM_POSE_SPACE_JUMPING:
        case MZM_POSE_SCREW_ATTACKING:
            break;
        default:
            return false;
    }
    int32_t native_y = s->y - 1;
    int32_t offset = s->facing * (MZM_HALF_BLOCK_SIZE - 1);
    if (point_solid(x->c, s->x, native_y + MZM_HALF_BLOCK_SIZE)) return false;
    if (point_solid(x->c, s->x, native_y - (MZM_BLOCK_SIZE * 2 + MZM_HALF_BLOCK_SIZE)))
        return false;
    if (!point_solid(x->c, s->x + offset, native_y -
                     (MZM_BLOCK_SIZE + MZM_HALF_BLOCK_SIZE + MZM_EIGHTH_BLOCK_SIZE)))
        return false;
    return !point_solid(x->c, s->x + offset, native_y - MZM_BLOCK_SIZE * 2);
}

/* SamusUpdateVelocityPosition followed by box-swept collision resolution. */
static void move_and_collide(Ctx *x) {
    MzmSamus *s = x->s;
    MzmHitboxType type = mzm_pose_hitbox(s->pose);
    MzmStandingStatus standing = mzm_pose_standing(s->pose);
    int32_t dy = 0;
    if (airborne_physics(s->pose)) dy = mzm_integrate_y(&s->y_velocity);

    int velocity = s->x_velocity;
    if (standing == MZM_STANDING_GROUND && s->pose == MZM_POSE_RUNNING) {
        if (s->facing > 0 && velocity < 0) velocity = 0;
        if (s->facing < 0 && velocity > 0) velocity = 0;
    }
    int32_t dx = (standing == MZM_STANDING_GROUND ||
                  standing == MZM_STANDING_MIDAIR) ? (velocity >> 3) : 0;

    s->touching_side = false;
    int step = dx > 0 ? 1 : -1;
    for (int32_t moved = 0; moved != dx; moved += step) {
        int32_t nx = s->x + step;
        if (!box_blocked(x->c, type, nx, s->y)) {
            s->x = nx;
            continue;
        }
        bool climbed = false;
        if (standing == MZM_STANDING_GROUND &&
            (box_near_slope(x->c, type, s->x, s->y) ||
             box_near_slope(x->c, type, nx, s->y))) {
            for (int rise = 1; rise <= 2 * MZM_PIXEL_SIZE; ++rise) {
                if (!box_blocked(x->c, type, nx, s->y - rise)) {
                    s->x = nx;
                    s->y -= rise;
                    climbed = true;
                    break;
                }
            }
        }
        if (!climbed) {
            s->touching_side = true;
            break;
        }
    }
    if (s->touching_side) {
        s->x_velocity = 0;
        if (s->pose == MZM_POSE_SPINNING || s->pose == MZM_POSE_SCREW_ATTACKING) {
            s->walljump_timer = MZM_WALL_JUMP_TICKS;
            s->last_wall = -s->facing;
        }
    }

    /* The pull-forward pose starts beside the grabbed block; like the
     * original forced movement it is not dropped by a ground check. */
    if (s->pose == MZM_POSE_PULLING_FORWARD) return;
    if (standing == MZM_STANDING_GROUND) {
        if (!box_blocked(x->c, type, s->x, s->y + 1) &&
            box_near_slope(x->c, type, s->x, s->y + 2 * MZM_PIXEL_SIZE)) {
            for (int fall = 1; fall <= 2 * MZM_PIXEL_SIZE; ++fall) {
                if (box_blocked(x->c, type, s->x, s->y + 1)) break;
                s->y++;
            }
        }
        if (!box_blocked(x->c, type, s->x, s->y + 1)) {
            s->forced = MZM_FORCED_NONE;
            request_midair(x);
        }
        return;
    }
    if (standing != MZM_STANDING_MIDAIR) return;

    step = dy > 0 ? 1 : -1;
    for (int32_t moved = 0; moved != dy; moved += step) {
        if (box_blocked(x->c, type, s->x, s->y + step)) {
            if (step > 0) {
                s->y_velocity = 0;
                request_landing(x);
                return;
            }
            if (s->y_velocity > 0) s->y_velocity = 0;
            break;
        }
        s->y += step;
    }
    if (airborne_physics(s->pose) && s->y_velocity <= 0 &&
        box_blocked(x->c, type, s->x, s->y + 1)) {
        s->y_velocity = 0;
        request_landing(x);
        return;
    }
    if (ledge_grab_allowed(x)) {
        int32_t old_y = s->y;
        set_pose(x, MZM_POSE_HANGING_ON_LEDGE);
        if (box_blocked(x->c, MZM_HITBOX_STANDING, s->x, s->y)) {
            /* The verified boxes disagree with the native probe; stay
             * airborne instead of embedding Samus in collision. */
            s->pose = MZM_POSE_MIDAIR;
            s->y = old_y;
            s->grabbed_ledge = false;
        }
    }
}

void mzm_samus_update(MzmSamus *samus, const MzmInput *input,
                      const MzmEquipment *equipment,
                      const MzmCollision *collision,
                      const MzmAnimationSource *animation) {
    Ctx x = {samus, input, equipment, collision, animation};
    samus->grabbed_ledge = false;
    samus->anim_counter++;
    if (samus->invincibility) samus->invincibility--;
    if (samus->unmorph_palette_timer) samus->unmorph_palette_timer--;
    set_spinning_pose(&x);
    int next = pose_handler(&x);
    if (next == NEXT_NONE) next = pose_graphics(&x);
    apply_next(&x, next);
    move_and_collide(&x);
}

/* SamusChangeToHurtPose */
void mzm_samus_hurt(MzmSamus *samus, const MzmCollision *collision,
                    bool lethal) {
    MzmSamus copy = copy_data(samus);
    samus->turning = false;
    samus->aim = copy.aim;
    samus->invincibility = MZM_INVINCIBILITY_TICKS;
    if (lethal) {
        /* The original also drifts Samus to the screen centre. */
        samus->pose = MZM_POSE_DYING;
        return;
    }
    if (mzm_pose_is_morphed(copy.pose)) {
        samus->pose = MZM_POSE_GETTING_HURT_IN_MORPH_BALL;
    } else {
        samus->pose = MZM_POSE_GETTING_HURT;
        if (box_blocked(collision, MZM_HITBOX_STANDING, samus->x, samus->y))
            samus->y = ((samus->y - 1) | (MZM_BLOCK_SIZE - 1)) + 1;
    }
    samus->y_velocity = mzm_pose_standing(copy.pose) == MZM_STANDING_MIDAIR ?
        8 * (MZM_EIGHTH_BLOCK_SIZE - 1) :
        8 * (MZM_QUARTER_BLOCK_SIZE - MZM_PIXEL_SIZE / 2);
}
