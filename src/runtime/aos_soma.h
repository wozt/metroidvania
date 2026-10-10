/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow Soma motion rules.
 *
 * Ports of verified parts of cvaos asm/code/code_08014548.s (see
 * docs/AOS_SOMA.md): the position integration at the start of the player
 * update sub_0801B0D8, ground and air steering with friction, the jump
 * routine sub_08019180 (normal jump only), the air rules of sub_0801938C
 * (jump release, apex float, slowed fall), the ledge start of sub_08018020
 * and the gravity of sub_08018B98. Collision response (sub_08014A04,
 * sub_08018020) and every other state are NOT ported. Values are 16.16
 * fixed point pixels per 60 Hz frame, positive Y downward. No SDL. */
#ifndef AOS_SOMA_H
#define AOS_SOMA_H

#include <stdbool.h>
#include <stdint.h>

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
    AOS_FLAG_SLOW_FALL = 1u << 22,     /* 0x400000: sub_0801938C slows falls */
    AOS_FLAG_HEAVY = 1u << 26          /* 0x4000000: weaker jump, +0.375 gravity */
};

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
    bool facing_left;       /* +0x58 bit 0x40 */
} AosSoma;

void aos_soma_integrate(AosSoma *soma);
/* Steering of states 0 and 1: held direction or friction toward zero. */
void aos_soma_steer(AosSoma *soma, uint16_t held, int32_t speed);
/* sub_08019180 normal jump; returns true when the jump starts. */
bool aos_soma_jump(AosSoma *soma, uint16_t pressed);
/* sub_08018020 entry: walking off a ledge starts falling. */
void aos_soma_leave_ground(AosSoma *soma);
/* sub_0801938C rules followed by the sub_08018B98 gravity. */
void aos_soma_air(AosSoma *soma, uint16_t held, bool slow_fall);
void aos_soma_gravity(AosSoma *soma);

#endif /* AOS_SOMA_H */
