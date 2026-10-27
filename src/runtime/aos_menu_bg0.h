/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MENU_BG0_H
#define AOS_MENU_BG0_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_intro_font.h"
#include "fusion/assets/rom.h"

/* The main menu's BG0 as GameModeMainMenuUpdate's setup leaves it
 * (src/code_08002454.c): character base 2 (VRAM 0x8000), map base 28
 * (0xE000, 32x32 entries), palettes 12 and 13. The text engine owns tiles
 * 0x000-0x0FF (0x8000) and the language's menu bank starts at tile 0x100
 * (0xA000). */
enum {
    AOS_MENU_BG0_MAP_SIZE = 32,
    AOS_MENU_BG0_CHAR_BYTES = 0x6000,
    AOS_MENU_BG0_TEXT_BYTES = 0x2000,
};

typedef struct {
    uint16_t map[AOS_MENU_BG0_MAP_SIZE * AOS_MENU_BG0_MAP_SIZE];
    uint8_t chars[AOS_MENU_BG0_CHAR_BYTES];
    uint8_t palette[0x200];
    uint8_t language;
} AosMenuBg0;

/* Loads the language bank (sub_0803FD9C with 0x0827B200, 0x08277984 or
 * 0x0826C848 at 0xA000) and palettes 12 (0x081183F4) and 13 (row 3 of
 * 0x080E5C20), with an empty map and empty text tiles. */
bool aos_menu_bg0_init(AosMenuBg0 *bg0, const FusionRom *rom, uint8_t language,
                       char *error, size_t error_size);
/* The setup's map loop: 128 consecutive entries 0xC020 + n from map cell
 * (2, 15) show text tiles 0x20-0x9F with palette 12. */
void aos_menu_bg0_map_text(AosMenuBg0 *bg0);
/* Copies the text engine's canvas into tiles 0x000-0x0FF. */
void aos_menu_bg0_set_text(AosMenuBg0 *bg0, const AosIntroTextTiles *text);
/* sub_08046DD4: `count` consecutive map entries from `entry` at (x, y). */
void aos_menu_bg0_put_tiles(AosMenuBg0 *bg0, int x, int y, uint16_t entry, int count);
/* sub_08046E5C: decimal digits right-aligned at column x (written leftwards)
 * as tile base + digit; leading zeros past the first digit become base + 0xD
 * unless zero_pad. At least `digits` digits, more while the value lasts. A
 * negative value writes one entry, the base itself. Returns the digit count. */
int aos_menu_bg0_put_number(AosMenuBg0 *bg0, int x, int y, int32_t value,
                            int digits, uint16_t base, bool zero_pad);
/* The BGR555 colour of screen pixel (x, y) with BG0 unscrolled; false when
 * the pixel is transparent. */
bool aos_menu_bg0_pixel(const AosMenuBg0 *bg0, unsigned x, unsigned y,
                        uint16_t *color);
#endif
