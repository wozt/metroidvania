#ifndef AOS_OBJECT19_H
#define AOS_OBJECT19_H
#include <stdbool.h>
#include <stdint.h>

/* Object19Create (0x08053BFC) / Object19Update (0x08053D10),
 * paired: a patrolling mover drawn through the sub_0803E058
 * affine rotation. The sub_08068AD4 probe (-0x10, 0, 0xB0, 0x110)
 * miss sets + 0x58 bit 0x20 and skips the update; near clears
 * bits 0 and 5 and advances the + 0x14 angle by + 0x16.
 *
 * Create stores + 0x30 = param0 + 0xE, collapses + 0x32 to
 * param1 bit 0 (the + 0x58 bit-6 mark, the + 0x16 negate and
 * the leg vx negate), fixes + 0x16 = 0x400 (negated on the
 * mirror), installs the sub_0804277C(8) hurt slot and the
 * 0x0C0CFAFA presets, rolls the sub_0803E654 palette slot into
 * + 0x58 bits 0-4, sets + 0x5C = 0xA and + 0x59 bits 0x10/2,
 * then arms the + 0xA-indexed 0x0852646C record with the
 * + 0x18 lifetime shifted right once.
 *
 * The + 0xA state machine walks a four-leg rectangle: each
 * state's expiry arms the NEXT 0x0852646C record — state 0
 * retreats right param0 frames at +1.0, state 1 descends
 * 0x160000 (22.0), state 2 advances left param0 frames and
 * state 3 ascends 22.0, looping. A zero record lifetime falls
 * back to param0 << 16; the collapsed mirror negates the
 * horizontal velocity. + 0x18 counts the leg down by 0x10000
 * a frame and + 0x40/+ 0x44 integrate the velocity.
 *
 * Every near update also ends in sub_0803E058(+ 0x14, 0x10000,
 * 0x10000, + 0x58 bit 6, ...): the sprite turns by the + 0x14
 * angle at unit scale, horizontally flipped on the mirror —
 * draw_special_objects applies that rotation. */
typedef struct {
    int32_t param0;         /* +0x30 s16: param0 + 0xE, the
                             * horizontal leg length */
    uint8_t mirror;         /* +0x32 collapsed to param1 bit 0 */
    uint8_t state;          /* +0xA: leg index 0-3 */
    int32_t lifetime;       /* +0x18: fixed-point leg timer */
    int32_t vx, vy;         /* +0x48/+0x4C: leg velocity */
    int32_t fx, fy;         /* +0x40/+0x44: fixed-point position */
    uint16_t angle;         /* +0x14: += +0x16 each update */
    uint16_t angle_rate;    /* +0x16: 0x400, negated on mirror */
    uint8_t flags58;        /* +0x58: bit 0x20 probe-miss mark,
                             * bit 0x40 mirror mark */
} AosObject19;

void aos_object19_create(AosObject19 *o, int param0, int param1,
                         int32_t fx, int32_t fy);
void aos_object19_update(AosObject19 *o, bool near);
#endif
