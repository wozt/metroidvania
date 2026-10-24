/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded Metroid: Zero Mission asset metadata readers. */
#ifndef FUSION_ASSETS_MZM_H
#define FUSION_ASSETS_MZM_H

#include "fusion/assets/rom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    FUSION_MZM_ROM_SYMBOL_CAPACITY = 65536,
    FUSION_MZM_ROOM_AREA_COUNT = 11,
    FUSION_MZM_ROOM_ENTRY_SIZE = 60,
    FUSION_MZM_ROOM_CAPACITY = 512,
    FUSION_MZM_ROOM_RLE_CELL_CAPACITY = 6144,
    FUSION_MZM_ROOM_BACKGROUND_COUNT = 3,
    FUSION_MZM_TEXT_TILE_CELL_CAPACITY = 4096,
    FUSION_MZM_BG_PROPERTY_RLE_COMPRESSED = 0x10,
    FUSION_MZM_BG_PROPERTY_LZ77_COMPRESSED = 0x40
};

typedef struct {
    const char *name;
    uint32_t address;
    uint32_t size;
    size_t elf_order;
} FusionMzmRomSymbol;

typedef struct {
    FusionMzmRomSymbol *entries;
    size_t count;
    size_t duplicate_name_count;
} FusionMzmRomSymbolTable;

typedef struct {
    const char *id;
    const char *symbol;
} FusionMzmRoomAreaSpec;

typedef struct {
    uint32_t table_pointer;
    size_t room_count;
} FusionMzmRoomArea;

typedef struct {
    FusionMzmRoomArea areas[FUSION_MZM_ROOM_AREA_COUNT];
    size_t total_rooms;
} FusionMzmRoomDirectory;

typedef struct {
    uint32_t entry_pointer;
    uint8_t tileset;
    uint8_t bg0_property;
    uint8_t bg1_property;
    uint8_t bg2_property;
    uint8_t bg3_property;
    uint32_t bg0_pointer;
    uint32_t bg1_pointer;
    uint32_t bg2_pointer;
    uint32_t clip_pointer;
    uint32_t bg3_pointer;
    uint8_t bg3_scrolling;
    uint8_t transparency;
    uint32_t default_sprite_pointer;
    uint8_t default_spriteset;
    uint8_t first_spriteset_event;
    uint32_t first_sprite_pointer;
    uint8_t first_spriteset;
    uint8_t second_spriteset_event;
    uint32_t second_sprite_pointer;
    uint8_t second_spriteset;
    uint8_t map_x;
    uint8_t map_y;
    uint8_t effect;
    uint8_t effect_y;
    uint16_t music_track;
} FusionMzmRoomEntry;

typedef struct {
    uint32_t source_pointer;
    uint8_t width;
    uint8_t height;
    uint16_t *cells;
    size_t cell_count;
    size_t encoded_size;
} FusionMzmRoomRleMap;

typedef struct {
    uint32_t source_pointer;
    uint8_t size_flag;
    uint8_t width_tiles;
    uint8_t height_tiles;
    uint16_t *cells;
    size_t cell_count;
} FusionMzmTextTilemap;

typedef enum {
    FUSION_MZM_BACKGROUND_NONE,
    FUSION_MZM_BACKGROUND_RLE,
    FUSION_MZM_BACKGROUND_TEXT,
    FUSION_MZM_BACKGROUND_UNSUPPORTED
} FusionMzmBackgroundStatus;

typedef struct {
    uint8_t layer;
    uint8_t property;
    uint32_t source_pointer;
    FusionMzmBackgroundStatus status;
    FusionMzmRoomRleMap rle;
    FusionMzmTextTilemap text;
} FusionMzmRoomBackground;

typedef struct {
    FusionMzmRoomBackground layers[FUSION_MZM_ROOM_BACKGROUND_COUNT];
} FusionMzmRoomBackgrounds;

/* Symbol names point into elf_data and remain valid only while that buffer is
 * alive. The returned entry array is released with fusion_mzm_free_symbols. */
bool fusion_mzm_read_rom_symbols(const FusionRom *rom,
                                 const uint8_t *elf_data, size_t elf_size,
                                 FusionMzmRomSymbolTable *table,
                                 char *error, size_t error_size);
void fusion_mzm_free_symbols(FusionMzmRomSymbolTable *table);

/* Writes the MZMROM01 bank and metroidvania-mzm-rom-v1 manifest through
 * sibling .tmp files. Parent directories must already exist. */
bool fusion_mzm_write_rom_bank(const FusionRom *rom,
                               const FusionMzmRomSymbolTable *table,
                               const char *bank_path, const char *manifest_path,
                               char *error, size_t error_size);

const FusionMzmRoomAreaSpec *fusion_mzm_room_area_spec(size_t index);
bool fusion_mzm_read_room_directory(const FusionRom *rom,
                                    const FusionMzmRomSymbolTable *symbols,
                                    FusionMzmRoomDirectory *directory,
                                    char *error, size_t error_size);
bool fusion_mzm_read_room_entry(const FusionRom *rom,
                                const FusionMzmRoomDirectory *directory,
                                size_t area, size_t room,
                                FusionMzmRoomEntry *entry,
                                char *error, size_t error_size);
bool fusion_mzm_read_room_rle(const FusionRom *rom, uint32_t pointer,
                              FusionMzmRoomRleMap *map,
                              char *error, size_t error_size);
void fusion_mzm_free_room_rle(FusionMzmRoomRleMap *map);
bool fusion_mzm_read_text_tilemap(const FusionRom *rom, uint32_t pointer,
                                  FusionMzmTextTilemap *map,
                                  char *error, size_t error_size);
void fusion_mzm_free_text_tilemap(FusionMzmTextTilemap *map);
bool fusion_mzm_read_room_backgrounds(const FusionRom *rom,
                                      const FusionMzmRoomEntry *entry,
                                      FusionMzmRoomBackgrounds *backgrounds,
                                      char *error, size_t error_size);
void fusion_mzm_free_room_backgrounds(FusionMzmRoomBackgrounds *backgrounds);

#endif /* FUSION_ASSETS_MZM_H */
