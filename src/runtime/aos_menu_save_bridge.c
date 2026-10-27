/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_save_bridge.h"

static int logical_slot(uint8_t native_slot) {
    if (native_slot >= 6) native_slot = (uint8_t)(native_slot - 6);
    if (native_slot >= 6 || (native_slot & 1u)) return -1;
    return native_slot / 2;
}

void aos_menu_save_bridge_init(AosMenuSaveBridge *bridge) {
    *bridge = (AosMenuSaveBridge){.pending_slot = -1, .launch_slot = -1};
}

void aos_menu_save_bridge_sync(AosMenuSaveBridge *bridge, AosMainMenu *menu) {
    menu->slot_present_mask = 0;
    menu->special_slot_mask = 0;
    menu->slot_mode_choice_mask = 0;
    menu->slot_intro_mask = 0;
    for (unsigned slot = 0; slot < AOS_SAVE_SLOT_COUNT; ++slot)
        if (bridge->present[slot])
            menu->slot_present_mask |= (uint8_t)(1u << (slot * 2));
}

bool aos_menu_save_bridge_consume(AosMenuSaveBridge *bridge, AosMainMenu *menu) {
    bridge->changed_mask = 0;
    bridge->removed_mask = 0;
    bridge->launch_slot = -1;
    bridge->start = AOS_PORTABLE_START_NONE;

    if (menu->load_slot_requested) {
        int slot = logical_slot(menu->requested_slot);
        if (slot < 0) return false;
        bridge->pending_slot = (int8_t)slot;
    }

    if (menu->copy_slots_requested) {
        int source = logical_slot(menu->source_slot);
        int destination = logical_slot(menu->destination_slot);
        if (source < 0 || destination < 0 || !bridge->present[source]) return false;
        bridge->slots[destination] = bridge->slots[source];
        bridge->slots[destination].slot = (uint8_t)destination;
        bridge->present[destination] = true;
        bridge->changed_mask |= (uint8_t)(1u << destination);
    }

    for (unsigned slot = 0; slot < AOS_SAVE_SLOT_COUNT; ++slot) {
        if (!(menu->delete_slots_mask & (1u << (slot * 2)))) continue;
        bridge->present[slot] = false;
        bridge->removed_mask |= (uint8_t)(1u << slot);
        if (bridge->pending_slot == (int8_t)slot) bridge->pending_slot = -1;
    }

    if (menu->name_committed) {
        if (bridge->pending_slot < 0) return false;
        unsigned slot = (unsigned)bridge->pending_slot;
        aos_save_init_new(&bridge->slots[slot], slot);
        for (unsigned i = 0; i < 8; ++i)
            bridge->slots[slot].name[i] = menu->committed_name[i];
        bridge->slots[slot].character = menu->current_character;
        bridge->slots[slot].challenge_flags = menu->name_unlock_flags;
        bridge->present[slot] = true;
        bridge->changed_mask |= (uint8_t)(1u << slot);
    }

    if (menu->load_game_requested || menu->intro_requested) {
        if (bridge->pending_slot < 0 ||
            !bridge->present[(unsigned)bridge->pending_slot])
            return false;
        bridge->launch_slot = bridge->pending_slot;
        bridge->start = menu->intro_requested ? AOS_PORTABLE_START_INTRO
                                             : AOS_PORTABLE_START_GAME;
    }

    aos_menu_save_bridge_sync(bridge, menu);
    return true;
}
