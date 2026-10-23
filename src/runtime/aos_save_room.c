/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_save_room.h"

#include "aos_soma.h"

bool aos_save_room_player_inside(int x, int y) {
    return (unsigned)(x - 104) <= 32u && (unsigned)(y - 128) <= 55u;
}

bool aos_save_room_update(AosSaveRoom *state, bool present, int x, int y,
                          uint16_t held) {
    if (!state) return false;
    bool active = present && aos_save_room_player_inside(x, y) && (held & AOS_KEY_UP);
    if (!active) {
        state->input_latched = false;
        return false;
    }
    if (state->input_latched) return false;
    state->input_latched = true;
    return true;
}
