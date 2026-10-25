/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/mzm.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ELF_SIZE = 0x240,
    SECTION_OFFSET = 0x40,
    STRING_OFFSET = 0x100,
    SYMBOL_OFFSET = 0x160,
    ROM_SIZE = 0x100
};

static void write_u16(uint8_t *data, size_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
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

static void write_symbol(uint8_t *elf, size_t index, uint32_t name,
                         uint32_t address, uint32_t size, uint8_t kind) {
    size_t offset = SYMBOL_OFFSET + index * 16;
    write_u32(elf, offset, name);
    write_u32(elf, offset + 4, address);
    write_u32(elf, offset + 8, size);
    elf[offset + 12] = kind;
}

static void make_elf(uint8_t elf[ELF_SIZE]) {
    memset(elf, 0, ELF_SIZE);
    memcpy(elf, "\x7f" "ELF", 4);
    elf[4] = 1;
    elf[5] = 1;
    write_u32(elf, 0x20, SECTION_OFFSET);
    write_u16(elf, 0x2E, 40);
    write_u16(elf, 0x30, 3);

    const char names[] = "\0first\0dup\0outside\0zero\0func\0";
    memcpy(elf + STRING_OFFSET, names, sizeof names);
    size_t strings = SECTION_OFFSET + 40;
    write_u32(elf, strings + 4, 3);
    write_u32(elf, strings + 16, STRING_OFFSET);
    write_u32(elf, strings + 20, sizeof names);

    size_t symbols = SECTION_OFFSET + 80;
    write_u32(elf, symbols + 4, 2);
    write_u32(elf, symbols + 16, SYMBOL_OFFSET);
    write_u32(elf, symbols + 20, 6 * 16);
    write_u32(elf, symbols + 24, 1);
    write_u32(elf, symbols + 36, 16);
    write_symbol(elf, 1, 7, 0x08000030, 4, 1);
    write_symbol(elf, 2, 1, 0x08000010, 8, 1);
    write_symbol(elf, 3, 7, 0x08000020, 4, 1);
    write_symbol(elf, 4, 11, 0x09000000, 4, 1);
    write_symbol(elf, 5, 24, 0x08000040, 4, 2);
}

static size_t read_file(const char *path, uint8_t *data, size_t capacity) {
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t size = fread(data, 1, capacity, file);
    assert(ferror(file) == 0);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
    return size;
}

static void test_room_entries(FusionRom *rom) {
    static const char *names[FUSION_MZM_ROOM_AREA_COUNT] = {
        "sBrinstarRoomEntries", "sKraidRoomEntries",
        "sNorfairRoomEntries", "sRidleyRoomEntries",
        "sTourianRoomEntries", "sCrateriaRoomEntries",
        "sChozodiaRoomEntries", "sTest1RoomEntries",
        "sTest2RoomEntries", "sTest3RoomEntries", "sTestRoomEntries"
    };
    FusionMzmRomSymbol entries[FUSION_MZM_ROOM_AREA_COUNT];
    for (size_t index = 0; index < FUSION_MZM_ROOM_AREA_COUNT; ++index) {
        entries[index] = (FusionMzmRomSymbol){
            names[index], 0x08000050, FUSION_MZM_ROOM_ENTRY_SIZE, index
        };
    }
    FusionMzmRomSymbolTable symbols = {
        entries, FUSION_MZM_ROOM_AREA_COUNT, 0
    };
    uint8_t *data = (uint8_t *)rom->data + 0x50;
    memset(data, 0, FUSION_MZM_ROOM_ENTRY_SIZE);
    data[0] = 8;
    data[1] = 1;
    data[2] = 2;
    data[3] = 3;
    data[4] = 4;
    for (size_t offset = 8; offset <= 24; offset += 4)
        write_u32(data, offset, 0x08000010 + (uint32_t)offset);
    data[28] = 5;
    data[29] = 6;
    write_u32(data, 32, 0x08000030);
    data[36] = 7;
    data[37] = 8;
    write_u32(data, 40, 0x08000034);
    data[44] = 9;
    data[45] = 10;
    write_u32(data, 48, 0x08000038);
    data[52] = 11;
    data[53] = 12;
    data[54] = 13;
    data[55] = 14;
    data[56] = 15;
    write_u16(data, 58, 0x1234);

    char error[160];
    FusionMzmRoomDirectory directory;
    assert(fusion_mzm_read_room_directory(rom, &symbols, &directory,
                                          error, sizeof error));
    assert(directory.total_rooms == FUSION_MZM_ROOM_AREA_COUNT);
    assert(directory.areas[0].room_count == 1);
    assert(directory.areas[10].table_pointer == 0x08000050);
    assert(!strcmp(fusion_mzm_room_area_spec(0)->id, "Brinstar"));
    assert(fusion_mzm_room_area_spec(FUSION_MZM_ROOM_AREA_COUNT) == NULL);
    FusionMzmRoomEntry room;
    assert(fusion_mzm_read_room_entry(rom, &directory, 0, 0, &room,
                                      error, sizeof error));
    assert(room.entry_pointer == 0x08000050);
    assert(room.tileset == 8 && room.bg0_property == 1 && room.bg3_property == 4);
    assert(room.bg0_pointer == 0x08000018 && room.clip_pointer == 0x08000024);
    assert(room.bg3_scrolling == 5 && room.transparency == 6);
    assert(room.default_sprite_pointer == 0x08000030);
    assert(room.default_spriteset == 7 && room.first_spriteset_event == 8);
    assert(room.first_sprite_pointer == 0x08000034);
    assert(room.first_spriteset == 9 && room.second_spriteset_event == 10);
    assert(room.second_sprite_pointer == 0x08000038);
    assert(room.second_spriteset == 11 && room.map_x == 12 && room.map_y == 13);
    assert(room.effect == 14 && room.effect_y == 15 && room.music_track == 0x1234);
    assert(!fusion_mzm_read_room_entry(rom, &directory, 0, 1, &room,
                                       error, sizeof error));

    entries[0].size = FUSION_MZM_ROOM_ENTRY_SIZE - 1;
    assert(!fusion_mzm_read_room_directory(rom, &symbols, &directory,
                                           error, sizeof error));
    entries[0].size = FUSION_MZM_ROOM_ENTRY_SIZE;
    write_u32(data, 20, 0x09000000);
    assert(fusion_mzm_read_room_directory(rom, &symbols, &directory,
                                          error, sizeof error));
    assert(!fusion_mzm_read_room_entry(rom, &directory, 0, 0, &room,
                                       error, sizeof error));
}

static void test_room_rle(FusionRom *rom) {
    static const uint8_t encoded[] = {
        2, 2,
        1, 0x82, 1, 0x82, 2, 0,
        1, 0x84, 0, 0
    };
    const size_t offset = 0xA0;
    memcpy(rom->data + offset, encoded, sizeof encoded);
    char error[160];
    FusionMzmRoomRleMap map;
    assert(fusion_mzm_read_room_rle(rom, 0x08000000 + offset, &map,
                                    error, sizeof error));
    assert(map.source_pointer == 0x08000000 + offset);
    assert(map.width == 2 && map.height == 2 && map.cell_count == 4);
    assert(map.encoded_size == sizeof encoded);
    assert(map.cells[0] == 1 && map.cells[1] == 1 &&
           map.cells[2] == 2 && map.cells[3] == 2);
    fusion_mzm_free_room_rle(&map);
    assert(map.cells == NULL && map.cell_count == 0);

    rom->data[offset] = 0;
    assert(!fusion_mzm_read_room_rle(rom, 0x08000000 + offset, &map,
                                     error, sizeof error));
    memcpy(rom->data + offset, encoded, sizeof encoded);
    rom->data[offset + 2] = 3;
    assert(!fusion_mzm_read_room_rle(rom, 0x08000000 + offset, &map,
                                     error, sizeof error));
    memcpy(rom->data + offset, encoded, sizeof encoded);
    rom->data[offset + 3] = 5;
    assert(!fusion_mzm_read_room_rle(rom, 0x08000000 + offset, &map,
                                     error, sizeof error));
    assert(!fusion_mzm_read_room_rle(rom, 0x080000FC, &map,
                                     error, sizeof error));
}

static void test_room_backgrounds(void) {
    enum {
        FIXTURE_SIZE = 0xC00,
        RLE_OFFSET = 0x100,
        TEXT_OFFSET = 0x180
    };
    uint8_t bytes[FIXTURE_SIZE] = {0};
    static const uint8_t encoded[] = {
        2, 2,
        1, 0x84, 3, 0,
        1, 0x84, 0, 0
    };
    memcpy(bytes + RLE_OFFSET, encoded, sizeof encoded);
    bytes[TEXT_OFFSET] = 0;
    uint8_t expanded[0x800];
    for (size_t index = 0; index < sizeof expanded; ++index)
        expanded[index] = (uint8_t)(index ^ 0x6D);
    assert(write_lz77_literals(bytes, TEXT_OFFSET + 4,
                               expanded, sizeof expanded) < 0xA00);
    FusionRom rom = {bytes, sizeof bytes, FUSION_ASSET_GAME_METROID};
    FusionMzmRoomEntry entry = {
        .bg0_property = FUSION_MZM_BG_PROPERTY_RLE_COMPRESSED,
        .bg1_property = 0,
        .bg2_property = FUSION_MZM_BG_PROPERTY_LZ77_COMPRESSED,
        .bg3_property = 0x46,
        .bg0_pointer = 0x08000000 + RLE_OFFSET,
        .bg1_pointer = 0x08000020,
        .bg2_pointer = 0x08000000 + TEXT_OFFSET,
        .bg3_pointer = 0x08000000 + TEXT_OFFSET,
    };
    char error[160];
    FusionMzmRoomBackgrounds backgrounds;
    assert(fusion_mzm_read_room_backgrounds(&rom, &entry, &backgrounds,
                                            error, sizeof error));
    assert(backgrounds.layers[0].status == FUSION_MZM_BACKGROUND_RLE);
    assert(backgrounds.layers[0].rle.width == 2 &&
           backgrounds.layers[0].rle.height == 2 &&
           backgrounds.layers[0].rle.cells[3] == 3);
    assert(backgrounds.layers[1].status == FUSION_MZM_BACKGROUND_NONE);
    assert(backgrounds.layers[2].status == FUSION_MZM_BACKGROUND_TEXT);
    assert(backgrounds.layers[2].text.size_flag == 0 &&
           backgrounds.layers[2].text.width_tiles == 32 &&
           backgrounds.layers[2].text.height_tiles == 32 &&
           backgrounds.layers[2].text.cell_count == 1024);
    assert(backgrounds.layers[2].text.cells[0] == 0x6C6D &&
           backgrounds.layers[2].text.cells[1023] == 0x9293);
    assert(backgrounds.layers[3].status == FUSION_MZM_BACKGROUND_TEXT);
    assert(backgrounds.layers[3].property == 0x46 &&
           backgrounds.layers[3].text.cell_count == 1024 &&
           backgrounds.layers[3].text.cells[0] == 0x6C6D);
    fusion_mzm_free_room_backgrounds(&backgrounds);
    assert(backgrounds.layers[0].rle.cells == NULL &&
           backgrounds.layers[2].text.cells == NULL &&
           backgrounds.layers[3].text.cells == NULL);

    entry.bg1_property = 1;
    assert(fusion_mzm_read_room_backgrounds(&rom, &entry, &backgrounds,
                                            error, sizeof error));
    assert(backgrounds.layers[1].status == FUSION_MZM_BACKGROUND_UNSUPPORTED);
    fusion_mzm_free_room_backgrounds(&backgrounds);
    bytes[TEXT_OFFSET + 5] = 1;
    assert(!fusion_mzm_read_text_tilemap(&rom, entry.bg2_pointer,
                                         &backgrounds.layers[2].text,
                                         error, sizeof error));
}

static void test_tileset_video_memory(void) {
    enum {
        FIXTURE_SIZE = 0x4000,
        TABLE_OFFSET = 0x100,
        GRAPHICS_OFFSET = 0x800,
        PALETTE_OFFSET = 0x1000,
        BACKGROUND_OFFSET = 0x1400,
        TILEMAP_OFFSET = 0x1800,
        COMMON_GFX_OFFSET = 0x2000,
        COMMON_TILEMAP_OFFSET = 0x2600,
        COMMON_PALETTE_OFFSET = 0x3000,
        DOOR_PALETTE_OFFSET = COMMON_PALETTE_OFFSET + 32,
        TILESET_INDEX = 3
    };
    uint8_t *bytes = calloc(FIXTURE_SIZE, 1);
    assert(bytes);
    uint8_t *entry = bytes + TABLE_OFFSET +
                     TILESET_INDEX * FUSION_MZM_TILESET_ENTRY_SIZE;
    write_u32(entry, 0, 0x08000000 + GRAPHICS_OFFSET);
    write_u32(entry, 4, 0x08000000 + PALETTE_OFFSET);
    write_u32(entry, 8, 0x08000000 + BACKGROUND_OFFSET);
    write_u32(entry, 12, 0x08000000 + TILEMAP_OFFSET);
    entry[16] = 5;
    entry[17] = 6;
    static const uint8_t graphics[] = {1, 2, 3, 4, 5, 6, 7, 8};
    static const uint8_t background[] = {9, 10, 11, 12};
    write_lz77_literals(bytes, GRAPHICS_OFFSET, graphics, sizeof graphics);
    write_lz77_literals(bytes, BACKGROUND_OFFSET,
                        background, sizeof background);
    for (size_t index = 0; index < 14 * 32; ++index)
        bytes[PALETTE_OFFSET + index] = (uint8_t)(index ^ 0xA5);
    for (size_t index = 0; index < 96; ++index)
        bytes[COMMON_PALETTE_OFFSET + index] = (uint8_t)(index ^ 0x5A);
    bytes[COMMON_GFX_OFFSET] = 0x11;
    bytes[COMMON_GFX_OFFSET + 1] = 0x22;
    bytes[COMMON_GFX_OFFSET + 2] = 0x33;
    bytes[COMMON_GFX_OFFSET + 3] = 0x44;
    write_u16(bytes, TILEMAP_OFFSET, 0x1234);
    for (size_t index = 0; index < 4; ++index)
        write_u16(bytes, TILEMAP_OFFSET + 2 + index * 2,
                  (uint16_t)(0x1001 + index));
    for (size_t index = 0;
         index < FUSION_MZM_COMMON_METATILE_COUNT *
                     FUSION_MZM_METATILE_CELLS; ++index) {
        write_u16(bytes, COMMON_TILEMAP_OFFSET + index * 2,
                  (uint16_t)(0x4000 + index));
    }

    FusionMzmRomSymbol symbol_entries[] = {
        {"sTilesetEntries", 0x08000000 + TABLE_OFFSET,
         FUSION_MZM_TILESET_COUNT * FUSION_MZM_TILESET_ENTRY_SIZE, 0},
        {"sCommonTilesGfx", 0x08000000 + COMMON_GFX_OFFSET, 4, 1},
        {"sCommonTilesPal", 0x08000000 + COMMON_PALETTE_OFFSET, 32, 2},
        {"sDoorTransitionPal", 0x08000000 + DOOR_PALETTE_OFFSET, 480, 3},
        {"sFixtureTilemap", 0x08000000 + TILEMAP_OFFSET, 10, 4},
        {"sCommonTilemap", 0x08000000 + COMMON_TILEMAP_OFFSET,
         FUSION_MZM_COMMON_METATILE_COUNT *
             FUSION_MZM_METATILE_CELLS * 2, 5},
    };
    FusionMzmRomSymbolTable symbols = {
        symbol_entries, sizeof symbol_entries / sizeof symbol_entries[0], 0
    };
    FusionRom rom = {bytes, FIXTURE_SIZE, FUSION_ASSET_GAME_METROID};
    char error[160];
    FusionMzmTilesetEntry decoded;
    assert(fusion_mzm_read_tileset_entry(&rom, &symbols, TILESET_INDEX,
                                         &decoded, error, sizeof error));
    assert(decoded.index == TILESET_INDEX);
    assert(decoded.graphics_pointer == 0x08000000 + GRAPHICS_OFFSET);
    assert(decoded.palette_pointer == 0x08000000 + PALETTE_OFFSET);
    assert(decoded.background_graphics_pointer ==
           0x08000000 + BACKGROUND_OFFSET);
    assert(decoded.tilemap_pointer == 0x08000000 + TILEMAP_OFFSET);
    assert(decoded.animated_tileset == 5 && decoded.animated_palette == 6);
    assert(!fusion_mzm_read_tileset_entry(
        &rom, &symbols, FUSION_MZM_TILESET_COUNT,
        &decoded, error, sizeof error));

    FusionMzmVideoMemory *memory = malloc(sizeof *memory);
    assert(memory);
    assert(fusion_mzm_load_tileset_video_memory(
        &rom, &symbols, TILESET_INDEX, memory, error, sizeof error));
    assert(memory->common_graphics_size == 4 &&
           memory->graphics_size == sizeof graphics &&
           memory->background_graphics_size == sizeof background);
    assert(!memcmp(memory->vram + 0x4800,
                   bytes + COMMON_GFX_OFFSET, 4));
    assert(!memcmp(memory->vram + 0x5800, graphics, sizeof graphics));
    assert(!memcmp(memory->vram + 0xFDE0 - sizeof background,
                   background, sizeof background));
    assert(memory->vram[0xFFE0] == 0 && memory->vram[0xFFFF] == 0);
    assert(memory->palette[0] == 0 && memory->palette[1] == 0);
    assert(memory->palette[2] == bytes[COMMON_PALETTE_OFFSET + 2]);
    assert(memory->palette[95] == bytes[COMMON_PALETTE_OFFSET + 95]);
    assert(memory->palette[96] == bytes[PALETTE_OFFSET + 32]);
    assert(memory->palette[511] == bytes[PALETTE_OFFSET + 447]);
    FusionMzmMetatileTable *metatiles = malloc(sizeof *metatiles);
    assert(metatiles);
    assert(fusion_mzm_read_tileset_metatiles(
        &rom, &symbols, TILESET_INDEX, metatiles, error, sizeof error));
    assert(metatiles->tileset_word_count == 4 &&
           metatiles->common_word_count ==
               FUSION_MZM_COMMON_METATILE_COUNT *
                   FUSION_MZM_METATILE_CELLS);
    assert(metatiles->cells[0] == 0x1001 &&
           metatiles->cells[3] == 0x1004 && metatiles->cells[4] == 0 &&
           metatiles->cells[4095] == 0 &&
           metatiles->cells[4096] == 0x4000 &&
           metatiles->cells[FUSION_MZM_METATILE_WORD_COUNT - 1] == 0x433F);
    for (size_t cell = 0; cell < FUSION_MZM_METATILE_CELLS; ++cell) {
        metatiles->cells[cell] = 1;
        metatiles->cells[
            FUSION_MZM_TILESET_METATILE_COUNT *
                FUSION_MZM_METATILE_CELLS + cell] = 0x1001;
    }
    memset(memory->vram + 0x4000 + 32, 0x11, 32);
    write_u16(memory->palette, 2, 0x001F);
    write_u16(memory->palette, 34, 0x03E0);
    uint16_t block_cells[] = {0, 0x400};
    FusionMzmRoomRleMap render_map = {
        .width = 2, .height = 1, .cells = block_cells, .cell_count = 2
    };
    FusionMzmRenderedBackground rendered;
    assert(fusion_mzm_render_rle_background(
        &render_map, metatiles, memory, &rendered, error, sizeof error));
    assert(rendered.width_pixels == 32 && rendered.height_pixels == 16 &&
           rendered.pixel_count == 512 && rendered.unresolved_count == 0);
    assert(rendered.rgba[0] == 255 && rendered.rgba[1] == 0 &&
           rendered.rgba[2] == 0 && rendered.rgba[3] == 255);
    size_t common_pixel = 16 * 4;
    assert(rendered.rgba[common_pixel] == 0 &&
           rendered.rgba[common_pixel + 1] == 255 &&
           rendered.rgba[common_pixel + 2] == 0 &&
           rendered.rgba[common_pixel + 3] == 255);
    fusion_mzm_free_rendered_background(&rendered);
    assert(rendered.rgba == NULL && rendered.pixel_count == 0);

    uint16_t *text_cells = calloc(1024, sizeof *text_cells);
    assert(text_cells);
    text_cells[0] = 1;
    memset(memory->vram + 0x8000 + 32, 0x11, 32);
    FusionMzmTextTilemap text_map = {
        .width_tiles = 32, .height_tiles = 32,
        .cells = text_cells, .cell_count = 1024
    };
    assert(fusion_mzm_render_text_background(
        &text_map, memory, &rendered, error, sizeof error));
    assert(rendered.width_pixels == 256 && rendered.height_pixels == 256 &&
           rendered.pixel_count == 65536 && rendered.unresolved_count == 0);
    assert(rendered.rgba[0] == 255 && rendered.rgba[1] == 0 &&
           rendered.rgba[2] == 0 && rendered.rgba[3] == 255);
    fusion_mzm_free_rendered_background(&rendered);
    free(text_cells);
    uint8_t bg1_rgba[] = {
        255, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0
    };
    uint8_t bg2_rgba[] = {
        0, 0, 255, 255, 0, 255, 0, 255, 0, 0, 0, 0
    };
    FusionMzmRenderedBackground bg1 = {3, 1, bg1_rgba, 3, 0};
    FusionMzmRenderedBackground bg2 = {3, 1, bg2_rgba, 3, 0};
    FusionMzmCompositeBackground composite;
    assert(fusion_mzm_compose_bg12(
        &bg1, &bg2, &composite, error, sizeof error));
    assert(composite.width_pixels == 3 && composite.height_pixels == 1 &&
           composite.pixel_count == 3 && composite.bg1_visible_count == 1 &&
           composite.bg2_visible_count == 1 &&
           composite.unresolved_count == 1);
    assert(!memcmp(composite.rgb,
                   (uint8_t[]){255, 0, 0, 0, 255, 0, 0, 0, 0}, 9));
    assert(!memcmp(composite.visibility, (uint8_t[]){1, 1, 0}, 3));
    FusionMzmSerializedBackground serialized;
    assert(fusion_mzm_serialize_background(
        &composite, &serialized, error, sizeof error));
    assert(serialized.bitmap_size == 66 &&
           serialized.visibility_size == 3 &&
           serialized.bitmap[0] == 'B' && serialized.bitmap[1] == 'M' &&
           serialized.bitmap[2] == 66 && serialized.bitmap[10] == 54 &&
           serialized.bitmap[14] == 40 && serialized.bitmap[18] == 3 &&
           serialized.bitmap[22] == 1 && serialized.bitmap[26] == 1 &&
           serialized.bitmap[28] == 24);
    assert(!memcmp(serialized.bitmap + 54,
                   (uint8_t[]){0, 0, 255, 0, 255, 0, 0, 0, 0, 0, 0, 0},
                   12));
    assert(!memcmp(serialized.visibility, (uint8_t[]){1, 1, 0}, 3));
    composite.visibility[0] = 2;
    FusionMzmSerializedBackground rejected;
    assert(!fusion_mzm_serialize_background(
        &composite, &rejected, error, sizeof error));
    composite.visibility[0] = 1;
    fusion_mzm_free_serialized_background(&serialized);
    assert(serialized.bitmap == NULL && serialized.visibility == NULL &&
           serialized.bitmap_size == 0 && serialized.visibility_size == 0);
    fusion_mzm_free_composite_background(&composite);
    assert(composite.rgb == NULL && composite.visibility == NULL &&
           composite.pixel_count == 0);
    assert(fusion_mzm_select_bg12(
        &bg1, NULL, &composite, error, sizeof error));
    assert(composite.bg1_visible_count == 1 &&
           composite.bg2_visible_count == 0 &&
           composite.unresolved_count == 2);
    assert(!memcmp(composite.rgb,
                   (uint8_t[]){255, 0, 0, 0, 0, 0, 0, 0, 0}, 9));
    assert(!memcmp(composite.visibility, (uint8_t[]){1, 0, 0}, 3));
    fusion_mzm_free_composite_background(&composite);
    assert(!fusion_mzm_compose_bg12(
        &bg1, NULL, &composite, error, sizeof error));
    bg2.width_pixels = 2;
    assert(!fusion_mzm_compose_bg12(
        &bg1, &bg2, &composite, error, sizeof error));
    symbol_entries[4].size = 1;
    assert(!fusion_mzm_read_tileset_metatiles(
        &rom, &symbols, TILESET_INDEX, metatiles, error, sizeof error));
    symbol_entries[4].size = 10;
    free(metatiles);
    bytes[GRAPHICS_OFFSET] = 0;
    assert(!fusion_mzm_load_tileset_video_memory(
        &rom, &symbols, TILESET_INDEX, memory, error, sizeof error));
    free(memory);
    free(bytes);
}

static void test_runtime_room_core(void) {
    enum {
        FIXTURE_SIZE = 0x800,
        COLLISION_OFFSET = 0x20,
        BEHAVIOR_OFFSET = 0x100,
        TILE_COLLISION_OFFSET = 0x280,
        TILE_BEHAVIOR_OFFSET = 0x380,
        MOVEMENT_OFFSET = 0x600
    };
    uint8_t bytes[FIXTURE_SIZE] = {0};
    bytes[COLLISION_OFFSET + 16] = 1;
    bytes[COLLISION_OFFSET + 32] = 11;
    write_u16(bytes, BEHAVIOR_OFFSET + 32 * 2, 3);
    write_u16(bytes, BEHAVIOR_OFFSET + 39 * 2, 4);
    write_u16(bytes, BEHAVIOR_OFFSET + 40 * 2, 5);
    write_u16(bytes, BEHAVIOR_OFFSET + 24 * 2, 8);
    bytes[TILE_COLLISION_OFFSET + 1] = 1;
    write_u16(bytes, TILE_BEHAVIOR_OFFSET + 1 * 2, 9);
    write_u16(bytes, MOVEMENT_OFFSET + 8 * 2, 8);
    write_u16(bytes, MOVEMENT_OFFSET + 9 * 2, 9);
    FusionMzmRomSymbol entries[] = {
        {"sClipdataCollisionTypes", 0x08000000 + COLLISION_OFFSET,
         FUSION_MZM_CLIPDATA_COUNT, 0},
        {"sClipdataBehaviorTypes", 0x08000000 + BEHAVIOR_OFFSET,
         FUSION_MZM_CLIPDATA_COUNT * 2, 1},
        {"sClipdataCollisionTypes_Tilemap",
         0x08000000 + TILE_COLLISION_OFFSET,
         FUSION_MZM_CLIPDATA_TILEMAP_COUNT, 2},
        {"sClipdataBehaviorTypes_Tilemap",
         0x08000000 + TILE_BEHAVIOR_OFFSET,
         FUSION_MZM_CLIPDATA_TILEMAP_COUNT * 2, 3},
        {"sMovementClipdataValues", 0x08000000 + MOVEMENT_OFFSET,
         FUSION_MZM_CLIPDATA_MOVEMENT_COUNT * 2, 4},
    };
    FusionMzmRomSymbolTable symbols = {
        entries, sizeof entries / sizeof entries[0], 0
    };
    FusionRom rom = {bytes, sizeof bytes, FUSION_ASSET_GAME_METROID};
    char error[160];
    FusionMzmClipdataTables tables;
    assert(fusion_mzm_read_clipdata_tables(
        &rom, &symbols, &tables, error, sizeof error));
    assert(tables.collision[16] == 1 && tables.collision[32] == 11 &&
           tables.behavior[32] == 3 && tables.behavior[39] == 4 &&
           tables.tilemap_collision[1] == 1 &&
           tables.tilemap_behavior[1] == 9 && tables.movement[9] == 9);
    uint16_t cells[] = {16, 32, 39, 40, 24, 0x401};
    FusionMzmRoomRleMap clipdata = {
        .width = 2, .height = 3, .cells = cells, .cell_count = 6
    };
    FusionMzmRuntimeRoomPayload payload;
    assert(fusion_mzm_serialize_runtime_room_core(
        "Brinstar", 7, &clipdata, &tables, &payload,
        error, sizeof error));
    static const char expected[] =
        "MVROOM-NATIVE\t1\tmzm\tBrinstar\t7\t32\t48\n"
        "C\t0\t0\t16\t1\n"
        "C\t1\t0\t32\t11\n"
        "B\t1\t0\tdoor\n"
        "C\t0\t1\t39\t0\n"
        "B\t0\t1\tup\n"
        "C\t1\t1\t40\t0\n"
        "B\t1\t1\tdown\n"
        "C\t0\t2\t24\t0\n"
        "M\t0\t2\t8\n"
        "C\t1\t2\t1025\t1\n"
        "M\t1\t2\t9\n"
        "END\n";
    assert(payload.size == sizeof expected - 1 &&
           !memcmp(payload.data, expected, sizeof expected) &&
           payload.clipdata_count == 6 && payload.transition_count == 3 &&
           payload.movement_count == 2);
    fusion_mzm_free_runtime_room_payload(&payload);
    assert(payload.data == NULL && payload.size == 0 &&
           payload.clipdata_count == 0);
    cells[5] = FUSION_MZM_CLIPDATA_TILEMAP_FLAG +
               FUSION_MZM_CLIPDATA_TILEMAP_COUNT;
    assert(!fusion_mzm_serialize_runtime_room_core(
        "Brinstar", 7, &clipdata, &tables, &payload,
        error, sizeof error));
    cells[5] = 0x401;
    assert(!fusion_mzm_serialize_runtime_room_core(
        "Bad\tArea", 7, &clipdata, &tables, &payload,
        error, sizeof error));
    entries[4].size--;
    assert(!fusion_mzm_read_clipdata_tables(
        &rom, &symbols, &tables, error, sizeof error));
}

int main(void) {
    uint8_t elf[ELF_SIZE];
    uint8_t rom_bytes[ROM_SIZE] = {0};
    make_elf(elf);
    FusionRom rom = {rom_bytes, sizeof rom_bytes, FUSION_ASSET_GAME_METROID};
    FusionMzmRomSymbolTable table;
    char error[160];
    assert(fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                       error, sizeof error));
    assert(table.count == 2);
    assert(table.duplicate_name_count == 1);
    assert(!strcmp(table.entries[0].name, "first"));
    assert(table.entries[0].address == 0x08000010 && table.entries[0].size == 8);
    assert(!strcmp(table.entries[1].name, "dup"));
    assert(table.entries[1].address == 0x08000020 && table.entries[1].size == 4);
    const char *bank_path = "fusion-mzm-rom-bank-test.bin";
    const char *manifest_path = "fusion-mzm-rom-symbols-test.tsv";
    remove(bank_path);
    remove(manifest_path);
    assert(fusion_mzm_write_rom_bank(&rom, &table, bank_path, manifest_path,
                                     error, sizeof error));
    uint8_t bank[16 + ROM_SIZE];
    assert(read_file(bank_path, bank, sizeof bank) == sizeof bank);
    assert(!memcmp(bank, "MZMROM01", 8));
    assert(bank[8] == 0 && bank[9] == 0 && bank[10] == 0 && bank[11] == 8);
    assert(bank[12] == 0 && bank[13] == 1 &&
           bank[14] == 0 && bank[15] == 0);
    assert(!memcmp(bank + 16, rom_bytes, ROM_SIZE));
    uint8_t manifest[256];
    size_t manifest_size = read_file(manifest_path, manifest, sizeof manifest - 1);
    manifest[manifest_size] = '\0';
    assert(!strcmp((char *)manifest,
                   "schema\tmetroidvania-mzm-rom-v1\n"
                   "symbol\tfirst\t134217744\t8\n"
                   "symbol\tdup\t134217760\t4\n"));
    assert(remove(bank_path) == 0);
    assert(remove(manifest_path) == 0);
    fusion_mzm_free_symbols(&table);
    assert(table.entries == NULL && table.count == 0);

    test_room_entries(&rom);
    test_room_rle(&rom);
    test_room_backgrounds();
    test_tileset_video_memory();
    test_runtime_room_core();

    elf[0] = 0;
    assert(!fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                        error, sizeof error));
    make_elf(elf);
    write_u32(elf, SECTION_OFFSET + 80 + 20, 15);
    assert(!fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                        error, sizeof error));
    make_elf(elf);
    write_symbol(elf, 1, 7, 0x080000F0, 0x20, 1);
    assert(!fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                        error, sizeof error));
    rom.game = FUSION_ASSET_GAME_ARIA;
    assert(!fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                        error, sizeof error));
    puts("Native Zero Mission ROM-symbol tests passed");
    return 0;
}
