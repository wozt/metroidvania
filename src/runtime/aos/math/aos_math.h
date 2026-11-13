/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow fixed-point trigonometry shared by the enemies, the souls
 * and the familiars: the ROM sine sub_080009E4 (sSineTable), the BIOS
 * ArcTan2 and Sqrt, and the 16.16 product sub_0803E86C. No SDL. */
#ifndef AOS_MATH_H
#define AOS_MATH_H

#include <stdbool.h>
#include <stdint.h>

/* sub_080009E4: sine of a 16-bit angle (0x10000 per turn), 16.16. */
int32_t aos_sine(uint32_t angle);
/* ArcTan2 (BIOS call 0x0A): the angle of (x, y), 0x10000 per turn. */
uint16_t aos_arctan2(int16_t x, int16_t y);

/* Sqrt (BIOS call 0x08): the integer square root, rounded down. */
uint32_t aos_isqrt(uint32_t x);
/* sub_0803E86C: a 16.16 product from the operands' top 24 bits, rounded
 * toward zero to a multiple of 0x100. */
int32_t aos_fixed_mul(int32_t a, int32_t b);

#endif

/* Entity-space helpers from code_08060B98.s, all working on
 * plain coordinates so callers pass the entity's + 0x40/+ 0x44
 * fine positions or + 0x42/+ 0x46 whole positions. */

/* sub_08068AD4: unsigned box probe — true when
 * 0 <= ex - x <= w and 0 <= ey - y <= h (the ROM compares the
 * low 16 bits shifted, so negatives fail the upper bound). */
bool aos_probe_box(int16_t ex, int16_t ey, int16_t x, int16_t y,
                   int16_t w, int16_t h);

/* sub_08068A70: ArcTan2 of the s16 deltas between two
 * + 0x42/+ 0x46 positions, returned as a 16-bit angle. */
uint16_t aos_entity_angle(int16_t ax, int16_t ay,
                          int16_t bx, int16_t by);

/* sub_08068AA0/AB4/ABC/AC8: phase complements —
 * 0x8000 - v, -v, 0xF00000 - v and 0xA00000 - v. */
int32_t aos_phase_opposite(int32_t v);
int32_t aos_phase_negate(int32_t v);
int32_t aos_phase_comp_f0(int32_t v);
int32_t aos_phase_comp_a0(int32_t v);

/* sub_08068B14/44: sign of (v - ref) as -1/0/1. */
int aos_sign_delta(int32_t v, int32_t ref);

/* sub_08068B74: squared-distance probe — the entity's + 0x40/
 * + 0x44 (16.16) against the reference point, true when the
 * whole-pixel distance is <= range (range << 16 compared). */
bool aos_within_range(int32_t ex, int32_t ey,
                      int32_t rx, int32_t ry, int32_t range);

/* sub_08068BC0: sign of (a - b) on s16 inputs: -1/0/1. */
int aos_s16_sign(int16_t a, int16_t b);

/* sub_08068BF0: sine steering — moves (px, py) toward (tx, ty)
 * using the sub_080009E4 lookups exactly like the ROM:
 * cross/dot products of the delta pick the turn direction.
 * `angle` is the entity facing in 16.16; returns the new
 * position through px/py. */
void aos_steer_toward(int32_t *px, int32_t *py,
                      int32_t tx, int32_t ty, uint32_t angle);

/* sub_08068C74/68CA8: clamped approach step — the ROM computes
 * cur + step * scale, clamps the result between cur and bound,
 * adds it to the entity's + 0x40 (X) or + 0x44 (Y) fine
 * position and returns it. This helper returns the clamped
 * value; the caller adds it to the position. */
int32_t aos_approach_step(int32_t cur, int32_t bound,
                          int32_t step, int32_t scale);

/* sub_08068CDC: advance along a heading by a clamped step —
 * dist = clamp(cur + step * scale, cur, bound) then the fine
 * position moves dist >> 8 along (sine(angle + 0x4000),
 * sine(angle)) exactly like the ROM's two sub_080009E4 lookups.
 * Returns the clamped distance. */
int32_t aos_advance_along(int32_t *px, int32_t *py, uint16_t angle,
                          int32_t cur, int32_t bound,
                          int32_t step, int32_t scale);
