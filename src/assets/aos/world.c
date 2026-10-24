/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include <stdio.h>

enum {
    GBA_ROM_BASE = 0x08000000,
    AOS_MAP_TABLE_OFFSET = 0x116650,
    AOS_AREA_DIRECTORY_OFFSET = 0x50EF08
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer, size_t size,
                           size_t *offset) {
    if (pointer < GBA_ROM_BASE) return false;
    size_t value = (size_t)(pointer - GBA_ROM_BASE);
    if (value > rom->size || size > rom->size - value) return false;
    *offset = value;
    return true;
}

bool fusion_aos_read_area_directory(const FusionRom *rom,
                                    FusionAosAreaDirectory *directory,
                                    char *error, size_t error_size) {
    if (!rom || !directory || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "expected an Aria of Sorrow ROM buffer");
        return false;
    }
    const size_t directory_size = FUSION_AOS_AREA_COUNT * sizeof(uint32_t);
    if (rom->size < AOS_AREA_DIRECTORY_OFFSET + directory_size) {
        set_error(error, error_size, "ROM is too short for the Aria area directory");
        return false;
    }

    *directory = (FusionAosAreaDirectory){0};
    uint32_t pointers[FUSION_AOS_AREA_COUNT];
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        pointers[area] = read_u32(rom->data + AOS_AREA_DIRECTORY_OFFSET + area * 4);
    }

    const uint32_t table_end = GBA_ROM_BASE + AOS_AREA_DIRECTORY_OFFSET;
    const uint32_t rom_end = GBA_ROM_BASE + (uint32_t)rom->size;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        uint32_t start = pointers[area];
        uint32_t end = area + 1 < FUSION_AOS_AREA_COUNT ? pointers[area + 1] : table_end;
        if (start < GBA_ROM_BASE || start >= rom_end || end <= start ||
            end > table_end || (end - start) % sizeof(uint32_t) != 0) {
            char message[128];
            snprintf(message, sizeof message,
                     "invalid room-pointer table bounds for Aria area %zu", area);
            set_error(error, error_size, message);
            return false;
        }
        directory->areas[area].room_table_pointer = start;
        directory->areas[area].room_count = (end - start) / sizeof(uint32_t);
        directory->total_rooms += directory->areas[area].room_count;
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_aos_read_map(const FusionRom *rom,
                         const FusionAosAreaDirectory *directory,
                         FusionAosMap *map, char *error, size_t error_size) {
    const size_t map_size = FUSION_AOS_MAP_CELL_CAPACITY * sizeof(uint16_t);
    if (!rom || !directory || !map || !rom->data ||
        rom->game != FUSION_ASSET_GAME_ARIA) {
        set_error(error, error_size, "expected an Aria of Sorrow ROM buffer and directory");
        return false;
    }
    if (rom->size < AOS_MAP_TABLE_OFFSET + map_size) {
        set_error(error, error_size, "ROM is too short for the Aria map table");
        return false;
    }

    *map = (FusionAosMap){0};
    bool seen_save[FUSION_AOS_AREA_COUNT][64] = {{false}};
    for (size_t y = 0; y < FUSION_AOS_MAP_HEIGHT; ++y) {
        for (size_t x = 0; x < FUSION_AOS_MAP_WIDTH; ++x) {
            size_t index = y * FUSION_AOS_MAP_WIDTH + x;
            uint16_t value = read_u16(rom->data + AOS_MAP_TABLE_OFFSET + index * 2);
            if (value == 0xFFFF) continue;
            size_t area = (value >> 6) & 0xF;
            size_t room = value & 0x3F;
            if (area >= FUSION_AOS_AREA_COUNT ||
                room >= directory->areas[area].room_count) {
                char message[128];
                snprintf(message, sizeof message,
                         "Aria map cell (%zu,%zu) references invalid room %zu:%zu",
                         x, y, area, room);
                set_error(error, error_size, message);
                return false;
            }
            size_t table_offset;
            if (!pointer_offset(rom, directory->areas[area].room_table_pointer,
                                directory->areas[area].room_count * sizeof(uint32_t),
                                &table_offset)) {
                set_error(error, error_size, "Aria room table points outside the ROM");
                return false;
            }
            uint32_t room_pointer = read_u32(rom->data + table_offset + room * 4);
            size_t ignored;
            if (!pointer_offset(rom, room_pointer, 1, &ignored)) {
                set_error(error, error_size, "Aria map cell room pointer is outside the ROM");
                return false;
            }
            bool save = (value & 0x8000) != 0;
            bool warp = (value & 0x4000) != 0;
            if (save && seen_save[area][room]) {
                char message[128];
                snprintf(message, sizeof message,
                         "Aria save room %zu:%zu occupies multiple map cells", area, room);
                set_error(error, error_size, message);
                return false;
            }
            seen_save[area][room] = seen_save[area][room] || save;
            FusionAosMapCell *cell = &map->cells[map->count++];
            *cell = (FusionAosMapCell){(uint8_t)x, (uint8_t)y, (uint8_t)area,
                                       (uint8_t)room, save, warp, room_pointer};
            map->save_count += save ? 1u : 0u;
            map->warp_count += warp ? 1u : 0u;
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}
