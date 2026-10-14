/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission Samus controller.
 *
 * A transliteration of the pinned mzm decompilation's src/samus.c: the pose
 * handlers and their graphics loops (SamusExecutePoseHandler), the pose
 * changes (SamusSetPose, SamusSetMidAir, SamusSetLandingPose,
 * SamusCheckCarryFromCopy), the physics, the velocity update and the native
 * point-probe collisions (SamusUpdateHitboxMovingDirection,
 * SamusCheckCollisions and their probes), with the tables of
 * include/constants/samus.h and src/data/samus/samus_animation_pointers.c.
 * Positions use native subpixels (4 per pixel, 64 per block) and velocities
 * native units (1/8 subpixel per 60 Hz frame; positive Y velocity moves
 * upward). The public `y` is the native yPosition plus one (the runtime's
 * feet sit on block boundaries); the controller works on native values.
 * This module has no SDL dependency. */
#ifndef MZM_SAMUS_H
#define MZM_SAMUS_H

#include <stdbool.h>
#include <stddef.h>
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
/* SAMUS_X_SPEEDBOOST_VELOCITY_CAP and SHINESPARK_TIMER_MAX. */
#define MZM_X_SPEEDBOOST_VELOCITY_CAP 160
#define MZM_SHINESPARK_TIMER_MAX 180
/* SAMUS_SIDEWARD_SHINESPARK_X_VELOCITY (also the upward speed) and the
 * diagonal velocities. */
#define MZM_SHINESPARK_VELOCITY 192
#define MZM_DIAGONAL_SHINESPARK_X_VELOCITY 160
#define MZM_DIAGONAL_SHINESPARK_Y_VELOCITY 96
#define MZM_X_MIDAIR_ACCELERATION 8
#define MZM_X_MIDAIR_MORPHED_VELOCITY_CAP 48
#define MZM_Y_ACCELERATION 10
#define MZM_Y_POSITIVE_VELOCITY_CAP 192
#define MZM_Y_NEGATIVE_VELOCITY_CAP 128
#define MZM_Y_SLOWED_VELOCITY_CAP 40
#define MZM_HIGH_JUMP_VELOCITY 232
#define MZM_LOW_JUMP_VELOCITY 192
#define MZM_SUITLESS_JUMP_VELOCITY 212
#define MZM_MORPH_BALL_JUMP_VELOCITY 212
#define MZM_ELEVATOR_SPEED 8
/* SAMUS_BOMB_BOUNCE_X_VELOCITY and SAMUS_BOMB_BOUNCE_Y_VELOCITY. */
#define MZM_BOMB_BOUNCE_X_VELOCITY 48
#define MZM_BOMB_BOUNCE_Y_VELOCITY 164
#define MZM_INVINCIBILITY_TICKS 48
#define MZM_WALL_JUMP_TICKS 8

/* SamusPose */
typedef enum {
    MZM_POSE_RUNNING,
    MZM_POSE_STANDING,
    MZM_POSE_TURNING_AROUND,
    MZM_POSE_SHOOTING,
    MZM_POSE_CROUCHING,
    MZM_POSE_TURNING_AROUND_AND_CROUCHING,
    MZM_POSE_SHOOTING_AND_CROUCHING,
    MZM_POSE_SKIDDING,
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
    MZM_POSE_TURNING_TO_AIM_WHILE_HANGING,
    MZM_POSE_HIDING_ARM_CANNON_WHILE_HANGING,
    MZM_POSE_AIMING_WHILE_HANGING,
    MZM_POSE_SHOOTING_WHILE_HANGING,
    MZM_POSE_PULLING_YOURSELF_UP_FROM_HANGING,
    MZM_POSE_PULLING_YOURSELF_FORWARD_FROM_HANGING,
    MZM_POSE_PULLING_YOURSELF_INTO_A_MORPH_BALL_TUNNEL,
    MZM_POSE_USING_AN_ELEVATOR,
    MZM_POSE_FACING_THE_FOREGROUND,
    MZM_POSE_TURNING_FROM_FACING_THE_FOREGROUND,
    MZM_POSE_GRABBED_BY_CHOZO_STATUE,
    MZM_POSE_DELAY_BEFORE_SHINESPARKING,
    MZM_POSE_SHINESPARKING,
    MZM_POSE_SHINESPARK_COLLISION,
    MZM_POSE_DELAY_AFTER_SHINESPARKING,
    MZM_POSE_DELAY_BEFORE_BALLSPARKING,
    MZM_POSE_BALLSPARKING,
    MZM_POSE_BALLSPARK_COLLISION,
    MZM_POSE_ON_ZIPLINE,
    MZM_POSE_SHOOTING_ON_ZIPLINE,
    MZM_POSE_TURNING_ON_ZIPLINE,
    MZM_POSE_MORPH_BALL_ON_ZIPLINE,
    MZM_POSE_SAVING_LOADING_GAME,
    MZM_POSE_DOWNLOADING_MAP_DATA,
    MZM_POSE_TURNING_AROUND_TO_DOWNLOAD_MAP_DATA,
    MZM_POSE_GETTING_HURT,
    MZM_POSE_GETTING_KNOCKED_BACK,
    MZM_POSE_GETTING_HURT_IN_MORPH_BALL,
    MZM_POSE_GETTING_KNOCKED_BACK_IN_MORPH_BALL,
    MZM_POSE_DYING,
    MZM_POSE_CROUCHING_TO_CRAWL,
    MZM_POSE_CRAWLING_STOPPED,
    MZM_POSE_STARTING_TO_CRAWL,
    MZM_POSE_CRAWLING,
    MZM_POSE_UNCROUCHING_FROM_CRAWLING,
    MZM_POSE_TURNING_AROUND_WHILE_CRAWLING,
    MZM_POSE_SHOOTING_WHILE_CRAWLING,
    MZM_POSE_UNCROUCHING_SUITLESS,
    MZM_POSE_CROUCHING_SUITLESS,
    MZM_POSE_GRABBING_A_LEDGE_SUITLESS,
    MZM_POSE_FACING_THE_BACKGROUND_SUITLESS,
    MZM_POSE_TURNING_FROM_FACING_THE_BACKGROUND_SUITLESS,
    MZM_POSE_ACTIVATING_ZIPLINES,
    MZM_POSE_IN_ESCAPE_SHIP,
    MZM_POSE_TURNING_TO_ENTER_ESCAPE_SHIP,
    MZM_POSE_COUNT,

    MZM_POSE_KNOCKBACK_REQUEST = 0xF9,
    MZM_POSE_HURT_REQUEST,
    MZM_POSE_LANDING_REQUEST = 0xFD,
    MZM_POSE_MID_AIR_REQUEST,
    MZM_POSE_NONE
} MzmPose;

/* SamusHitboxType */
typedef enum {
    MZM_HITBOX_STANDING,
    MZM_HITBOX_CROUCHED,
    MZM_HITBOX_MORPHED,
    MZM_HITBOX_ZIPLINE
} MzmHitboxType;

/* SamusStandingStatus */
typedef enum {
    MZM_STANDING_GROUND,
    MZM_STANDING_ENEMY,
    MZM_STANDING_MIDAIR,
    MZM_STANDING_NOT_IN_CONTROL,
    MZM_STANDING_FORCED,
    MZM_STANDING_HANGING
} MzmStandingStatus;

/* Offsets from Samus's bottom-centre position, in subpixels
 * (sSamusBlockHitboxData). */
typedef struct { int16_t left, top, right; } MzmBlockHitbox;

/* ArmCannonDirection */
typedef enum {
    MZM_AIM_FORWARD,
    MZM_AIM_DIAGONAL_UP,
    MZM_AIM_DIAGONAL_DOWN,
    MZM_AIM_UP,
    MZM_AIM_DOWN,
    MZM_AIM_NONE
} MzmAim;

/* DiagonalAim */
enum { MZM_DIAG_AIM_NONE, MZM_DIAG_AIM_UP, MZM_DIAG_AIM_DOWN };

/* GBA-equivalent buttons. Diagonal aim is the L shoulder, weapon arming R. */
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

/* WeaponHightlighted */
enum { MZM_WH_MISSILE = 1u << 0, MZM_WH_SUPER_MISSILE = 1u << 1, MZM_WH_POWER_BOMB = 1u << 2 };

typedef struct {
    uint16_t held;
    uint16_t pressed;
    /* gSamusPhysics.hasNewProjectile, from SamusCheckNewProjectile. */
    bool new_projectile;
    /* gSamusWeaponInfo.weaponHighlighted (MZM_WH_*) and chargeCounter, read
     * by SamusAimCannon and SamusHangingOnLedge. */
    uint8_t weapon_highlighted;
    uint8_t charge_counter;
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
    MZM_ITEM_POWER_GRIP = 1u << 7,
    MZM_ITEM_BOMBS = 1u << 8,         /* BBF_BOMBS */
    MZM_ITEM_CHARGE_BEAM = 1u << 9,   /* BBF_CHARGE_BEAM */
    MZM_ITEM_LONG_BEAM = 1u << 10,    /* BBF_LONG_BEAM */
    MZM_ITEM_ICE_BEAM = 1u << 11,     /* BBF_ICE_BEAM */
    MZM_ITEM_WAVE_BEAM = 1u << 12,    /* BBF_WAVE_BEAM */
    MZM_ITEM_PLASMA_BEAM = 1u << 13   /* BBF_PLASMA_BEAM */
};

/* Damage types of clipdata-affecting actions (hatches, blocks). */
enum {
    MZM_DAMAGE_BEAM = 1u << 0,
    MZM_DAMAGE_BOMB_PISTOL = 1u << 1,
    MZM_DAMAGE_MISSILE = 1u << 2,
    MZM_DAMAGE_SUPER_MISSILE = 1u << 3,
    MZM_DAMAGE_POWER_BOMB = 1u << 4
};

typedef struct {
    MzmSuitType suit;
    uint32_t items;
    int energy, max_energy;
    int missiles, max_missiles;
    int super_missiles, max_super_missiles;
    int power_bombs, max_power_bombs;
    /* Obtained but not activated (gEquipment.beamBombs / suitMisc without
     * their activation): the unknown items until the fully powered suit. */
    uint32_t unknown_items;
    /* gEquipment.grabbedByMetroid: Samus is slowed. */
    bool grabbed_by_metroid;
} MzmEquipment;

/* ClipdataProcessForSamus's result: the ClipdataType plus this flag when
 * the point is in the solid part of its block. */
#define MZM_SAMUS_CLIP_SOLID_FLAG (1u << 24)
/* ClipdataCheckCurrentAffectingAtPosition: movement << 16 | hazard. */
enum {
    MZM_SAMUS_MOVEMENT_ELEVATOR_DOWN_BLOCK = 1,
    MZM_SAMUS_MOVEMENT_ELEVATOR_UP_BLOCK = 2,
    MZM_SAMUS_MOVEMENT_NON_POWER_GRIP = 6,
};
/* DestructingAction of BlockSamusApplyScrewSpeedboosterDamageToEnvironment. */
enum {
    MZM_SAMUS_DESTRUCT_SPEED = 1,
    MZM_SAMUS_DESTRUCT_SCREW = 2,
    MZM_SAMUS_DESTRUCT_SPEED_SCREW = 3,
    MZM_SAMUS_DESTRUCT_SPEED_ON_GROUND = 4,
};

typedef struct {
    void *context;
    /* True when the pixel-space box overlaps blocking collision (spawn
     * placement and the other modules' box tests). */
    bool (*blocked)(void *context, float x, float y, float w, float h);
    /* Optional: true when grounded movement may follow a slope here. */
    bool (*slope_near)(void *context, float x, float y, float w, float h);
    /* Optional native point query (ClipdataProcessForSamus/ClipdataProcess)
     * in subpixels; actor 0 is Samus, 1 a projectile (a non-sprite
     * ClipdataProcess), 2 a sprite (ClipdataProcess without an action). Falls back to a
     * one-pixel box through ``blocked``. */
    bool (*solid_point)(void *context, int32_t x, int32_t y, int actor);
    /* Optional: a projectile of the given damage type (MZM_DAMAGE_*) hit
     * solid collision at this subpixel point, as BlockApplyCcaa sees it. */
    void (*affect)(void *context, int32_t x, int32_t y, int damage);
    /* ClipdataProcessForSamus at a native subpixel point (the ClipdataType
     * plus MZM_SAMUS_CLIP_SOLID_FLAG). When NULL, solid_point gives the
     * solid flag of a plain block. */
    uint32_t (*clipdata)(void *context, uint16_t y, uint16_t x);
    /* Optional ClipdataCheckCurrentAffectingAtPosition (movement << 16 |
     * hazard); NULL is no movement block and no hazard. */
    uint32_t (*affecting)(void *context, uint16_t y, uint16_t x);
    /* Optional BlockSamusApplyScrewSpeedboosterDamageToEnvironment. */
    void (*destruct)(void *context, uint16_t x, uint16_t y, int action);
    /* Optional ClipdataCheckGroundEffect (MZM_GROUND_EFFECT_*). */
    uint8_t (*ground_effect)(void *context, uint16_t y, uint16_t x);
} MzmCollision;

/* GroundEffect */
enum {
    MZM_GROUND_EFFECT_NONE,
    MZM_GROUND_EFFECT_WET_GROUND,
    MZM_GROUND_EFFECT_DUSTY_GROUND,
    MZM_GROUND_EFFECT_VERY_DUSTY_GROUND,
    MZM_GROUND_EFFECT_UNKNOWN,
    MZM_GROUND_EFFECT_BUBBLY_GROUND
};

struct MzmSamus;
typedef struct {
    void *context;
    /* The native frame durations (60 Hz) of a Samus animation, named by its
     * library key ("PowerSuit/pose/SPOSE_RUNNING/right",
     * "PowerSuit/Running/ACD_FORWARD/right", ...). Returns the frame count,
     * or 0 when the animation is unavailable. Unavailable animations end
     * after a single tick instead of stalling the state machine. */
    int (*durations)(void *context, const char *key, uint8_t *durations, int max);
} MzmAnimationSource;

/* ForcedMovement (its values overlap by pose). */
enum {
    MZM_FORCED_NONE = 0,
    MZM_FORCED_UPWARDS_SHINESPARK = 0,
    MZM_FORCED_SIDEWARDS_SHINESPARK = 1,
    MZM_FORCED_DIAGONAL_SHINESPARK = 2,
    MZM_FORCED_CRAWLING_ARM_CANNON_DOWN = 0,
    MZM_FORCED_CRAWLING_ARM_CANNON_UP = 1,
    MZM_FORCED_FALL = 0,                /* FORCED_MOVEMENT_MID_AIR_FALL */
    MZM_FORCED_JUMP = 1,                /* FORCED_MOVEMENT_MID_AIR_JUMP */
    MZM_FORCED_CARRY = 2,               /* FORCED_MOVEMENT_MID_AIR_CARRY */
    MZM_FORCED_BOUNCE_AFTER_FALL = 1,   /* FORCED_MOVEMENT_MORPH_BALL_BOUNCE_AFTER_FALL */
    MZM_FORCED_BOMB_JUMP_RIGHT = 10,
    MZM_FORCED_BOMB_JUMP_UP = 11,
    MZM_FORCED_BOMB_JUMP_LEFT = 12,
    MZM_FORCED_BOUNCE_BEFORE_JUMP = 20, /* FORCED_MOVEMENT_MORPH_BALL_BOUNCE_BEFORE_JUMP */
    MZM_FORCED_LAUNCHED_BY_CANNON = 0xF0,
    MZM_FORCED_DAMAGED_BY_RUINS_TEST = 0xFF
};

/* The direction argument of SamusBombBounce: a bomb jump forced movement,
 * plus MZM_BOMB_JUMP_ABOVE when the bomb is above Samus's feet. */
enum { MZM_BOMB_JUMP_ABOVE = 0x80 };

/* The fields of struct SamusData. */
#define MZM_SAMUS_DATA_FIELDS                                                         \
    MzmPose pose;                                                                     \
    uint8_t standing;           /* standingStatus (MzmStandingStatus) */              \
    MzmAim aim;                 /* armCannonDirection */                              \
    bool turning;                                                                     \
    uint8_t forced;             /* forcedMovement */                                  \
    uint8_t speedboosting;      /* speedboostingShinesparking */                      \
    uint8_t invincibility;      /* invincibilityTimer */                              \
    uint8_t walljump_timer;                                                           \
    uint8_t shinespark_timer;                                                         \
    uint8_t unmorph_palette_timer;                                                    \
    uint8_t timer;                                                                    \
    uint16_t last_wall;         /* lastWallTouchedMidAir (native key bits) */         \
    int facing;                 /* direction: 1 KEY_RIGHT, -1 KEY_LEFT */             \
    uint16_t elevator_direction; /* elevatorDirection: MZM_KEY_UP or MZM_KEY_DOWN */  \
    int32_t x, y;               /* xPosition, yPosition (y: native + 1 outside) */    \
    int16_t x_velocity, y_velocity;                                                   \
    uint16_t current_slope;     /* currentSlope (SLOPE_* | native key bits) */        \
    uint8_t anim_counter;       /* animationDurationCounter */                        \
    uint8_t anim_frame;         /* currentAnimationFrame */

typedef struct { MZM_SAMUS_DATA_FIELDS } MzmSamusData;

typedef struct MzmSamus {
    union {
        struct { MZM_SAMUS_DATA_FIELDS };
        MzmSamusData data;
    };
    /* gSamusDataCopy (its yPosition is the public copy_y). */
    MzmSamusData copy;
    int32_t copy_y;
    /* gSamusPhysics: moving directions, touching flags, liquid slowing and
     * the block hitbox of SamusUpdateVelocityPosition. */
    uint8_t h_moving, v_moving;
    uint8_t touching_side, touching_top;
    bool slowed;
    bool has_new_projectile;
    MzmHitboxType hitbox_type;
    /* gSamusWeaponInfo.diagonalAim */
    uint8_t diagonal_aim;
    /* gPreviousXPosition / gPreviousYPosition (native). */
    int32_t prev_x, prev_y;
    /* gUnk_03004FC9: no vertical moving direction this frame. */
    bool keep_vertical;
    /* gScrewSpeedAnimation.flag */
    uint8_t screw_speed_flag;
    /* standingStatus STANDING_ENEMY: set by the sprites. */
    bool on_enemy;
    /* A ledge grab this update (the grab sound). */
    bool grabbed_ledge;
    /* gBg1XPosition / gBg1YPosition (subpixels) for SamusChangeToHurtPose
     * and SamusDying. */
    int32_t camera_x, camera_y;
    /* gSamusHazardDamage: damageTimer, knockbackTimer, paletteTimer. */
    uint8_t hazard_damage_timer, hazard_knockback_timer, hazard_palette_timer;
    /* gMainGameMode = GM_GAMEOVER: SamusDying's timer reached 120. */
    bool game_over;
    /* gMonochromeBgFading requests (MZM_MONOCHROME_*): black when the hit
     * kills her, white once her dying animation flashes; the game state
     * takes them (ColorFadingApplyMonochrome). */
    uint8_t monochrome;
} MzmSamus;

/* MonochromeFading */
enum { MZM_MONOCHROME_NONE, MZM_MONOCHROME_BLACK, MZM_MONOCHROME_WHITE };

/* HazardType of ClipdataCheckCurrentAffectingAtPosition's low byte. */
enum {
    MZM_HAZARD_NONE, MZM_HAZARD_WATER, MZM_HAZARD_LAVA, MZM_HAZARD_WEAK_ACID,
    MZM_HAZARD_STRONG_ACID, MZM_HAZARD_HEAT, MZM_HAZARD_COLD_KNOCKBACK, MZM_HAZARD_COLD
};

const MzmBlockHitbox *mzm_block_hitbox(MzmHitboxType type);
/* sSamusCollisionData[pose][SCDF_BLOCK_HITBOX] and [SCDF_STANDING_STATUS]. */
MzmHitboxType mzm_pose_hitbox(MzmPose pose);
MzmStandingStatus mzm_pose_standing(MzmPose pose);
/* The SPOSE_* name of a pose. */
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
/* SamusUpdate followed by SamusUpdateHitboxMovingDirection; the hazards
 * (SamusTakeHazardDamage) take energy from the equipment. */
void mzm_samus_update(MzmSamus *samus, const MzmInput *input,
                      MzmEquipment *equipment,
                      const MzmCollision *collision,
                      const MzmAnimationSource *animation);
/* SamusSetPose(SPOSE_HURT_REQUEST): `lethal` when the energy is 0. */
void mzm_samus_hurt(MzmSamus *samus, const MzmCollision *collision,
                    bool lethal);
/* SamusSetPose(SPOSE_KNOCKBACK_REQUEST) */
void mzm_samus_knockback(MzmSamus *samus, const MzmCollision *collision);
/* The pose test of SamusBombBounce (no liquid). */
bool mzm_samus_bomb_bounce_allowed(const MzmSamus *samus, int direction);
/* SamusSetPose(SPOSE_GRABBED_BY_CHOZO_STATUE) from a statue's arm, and
 * SamusSetPose(SPOSE_MORPH_BALL) when it lets go. */
void mzm_samus_chozo_grab(MzmSamus *samus, bool grabbed, const MzmEquipment *equipment,
                          const MzmCollision *collision);
/* SamusSetPose from a sprite (the map station). */
void mzm_samus_sprite_set_pose(MzmSamus *samus, MzmPose pose, const MzmEquipment *equipment,
                               const MzmCollision *collision);
/* SamusSetPose(SPOSE_LANDING_REQUEST) from SpriteUtilSamusStandingOnSprite. */
void mzm_samus_land_on_sprite(MzmSamus *samus, const MzmInput *input,
                              const MzmEquipment *equipment, const MzmCollision *collision);
/* SamusBombBounce: the bomb jump through SamusSetMidAir; false when the
 * pose does not bounce. */
bool mzm_samus_bomb_bounce(MzmSamus *samus, const MzmEquipment *equipment,
                           const MzmCollision *collision, int direction);
/* The body animation SamusUpdateGraphicsOam draws (its library key, e.g.
 * "PowerSuit/Running/ACD_NONE/left"), from the pose, the facing, the arm
 * cannon direction and gSamusWeaponInfo.weaponHighlighted. */
void mzm_samus_graphics_key(const MzmSamus *samus, const MzmEquipment *equipment,
                            uint8_t weapon_highlighted, char *key, size_t size);
/* The armed arm cannon graphics of SamusUpdateGraphicsOam (missiles or
 * super missiles highlighted). */
bool mzm_samus_armed_graphics(uint8_t weapon_highlighted);

#endif /* MZM_SAMUS_H */
