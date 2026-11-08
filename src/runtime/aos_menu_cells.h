/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's software cell renderer: the gEwramData +0x11598..+0x115C6 state,
 * the staging strip at 0x03001A18, the masked merge routines that
 * composite glyphs into the window cell buffer, the two text cursor
 * contexts and the icon/window walker. No SDL, no ROM access: callers
 * supply the framebuffer, the per-character advance table and the glyph
 * metric records. */
#ifndef AOS_MENU_CELLS_H
#define AOS_MENU_CELLS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The destination cell buffer (+0x11598 points at it): each cell row
 * spans 0x400 bytes = 256 words = 32 cells of 8 words (8 pixel rows of
 * one 8x8 cell, one 32-bit word per pixel row, eight 4bpp nibbles). */
enum {
    AOS_MENU_CELLS_ROW_WORDS = 256,
    AOS_MENU_CELLS_COLS = 32,
    AOS_MENU_CELLS_LARGE_ROWS = 12,
    AOS_MENU_CELLS_SMALL_ROWS = 8,
    AOS_MENU_CELLS_PIXELS = 8,
};

typedef struct {
    /* +0x11598: destination cell buffer (row pitch 256 words). */
    uint32_t *fb;
    /* +0x115C4: attribute OR-ed into every set pixel nibble. */
    uint16_t attr;
    /* +0x115C6: ink byte replicated into every pixel nibble. */
    uint16_t ink;
    /* Context A cursor (large font, 12px lines): +0x115A0..+0x115B0. */
    uint16_t a_col;       /* +0x115A0 text column */
    uint16_t a_row;       /* +0x115A2 text row */
    uint16_t a_x;         /* +0x115A4 pixels accumulated in the 6px slot */
    uint16_t a_px;        /* +0x115A6 absolute pixel x */
    uint16_t a_y;         /* +0x115A8 absolute pixel y (row * 12) */
    uint16_t a_col_home;  /* +0x115AA column reset on wrap */
    uint16_t a_row_stop;  /* +0x115AC row clamp on wrap */
    uint16_t a_col_max;   /* +0x115AE column limit */
    uint16_t a_row_max;   /* +0x115B0 row limit for init clamp */
    /* Context B cursor (small font / icon walker): +0x115B2..+0x115C2. */
    uint16_t b_col;       /* +0x115B2 column */
    uint16_t b_row;       /* +0x115B4 cell row */
    uint16_t b_sx;        /* +0x115B6 sub-column pixel offset */
    uint16_t b_px;        /* +0x115B8 absolute pixel x */
    uint16_t b_py;        /* +0x115BA absolute pixel y (row * 8) */
    uint16_t b_x0;        /* +0x115BC window left column */
    uint16_t b_y0;        /* +0x115BE window top row */
    uint16_t b_x1;        /* +0x115C0 window right column limit */
    uint16_t b_y1;        /* +0x115C2 window bottom row limit */
    /* 0x03001A18: glyph staging strip written by the blitters. */
    uint32_t scratch[AOS_MENU_CELLS_LARGE_ROWS];
} AosMenuCells;

void aos_menu_cells_init(AosMenuCells *cells);
void aos_menu_cells_set_fb(AosMenuCells *cells, uint32_t *fb); /* sub_080412DC */
void aos_menu_cells_set_attr(AosMenuCells *cells, uint16_t attr); /* sub_080412F0 */
void aos_menu_cells_set_ink(AosMenuCells *cells, uint16_t ink);   /* sub_08041304 */

/* Primitive glyph blitters into scratch[]: sub_0803FEA4 (large, 12
 * rows) and sub_080402DC (small, 8 rows). `metrics` is the glyph record
 * (14 bytes large / 10 bytes small); bytes 0..rows-1 are the pixel-row
 * bitmasks consumed MSB first. `skip` leading bits are discarded (left
 * bearing), then `width` pixel slots are emitted. */
void aos_menu_cells_blit_large(AosMenuCells *cells, uint16_t ch,
                               uint16_t skip, uint16_t width,
                               const uint8_t *metrics);
void aos_menu_cells_blit_small(AosMenuCells *cells, uint16_t ch,
                               uint16_t skip, uint16_t width,
                               const uint8_t *metrics);

/* Masked merges of scratch[] into the cell buffer.
 * Large font uses the context-A pixel coordinates (+0x115A6/+0x115A8)
 * and spills vertically into the next cell row (+0x400 bytes). */
void aos_menu_cells_merge_large_single(AosMenuCells *cells,
                                        uint16_t width);
void aos_menu_cells_merge_large_dual(AosMenuCells *cells,
                                     uint16_t width);
/* Small font uses the context-B coordinates (+0x115B8/+0x115B4, the
 * row already in cell units) and never spills vertically. */
void aos_menu_cells_merge_small_single(AosMenuCells *cells,
                                       uint16_t width);
void aos_menu_cells_merge_small_dual(AosMenuCells *cells,
                                     uint16_t width);

/* Cell clears used by the icon walker: sub_080404A0 blanks `width`
 * nibbles of one cell, sub_08040508 splits across two cells. */
void aos_menu_cells_clear_single(AosMenuCells *cells, uint16_t width);
void aos_menu_cells_clear_dual(AosMenuCells *cells, uint16_t width);

/* Text printers. `char_info` is the 4-byte per-character record
 * (byte 0 = left bearing, byte 1 = advance width — tables 0x080E2B4C
 * for context A and 0x080E2EC0 for context B); `glyph` is the metric
 * record for the blitter. Return -1 while printing continues, or the
 * character code itself for ch < 0x20 (end of run / command). */
int aos_menu_cells_print_a(AosMenuCells *cells, uint16_t ch,
                           const uint8_t *char_info, const uint8_t *glyph);
int aos_menu_cells_print_b(AosMenuCells *cells, uint16_t ch,
                           const uint8_t *char_info, const uint8_t *glyph);

/* String runners mirroring sub_08041318 / sub_08041338: bytes are read
 * from script[2 + offset + i] until the printer stops returning -1.
 * `char_tbl`/`glyph_tbl` are the ROM metric tables with the given
 * record strides (4 and 14 for context A, 4 and 10 for context B).
 * Returns the number of bytes consumed before the stop. */
int aos_menu_cells_run_a(AosMenuCells *cells, const uint8_t *script,
                         uint16_t offset, const uint8_t *char_tbl,
                         unsigned char_stride, const uint8_t *glyph_tbl,
                         unsigned glyph_stride);
int aos_menu_cells_run_b(AosMenuCells *cells, const uint8_t *script,
                         uint16_t offset, const uint8_t *char_tbl,
                         unsigned char_stride, const uint8_t *glyph_tbl,
                         unsigned glyph_stride);

/* Icon/window walker: sub_080411AC positions the context-B cursor at
 * (col, row) and steps once; sub_08041204 fills the configured window
 * rectangle (+0x115BC..+0x115C2) then restores the cursor origin. */
void aos_menu_cells_icon_begin(AosMenuCells *cells, uint16_t col,
                               uint16_t row);
void aos_menu_cells_icon_step(AosMenuCells *cells);
void aos_menu_cells_icon_fill_rect(AosMenuCells *cells);

/* sub_08040748: configure the context-A window. `col`/`row` are the
 * home (top-left) text coordinates, `w`/`h` the extents in 6px columns
 * and 12px rows; the native code derives col_max = col + w - 2 and
 * row_max = row + h - 1, clamps all four bounds to 40x20, resets the
 * cursor and recomputes the absolute pixel position. */
void aos_menu_cells_window_a(AosMenuCells *cells, uint16_t col,
                             uint16_t row, uint16_t w, uint16_t h);

/* sub_08040970: context-B window bounds (x0/y0 clamps 40/31,
 * x1 = x+w-2, y1 = y+h-1), clears the sub-column offset and recomputes
 * the pixel cursor from the current cell cursor. */
void aos_menu_cells_window_b(AosMenuCells *cells, uint16_t x, uint16_t y,
                             uint16_t w, uint16_t h);

/* sub_0804066C: move the context-A cursor to (col, row) — clamped to
 * 40x20 — clearing the sub-column offset and recomputing the absolute
 * pixel position. */
void aos_menu_cells_cursor_a(AosMenuCells *cells, uint16_t col,
                             uint16_t row);

/* Cell commit: each 8x8 cell of the framebuffer is already a native
 * 32-byte 4bpp tile (8 rows of one 32-bit word, low nibble = leftmost
 * pixel). `aos_menu_cells_cell_tile` copies one cell out verbatim;
 * callers walk the window rectangle and write the tiles plus the
 * tilemap entries themselves (the role of sub_080472B0). */
bool aos_menu_cells_cell_tile(const AosMenuCells *cells, uint16_t col,
                              uint16_t row, uint8_t out[32]);

#endif /* AOS_MENU_CELLS_H */
