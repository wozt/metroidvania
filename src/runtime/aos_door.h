/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow wooden door (special object 0x00).
 *
 * Ports of cvaos Object00Create (0x0804D9DC) and Object00Update
 * (0x0804DAB4): the door blocks three 16x16 collision blocks above its
 * base, opens when Soma touches it while facing it, then walks Soma
 * through with forced input; a door Soma enters through is open, walks him
 * out and closes behind him. Graphics (sub_0804D8F0 loads 0x081CBE0C,
 * 0x08209AE0 and 0x0820F160), the enemy pause (sub_0800C5A8), the
 * gEwramData + 0x42C flags and the off-screen cleanup state 0x63 are not
 * modelled. No SDL. */
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
} AosDoor;

/* Object00Create; camera_x is the BG1 X used for the facing test. */
void aos_door_create(AosDoor *door, AosCollision *layer, int32_t x, int32_t y,
                     const AosSoma *soma, int32_t camera_x, AosForcedInput *input);
/* Object00Update; returns the sound to play (0 for none). */
int aos_door_update(AosDoor *door, AosCollision *layer, const AosSoma *soma,
                    AosForcedInput *input);

#endif /* AOS_DOOR_H */
