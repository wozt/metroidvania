/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_save_bridge.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosMenuSaveBridge bridge;
    AosMainMenu menu = {0};
    aos_menu_save_bridge_init(&bridge);
    bridge.present[0] = true;
    aos_save_init_new(&bridge.slots[0], 0);
    bridge.slots[0].area = 4;
    aos_menu_save_bridge_sync(&bridge, &menu);
    assert(menu.slot_present_mask == 1);

    menu = (AosMainMenu){.copy_slots_requested = true, .source_slot = 0,
                         .destination_slot = 4};
    assert(aos_menu_save_bridge_consume(&bridge, &menu));
    assert(bridge.present[2] && bridge.slots[2].slot == 2 &&
           bridge.slots[2].area == 4 && bridge.changed_mask == (1u << 2));

    menu = (AosMainMenu){.delete_slots_mask = 1u << 4};
    assert(aos_menu_save_bridge_consume(&bridge, &menu));
    assert(!bridge.present[2] && bridge.removed_mask == (1u << 2));

    menu = (AosMainMenu){.load_slot_requested = true, .requested_slot = 2};
    assert(aos_menu_save_bridge_consume(&bridge, &menu));
    assert(bridge.pending_slot == 1);
    menu = (AosMainMenu){.name_committed = true, .intro_requested = true,
                         .committed_name = {0x0B, 0x16, 0x0D, 0x0A,
                                            0x16, 0x14, 0, 0},
                         .current_character = 1,
                         .name_unlock_flags = 0x100000};
    assert(aos_menu_save_bridge_consume(&bridge, &menu));
    assert(bridge.present[1] && bridge.changed_mask == (1u << 1) &&
           bridge.slots[1].name[0] == 0x0B && bridge.slots[1].character == 1 &&
           bridge.slots[1].challenge_flags == 0x100000 &&
           bridge.start == AOS_PORTABLE_START_INTRO && bridge.launch_slot == 1);

    menu = (AosMainMenu){.load_slot_requested = true, .load_game_requested = true,
                         .requested_slot = 0};
    assert(aos_menu_save_bridge_consume(&bridge, &menu));
    assert(bridge.start == AOS_PORTABLE_START_GAME && bridge.launch_slot == 0);

    puts("Aria main-menu save bridge tests passed");
    return 0;
}
