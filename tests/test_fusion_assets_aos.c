/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    GBA_ROM_BASE = 0x08000000,
    DIRECTORY_OFFSET = 0x50EF08,
    FIXTURE_SIZE = DIRECTORY_OFFSET + FUSION_AOS_AREA_COUNT * 4
};

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

int main(void) {
    uint8_t *bytes = calloc(1, FIXTURE_SIZE);
    assert(bytes);
    size_t total_entries = FUSION_AOS_AREA_COUNT * (FUSION_AOS_AREA_COUNT + 1) / 2;
    size_t table_offset = DIRECTORY_OFFSET - total_entries * 4;
    size_t cursor = table_offset;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        write_u32(bytes, DIRECTORY_OFFSET + area * 4,
                  GBA_ROM_BASE + (uint32_t)cursor);
        cursor += (area + 1) * 4;
    }
    assert(cursor == DIRECTORY_OFFSET);

    FusionRom rom = {bytes, FIXTURE_SIZE, FUSION_ASSET_GAME_ARIA};
    FusionAosAreaDirectory directory;
    char error[160];
    assert(fusion_aos_read_area_directory(&rom, &directory, error, sizeof error));
    assert(directory.total_rooms == total_entries);
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        assert(directory.areas[area].room_count == area + 1);
    }

    rom.game = FUSION_ASSET_GAME_METROID;
    assert(!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error));
    rom.game = FUSION_ASSET_GAME_ARIA;
    write_u32(bytes, DIRECTORY_OFFSET + 4,
              directory.areas[0].room_table_pointer);
    assert(!fusion_aos_read_area_directory(&rom, &directory, error, sizeof error));
    free(bytes);
    puts("Native Aria area directory tests passed");
    return 0;
}
