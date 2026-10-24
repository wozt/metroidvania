/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/aos.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GBA_ROM_BASE = 0x08000000,
    MAP_OFFSET = 0x116650,
    DIRECTORY_OFFSET = 0x50EF08,
    FIXTURE_SIZE = DIRECTORY_OFFSET + FUSION_AOS_AREA_COUNT * 4
};

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

static void write_u16(uint8_t *data, size_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
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
        for (size_t room = 0; room <= area; ++room) {
            write_u32(bytes, cursor + room * 4,
                      GBA_ROM_BASE + 0x100u + (uint32_t)(area * 64 + room));
        }
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

    memset(bytes + MAP_OFFSET, 0xFF,
           FUSION_AOS_MAP_CELL_CAPACITY * sizeof(uint16_t));
    write_u16(bytes, MAP_OFFSET, 0x8000);
    write_u16(bytes, MAP_OFFSET + 2, (uint16_t)(0x4000 | (1u << 6) | 1u));
    FusionAosMap map;
    assert(fusion_aos_read_map(&rom, &directory, &map, error, sizeof error));
    assert(map.count == 2 && map.save_count == 1 && map.warp_count == 1);
    assert(map.cells[0].map_x == 0 && map.cells[0].map_y == 0);
    assert(map.cells[0].area == 0 && map.cells[0].room == 0 && map.cells[0].save);
    assert(map.cells[1].area == 1 && map.cells[1].room == 1 && map.cells[1].warp);
    write_u16(bytes, MAP_OFFSET + 4, 0x8000);
    assert(!fusion_aos_read_map(&rom, &directory, &map, error, sizeof error));
    write_u16(bytes, MAP_OFFSET + 4, 0xFFFF);
    write_u16(bytes, MAP_OFFSET, (uint16_t)(15u << 6));
    assert(!fusion_aos_read_map(&rom, &directory, &map, error, sizeof error));
    write_u16(bytes, MAP_OFFSET, 0x8000);

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
