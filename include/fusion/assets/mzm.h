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
    FUSION_MZM_CLIPDATA_COUNT = 176,
    FUSION_MZM_CLIPDATA_TILEMAP_COUNT = 208,
    FUSION_MZM_CLIPDATA_TILEMAP_FLAG = 1 << 10,
    FUSION_MZM_CLIPDATA_MOVEMENT_COUNT = 16,
    FUSION_MZM_DOOR_RECORD_SIZE = 12,
    FUSION_MZM_DOOR_CAPACITY = 256,
    FUSION_MZM_AREA_CONNECTION_CAPACITY = 32,
    FUSION_MZM_HATCH_COUNT = 8,
    FUSION_MZM_HATCH_ANIMATION_COUNT = 6,
    FUSION_MZM_HATCH_LOCK_RECORD_SIZE = 8,
    FUSION_MZM_HATCH_LOCK_CAPACITY = 32,
    FUSION_MZM_ROOM_BACKGROUND_COUNT = 4,
    FUSION_MZM_PRIMARY_SPRITE_COUNT = 207,
    FUSION_MZM_SPRITESET_CAPACITY = 128,
    FUSION_MZM_SPRITESET_MEMBER_CAPACITY = 16,
    FUSION_MZM_SPRITESET_ENTRY_TYPES = 16,
    FUSION_MZM_ROOM_SPRITE_CAPACITY = 64,
    FUSION_MZM_ROOM_SPRITE_VARIANT_COUNT = 3,
    FUSION_MZM_EXPORT_SKIP_CAPACITY = 64,
    FUSION_MZM_TEXT_TILE_CELL_CAPACITY = 4096,
    FUSION_MZM_TILESET_COUNT = 79,
    FUSION_MZM_TILESET_ENTRY_SIZE = 20,
    FUSION_MZM_TILESET_METATILE_COUNT = 1024,
    FUSION_MZM_COMMON_METATILE_COUNT = 208,
    FUSION_MZM_SHEET_COLUMNS = 32,
    FUSION_MZM_SHEET_BLOCKS = FUSION_MZM_TILESET_METATILE_COUNT +
                              FUSION_MZM_COMMON_METATILE_COUNT,
    FUSION_MZM_METATILE_CELLS = 4,
    FUSION_MZM_METATILE_WORD_COUNT =
        (FUSION_MZM_TILESET_METATILE_COUNT +
         FUSION_MZM_COMMON_METATILE_COUNT) * FUSION_MZM_METATILE_CELLS,
    FUSION_MZM_RENDER_PIXEL_CAPACITY = 4096 * 4096,
    FUSION_MZM_VRAM_SIZE = 0x10000,
    FUSION_MZM_PALETTE_SIZE = 0x200,
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

typedef struct {
    uint8_t index;
    uint32_t graphics_pointer;
    uint32_t palette_pointer;
    uint32_t background_graphics_pointer;
    uint32_t tilemap_pointer;
    uint8_t animated_tileset;
    uint8_t animated_palette;
} FusionMzmTilesetEntry;

typedef struct {
    FusionMzmTilesetEntry entry;
    uint8_t vram[FUSION_MZM_VRAM_SIZE];
    uint8_t palette[FUSION_MZM_PALETTE_SIZE];
    size_t common_graphics_size;
    size_t graphics_size;
    size_t background_graphics_size;
} FusionMzmVideoMemory;

typedef struct {
    FusionMzmTilesetEntry entry;
    uint16_t cells[FUSION_MZM_METATILE_WORD_COUNT];
    size_t tileset_word_count;
    size_t common_word_count;
} FusionMzmMetatileTable;

typedef struct {
    size_t width_pixels;
    size_t height_pixels;
    uint8_t *rgba;
    size_t pixel_count;
    size_t unresolved_count;
} FusionMzmRenderedBackground;

typedef struct {
    size_t width_pixels;
    size_t height_pixels;
    uint8_t *rgb;
    uint8_t *visibility;
    size_t pixel_count;
    size_t bg1_visible_count;
    size_t bg2_visible_count;
    size_t unresolved_count;
} FusionMzmCompositeBackground;

typedef struct {
    uint8_t *bitmap;
    size_t bitmap_size;
    uint8_t *visibility;
    size_t visibility_size;
} FusionMzmSerializedBackground;

typedef struct {
    uint8_t *data;
    size_t size;
} FusionMzmBitmap;

typedef struct {
    uint8_t *data;
    size_t size;
} FusionMzmTextFile;

typedef struct {
    uint8_t collision[FUSION_MZM_CLIPDATA_COUNT];
    uint16_t behavior[FUSION_MZM_CLIPDATA_COUNT];
    uint8_t tilemap_collision[FUSION_MZM_CLIPDATA_TILEMAP_COUNT];
    uint16_t tilemap_behavior[FUSION_MZM_CLIPDATA_TILEMAP_COUNT];
    uint16_t movement[FUSION_MZM_CLIPDATA_MOVEMENT_COUNT];
} FusionMzmClipdataTables;

typedef struct {
    uint8_t type;
    uint8_t source_room;
    uint8_t x_start;
    uint8_t x_end;
    uint8_t y_start;
    uint8_t y_end;
    uint8_t destination_door;
    int8_t x_exit;
    int8_t y_exit;
} FusionMzmDoor;

typedef struct {
    FusionMzmDoor *entries;
    size_t count;
} FusionMzmDoorList;

typedef struct {
    uint8_t source_area;
    uint8_t source_door;
    uint8_t destination_area;
} FusionMzmAreaConnection;

typedef struct {
    FusionMzmDoorList areas[FUSION_MZM_ROOM_AREA_COUNT];
    FusionMzmAreaConnection connections[FUSION_MZM_AREA_CONNECTION_CAPACITY];
    size_t connection_count;
} FusionMzmDoorDirectory;

typedef struct {
    uint8_t room;
    uint8_t event;
    uint8_t type;
    uint16_t hatch_mask;
} FusionMzmHatchLockEvent;

typedef struct {
    FusionMzmHatchLockEvent *entries;
    size_t count;
} FusionMzmHatchLockList;

typedef struct {
    uint8_t types[FUSION_MZM_HATCH_COUNT];
    uint8_t durations[FUSION_MZM_HATCH_ANIMATION_COUNT];
    uint16_t tilemap_values[FUSION_MZM_HATCH_COUNT];
    uint16_t weakness[FUSION_MZM_HATCH_COUNT];
    uint16_t health[FUSION_MZM_HATCH_COUNT];
    FusionMzmHatchLockList locks[FUSION_MZM_ROOM_AREA_COUNT];
} FusionMzmHatchTables;

typedef struct {
    uint8_t sprite;
    uint8_t slot;
} FusionMzmSpritesetMember;

typedef struct {
    FusionMzmSpritesetMember members[FUSION_MZM_SPRITESET_MEMBER_CAPACITY];
    size_t count;
} FusionMzmSpriteset;

/* sSpritesetPointers: each spriteset's (PrimarySprite, graphics slot)
 * pairs up to SPRITESET_EMPTY or the end of its array symbol. */
typedef struct {
    FusionMzmSpriteset sets[FUSION_MZM_SPRITESET_CAPACITY];
    size_t count;
} FusionMzmSpritesetTable;

typedef struct {
    uint8_t y;
    uint8_t x;
    uint8_t slot;
} FusionMzmRoomSpritePlacement;

typedef struct {
    uint8_t variant;
    uint8_t event;
    uint8_t spriteset;
    FusionMzmRoomSpritePlacement placements[FUSION_MZM_ROOM_SPRITE_CAPACITY];
    size_t placement_count;
} FusionMzmRoomSpriteVariant;

/* RoomLoadEntry's sprite choices: variant 0 (the default) then the event
 * variants 1 and 2 whose event is set. A room with a UCHAR_MAX spriteset
 * (the test areas) has none. */
typedef struct {
    FusionMzmRoomSpriteVariant variants[FUSION_MZM_ROOM_SPRITE_VARIANT_COUNT];
    size_t count;
} FusionMzmRoomSprites;

typedef struct {
    uint8_t *data;
    size_t size;
    size_t clipdata_count;
    size_t transition_count;
    size_t movement_count;
    size_t door_count;
    size_t exit_count;
    size_t location_count;
    size_t hatch_count;
    size_t hatch_lock_count;
    size_t music_count;
    size_t background_layer_count;
    size_t background_row_count;
    size_t text_background_count;
    size_t sprite_count;
    size_t spriteset_member_count;
    size_t sprite_variant_count;
    size_t variant_sprite_count;
    size_t variant_member_count;
} FusionMzmRuntimeRoomPayload;

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
bool fusion_mzm_read_tileset_entry(const FusionRom *rom,
                                   const FusionMzmRomSymbolTable *symbols,
                                   size_t index, FusionMzmTilesetEntry *entry,
                                   char *error, size_t error_size);
bool fusion_mzm_load_tileset_video_memory(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    size_t index, FusionMzmVideoMemory *memory,
    char *error, size_t error_size);
bool fusion_mzm_read_tileset_metatiles(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    size_t index, FusionMzmMetatileTable *table,
    char *error, size_t error_size);
bool fusion_mzm_render_rle_background(
    const FusionMzmRoomRleMap *map, const FusionMzmMetatileTable *metatiles,
    const FusionMzmVideoMemory *memory,
    FusionMzmRenderedBackground *rendered,
    char *error, size_t error_size);
bool fusion_mzm_render_text_background(
    const FusionMzmTextTilemap *map, const FusionMzmVideoMemory *memory,
    FusionMzmRenderedBackground *rendered,
    char *error, size_t error_size);
/* The runtime metatile sheet: every block value of a tileset (the common
 * blocks after the 1024 tileset blocks), 32 blocks a row. */
bool fusion_mzm_render_metatile_sheet(
    const FusionMzmMetatileTable *metatiles,
    const FusionMzmVideoMemory *memory,
    FusionMzmRenderedBackground *rendered,
    char *error, size_t error_size);
void fusion_mzm_free_rendered_background(
    FusionMzmRenderedBackground *rendered);
/* A top-down 32-bit BITMAPV4HEADER BMP with an alpha mask, as
 * scripts/mzm_bg_layers.py::bmp32 writes the sheets and text layers. */
bool fusion_mzm_serialize_bmp32(
    const FusionMzmRenderedBackground *rendered, FusionMzmBitmap *bitmap,
    char *error, size_t error_size);
void fusion_mzm_free_bitmap(FusionMzmBitmap *bitmap);
bool fusion_mzm_compose_bg12(
    const FusionMzmRenderedBackground *bg1,
    const FusionMzmRenderedBackground *bg2,
    FusionMzmCompositeBackground *composite,
    char *error, size_t error_size);
bool fusion_mzm_select_bg12(
    const FusionMzmRenderedBackground *bg1,
    const FusionMzmRenderedBackground *bg2,
    FusionMzmCompositeBackground *composite,
    char *error, size_t error_size);
/* The runtime export's background.bmp image
 * (scripts/mzm_runtime_room.py::produce): the older diagnostic renderer of
 * scripts/mzm_room_render.py. Only the tileset's own metatiles are drawn
 * (common blocks stay black), tiles come from the decompressed tileset
 * graphics at the character base inferred from BG1 (graphic_base: 192 or
 * 704), BG rows 3 to 15 use source palette rows 1 to 13 and color 0 is drawn
 * but not visible; BG1 goes over BG2 when BG2's property is exactly
 * BG_PROP_RLE_COMPRESSED and its layer has the same size. *no_base is set (with false) when no BG1 tile reaches the graphics,
 * which the export reports as a skipped room. */
bool fusion_mzm_render_export_background(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    const FusionMzmRoomEntry *entry,
    const FusionMzmRoomBackgrounds *backgrounds,
    FusionMzmCompositeBackground *composite, bool *no_base,
    char *error, size_t error_size);
void fusion_mzm_free_composite_background(
    FusionMzmCompositeBackground *composite);
bool fusion_mzm_serialize_background(
    const FusionMzmCompositeBackground *composite,
    FusionMzmSerializedBackground *serialized,
    char *error, size_t error_size);
void fusion_mzm_free_serialized_background(
    FusionMzmSerializedBackground *serialized);
bool fusion_mzm_read_clipdata_tables(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmClipdataTables *tables,
    char *error, size_t error_size);
bool fusion_mzm_read_door_directory(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmDoorDirectory *directory,
    char *error, size_t error_size);
void fusion_mzm_free_door_directory(FusionMzmDoorDirectory *directory);
bool fusion_mzm_read_hatch_tables(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmHatchTables *tables,
    char *error, size_t error_size);
void fusion_mzm_free_hatch_tables(FusionMzmHatchTables *tables);
bool fusion_mzm_serialize_runtime_room_core(
    const char *area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size);
bool fusion_mzm_serialize_runtime_room_doors(
    size_t area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    const FusionMzmDoorDirectory *doors,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size);
bool fusion_mzm_serialize_runtime_room_hatches(
    size_t area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    const FusionMzmDoorDirectory *doors,
    const FusionMzmHatchTables *hatches,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size);
bool fusion_mzm_serialize_runtime_room_metadata(
    size_t area, size_t room, const FusionMzmRoomRleMap *clipdata,
    const FusionMzmClipdataTables *tables,
    const FusionMzmDoorDirectory *doors,
    const FusionMzmHatchTables *hatches,
    const FusionMzmRoomEntry *entry,
    const FusionMzmRoomBackgrounds *backgrounds,
    FusionMzmRuntimeRoomPayload *payload,
    char *error, size_t error_size);
/* The PrimarySprite enumerator name without its PSPRITE_ prefix, NULL
 * outside the enum. */
const char *fusion_mzm_primary_sprite_name(size_t id);
bool fusion_mzm_read_spritesets(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmSpritesetTable *table,
    char *error, size_t error_size);
bool fusion_mzm_read_room_sprites(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    const FusionMzmSpritesetTable *spritesets,
    const FusionMzmRoomEntry *entry, FusionMzmRoomSprites *sprites,
    char *error, size_t error_size);
/* The metadata payload with the sprite records P, S, F, J and N between
 * the hatches and the music track. */
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
    char *error, size_t error_size);
/* blocks.tsv (metroidvania-mzm-blocks-v1): the block, tank and bomb-chain
 * tables of src/data/block_data.c read from bounded ELF objects, then each
 * raw Clipdata value's collision type and behavior, as
 * scripts/mzm_c_tables.py::block_table_lines writes them. */
bool fusion_mzm_serialize_block_tables(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    const FusionMzmClipdataTables *tables, FusionMzmTextFile *file,
    char *error, size_t error_size);
/* power_bomb.tsv (metroidvania-mzm-power-bomb-v1): each
 * sHaze_PowerBomb_WindowValuesPointers entry's s16 window values. */
bool fusion_mzm_serialize_power_bomb_windows(
    const FusionRom *rom, const FusionMzmRomSymbolTable *symbols,
    FusionMzmTextFile *file, char *error, size_t error_size);
void fusion_mzm_free_text_file(FusionMzmTextFile *file);
typedef struct {
    size_t room_count;
    size_t skipped_count;
    size_t sheet_count;
    size_t text_layer_count;
    size_t room_bytes;
    size_t background_bytes;
    /* The skipped rooms in export order, "item: reason". */
    char skipped[FUSION_MZM_EXPORT_SKIP_CAPACITY][160];
} FusionMzmExportSummary;

/* Writes the runtime-room export of scripts/mzm_runtime_room.py --all into
 * OUTPUT (the runtime rooms folder, created when missing): each exportable
 * room's <area>_<NNN>/ text layers, room.tsv and background.bmp, the used
 * tilesets/tileset_NN.bmp sheets, blocks.tsv and power_bomb.tsv on the first
 * room, then export_index.tsv, every file atomically and never through a
 * symlink. Rooms are skipped, with the reference's reason, without an RLE
 * BG1, without tile graphics under BG1 or in an area whose name is not
 * purely alphabetic (the hatch-event parser's rule). */
bool fusion_mzm_export_runtime_rooms(
    const FusionRom *rom, const uint8_t *elf, size_t elf_size,
    const char *output, FusionMzmExportSummary *summary,
    char *error, size_t error_size);
typedef struct {
    size_t samus_sequences;
    size_t samus_frames;
    size_t samus_unique_bmps;
    size_t bindings;
    size_t projectile_sequences;
    size_t projectile_frames;
    size_t projectile_unique_bmps;
} FusionMzmSamusExportSummary;

/* Writes the private Samus sprite library into SAMUS_OUTPUT (runtime_index.tsv,
 * objects/, animation_map.tsv, cannon_offsets.tsv, palettes.tsv,
 * sequences.json and manifest.json) and the projectile library into
 * PROJECTILE_OUTPUT (runtime_index.tsv, objects/ and sequences.json). Every
 * consumed ELF symbol must hold the ROM's bytes; any unresolved variant,
 * projectile table or particle fails the whole export. */
bool fusion_mzm_export_samus(const FusionRom *rom, const uint8_t *elf, size_t elf_size,
                             const char *samus_output, const char *projectile_output,
                             FusionMzmSamusExportSummary *summary,
                             char *error, size_t error_size);
typedef struct {
    size_t sequences;
    size_t frames;
    size_t unique_bmps;
} FusionMzmSpriteExportSummary;

/* Writes the private enemy sprite library into RUNTIME_OUTPUT
 * (runtime_index.tsv, objects/ and oam_bounds.tsv) and the sprite engine's
 * ROM tables into METADATA_OUTPUT/sprites.tsv. Every consumed ELF symbol
 * must hold the ROM's bytes. */
bool fusion_mzm_export_sprites(const FusionRom *rom, const uint8_t *elf, size_t elf_size,
                               const char *runtime_output, const char *metadata_output,
                               FusionMzmSpriteExportSummary *summary,
                               char *error, size_t error_size);
void fusion_mzm_free_runtime_room_payload(
    FusionMzmRuntimeRoomPayload *payload);

#endif /* FUSION_ASSETS_MZM_H */
