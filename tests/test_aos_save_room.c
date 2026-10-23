/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_save_room.h"
#include "aos_soma.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosSaveRoom room = {0};

    assert(aos_save_room_player_inside(104, 128));
    assert(aos_save_room_player_inside(136, 183));
    assert(!aos_save_room_player_inside(103, 128));
    assert(!aos_save_room_player_inside(137, 128));
    assert(!aos_save_room_player_inside(104, 127));
    assert(!aos_save_room_player_inside(104, 184));

    assert(!aos_save_room_update(&room, false, 120, 150, AOS_KEY_UP));
    assert(!aos_save_room_update(&room, true, 120, 150, 0));
    assert(aos_save_room_update(&room, true, 120, 150, AOS_KEY_UP));
    assert(!aos_save_room_update(&room, true, 120, 150, AOS_KEY_UP));
    assert(!aos_save_room_update(&room, true, 120, 150, 0));
    assert(aos_save_room_update(&room, true, 120, 150, AOS_KEY_UP));
    assert(!aos_save_room_update(&room, true, 50, 150, AOS_KEY_UP));
    assert(aos_save_room_update(&room, true, 120, 150, AOS_KEY_UP));

    puts("Aria save-room interaction tests passed");
    return 0;
}
