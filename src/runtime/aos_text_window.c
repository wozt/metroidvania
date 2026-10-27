/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_text_window.h"

enum {
    AOS_TEXT_COLUMN_LIMIT = 0x28,
    AOS_TEXT_PIXEL_LIMIT = 0xF0,
    AOS_TEXT_CELL_PIXELS = 6,
};

static void init(AosTextCursor *cursor, uint8_t row_pixels, uint16_t row_limit) {
    *cursor = (AosTextCursor){.row_pixels = row_pixels, .row_limit = row_limit};
}

void aos_text_cursor_init_a(AosTextCursor *cursor) { init(cursor, 12, 0x14); }
void aos_text_cursor_init_b(AosTextCursor *cursor) { init(cursor, 8, 0x1F); }

/* The common tail: subpixel 0, pixel position of the current cell. */
static void cell_start(AosTextCursor *cursor) {
    cursor->subpixel = 0;
    cursor->pixel_x = (uint16_t)(cursor->column * AOS_TEXT_CELL_PIXELS);
    cursor->pixel_y = (uint16_t)(cursor->row * cursor->row_pixels);
}

void aos_text_set_cursor(AosTextCursor *cursor, uint16_t column, uint16_t row) {
    cursor->column = column > AOS_TEXT_COLUMN_LIMIT ? AOS_TEXT_COLUMN_LIMIT : column;
    cursor->row = row > cursor->row_limit ? cursor->row_limit : row;
    cell_start(cursor);
}

void aos_text_set_pixel_cursor(AosTextCursor *cursor, uint16_t pixel_x, uint16_t row) {
    if (pixel_x > AOS_TEXT_PIXEL_LIMIT) pixel_x = AOS_TEXT_PIXEL_LIMIT;
    if (row > cursor->row_limit) row = cursor->row_limit;
    cursor->column = (uint16_t)(pixel_x / AOS_TEXT_CELL_PIXELS);
    cursor->subpixel = (uint16_t)(pixel_x % AOS_TEXT_CELL_PIXELS);
    cursor->pixel_x = pixel_x;
    cursor->row = row;
    cursor->pixel_y = (uint16_t)(row * 12);
}

void aos_text_set_window(AosTextCursor *cursor, uint16_t column, uint16_t row,
                         uint16_t width, uint16_t height) {
    /* The edges are 16-bit sums, clamped after the store as natively. */
    cursor->left = column;
    cursor->top = row;
    cursor->right = (uint16_t)(column + width - 2);
    cursor->bottom = (uint16_t)(row + height - 1);
    if (cursor->left > AOS_TEXT_COLUMN_LIMIT) cursor->left = AOS_TEXT_COLUMN_LIMIT;
    if (cursor->top > cursor->row_limit) cursor->top = cursor->row_limit;
    if (cursor->right > AOS_TEXT_COLUMN_LIMIT) cursor->right = AOS_TEXT_COLUMN_LIMIT;
    if (cursor->bottom > cursor->row_limit) cursor->bottom = cursor->row_limit;
    cell_start(cursor);
}

void aos_text_newline(AosTextCursor *cursor) {
    cursor->column = cursor->left;
    if (++cursor->row > cursor->bottom) cursor->row = cursor->top;
    cell_start(cursor);
}

int32_t aos_text_put(AosTextCursor *cursor, const uint8_t *metrics, uint8_t code,
                     AosTextGlyphFn draw, void *user) {
    if (!(code & 0xE0)) return code;
    const uint8_t *metric = metrics + (size_t)(code - 0x20) * 4;
    uint8_t left = metric[0], width = metric[1];
    if (draw) draw(user, cursor->pixel_x, cursor->pixel_y, code, left, width);
    uint16_t subpixel = (uint16_t)(cursor->subpixel + width);
    cursor->subpixel = subpixel;
    if (subpixel > 5) {
        cursor->subpixel = (uint16_t)(subpixel - AOS_TEXT_CELL_PIXELS);
        if (++cursor->column > cursor->right) {
            cursor->column = cursor->left;
            if (++cursor->row > cursor->bottom) cursor->row = cursor->top;
            cell_start(cursor);
            return AOS_TEXT_NOT_CONTROL;
        }
    }
    cursor->pixel_x = (uint16_t)(cursor->pixel_x + width);
    return AOS_TEXT_NOT_CONTROL;
}

int32_t aos_text_put_at(AosTextCursor *cursor, const uint8_t *metrics,
                        uint16_t column, uint16_t row, uint8_t code,
                        AosTextGlyphFn draw, void *user) {
    cursor->column = column > cursor->right ? cursor->right : column;
    cursor->row = row > cursor->bottom ? cursor->bottom : row;
    cell_start(cursor);
    return aos_text_put(cursor, metrics, code, draw, user);
}

void aos_text_clear_window(AosTextCursor *cursor, AosTextClearFn clear, void *user) {
    for (uint16_t row = cursor->top; row <= cursor->bottom; ++row)
        for (uint16_t column = cursor->left; column <= cursor->right + 1; ++column)
            if (clear)
                clear(user, (uint16_t)(column * AOS_TEXT_CELL_PIXELS),
                      (uint16_t)(row * cursor->row_pixels));
    cursor->column = cursor->left;
    cursor->row = cursor->top;
    cell_start(cursor);
}

int32_t aos_text_print(AosTextCursor *cursor, const uint8_t *metrics,
                       const uint8_t *entry, size_t entry_size, size_t start,
                       AosTextGlyphFn draw, void *user) {
    for (size_t index = start; 2 + index < entry_size; ++index)
        if (aos_text_put(cursor, metrics, entry[2 + index], draw, user) !=
            AOS_TEXT_NOT_CONTROL)
            return (int32_t)index;
    return -1;
}
