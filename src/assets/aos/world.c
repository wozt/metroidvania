/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include <stdio.h>

enum {
    GBA_ROM_BASE = 0x08000000,
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
