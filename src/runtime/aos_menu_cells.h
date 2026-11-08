/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's software cell renderer state (gEwramData +0x11598..+0x115C6 and
 * the staging buffer at 0x03001A18). The native glyph blitters draw into
 * a fixed scratch strip which the masked merge routines then composite
 * into the window cell buffer; this module ports the state and the two
 * primitive blitters. No SDL, no ROM access. */
#ifndef AOS_MENU_CELLS_H
#define AOS_MENU_CELLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The staging strip: sub_0803FEA4 writes 12 words (large font, 12 pixel
 * rows) and sub_080402DC 8 words (small font, 8 pixel rows) at
 * 0x03001A18. Each word packs one pixel row of an 8-pixel cell: eight
 * 4bpp nibbles, bit 7 of the glyph byte is the leftmost pixel. */
enum {
    AOS_MENU_CELLS_LARGE_ROWS = 12,
    AOS_MENU_CELLS_SMALL_ROWS = 8,
    AOS_MENU_CELLS_PIXELS = 8,
};

typedef struct {
    /* +0x115C4: attribute OR-ed into every set pixel nibble (low nibble
     * lands on each pixel; higher bits bleed into following nibbles,
     * exactly like the ARM shift). */
    uint16_t attr;
    /* +0x115C6: ink byte replicated into every pixel nibble before the
     * glyph bits are applied ((ink * 17) & 0xFF per byte). */
    uint16_t ink;
    /* 0x03001A18: the staging strip the blitters write. */
    uint32_t scratch[AOS_MENU_CELLS_LARGE_ROWS];
} AosMenuCells;

void aos_menu_cells_init(AosMenuCells *cells);
void aos_menu_cells_set_attr(AosMenuCells *cells, uint16_t attr); /* sub_080412F0 */
void aos_menu_cells_set_ink(AosMenuCells *cells, uint16_t ink);   /* sub_08041304 */

/* sub_0803FEA4: blit one large-font glyph into scratch[0..11].
 * `metrics` is the 14-byte record at 0x0850A136 + (ch - 0x20) * 14;
 * bytes 0..11 are the pixel-row bitmasks consumed MSB first.
 * `skip` leading bits are discarded (left bearing), then `width`
 * pixel slots are emitted. */
void aos_menu_cells_blit_large(AosMenuCells *cells, uint16_t ch,
                               uint16_t skip, uint16_t width,
                               const uint8_t *metrics);

/* sub_080402DC: same for the small font — 8 rows, metrics record of 10
 * bytes at 0x08509876 + (ch - 0x20) * 10, bytes 0..7 are the rows. */
void aos_menu_cells_blit_small(AosMenuCells *cells, uint16_t ch,
                               uint16_t skip, uint16_t width,
                               const uint8_t *metrics);

#endif /* AOS_MENU_CELLS_H */
