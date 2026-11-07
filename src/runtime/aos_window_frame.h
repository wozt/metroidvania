/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_WINDOW_FRAME_H
#define AOS_WINDOW_FRAME_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/rom.h"

/* The gameplay dialogue window's BG0 frame as sub_0805AFC0 leaves it:
 * sub_0805AE64 uploads the language's window sheet to VRAM 0x06000000
 * (English descriptor 0x0815E084 -> tiles 0x000-0x0FF, French
 * 0x0815E09C, German the pair 0x0815E08C/0x0815E094 -> tiles
 * 0x000-0x1FF) and palette row 0 of 0x081183F4 to BG palette row 15.
 * The state machine then clears the 32x32 map at 0x0600E000 and writes
 * four strips with sub_08046DD4: 29 consecutive entries at (2, 16+r),
 * bases 0xF060 + r*0x20 for r = 0..3 (tiles 0x060-0x0DC of the sheet).
 * Entries follow the engine's convention used by aos_menu_bg0_pixel:
 * bits 12-15 select the palette row, bits 10-11 the flips. */
enum {
    AOS_WINDOW_FRAME_MAP_SIZE = 32,
    AOS_WINDOW_FRAME_TILE_BYTES = 0x4000,
    AOS_WINDOW_FRAME_SHEET_TILES = 0x100,
};

typedef struct {
    uint16_t map[AOS_WINDOW_FRAME_MAP_SIZE * AOS_WINDOW_FRAME_MAP_SIZE];
    uint8_t tiles[AOS_WINDOW_FRAME_TILE_BYTES];
    uint8_t palette[0x200];
    uint8_t language;
} AosWindowFrame;

/* Loads the language window sheet(s) and the dialogue palette row. */
bool aos_window_frame_init(AosWindowFrame *frame, const FusionRom *rom,
                           uint8_t language, char *error, size_t error_size);
/* The four native border strips written by sub_0805AFC0. */
void aos_window_frame_build(AosWindowFrame *frame);
/* The BGR555 colour of screen pixel (x, y); false when transparent. */
bool aos_window_frame_pixel(const AosWindowFrame *frame, unsigned x,
                            unsigned y, uint16_t *color);
#endif
