/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_bg0.h"

#include "aos_rom_graphics.h"

#include <stdio.h>
#include <string.h>

enum {
    AOS_MENU_TEXT_PALETTE = 0x081183F4,
    AOS_MENU_DIGIT_PALETTE = 0x080E5C20,
    AOS_MENU_BANK_ENGLISH = 0x0827B200,
    AOS_MENU_BANK_FRENCH = 0x08277984,
    AOS_MENU_BANK_GERMAN = 0x0826C848,
};

bool aos_menu_bg0_init(AosMenuBg0 *bg0, const FusionRom *rom, uint8_t language,
                       char *error, size_t error_size) {
    if (!bg0 || !rom || language < 1 || language > 3) {
        if (error && error_size) snprintf(error, error_size, "invalid Aria menu BG0 request");
        return false;
    }
    memset(bg0, 0, sizeof *bg0);
    /* The switch on unk_60.language: 2 French, 3 German, otherwise English. */
    static const uint32_t banks[] = {AOS_MENU_BANK_ENGLISH, AOS_MENU_BANK_FRENCH,
                                     AOS_MENU_BANK_GERMAN};
    if (!aos_rom_load_graphics_resource(rom, banks[language - 1],
                                        bg0->chars + AOS_MENU_BG0_TEXT_BYTES,
                                        AOS_MENU_BG0_CHAR_BYTES - AOS_MENU_BG0_TEXT_BYTES,
                                        error, error_size) ||
        !aos_rom_load_palette_range(rom, AOS_MENU_TEXT_PALETTE, 0, 1, 0xC,
                                    bg0->palette, error, error_size) ||
        !aos_rom_load_palette_range(rom, AOS_MENU_DIGIT_PALETTE, 3, 1, 0xD,
                                    bg0->palette, error, error_size))
        return false;
    bg0->language = language;
    return true;
}

void aos_menu_bg0_map_text(AosMenuBg0 *bg0) {
    for (unsigned index = 0; index < 0x80; ++index)
        bg0->map[15 * AOS_MENU_BG0_MAP_SIZE + 2 + index] = (uint16_t)(index - 0x3FE0);
}

void aos_menu_bg0_set_text(AosMenuBg0 *bg0, const AosIntroTextTiles *text) {
    memcpy(bg0->chars, text->data, AOS_MENU_BG0_TEXT_BYTES);
}

/* The map is 0x800 bytes; writes past it would land in VRAM the menu does
 * not show and are dropped. */
static void store(AosMenuBg0 *bg0, int cell, uint16_t entry) {
    if (cell >= 0 && cell < AOS_MENU_BG0_MAP_SIZE * AOS_MENU_BG0_MAP_SIZE)
        bg0->map[cell] = entry;
}

void aos_menu_bg0_put_tiles(AosMenuBg0 *bg0, int x, int y, uint16_t entry, int count) {
    int cell = y * AOS_MENU_BG0_MAP_SIZE + x;
    for (; count > 0; --count) store(bg0, cell++, entry++);
}

int aos_menu_bg0_put_number(AosMenuBg0 *bg0, int x, int y, int32_t value,
                            int digits, uint16_t base, bool zero_pad) {
    int cell = y * AOS_MENU_BG0_MAP_SIZE + x;
    if (value < 0) {
        store(bg0, cell, base);
        return 1;
    }
    int count = 1;
    while (count <= digits || value != 0) {
        int32_t digit = value % 10;
        store(bg0, cell, count != 1 && value == 0 && !zero_pad ? (uint16_t)(base + 0xD)
                                                              : (uint16_t)(base + digit));
        value /= 10;
        --cell;
        ++count;
    }
    return count - 1;
}

bool aos_menu_bg0_pixel(const AosMenuBg0 *bg0, unsigned x, unsigned y,
                        uint16_t *color) {
    uint16_t entry = bg0->map[(y / 8 % AOS_MENU_BG0_MAP_SIZE) * AOS_MENU_BG0_MAP_SIZE +
                              x / 8 % AOS_MENU_BG0_MAP_SIZE];
    unsigned tile = entry & 0x3FF, px = x % 8, py = y % 8;
    if (entry & 0x400) px = 7 - px;
    if (entry & 0x800) py = 7 - py;
    size_t offset = (size_t)tile * 32 + py * 4 + px / 2;
    if (offset >= AOS_MENU_BG0_CHAR_BYTES) return false;
    unsigned index = (bg0->chars[offset] >> (px & 1u ? 4 : 0)) & 0xF;
    if (!index) return false;
    size_t slot = ((size_t)(entry >> 12) * 16 + index) * 2;
    *color = (uint16_t)(bg0->palette[slot] | bg0->palette[slot + 1] << 8);
    return true;
}
