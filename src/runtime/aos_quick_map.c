/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_quick_map.h"

#include "aos_rom_graphics.h"

#include <stdio.h>
#include <string.h>

enum {
    AOS_ROM_BASE = 0x08000000,
    AOS_QUICK_MAP_GRAPHICS = 0x080E3664,
    AOS_QUICK_MAP_GRAPHICS_SIZE = 0x1C00,
    AOS_QUICK_MAP_PALETTE = 0x081183CC,
    AOS_QUICK_MAP_CASTLE = 0x08116650,
    AOS_QUICK_MAP_CONNECTIONS = 0x08117DD0,
    AOS_QUICK_MAP_SCREEN_WIDTH = 240,
    AOS_QUICK_MAP_SCREEN_HEIGHT = 160,
};

static void set_error(char *error, size_t size, const char *message) {
    if (!error || !size) return;
    snprintf(error, size, "%s", message);
    error[size - 1] = '\0';
}

static const uint8_t *rom_slice(const FusionRom *rom, uint32_t pointer,
                                size_t size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA ||
        pointer < AOS_ROM_BASE)
        return NULL;
    size_t offset = pointer - AOS_ROM_BASE;
    if (offset > rom->size || size > rom->size - offset) return NULL;
    return rom->data + offset;
}

static uint16_t read_u16(const uint8_t *bytes) {
    return (uint16_t)(bytes[0] | bytes[1] << 8);
}

bool aos_quick_map_init(AosQuickMap *map, const FusionRom *rom,
                        char *error, size_t error_size) {
    if (!map || !rom) {
        set_error(error, error_size, "invalid Aria quick-map request");
        return false;
    }
    const uint8_t *graphics = rom_slice(rom, AOS_QUICK_MAP_GRAPHICS,
                                         AOS_QUICK_MAP_GRAPHICS_SIZE);
    const uint8_t *castle = rom_slice(rom, AOS_QUICK_MAP_CASTLE,
                                      AOS_QUICK_MAP_CELLS * 2);
    const uint8_t *connections = rom_slice(rom, AOS_QUICK_MAP_CONNECTIONS,
                                            32 * AOS_QUICK_MAP_HEIGHT);
    if (!graphics || !castle || !connections) {
        set_error(error, error_size, "truncated Aria quick-map resources");
        return false;
    }
    *map = (AosQuickMap){0};
    memcpy(map->vram + 0x6000, graphics, AOS_QUICK_MAP_GRAPHICS_SIZE);
    memcpy(map->connections, connections, sizeof map->connections);
    for (size_t index = 0; index < AOS_QUICK_MAP_CELLS; ++index)
        map->castle[index] = read_u16(castle + index * 2);
    if (!aos_rom_load_palette_resource(rom, AOS_QUICK_MAP_PALETTE,
                                       map->palette, error, error_size)) {
        *map = (AosQuickMap){0};
        return false;
    }
    if (error && error_size) error[0] = '\0';
    return true;
}

bool aos_quick_map_visit(AosQuickMap *map, const FusionRom *rom,
                         unsigned area, unsigned room,
                         int player_x, int player_y,
                         char *error, size_t error_size) {
    if (!map || !rom || area >= FUSION_AOS_AREA_COUNT ||
        (map->directory.total_rooms &&
         room >= map->directory.areas[area].room_count)) {
        set_error(error, error_size, "invalid Aria quick-map room");
        return false;
    }
    if (!map->directory.total_rooms &&
        !fusion_aos_read_area_directory(rom, &map->directory,
                                        error, error_size))
        return false;
    if (room >= map->directory.areas[area].room_count) {
        set_error(error, error_size, "invalid Aria quick-map room");
        return false;
    }
    FusionAosRoomDescriptor descriptor;
    if (!fusion_aos_read_room_descriptor(rom, &map->directory, area, room,
                                         &descriptor, error, error_size))
        return false;
    int x = descriptor.map_x + (player_x >> 8);
    int y = descriptor.map_y + (player_y >> 8);
    if (x < 0 || x >= AOS_QUICK_MAP_WIDTH ||
        y < 0 || y >= AOS_QUICK_MAP_HEIGHT) {
        set_error(error, error_size, "Aria quick-map position is outside the castle");
        return false;
    }
    map->explored[y * AOS_QUICK_MAP_WIDTH + x] = 1;
    map->current_x = (uint8_t)x;
    map->current_y = (uint8_t)y;
    map->has_current = true;
    if (error && error_size) error[0] = '\0';
    return true;
}

static uint16_t *cell_word(AosQuickMap *map, int block, int x, int y) {
    size_t word = (size_t)(block + 0x22) * 16 + (size_t)(y & 1) * 8 + (x & 1);
    return (uint16_t *)map->vram + word;
}

static int connection(const AosQuickMap *map, int x, int y) {
    if (x < 0 || x >= AOS_QUICK_MAP_WIDTH ||
        y < 0 || y >= AOS_QUICK_MAP_HEIGHT)
        return 0;
    uint8_t pair = map->connections[y * 32 + (x >> 1)];
    return x & 1 ? pair & 0xF : pair >> 4;
}

static void vertical_edge(AosQuickMap *map, int x, int y,
                          int color, int border, int edge) {
    if (map->castle[y * AOS_QUICK_MAP_WIDTH + x] == 0xFFFF) color = 0;
    int block = (x >> 1) + ((y >> 1) << 5);
    edge >>= 2;
    if (edge == 1) edge = 3; /* undiscovered breakable-map edge */
    int fill = edge == 0 ? (color << 4) | (color << 8) | (color << 12)
                         : edge == 2 ? 0xBBB0
                                     : (border << 4) | (border << 8) | (border << 12);
    uint16_t *word = cell_word(map, block, x, y);
    int first = *word & 0xF;
    *word = (uint16_t)(fill | ((first == border || (edge & 2)) ? border : first));
    if (edge & 2) {
        block = ((x + 1) >> 1) + ((y >> 1) << 5);
        word = cell_word(map, block, x + 1, y);
        *word = (uint16_t)((*word & 0xFFF0) | border);
    }
}

static void horizontal_edge(AosQuickMap *map, int x, int y,
                            int color, int border, int edge) {
    if (map->castle[y * AOS_QUICK_MAP_WIDTH + x] == 0xFFFF) color = 0;
    edge &= 3;
    if (edge == 1) edge = 3;
    int block = (x >> 1) + ((y >> 1) << 5);
    uint16_t *word = cell_word(map, block, x, y);
    for (int row = 0; row < 4; ++row, word += 2) {
        int first = *word & 0xF;
        int value;
        if (edge == 2) value = 0xB;
        else if (row == 0)
            value = (edge & 2) || first == 5 ? border : color;
        else if (row == 2)
            value = edge == 3 ? border : color;
        else
            value = edge & 2 ? border : color;
        if (first != border) *word = (uint16_t)((*word & 0xFFF0) | value);
    }
    block = (x >> 1) + (((y + 1) >> 1) << 5);
    word = cell_word(map, block, x, y + 1);
    int first = *word & 0xF;
    int value = first ? (first == 5 || first == 1 ? border : color)
                      : (edge & 2 ? border : color);
    *word = (uint16_t)((*word & 0xFFF0) | value);
}

static void draw_cell(AosQuickMap *map, int x, int y, int color) {
    int border = color == 4 ? 5 : 1;
    int block = (x >> 1) + ((y >> 1) << 5);
    uint16_t *word = cell_word(map, block, x, y);
    uint16_t fill = (uint16_t)(color | color << 4 | color << 8 | color << 12);
    for (int row = 1; row < 4; ++row) word[row * 2] = fill;
    vertical_edge(map, x, y, color, border, connection(map, x, y));
    horizontal_edge(map, x, y, color, border, connection(map, x, y));
    vertical_edge(map, x, y + 1, color, border, connection(map, x, y + 1));
    horizontal_edge(map, x + 1, y, color, border, connection(map, x + 1, y));
}

static bool map_item_exception(int x, int y) {
    static const uint8_t exceptions[][2] = {
        {4, 13}, {12, 13}, {13, 13}, {15, 22}, {47, 20},
    };
    for (size_t index = 0; index < sizeof exceptions / sizeof exceptions[0]; ++index)
        if (x == exceptions[index][0] && y == exceptions[index][1]) return true;
    return false;
}

bool aos_quick_map_render(AosQuickMap *map, bool castle_map_1,
                          bool castle_map_2, bool castle_map_3,
                          uint32_t animation_frame,
                          uint8_t *rgba, size_t rgba_size) {
    if (!map || !rgba || rgba_size < AOS_QUICK_MAP_SCREEN_WIDTH *
                                      AOS_QUICK_MAP_SCREEN_HEIGHT * 4)
        return false;
    memset(map->vram, 0, 0x6000);
    for (int y = 0; y < AOS_QUICK_MAP_HEIGHT; ++y)
        for (int x = 0; x < AOS_QUICK_MAP_WIDTH; ++x) {
            size_t index = (size_t)y * AOS_QUICK_MAP_WIDTH + x;
            uint16_t room = map->castle[index];
            if (room == 0xFFFF) continue;
            unsigned area = (room >> 6) & 0xF;
            bool charted = (castle_map_1 && (area == 0 || area == 1)) ||
                           (castle_map_2 && area == 3) ||
                           (castle_map_3 && area == 5);
            if (charted && !map_item_exception(x, y)) draw_cell(map, x, y, 4);
            if (map->explored[index])
                draw_cell(map, x, y, room & 0x8000 ? 2 : room & 0x4000 ? 3 : 9);
        }

    for (unsigned y = 0; y < AOS_QUICK_MAP_SCREEN_HEIGHT; ++y)
        for (unsigned x = 0; x < AOS_QUICK_MAP_SCREEN_WIDTH; ++x) {
            unsigned source_x = x + 2; /* BG0HOFS = 2 in sub_08010D60. */
            size_t tile = (y / 8) * 32 + source_x / 8;
            size_t byte = tile * 32 + (y & 7) * 4 + (source_x & 7) / 2;
            unsigned nibble = (map->vram[byte] >> ((source_x & 1) * 4)) & 0xF;
            size_t palette = nibble * 2;
            uint16_t bgr = (uint16_t)(map->palette[palette] |
                                      map->palette[palette + 1] << 8);
            size_t out = ((size_t)y * AOS_QUICK_MAP_SCREEN_WIDTH + x) * 4;
            rgba[out] = (uint8_t)(((bgr & 31) * 255) / 31);
            rgba[out + 1] = (uint8_t)((((bgr >> 5) & 31) * 255) / 31);
            rgba[out + 2] = (uint8_t)((((bgr >> 10) & 31) * 255) / 31);
            rgba[out + 3] = 255;
        }
    if (map->has_current) {
        int center_x = (map->current_x + 4) * 4;
        int center_y = (map->current_y + 2) * 4 + 2;
        int phase = (int)(animation_frame >> 2) & 15;
        int radius = 16 - phase;
        int brightness = phase + 1;
        for (int y = center_y - radius; y <= center_y + radius; ++y)
            for (int x = center_x - radius; x <= center_x + radius; ++x) {
                if (x < 0 || x >= AOS_QUICK_MAP_SCREEN_WIDTH ||
                    y < 0 || y >= AOS_QUICK_MAP_SCREEN_HEIGHT ||
                    (x - center_x) * (x - center_x) +
                        (y - center_y) * (y - center_y) > radius * radius)
                    continue;
                size_t out = ((size_t)y * AOS_QUICK_MAP_SCREEN_WIDTH + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                    rgba[out + channel] = (uint8_t)(
                        rgba[out + channel] +
                        (255 - rgba[out + channel]) * brightness / 16);
            }
    }
    return true;
}
