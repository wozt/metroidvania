/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_cells.h"

#include <string.h>

void aos_menu_cells_init(AosMenuCells *cells) {
    if (cells) memset(cells, 0, sizeof *cells);
}

void aos_menu_cells_set_fb(AosMenuCells *cells, uint32_t *fb) {
    if (cells) cells->fb = fb;
}

void aos_menu_cells_set_attr(AosMenuCells *cells, uint16_t attr) {
    if (cells) cells->attr = attr;
}

void aos_menu_cells_set_ink(AosMenuCells *cells, uint16_t ink) {
    if (cells) cells->ink = ink;
}

/* The ink template: (ink + ink * 16) & 0xFF replicated into all four
 * bytes, so every nibble of the word starts as the ink pattern. */
static uint32_t ink_word(uint16_t ink) {
    uint32_t byte = (uint32_t)((ink * 17u) & 0xFFu);
    return byte | byte << 8 | byte << 16 | byte << 24;
}

/* Shared body of sub_0803FEA4 / sub_080402DC: `rows` pixel rows are read
 * from `metrics`; `skip` leading bits are consumed, then `width` pixel
 * slots are emitted. A set bit clears its nibble and ORs the attribute
 * shifted into place; a clear bit leaves the ink nibble untouched. */
static void blit_rows(AosMenuCells *cells, const uint8_t *metrics,
                      unsigned rows, uint16_t skip, uint16_t width) {
    uint32_t word_base = ink_word(cells->ink);
    for (unsigned row = 0; row < rows; ++row) {
        uint32_t word = word_base;
        uint8_t bits = metrics[row];
        unsigned px = 0;
        while (px < skip) {
            bits = (uint8_t)(bits << 1);
            ++px;
        }
        px = 0;
        while (px < width) {
            bool set = (bits & 0x80) != 0;
            bits = (uint8_t)(bits << 1);
            if (set) {
                word &= ~(0xFu << (px * 4));
                word |= (uint32_t)cells->attr << (px * 4);
            }
            ++px;
        }
        cells->scratch[row] = word;
    }
}

void aos_menu_cells_blit_large(AosMenuCells *cells, uint16_t ch,
                               uint16_t skip, uint16_t width,
                               const uint8_t *metrics) {
    (void)ch;
    if (cells && metrics)
        blit_rows(cells, metrics, AOS_MENU_CELLS_LARGE_ROWS, skip, width);
}

void aos_menu_cells_blit_small(AosMenuCells *cells, uint16_t ch,
                               uint16_t skip, uint16_t width,
                               const uint8_t *metrics) {
    (void)ch;
    if (cells && metrics)
        blit_rows(cells, metrics, AOS_MENU_CELLS_SMALL_ROWS, skip, width);
}

/* Nibble mask for pixel slots [first, last): the ARM LSL by px * 4
 * yields 0 for shifts >= 32, so out-of-cell slots contribute nothing. */
static uint32_t nibble_mask(unsigned first, unsigned last) {
    uint32_t mask = 0;
    for (unsigned px = first; px < last; ++px) {
        unsigned shift = px * 4;
        if (shift < 32) mask |= 0xFu << shift;
    }
    return mask;
}

/* Context-A destination: fb + (a_px >> 3) * 8 words + (a_y >> 3) * 256
 * words — pixel x gives the cell column, pixel y the cell row. The
 * large-font merges may spill one cell row down (+256 words) when the
 * glyph crosses the 8px cell boundary vertically. */
static uint32_t *cell_a_base(const AosMenuCells *cells) {
    return cells->fb + (cells->a_px >> 3) * 8 +
           (cells->a_y >> 3) * AOS_MENU_CELLS_ROW_WORDS;
}

void aos_menu_cells_merge_large_single(AosMenuCells *cells,
                                       uint16_t width) {
    /* sub_0803FF64: one horizontal cell, up to two vertical cell rows. */
    if (!cells->fb) return;
    unsigned xs = cells->a_px & 7;
    uint32_t mask = nibble_mask(xs, xs + width);
    uint32_t keep = ~mask;
    unsigned ys = cells->a_y & 7;
    int ry = (int)ys;
    uint32_t *dst = cell_a_base(cells);
    unsigned spill_at = 8 - ys;
    for (unsigned row = 0; row < AOS_MENU_CELLS_LARGE_ROWS; ++row) {
        if (row == spill_at) {
            dst += AOS_MENU_CELLS_ROW_WORDS;
            ry -= 8;
        }
        uint32_t *wp = dst + (row + ry);
        *wp = (*wp & keep) | ((cells->scratch[row] << (xs * 4)) & mask);
    }
}

/* Arithmetic right shift like the ARM ASR: shift >= 32 yields the
 * sign extension (0 or all-ones), never undefined behaviour. */
static uint32_t asr32(uint32_t value, unsigned shift) {
    int32_t v = (int32_t)value;
    if (shift >= 32) return v < 0 ? UINT32_MAX : 0;
    return (uint32_t)(v >> shift);
}

void aos_menu_cells_merge_large_dual(AosMenuCells *cells,
                                     uint16_t width) {
    /* sub_08040048: the glyph spills into the next cell on the right
     * (+8 words) and, vertically, into the next cell row (+256 words). */
    if (!cells->fb) return;
    unsigned xs = cells->a_px & 7;
    uint32_t mask_a = nibble_mask(xs, 8);
    uint32_t mask_b = nibble_mask(0, xs + width - 8);
    uint32_t keep_a = ~mask_a, keep_b = ~mask_b;
    unsigned sh_a = xs * 4, sh_b = (8 - xs) * 4;
    unsigned ys = cells->a_y & 7;
    int ry = (int)ys;
    uint32_t *dst = cell_a_base(cells);
    unsigned spill_at = 8 - ys;
    for (unsigned row = 0; row < AOS_MENU_CELLS_LARGE_ROWS; ++row) {
        if (row == spill_at) {
            dst += AOS_MENU_CELLS_ROW_WORDS;
            ry -= 8;
        }
        uint32_t *wa = dst + (row + ry);
        uint32_t *wb = wa + 8;
        *wa = (*wa & keep_a) |
              ((cells->scratch[row] << sh_a) & mask_a);
        *wb = (*wb & keep_b) |
              (asr32(cells->scratch[row], sh_b) & mask_b);
    }
}

/* Context-B destination: fb + (b_px >> 3) * 8 words + b_row * 256
 * words — b_row is already a cell row (+0x115B4), and the small font
 * is exactly one cell tall so there is no vertical spill. */
static uint32_t *cell_b_base(const AosMenuCells *cells) {
    return cells->fb + (cells->b_px >> 3) * 8 +
           cells->b_row * AOS_MENU_CELLS_ROW_WORDS;
}

void aos_menu_cells_merge_small_single(AosMenuCells *cells,
                                       uint16_t width) {
    /* The single-cell small-font merge inlined in sub_08040C60. */
    if (!cells->fb) return;
    unsigned xs = cells->b_px & 7;
    uint32_t mask = nibble_mask(xs, xs + width);
    uint32_t keep = ~mask;
    uint32_t *wp = cell_b_base(cells);
    for (unsigned row = 0; row < AOS_MENU_CELLS_SMALL_ROWS; ++row)
        wp[row] = (wp[row] & keep) |
                  ((cells->scratch[row] << (xs * 4)) & mask);
}

void aos_menu_cells_merge_small_dual(AosMenuCells *cells,
                                     uint16_t width) {
    /* sub_0804039C: two horizontal cells, one cell row. */
    if (!cells->fb) return;
    unsigned xs = cells->b_px & 7;
    uint32_t mask_a = nibble_mask(xs, 8);
    uint32_t mask_b = nibble_mask(0, xs + width - 8);
    uint32_t keep_a = ~mask_a, keep_b = ~mask_b;
    unsigned sh_a = xs * 4, sh_b = (8 - xs) * 4;
    uint32_t *wp = cell_b_base(cells);
    for (unsigned row = 0; row < AOS_MENU_CELLS_SMALL_ROWS; ++row) {
        wp[row] = (wp[row] & keep_a) |
                  ((cells->scratch[row] << sh_a) & mask_a);
        wp[row + 8] = (wp[row + 8] & keep_b) |
                      (asr32(cells->scratch[row], sh_b) & mask_b);
    }
}

void aos_menu_cells_clear_single(AosMenuCells *cells, uint16_t width) {
    /* sub_080404A0: blank `width` nibbles of the current B cell. */
    if (!cells->fb) return;
    unsigned xs = cells->b_px & 7;
    uint32_t mask = nibble_mask(xs, xs + width);
    uint32_t *wp = cell_b_base(cells);
    for (unsigned row = 0; row < AOS_MENU_CELLS_SMALL_ROWS; ++row)
        wp[row] &= ~mask;
}

void aos_menu_cells_clear_dual(AosMenuCells *cells, uint16_t width) {
    /* sub_08040508: blank across the current B cell and the next one. */
    if (!cells->fb) return;
    unsigned xs = cells->b_px & 7;
    uint32_t mask_a = nibble_mask(xs, 8);
    uint32_t mask_b = nibble_mask(0, xs + width - 8);
    uint32_t *wp = cell_b_base(cells);
    for (unsigned row = 0; row < AOS_MENU_CELLS_SMALL_ROWS; ++row) {
        wp[row] &= ~mask_a;
        wp[row + 8] &= ~mask_b;
    }
}

int aos_menu_cells_print_a(AosMenuCells *cells, uint16_t ch,
                           const uint8_t *char_info, const uint8_t *glyph) {
    /* sub_08040AB8: blit + merge + advance the context-A cursor. */
    if ((ch & 0xE0) == 0) return ch;
    uint16_t skip = char_info[0], width = char_info[1];
    aos_menu_cells_blit_large(cells, ch, skip, width, glyph);
    if ((cells->a_px & 7) + width <= 7)
        aos_menu_cells_merge_large_single(cells, width);
    else
        aos_menu_cells_merge_large_dual(cells, width);
    cells->a_x = (uint16_t)(cells->a_x + width);
    if (cells->a_x > 5) {
        cells->a_col += 1;
        if (cells->a_col > cells->a_col_max) {
            cells->a_col = cells->a_col_home;
            cells->a_row += 1;
            if (cells->a_row > cells->a_row_max)
                cells->a_row = cells->a_row_stop;
        }
        cells->a_y = (uint16_t)(cells->a_row * 12);
        cells->a_x = 0;
        cells->a_px = (uint16_t)(cells->a_col * 6);
    } else {
        cells->a_px = (uint16_t)(cells->a_px + width);
    }
    return -1;
}

int aos_menu_cells_print_b(AosMenuCells *cells, uint16_t ch,
                           const uint8_t *char_info, const uint8_t *glyph) {
    /* sub_08040C60: same flow on the context-B cursor; the single-cell
     * merge is inlined natively and exposed here as a function. */
    if ((ch & 0xE0) == 0) return ch;
    uint16_t skip = char_info[0], width = char_info[1];
    aos_menu_cells_blit_small(cells, ch, skip, width, glyph);
    if ((cells->b_px & 7) + width <= 7)
        aos_menu_cells_merge_small_single(cells, width);
    else
        aos_menu_cells_merge_small_dual(cells, width);
    cells->b_sx = (uint16_t)(cells->b_sx + width);
    if (cells->b_sx > 5) {
        cells->b_col += 1;
        if (cells->b_col > cells->b_x1) {
            cells->b_col = cells->b_x0;
            cells->b_row += 1;
            if (cells->b_row > cells->b_y1)
                cells->b_row = cells->b_y0;
            cells->b_py = (uint16_t)(cells->b_row * 8);
            cells->b_sx = 0;
            cells->b_px = (uint16_t)(cells->b_col * 6);
        } else {
            cells->b_px = (uint16_t)(cells->b_px + width);
        }
    } else {
        cells->b_px = (uint16_t)(cells->b_px + width);
    }
    return -1;
}

static int run_text(AosMenuCells *cells, const uint8_t *script,
                    uint16_t offset, const uint8_t *char_tbl,
                    unsigned char_stride, const uint8_t *glyph_tbl,
                    unsigned glyph_stride, bool large) {
    /* sub_08041318 / sub_08041338 feed bytes to the printer; the
     * control-byte dispatch of sub_08046BC8 treats byte 6 as a
     * newline: it repositions the cursor with sub_0804066C to
     * (column at entry, row at entry + newline count), clamped to
     * 40x20 — not a relative wrap reset. Other control bytes end
     * the run (10 = terminator, 11..18 = icon commands handled by
     * the caller's jump table, not ported here). */
    const uint8_t *p = script + 2 + offset;
    uint16_t base_col = large ? cells->a_col : cells->b_col;
    uint16_t base_row = large ? cells->a_row : cells->b_row;
    uint16_t lines = 0;
    int i = 0;
    for (;;) {
        uint16_t ch = p[i];
        int ret;
        if (ch & 0xE0) {
            const uint8_t *ci = char_tbl + (unsigned)(ch - 0x20) * char_stride;
            const uint8_t *gl = glyph_tbl + (unsigned)(ch - 0x20) * glyph_stride;
            ret = large ? aos_menu_cells_print_a(cells, ch, ci, gl)
                        : aos_menu_cells_print_b(cells, ch, ci, gl);
        } else if (ch == 6) {
            /* _08046C7C: sl += 1; sub_0804066C(base_col, base_row + sl). */
            ++lines;
            uint16_t col = base_col, row = (uint16_t)(base_row + lines);
            if (col > 0x28) col = 0x28;
            if (row > 0x14) row = 0x14;
            if (large) {
                cells->a_col = col;
                cells->a_row = row;
                cells->a_x = 0;
                cells->a_px = (uint16_t)(col * 6);
                cells->a_y = (uint16_t)(row * 12);
            } else {
                cells->b_col = col;
                cells->b_row = row;
                cells->b_sx = 0;
                cells->b_px = (uint16_t)(col * 6);
                cells->b_py = (uint16_t)(row * 8);
            }
            ++i;
            continue;
        } else {
            ret = large ? aos_menu_cells_print_a(cells, ch, NULL, NULL)
                        : aos_menu_cells_print_b(cells, ch, NULL, NULL);
        }
        if (ret != -1) {
            /* _08046D50: sub_08046BC8 restores the saved cursor on exit. */
            if (large) {
                uint16_t col = base_col > 0x28 ? 0x28 : base_col;
                uint16_t row = base_row > 0x14 ? 0x14 : base_row;
                cells->a_col = col;
                cells->a_row = row;
                cells->a_x = 0;
                cells->a_px = (uint16_t)(col * 6);
                cells->a_y = (uint16_t)(row * 12);
            }
            return i;
        }
        ++i;
    }
}

int aos_menu_cells_run_a(AosMenuCells *cells, const uint8_t *script,
                         uint16_t offset, const uint8_t *char_tbl,
                         unsigned char_stride, const uint8_t *glyph_tbl,
                         unsigned glyph_stride) {
    return run_text(cells, script, offset, char_tbl, char_stride,
                    glyph_tbl, glyph_stride, true);
}

int aos_menu_cells_run_b(AosMenuCells *cells, const uint8_t *script,
                         uint16_t offset, const uint8_t *char_tbl,
                         unsigned char_stride, const uint8_t *glyph_tbl,
                         unsigned glyph_stride) {
    return run_text(cells, script, offset, char_tbl, char_stride,
                    glyph_tbl, glyph_stride, false);
}

void aos_menu_cells_icon_begin(AosMenuCells *cells, uint16_t col,
                               uint16_t row) {
    /* sub_080411AC: position the B cursor and take one step. */
    cells->b_col = col;
    cells->b_row = row;
    cells->b_sx = 0;
    cells->b_px = (uint16_t)(col * 6);
    cells->b_py = (uint16_t)(row * 8);
    aos_menu_cells_icon_step(cells);
}

void aos_menu_cells_icon_step(AosMenuCells *cells) {
    /* sub_080410BC: clear the 6px icon slot at the cursor, then advance
     * one column (wrapping inside the configured window). */
    if (!cells->fb) return;
    if ((cells->b_px & 7) + 6 <= 7)
        aos_menu_cells_clear_single(cells, 6);
    else
        aos_menu_cells_clear_dual(cells, 6);
    uint16_t old = cells->b_sx;
    cells->b_sx = (uint16_t)(old + 6);
    if (cells->b_sx > 5) {
        cells->b_sx = old;
        cells->b_col += 1;
        if (cells->b_col > cells->b_x1) {
            cells->b_col = cells->b_x0;
            cells->b_row += 1;
            if (cells->b_row > cells->b_y1)
                cells->b_row = cells->b_y0;
            cells->b_py = (uint16_t)(cells->b_row * 8);
            cells->b_sx = 0;
            cells->b_px = (uint16_t)(cells->b_col * 6);
        } else {
            cells->b_px = (uint16_t)(cells->b_px + 6);
        }
    }
}

void aos_menu_cells_icon_fill_rect(AosMenuCells *cells) {
    /* sub_08041204: walk every cell of the window rectangle, then
     * restore the cursor to the window origin. The column loop runs
     * through b_x1 + 1 inclusive, exactly like the native code. */
    if (!cells->fb) return;
    for (uint16_t row = cells->b_y0; row <= cells->b_y1; ++row) {
        uint16_t last = (uint16_t)(cells->b_x1 + 1);
        for (uint16_t col = cells->b_x0; col <= last; ++col)
            aos_menu_cells_icon_begin(cells, col, row);
    }
    cells->b_col = cells->b_x0;
    cells->b_row = cells->b_y0;
    cells->b_sx = 0;
    cells->b_px = (uint16_t)(cells->b_col * 6);
    cells->b_py = (uint16_t)(cells->b_row * 8);
}

void aos_menu_cells_window_b(AosMenuCells *cells, uint16_t x, uint16_t y,
                             uint16_t w, uint16_t h) {
    /* sub_08040970: store the context-B window bounds — x0/y0 clamped
     * to 40/31, x1 = x + w - 2, y1 = y + h - 1 — then clear the
     * sub-column offset and recompute the absolute pixel position
     * from the CURRENT cell cursor (b_col/b_row are not reset). */
    cells->b_x0 = x > 0x28 ? 0x28 : x;
    cells->b_y0 = y > 0x1F ? 0x1F : y;
    cells->b_x1 = (uint16_t)(x + w - 2);
    if (cells->b_x1 > 0x28) cells->b_x1 = 0x28;
    cells->b_y1 = (uint16_t)(y + h - 1);
    if (cells->b_y1 > 0x1F) cells->b_y1 = 0x1F;
    cells->b_sx = 0;
    cells->b_px = (uint16_t)(cells->b_col * 6);
    cells->b_py = (uint16_t)(cells->b_row * 8);
}

void aos_menu_cells_window_a(AosMenuCells *cells, uint16_t col,
                             uint16_t row, uint16_t w, uint16_t h) {
    /* sub_08040748: store the window bounds, clamp to 40x20 cells,
     * then reset cursor A to the home position. */
    cells->a_col_home = col;
    cells->a_row_stop = row;
    cells->a_col_max = (uint16_t)(col + w - 2);
    cells->a_row_max = (uint16_t)(row + h - 1);
    if (cells->a_col_home > 0x28) cells->a_col_home = 0x28;
    if (cells->a_row_stop > 0x14) cells->a_row_stop = 0x14;
    if (cells->a_col_max > 0x28) cells->a_col_max = 0x28;
    if (cells->a_row_max > 0x14) cells->a_row_max = 0x14;
    cells->a_x = 0;
    cells->a_px = (uint16_t)(cells->a_col * 6);
    cells->a_y = (uint16_t)(cells->a_row * 12);
}

bool aos_menu_cells_cell_tile(const AosMenuCells *cells, uint16_t col,
                              uint16_t row, uint8_t out[32]) {
    if (!cells->fb || !out) return false;
    const uint32_t *src =
        cells->fb + (size_t)row * AOS_MENU_CELLS_ROW_WORDS + col * 8;
    memcpy(out, src, 32);
    return true;
}

void aos_menu_cells_cursor_a(AosMenuCells *cells, uint16_t col,
                             uint16_t row) {
    if (col > 0x28) col = 0x28;
    if (row > 0x14) row = 0x14;
    cells->a_col = col;
    cells->a_row = row;
    cells->a_x = 0;
    cells->a_px = (uint16_t)(col * 6);
    cells->a_y = (uint16_t)(row * 12);
}
