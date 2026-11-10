/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria save-room interaction extracted from Object1CUpdate. */
#ifndef AOS_SAVE_ROOM_H
#define AOS_SAVE_ROOM_H

#include <stdbool.h>
#include <stdint.h>

enum {
    AOS_ROOM_FLAG_SAVE = 1u << 0,
    AOS_ROOM_FLAG_WARP = 1u << 1,
    AOS_OBJECT_SAVE_POINT = 0x1C
};

typedef struct {
    bool input_latched;
} AosSaveRoom;

/* sub_08068AD4(Object1CUpdate): unsigned bounds from (104, 128), 32 x 55. */
bool aos_save_room_player_inside(int x, int y);

/* Returns true once when Up activates a present save point. Releasing Up or
 * leaving its native rectangle rearms the interaction. */
bool aos_save_room_update(AosSaveRoom *state, bool present, int x, int y,
                          uint16_t held);

#endif /* AOS_SAVE_ROOM_H */
