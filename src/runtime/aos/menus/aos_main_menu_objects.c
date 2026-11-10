/* SPDX-License-Identifier: GPL-3.0-only */
/* The main menu's object graphics: see aos_main_menu_objects.h. */
#include "aos_main_menu_objects.h"

#include "../../aos/game/aos_obj_rom.h"
#include "../core/aos_rom_graphics.h"

#include <stdio.h>
#include <string.h>

/* sub_08006BC0's resources by language (default, 2, 3). */
static const uint32_t sheet_graphics[3][AOS_MENU_SHEETS] = {
    {0x082052D8, 0x082052E0, 0x080E5BC0},
    {0x0827798C, 0x0827AA14, 0x08277994},
    {0x0826C840, 0x0827AA0C, 0x0826C850},
};
static const uint32_t sheet_frames[AOS_MENU_SHEETS] = {0x082571E0, 0x08259120, 0x08267A38};

bool aos_main_menu_objects_init(AosMainMenuObjects *objects, const FusionRom *rom,
                                uint8_t language, char *error, size_t error_size) {
    if (!objects || !rom || rom->game != FUSION_ASSET_GAME_ARIA) {
        if (error && error_size) snprintf(error, error_size, "invalid Aria menu objects request");
        return false;
    }
    memset(objects, 0, sizeof *objects);
    unsigned set = language == 2 ? 1 : language == 3 ? 2 : 0;
    for (unsigned k = 0; k < AOS_MENU_SHEETS; ++k)
        if (!aos_obj_sheet_decode(&objects->sheets[k], rom, sheet_graphics[set][k], sheet_frames[k],
                                  error, error_size))
            return false;
    uint8_t palette[0x200] = {0};
    if (!aos_rom_load_palette_resource(rom, 0x080E5E24, palette, error, error_size)) return false;
    for (int k = 0; k < 256; ++k)
        objects->palette[k] = (uint16_t)(palette[k * 2] | palette[k * 2 + 1] << 8);
    if (error && error_size) error[0] = '\0';
    return true;
}
