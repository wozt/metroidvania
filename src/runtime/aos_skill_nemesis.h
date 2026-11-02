/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_NEMESIS_H
#define AOS_SKILL_NEMESIS_H
#include <stdbool.h>
#include <stdint.h>

/* SkillNemesisUse (0x080251F0): sequence 2 of skill resource 21 rising
 * over Soma while the enemies have no target (gEwramData + 0x13168 = 0)
 * for 300 frames, then 30 frames of blinking. It has no attack. */
enum { AOS_NEMESIS_SOUL_FRAMES = 0x12C, AOS_NEMESIS_SOUL_BLINK = 0x1E,
       AOS_NEMESIS_SOUL_SEQUENCE = 2 };

typedef struct {
    uint8_t state;          /* + 0x0A: 1 shown, 2 blinking */
    int16_t timer;          /* + 0x24 */
    uint8_t blink;          /* + 0x0D */
    uint8_t rise;           /* + 0x0B */
    bool blinking;          /* + 0x0F */
} AosNemesisSoul;

/* State 0: 300 frames (sound 0x97, the count, sequence 2 and the cleared
 * target belong to the caller). */
AosNemesisSoul aos_nemesis_soul_create(void);
/* One update: the timer runs out, then the blink; the rise grows by one a
 * frame up to 30. Returns false when it is deleted (the caller gives
 * 0x13168 back to Soma). */
bool aos_nemesis_soul_update(AosNemesisSoul *soul);
/* sub_08031EA4's offset from Soma: (0, -8 - rise). */
int aos_nemesis_soul_dy(const AosNemesisSoul *soul);
#endif
