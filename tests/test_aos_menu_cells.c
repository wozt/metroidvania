/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_menu_cells.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Hand-computed expectations for the cell-engine ports.
 * ink = 0x11 -> byte (0x11 * 17) & 0xFF = 0x21 -> word 0x21212121.
 * attr = 0x5: a set pixel clears its nibble and ORs attr << px*4. */

#define FB_WORDS (AOS_MENU_CELLS_ROW_WORDS * 4)

static void test_large_basic(void) {
    AosMenuCells cells;
    uint8_t metrics[14] = {0};
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_ink(&cells, 0x11);
    aos_menu_cells_set_attr(&cells, 0x5);
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
    aos_menu_cells_blit_large(&cells, 'A', 1, 8, metrics);
    assert(cells.scratch[0] == 0x25555555u);
    aos_menu_cells_blit_large(&cells, 'A', 0, 4, metrics);
    assert(cells.scratch[0] == 0x21215555u);
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
    assert(cells.scratch[8] == 0 && cells.scratch[11] == 0);
}

static void test_merge_large_spill(void) {
    /* a_px = 0 (cell col 0, sub-x 0), a_y = 4 (cell row 0, sub-y 4):
     * 12 glyph rows cover cell rows 0 (rows 4-7) and 1 (rows 0-7). */
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    memset(fb, 0xFF, sizeof fb);
    cells.a_px = 0;
    cells.a_y = 4;
    for (unsigned i = 0; i < AOS_MENU_CELLS_LARGE_ROWS; ++i)
        cells.scratch[i] = 0x11111111u * (i + 1);
    aos_menu_cells_merge_large_single(&cells, 8);
    /* rows 0-3 land on cell(0,0) pixel rows 4-7; rows 4-11 on
     * cell(0,1) pixel rows 0-7. */
    assert(fb[4] == 0x11111111u);
    assert(fb[7] == 0x44444444u);
    assert(fb[256] == 0x55555555u);
    assert(fb[263] == 0xCCCCCCCCu);
    assert(fb[0] == 0xFFFFFFFFu && fb[3] == 0xFFFFFFFFu);
}

static void test_merge_large_dual(void) {
    /* a_px = 4 -> sub-x 4, width 8 spills into the next cell. */
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    memset(fb, 0, sizeof fb);
    cells.a_px = 4;
    cells.a_y = 0;
    for (unsigned i = 0; i < AOS_MENU_CELLS_LARGE_ROWS; ++i)
        cells.scratch[i] = 0x12345678u;
    aos_menu_cells_merge_large_dual(&cells, 8);
    /* Cell(0,0): low 4 nibbles of scratch land in nibbles 4-7.
     * Cell(1,0) (word offset +8): high 4 nibbles land in nibbles 0-3. */
    assert(fb[0] == 0x56780000u);
    assert(fb[8] == 0x00001234u);
    /* Vertical spill at row 8 -> cell row 1; every row writes both
     * cells, so cell column 2 and the tail of row 1 stay untouched. */
    assert(fb[256] == 0x56780000u);
    assert(fb[264] == 0x00001234u);
    assert(fb[12] == 0x00001234u && fb[20] == 0);
    assert(fb[16] == 0 && fb[260] == 0);
}

static void test_merge_small_and_clear(void) {
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    memset(fb, 0xFF, sizeof fb);
    cells.b_px = 4;
    cells.b_row = 0;
    for (unsigned i = 0; i < AOS_MENU_CELLS_SMALL_ROWS; ++i)
        cells.scratch[i] = 0x12345678u;
    /* small dual at sub-x 4, width 8: same split as large dual but
     * only 8 rows and no vertical spill. */
    aos_menu_cells_merge_small_dual(&cells, 8);
    /* fb prefilled 0xFF: kept nibbles stay F around the glyph bits. */
    assert(fb[0] == 0x5678FFFFu);
    assert(fb[8] == 0xFFFF1234u);
    assert(fb[7] == 0x5678FFFFu);
    assert(fb[15] == 0xFFFF1234u);
    /* clear_single(6) at sub-x 4 blanks nibbles 4-7 only. */
    memset(fb, 0xFF, sizeof fb);
    aos_menu_cells_clear_single(&cells, 6);
    for (unsigned i = 0; i < 8; ++i) assert(fb[i] == 0x0000FFFFu);
    /* clear_dual(6): cell keeps nibbles 0-3, next cell keeps 4-7. */
    memset(fb, 0xFF, sizeof fb);
    aos_menu_cells_clear_dual(&cells, 6);
    for (unsigned i = 0; i < 8; ++i) {
        assert(fb[i] == 0x0000FFFFu);
        assert(fb[i + 8] == 0xFFFFFF00u);
    }
}

static void test_print_a_cursor(void) {
    /* Drive the real printer: glyph 'A' row0 = 0x80 only. */
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    uint8_t char_info[4] = {0, 4, 0, 0};
    uint8_t glyph[14] = {0};
    glyph[0] = 0x80;
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    memset(fb, 0, sizeof fb);
    aos_menu_cells_set_ink(&cells, 0x11);
    aos_menu_cells_set_attr(&cells, 0x5);
    cells.a_col_home = 0;
    cells.a_row_stop = 2;
    cells.a_col_max = 3;
    cells.a_row_max = 5;
    /* a_x accumulates inside a column: two 4px chars overflow the 6px
     * slot (4 + 4 > 5), so each column holds one glyph advance pair —
     * px 0,4 -> col1 px6 -> px10 -> col2 px12 -> px16 -> col3 px18 -> px22. */
    for (int i = 0; i < 5; ++i) {
        assert(aos_menu_cells_print_a(&cells, 'A', char_info, glyph) == -1);
    }
    assert(cells.a_col == 2 && cells.a_px == 16);
    /* Columns 3 then overflow to row 1 column 0 with a_y = 12. */
    assert(aos_menu_cells_print_a(&cells, 'A', char_info, glyph) == -1);
    assert(cells.a_col == 3 && cells.a_px == 18);
    assert(aos_menu_cells_print_a(&cells, 'A', char_info, glyph) == -1);
    assert(cells.a_col == 3 && cells.a_px == 22);
    assert(aos_menu_cells_print_a(&cells, 'A', char_info, glyph) == -1);
    assert(cells.a_col == 0 && cells.a_row == 1);
    assert(cells.a_y == 12 && cells.a_px == 0 && cells.a_x == 0);
    /* The wrapped glyph landed in cell row 1 (a_y >> 3 = 1). */
    assert(fb[AOS_MENU_CELLS_ROW_WORDS] != 0);
}

static void test_icon_walker(void) {
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    memset(fb, 0xFF, sizeof fb);
    cells.b_x0 = 0;
    cells.b_x1 = 2;
    cells.b_y0 = 0;
    cells.b_y1 = 0;
    /* icon_begin(0,0): 6px slot at x=0 clears cell nibbles 0-5. */
    aos_menu_cells_icon_begin(&cells, 0, 0);
    /* mask = nibbles 0-5 -> keeps nibbles 6-7: 0xFF000000 */
    assert(fb[0] == 0xFF000000u);
    /* Cursor advanced one column: b_px = 6. */
    assert(cells.b_col == 1 && cells.b_px == 6);
    /* icon_begin(1,0): 6px at x=6 spans cells 0-1: clears nibbles 6-7
     * of cell(0,0) and nibbles 0-3 of cell(1,0). */
    aos_menu_cells_icon_begin(&cells, 1, 0);
    assert(fb[0] == 0x00000000u);
    assert(fb[8] == 0xFFFF0000u);
    /* fill_rect over a 1-row window restores the cursor origin. */
    aos_menu_cells_icon_fill_rect(&cells);
    assert(cells.b_col == cells.b_x0 && cells.b_row == cells.b_y0);
    assert(cells.b_sx == 0 && cells.b_px == cells.b_x0 * 6);
}

static void test_run_text(void) {
    /* 'A' = 0x41 -> char_tbl index 0x21, glyph index 0x21. */
    static uint8_t char_tbl[0x60 * 4];
    static uint8_t glyph_tbl[0x60 * 14];
    static uint8_t script[8] = {0xAA, 0xBB, 'A', 'B', 0x01, 0, 0, 0};
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    memset(fb, 0, sizeof fb);
    aos_menu_cells_set_ink(&cells, 0x11);
    aos_menu_cells_set_attr(&cells, 0x5);
    char_tbl[0x21 * 4 + 0] = 0;
    char_tbl[0x21 * 4 + 1] = 4;
    char_tbl[0x22 * 4 + 0] = 0;
    char_tbl[0x22 * 4 + 1] = 4;
    glyph_tbl[0x21 * 14] = 0x80;
    glyph_tbl[0x22 * 14] = 0x80;
    cells.a_col_home = 0;
    cells.a_row_stop = 5;
    cells.a_col_max = 50;
    cells.a_row_max = 9;
    /* 'A' prints, 'B' prints, 0x01 (< 0x20) stops the run; sub_08046BC8
     * restores the entry cursor on exit, so a_col/a_row/a_px are back
     * at their pre-run values (all zero after init). */
    int used = aos_menu_cells_run_a(&cells, script, 0, char_tbl, 4,
                                    glyph_tbl, 14);
    assert(used == 2);
    assert(cells.a_col == 0 && cells.a_row == 0 && cells.a_px == 0);
    assert(fb[0] != 0);
}

static void test_window_a_and_tile(void) {
    /* sub_08040748: hub call is window_a(0, 0, 26, 3) -> cols 0-24,
     * rows 0-2; cursor pixel position follows the current col/row. */
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    memset(fb, 0, sizeof fb);
    cells.a_col = 2;
    cells.a_row = 1;
    aos_menu_cells_window_a(&cells, 0, 0, 26, 3);
    assert(cells.a_col_home == 0 && cells.a_row_stop == 0);
    assert(cells.a_col_max == 24 && cells.a_row_max == 2);
    assert(cells.a_x == 0 && cells.a_px == 12 && cells.a_y == 12);
    /* cell_tile copies the 32-byte cell verbatim: word i lands at
     * bytes 4i..4i+3 little-endian, low nibble = leftmost pixel. */
    fb[0] = 0x01234567u;
    fb[1] = 0x89ABCDEFu;
    uint8_t tile[32];
    assert(aos_menu_cells_cell_tile(&cells, 0, 0, tile));
    assert(tile[0] == 0x67 && tile[1] == 0x45 && tile[3] == 0x01);
    assert(tile[4] == 0xEF && tile[7] == 0x89);
    assert(aos_menu_cells_cell_tile(&cells, 1, 0, tile));
    assert(tile[0] == 0 && tile[31] == 0);
    /* cell (0,1) starts at word 256. */
    fb[256] = 0xA5A5A5A5u;
    assert(aos_menu_cells_cell_tile(&cells, 0, 1, tile));
    assert(tile[0] == 0xA5);
}

static void test_window_b(void) {
    /* sub_08040970: bounds x0/y0 (clamped 40/31), x1 = x+w-2,
     * y1 = y+h-1; the pixel cursor is recomputed from the CURRENT
     * b_col/b_row — the cell cursor itself is not reset. */
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    cells.b_col = 3;
    cells.b_row = 2;
    cells.b_sx = 5;
    cells.b_px = 99;
    cells.b_py = 99;
    aos_menu_cells_window_b(&cells, 0, 0xC, 0xE, 1);
    assert(cells.b_x0 == 0 && cells.b_y0 == 0xC);
    assert(cells.b_x1 == 0xC && cells.b_y1 == 0xC);
    assert(cells.b_sx == 0);
    assert(cells.b_px == 18 && cells.b_py == 16);
    /* Clamps: x0/x1 cap at 40, y0/y1 at 31. */
    aos_menu_cells_window_b(&cells, 50, 40, 10, 10);
    assert(cells.b_x0 == 0x28 && cells.b_x1 == 0x28);
    assert(cells.b_y0 == 0x1F && cells.b_y1 == 0x1F);
}

static void test_print_at(void) {
    /* sub_08040E10/40BDC: positioned print clamps to the window and
     * advances the cursor like a normal print. */
    AosMenuCells cells;
    uint32_t fb[FB_WORDS];
    static uint8_t char_tbl[0x60 * 4];
    static uint8_t glyph_tbl[0x60 * 10];
    aos_menu_cells_init(&cells);
    aos_menu_cells_set_fb(&cells, fb);
    memset(fb, 0, sizeof fb);
    aos_menu_cells_set_ink(&cells, 0x11);
    aos_menu_cells_set_attr(&cells, 0x5);
    aos_menu_cells_window_b(&cells, 0, 0, 0x20, 4);
    char_tbl[0x21 * 4 + 1] = 4;
    glyph_tbl[0x21 * 10] = 0xF0;
    /* col 0x30 clamps to b_x1 = 0+0x20-2 = 0x1E; row 9 clamps to 3. */
    int ret = aos_menu_cells_print_at_b(&cells, 'A', char_tbl + 0x21 * 4,
                                        glyph_tbl + 0x21 * 10, 0x30, 9);
    assert(ret == -1);
    assert(cells.b_col == 0x1E && cells.b_row == 3);
    assert(cells.b_px == 0x1E * 6 + 4);
    /* Pixel x 180 -> cell column 22, row 3; width 4 at sub-x 4 fills
     * nibbles 4-7 of that cell exactly (fb[3*256 + 22*8]). */
    assert(fb[3 * 256 + 22 * 8] != 0);
}

int main(void) {
    test_large_basic();
    test_skip_and_width();
    test_small_rows();
    test_merge_large_spill();
    test_merge_large_dual();
    test_merge_small_and_clear();
    test_print_a_cursor();
    test_icon_walker();
    test_run_text();
    test_window_a_and_tile();
    test_window_b();
    test_print_at();
    printf("aos_menu_cells tests passed\n");
    return 0;
}
