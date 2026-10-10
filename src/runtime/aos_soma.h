/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow Soma motion rules.
 *
 * Ports of verified parts of cvaos asm/code/code_08014548.s (see
 * docs/AOS_SOMA.md): the position integration at the start of the player
 * update sub_0801B0D8, ground and air steering with friction, the jump
 * routine sub_08019180 (normal jump only), the air rules of sub_0801938C
 * (jump release, apex float, slowed fall), the ledge start of sub_08018020
 * and the gravity of sub_08018B98 with its ceiling bump, and the tile path of
 * the collision pass sub_08014A04 (walls, ceilings, water flags, floor snap
 * and landing), the movement part of player states 0, 3, 4, 5, 6 and 7
 * with the ability moves, and the animation requests. Moving platforms and
 * carried entities (gEwramData + 0x1316C, + 0x131B4), the other states,
 * the weapon entities (sprites, hitboxes), hurtboxes and effects are NOT
 * ported.
 * Values are 16.16 fixed point pixels per 60 Hz frame, positive Y downward;
 * positions are room pixels. No SDL. */
#ifndef AOS_SOMA_H
#define AOS_SOMA_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "aos_collision.h"
#include "aos_combat.h"

#define AOS_FIXED(value) ((int32_t)((value) * 65536.0))
#define AOS_WALK_SPEED 0x18000          /* 1.5 */
#define AOS_FAST_WALK_SPEED 0x40000     /* 4.0, flag 0x400 of +0x13260 */
#define AOS_FRICTION 0x4000             /* 0.25 */
#define AOS_JUMP_VELOCITY ((int32_t)0xFFFB0C00)        /* -4.953125 */
#define AOS_SLOWED_JUMP_VELOCITY ((int32_t)0xFFFEC400) /* -1.234375 */
#define AOS_FALL_CAP 0x80000            /* 8.0 */

/* Entity flags (+0x10) used here. */
enum {
    AOS_FLAG_AIRBORNE = 1u << 1,
    AOS_FLAG_ATTACKING = 1u << 5,      /* 0x20 */
    AOS_FLAG_AIR_ATTACK = 1u << 6,     /* 0x40 */
    AOS_FLAG_STOP_AT_WALL = 1u << 7,   /* 0x80: walls stop instead of bounce */
    AOS_FLAG_CROUCH = 1u << 10,        /* 0x400 */
    AOS_FLAG_PLATFORM_ONLY = 1u << 12, /* 0x1000: no solid cell under the feet */
    AOS_FLAG_SLOPE_LEFT = 1u << 13,    /* 0x2000: slope byte without bit 2 */
    AOS_FLAG_SLOPE_RIGHT = 1u << 14,   /* 0x4000: slope byte with bit 2 */
    AOS_FLAG_HEAD_CEILING = 1u << 15,  /* 0x8000: ceiling at x +/- 5, y - 20 */
    AOS_FLAG_HARD_LANDING = 1u << 16,  /* 0x10000 */
    AOS_FLAG_WALL = 1u << 18,          /* 0x40000: a wall pushed Soma */
    AOS_FLAG_GROUNDED = 1u << 20,      /* 0x100000 */
    AOS_FLAG_KICK_HIT = 1u << 19,      /* 0x80000: set by attack collisions */
    AOS_FLAG_ANIM_DONE = 1u << 21,     /* 0x200000: set by the animation player */
    AOS_FLAG_SLOW_FALL = 1u << 22,     /* 0x400000: sub_0801938C slows falls */
    AOS_FLAG_HEAD_SPECIAL = 1u << 23,  /* 0x800000: bit-3 cell at y - 25 */
    AOS_FLAG_BODY_SPECIAL = 1u << 24,  /* 0x1000000: bit-3 cell at y - 8 */
    AOS_FLAG_HEAVY = 1u << 26,         /* 0x4000000: weaker jump, +0.375 gravity */
    AOS_FLAG_SNAPPED = 1u << 27,       /* 0x8000000: floor snap this frame */
    AOS_FLAG_BACKDASH = 1u << 28,      /* 0x10000000 */
    AOS_FLAG_LOW_CEILING = 1u << 29    /* 0x20000000: ceiling at y - 33 */
};

/* Bits of gEwramData + 0x13260 read by these rules, named after their
 * effect here; what sets them (souls, items) is not traced. */
enum {
    AOS_ABILITY_SLOW_FALL = 0x100,
    AOS_ABILITY_FAST_WALK = 0x400,
    AOS_ABILITY_ZERO_FALL = 0x800,     /* sub_0801938C zeroes vy */
    AOS_ABILITY_FLOOR_MODE = 0x4000,   /* floor walks use collision mode 1 */
    AOS_ABILITY_SPECIAL_WALK = 0x8000  /* clears 0x800000, damps bit-3 entry */
};

/* Wall probe Y offsets (entity + 0x18 points to count, offsets). */
extern const int8_t aos_soma_stand_probes[]; /* 0x080E12DC: -12, -20, -28 */
extern const int8_t aos_soma_low_probes[];   /* 0x080E12EA: -6, -9 */
extern const int8_t aos_soma_ceiling_probes[];  /* 0x080E12E8: -12 */
extern const int8_t aos_soma_special_probes[];  /* 0x080E12E4: -6, -16, -28 */

typedef enum {
    AOS_LANDING_NONE,
    AOS_LANDING_NORMAL,     /* state 0 */
    AOS_LANDING_HARD        /* state 4, vy > 6.25 or flag 0x80 */
} AosLanding;

/* GBA buttons as stored in gEwramData + 0x1C (held) / + 0x1E (pressed). */
enum {
    AOS_KEY_RIGHT = 0x10,
    AOS_KEY_LEFT = 0x20,
    AOS_KEY_UP = 0x40,
    AOS_KEY_DOWN = 0x80,
    AOS_KEY_JUMP = 0x01,              /* A: default of gEwramData + 0x1339A */
    AOS_KEY_ATTACK = 0x02,            /* B: + 0x13398 */
    AOS_KEY_GUARDIAN = 0x100,         /* R: + 0x1339E */
    AOS_KEY_ABILITY = 0x200           /* L: + 0x1339C (backdash, high jump) */
};

/* Ability bits of gEwramData + 0x13396 tested with sub_08032AB8(bit). */
enum {
    AOS_MOVE_BACKDASH = 1u << 0,
    AOS_MOVE_SLIDE = 1u << 1,
    AOS_MOVE_AIR_JUMP = 1u << 2,      /* sub_080190E0 */
    AOS_MOVE_DIVE_KICK = 1u << 3,     /* sub_08017D90 */
    AOS_MOVE_HIGH_JUMP = 1u << 4
};

/* Equipped weapon as sub_08023368 returns it, with the body animations of
 * sub_080233BC (postures: stand, crouch, air, recover, crouch recover);
 * exported by scripts/aos_weapons.py. */
typedef struct {
    uint8_t weapon_class;   /* record + 8; class 5 cannot attack */
    uint16_t flags;         /* record + 0x10: elements in bits 0-5; 0x2000 keeps air
                             * attacks on landing */
    uint8_t anims[5];
    uint8_t interval;       /* record + 0x17: recent-hit cooldown (0x1307C) */
} AosWeapon;

enum { AOS_WEAPON_LANDING_ATTACK = 0x2000 };

typedef struct {
    int32_t x, y;           /* +0x40, +0x44 */
    int32_t vx, vy;         /* +0x48, +0x4C */
    int32_t extra_vx;       /* +0x2C, applied once */
    int32_t friction;       /* +0x50 */
    int32_t gravity_mod;    /* +0x54 */
    uint32_t flags;         /* +0x10 */
    uint16_t air_frames;    /* +0x14 */
    uint16_t drop_timer;    /* +0x16, skips landing while non-zero */
    uint8_t state;          /* +0x0A */
    uint8_t frame_counter;  /* +0x0D */
    uint8_t slope_contact;  /* +0x1C: floor slope ahead in the walk direction */
    uint8_t slope_step;     /* +0x1D: steepest slope byte >> 6 under the feet */
    bool facing_left;       /* +0x58 bit 0x40 */
    uint32_t abilities;     /* gEwramData + 0x13260 */
    uint32_t moves;         /* gEwramData + 0x13396, AOS_MOVE_* */
    const int8_t *wall_probes;  /* entity + 0x18; NULL means stand probes */
    int8_t air_probes[5];   /* gEwramData + 0x13218: count, offsets */
    uint16_t held, pressed; /* this frame's gEwramData + 0x1C / + 0x1E */
    uint16_t anim_request;  /* + 0x20: one-shot animation, 0xFF for none */
    uint8_t up_frames;      /* + 0x2A: frames with Up held while idle */
    bool air_anim_locked;   /* gEwramData + 0x131B8 & 4, cleared every frame */
    AosWeapon weapon;
    bool weapon_active;     /* gEwramData + 0x1311C: the weapon entity exists */
    AosBox hurtbox;         /* set by sub_080428B4 with each animation change */
    AosCombat combat;       /* + 0x70: type 1 */
    int16_t hp, max_hp;     /* gEwramData + 0x1327A / + 0x1327E */
    int16_t pending_damage; /* gEwramData + 0x131D6 */
    uint8_t pending_type;   /* + 0x131D4: 0 normal, 1 strong */
    int32_t pending_source_x;   /* + 0x131D8 */
    AosAnimState anim;
    const AosAnimSet *anims;    /* animation timings; NULL never ends one */
} AosSoma;

#define AOS_ANIM_NONE 0xFF

/* Soma's hurtboxes (sub_080428B4 arguments). */
extern const AosBox aos_soma_stand_box;   /* 0x080E12F8: -6, -32, 12 x 28 */
extern const AosBox aos_soma_low_box;     /* 0x080E12FC / 0x080E1300: -5, -16, 12 x 14 */
extern const AosBox aos_soma_slide_box;   /* 0x080E1304: -8, -12, 16 x 12 */

/* Soma animation ids requested by the ported code (descriptor indices). */
enum {
    AOS_SOMA_ANIM_IDLE = 0x00, AOS_SOMA_ANIM_WALK = 0x01,
    AOS_SOMA_ANIM_CROUCH = 0x02, AOS_SOMA_ANIM_FAST = 0x03,
    AOS_SOMA_ANIM_CROUCH_DOWN = 0x09, AOS_SOMA_ANIM_STAND_UP = 0x0A,
    AOS_SOMA_ANIM_JUMP_FORWARD = 0x0B, AOS_SOMA_ANIM_FALL = 0x0C,
    AOS_SOMA_ANIM_LAND = 0x0D, AOS_SOMA_ANIM_HARD_LANDING = 0x13,
    AOS_SOMA_ANIM_LOOK_UP = 0x17, AOS_SOMA_ANIM_TURN = 0x18,
    AOS_SOMA_ANIM_STOP = 0x19, AOS_SOMA_ANIM_WALK_START = 0x1A,
    AOS_SOMA_ANIM_HIGH_JUMP = 0x24, AOS_SOMA_ANIM_SPECIAL_TURN = 0x29,
    AOS_SOMA_ANIM_JUMP = 0x32, AOS_SOMA_ANIM_AIR_JUMP = 0x14,
    AOS_SOMA_ANIM_BACKDASH = 0x15, AOS_SOMA_ANIM_SLIDE = 0x1F,
    AOS_SOMA_ANIM_SLIDE_DOWNHILL = 0x2E, AOS_SOMA_ANIM_CEILING_CRASH = 0x25,
    AOS_SOMA_ANIM_DIVE_KICK = 0x26, AOS_SOMA_ANIM_DIVE_DROP = 0x27,
    AOS_SOMA_ANIM_HIT_FRONT = 0x0E, AOS_SOMA_ANIM_HIT_CROUCH = 0x0F,
    AOS_SOMA_ANIM_HIT_BACK = 0x10, AOS_SOMA_ANIM_KNOCKED = 0x11,
    AOS_SOMA_ANIM_DEATH = 0x33
};

/* Spawn state of sub_08014548: grounded, animation-end flag set, no
 * pending animation, idle animation started. */
AosSoma aos_soma_spawn(int32_t x, int32_t y, const AosAnimSet *anims);

void aos_soma_integrate(AosSoma *soma);
/* Steering of states 0 and 1: held direction or friction toward zero. */
void aos_soma_steer(AosSoma *soma, uint16_t held, int32_t speed);
/* sub_08019180: platform drop-through, normal jump with its three-frame
 * grace, and the flag 0x800000 jump; returns true when a jump starts. */
bool aos_soma_jump(AosSoma *soma, uint16_t held, uint16_t pressed);
/* sub_08018020 entry: walking off a ledge starts falling. */
void aos_soma_leave_ground(AosSoma *soma);
/* sub_0801938C rules followed by the sub_08018B98 gravity. A NULL layer
 * skips the ceiling bump. */
void aos_soma_air(AosSoma *soma, const AosCollision *layer, uint16_t held);
void aos_soma_gravity(AosSoma *soma, const AosCollision *layer);
/* sub_08021654 (normal mode): damage from an attack of power `attack`
 * against DEF `defence`, HP loss, and the pending hit the next update
 * reacts to (at 0 HP, the death of sub_0801AF20). Returns the damage. */
int aos_soma_take_hit(AosSoma *soma, int attack, int defence, int32_t source_x,
                      uint8_t knockback_type);
/* sub_08014A04 on the BG1 layer only. */
AosLanding aos_soma_collide(AosSoma *soma, const AosCollision *layer);
/* One player frame of sub_0801B0D8: animation-end flag, integration,
 * collision pass, state routine, wall probe selection, the pending
 * animation (+ 0x20) and the animation step. States 0 (normal, ground and
 * air), 1 (attack), 3 (slide), 4 (hard landing), 5 (high jump), 6
 * (ceiling crash), 7 (dive kick), 12 (hit), 13 (knockback) and 16 (death)
 * are ported; other states do nothing. */
AosLanding aos_soma_update(AosSoma *soma, const AosCollision *layer, uint16_t held,
                           uint16_t pressed);

#endif /* AOS_SOMA_H */
