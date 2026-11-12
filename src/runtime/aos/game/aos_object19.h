#ifndef AOS_OBJECT19_H
#define AOS_OBJECT19_H
#include <stdbool.h>
#include <stdint.h>

/* Object19Update (0x08053D10), paired with the already-covered
 * Object19Create: a patrolling mover drawn through the sub_0803E058
 * affine rotation. The sub_08068AD4 probe (-0x10, 0, 0xB0, 0x110)
 * miss sets + 0x58 bit 0x20 and skips the update; near clears bits
 * 0 and 5 and advances the + 0x14 angle by + 0x16.
 *
 * The + 0xA state machine walks a four-leg rectangle: each state's
 * expiry arms the next leg from the 0x0852646C records — state 0
 * retreats right param0 frames at +1.0, state 1 descends 0x160000
 * (22.0), state 2 advances left param0 frames and state 3 ascends
 * 22.0, looping. A zero record lifetime falls back to param0 << 16;
 * param1 (+ 0x32) negates the horizontal velocity. + 0x18 counts
 * the leg down by 0x10000 a frame and + 0x40/+ 0x44 integrate the
 * velocity. */
typedef struct {
    int32_t param0;         /* +0x30 s16: horizontal leg length */
    int32_t param1;         /* +0x32 s16: horizontal mirror */
    uint8_t state;          /* +0xA: leg index 0-3 */
    int32_t lifetime;       /* +0x18: fixed-point leg timer */
    int32_t vx, vy;         /* +0x48/+0x4C: leg velocity */
    int32_t fx, fy;         /* +0x40/+0x44: fixed-point position */
    uint16_t angle;         /* +0x14: += +0x16 each update */
    uint16_t angle_rate;    /* +0x16 */
    uint8_t flags58;        /* +0x58: bit 0x20 probe-miss mark */
} AosObject19;

void aos_object19_create(AosObject19 *o, int param0, int param1,
                         int32_t fx, int32_t fy, uint16_t angle_rate);
void aos_object19_update(AosObject19 *o, bool near);
#endif
