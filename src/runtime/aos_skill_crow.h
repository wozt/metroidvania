/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_CROW_H
#define AOS_SKILL_CROW_H
#include <stdbool.h>
#include <stdint.h>

enum { AOS_CROW_SEQUENCE = 2, AOS_CROW_FRAMES = 50 };

/* sub_SkillBlueCrowUse's crow, circling Soma. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int16_t angle, step;    /* + 0x2C, + 0x2E */
    int32_t radius;         /* + 0x24: whole pixels */
    uint8_t timer;          /* + 0x0D */
    uint8_t state;          /* + 0x0A: 0 created, 1 circling, 2 deleted next */
    bool facing_left;       /* + 0x58 bit 6: the opposite of Soma's */
} AosCrow;

/* State 0: the angle 0x1000 stepping -0x147 a frame (0x7000 and +0x147
 * when Soma faces left), 50 frames, facing away from Soma's facing. */
AosCrow aos_crow_create(bool soma_facing_left);
/* One update: state 1 sets the radius sin(timer * 0x28F) * 60 (whole
 * pixels toward zero), steps the angle and places the crow at Soma plus
 * (cos, sin) * radius, 18 pixels up, then counts down to state 2; state 2
 * deletes it (false). */
bool aos_crow_update(AosCrow *crow, int32_t soma_x, int32_t soma_y);
#endif
