/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Samus pose controller.
 *
 * Constants, hitboxes and pose transitions follow the pinned mzm
 * decompilation (src/samus.c, include/constants/samus.h and
 * src/data/samus/samus_animation_pointers.c). Positions use native
 * subpixels (4 per pixel, 64 per block) and velocities use native units
 * (1/8 subpixel per 60 Hz frame; positive Y velocity moves upward).
 *
 * Block collision is still resolved against the runtime's verified
 * Clipdata boxes with a subpixel sweep instead of the original point-probe
 * routines, so slope speed changes and partial-ceiling nudges are not yet
 * reproduced. This module has no SDL dependency. */
#ifndef MZM_SAMUS_H
#define MZM_SAMUS_H

#include <stdbool.h>
#include <stdint.h>

#define MZM_SUBPIXELS_PER_PIXEL 4
#define MZM_BLOCK_SIZE 64
#define MZM_HALF_BLOCK_SIZE 32
#define MZM_QUARTER_BLOCK_SIZE 16
#define MZM_EIGHTH_BLOCK_SIZE 8
#define MZM_PIXEL_SIZE 4

#define MZM_X_ACCELERATION 8
#define MZM_X_VELOCITY_CAP 96
#define MZM_X_MIDAIR_VELOCITY_CAP 64
#define MZM_X_MIDAIR_ACCELERATION 8
#define MZM_X_MIDAIR_MORPHED_VELOCITY_CAP 48
#define MZM_Y_ACCELERATION 10
#define MZM_Y_POSITIVE_VELOCITY_CAP 192
#define MZM_Y_NEGATIVE_VELOCITY_CAP 128
#define MZM_HIGH_JUMP_VELOCITY 232
#define MZM_LOW_JUMP_VELOCITY 192
#define MZM_SUITLESS_JUMP_VELOCITY 212
#define MZM_MORPH_BALL_JUMP_VELOCITY 212
#define MZM_INVINCIBILITY_TICKS 48
#define MZM_WALL_JUMP_TICKS 8

typedef enum {
    MZM_POSE_STANDING,
    MZM_POSE_RUNNING,
    MZM_POSE_TURNING_AROUND,
    MZM_POSE_SHOOTING,
    MZM_POSE_CROUCHING,
    MZM_POSE_TURNING_AROUND_AND_CROUCHING,
    MZM_POSE_SHOOTING_AND_CROUCHING,
    MZM_POSE_MIDAIR,
    MZM_POSE_TURNING_AROUND_MIDAIR,
    MZM_POSE_LANDING,
    MZM_POSE_STARTING_SPIN_JUMP,
    MZM_POSE_SPINNING,
    MZM_POSE_STARTING_WALL_JUMP,
    MZM_POSE_SPACE_JUMPING,
    MZM_POSE_SCREW_ATTACKING,
    MZM_POSE_MORPHING,
    MZM_POSE_MORPH_BALL,
    MZM_POSE_ROLLING,
    MZM_POSE_UNMORPHING,
    MZM_POSE_MORPH_BALL_MIDAIR,
    MZM_POSE_HANGING_ON_LEDGE,
    MZM_POSE_PULLING_UP,
    MZM_POSE_PULLING_FORWARD,
    MZM_POSE_GETTING_HURT,
    MZM_POSE_GETTING_HURT_IN_MORPH_BALL,
    MZM_POSE_DYING,
    MZM_POSE_COUNT
} MzmPose;

typedef enum {
    MZM_HITBOX_STANDING,
    MZM_HITBOX_CROUCHED,
    MZM_HITBOX_MORPHED
} MzmHitboxType;

typedef enum {
    MZM_STANDING_GROUND,
    MZM_STANDING_MIDAIR,
    MZM_STANDING_HANGING,
    MZM_STANDING_FORCED
} MzmStandingStatus;

/* Offsets from Samus's bottom-centre position, in subpixels. */
typedef struct { int16_t left, top, right; } MzmBlockHitbox;

typedef enum {
    MZM_AIM_FORWARD,
    MZM_AIM_DIAGONAL_UP,
    MZM_AIM_DIAGONAL_DOWN,
    MZM_AIM_UP,
    MZM_AIM_DOWN
} MzmAim;

/* GBA-equivalent buttons. Diagonal aim is the L shoulder. */
enum {
    MZM_KEY_RIGHT = 1u << 0,
    MZM_KEY_LEFT = 1u << 1,
    MZM_KEY_UP = 1u << 2,
    MZM_KEY_DOWN = 1u << 3,
    MZM_KEY_A = 1u << 4,
    MZM_KEY_B = 1u << 5,
    MZM_KEY_L = 1u << 6,
    MZM_KEY_R = 1u << 7,
    MZM_KEY_SELECT = 1u << 8
};

typedef struct {
    uint16_t held;
    uint16_t pressed;
    /* Set by the weapon system when a projectile was spawned this frame. */
    bool new_projectile;
} MzmInput;

typedef enum {
    MZM_SUIT_NORMAL,
    MZM_SUIT_FULLY_POWERED,
    MZM_SUIT_SUITLESS
} MzmSuitType;

enum {
    MZM_ITEM_MORPH_BALL = 1u << 0,
    MZM_ITEM_HIGH_JUMP = 1u << 1,
    MZM_ITEM_SPEED_BOOSTER = 1u << 2,
    MZM_ITEM_SPACE_JUMP = 1u << 3,
    MZM_ITEM_SCREW_ATTACK = 1u << 4,
    MZM_ITEM_VARIA_SUIT = 1u << 5,
    MZM_ITEM_GRAVITY_SUIT = 1u << 6,
    MZM_ITEM_POWER_GRIP = 1u << 7
};

typedef struct {
    MzmSuitType suit;
    uint32_t items;
    int energy, max_energy;
    int missiles, max_missiles;
    int super_missiles, max_super_missiles;
} MzmEquipment;

typedef struct {
    void *context;
    /* True when the pixel-space box overlaps blocking collision. */
    bool (*blocked)(void *context, float x, float y, float w, float h);
    /* Optional: true when grounded movement may follow a slope here. */
    bool (*slope_near)(void *context, float x, float y, float w, float h);
} MzmCollision;

struct MzmSamus;
typedef struct {
    void *context;
    /* Write the native 60 Hz duration of each frame of the animation that
     * the pose currently displays. Returns the frame count, or 0 when the
     * animation is unavailable. Unavailable one-shot animations end after a
     * single tick instead of stalling the state machine. */
    int (*durations)(void *context, const struct MzmSamus *samus,
                     uint8_t *durations, int max);
} MzmAnimationSource;

typedef enum {
    MZM_FORCED_NONE,
    MZM_FORCED_JUMP,
    MZM_FORCED_FALL,
    MZM_FORCED_CARRY,
    MZM_FORCED_BOUNCE_BEFORE_JUMP,
    MZM_FORCED_BOUNCE_AFTER_FALL
} MzmForcedMovement;

typedef struct MzmSamus {
    MzmPose pose;
    int32_t x, y;
    int16_t x_velocity, y_velocity;
    int facing;
    bool turning;
    MzmAim aim;
    MzmForcedMovement forced;
    int last_wall;
    uint8_t walljump_timer;
    uint8_t timer;
    uint8_t anim_frame, anim_counter;
    uint8_t invincibility;
    bool touching_side;
    bool grabbed_ledge;
} MzmSamus;

const MzmBlockHitbox *mzm_block_hitbox(MzmHitboxType type);
MzmHitboxType mzm_pose_hitbox(MzmPose pose);
MzmStandingStatus mzm_pose_standing(MzmPose pose);
const char *mzm_pose_name(MzmPose pose);
bool mzm_pose_is_spinning(MzmPose pose);
bool mzm_pose_is_morphed(MzmPose pose);

int16_t mzm_apply_x_acceleration(int16_t velocity, int facing,
                                 int acceleration, int cap);
/* Advance one frame of airborne Y motion; returns the subpixel delta
 * (positive is downward) and applies gravity to the velocity. */
int32_t mzm_integrate_y(int16_t *y_velocity);
int16_t mzm_jump_velocity(const MzmEquipment *equipment);
/* Native SpriteUtilTakeDamageFromSprite suit reduction; false on death. */
bool mzm_equipment_damage(MzmEquipment *equipment, int damage);

void mzm_samus_init(MzmSamus *samus, int32_t x, int32_t y, int facing);
void mzm_samus_box(const MzmSamus *samus, float *x, float *y,
                   float *w, float *h);
void mzm_samus_update(MzmSamus *samus, const MzmInput *input,
                      const MzmEquipment *equipment,
                      const MzmCollision *collision,
                      const MzmAnimationSource *animation);
/* Native SamusChangeToHurtPose: lethal hits enter the dying pose. */
void mzm_samus_hurt(MzmSamus *samus, const MzmCollision *collision,
                    bool lethal);

#endif /* MZM_SAMUS_H */
