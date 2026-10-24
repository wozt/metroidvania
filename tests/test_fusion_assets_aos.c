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
    ROOM_BASE_OFFSET = 0x1000,
    ROOM_STRIDE = 0x40,
    RESOLVED_ROOM_OFFSET = 0x6000,
    BACKGROUND_OFFSET = 0x7000,
    BACKGROUND_METADATA_OFFSET = 0x7100,
    GRAPHICS_LOAD_OFFSET = 0x7200,
    PALETTE_LOAD_OFFSET = 0x7300,
    UNTERMINATED_LOAD_OFFSET = 0x7400,
    ENTITY_OFFSET = 0x7800,
    UNTERMINATED_ENTITY_OFFSET = 0x9000,
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

static void write_room_descriptor(uint8_t *data, size_t offset, uint16_t selector,
                                  uint32_t next_pointer, uint16_t display_control,
                                  uint16_t coordinates) {
    write_u16(data, offset, display_control);
    write_u16(data, offset + 2, selector);
    write_u32(data, offset + 4, next_pointer);
    write_u32(data, offset + 24, GBA_ROM_BASE + (uint32_t)offset);
    write_u16(data, offset + 30, 0x1040);
    write_u16(data, offset + 34, coordinates);
}

static void write_transition(uint8_t *data, size_t offset, uint32_t target,
                             int8_t source_x, int8_t source_y, uint16_t seed) {
    write_u32(data, offset, target);
    data[offset + 4] = (uint8_t)source_x;
    data[offset + 5] = (uint8_t)source_y;
    for (size_t field = 0; field < 5; ++field) {
        write_u16(data, offset + 6 + field * 2, (uint16_t)(seed + field));
    }
}

static void write_background(uint8_t *data, size_t offset, uint8_t field_0,
                             uint8_t field_1, uint16_t seed,
                             uint32_t metadata_pointer) {
    data[offset] = field_0;
    data[offset + 1] = field_1;
    write_u16(data, offset + 2, seed);
    write_u16(data, offset + 4, (uint16_t)(seed + 1));
    write_u16(data, offset + 6, (uint16_t)(seed + 2));
    write_u32(data, offset + 8, metadata_pointer);
}

static void write_load_entry(uint8_t *data, size_t offset, uint32_t resource,
                             uint8_t parameter_0, uint8_t parameter_1,
                             uint8_t parameter_2) {
    write_u32(data, offset, resource);
    data[offset + 4] = parameter_0;
    data[offset + 5] = parameter_1;
    data[offset + 6] = parameter_2;
}

static void write_entity(uint8_t *data, size_t offset, int16_t x, int16_t y,
                         uint8_t persistent_index, uint8_t kind,
                         uint8_t entity_id, uint8_t flags,
                         uint16_t parameter_0, uint16_t parameter_1) {
    write_u16(data, offset, (uint16_t)x);
    write_u16(data, offset + 2, (uint16_t)y);
    data[offset + 4] = persistent_index;
    data[offset + 5] = kind;
    data[offset + 6] = entity_id;
    data[offset + 7] = flags;
    write_u16(data, offset + 8, parameter_0);
    write_u16(data, offset + 10, parameter_1);
}

int main(void) {
    uint8_t *bytes = calloc(1, FIXTURE_SIZE);
    assert(bytes);
    size_t total_entries = FUSION_AOS_AREA_COUNT * (FUSION_AOS_AREA_COUNT + 1) / 2;
    size_t table_offset = DIRECTORY_OFFSET - total_entries * 4;
    size_t cursor = table_offset;
    size_t descriptor_index = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        write_u32(bytes, DIRECTORY_OFFSET + area * 4,
                  GBA_ROM_BASE + (uint32_t)cursor);
        for (size_t room = 0; room <= area; ++room) {
            size_t descriptor_offset = ROOM_BASE_OFFSET + descriptor_index * ROOM_STRIDE;
            write_u32(bytes, cursor + room * 4,
                      GBA_ROM_BASE + (uint32_t)descriptor_offset);
            write_room_descriptor(bytes, descriptor_offset, 0xFFFF, 0, 0x1F00,
                                  (uint16_t)(area | (room << 7)));
            ++descriptor_index;
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

    write_room_descriptor(bytes, ROOM_BASE_OFFSET, 0,
                          GBA_ROM_BASE + RESOLVED_ROOM_OFFSET, 0, 0);
    write_room_descriptor(bytes, RESOLVED_ROOM_OFFSET, 0xFFFF, 0, 0x1234,
                          (uint16_t)(5 | (6 << 7) | 0xC000));
    write_u32(bytes, RESOLVED_ROOM_OFFSET + 8, GBA_ROM_BASE + BACKGROUND_OFFSET);
    write_u32(bytes, RESOLVED_ROOM_OFFSET + 12, GBA_ROM_BASE + GRAPHICS_LOAD_OFFSET);
    write_u32(bytes, RESOLVED_ROOM_OFFSET + 16, GBA_ROM_BASE + PALETTE_LOAD_OFFSET);
    write_u32(bytes, RESOLVED_ROOM_OFFSET + 20, GBA_ROM_BASE + ENTITY_OFFSET);
    write_u32(bytes, RESOLVED_ROOM_OFFSET + 24,
              GBA_ROM_BASE + RESOLVED_ROOM_OFFSET - 0x20);
    write_transition(bytes, RESOLVED_ROOM_OFFSET - 0x20,
                     GBA_ROM_BASE + ROOM_BASE_OFFSET + ROOM_STRIDE, -1, 2, 10);
    write_transition(bytes, RESOLVED_ROOM_OFFSET - 0x10,
                     GBA_ROM_BASE + ROOM_BASE_OFFSET + 2 * ROOM_STRIDE, 3, -4, 20);
    FusionAosRoomDescriptor descriptor;
    assert(fusion_aos_read_room_descriptor(&rom, &directory, 0, 0, &descriptor,
                                           error, sizeof error));
    assert(descriptor.variant_count == 2);
    assert(descriptor.source_pointer == GBA_ROM_BASE + ROOM_BASE_OFFSET);
    assert(descriptor.resolved_pointer == GBA_ROM_BASE + RESOLVED_ROOM_OFFSET);
    assert(descriptor.display_control == 0x1234 && descriptor.blend_control == 0x1040);
    assert(descriptor.map_x == 5 && descriptor.map_y == 6);
    assert(descriptor.coordinate_flag_0 && descriptor.coordinate_flag_1);
    assert(descriptor.transition_count == 2);
    FusionAosTransitionList transitions;
    assert(fusion_aos_read_room_transitions(&rom, &directory, &descriptor,
                                            &transitions, error, sizeof error));
    assert(transitions.count == 2);
    assert(transitions.entries[0].source_screen_x == -1);
    assert(transitions.entries[0].source_screen_y == 2);
    assert(transitions.entries[0].field_6 == 10 && transitions.entries[0].field_e == 14);
    assert(transitions.entries[0].target_area == 1 &&
           transitions.entries[0].target_room == 0);
    assert(transitions.entries[1].source_screen_y == -4);
    assert(transitions.entries[1].target_area == 1 &&
           transitions.entries[1].target_room == 1);
    write_u32(bytes, RESOLVED_ROOM_OFFSET - 0x20, GBA_ROM_BASE + 0x7000);
    assert(!fusion_aos_read_room_transitions(&rom, &directory, &descriptor,
                                             &transitions, error, sizeof error));
    write_u32(bytes, RESOLVED_ROOM_OFFSET - 0x20,
              GBA_ROM_BASE + ROOM_BASE_OFFSET + ROOM_STRIDE);
    write_background(bytes, BACKGROUND_OFFSET, 1, 2, 0x100,
                     GBA_ROM_BASE + BACKGROUND_METADATA_OFFSET);
    write_background(bytes, BACKGROUND_OFFSET + 0x0C, 3, 4, 0x200, 0);
    write_background(bytes, BACKGROUND_OFFSET + 0x18, 5, 6, 0x300,
                     GBA_ROM_BASE + BACKGROUND_METADATA_OFFSET + 2);
    bytes[BACKGROUND_METADATA_OFFSET] = 7;
    bytes[BACKGROUND_METADATA_OFFSET + 1] = 8;
    bytes[BACKGROUND_METADATA_OFFSET + 2] = 9;
    bytes[BACKGROUND_METADATA_OFFSET + 3] = 10;
    FusionAosBackgroundList backgrounds;
    assert(fusion_aos_read_room_backgrounds(&rom, &descriptor, &backgrounds,
                                            error, sizeof error));
    assert(backgrounds.entries[0].layer == 1);
    assert(backgrounds.entries[0].control == 0x100);
    assert(backgrounds.entries[0].width_screens == 7 &&
           backgrounds.entries[0].height_screens == 8);
    assert(!backgrounds.entries[1].has_dimensions);
    assert(backgrounds.entries[2].field_6 == 0x302);
    assert(backgrounds.entries[2].width_screens == 9 &&
           backgrounds.entries[2].height_screens == 10);
    write_load_entry(bytes, GRAPHICS_LOAD_OFFSET, GBA_ROM_BASE + 0x8000, 1, 2, 3);
    write_load_entry(bytes, GRAPHICS_LOAD_OFFSET + 8, GBA_ROM_BASE + 0x8100, 4, 5, 6);
    write_load_entry(bytes, GRAPHICS_LOAD_OFFSET + 16, 0, 0, 0, 0);
    write_load_entry(bytes, PALETTE_LOAD_OFFSET, GBA_ROM_BASE + 0x8200, 7, 8, 9);
    write_load_entry(bytes, PALETTE_LOAD_OFFSET + 8, 0, 0, 0, 0);
    FusionAosLoadList loads;
    assert(fusion_aos_read_load_list(&rom, descriptor.graphics_pointer, &loads,
                                     error, sizeof error));
    assert(loads.count == 2);
    assert(loads.entries[0].parameters[0] == 1 &&
           loads.entries[1].parameters[2] == 6);
    assert(fusion_aos_read_load_list(&rom, descriptor.palette_pointer, &loads,
                                     error, sizeof error));
    assert(loads.count == 1 && loads.entries[0].parameters[1] == 8);
    for (size_t index = 0; index < FUSION_AOS_LOAD_ENTRY_CAPACITY; ++index) {
        write_load_entry(bytes, UNTERMINATED_LOAD_OFFSET + index * 8,
                         GBA_ROM_BASE + 0x8000, 0, 0, 0);
    }
    assert(!fusion_aos_read_load_list(&rom,
                                      GBA_ROM_BASE + UNTERMINATED_LOAD_OFFSET,
                                      &loads, error, sizeof error));
    write_entity(bytes, ENTITY_OFFSET, -12, 34, 5, 1, 0x21, 7, 0x1234, 0x5678);
    write_entity(bytes, ENTITY_OFFSET + 12, 56, -78, 6, 2, 0x22, 8, 9, 10);
    write_entity(bytes, ENTITY_OFFSET + 24, INT16_MAX, 0, 0, 0, 0, 0, 0, 0);
    FusionAosEntityList entities;
    assert(fusion_aos_read_entity_list(&rom, descriptor.entities_pointer, &entities,
                                       error, sizeof error));
    assert(entities.count == 2);
    assert(entities.entries[0].x == -12 && entities.entries[0].y == 34);
    assert(entities.entries[0].persistent_index == 5 &&
           entities.entries[0].entity_id == 0x21);
    assert(entities.entries[0].parameters[0] == 0x1234 &&
           entities.entries[0].parameters[1] == 0x5678);
    assert(entities.entries[1].y == -78 && entities.entries[1].kind == 2);
    assert(fusion_aos_read_entity_list(&rom, 0, &entities, error, sizeof error));
    assert(entities.count == 0);
    for (size_t index = 0; index < FUSION_AOS_ENTITY_CAPACITY; ++index) {
        write_entity(bytes, UNTERMINATED_ENTITY_OFFSET + index * 12,
                     1, 2, 0, 1, 3, 0, 0, 0);
    }
    assert(!fusion_aos_read_entity_list(&rom,
                                        GBA_ROM_BASE + UNTERMINATED_ENTITY_OFFSET,
                                        &entities, error, sizeof error));
    write_u32(bytes, ROOM_BASE_OFFSET + 4, GBA_ROM_BASE + ROOM_BASE_OFFSET);
    assert(!fusion_aos_read_room_descriptor(&rom, &directory, 0, 0, &descriptor,
                                            error, sizeof error));
    write_u32(bytes, ROOM_BASE_OFFSET + 4, GBA_ROM_BASE + RESOLVED_ROOM_OFFSET);

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
