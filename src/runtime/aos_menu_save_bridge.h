/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MENU_SAVE_BRIDGE_H
#define AOS_MENU_SAVE_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>

#include "aos_main_menu.h"
#include "aos_save.h"

typedef enum {
    AOS_PORTABLE_START_NONE,
    AOS_PORTABLE_START_GAME,
    AOS_PORTABLE_START_INTRO,
} AosPortableStart;

/* Host-side mapping between the native six-entry file/quicksave controller
 * and the three portable save files. Odd native quicksave entries currently
 * have no portable counterpart. */
typedef struct {
    AosSaveData slots[AOS_SAVE_SLOT_COUNT];
    bool present[AOS_SAVE_SLOT_COUNT];
    int8_t pending_slot;
    int8_t launch_slot;
    uint8_t changed_mask;
    uint8_t removed_mask;
    AosPortableStart start;
} AosMenuSaveBridge;

void aos_menu_save_bridge_init(AosMenuSaveBridge *bridge);
void aos_menu_save_bridge_sync(AosMenuSaveBridge *bridge, AosMainMenu *menu);
bool aos_menu_save_bridge_consume(AosMenuSaveBridge *bridge, AosMainMenu *menu);

#endif
