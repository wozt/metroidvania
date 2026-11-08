/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_cells.h"

#include <string.h>

void aos_menu_cells_init(AosMenuCells *cells) {
    if (cells) memset(cells, 0, sizeof *cells);
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
