/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_SKULL_H
#define AOS_SKILL_SKULL_H
#include <stdbool.h>
#include <stdint.h>

enum { AOS_SKULL_BEAM_SOUND = 0xCE };

/* SkillBeamSkeletonUse's skull, held beside Soma. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int16_t dx, dy;         /* + 0x4A, + 0x4E: whole-pixel offsets from Soma */
    bool facing_left;       /* + 0x58 bit 6: Soma's */
    uint8_t state;          /* + 0x0A: 1 shown, 2 deleted next */
} AosSkull;

/* State 0: Soma's facing, 20 pixels ahead of him and 16 up. */
AosSkull aos_skull_create(bool soma_facing_left);
/* One update before its animation step: state 1 keeps it at Soma's whole
 * position plus its offsets; state 2 deletes it (false) after releasing
 * the beam's held sound. `frame` and `tick` are its animation's before
 * the step: at frame 10, tick 0, the beam sound 0xCE is held
 * (sub_08013D60). */
bool aos_skull_update(AosSkull *skull, int32_t soma_x, int32_t soma_y, uint8_t frame,
                      uint8_t tick);
/* The animation ended (sub_0803F17C returned 3): deleted on the next update. */
void aos_skull_end(AosSkull *skull);
#endif
