/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_text_window.h"

#include "aos_intro_font.h"
#include "aos_intro_resources.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t x[64], y[64];
    uint8_t code[64];
    size_t count;
} Log;

static void glyph(void *user, uint16_t pixel_x, uint16_t pixel_y, uint8_t code,
                  uint8_t left, uint8_t width) {
    (void)left;
    (void)width;
    Log *log = user;
    log->x[log->count] = pixel_x;
    log->y[log->count] = pixel_y;
    log->code[log->count++] = code;
}

static void cell(void *user, uint16_t pixel_x, uint16_t pixel_y) {
    Log *log = user;
    log->x[log->count] = pixel_x;
    log->y[log->count++] = pixel_y;
}

int main(int argc, char **argv) {
    /* 4-byte metric records from code 0x20: 'A' is 5 pixels wide, 'W' 7,
     * 'i' 2. */
    static uint8_t metrics[0xE0 * 4];
    metrics[('A' - 0x20) * 4 + 1] = 5;
    metrics[('W' - 0x20) * 4 + 1] = 7;
    metrics[('i' - 0x20) * 4 + 1] = 2;

    AosTextCursor a;
    aos_text_cursor_init_a(&a);
    /* sub_0804066C: cells of 6x12 pixels, clamped to column 40, row 20. */
    aos_text_set_cursor(&a, 3, 2);
    assert(a.column == 3 && a.row == 2 && a.pixel_x == 18 && a.pixel_y == 24 &&
           a.subpixel == 0);
    aos_text_set_cursor(&a, 99, 99);
    assert(a.column == 40 && a.row == 20 && a.pixel_x == 240 && a.pixel_y == 240);
    /* sub_080406E4: a pixel column split into cell and remainder. */
    aos_text_set_pixel_cursor(&a, 50, 1);
    assert(a.column == 8 && a.subpixel == 2 && a.pixel_x == 50 && a.pixel_y == 12);
    aos_text_set_pixel_cursor(&a, 300, 1);
    assert(a.pixel_x == 240 && a.column == 40 && a.subpixel == 0);

    /* sub_08040748 (the main menu's window): right edge x + w - 2, bottom
     * y + h - 1; the cursor returns to the start of its cell. */
    aos_text_set_cursor(&a, 2, 2);
    a.subpixel = 4;
    a.pixel_x = 99;
    aos_text_set_window(&a, 0, 1, 0x25, 3);
    assert(a.left == 0 && a.top == 1 && a.right == 0x23 && a.bottom == 3);
    assert(a.column == 2 && a.row == 2 && a.subpixel == 0 && a.pixel_x == 12);
    AosTextCursor clamped;
    aos_text_cursor_init_a(&clamped);
    aos_text_set_window(&clamped, 0, 0, 0x29, 30);
    assert(clamped.right == 0x27 && clamped.bottom == 20);

    /* sub_08040AB8: glyphs advance by width; the cell changes once the
     * remainder passes 5, and the pixel position keeps running. */
    Log log = {0};
    aos_text_set_cursor(&a, 0, 1);
    assert(aos_text_put(&a, metrics, 'A', glyph, &log) == AOS_TEXT_NOT_CONTROL);
    assert(log.x[0] == 0 && log.y[0] == 12 && a.subpixel == 5 && a.column == 0 &&
           a.pixel_x == 5);
    aos_text_put(&a, metrics, 'W', glyph, &log);
    assert(log.x[1] == 5 && a.subpixel == 6 && a.column == 1 && a.pixel_x == 12);
    aos_text_put(&a, metrics, 'i', glyph, &log);
    assert(a.subpixel == 2 && a.column == 2 && a.pixel_x == 14);
    /* Controls are returned and draw nothing. */
    assert(aos_text_put(&a, metrics, 6, glyph, &log) == 6 && log.count == 3);

    /* Passing the right edge wraps to the left column of the next row, then
     * from the bottom row back to the top. */
    aos_text_set_cursor(&a, 0x23, 3);
    a.subpixel = 3;
    aos_text_put(&a, metrics, 'A', glyph, &log);
    assert(a.column == 0 && a.row == 1 && a.subpixel == 0 && a.pixel_x == 0 &&
           a.pixel_y == 12);

    /* sub_08040814: newline at the left column, wrapping to the top. */
    aos_text_set_cursor(&a, 7, 2);
    aos_text_newline(&a);
    assert(a.column == 0 && a.row == 3 && a.pixel_y == 36);
    aos_text_newline(&a);
    assert(a.row == 1);

    /* sub_08040BDC clamps to the window's right column and bottom row. */
    log.count = 0;
    aos_text_put_at(&a, metrics, 50, 9, 'A', glyph, &log);
    assert(log.x[0] == 0x23 * 6 && log.y[0] == 36);

    /* sub_08040FE0: every cell from the left column to one past the right,
     * top to bottom, then the top-left cell. */
    log.count = 0;
    AosTextCursor small;
    aos_text_cursor_init_a(&small);
    aos_text_set_window(&small, 2, 1, 4, 2);
    aos_text_set_cursor(&small, 4, 2);
    aos_text_clear_window(&small, cell, &log);
    assert(log.count == 4 * 2);
    assert(log.x[0] == 12 && log.y[0] == 12 && log.x[3] == 30 && log.y[7] == 24);
    assert(small.column == 2 && small.row == 1 && small.pixel_x == 12 &&
           small.pixel_y == 12);

    /* sub_08041318: codes from offset 2 up to the first control, whose
     * index (from offset 2) is returned; the entry's end stops a bounded run. */
    static const uint8_t entry[] = {0, 0, 'A', 'i', 6, 'W', 10};
    log.count = 0;
    aos_text_set_cursor(&a, 0, 1);
    assert(aos_text_print(&a, metrics, entry, sizeof entry, 0, glyph, &log) == 2);
    assert(log.count == 2 && log.code[0] == 'A' && log.code[1] == 'i');
    assert(aos_text_print(&a, metrics, entry, sizeof entry, 3, glyph, &log) == 4);
    assert(log.count == 3 && log.code[2] == 'W');
    static const uint8_t open[] = {0, 0, 'A'};
    assert(aos_text_print(&a, metrics, open, sizeof open, 0, glyph, &log) == -1);

    /* Context B: 8-pixel rows up to 31 (sub_08040898), but the pixel cursor
     * (sub_0804090C) still places rows at 12 pixels. */
    AosTextCursor b;
    aos_text_cursor_init_b(&b);
    aos_text_set_cursor(&b, 2, 3);
    assert(b.pixel_x == 12 && b.pixel_y == 24);
    aos_text_set_cursor(&b, 0, 40);
    assert(b.row == 31 && b.pixel_y == 248);
    aos_text_set_pixel_cursor(&b, 13, 3);
    assert(b.column == 2 && b.subpixel == 1 && b.pixel_y == 36);
    aos_text_set_window(&b, 0, 0, 0x1E, 4);
    assert(b.right == 0x1C && b.bottom == 3);

    /* Optional local check: every main-menu text, printed with the ROM's
     * font metrics from its native cell, ends before the window's right edge
     * and stays on its row (nothing wraps). */
    if (argc == 2) {
        FusionRom rom = {0};
        char error[256];
        AosIntroResources resources;
        AosIntroFont font;
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &rom, error,
                               sizeof error));
        assert(aos_intro_resources_init(&resources, &rom, error, sizeof error));
        assert(aos_intro_font_init(&font, &rom, error, sizeof error));
        static const uint16_t ids[] = {0x354, 0x355, 0x356, 0x358, 0x359, 0x35A,
                                       0x35B, 0x35C, 0x377, 0x379};
        for (uint8_t language = 1; language <= 3; ++language) {
            for (size_t index = 0; index < sizeof ids / sizeof ids[0]; ++index) {
                const uint8_t *text;
                size_t size;
                assert(aos_intro_resources_entry(&resources, language, ids[index],
                                                 &text, &size));
                AosTextCursor menu;
                aos_text_cursor_init_a(&menu);
                aos_text_set_window(&menu, 0, 1, 0x25, 3);
                aos_text_set_cursor(&menu, 0, 1);
                int32_t stop = aos_text_print(&menu, font.metrics, text, size, 0,
                                              NULL, NULL);
                if (stop < 0 || menu.row != 1)
                    fprintf(stderr, "text 0x%X language %u: stop %d row %u\n",
                            ids[index], language, stop, menu.row);
                assert(stop >= 0 && menu.row == 1);
            }
        }
        fusion_rom_close(&rom);
    }
    puts("Aria native text window tests passed");
    return 0;
}
