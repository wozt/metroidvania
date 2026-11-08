/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_cells.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Hand-computed expectations for the sub_0803FEA4/sub_080402DC ports.
 * ink = 0x11 -> byte (0x11 * 17) & 0xFF = 0x21 -> word 0x21212121.
 * attr = 0x5: a set pixel clears its nibble and ORs attr << px*4, so
 * pixel 0 writes nibble 0 = 5 (higher attr bits would bleed into the
 * next nibble, matching the ARM 32-bit shift). */

static void test_large_basic(void) {
    AosMenuCells cells;
    uint8_t metrics[14] = {0};
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_ink(&cells, 0x11);
    aos_menu_cells_set_attr(&cells, 0x5);
    /* Row 0: only the leftmost pixel set (0x80). Rows 1-11 empty. */
    metrics[0] = 0x80;
    aos_menu_cells_blit_large(&cells, 'A', 0, 8, metrics);
    assert(cells.scratch[0] == 0x21212125u);
    for (unsigned row = 1; row < AOS_MENU_CELLS_LARGE_ROWS; ++row)
        assert(cells.scratch[row] == 0x21212121u);
}

static void test_skip_and_width(void) {
    AosMenuCells cells;
    uint8_t metrics[14] = {0};
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_ink(&cells, 0x11);
    aos_menu_cells_set_attr(&cells, 0x5);
    metrics[0] = 0xFF;
    /* skip = 1 discards the leftmost bit: pixels 0-6 set, pixel 7 ink. */
    aos_menu_cells_blit_large(&cells, 'A', 1, 8, metrics);
    assert(cells.scratch[0] == 0x25555555u);
    /* width = 4 emits only four pixels: nibbles 0-3 attr, 4-7 ink. */
    aos_menu_cells_blit_large(&cells, 'A', 0, 4, metrics);
    assert(cells.scratch[0] == 0x21215555u);
    /* skip beyond the byte: every emitted pixel reads a zero bit. */
    aos_menu_cells_blit_large(&cells, 'A', 12, 8, metrics);
    assert(cells.scratch[0] == 0x21212121u);
}

static void test_small_rows(void) {
    AosMenuCells cells;
    uint8_t metrics[10] = {0};
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_ink(&cells, 0x11);
    aos_menu_cells_set_attr(&cells, 0x5);
    metrics[0] = 0x80;
    metrics[7] = 0x80;
    aos_menu_cells_blit_small(&cells, 'a', 0, 8, metrics);
    assert(cells.scratch[0] == 0x21212125u);
    assert(cells.scratch[7] == 0x21212125u);
    /* The small blit only owns the first 8 scratch words. */
    assert(cells.scratch[8] == 0 && cells.scratch[11] == 0);
}

int main(void) {
    test_large_basic();
    test_skip_and_width();
    test_small_rows();
    printf("aos_menu_cells tests passed\n");
    return 0;
}
