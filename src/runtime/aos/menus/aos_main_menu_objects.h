/* SPDX-License-Identifier: GPL-3.0-only */
/* The main menu's object graphics (sub_08006BC0): the three sheets of
 * entity 0 for a language (the icons, cursor and ornament sheet, the name
 * font, the PLAY / COPY / DELETE labels) with their frame descriptors
 * 0x082571E0, 0x08259120 and 0x08267A38, and the OBJ palette of
 * sub_0803C74C(0x080E5E24). No SDL. */
#ifndef AOS_MAIN_MENU_OBJECTS_H
#define AOS_MAIN_MENU_OBJECTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_main_menu_sprites.h"
#include "fusion/assets/rom.h"

typedef struct {
    AosObjSheet sheets[AOS_MENU_SHEETS];
    uint16_t palette[256];   /* the OBJ palette, 15-bit colours */
} AosMainMenuObjects;

/* `language` as the save's (1 English, 2, 3; others English). */
bool aos_main_menu_objects_init(AosMainMenuObjects *objects, const FusionRom *rom,
                                uint8_t language, char *error, size_t error_size);

#endif
