/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow wooden door (special object 0x00).
 *
 * Ports of cvaos Object00Create (0x0804D9DC) and Object00Update
 * (0x0804DAB4): the door blocks three 16x16 collision blocks above its
 * base, opens when Soma touches it while facing it, then walks Soma
 * through with forced input; a door Soma enters through is open, walks him
 * out and closes behind him; it is hidden outside the screen widened by 32
 * pixels left and right and 48 below. While Soma passes, the door holds
 * gEwramData + 0x42C bits 0x101 and clears gEwramData + 0xA074 bit 1
 * (sub_0800C5A8(0)); the runtime exposes this as `holding` (the readers of
 * both globals are not traced). Graphics (sub_0804D8F0 loads 0x081CBE0C,
 * 0x08209AE0 and 0x0820F160) are exported separately; the cleanup state
 * 0x63, entered when + 0x59 bit 2 is clear (its setter is not traced), is
 * not modelled. No SDL. */
#ifndef AOS_DOOR_H
#define AOS_DOOR_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_collision.h"
#include "aos_soma.h"

/* SetPlayerInput(1, keys): replaces the player's input for a frame. */
typedef struct {
    bool active;
    uint16_t held;
} AosForcedInput;

typedef struct {
    int32_t x, y;           /* room pixels of the door base */
    uint8_t state;          /* + 0x0A: 0 closed, 1 opening, 2 open, 10 exit, 11 closing */
    bool facing_left;       /* + 0x58 bit 0x40: right half of the screen */
    int32_t swing;          /* + 0x14: 0 closed .. 0x4000 open */
    bool hidden;            /* + 0x58 bit 0x20 */
    bool holding;           /* 0x42C |= 0x101 and 0xA074 bit 1 cleared */
} AosDoor;

/* Object00Create; camera_x is the BG1 X used for the facing test. */
void aos_door_create(AosDoor *door, AosCollision *layer, int32_t x, int32_t y,
                     const AosSoma *soma, int32_t camera_x, AosForcedInput *input);
/* Object00Update with the camera (BG1) position; returns the sound to play
 * (0 for none). */
int aos_door_update(AosDoor *door, AosCollision *layer, const AosSoma *soma,
                    AosForcedInput *input, int32_t camera_x, int32_t camera_y);

#endif /* AOS_DOOR_H */
