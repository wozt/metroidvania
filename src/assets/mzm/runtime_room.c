/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/mzm.h"

#include "fusion/assets/gba.h"

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
