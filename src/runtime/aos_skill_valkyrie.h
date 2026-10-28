/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_VALKYRIE_H
#define AOS_SKILL_VALKYRIE_H
#include <stdbool.h>
#include <stdint.h>

/* SkillValkyrieUse's warrior, standing where it was summoned. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    bool facing_left;       /* + 0x58 bit 6: Soma's */
    uint8_t state;          /* + 0x0A: 1 striking, 2 blinking out */
    uint8_t timer;          /* + 0x0D: frames since the cast, then the blink */
    bool hidden;            /* + 0x58 bit 5 */
    bool blinking;          /* + 0x0F */
} AosValkyrie;

/* A sparkle of sub_08045B44(x, y, 0, ay, 0, 6) and the two offsets its
 * sub_08045000 draws (+ 0x1C, + 0x20). */
typedef struct {
    bool spawn;
    int32_t x, y, ay;
    int32_t offset_x, offset_y;
} AosValkyrieSpark;

/* State 0: placed by sub_08031EA4 at Soma's facing and (20, -16). */
AosValkyrie aos_valkyrie_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* One update before its animation step (`animation_ended` is that step's
 * result 3 of the previous update, applied first): striking, its counter
 * rises and from the 20th frame every fourth one throws a sparkle along a
 * diagonal (x + counter * 0.75 + 32 toward its facing, y + 56 - counter *
 * 31 / 16, each +/- 16 pixels at random, ay at random) and it is hidden
 * outside sub_08068AD4(e, -64, -8, 0x170, 0xB0); blinking, it counts 10
 * frames down and is deleted (false). */
bool aos_valkyrie_update(AosValkyrie *valkyrie, int cam_x, int cam_y, uint32_t (*random)(void),
                         AosValkyrieSpark *spark);
/* sub_0803F17C returned 3 outside state 2: state 2, 10 frames, blinking. */
void aos_valkyrie_animation_ended(AosValkyrie *valkyrie);
#endif
