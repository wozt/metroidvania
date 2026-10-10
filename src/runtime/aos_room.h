/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow room exits.
 *
 * Ports of cvaos sub_08011A44 (the player left the room bounds),
 * sub_08010244 (velocity adjustments when the transition starts) and
 * sub_08010350 (choice of the transition entry by the screen Soma leaves
 * through, and his position in the target room). Coordinates are room
 * pixels as GetEntityRoomXPositionInteger returns them. The fade and the
 * camera scroll are not modelled. No SDL. */
#ifndef AOS_ROOM_H
#define AOS_ROOM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_collision.h"
#include "aos_soma.h"

#define AOS_EXIT_CELL 0xF0

typedef struct {
    int8_t screen_x, screen_y;  /* + 4, + 5: screen left, relative to the room */
    int16_t adjust_x;           /* + 6, added to the arrival X on screen */
    uint16_t load_x, load_y;    /* + 0xA, + 0xC: BG1 position in the target */
    uint8_t area, room;         /* resolved target of the pointer at + 0 */
} AosTransition;

/* sub_08011A44 with HP > 0: true when (x, y) is outside the room or on an
 * exit cell (BG1 byte 0xF0, read through sub_08001FE8); layer may be NULL. */
bool aos_room_outside(const AosCollision *layer, int width_screens, int height_screens,
                      int32_t x, int32_t y);
/* sub_08010244: stop motion back into the room and slow fast rises;
 * screen_x / screen_y are Soma's position on screen. */
void aos_room_exit_velocity(AosSoma *soma, int screen_x, int screen_y);
/* sub_08010350: the entry for the screen Soma leaves through, or NULL. */
const AosTransition *aos_room_find_exit(const AosTransition *list, size_t count,
                                        int width_screens, int height_screens,
                                        int32_t x, int32_t y);
/* Arrival position in the target room of the given dimensions. */
void aos_room_arrival(const AosTransition *exit, int target_width_screens,
                      int target_height_screens, int32_t x, int32_t y,
                      int32_t *out_x, int32_t *out_y);

#endif /* AOS_ROOM_H */
