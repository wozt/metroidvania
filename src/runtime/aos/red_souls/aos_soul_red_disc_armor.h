/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_DISC_ARMOR_H
#define AOS_SOUL_RED_DISC_ARMOR_H
#include <stdbool.h>
#include <stdint.h>

/* SkillDiscArmorUse's orbiting saw disc: state 1 anchors it at the
 * source position (+ 0x50/+ 0x54 = source +0x40/+0x44 - 0x18000) and
 * each frame computes a |sin(t * 1260)| * 60 pixel orbit radius and a
 * + 0x2C spin angle stepped by +/- 0x276; + 0x0D = 0x1A frames of life
 * then state 2 deletes. The sub_08023530(e, sub_08027140) partner
 * registration, the + 0x30 spin timer, the sub_0803E654 variant pick,
 * the sub_0803E058/+ 0x58 visual and the + 0x59/+ 0x72 flag bookkeeping
 * are not ported. */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 orbit, 2 delete */
    uint8_t timer;      /* + 0x0D: 0x1A orbit frames */
    int32_t angle;      /* + 0x2C: 0x2000/0x6000 start */
    int32_t step;       /* + 0x2E: -0x276, +0x276 when mirrored */
    int32_t anchor_x;   /* + 0x50: source x (16.16) */
    int32_t anchor_y;   /* + 0x54: source y - 0x18000 (16.16) */
    int32_t x;          /* + 0x40: orbit position out (16.16) */
    int32_t y;          /* + 0x44 */
} AosDiscArmor;

/* State 0 loads the entry-0x2A record, + 0x65 = 1, + 0x5C = 0xD,
 * + 0x59 |= 0x12, + 0x58 = variant | source facing bit 6, + 0x2C =
 * 0x2000 (0x6000 mirrored) with + 0x2E = -+0x276, + 0x30 rate 0x1B00,
 * box preset 5 plus 0x2020F0F0, song 0xD4 and enters state 1 which
 * runs on the same frame. `mirrored` is the source + 0x58 bit 6. */
AosDiscArmor aos_disc_armor_create(int32_t source_x, int32_t source_y,
                                   bool mirrored);
/* One update; returns false once deleted (state 2 deletes on the
 * frame after the orbit ends, like the native EntityDelete case). */
bool aos_disc_armor_update(AosDiscArmor *disc);
#endif
