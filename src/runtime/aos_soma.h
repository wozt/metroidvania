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
 * and landing). Moving platforms and carried entities (gEwramData + 0x1316C,
 * + 0x131B4), the player state routines and the animations are NOT ported.
 * Values are 16.16 fixed point pixels per 60 Hz frame, positive Y downward;
 * positions are room pixels. No SDL. */
#ifndef AOS_SOMA_H
#define AOS_SOMA_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_collision.h"

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
    AOS_FLAG_STOP_AT_WALL = 1u << 7,   /* 0x80: walls stop instead of bounce */
    AOS_FLAG_PLATFORM_ONLY = 1u << 12, /* 0x1000: no solid cell under the feet */
    AOS_FLAG_SLOPE_LEFT = 1u << 13,    /* 0x2000: slope byte without bit 2 */
    AOS_FLAG_SLOPE_RIGHT = 1u << 14,   /* 0x4000: slope byte with bit 2 */
    AOS_FLAG_HEAD_CEILING = 1u << 15,  /* 0x8000: ceiling at x +/- 5, y - 20 */
    AOS_FLAG_HARD_LANDING = 1u << 16,  /* 0x10000 */
    AOS_FLAG_WALL = 1u << 18,          /* 0x40000: a wall pushed Soma */
    AOS_FLAG_GROUNDED = 1u << 20,      /* 0x100000 */
    AOS_FLAG_SLOW_FALL = 1u << 22,     /* 0x400000: sub_0801938C slows falls */
    AOS_FLAG_HEAD_SPECIAL = 1u << 23,  /* 0x800000: bit-3 cell at y - 25 */
    AOS_FLAG_BODY_SPECIAL = 1u << 24,  /* 0x1000000: bit-3 cell at y - 8 */
    AOS_FLAG_HEAVY = 1u << 26,         /* 0x4000000: weaker jump, +0.375 gravity */
    AOS_FLAG_SNAPPED = 1u << 27,       /* 0x8000000: floor snap this frame */
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

typedef enum {
    AOS_LANDING_NONE,
    AOS_LANDING_NORMAL,     /* state 0 */
    AOS_LANDING_HARD        /* state 4, vy > 6.25 or flag 0x80 */
} AosLanding;

/* GBA buttons as stored in gEwramData + 0x1C (held) / + 0x1E (pressed). */
enum {
    AOS_KEY_RIGHT = 0x10,
    AOS_KEY_LEFT = 0x20,
    AOS_KEY_JUMP = 0x01               /* default A; the game reads 0x1339A */
};

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
    uint8_t slope_contact;  /* +0x1C: floor slope ahead in the walk direction */
    uint8_t slope_step;     /* +0x1D: steepest slope byte >> 6 under the feet */
    bool facing_left;       /* +0x58 bit 0x40 */
    uint32_t abilities;     /* gEwramData + 0x13260 */
    const int8_t *wall_probes;  /* entity + 0x18; NULL means stand probes */
} AosSoma;

void aos_soma_integrate(AosSoma *soma);
/* Steering of states 0 and 1: held direction or friction toward zero. */
void aos_soma_steer(AosSoma *soma, uint16_t held, int32_t speed);
/* sub_08019180 normal jump; returns true when the jump starts. */
bool aos_soma_jump(AosSoma *soma, uint16_t pressed);
/* sub_08018020 entry: walking off a ledge starts falling. */
void aos_soma_leave_ground(AosSoma *soma);
/* sub_0801938C rules followed by the sub_08018B98 gravity. A NULL layer
 * skips the ceiling bump. */
void aos_soma_air(AosSoma *soma, const AosCollision *layer, uint16_t held);
void aos_soma_gravity(AosSoma *soma, const AosCollision *layer);
/* sub_08014A04 on the BG1 layer only. */
AosLanding aos_soma_collide(AosSoma *soma, const AosCollision *layer);

#endif /* AOS_SOMA_H */
