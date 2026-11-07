/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_window_frame.h"

#include "aos_rom_graphics.h"

#include <stdio.h>
#include <string.h>

enum {
    AOS_ROM_BASE = 0x08000000,
    AOS_WINDOW_PALETTE = 0x081183F4,
    AOS_PORTRAIT_UI_PALETTE = 0x080E5C20,
    /* sub_0805AE64's per-language window sheet descriptors; German
     * uploads a second sheet at VRAM 0x06002000 (tiles 0x100-0x1FF). */
    AOS_WINDOW_SHEET_EN = 0x0815E084,
    AOS_WINDOW_SHEET_FR = 0x0815E09C,
    AOS_WINDOW_SHEET_DE0 = 0x0815E08C,
    AOS_WINDOW_SHEET_DE1 = 0x0815E094,
    /* sub_080676BC copies bytes 0x400..0x7FF from the selected language
     * bank to VRAM 0x06005800, replacing tiles 0xC0..0xDF. */
    AOS_WINDOW_BANK_EN = 0x0827B200,
    AOS_WINDOW_BANK_FR = 0x08277984,
    AOS_WINDOW_BANK_DE = 0x0826C848,
};

bool aos_window_frame_init(AosWindowFrame *frame, const FusionRom *rom,
                           uint8_t language, char *error, size_t error_size) {
    if (!frame || !rom || language < 1 || language > 3) {
        if (error && error_size)
            snprintf(error, error_size, "invalid Aria window frame request");
        return false;
    }
    memset(frame, 0, sizeof *frame);
    /* unk_60.language: 2 French, 3 German, otherwise English. */
    static const uint32_t sheets[][2] = {
        {AOS_WINDOW_SHEET_EN, 0},
        {AOS_WINDOW_SHEET_FR, 0},
        {AOS_WINDOW_SHEET_DE0, 0},
        {AOS_WINDOW_SHEET_DE1, 0x2000},
    };
    unsigned first = language == 3 ? 2 : language - 1;
    unsigned count = language == 3 ? 2 : 1;
    for (unsigned i = 0; i < count; ++i) {
        if (!aos_rom_load_graphics_resource(
                rom, sheets[first + i][0],
                frame->tiles + sheets[first + i][1],
                AOS_WINDOW_FRAME_TILE_BYTES - sheets[first + i][1],
                error, error_size))
            return false;
    }
    static const uint32_t banks[] = {
        AOS_WINDOW_BANK_EN, AOS_WINDOW_BANK_FR, AOS_WINDOW_BANK_DE,
    };
    uint8_t bank[AOS_WINDOW_FRAME_TILE_BYTES] = {0};
    if (!aos_rom_load_graphics_resource(rom, banks[language - 1], bank,
                                        sizeof bank, error, error_size))
        return false;
    /* sub_0803B9B4 leaves the four-byte expanded header in EWRAM, whereas
     * aos_rom_load_graphics_resource normalizes it away. Rebase the native
     * +0x400 DMA source for compressed banks. */
    size_t bank_rom_offset = banks[language - 1] - AOS_ROM_BASE;
    size_t bank_offset = bank_rom_offset < rom->size &&
                                 rom->data[bank_rom_offset] == 1
                             ? 0x3FCu : 0x400u;
    memcpy(frame->tiles + 0x1800, bank + bank_offset, 0x400);
    if (!aos_rom_load_palette_range(rom, AOS_WINDOW_PALETTE, 0, 1, 0xF,
                                    frame->palette, error, error_size) ||
        /* sub_080676BC replaces row 15 immediately before the portrait
         * and wait marker are displayed. */
        !aos_rom_load_palette_range(rom, AOS_PORTRAIT_UI_PALETTE, 4, 1, 0xF,
                                    frame->palette, error, error_size))
        return false;
    frame->language = language;
    return true;
}

void aos_window_frame_build(AosWindowFrame *frame) {
    if (!frame) return;
    memset(frame->map, 0, sizeof frame->map);
    /* sub_0805AFC0's strip loop: r = 0..3 writes 0x1D entries at
     * (2, 0x10 + r) starting at (0xF0600000 + r * 0x200000) >> 16,
     * i.e. bases 0xF060, 0xF080, 0xF0A0, 0xF0C0. */
    for (unsigned r = 0; r < 4; ++r) {
        int cell = (0x10 + r) * AOS_WINDOW_FRAME_MAP_SIZE + 2;
        uint16_t entry = (uint16_t)(0xF060 + r * 0x20);
        for (unsigned i = 0; i < 0x1D; ++i)
            frame->map[cell++] = (uint16_t)(entry + i);
    }
}

bool aos_window_frame_entry_pixel(const AosWindowFrame *frame, uint16_t entry,
                                  unsigned x, unsigned y, uint16_t *color) {
    if (!frame || !color || x >= 8 || y >= 8) return false;
    unsigned tile = entry & 0x3FF, px = x, py = y;
    if (entry & 0x400) px = 7 - px;
    if (entry & 0x800) py = 7 - py;
    size_t offset = (size_t)tile * 32 + py * 4 + px / 2;
    if (offset >= AOS_WINDOW_FRAME_TILE_BYTES) return false;
    unsigned index = (frame->tiles[offset] >> (px & 1u ? 4 : 0)) & 0xF;
    if (!index) return false;
    size_t slot = ((size_t)(entry >> 12) * 16 + index) * 2;
    *color = (uint16_t)(frame->palette[slot] | frame->palette[slot + 1] << 8);
    return true;
}

bool aos_window_frame_pixel(const AosWindowFrame *frame, unsigned x,
                            unsigned y, uint16_t *color) {
    if (!frame || !color) return false;
    uint16_t entry = frame->map[(y / 8 % AOS_WINDOW_FRAME_MAP_SIZE) *
                                    AOS_WINDOW_FRAME_MAP_SIZE +
                                x / 8 % AOS_WINDOW_FRAME_MAP_SIZE];
    return aos_window_frame_entry_pixel(frame, entry, x % 8, y % 8, color);
}
