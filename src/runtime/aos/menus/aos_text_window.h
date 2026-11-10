/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_TEXT_WINDOW_H
#define AOS_TEXT_WINDOW_H
#include <stddef.h>
#include <stdint.h>

/* One of Aria's two text cursors (gEwramData + 0x115A0 and + 0x115B2,
 * src/code/code_08039340.s). Text advances in 6-pixel columns; context A
 * uses 12-pixel rows (rows up to 20), context B 8-pixel rows (up to 31). */
typedef struct {
    uint16_t column, row, subpixel, pixel_x, pixel_y;
    uint16_t left, top, right, bottom;
    uint8_t row_pixels;
    uint16_t row_limit;
} AosTextCursor;

enum { AOS_TEXT_NOT_CONTROL = -1 };

/* Called for each glyph at the cursor's pixel position before it advances,
 * and for each 6-pixel cell a window clear erases. */
typedef void (*AosTextGlyphFn)(void *user, uint16_t pixel_x, uint16_t pixel_y,
                               uint8_t code, uint8_t left, uint8_t width);
typedef void (*AosTextClearFn)(void *user, uint16_t pixel_x, uint16_t pixel_y);

/* An all-zero context A or B (its EWRAM is cleared by sub_080006CC). */
void aos_text_cursor_init_a(AosTextCursor *cursor);
void aos_text_cursor_init_b(AosTextCursor *cursor);
/* sub_0804066C / sub_08040898: column and row, clamped to 40 and the row
 * limit, at the column's first pixel. */
void aos_text_set_cursor(AosTextCursor *cursor, uint16_t column, uint16_t row);
/* sub_080406E4 / sub_0804090C: a pixel column (clamped to 240) and a row.
 * Both natively place the row at 12 pixels, context B included. */
void aos_text_set_pixel_cursor(AosTextCursor *cursor, uint16_t pixel_x, uint16_t row);
/* sub_08040748 / sub_08040970: window from (column, row), width and height in
 * cells; its right edge is column + width - 2 and its bottom row + height - 1.
 * The cursor returns to the start of its current cell. */
void aos_text_set_window(AosTextCursor *cursor, uint16_t column, uint16_t row,
                         uint16_t width, uint16_t height);
/* sub_08040814 / sub_08040A38: next row at the window's left column,
 * wrapping to its top row. */
void aos_text_newline(AosTextCursor *cursor);
/* sub_08040AB8 / sub_08040C60: draws a glyph (code 0x20-0xFF, metrics are the
 * left offset and width of each 4-byte record) and advances, wrapping at the
 * window's right column and bottom row. Returns AOS_TEXT_NOT_CONTROL, or the
 * code itself when it is a control (0x00-0x1F) and nothing is drawn. */
int32_t aos_text_put(AosTextCursor *cursor, const uint8_t *metrics, uint8_t code,
                     AosTextGlyphFn draw, void *user);
/* sub_08040BDC / sub_08040E10: places the cursor (clamped to the window's
 * right column and bottom row) and puts one code. */
int32_t aos_text_put_at(AosTextCursor *cursor, const uint8_t *metrics,
                        uint16_t column, uint16_t row, uint8_t code,
                        AosTextGlyphFn draw, void *user);
/* sub_08040FE0 / sub_08041204: erases every cell from the window's left
 * column to one past its right column, top to bottom row, then returns the
 * cursor to the window's top-left cell. */
void aos_text_clear_window(AosTextCursor *cursor, AosTextClearFn clear, void *user);
/* sub_08041318 / sub_08041338: puts the entry's codes from offset 2 + start
 * until the first control, and returns that control's index (relative to
 * offset 2). The native loop is unbounded; this one stops at the entry's
 * end and then returns -1. */
int32_t aos_text_print(AosTextCursor *cursor, const uint8_t *metrics,
                       const uint8_t *entry, size_t entry_size, size_t start,
                       AosTextGlyphFn draw, void *user);
#endif
