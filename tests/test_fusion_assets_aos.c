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
    BOSS_ENTITY_OFFSET = 0xB000,
    COMMON_BACKGROUND_OFFSET = 0xC000,
    COMMON_METADATA_OFFSET = 0xC030,
    EMPTY_LOAD_OFFSET = 0xC040,
    COLLISION_METADATA_OFFSET = 0xC100,
    COLLISION_BLOCKS_OFFSET = 0xC200,
    COLLISION_VALUES_OFFSET = 0xC300,
    COLLISION_MAP_OFFSET = 0xC400,
    GRAPHICS_RESOURCE_OFFSET = 0xD000,
    COMPRESSED_GRAPHICS_RESOURCE_OFFSET = 0xD900,
    PALETTE_RESOURCE_OFFSET = 0xDA00,
    GRAPHICS_LZ77_OFFSET = 0xDB00,
    ENEMY_TABLE_OFFSET = 0x0E9644,
    ENEMY_ENTRY_SIZE = 0x24,
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

static size_t write_lz77_literals(uint8_t *data, size_t offset,
                                  const uint8_t *expanded, size_t size) {
    data[offset] = 0x10;
    data[offset + 1] = (uint8_t)size;
    data[offset + 2] = (uint8_t)(size >> 8);
    data[offset + 3] = (uint8_t)(size >> 16);
    size_t input = 0, output = offset + 4;
    while (input < size) {
        data[output++] = 0;
        size_t count = size - input < 8 ? size - input : 8;
        memcpy(data + output, expanded + input, count);
        input += count;
        output += count;
    }
    return output - offset;
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
    static const size_t room_counts[FUSION_AOS_AREA_COUNT] = {
        11, 7, 16, 20, 12, 7, 8, 38, 2, 5, 1, 23
    };
    uint32_t room_pointers[FUSION_AOS_AREA_COUNT][64] = {{0}};
    size_t total_entries = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        total_entries += room_counts[area];
    }
    size_t table_offset = DIRECTORY_OFFSET - total_entries * 4;
    size_t cursor = table_offset;
    size_t descriptor_index = 0;
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        write_u32(bytes, DIRECTORY_OFFSET + area * 4,
                  GBA_ROM_BASE + (uint32_t)cursor);
        for (size_t room = 0; room < room_counts[area]; ++room) {
            size_t descriptor_offset = ROOM_BASE_OFFSET + descriptor_index * ROOM_STRIDE;
            room_pointers[area][room] = GBA_ROM_BASE + (uint32_t)descriptor_offset;
            write_u32(bytes, cursor + room * 4, room_pointers[area][room]);
            write_room_descriptor(bytes, descriptor_offset, 0xFFFF, 0, 0x1F00,
                                  (uint16_t)(area | (room << 7)));
            write_u32(bytes, descriptor_offset + 8,
                      GBA_ROM_BASE + COMMON_BACKGROUND_OFFSET);
            write_u32(bytes, descriptor_offset + 12,
                      GBA_ROM_BASE + EMPTY_LOAD_OFFSET);
            write_u32(bytes, descriptor_offset + 16,
                      GBA_ROM_BASE + EMPTY_LOAD_OFFSET);
            ++descriptor_index;
        }
        cursor += room_counts[area] * 4;
    }
    assert(cursor == DIRECTORY_OFFSET);
    for (size_t layer = 0; layer < FUSION_AOS_BACKGROUND_COUNT; ++layer) {
        write_background(bytes, COMMON_BACKGROUND_OFFSET + layer * 12,
                         (uint8_t)layer, (uint8_t)(layer + 1),
                         (uint16_t)(0x400 + layer),
                         GBA_ROM_BASE + COMMON_METADATA_OFFSET + layer * 2);
        bytes[COMMON_METADATA_OFFSET + layer * 2] = 1;
        bytes[COMMON_METADATA_OFFSET + layer * 2 + 1] = 1;
    }

    for (size_t index = 0; index < FUSION_AOS_BOSS_ROOM_COUNT; ++index) {
        const FusionAosBossSpec *spec = fusion_aos_boss_spec(index);
        assert(spec);
        size_t descriptor_offset = room_pointers[spec->area][spec->room] - GBA_ROM_BASE;
        size_t entity_offset = BOSS_ENTITY_OFFSET + index * 24;
        write_u32(bytes, descriptor_offset + 20,
                  GBA_ROM_BASE + (uint32_t)entity_offset);
        write_entity(bytes, entity_offset, (int16_t)index, (int16_t)(index + 1),
                     0, 1, spec->enemy_id, 0, 0, 0);
        write_entity(bytes, entity_offset + 12, INT16_MAX, 0, 0, 0, 0, 0, 0, 0);
    }

    FusionRom rom = {bytes, FIXTURE_SIZE, FUSION_ASSET_GAME_ARIA};
    FusionAosAreaDirectory directory;
    char error[160];
    assert(fusion_aos_read_area_directory(&rom, &directory, error, sizeof error));
    assert(directory.total_rooms == total_entries);
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        assert(directory.areas[area].room_count == room_counts[area]);
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
                     room_pointers[1][0], -1, 2, 10);
    write_transition(bytes, RESOLVED_ROOM_OFFSET - 0x10,
                     room_pointers[1][1], 3, -4, 20);
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
              room_pointers[1][0]);
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
    write_background(bytes, BACKGROUND_OFFSET, 1, 2, 0x100,
                     GBA_ROM_BASE + COLLISION_METADATA_OFFSET);
    bytes[COLLISION_METADATA_OFFSET] = 1;
    bytes[COLLISION_METADATA_OFFSET + 1] = 1;
    write_u16(bytes, COLLISION_METADATA_OFFSET + 2, 0);
    write_u32(bytes, COLLISION_METADATA_OFFSET + 4,
              GBA_ROM_BASE + COLLISION_BLOCKS_OFFSET);
    write_u32(bytes, COLLISION_METADATA_OFFSET + 8,
              GBA_ROM_BASE + COLLISION_VALUES_OFFSET);
    write_u32(bytes, COLLISION_METADATA_OFFSET + 12,
              GBA_ROM_BASE + COLLISION_MAP_OFFSET);
    for (size_t index = 0; index < 16; ++index)
        write_u16(bytes, COLLISION_BLOCKS_OFFSET + index * 2,
                  (uint16_t)(0x1000 + index));
    for (size_t index = 0; index < 16; ++index)
        bytes[COLLISION_VALUES_OFFSET + index] = (uint8_t)index;
    bytes[COLLISION_VALUES_OFFSET + 15] = 0x41;
    write_u16(bytes, COLLISION_MAP_OFFSET, 0xC001);
    assert(fusion_aos_read_room_backgrounds(&rom, &descriptor, &backgrounds,
                                            error, sizeof error));
    FusionAosBackgroundTilemap tilemap;
    assert(fusion_aos_read_background_tiles(&rom, &backgrounds.entries[0],
                                            &tilemap, error, sizeof error));
    assert(tilemap.status == FUSION_AOS_TILEMAP_DECODED);
    assert(tilemap.width_screens == 1 && tilemap.height_screens == 1);
    assert(tilemap.maximum_block == 1 && tilemap.cell_count == 1024);
    assert(tilemap.width_tiles == 32 && tilemap.height_tiles == 32);
    assert(tilemap.cells[0] == 0x1C0F && tilemap.cells[1] == 0x1C0E &&
           tilemap.cells[2] == 0x1C0D && tilemap.cells[3] == 0x1C0C);
    FusionAosVideoMemory render_memory = {0};
    render_memory.vram[15 * 32 + 31] = 0x20;
    write_u16(render_memory.palette, 18 * 2, 0x03E0);
    FusionAosRenderedBackground rendered;
    assert(fusion_aos_render_background(&backgrounds.entries[0], &tilemap,
                                        &render_memory, &rendered,
                                        error, sizeof error));
    assert(rendered.width_pixels == 256 && rendered.height_pixels == 256 &&
           rendered.pixel_count == 65536 && rendered.unresolved_count == 0);
    assert(rendered.rgba[0] == 0 && rendered.rgba[1] == 255 &&
           rendered.rgba[2] == 0 && rendered.rgba[3] == 255);
    fusion_aos_free_rendered_background(&rendered);
    assert(rendered.rgba == NULL && rendered.pixel_count == 0);
    /* 256 colours (BGCNT bit 7): 64-byte tiles, one byte per pixel into
     * the whole palette; the entry's palette bank is ignored. Cell 0 is tile
     * 15 flipped both ways, so its first pixel is byte 63 of the tile. */
    FusionAosBackground eight_bpp = backgrounds.entries[0];
    eight_bpp.control |= 0x80;
    size_t eight_base = (size_t)((eight_bpp.control >> 2) & 3) * 0x4000;
    render_memory.vram[eight_base + 15 * 64 + 63] = 49;
    write_u16(render_memory.palette, 49 * 2, 0x7C00);
    assert(fusion_aos_render_background(&eight_bpp, &tilemap,
                                        &render_memory, &rendered,
                                        error, sizeof error));
    assert(rendered.rgba[0] == 0 && rendered.rgba[1] == 0 && rendered.rgba[2] == 255 &&
           rendered.rgba[3] == 255);
    fusion_aos_free_rendered_background(&rendered);
    uint8_t front_rgba[] = {255, 0, 0, 255, 0, 0, 0, 0};
    uint8_t rear_rgba[] = {0, 0, 255, 255};
    FusionAosBackground front = {.layer = 1, .field_0 = 22};
    FusionAosBackground rear = {.layer = 2, .field_0 = 31};
    FusionAosRenderedBackground front_rendered = {
        2, 1, front_rgba, 2, 0
    };
    FusionAosRenderedBackground rear_rendered = {
        1, 1, rear_rgba, 1, 0
    };
    FusionAosCompositeLayer layers[] = {
        {&front, &front_rendered}, {&rear, &rear_rendered}
    };
    FusionAosCompositeBackground composite;
    assert(fusion_aos_composite_backgrounds(
        layers, 2, &composite, error, sizeof error));
    assert(composite.width_pixels == 2 && composite.height_pixels == 1 &&
           composite.pixel_count == 2);
    assert(composite.rgb[0] == 255 && composite.rgb[1] == 0 &&
           composite.rgb[2] == 0 && composite.rgb[3] == 0 &&
           composite.rgb[4] == 0 && composite.rgb[5] == 255);
    FusionAosBitmap24 bitmap;
    assert(fusion_aos_serialize_composite_bmp24(
        &composite, &bitmap, error, sizeof error));
    assert(bitmap.size == 62 && bitmap.data[0] == 'B' &&
           bitmap.data[1] == 'M' && bitmap.data[2] == 62 &&
           bitmap.data[10] == 54 && bitmap.data[14] == 40 &&
           bitmap.data[18] == 2 && bitmap.data[22] == 1 &&
           bitmap.data[26] == 1 && bitmap.data[28] == 24);
    assert(bitmap.data[54] == 0 && bitmap.data[55] == 0 &&
           bitmap.data[56] == 255 && bitmap.data[57] == 255 &&
           bitmap.data[58] == 0 && bitmap.data[59] == 0 &&
           bitmap.data[60] == 0 && bitmap.data[61] == 0);
    fusion_aos_free_bitmap24(&bitmap);
    assert(bitmap.data == NULL && bitmap.size == 0);
    size_t saved_pixel_count = composite.pixel_count;
    composite.pixel_count = 1;
    assert(!fusion_aos_serialize_composite_bmp24(
        &composite, &bitmap, error, sizeof error));
    composite.pixel_count = saved_pixel_count;
    fusion_aos_free_composite_background(&composite);
    assert(composite.rgb == NULL && composite.pixel_count == 0);
    assert(fusion_aos_composite_backgrounds(
        &layers[1], 1, &composite, error, sizeof error));
    assert(composite.width_pixels == 1 && composite.height_pixels == 1 &&
           composite.rgb[2] == 255);
    fusion_aos_free_composite_background(&composite);
    assert(!fusion_aos_composite_backgrounds(
        NULL, 0, &composite, error, sizeof error));
    fusion_aos_free_background_tilemap(&tilemap);
    assert(tilemap.cells == NULL && tilemap.cell_count == 0);
    assert(fusion_aos_read_background_tiles(&rom, &backgrounds.entries[1],
                                            &tilemap, error, sizeof error));
    assert(tilemap.status == FUSION_AOS_TILEMAP_ABSENT);
    FusionAosCollisionMap collision;
    assert(fusion_aos_read_background_collision(&rom, &backgrounds.entries[0],
                                                &collision, error, sizeof error));
    assert(collision.status == FUSION_AOS_COLLISION_DECODED);
    assert(collision.width_screens == 1 && collision.height_screens == 1);
    assert(collision.maximum_block == 1 && collision.cell_count == 1024);
    assert(collision.width_tiles == 32 && collision.height_tiles == 32);
    assert(collision.cells[0] == 0x45 && collision.cells[1] == 14 &&
           collision.cells[2] == 13 && collision.cells[3] == 12);
    fusion_aos_free_collision_map(&collision);
    assert(collision.cells == NULL && collision.cell_count == 0);
    assert(fusion_aos_read_background_collision(&rom, &backgrounds.entries[1],
                                                &collision, error, sizeof error));
    assert(collision.status == FUSION_AOS_COLLISION_ABSENT);
    write_u32(bytes, COLLISION_METADATA_OFFSET + 8, 0);
    assert(fusion_aos_read_background_collision(&rom, &backgrounds.entries[0],
                                                &collision, error, sizeof error));
    assert(collision.status == FUSION_AOS_COLLISION_NONE);
    write_u16(bytes, COLLISION_METADATA_OFFSET + 2, 1);
    assert(fusion_aos_read_background_collision(&rom, &backgrounds.entries[0],
                                                &collision, error, sizeof error));
    assert(collision.status == FUSION_AOS_COLLISION_UNSUPPORTED_AFFINE);
    assert(fusion_aos_read_background_tiles(&rom, &backgrounds.entries[0],
                                            &tilemap, error, sizeof error));
    assert(tilemap.status == FUSION_AOS_TILEMAP_UNSUPPORTED_AFFINE);
    write_u16(bytes, COLLISION_METADATA_OFFSET + 2, 0);
    bytes[COLLISION_METADATA_OFFSET] = 17;
    assert(!fusion_aos_read_background_collision(&rom, &backgrounds.entries[0],
                                                 &collision, error, sizeof error));
    assert(!fusion_aos_read_background_tiles(&rom, &backgrounds.entries[0],
                                             &tilemap, error, sizeof error));
    bytes[COLLISION_METADATA_OFFSET] = 1;
    write_u32(bytes, COLLISION_METADATA_OFFSET + 4,
              GBA_ROM_BASE + FIXTURE_SIZE - 1);
    assert(!fusion_aos_read_background_collision(&rom, &backgrounds.entries[0],
                                                 &collision, error, sizeof error));
    assert(!fusion_aos_read_background_tiles(&rom, &backgrounds.entries[0],
                                             &tilemap, error, sizeof error));
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

    write_load_entry(bytes, GRAPHICS_LOAD_OFFSET,
                     GBA_ROM_BASE + GRAPHICS_RESOURCE_OFFSET, 1, 0, 1);
    write_load_entry(bytes, GRAPHICS_LOAD_OFFSET + 8,
                     GBA_ROM_BASE + COMPRESSED_GRAPHICS_RESOURCE_OFFSET, 2, 0, 1);
    write_load_entry(bytes, GRAPHICS_LOAD_OFFSET + 16, 0, 0, 0, 0);
    bytes[GRAPHICS_RESOURCE_OFFSET] = 0;
    bytes[GRAPHICS_RESOURCE_OFFSET + 1] = 4;
    bytes[GRAPHICS_RESOURCE_OFFSET + 2] = 1;
    for (size_t index = 0; index < 0x800; ++index)
        bytes[GRAPHICS_RESOURCE_OFFSET + 4 + index] = (uint8_t)(index ^ 0x5A);
    bytes[COMPRESSED_GRAPHICS_RESOURCE_OFFSET] = 1;
    bytes[COMPRESSED_GRAPHICS_RESOURCE_OFFSET + 1] = 4;
    bytes[COMPRESSED_GRAPHICS_RESOURCE_OFFSET + 2] = 1;
    write_u32(bytes, COMPRESSED_GRAPHICS_RESOURCE_OFFSET + 4,
              GBA_ROM_BASE + GRAPHICS_LZ77_OFFSET);
    uint8_t expanded[0x804] = {0};
    for (size_t index = 0; index < 0x800; ++index)
        expanded[4 + index] = (uint8_t)(index ^ 0xA5);
    assert(write_lz77_literals(bytes, GRAPHICS_LZ77_OFFSET,
                               expanded, sizeof expanded) < 0xA00);

    write_load_entry(bytes, PALETTE_LOAD_OFFSET,
                     GBA_ROM_BASE + PALETTE_RESOURCE_OFFSET, 2, 1, 1);
    write_load_entry(bytes, PALETTE_LOAD_OFFSET + 8, 0, 0, 0, 0);
    bytes[PALETTE_RESOURCE_OFFSET] = 0;
    bytes[PALETTE_RESOURCE_OFFSET + 1] = 4;
    bytes[PALETTE_RESOURCE_OFFSET + 2] = 1;
    for (size_t index = 0; index < 0x40; ++index)
        bytes[PALETTE_RESOURCE_OFFSET + 4 + index] = (uint8_t)(index + 7);

    FusionAosVideoMemory memory;
    assert(fusion_aos_load_video_memory(&rom, &descriptor, &memory,
                                        error, sizeof error));
    assert(memory.graphics_load_count == 2 && memory.palette_load_count == 1);
    assert(memory.vram[0] == 0 && memory.vram[0x800] == 0x5A &&
           memory.vram[0xFFF] == (uint8_t)(0x7FF ^ 0x5A));
    assert(memory.vram[0x1000] == 0xA5 &&
           memory.vram[0x17FF] == (uint8_t)(0x7FF ^ 0xA5));
    assert(memory.palette[0x3F] == 0 && memory.palette[0x40] == 0x27 &&
           memory.palette[0x5F] == 0x46 && memory.palette[0x60] == 0);

    bytes[GRAPHICS_LOAD_OFFSET + 4] = 32;
    assert(!fusion_aos_load_video_memory(&rom, &descriptor, &memory,
                                         error, sizeof error));
    bytes[GRAPHICS_LOAD_OFFSET + 4] = 1;
    bytes[GRAPHICS_RESOURCE_OFFSET] = 2;
    assert(!fusion_aos_load_video_memory(&rom, &descriptor, &memory,
                                         error, sizeof error));
    bytes[GRAPHICS_RESOURCE_OFFSET] = 0;
    bytes[PALETTE_LOAD_OFFSET + 4] = 16;
    assert(!fusion_aos_load_video_memory(&rom, &descriptor, &memory,
                                         error, sizeof error));
    bytes[PALETTE_LOAD_OFFSET + 4] = 2;
    bytes[PALETTE_RESOURCE_OFFSET] = 1;
    assert(!fusion_aos_load_video_memory(&rom, &descriptor, &memory,
                                         error, sizeof error));
    bytes[PALETTE_RESOURCE_OFFSET] = 0;
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
    uint8_t room_cells[32 * 32] = {0x45, 0x0E};
    FusionAosCollisionMap room_collision = {
        .status = FUSION_AOS_COLLISION_DECODED,
        .width_screens = 1,
        .height_screens = 1,
        .width_tiles = 32,
        .height_tiles = 32,
        .cells = room_cells,
        .cell_count = 32 * 32,
    };
    FusionAosTransitionList room_transitions = {.count = 1};
    room_transitions.entries[0] = (FusionAosTransition){
        .source_screen_x = -1,
        .source_screen_y = 2,
        .field_6 = 0xFFF0,
        .load_x = 1040,
        .load_y = 512,
        .target_area = 1,
        .target_room = 6,
    };
    FusionAosEntityList room_entities = {.count = 1};
    room_entities.entries[0] = entities.entries[0];
    FusionAosRuntimeRoomPayload room_payload;
    assert(fusion_aos_serialize_runtime_room(
        3, 7, &room_collision, &room_transitions, &room_entities,
        FUSION_AOS_ROOM_FLAG_SAVE | FUSION_AOS_ROOM_FLAG_WARP,
        &room_payload, error, sizeof error));
    const char *room_text = (const char *)room_payload.data;
    assert(!strncmp(room_text,
                    "AOSROOM-NATIVE\t4\t3\t7\t1\t1\t32\t32\t3\nR\t450e",
                    strlen("AOSROOM-NATIVE\t4\t3\t7\t1\t1\t32\t32\t3\nR\t450e")));
    assert(strstr(room_text, "\nT\t-1\t2\t-16\t1040\t512\t1\t6\n"));
    assert(strstr(room_text, "\nE\t1\t33\t-12\t34\t4660\t22136\t7\nEND\n"));
    assert(room_payload.data[room_payload.size] == '\0');
    fusion_aos_free_runtime_room_payload(&room_payload);
    assert(room_payload.data == NULL && room_payload.size == 0);
    assert(!fusion_aos_serialize_runtime_room(
        3, 7, &room_collision, &room_transitions, &room_entities, 4,
        &room_payload, error, sizeof error));
    assert(fusion_aos_boss_spec(0) != NULL);
    assert(fusion_aos_boss_spec(FUSION_AOS_BOSS_ROOM_COUNT) == NULL);
    const uint16_t boss_health = 0x3456;
    write_u16(bytes, ENEMY_TABLE_OFFSET + 0x21 * ENEMY_ENTRY_SIZE + 0x0C,
              boss_health);
    const FusionAosBossSpec test_boss = {
        "aria.test", "Synthetic Boss", "fight", 0, 0, 0x21
    };
    FusionAosBossRoom boss;
    assert(fusion_aos_read_boss_room(&rom, &directory, &test_boss, &boss,
                                     error, sizeof error));
    assert(boss.spec == &test_boss);
    assert(boss.map_x == descriptor.map_x && boss.map_y == descriptor.map_y);
    assert(boss.room_pointer == descriptor.source_pointer);
    assert(boss.entity_pointer == entities.entries[0].entry_pointer);
    assert(boss.enemy_table_pointer ==
           GBA_ROM_BASE + ENEMY_TABLE_OFFSET + 0x21 * ENEMY_ENTRY_SIZE);
    assert(boss.health == boss_health);
    write_entity(bytes, ENTITY_OFFSET + 12, 56, -78, 6, 1, 0x21, 8, 9, 10);
    assert(!fusion_aos_read_boss_room(&rom, &directory, &test_boss, &boss,
                                      error, sizeof error));
    write_entity(bytes, ENTITY_OFFSET + 12, 56, -78, 6, 2, 0x22, 8, 9, 10);
    const FusionAosBossSpec missing_boss = {
        "aria.missing", "Missing Boss", "fight", 0, 0, 0x22
    };
    assert(!fusion_aos_read_boss_room(&rom, &directory, &missing_boss, &boss,
                                      error, sizeof error));
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

    FusionAosWorldSummary summary;
    assert(fusion_aos_read_world_summary(&rom, &summary, error, sizeof error));
    assert(summary.native_room_count == total_entries);
    assert(summary.mapped_cells == 2);
    assert(summary.transition_count == 2);
    assert(summary.entity_count == 2 + FUSION_AOS_BOSS_ROOM_COUNT);
    assert(summary.boss_room_count == FUSION_AOS_BOSS_ROOM_COUNT);
    assert(summary.savepoint_count == 1 && summary.warp_count == 1);
    const char *catalog_path = "fusion-aos-world-test.json";
    const char *index_path = "fusion-aos-rooms-test.tsv";
    remove(catalog_path);
    remove(index_path);
    assert(fusion_aos_write_world_catalog(&rom, catalog_path, error, sizeof error));
    assert(fusion_aos_write_room_index(&rom, index_path, error, sizeof error));
    FILE *catalog = fopen(catalog_path, "rb");
    assert(catalog);
    char catalog_header[40] = {0};
    assert(fread(catalog_header, 1, sizeof catalog_header - 1, catalog) ==
           sizeof catalog_header - 1);
    assert(fclose(catalog) == 0);
    assert(strstr(catalog_header, "\"format\":\"MV_AOS_WORLD_2\"") != NULL);
    FILE *index_file = fopen(index_path, "rb");
    assert(index_file);
    char index_header[64] = {0};
    assert(fread(index_header, 1, sizeof index_header - 1, index_file) ==
           sizeof index_header - 1);
    assert(fclose(index_file) == 0);
    assert(strstr(index_header, "# VERIFIED ARIA STRUCTURAL INDEX") != NULL);
    assert(remove(catalog_path) == 0);
    assert(remove(index_path) == 0);

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
