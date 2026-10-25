/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/mzm.h"

#include "fusion/assets/gba.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { GBA_ROM_BASE = 0x08000000 };

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static bool pointer_offset(const FusionRom *rom, uint32_t pointer, size_t size,
                           size_t *offset) {
    if (pointer < GBA_ROM_BASE) return false;
    size_t value = (size_t)(pointer - GBA_ROM_BASE);
    if (value > rom->size || size > rom->size - value) return false;
    *offset = value;
    return true;
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static void write_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static bool append_format(uint8_t *data, size_t capacity, size_t *used,
                          const char *format, ...) {
    if (*used >= capacity) return false;
    va_list arguments;
    va_start(arguments, format);
    int written = vsnprintf((char *)data + *used, capacity - *used,
                            format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= capacity - *used) return false;
    *used += (size_t)written;
    return true;
}

static const FusionMzmRomSymbol *find_symbol(
    const FusionMzmRomSymbolTable *symbols, const char *name) {
    if (!symbols || (symbols->count && !symbols->entries)) return NULL;
    for (size_t index = 0; index < symbols->count; ++index) {
        if (symbols->entries[index].name &&
            !strcmp(symbols->entries[index].name, name))
            return &symbols->entries[index];
    }
    return NULL;
}

static const FusionMzmRomSymbol *find_symbol_at(
    const FusionMzmRomSymbolTable *symbols, uint32_t address) {
    if (!symbols || (symbols->count && !symbols->entries)) return NULL;
    for (size_t index = 0; index < symbols->count; ++index) {
        if (symbols->entries[index].address == address)
            return &symbols->entries[index];
    }
    return NULL;
}

static const uint8_t *bounded_symbol_data(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    const char *name, size_t required_size) {
    const FusionMzmRomSymbol *symbol = find_symbol(symbols, name);
    size_t offset;
    if (!symbol || symbol->size < required_size ||
        !pointer_offset(rom, symbol->address, required_size, &offset)) {
        return NULL;
    }
    return rom->data + offset;
}

bool fusion_mzm_read_clipdata_tables(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmClipdataTables *tables,
    char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        !symbols || !tables) {
        set_error(error, error_size,
                  "invalid Zero Mission Clipdata-table request");
        return false;
    }
    *tables = (FusionMzmClipdataTables){0};
    const uint8_t *collision = bounded_symbol_data(
        rom, symbols, "sClipdataCollisionTypes", FUSION_MZM_CLIPDATA_COUNT);
    const uint8_t *behavior = bounded_symbol_data(
        rom, symbols, "sClipdataBehaviorTypes",
        FUSION_MZM_CLIPDATA_COUNT * 2);
    const uint8_t *tilemap_collision = bounded_symbol_data(
        rom, symbols, "sClipdataCollisionTypes_Tilemap",
        FUSION_MZM_CLIPDATA_TILEMAP_COUNT);
    const uint8_t *tilemap_behavior = bounded_symbol_data(
        rom, symbols, "sClipdataBehaviorTypes_Tilemap",
        FUSION_MZM_CLIPDATA_TILEMAP_COUNT * 2);
    const uint8_t *movement = bounded_symbol_data(
        rom, symbols, "sMovementClipdataValues",
        FUSION_MZM_CLIPDATA_MOVEMENT_COUNT * 2);
    if (!collision || !behavior || !tilemap_collision ||
        !tilemap_behavior || !movement) {
        set_error(error, error_size,
                  "missing or invalid Zero Mission Clipdata tables");
        return false;
    }
    memcpy(tables->collision, collision, sizeof tables->collision);
    memcpy(tables->tilemap_collision, tilemap_collision,
           sizeof tables->tilemap_collision);
    for (size_t index = 0; index < FUSION_MZM_CLIPDATA_COUNT; ++index)
        tables->behavior[index] = read_u16(behavior + index * 2);
    for (size_t index = 0; index < FUSION_MZM_CLIPDATA_TILEMAP_COUNT; ++index)
        tables->tilemap_behavior[index] =
            read_u16(tilemap_behavior + index * 2);
    for (size_t index = 0; index < FUSION_MZM_CLIPDATA_MOVEMENT_COUNT; ++index)
        tables->movement[index] = read_u16(movement + index * 2);
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_door_directory(FusionMzmDoorDirectory *directory) {
    if (!directory) return;
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area)
        free(directory->areas[area].entries);
    *directory = (FusionMzmDoorDirectory){0};
}

bool fusion_mzm_read_door_directory(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmDoorDirectory *directory,
    char *error, size_t error_size) {
    static const char *door_symbols[FUSION_MZM_ROOM_AREA_COUNT] = {
        "sBrinstarDoors", "sKraidDoors", "sNorfairDoors",
        "sRidleyDoors", "sTourianDoors", "sCrateriaDoors",
        "sChozodiaDoors", NULL, NULL, NULL, "sTestDoors"
    };
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        !symbols || !directory) {
        set_error(error, error_size,
                  "invalid Zero Mission door-directory request");
        return false;
    }
    *directory = (FusionMzmDoorDirectory){0};
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        if (!door_symbols[area]) continue;
        const FusionMzmRomSymbol *symbol =
            find_symbol(symbols, door_symbols[area]);
        size_t offset;
        if (!symbol || symbol->size == 0 ||
            symbol->size % FUSION_MZM_DOOR_RECORD_SIZE != 0 ||
            symbol->size / FUSION_MZM_DOOR_RECORD_SIZE >
                FUSION_MZM_DOOR_CAPACITY ||
            !pointer_offset(rom, symbol->address, symbol->size, &offset)) {
            fusion_mzm_free_door_directory(directory);
            set_error(error, error_size,
                      "missing or invalid Zero Mission door table");
            return false;
        }
        size_t count = symbol->size / FUSION_MZM_DOOR_RECORD_SIZE;
        FusionMzmDoor *entries = calloc(count, sizeof *entries);
        if (!entries) {
            fusion_mzm_free_door_directory(directory);
            set_error(error, error_size,
                      "cannot allocate Zero Mission door table");
            return false;
        }
        for (size_t index = 0; index < count; ++index) {
            const uint8_t *data = rom->data + offset +
                                  index * FUSION_MZM_DOOR_RECORD_SIZE;
            uint8_t kind = data[0] & 0xF;
            if (kind > 6) {
                free(entries);
                fusion_mzm_free_door_directory(directory);
                set_error(error, error_size,
                          "invalid Zero Mission door type");
                return false;
            }
            entries[index] = (FusionMzmDoor){
                data[0], data[1], data[2], data[3], data[4], data[5],
                data[6], (int8_t)data[7], (int8_t)data[8]
            };
        }
        directory->areas[area] = (FusionMzmDoorList){entries, count};
    }
    const FusionMzmRomSymbol *connections =
        find_symbol(symbols, "sAreaConnections");
    size_t connection_offset;
    if (!connections || connections->size == 0 ||
        connections->size % 3 != 0 ||
        connections->size / 3 > FUSION_MZM_AREA_CONNECTION_CAPACITY ||
        !pointer_offset(rom, connections->address, connections->size,
                        &connection_offset)) {
        fusion_mzm_free_door_directory(directory);
        set_error(error, error_size,
                  "missing or invalid Zero Mission area connections");
        return false;
    }
    for (size_t index = 0; index < connections->size / 3; ++index) {
        const uint8_t *data = rom->data + connection_offset + index * 3;
        if (data[0] == UINT8_MAX) break;
        if (data[0] >= 7 || data[2] >= 7) {
            fusion_mzm_free_door_directory(directory);
            set_error(error, error_size,
                      "invalid Zero Mission area connection");
            return false;
        }
        directory->connections[directory->connection_count++] =
            (FusionMzmAreaConnection){data[0], data[1], data[2]};
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_hatch_tables(FusionMzmHatchTables *tables) {
    if (!tables) return;
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area)
        free(tables->locks[area].entries);
    *tables = (FusionMzmHatchTables){0};
}

bool fusion_mzm_read_hatch_tables(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmHatchTables *tables,
    char *error, size_t error_size) {
    static const char *lock_symbols[FUSION_MZM_ROOM_AREA_COUNT] = {
        "sHatchLockEventsBrinstar", "sHatchLockEventsKraid", NULL,
        NULL, NULL, "sHatchLockEventsCrateria",
        "sHatchLockEventsChozodia", NULL, NULL, NULL, NULL
    };
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        !symbols || !tables) {
        set_error(error, error_size,
                  "invalid Zero Mission hatch-table request");
        return false;
    }
    *tables = (FusionMzmHatchTables){0};
    const FusionMzmRomSymbol *types = find_symbol(symbols, "sHatchTypeTable");
    const FusionMzmRomSymbol *durations =
        find_symbol(symbols, "sHatchesAnimationDurationCounter");
    const FusionMzmRomSymbol *tilemaps =
        find_symbol(symbols, "sHatchesTilemapValues");
    const FusionMzmRomSymbol *behaviors =
        find_symbol(symbols, "sHatchBehaviors");
    size_t type_offset, duration_offset, tilemap_offset, behavior_offset;
    if (!types || types->size != FUSION_MZM_HATCH_COUNT ||
        !durations ||
        durations->size != FUSION_MZM_HATCH_ANIMATION_COUNT ||
        !tilemaps || tilemaps->size != FUSION_MZM_HATCH_COUNT * 2 ||
        !behaviors || behaviors->size != FUSION_MZM_HATCH_COUNT * 4 ||
        !pointer_offset(rom, types->address, types->size, &type_offset) ||
        !pointer_offset(rom, durations->address, durations->size,
                        &duration_offset) ||
        !pointer_offset(rom, tilemaps->address, tilemaps->size,
                        &tilemap_offset) ||
        !pointer_offset(rom, behaviors->address, behaviors->size,
                        &behavior_offset)) {
        set_error(error, error_size,
                  "missing or invalid Zero Mission hatch tables");
        return false;
    }
    memcpy(tables->types, rom->data + type_offset, sizeof tables->types);
    memcpy(tables->durations, rom->data + duration_offset,
           sizeof tables->durations);
    for (size_t index = 0; index < FUSION_MZM_HATCH_COUNT; ++index) {
        if (tables->types[index] >= FUSION_MZM_HATCH_COUNT) {
            set_error(error, error_size,
                      "invalid Zero Mission hatch type mapping");
            return false;
        }
        tables->tilemap_values[index] =
            read_u16(rom->data + tilemap_offset + index * 2);
        tables->weakness[index] =
            read_u16(rom->data + behavior_offset + index * 4);
        tables->health[index] =
            read_u16(rom->data + behavior_offset + index * 4 + 2);
    }
    for (size_t area = 0; area < FUSION_MZM_ROOM_AREA_COUNT; ++area) {
        if (!lock_symbols[area]) continue;
        const FusionMzmRomSymbol *symbol =
            find_symbol(symbols, lock_symbols[area]);
        size_t offset;
        if (!symbol || symbol->size == 0 ||
            symbol->size % FUSION_MZM_HATCH_LOCK_RECORD_SIZE != 0 ||
            symbol->size / FUSION_MZM_HATCH_LOCK_RECORD_SIZE >
                FUSION_MZM_HATCH_LOCK_CAPACITY ||
            !pointer_offset(rom, symbol->address, symbol->size, &offset)) {
            fusion_mzm_free_hatch_tables(tables);
            set_error(error, error_size,
                      "missing or invalid Zero Mission hatch lock table");
            return false;
        }
        size_t count = symbol->size / FUSION_MZM_HATCH_LOCK_RECORD_SIZE;
        FusionMzmHatchLockEvent *entries = calloc(count, sizeof *entries);
        if (!entries) {
            fusion_mzm_free_hatch_tables(tables);
            set_error(error, error_size,
                      "cannot allocate Zero Mission hatch lock table");
            return false;
        }
        for (size_t index = 0; index < count; ++index) {
            const uint8_t *data = rom->data + offset +
                index * FUSION_MZM_HATCH_LOCK_RECORD_SIZE;
            if (data[2] > 3) {
                free(entries);
                fusion_mzm_free_hatch_tables(tables);
                set_error(error, error_size,
                          "invalid Zero Mission hatch lock type");
                return false;
            }
            entries[index] = (FusionMzmHatchLockEvent){
                data[0], data[1], data[2], read_u16(data + 3)
            };
        }
        tables->locks[area] = (FusionMzmHatchLockList){entries, count};
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_mzm_read_tileset_entry(const FusionRom *rom,
                                   const FusionMzmRomSymbolTable *symbols,
                                   size_t index, FusionMzmTilesetEntry *entry,
                                   char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        !symbols || !entry || index >= FUSION_MZM_TILESET_COUNT) {
        set_error(error, error_size, "invalid Zero Mission tileset request");
        return false;
    }
    *entry = (FusionMzmTilesetEntry){0};
    const FusionMzmRomSymbol *table = find_symbol(symbols, "sTilesetEntries");
    size_t required = FUSION_MZM_TILESET_COUNT * FUSION_MZM_TILESET_ENTRY_SIZE;
    size_t table_offset;
    if (!table || table->size < required ||
        !pointer_offset(rom, table->address, required, &table_offset)) {
        set_error(error, error_size,
                  "missing or invalid Zero Mission tileset table");
        return false;
    }
    const uint8_t *data = rom->data + table_offset +
                          index * FUSION_MZM_TILESET_ENTRY_SIZE;
    *entry = (FusionMzmTilesetEntry){
        (uint8_t)index, read_u32(data), read_u32(data + 4),
        read_u32(data + 8), read_u32(data + 12), data[16], data[17]
    };
    const uint32_t pointers[] = {
        entry->graphics_pointer, entry->palette_pointer,
        entry->background_graphics_pointer, entry->tilemap_pointer
    };
    for (size_t pointer = 0;
         pointer < sizeof pointers / sizeof pointers[0]; ++pointer) {
        size_t ignored;
        if (!pointer_offset(rom, pointers[pointer], 1, &ignored)) {
            set_error(error, error_size,
                      "Zero Mission tileset resource points outside the ROM");
            *entry = (FusionMzmTilesetEntry){0};
            return false;
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_mzm_load_tileset_video_memory(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    size_t index, FusionMzmVideoMemory *memory,
    char *error, size_t error_size) {
    if (!memory) {
        set_error(error, error_size,
                  "invalid Zero Mission tileset video-memory request");
        return false;
    }
    *memory = (FusionMzmVideoMemory){0};
    if (!fusion_mzm_read_tileset_entry(rom, symbols, index, &memory->entry,
                                       error, error_size)) {
        return false;
    }

    const FusionMzmRomSymbol *common_gfx =
        find_symbol(symbols, "sCommonTilesGfx");
    const FusionMzmRomSymbol *common_palette =
        find_symbol(symbols, "sCommonTilesPal");
    const FusionMzmRomSymbol *door_palette =
        find_symbol(symbols, "sDoorTransitionPal");
    size_t common_gfx_offset, common_palette_offset;
    if (!common_gfx || common_gfx->size > sizeof memory->vram - 0x4800 ||
        !pointer_offset(rom, common_gfx->address, common_gfx->size,
                        &common_gfx_offset)) {
        set_error(error, error_size,
                  "missing or invalid Zero Mission common tile graphics");
        return false;
    }
    if (!common_palette || !door_palette || common_palette->size < 32 ||
        door_palette->size < 64 ||
        door_palette->address != common_palette->address + common_palette->size ||
        !pointer_offset(rom, common_palette->address, 96,
                        &common_palette_offset)) {
        set_error(error, error_size,
                  "missing or invalid Zero Mission common tile palette");
        return false;
    }
    memcpy(memory->vram + 0x4800, rom->data + common_gfx_offset,
           common_gfx->size);
    memory->common_graphics_size = common_gfx->size;

    size_t graphics_offset;
    if (!pointer_offset(rom, memory->entry.graphics_pointer, 4,
                        &graphics_offset)) {
        set_error(error, error_size,
                  "Zero Mission tileset graphics point outside the ROM");
        return false;
    }
    uint32_t graphics_size = fusion_gba_uncompressed_size(
        rom->data + graphics_offset, rom->size - graphics_offset);
    if (graphics_size == 0 || graphics_size > sizeof memory->vram - 0x5800 ||
        !fusion_gba_lz77_uncompress(rom->data + graphics_offset,
                                    rom->size - graphics_offset,
                                    memory->vram + 0x5800,
                                    sizeof memory->vram - 0x5800,
                                    &memory->graphics_size)) {
        set_error(error, error_size,
                  "invalid Zero Mission tileset graphics");
        return false;
    }

    size_t background_offset;
    if (!pointer_offset(rom, memory->entry.background_graphics_pointer, 4,
                        &background_offset)) {
        set_error(error, error_size,
                  "Zero Mission background graphics point outside the ROM");
        return false;
    }
    uint32_t background_size = fusion_gba_uncompressed_size(
        rom->data + background_offset, rom->size - background_offset);
    if (background_size == 0 || background_size > 0xFDE0) {
        set_error(error, error_size,
                  "invalid Zero Mission background graphics size");
        return false;
    }
    size_t background_destination = 0xFDE0 - background_size;
    if (!fusion_gba_lz77_uncompress(rom->data + background_offset,
                                    rom->size - background_offset,
                                    memory->vram + background_destination,
                                    sizeof memory->vram - background_destination,
                                    &memory->background_graphics_size)) {
        set_error(error, error_size,
                  "invalid Zero Mission background graphics");
        return false;
    }
    memset(memory->vram + 0xFFE0, 0, 0x20);

    size_t palette_offset;
    if (!pointer_offset(rom, memory->entry.palette_pointer, 14 * 32,
                        &palette_offset)) {
        set_error(error, error_size,
                  "Zero Mission tileset palette points outside the ROM");
        return false;
    }
    memcpy(memory->palette + 3 * 32,
           rom->data + palette_offset + 32, 13 * 32);
    memcpy(memory->palette + 2, rom->data + common_palette_offset + 2, 94);
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_mzm_read_tileset_metatiles(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    size_t index, FusionMzmMetatileTable *table,
    char *error, size_t error_size) {
    if (!table) {
        set_error(error, error_size,
                  "invalid Zero Mission metatile-table request");
        return false;
    }
    *table = (FusionMzmMetatileTable){0};
    if (!fusion_mzm_read_tileset_entry(rom, symbols, index, &table->entry,
                                       error, error_size)) {
        return false;
    }
    const FusionMzmRomSymbol *tileset =
        find_symbol_at(symbols, table->entry.tilemap_pointer);
    size_t tileset_offset;
    if (!tileset || tileset->size < 2 || (tileset->size - 2) % 2 != 0 ||
        !pointer_offset(rom, tileset->address, tileset->size,
                        &tileset_offset)) {
        set_error(error, error_size,
                  "missing or invalid Zero Mission tileset metatiles");
        return false;
    }
    size_t available_words = (tileset->size - 2) / 2;
    size_t tileset_capacity = FUSION_MZM_TILESET_METATILE_COUNT *
                              FUSION_MZM_METATILE_CELLS;
    table->tileset_word_count = available_words < tileset_capacity
        ? available_words : tileset_capacity;
    for (size_t word = 0; word < table->tileset_word_count; ++word)
        table->cells[word] = read_u16(
            rom->data + tileset_offset + 2 + word * 2);

    const FusionMzmRomSymbol *common =
        find_symbol(symbols, "sCommonTilemap");
    size_t common_words = FUSION_MZM_COMMON_METATILE_COUNT *
                          FUSION_MZM_METATILE_CELLS;
    size_t common_size = common_words * 2;
    size_t common_offset;
    if (!common || common->size < common_size ||
        !pointer_offset(rom, common->address, common_size, &common_offset)) {
        set_error(error, error_size,
                  "missing or invalid Zero Mission common metatiles");
        *table = (FusionMzmMetatileTable){0};
        return false;
    }
    size_t destination = tileset_capacity;
    for (size_t word = 0; word < common_words; ++word)
        table->cells[destination + word] =
            read_u16(rom->data + common_offset + word * 2);
    table->common_word_count = common_words;
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_rendered_background(
    FusionMzmRenderedBackground *rendered) {
    if (!rendered) return;
    free(rendered->rgba);
    *rendered = (FusionMzmRenderedBackground){0};
}

static bool allocate_rendered_background(
    size_t width, size_t height, FusionMzmRenderedBackground *rendered,
    char *error, size_t error_size) {
    if (!rendered || width == 0 || height == 0 ||
        width > FUSION_MZM_RENDER_PIXEL_CAPACITY / height) {
        set_error(error, error_size,
                  "invalid Zero Mission background-render request");
        return false;
    }
    *rendered = (FusionMzmRenderedBackground){0};
    size_t pixel_count = width * height;
    if (pixel_count > FUSION_MZM_RENDER_PIXEL_CAPACITY ||
        pixel_count > SIZE_MAX / 4) {
        set_error(error, error_size,
                  "Zero Mission rendered background exceeds the pixel safety cap");
        return false;
    }
    uint8_t *rgba = calloc(pixel_count, 4);
    if (!rgba) {
        set_error(error, error_size,
                  "cannot allocate Zero Mission rendered background");
        return false;
    }
    *rendered = (FusionMzmRenderedBackground){
        width, height, rgba, pixel_count, 0
    };
    return true;
}

static void render_tile(uint16_t word, size_t character_base,
                        const FusionMzmVideoMemory *memory,
                        FusionMzmRenderedBackground *rendered,
                        size_t destination_x, size_t destination_y) {
    size_t tile_offset = character_base + (size_t)(word & 0x3FF) * 32;
    if (tile_offset > sizeof memory->vram ||
        32 > sizeof memory->vram - tile_offset) {
        ++rendered->unresolved_count;
        return;
    }
    size_t palette_bank = (word >> 12) & 0xF;
    for (size_t y = 0; y < 8; ++y) {
        size_t source_y = (word & 0x800) ? 7 - y : y;
        for (size_t x = 0; x < 8; ++x) {
            size_t source_x = (word & 0x400) ? 7 - x : x;
            uint8_t packed = memory->vram[
                tile_offset + source_y * 4 + source_x / 2];
            uint8_t color_index =
                (packed >> (4 * (source_x & 1))) & 0xF;
            if (color_index == 0) continue;
            size_t color_offset = (palette_bank * 16 + color_index) * 2;
            uint16_t color = read_u16(memory->palette + color_offset);
            size_t destination =
                ((destination_y + y) * rendered->width_pixels +
                 destination_x + x) * 4;
            rendered->rgba[destination] =
                (uint8_t)(((color & 31) * 255) / 31);
            rendered->rgba[destination + 1] =
                (uint8_t)((((color >> 5) & 31) * 255) / 31);
            rendered->rgba[destination + 2] =
                (uint8_t)((((color >> 10) & 31) * 255) / 31);
            rendered->rgba[destination + 3] = 255;
        }
    }
}

bool fusion_mzm_render_rle_background(
    const FusionMzmRoomRleMap *map, const FusionMzmMetatileTable *metatiles,
    const FusionMzmVideoMemory *memory,
    FusionMzmRenderedBackground *rendered,
    char *error, size_t error_size) {
    if (!map || !metatiles || !memory || !rendered || !map->cells ||
        map->width == 0 || map->height == 0 ||
        map->cell_count != (size_t)map->width * map->height ||
        metatiles->tileset_word_count >
            FUSION_MZM_TILESET_METATILE_COUNT * FUSION_MZM_METATILE_CELLS ||
        metatiles->common_word_count !=
            FUSION_MZM_COMMON_METATILE_COUNT * FUSION_MZM_METATILE_CELLS) {
        set_error(error, error_size,
                  "invalid Zero Mission RLE background-render request");
        return false;
    }
    if (!allocate_rendered_background((size_t)map->width * 16,
                                      (size_t)map->height * 16,
                                      rendered, error, error_size)) {
        return false;
    }
    for (size_t block_y = 0; block_y < map->height; ++block_y) {
        for (size_t block_x = 0; block_x < map->width; ++block_x) {
            size_t value = map->cells[block_x + block_y * map->width];
            size_t block;
            if (value & 0x400) {
                size_t common = value - 0x400;
                block = common < FUSION_MZM_COMMON_METATILE_COUNT
                    ? FUSION_MZM_TILESET_METATILE_COUNT + common : 0;
            } else {
                block = value < FUSION_MZM_TILESET_METATILE_COUNT ? value : 0;
            }
            size_t first_word = block * FUSION_MZM_METATILE_CELLS;
            for (size_t cell = 0; cell < FUSION_MZM_METATILE_CELLS; ++cell) {
                render_tile(metatiles->cells[first_word + cell], 0x4000,
                            memory, rendered,
                            block_x * 16 + cell % 2 * 8,
                            block_y * 16 + cell / 2 * 8);
            }
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_mzm_render_text_background(
    const FusionMzmTextTilemap *map, const FusionMzmVideoMemory *memory,
    FusionMzmRenderedBackground *rendered,
    char *error, size_t error_size) {
    if (!map || !memory || !rendered || !map->cells ||
        map->width_tiles == 0 || map->height_tiles == 0 ||
        map->width_tiles % 32 != 0 || map->height_tiles % 32 != 0 ||
        map->cell_count != (size_t)map->width_tiles * map->height_tiles) {
        set_error(error, error_size,
                  "invalid Zero Mission text background-render request");
        return false;
    }
    if (!allocate_rendered_background((size_t)map->width_tiles * 8,
                                      (size_t)map->height_tiles * 8,
                                      rendered, error, error_size)) {
        return false;
    }
    size_t screens_x = map->width_tiles / 32;
    size_t screens_y = map->height_tiles / 32;
    for (size_t screen_y = 0; screen_y < screens_y; ++screen_y) {
        for (size_t screen_x = 0; screen_x < screens_x; ++screen_x) {
            size_t screen = screen_y * screens_x + screen_x;
            for (size_t tile_y = 0; tile_y < 32; ++tile_y) {
                for (size_t tile_x = 0; tile_x < 32; ++tile_x) {
                    size_t source = screen * 1024 + tile_y * 32 + tile_x;
                    render_tile(map->cells[source], 0x8000, memory, rendered,
                                screen_x * 256 + tile_x * 8,
                                screen_y * 256 + tile_y * 8);
                }
            }
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_mzm_render_metatile_sheet(
    const FusionMzmMetatileTable *metatiles,
    const FusionMzmVideoMemory *memory,
    FusionMzmRenderedBackground *rendered,
    char *error, size_t error_size) {
    if (!metatiles || !memory || !rendered ||
        metatiles->tileset_word_count >
            FUSION_MZM_TILESET_METATILE_COUNT * FUSION_MZM_METATILE_CELLS ||
        metatiles->common_word_count !=
            FUSION_MZM_COMMON_METATILE_COUNT * FUSION_MZM_METATILE_CELLS) {
        set_error(error, error_size,
                  "invalid Zero Mission metatile-sheet request");
        return false;
    }
    size_t rows = (FUSION_MZM_SHEET_BLOCKS + FUSION_MZM_SHEET_COLUMNS - 1) /
                  FUSION_MZM_SHEET_COLUMNS;
    if (!allocate_rendered_background(FUSION_MZM_SHEET_COLUMNS * 16, rows * 16,
                                      rendered, error, error_size)) {
        return false;
    }
    /* Tileset blocks beyond the table are zero words, as the reference
     * pads them. */
    for (size_t block = 0; block < FUSION_MZM_SHEET_BLOCKS; ++block) {
        size_t x = block % FUSION_MZM_SHEET_COLUMNS * 16;
        size_t y = block / FUSION_MZM_SHEET_COLUMNS * 16;
        for (size_t cell = 0; cell < FUSION_MZM_METATILE_CELLS; ++cell) {
            size_t word = block * FUSION_MZM_METATILE_CELLS + cell;
            uint16_t value = word < metatiles->tileset_word_count ||
                                 block >= FUSION_MZM_TILESET_METATILE_COUNT
                ? metatiles->cells[word] : 0;
            render_tile(value, 0x4000, memory, rendered,
                        x + cell % 2 * 8, y + cell / 2 * 8);
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_bitmap(FusionMzmBitmap *bitmap) {
    if (!bitmap) return;
    free(bitmap->data);
    *bitmap = (FusionMzmBitmap){0};
}

bool fusion_mzm_serialize_bmp32(
    const FusionMzmRenderedBackground *rendered, FusionMzmBitmap *bitmap,
    char *error, size_t error_size) {
    enum { FILE_HEADER_SIZE = 14, INFO_HEADER_SIZE = 108 };
    if (!bitmap) {
        set_error(error, error_size, "invalid Zero Mission bitmap output");
        return false;
    }
    *bitmap = (FusionMzmBitmap){0};
    if (!rendered || !rendered->rgba || rendered->width_pixels == 0 ||
        rendered->height_pixels == 0 || rendered->width_pixels > 4096 ||
        rendered->height_pixels > 4096 ||
        rendered->pixel_count !=
            rendered->width_pixels * rendered->height_pixels) {
        set_error(error, error_size, "invalid Zero Mission 32-bit bitmap");
        return false;
    }
    size_t pixel_bytes = rendered->pixel_count * 4;
    size_t offset = FILE_HEADER_SIZE + INFO_HEADER_SIZE;
    size_t size = offset + pixel_bytes;
    uint8_t *data = calloc(size, 1);
    if (!data) {
        set_error(error, error_size, "cannot allocate Zero Mission bitmap");
        return false;
    }
    data[0] = 'B';
    data[1] = 'M';
    write_u32(data + 2, (uint32_t)size);
    write_u32(data + 10, (uint32_t)offset);
    uint8_t *info = data + FILE_HEADER_SIZE;
    write_u32(info, INFO_HEADER_SIZE);
    write_u32(info + 4, (uint32_t)rendered->width_pixels);
    write_u32(info + 8, (uint32_t)-(int32_t)rendered->height_pixels);
    write_u16(info + 12, 1);
    write_u16(info + 14, 32);
    write_u32(info + 16, 3);  /* BI_BITFIELDS */
    write_u32(info + 20, (uint32_t)pixel_bytes);
    write_u32(info + 24, 2835);
    write_u32(info + 28, 2835);
    write_u32(info + 40, 0x00FF0000);
    write_u32(info + 44, 0x0000FF00);
    write_u32(info + 48, 0x000000FF);
    write_u32(info + 52, 0xFF000000);
    memcpy(info + 56, "BGRs", 4);
    for (size_t pixel = 0; pixel < rendered->pixel_count; ++pixel) {
        const uint8_t *source = rendered->rgba + pixel * 4;
        uint8_t *destination = data + offset + pixel * 4;
        destination[0] = source[2];
        destination[1] = source[1];
        destination[2] = source[0];
        destination[3] = source[3];
    }
    *bitmap = (FusionMzmBitmap){data, size};
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_composite_background(
    FusionMzmCompositeBackground *composite) {
    if (!composite) return;
    free(composite->rgb);
    free(composite->visibility);
    *composite = (FusionMzmCompositeBackground){0};
}

bool fusion_mzm_compose_bg12(
    const FusionMzmRenderedBackground *bg1,
    const FusionMzmRenderedBackground *bg2,
    FusionMzmCompositeBackground *composite,
    char *error, size_t error_size) {
    if (!bg2) {
        if (composite) *composite = (FusionMzmCompositeBackground){0};
        set_error(error, error_size,
                  "invalid Zero Mission BG1/BG2 composite layers");
        return false;
    }
    return fusion_mzm_select_bg12(bg1, bg2, composite, error, error_size);
}

bool fusion_mzm_select_bg12(
    const FusionMzmRenderedBackground *bg1,
    const FusionMzmRenderedBackground *bg2,
    FusionMzmCompositeBackground *composite,
    char *error, size_t error_size) {
    if (!composite) {
        set_error(error, error_size,
                  "invalid Zero Mission background-composite output");
        return false;
    }
    *composite = (FusionMzmCompositeBackground){0};
    if (!bg1 || !bg1->rgba ||
        bg1->width_pixels == 0 || bg1->height_pixels == 0 ||
        bg1->width_pixels >
            FUSION_MZM_RENDER_PIXEL_CAPACITY / bg1->height_pixels ||
        bg1->pixel_count != bg1->width_pixels * bg1->height_pixels ||
        bg1->pixel_count > FUSION_MZM_RENDER_PIXEL_CAPACITY ||
        bg1->pixel_count > SIZE_MAX / 3 ||
        (bg2 && (!bg2->rgba ||
                 bg1->width_pixels != bg2->width_pixels ||
                 bg1->height_pixels != bg2->height_pixels ||
                 bg2->pixel_count != bg1->pixel_count))) {
        set_error(error, error_size,
                  "invalid Zero Mission background-selection layers");
        return false;
    }
    uint8_t *rgb = calloc(bg1->pixel_count, 3);
    uint8_t *visibility = calloc(bg1->pixel_count, 1);
    if (!rgb || !visibility) {
        free(rgb);
        free(visibility);
        set_error(error, error_size,
                  "cannot allocate Zero Mission background composite");
        return false;
    }
    size_t bg1_visible = 0, bg2_visible = 0, unresolved = 0;
    for (size_t pixel = 0; pixel < bg1->pixel_count; ++pixel) {
        size_t rgba_offset = pixel * 4;
        size_t rgb_offset = pixel * 3;
        if (bg1->rgba[rgba_offset + 3] != 0) {
            memcpy(rgb + rgb_offset, bg1->rgba + rgba_offset, 3);
            visibility[pixel] = 1;
            ++bg1_visible;
        } else if (bg2 && bg2->rgba[rgba_offset + 3] != 0) {
            memcpy(rgb + rgb_offset, bg2->rgba + rgba_offset, 3);
            visibility[pixel] = 1;
            ++bg2_visible;
        } else {
            ++unresolved;
        }
    }
    *composite = (FusionMzmCompositeBackground){
        bg1->width_pixels, bg1->height_pixels, rgb, visibility,
        bg1->pixel_count, bg1_visible, bg2_visible, unresolved
    };
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_serialized_background(
    FusionMzmSerializedBackground *serialized) {
    if (!serialized) return;
    free(serialized->bitmap);
    free(serialized->visibility);
    *serialized = (FusionMzmSerializedBackground){0};
}

bool fusion_mzm_serialize_background(
    const FusionMzmCompositeBackground *composite,
    FusionMzmSerializedBackground *serialized,
    char *error, size_t error_size) {
    enum { BMP_HEADER_SIZE = 54 };
    if (!serialized) {
        set_error(error, error_size,
                  "invalid Zero Mission serialized-background output");
        return false;
    }
    *serialized = (FusionMzmSerializedBackground){0};
    if (!composite || !composite->rgb || !composite->visibility ||
        composite->width_pixels == 0 || composite->height_pixels == 0 ||
        composite->width_pixels > 4096 || composite->height_pixels > 4096 ||
        composite->width_pixels > SIZE_MAX / composite->height_pixels ||
        composite->pixel_count !=
            composite->width_pixels * composite->height_pixels ||
        composite->width_pixels > (SIZE_MAX - 3) / 3) {
        set_error(error, error_size,
                  "invalid Zero Mission serialized background");
        return false;
    }
    size_t row_bytes = composite->width_pixels * 3;
    size_t pitch = (row_bytes + 3) & ~(size_t)3;
    if (composite->height_pixels > (SIZE_MAX - BMP_HEADER_SIZE) / pitch) {
        set_error(error, error_size,
                  "Zero Mission background bitmap exceeds the size bound");
        return false;
    }
    size_t pixel_bytes = pitch * composite->height_pixels;
    size_t bitmap_size = BMP_HEADER_SIZE + pixel_bytes;
    if (bitmap_size > UINT32_MAX || pixel_bytes > UINT32_MAX ||
        composite->width_pixels > INT32_MAX ||
        composite->height_pixels > INT32_MAX) {
        set_error(error, error_size,
                  "Zero Mission background bitmap exceeds the BMP format bound");
        return false;
    }
    for (size_t pixel = 0; pixel < composite->pixel_count; ++pixel) {
        if (composite->visibility[pixel] > 1) {
            set_error(error, error_size,
                      "invalid Zero Mission background visibility mask");
            return false;
        }
    }
    uint8_t *bitmap = calloc(bitmap_size, 1);
    uint8_t *visibility = malloc(composite->pixel_count);
    if (!bitmap || !visibility) {
        free(bitmap);
        free(visibility);
        set_error(error, error_size,
                  "cannot allocate Zero Mission serialized background");
        return false;
    }
    bitmap[0] = 'B';
    bitmap[1] = 'M';
    write_u32(bitmap + 2, (uint32_t)bitmap_size);
    write_u32(bitmap + 10, BMP_HEADER_SIZE);
    write_u32(bitmap + 14, 40);
    write_u32(bitmap + 18, (uint32_t)composite->width_pixels);
    write_u32(bitmap + 22, (uint32_t)composite->height_pixels);
    write_u16(bitmap + 26, 1);
    write_u16(bitmap + 28, 24);
    write_u32(bitmap + 34, (uint32_t)pixel_bytes);
    for (size_t y = 0; y < composite->height_pixels; ++y) {
        const uint8_t *source = composite->rgb + y * row_bytes;
        uint8_t *destination = bitmap + BMP_HEADER_SIZE +
            (composite->height_pixels - y - 1) * pitch;
        for (size_t x = 0; x < composite->width_pixels; ++x) {
            destination[x * 3] = source[x * 3 + 2];
            destination[x * 3 + 1] = source[x * 3 + 1];
            destination[x * 3 + 2] = source[x * 3];
        }
    }
    memcpy(visibility, composite->visibility, composite->pixel_count);
    *serialized = (FusionMzmSerializedBackground){
        bitmap, bitmap_size, visibility, composite->pixel_count
    };
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

/* The first sized symbol at an address (array data, not a label). */
static const FusionMzmRomSymbol *find_sized_symbol_at(
    const FusionMzmRomSymbolTable *symbols, uint32_t address) {
    if (!symbols || (symbols->count && !symbols->entries)) return NULL;
    for (size_t index = 0; index < symbols->count; ++index) {
        if (symbols->entries[index].address == address &&
            symbols->entries[index].size > 0)
            return &symbols->entries[index];
    }
    return NULL;
}

bool fusion_mzm_read_spritesets(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmSpritesetTable *table,
    char *error, size_t error_size) {
    if (!table) {
        set_error(error, error_size,
                  "invalid Zero Mission spriteset output request");
        return false;
    }
    *table = (FusionMzmSpritesetTable){0};
    const FusionMzmRomSymbol *pointers =
        rom ? find_symbol(symbols, "sSpritesetPointers") : NULL;
    size_t pointers_offset;
    if (!pointers || pointers->size == 0 || pointers->size % 4 != 0 ||
        pointers->size / 4 > FUSION_MZM_SPRITESET_CAPACITY ||
        !pointer_offset(rom, pointers->address, pointers->size,
                        &pointers_offset)) {
        set_error(error, error_size,
                  "missing or invalid Zero Mission sSpritesetPointers");
        return false;
    }
    size_t count = pointers->size / 4;
    for (size_t index = 0; index < count; ++index) {
        uint32_t address = read_u32(rom->data + pointers_offset + index * 4);
        const FusionMzmRomSymbol *set = find_sized_symbol_at(symbols, address);
        size_t offset;
        if (!set || !pointer_offset(rom, set->address, set->size, &offset)) {
            *table = (FusionMzmSpritesetTable){0};
            set_error(error, error_size,
                      "Zero Mission spriteset has no bounded array symbol");
            return false;
        }
        FusionMzmSpriteset *entries = &table->sets[index];
        for (size_t pair = 0; pair + 1 < set->size; pair += 2) {
            uint8_t sprite = rom->data[offset + pair];
            if (sprite == 0) break;  /* SPRITESET_EMPTY */
            if (entries->count == FUSION_MZM_SPRITESET_MEMBER_CAPACITY) {
                *table = (FusionMzmSpritesetTable){0};
                set_error(error, error_size,
                          "Zero Mission spriteset exceeds its member capacity");
                return false;
            }
            entries->members[entries->count++] = (FusionMzmSpritesetMember){
                sprite, rom->data[offset + pair + 1]
            };
        }
    }
    table->count = count;
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

/* sSpritesetPointers index clamped like SpriteLoadSpriteset's table end. */
static uint8_t clamp_spriteset(const FusionMzmSpritesetTable *table,
                               uint8_t index) {
    return index >= table->count - 1 ? (uint8_t)(table->count - 1) : index;
}

bool fusion_mzm_read_room_sprites(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    const FusionMzmSpritesetTable *spritesets,
    const FusionMzmRoomEntry *entry, FusionMzmRoomSprites *sprites,
    char *error, size_t error_size) {
    if (!sprites) {
        set_error(error, error_size,
                  "invalid Zero Mission room-sprite output request");
        return false;
    }
    *sprites = (FusionMzmRoomSprites){0};
    if (!rom || !entry || !spritesets || spritesets->count == 0 ||
        spritesets->count > FUSION_MZM_SPRITESET_CAPACITY) {
        set_error(error, error_size, "invalid Zero Mission room-sprite input");
        return false;
    }
    /* The test areas name UCHAR_MAX spritesets: no sprite choice. */
    if (entry->default_spriteset == UINT8_MAX ||
        entry->first_spriteset == UINT8_MAX ||
        entry->second_spriteset == UINT8_MAX) {
        if (error && error_size > 0) error[0] = '\0';
        return true;
    }
    const struct {
        uint8_t variant, event;
        uint32_t pointer;
        uint8_t spriteset;
    } choices[FUSION_MZM_ROOM_SPRITE_VARIANT_COUNT] = {
        {0, 0, entry->default_sprite_pointer, entry->default_spriteset},
        {1, entry->first_spriteset_event, entry->first_sprite_pointer,
         entry->first_spriteset},
        {2, entry->second_spriteset_event, entry->second_sprite_pointer,
         entry->second_spriteset},
    };
    for (size_t choice = 0; choice < FUSION_MZM_ROOM_SPRITE_VARIANT_COUNT;
         ++choice) {
        if (choices[choice].variant && choices[choice].event == 0) continue;
        const FusionMzmRomSymbol *data =
            find_sized_symbol_at(symbols, choices[choice].pointer);
        size_t offset;
        if (!data || !pointer_offset(rom, data->address, data->size, &offset)) {
            *sprites = (FusionMzmRoomSprites){0};
            set_error(error, error_size,
                      "Zero Mission room sprite data has no bounded symbol");
            return false;
        }
        FusionMzmRoomSpriteVariant *variant =
            &sprites->variants[sprites->count++];
        variant->variant = choices[choice].variant;
        variant->event = choices[choice].event;
        variant->spriteset = clamp_spriteset(spritesets,
                                             choices[choice].spriteset);
        for (size_t index = 0; index + 2 < data->size; index += 3) {
            const uint8_t *triplet = rom->data + offset + index;
            if (triplet[0] == 0xFF) break;  /* ROOM_SPRITE_DATA_TERMINATOR */
            if (variant->placement_count == FUSION_MZM_ROOM_SPRITE_CAPACITY) {
                *sprites = (FusionMzmRoomSprites){0};
                set_error(error, error_size,
                          "Zero Mission room exceeds the sprite capacity");
                return false;
            }
            variant->placements[variant->placement_count++] =
                (FusionMzmRoomSpritePlacement){triplet[0], triplet[1],
                                               triplet[2]};
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_runtime_room_payload(
    FusionMzmRuntimeRoomPayload *payload) {
    if (!payload) return;
    free(payload->data);
    *payload = (FusionMzmRuntimeRoomPayload){0};
}

static bool valid_area_id(const char *area) {
    if (!area || !area[0]) return false;
    for (size_t index = 0; index < 32; ++index) {
        unsigned char value = (unsigned char)area[index];
        if (value == '\0') return true;
        if (!((value >= 'a' && value <= 'z') ||
              (value >= 'A' && value <= 'Z') ||
              (value >= '0' && value <= '9') || value == '_')) {
            return false;
        }
    }
    return false;
}

static bool resolve_clipdata(
    uint16_t raw, const FusionMzmClipdataTables *tables,
    uint8_t *collision, uint16_t *behavior) {
    if (raw < FUSION_MZM_CLIPDATA_COUNT) {
        *collision = tables->collision[raw];
        *behavior = tables->behavior[raw];
        return true;
    }
    if (raw < FUSION_MZM_CLIPDATA_TILEMAP_FLAG) {
        *collision = 0;
        *behavior = 0;
        return true;
    }
    size_t index = raw - FUSION_MZM_CLIPDATA_TILEMAP_FLAG;
    if (index >= FUSION_MZM_CLIPDATA_TILEMAP_COUNT) return false;
    *collision = tables->tilemap_collision[index];
    *behavior = tables->tilemap_behavior[index];
    return true;
}

static const char *door_kind_name(uint8_t type) {
    switch (type & 0xF) {
        case 0: return "none";
        case 1: return "area";
        case 2: return "nohatch";
        case 3:
        case 4: return "hatch";
        case 5:
        case 6: return "mothership";
    }
    return NULL;
}

static const char *hatch_type_name(uint8_t type) {
    static const char *names[FUSION_MZM_HATCH_COUNT] = {
        "none", "unused", "normal", "missile", "super_missile",
        "power_bomb", "locked", "locked_navigation"
    };
    return type < FUSION_MZM_HATCH_COUNT ? names[type] : NULL;
}

static const char *room_effect_name(uint8_t effect) {
    static const char *names[] = {
        "EFFECT_NONE", "EFFECT_WATER", "EFFECT_LAVA",
        "EFFECT_WEAK_ACID", "EFFECT_LAVA_HEAT_HAZE",
        "EFFECT_STRONG_ACID", "EFFECT_SNOWFLAKES_COLD_KNOCKBACK",
        "EFFECT_SNOWFLAKES_COLD", "EFFECT_HEAT_BG3_HAZE",
        "EFFECT_HEAT_BG2_BG3_HAZE", "EFFECT_BG3_GRADIENT",
        "EFFECT_BG2_GRADIENT", "EFFECT_HAZE_BG1_BG2_BG3"
    };
    return effect < sizeof names / sizeof names[0] ? names[effect] : NULL;
}

static size_t runtime_sheet_block(uint16_t value) {
    if (value & FUSION_MZM_CLIPDATA_TILEMAP_FLAG) {
        size_t common = value - FUSION_MZM_CLIPDATA_TILEMAP_FLAG;
        return common < FUSION_MZM_COMMON_METATILE_COUNT
            ? FUSION_MZM_TILESET_METATILE_COUNT + common : 0;
    }
    return value < FUSION_MZM_TILESET_METATILE_COUNT ? value : 0;
}

static bool lowercase_area_id(const char *area, char output[32]) {
    size_t index = 0;
    for (; index < 31 && area[index]; ++index) {
        char value = area[index];
        output[index] = value >= 'A' && value <= 'Z'
            ? (char)(value - 'A' + 'a') : value;
    }
    if (area[index]) return false;
    output[index] = '\0';
    return true;
}

static bool serialize_runtime_room(
    const char *area, size_t area_index, size_t room,
    const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    const FusionMzmDoorDirectory *doors,
    const FusionMzmHatchTables *hatches,
    const FusionMzmRoomEntry *entry,
    const FusionMzmRoomBackgrounds *backgrounds,
    const FusionMzmSpritesetTable *spritesets,
    const FusionMzmRoomSprites *sprites,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size) {
    if (!payload) {
        set_error(error, error_size,
                  "invalid Zero Mission runtime-room output request");
        return false;
    }
    *payload = (FusionMzmRuntimeRoomPayload){0};
    if (!valid_area_id(area) || room >= FUSION_MZM_ROOM_CAPACITY ||
        !clipdata || !tables || !clipdata->cells ||
        clipdata->width == 0 || clipdata->height == 0 ||
        clipdata->cell_count !=
            (size_t)clipdata->width * clipdata->height ||
        clipdata->cell_count > FUSION_MZM_ROOM_RLE_CELL_CAPACITY ||
        clipdata->cell_count > (SIZE_MAX - 256) / 96 ||
        (doors && (area_index >= FUSION_MZM_ROOM_AREA_COUNT ||
                   doors->areas[area_index].count > FUSION_MZM_DOOR_CAPACITY ||
                   (doors->areas[area_index].count &&
                    !doors->areas[area_index].entries) ||
                   doors->connection_count >
                       FUSION_MZM_AREA_CONNECTION_CAPACITY)) ||
        (hatches && (!doors || area_index >= FUSION_MZM_ROOM_AREA_COUNT ||
                     hatches->locks[area_index].count >
                         FUSION_MZM_HATCH_LOCK_CAPACITY ||
                     (hatches->locks[area_index].count &&
                      !hatches->locks[area_index].entries))) ||
        ((entry == NULL) != (backgrounds == NULL)) ||
        (entry && (!hatches || entry->tileset >= FUSION_MZM_TILESET_COUNT ||
                   !room_effect_name(entry->effect))) ||
        ((spritesets == NULL) != (sprites == NULL)) ||
        (sprites && (!entry || spritesets->count == 0 ||
                     spritesets->count > FUSION_MZM_SPRITESET_CAPACITY ||
                     sprites->count > FUSION_MZM_ROOM_SPRITE_VARIANT_COUNT))) {
        set_error(error, error_size,
                  "invalid Zero Mission runtime-room core payload");
        return false;
    }
    size_t capacity = 256 + clipdata->cell_count * 96;
    size_t door_capacity = doors ? doors->areas[area_index].count : 0;
    if (door_capacity > (SIZE_MAX - capacity) / 256) {
        set_error(error, error_size,
                  "Zero Mission runtime-room payload exceeds the size bound");
        return false;
    }
    capacity += door_capacity * 256;
    if (hatches) {
        if (capacity > SIZE_MAX - 4096) {
            set_error(error, error_size,
                      "Zero Mission runtime-room payload exceeds the size bound");
            return false;
        }
        capacity += 4096;
    }
    if (backgrounds) {
        size_t background_cells = 0;
        for (size_t layer = 0; layer < 3; ++layer) {
            const FusionMzmRoomBackground *background =
                &backgrounds->layers[layer];
            if (background->status != FUSION_MZM_BACKGROUND_RLE) continue;
            if (!background->rle.cells || background->rle.width == 0 ||
                background->rle.height == 0 ||
                background->rle.cell_count !=
                    (size_t)background->rle.width * background->rle.height ||
                background_cells > SIZE_MAX - background->rle.cell_count) {
                set_error(error, error_size,
                          "invalid Zero Mission runtime-room background rows");
                return false;
            }
            background_cells += background->rle.cell_count;
        }
        if (capacity > SIZE_MAX - 1024 ||
            background_cells > (SIZE_MAX - capacity - 1024) / 8) {
            set_error(error, error_size,
                      "Zero Mission runtime-room payload exceeds the size bound");
            return false;
        }
        capacity += 1024 + background_cells * 8;
    }
    if (sprites) {
        /* At most 64 placements and 16 members per variant, each record
         * under 64 bytes. */
        enum { SPRITE_RECORD_CAPACITY = FUSION_MZM_ROOM_SPRITE_VARIANT_COUNT *
               (FUSION_MZM_ROOM_SPRITE_CAPACITY +
                FUSION_MZM_SPRITESET_MEMBER_CAPACITY + 1) * 64 };
        if (capacity > SIZE_MAX - SPRITE_RECORD_CAPACITY) {
            set_error(error, error_size,
                      "Zero Mission runtime-room payload exceeds the size bound");
            return false;
        }
        capacity += SPRITE_RECORD_CAPACITY;
    }
    uint8_t *data = malloc(capacity);
    if (!data) {
        set_error(error, error_size,
                  "cannot allocate Zero Mission runtime-room core payload");
        return false;
    }
    size_t width_pixels = (size_t)clipdata->width * 16;
    size_t height_pixels = (size_t)clipdata->height * 16;
    size_t used = 0, clipdata_count = 0, transition_count = 0;
    size_t movement_count = 0, door_count = 0, exit_count = 0;
    size_t location_count = 0, hatch_count = 0, hatch_lock_count = 0;
    size_t music_count = 0, background_layer_count = 0;
    size_t background_row_count = 0, text_background_count = 0;
    size_t sprite_count = 0, spriteset_member_count = 0;
    size_t sprite_variant_count = 0, variant_sprite_count = 0;
    size_t variant_member_count = 0;
    if (!append_format(
            data, capacity, &used,
            "MVROOM-NATIVE\t1\tmzm\t%s\t%zu\t%zu\t%zu\n",
            area, room, width_pixels, height_pixels)) {
        goto overflow;
    }
    for (size_t y = 0; y < clipdata->height; ++y) {
        for (size_t x = 0; x < clipdata->width; ++x) {
            uint16_t raw = clipdata->cells[y * clipdata->width + x];
            if (raw == 0) continue;
            uint8_t collision;
            uint16_t behavior;
            if (!resolve_clipdata(raw, tables, &collision, &behavior)) {
                free(data);
                set_error(error, error_size,
                          "Zero Mission Clipdata value is outside native tables");
                return false;
            }
            if (!append_format(data, capacity, &used,
                               "C\t%zu\t%zu\t%u\t%u\n",
                               x, y, (unsigned)raw,
                               (unsigned)collision)) {
                goto overflow;
            }
            ++clipdata_count;
            const char *transition = NULL;
            if (behavior == 3) transition = "door";
            else if (behavior == 4) transition = "up";
            else if (behavior == 5) transition = "down";
            if (transition) {
                if (!append_format(data, capacity, &used,
                                   "B\t%zu\t%zu\t%s\n",
                                   x, y, transition)) {
                    goto overflow;
                }
                ++transition_count;
            }
            uint16_t movement =
                behavior < FUSION_MZM_CLIPDATA_MOVEMENT_COUNT
                    ? tables->movement[behavior] : 0;
            if (movement != 0) {
                if (!append_format(data, capacity, &used,
                                   "M\t%zu\t%zu\t%u\n",
                                   x, y, (unsigned)movement)) {
                    goto overflow;
                }
                ++movement_count;
            }
        }
    }
    if (doors) {
        const FusionMzmDoorList *list = &doors->areas[area_index];
        char local_area[32];
        if (!lowercase_area_id(area, local_area)) goto overflow;
        for (size_t index = 0; index < list->count; ++index) {
            const FusionMzmDoor *door = &list->entries[index];
            if (door->source_room != room) continue;
            const char *kind = door_kind_name(door->type);
            if (!kind) goto overflow;
            const FusionMzmDoor *target = NULL;
            const char *target_area = local_area;
            char target_area_buffer[32];
            uint8_t base_kind = door->type & 0xF;
            if (base_kind != 0 && base_kind != 1 &&
                door->destination_door < list->count) {
                target = &list->entries[door->destination_door];
            } else if (base_kind == 1) {
                for (size_t connection = 0;
                     connection < doors->connection_count; ++connection) {
                    const FusionMzmAreaConnection *candidate =
                        &doors->connections[connection];
                    if (candidate->source_area != area_index ||
                        candidate->source_door != index) {
                        continue;
                    }
                    const FusionMzmRoomAreaSpec *target_spec =
                        fusion_mzm_room_area_spec(candidate->destination_area);
                    const FusionMzmDoorList *target_list =
                        &doors->areas[candidate->destination_area];
                    if (target_spec &&
                        door->destination_door < target_list->count &&
                        lowercase_area_id(target_spec->id,
                                          target_area_buffer)) {
                        target = &target_list->entries[door->destination_door];
                        target_area = target_area_buffer;
                    }
                    break;
                }
            }
            if (target) {
                if (!append_format(
                        data, capacity, &used,
                        "D\t%zu\t%s\t%u\t%u\t%u\t%u\t%s_%03u\t%u\t%u\t%d\t%d\n",
                        index, kind, (unsigned)door->x_start,
                        (unsigned)door->x_end, (unsigned)door->y_start,
                        (unsigned)door->y_end, target_area,
                        (unsigned)target->source_room,
                        (unsigned)target->x_start, (unsigned)target->y_end,
                        (int)target->x_exit, (int)target->y_exit)) {
                    goto overflow;
                }
            } else if (!append_format(
                           data, capacity, &used,
                           "D\t%zu\t%s\t%u\t%u\t%u\t%u\t-\t0\t0\t0\t0\n",
                           index, kind, (unsigned)door->x_start,
                           (unsigned)door->x_end, (unsigned)door->y_start,
                           (unsigned)door->y_end)) {
                goto overflow;
            }
            ++door_count;
        }
        for (size_t index = 0; index < list->count; ++index) {
            const FusionMzmDoor *door = &list->entries[index];
            if (door->source_room != room) continue;
            if (!append_format(data, capacity, &used,
                               "X\t%zu\t%d\t%d\n", index,
                               (int)door->x_exit, (int)door->y_exit)) {
                goto overflow;
            }
            ++exit_count;
        }
        for (size_t index = 0; index < list->count; ++index) {
            const FusionMzmDoor *door = &list->entries[index];
            if (door->source_room != room || !(door->type & 0x40)) continue;
            if (!append_format(data, capacity, &used,
                               "Y\t%zu\n", index)) {
                goto overflow;
            }
            ++location_count;
        }
    }
    if (hatches) {
        enum {
            CLIPDATA_TYPE_DOOR = 11,
            CLIP_BEHAVIOR_DOOR_START = 127,
            ROOM_HATCH_CAPACITY = 16
        };
        const FusionMzmDoorList *list = &doors->areas[area_index];
        size_t hatch_doors[ROOM_HATCH_CAPACITY];
        size_t hatch_door_count = 0;
        for (size_t index = 0; index < list->count; ++index) {
            const FusionMzmDoor *door = &list->entries[index];
            uint8_t kind = door->type & 0xF;
            if (door->source_room != room || (kind != 3 && kind != 4))
                continue;
            if (hatch_door_count == ROOM_HATCH_CAPACITY) {
                free(data);
                set_error(error, error_size,
                          "Zero Mission room exceeds the hatch capacity");
                return false;
            }
            hatch_doors[hatch_door_count++] = index;
        }
        for (size_t slot = 0; slot < hatch_door_count; ++slot) {
            size_t door_index = hatch_doors[slot];
            const FusionMzmDoor *door = &list->entries[door_index];
            size_t position = (size_t)clipdata->width * door->y_start +
                              door->x_start;
            if (position >= clipdata->cell_count) {
                free(data);
                set_error(error, error_size,
                          "Zero Mission hatch lies outside room Clipdata");
                return false;
            }
            uint16_t raw = position + 1 < clipdata->cell_count
                ? clipdata->cells[position + 1] : 0;
            uint8_t collision;
            uint16_t behavior;
            if (!resolve_clipdata(raw, tables, &collision, &behavior)) {
                free(data);
                set_error(error, error_size,
                          "Zero Mission hatch Clipdata is outside native tables");
                return false;
            }
            bool facing_right = collision == CLIPDATA_TYPE_DOOR;
            if (!facing_right) {
                if (position == 0 ||
                    !resolve_clipdata(clipdata->cells[position - 1], tables,
                                      &collision, &behavior)) {
                    free(data);
                    set_error(error, error_size,
                              "Zero Mission hatch has no bounded Clipdata side");
                    return false;
                }
            }
            size_t mapping = 0;
            if (behavior > CLIP_BEHAVIOR_DOOR_START &&
                behavior <= CLIP_BEHAVIOR_DOOR_START + 5) {
                mapping = behavior - CLIP_BEHAVIOR_DOOR_START;
            }
            uint8_t hatch_type = hatches->types[mapping];
            const char *name = hatch_type_name(hatch_type);
            if (!name) {
                free(data);
                set_error(error, error_size,
                          "invalid Zero Mission mapped hatch type");
                return false;
            }
            if (hatch_type < 2) continue;
            int x = (int)door->x_start + (facing_right ? 1 : -1);
            if (!append_format(
                    data, capacity, &used,
                    "H\t%zu\t%d\t%u\t%s\t%u\t%u\n",
                    door_index, x, (unsigned)door->y_start, name,
                    (unsigned)hatches->weakness[hatch_type],
                    (unsigned)hatches->health[hatch_type])) {
                goto overflow;
            }
            ++hatch_count;
        }
        for (size_t index = 0; sprites && index < sprites->count; ++index) {
            const FusionMzmRoomSpriteVariant *variant =
                &sprites->variants[index];
            if (variant->spriteset >= spritesets->count ||
                variant->placement_count > FUSION_MZM_ROOM_SPRITE_CAPACITY) {
                free(data);
                set_error(error, error_size,
                          "invalid Zero Mission room sprite variant");
                return false;
            }
            const FusionMzmSpriteset *set =
                &spritesets->sets[variant->spriteset];
            bool event_variant = variant->variant != 0;
            if (event_variant &&
                !append_format(data, capacity, &used, "F\t%u\t%u\t%u\n",
                               (unsigned)variant->variant,
                               (unsigned)variant->event,
                               (unsigned)variant->spriteset)) {
                goto overflow;
            }
            if (event_variant) ++sprite_variant_count;
            /* The placements: a slot above 16 picks spriteset member
             * (slot - 1) % 16 (skipped beyond the members), any other is
             * PrimarySprite slot - 1 in graphics slot 0. */
            for (size_t slot = 0; slot < variant->placement_count; ++slot) {
                const FusionMzmRoomSpritePlacement *placement =
                    &variant->placements[slot];
                unsigned raw = placement->slot & 0x7F;
                const char *name;
                char number[16];
                unsigned gfx_slot = 0;
                if (raw > FUSION_MZM_SPRITESET_ENTRY_TYPES) {
                    size_t choice = (raw - 1) % FUSION_MZM_SPRITESET_ENTRY_TYPES;
                    if (choice >= set->count) continue;
                    name = fusion_mzm_primary_sprite_name(
                        set->members[choice].sprite);
                    gfx_slot = set->members[choice].slot & 7;
                } else {
                    name = fusion_mzm_primary_sprite_name(raw - 1);
                    if (!name) {
                        snprintf(number, sizeof number, "%d", (int)raw - 1);
                        name = number;
                    }
                }
                if (!name) {
                    free(data);
                    set_error(error, error_size,
                              "Zero Mission spriteset member is outside PrimarySprite");
                    return false;
                }
                if (event_variant
                        ? !append_format(data, capacity, &used,
                                         "J\t%u\t%u\t%u\t%s\t%u\t%zu\n",
                                         (unsigned)variant->variant,
                                         (unsigned)placement->y,
                                         (unsigned)placement->x, name,
                                         gfx_slot, slot)
                        : !append_format(data, capacity, &used,
                                         "P\t%u\t%u\t%s\t%u\t%zu\n",
                                         (unsigned)placement->y,
                                         (unsigned)placement->x, name,
                                         gfx_slot, slot)) {
                    goto overflow;
                }
                if (event_variant) ++variant_sprite_count;
                else ++sprite_count;
            }
            for (size_t member = 0; member < set->count; ++member) {
                const char *name =
                    fusion_mzm_primary_sprite_name(set->members[member].sprite);
                if (!name) {
                    free(data);
                    set_error(error, error_size,
                              "Zero Mission spriteset member is outside PrimarySprite");
                    return false;
                }
                unsigned gfx_slot = set->members[member].slot & 7;
                if (event_variant
                        ? !append_format(data, capacity, &used,
                                         "N\t%u\t%s\t%u\n",
                                         (unsigned)variant->variant, name,
                                         gfx_slot)
                        : !append_format(data, capacity, &used,
                                         "S\t%u\t%s\t%u\n",
                                         (unsigned)variant->spriteset, name,
                                         gfx_slot)) {
                    goto overflow;
                }
                if (event_variant) ++variant_member_count;
                else ++spriteset_member_count;
            }
        }
        if (entry) {
            const char *effect = room_effect_name(entry->effect);
            if (!append_format(data, capacity, &used, "U\t%u\n",
                               (unsigned)entry->music_track) ||
                !append_format(
                    data, capacity, &used,
                    "L\t%u\t%u\t%u\t%u\t%u\t%u\t%s\nE\t%u\n",
                    (unsigned)entry->tileset,
                    (unsigned)entry->transparency,
                    (unsigned)entry->bg0_property,
                    (unsigned)entry->bg2_property,
                    (unsigned)entry->bg3_property,
                    (unsigned)entry->bg3_scrolling, effect,
                    (unsigned)entry->effect_y)) {
                goto overflow;
            }
            music_count = 1;
            for (size_t layer = 0; layer < 3; ++layer) {
                const FusionMzmRoomBackground *background =
                    &backgrounds->layers[layer];
                if (background->status != FUSION_MZM_BACKGROUND_RLE)
                    continue;
                if (!append_format(data, capacity, &used,
                                   "G\t%zu\t%u\t%u\n", layer,
                                   (unsigned)background->rle.width,
                                   (unsigned)background->rle.height)) {
                    goto overflow;
                }
                ++background_layer_count;
                for (size_t row = 0; row < background->rle.height; ++row) {
                    if (!append_format(data, capacity, &used,
                                       "R\t%zu\t%zu\t", layer, row)) {
                        goto overflow;
                    }
                    for (size_t column = 0;
                         column < background->rle.width; ++column) {
                        uint16_t value = background->rle.cells[
                            row * background->rle.width + column];
                        if (!append_format(data, capacity, &used, "%s%zu",
                                           column ? "," : "",
                                           runtime_sheet_block(value))) {
                            goto overflow;
                        }
                    }
                    if (!append_format(data, capacity, &used, "\n"))
                        goto overflow;
                    ++background_row_count;
                }
            }
            for (size_t layer = 0;
                 layer < FUSION_MZM_ROOM_BACKGROUND_COUNT; ++layer) {
                if (layer != 0 && layer != 3) continue;
                const FusionMzmRoomBackground *background =
                    &backgrounds->layers[layer];
                if (background->status != FUSION_MZM_BACKGROUND_TEXT)
                    continue;
                if (!append_format(
                        data, capacity, &used, "T\t%zu\t%u\t%u\t%u\n",
                        layer, (unsigned)background->text.size_flag,
                        (unsigned)background->text.width_tiles * 8,
                        (unsigned)background->text.height_tiles * 8)) {
                    goto overflow;
                }
                ++text_background_count;
            }
        }
        if (!append_format(data, capacity, &used, "V\t")) goto overflow;
        for (size_t index = 0; index < FUSION_MZM_HATCH_COUNT; ++index) {
            if (!append_format(data, capacity, &used, "%s%u",
                               index ? "," : "",
                               (unsigned)hatches->tilemap_values[index])) {
                goto overflow;
            }
        }
        if (!append_format(data, capacity, &used, "\nA\t")) goto overflow;
        for (size_t index = 0;
             index < FUSION_MZM_HATCH_ANIMATION_COUNT; ++index) {
            if (!append_format(data, capacity, &used, "%s%u",
                               index ? "," : "",
                               (unsigned)hatches->durations[index])) {
                goto overflow;
            }
        }
        if (!append_format(data, capacity, &used, "\nW\t")) goto overflow;
        for (size_t index = 0; index < FUSION_MZM_HATCH_COUNT; ++index) {
            if (!append_format(data, capacity, &used, "%s%u:%u",
                               index ? "," : "",
                               (unsigned)hatches->weakness[index],
                               (unsigned)hatches->health[index])) {
                goto overflow;
            }
        }
        if (!append_format(data, capacity, &used, "\nK\t")) goto overflow;
        for (size_t index = 0;
             index < FUSION_MZM_CLIPDATA_TILEMAP_COUNT; ++index) {
            if (!append_format(data, capacity, &used, "%s%u",
                               index ? "," : "",
                               (unsigned)tables->tilemap_collision[index])) {
                goto overflow;
            }
        }
        if (!append_format(data, capacity, &used, "\n")) goto overflow;
        const FusionMzmHatchLockList *locks = &hatches->locks[area_index];
        for (size_t slot = 0; slot < ROOM_HATCH_CAPACITY; ++slot) {
            for (size_t lock = 0; lock < locks->count; ++lock) {
                const FusionMzmHatchLockEvent *event = &locks->entries[lock];
                if (event->room != room ||
                    !(event->hatch_mask & (uint16_t)(1u << slot))) {
                    continue;
                }
                if (slot >= hatch_door_count) {
                    free(data);
                    set_error(error, error_size,
                              "Zero Mission hatch lock references a missing slot");
                    return false;
                }
                if (!append_format(data, capacity, &used,
                                   "Q\t%zu\t%u\t%u\t%u\n",
                                   hatch_doors[slot], (unsigned)event->event,
                                   (unsigned)(event->type & 1),
                                   (unsigned)(event->type >= 2))) {
                    goto overflow;
                }
                ++hatch_lock_count;
            }
        }
    }
    if (5 > capacity - used) goto overflow;
    memcpy(data + used, "END\n", 4);
    used += 4;
    data[used] = '\0';
    *payload = (FusionMzmRuntimeRoomPayload){
        data, used, clipdata_count, transition_count, movement_count,
        door_count, exit_count, location_count, hatch_count, hatch_lock_count,
        music_count, background_layer_count, background_row_count,
        text_background_count, sprite_count, spriteset_member_count,
        sprite_variant_count, variant_sprite_count, variant_member_count
    };
    if (error && error_size > 0) error[0] = '\0';
    return true;

overflow:
    free(data);
    set_error(error, error_size,
              "Zero Mission runtime-room core payload exceeds the size bound");
    return false;
}

bool fusion_mzm_serialize_runtime_room_core(
    const char *area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size) {
    return serialize_runtime_room(area, SIZE_MAX, room, clipdata, tables, NULL,
                                  NULL, NULL, NULL, NULL, NULL, payload, error,
                                  error_size);
}

bool fusion_mzm_serialize_runtime_room_doors(
    size_t area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    const FusionMzmDoorDirectory *doors,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size) {
    const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
    if (!spec) {
        if (payload) *payload = (FusionMzmRuntimeRoomPayload){0};
        set_error(error, error_size,
                  "invalid Zero Mission runtime-room door area");
        return false;
    }
    return serialize_runtime_room(spec->id, area, room, clipdata, tables,
                                  doors, NULL, NULL, NULL, NULL, NULL, payload,
                                  error, error_size);
}

bool fusion_mzm_serialize_runtime_room_hatches(
    size_t area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    const FusionMzmDoorDirectory *doors,
    const FusionMzmHatchTables *hatches,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size) {
    const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
    if (!spec) {
        if (payload) *payload = (FusionMzmRuntimeRoomPayload){0};
        set_error(error, error_size,
                  "invalid Zero Mission runtime-room hatch area");
        return false;
    }
    return serialize_runtime_room(spec->id, area, room, clipdata, tables,
                                  doors, hatches, NULL, NULL, NULL, NULL,
                                  payload, error, error_size);
}

bool fusion_mzm_serialize_runtime_room_metadata(
    size_t area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    const FusionMzmDoorDirectory *doors,
    const FusionMzmHatchTables *hatches,
    const FusionMzmRoomEntry *entry,
    const FusionMzmRoomBackgrounds *backgrounds,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size) {
    const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
    if (!spec) {
        if (payload) *payload = (FusionMzmRuntimeRoomPayload){0};
        set_error(error, error_size,
                  "invalid Zero Mission runtime-room metadata area");
        return false;
    }
    return serialize_runtime_room(spec->id, area, room, clipdata, tables,
                                  doors, hatches, entry, backgrounds, NULL,
                                  NULL, payload, error, error_size);
}

bool fusion_mzm_serialize_runtime_room_sprites(
    size_t area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    const FusionMzmDoorDirectory *doors,
    const FusionMzmHatchTables *hatches,
    const FusionMzmRoomEntry *entry,
    const FusionMzmRoomBackgrounds *backgrounds,
    const FusionMzmSpritesetTable *spritesets,
    const FusionMzmRoomSprites *sprites,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size) {
    const FusionMzmRoomAreaSpec *spec = fusion_mzm_room_area_spec(area);
    if (!spec || !spritesets || !sprites) {
        if (payload) *payload = (FusionMzmRuntimeRoomPayload){0};
        set_error(error, error_size,
                  "invalid Zero Mission runtime-room sprite input");
        return false;
    }
    return serialize_runtime_room(spec->id, area, room, clipdata, tables,
                                  doors, hatches, entry, backgrounds,
                                  spritesets, sprites, payload, error,
                                  error_size);
}

static bool read_count(const uint8_t *data, size_t size, size_t *position,
                       uint8_t count_bytes, uint16_t *count) {
    if (*position > size || count_bytes > size - *position) return false;
    if (count_bytes == 1) {
        *count = data[(*position)++];
    } else {
        *count = (uint16_t)data[*position] << 8 | data[*position + 1];
        *position += 2;
    }
    return true;
}

static bool decode_pass(const uint8_t *data, size_t size, size_t *position,
                        uint8_t *output, size_t total,
                        char *error, size_t error_size) {
    if (*position >= size) {
        set_error(error, error_size, "missing Zero Mission room RLE pass");
        return false;
    }
    uint8_t count_bytes = data[(*position)++];
    if (count_bytes != 1 && count_bytes != 2) {
        set_error(error, error_size, "invalid Zero Mission room RLE pass width");
        return false;
    }
    uint16_t encoded_bit = count_bytes == 1 ? 0x80 : 0x8000;
    size_t written = 0;
    for (;;) {
        uint16_t raw_count;
        if (!read_count(data, size, position, count_bytes, &raw_count)) {
            set_error(error, error_size, "truncated Zero Mission room RLE count");
            return false;
        }
        if (raw_count == 0) break;
        bool repeated = (raw_count & encoded_bit) != 0;
        size_t count = raw_count & (encoded_bit - 1);
        if (count == 0 || count > total - written) {
            set_error(error, error_size, "Zero Mission room RLE pass overflows its map");
            return false;
        }
        if (repeated) {
            if (*position >= size) {
                set_error(error, error_size,
                          "truncated Zero Mission room RLE repeat");
                return false;
            }
            memset(output + written, data[(*position)++], count);
        } else {
            if (*position > size || count > size - *position) {
                set_error(error, error_size,
                          "truncated Zero Mission room RLE literals");
                return false;
            }
            memcpy(output + written, data + *position, count);
            *position += count;
        }
        written += count;
    }
    if (written != total) {
        set_error(error, error_size,
                  "Zero Mission room RLE pass does not fill its map");
        return false;
    }
    return true;
}

void fusion_mzm_free_room_rle(FusionMzmRoomRleMap *map) {
    if (!map) return;
    free(map->cells);
    *map = (FusionMzmRoomRleMap){0};
}

bool fusion_mzm_read_room_rle(const FusionRom *rom, uint32_t pointer,
                              FusionMzmRoomRleMap *map,
                              char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID || !map) {
        set_error(error, error_size, "invalid Zero Mission room RLE request");
        return false;
    }
    *map = (FusionMzmRoomRleMap){0};
    size_t offset;
    if (!pointer_offset(rom, pointer, 6, &offset)) {
        set_error(error, error_size, "Zero Mission room RLE points outside the ROM");
        return false;
    }
    const uint8_t *data = rom->data + offset;
    size_t size = rom->size - offset;
    size_t total = (size_t)data[0] * data[1];
    if (data[0] == 0 || data[1] == 0 ||
        total > FUSION_MZM_ROOM_RLE_CELL_CAPACITY) {
        set_error(error, error_size,
                  "invalid Zero Mission room RLE dimensions");
        return false;
    }
    uint8_t *halves = malloc(total * 2);
    uint16_t *cells = malloc(total * sizeof *cells);
    if (!halves || !cells) {
        free(halves);
        free(cells);
        set_error(error, error_size, "cannot allocate Zero Mission room RLE map");
        return false;
    }
    size_t position = 2;
    if (!decode_pass(data, size, &position, halves, total, error, error_size) ||
        !decode_pass(data, size, &position, halves + total, total,
                     error, error_size)) {
        free(halves);
        free(cells);
        return false;
    }
    for (size_t index = 0; index < total; ++index)
        cells[index] = (uint16_t)halves[index] |
                       (uint16_t)halves[total + index] << 8;
    free(halves);
    *map = (FusionMzmRoomRleMap){
        pointer, data[0], data[1], cells, total, position
    };
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_text_tilemap(FusionMzmTextTilemap *map) {
    if (!map) return;
    free(map->cells);
    *map = (FusionMzmTextTilemap){0};
}

bool fusion_mzm_read_text_tilemap(const FusionRom *rom, uint32_t pointer,
                                  FusionMzmTextTilemap *map,
                                  char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID || !map) {
        set_error(error, error_size,
                  "invalid Zero Mission text-tilemap request");
        return false;
    }
    *map = (FusionMzmTextTilemap){0};
    size_t offset;
    if (!pointer_offset(rom, pointer, 8, &offset)) {
        set_error(error, error_size,
                  "Zero Mission text tilemap points outside the ROM");
        return false;
    }
    uint8_t size_flag = rom->data[offset];
    uint8_t width_tiles = size_flag == 0 || size_flag == 2 ? 32 : 64;
    uint8_t height_tiles = size_flag == 2 ? 64 : 32;
    size_t cell_count = (size_t)width_tiles * height_tiles;
    uint32_t expanded_size = fusion_gba_uncompressed_size(
        rom->data + offset + 4, rom->size - offset - 4);
    if (cell_count > FUSION_MZM_TEXT_TILE_CELL_CAPACITY ||
        expanded_size != cell_count * sizeof(uint16_t)) {
        set_error(error, error_size,
                  "Zero Mission text tilemap has an invalid expanded size");
        return false;
    }
    uint8_t *expanded = malloc(expanded_size);
    uint16_t *cells = malloc(cell_count * sizeof *cells);
    if (!expanded || !cells) {
        free(expanded);
        free(cells);
        set_error(error, error_size,
                  "cannot allocate Zero Mission text tilemap");
        return false;
    }
    size_t written = 0;
    if (!fusion_gba_lz77_uncompress(rom->data + offset + 4,
                                    rom->size - offset - 4,
                                    expanded, expanded_size, &written) ||
        written != expanded_size) {
        free(expanded);
        free(cells);
        set_error(error, error_size,
                  "Zero Mission text tilemap has invalid LZ77 data");
        return false;
    }
    for (size_t index = 0; index < cell_count; ++index)
        cells[index] = (uint16_t)expanded[index * 2] |
                       (uint16_t)expanded[index * 2 + 1] << 8;
    free(expanded);
    *map = (FusionMzmTextTilemap){
        pointer, size_flag, width_tiles, height_tiles, cells, cell_count
    };
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

void fusion_mzm_free_room_backgrounds(FusionMzmRoomBackgrounds *backgrounds) {
    if (!backgrounds) return;
    for (size_t layer = 0; layer < FUSION_MZM_ROOM_BACKGROUND_COUNT; ++layer) {
        fusion_mzm_free_room_rle(&backgrounds->layers[layer].rle);
        fusion_mzm_free_text_tilemap(&backgrounds->layers[layer].text);
    }
    *backgrounds = (FusionMzmRoomBackgrounds){0};
}

bool fusion_mzm_read_room_backgrounds(const FusionRom *rom,
                                      const FusionMzmRoomEntry *entry,
                                      FusionMzmRoomBackgrounds *backgrounds,
                                      char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        !entry || !backgrounds) {
        set_error(error, error_size,
                  "invalid Zero Mission room-background request");
        return false;
    }
    *backgrounds = (FusionMzmRoomBackgrounds){0};
    const uint8_t properties[FUSION_MZM_ROOM_BACKGROUND_COUNT] = {
        entry->bg0_property, entry->bg1_property,
        entry->bg2_property, entry->bg3_property
    };
    const uint32_t pointers[FUSION_MZM_ROOM_BACKGROUND_COUNT] = {
        entry->bg0_pointer, entry->bg1_pointer,
        entry->bg2_pointer, entry->bg3_pointer
    };
    for (size_t layer = 0; layer < FUSION_MZM_ROOM_BACKGROUND_COUNT; ++layer) {
        FusionMzmRoomBackground *background = &backgrounds->layers[layer];
        background->layer = (uint8_t)layer;
        background->property = properties[layer];
        background->source_pointer = pointers[layer];
        if (properties[layer] & FUSION_MZM_BG_PROPERTY_RLE_COMPRESSED) {
            background->status = FUSION_MZM_BACKGROUND_RLE;
            if (!fusion_mzm_read_room_rle(rom, pointers[layer],
                                          &background->rle,
                                          error, error_size)) {
                fusion_mzm_free_room_backgrounds(backgrounds);
                return false;
            }
        } else if (properties[layer] & FUSION_MZM_BG_PROPERTY_LZ77_COMPRESSED) {
            background->status = FUSION_MZM_BACKGROUND_TEXT;
            if (!fusion_mzm_read_text_tilemap(rom, pointers[layer],
                                              &background->text,
                                              error, error_size)) {
                fusion_mzm_free_room_backgrounds(backgrounds);
                return false;
            }
        } else {
            background->status = properties[layer] == 0
                ? FUSION_MZM_BACKGROUND_NONE
                : FUSION_MZM_BACKGROUND_UNSUPPORTED;
        }
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}
