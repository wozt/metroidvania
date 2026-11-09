/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_ARROW_H
#define AOS_SOUL_RED_ARROW_H
#include <stdbool.h>
#include <stdint.h>

/* The arrow of SkillSkullArcherUse's bow (sub_08028BC4). */
typedef struct {
    int32_t x;          /* + 0x40 (room pixels, 16.16) */
    int32_t vx;         /* + 0x48 */
    uint8_t state;      /* + 0x0A: 1 flying, 2 stuck and blinking */
    uint8_t timer;      /* the blink countdown of state 2 */
} AosArrow;

/* Its first update: frame 1, 6 pixels a frame toward the facing (sound
 * 0xC8 is not played). */
AosArrow aos_arrow_start(int32_t x, bool facing_left);
/* One update: state 1 moves it and, once its hit callback has run
 * (`struck`), stops it for 16 blinking frames (the stuck-arrow helper
 * sub_080239E4 is not ported); false when the blink is over and it ends. */
bool aos_arrow_update(AosArrow *arrow, bool struck);
#endif
