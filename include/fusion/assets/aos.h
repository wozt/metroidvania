/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded Aria of Sorrow metadata readers. */
#ifndef FUSION_ASSETS_AOS_H
#define FUSION_ASSETS_AOS_H

#include "fusion/assets/rom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    FUSION_AOS_AREA_COUNT = 12,
    FUSION_AOS_ROOM_VARIANT_CAPACITY = 16,
    FUSION_AOS_ROOM_TRANSITION_CAPACITY = 16,
    FUSION_AOS_BACKGROUND_COUNT = 3,
    FUSION_AOS_LOAD_ENTRY_CAPACITY = 64,
    FUSION_AOS_ENTITY_CAPACITY = 512,
    FUSION_AOS_BOSS_ROOM_COUNT = 12,
    FUSION_AOS_BACKGROUND_SCREEN_LIMIT = 16,
    FUSION_AOS_VRAM_SIZE = 0x10000,
    FUSION_AOS_PALETTE_SIZE = 0x200,
    FUSION_AOS_RENDER_PIXEL_CAPACITY = 4096 * 4096,
    FUSION_AOS_COLLISION_CELL_CAPACITY =
        FUSION_AOS_BACKGROUND_SCREEN_LIMIT * FUSION_AOS_BACKGROUND_SCREEN_LIMIT *
        32 * 32,
    FUSION_AOS_MAP_WIDTH = 64,
    FUSION_AOS_MAP_HEIGHT = 35,
    FUSION_AOS_ROOM_FLAG_SAVE = 1 << 0,
    FUSION_AOS_ROOM_FLAG_WARP = 1 << 1,
    FUSION_AOS_MAP_CELL_CAPACITY = FUSION_AOS_MAP_WIDTH * FUSION_AOS_MAP_HEIGHT,
    FUSION_AOS_EXPORT_SKIP_CAPACITY = 64
};

typedef struct {
    uint32_t room_table_pointer;
    size_t room_count;
} FusionAosAreaEntry;

typedef struct {
    FusionAosAreaEntry areas[FUSION_AOS_AREA_COUNT];
    size_t total_rooms;
} FusionAosAreaDirectory;

typedef struct {
    uint8_t map_x;
    uint8_t map_y;
    uint8_t area;
    uint8_t room;
    bool save;
    bool warp;
    uint32_t room_pointer;
} FusionAosMapCell;

typedef struct {
    FusionAosMapCell cells[FUSION_AOS_MAP_CELL_CAPACITY];
    size_t count;
    size_t save_count;
    size_t warp_count;
} FusionAosMap;

typedef struct {
    uint32_t pointer;
    uint16_t selector;
} FusionAosRoomVariant;

typedef struct {
    uint32_t source_pointer;
    uint32_t resolved_pointer;
    FusionAosRoomVariant variants[FUSION_AOS_ROOM_VARIANT_CAPACITY];
    size_t variant_count;
    uint16_t display_control;
    uint32_t next_pointer;
    uint32_t backgrounds_pointer;
    uint32_t graphics_pointer;
    uint32_t palette_pointer;
    uint32_t entities_pointer;
    uint32_t transitions_pointer;
    size_t transition_count;
    uint16_t blend_control;
    uint8_t map_x;
    uint8_t map_y;
    bool coordinate_flag_0;
    bool coordinate_flag_1;
} FusionAosRoomDescriptor;

typedef struct {
    uint32_t entry_pointer;
    uint32_t target_pointer;
    int8_t source_screen_x;
    int8_t source_screen_y;
    uint16_t field_6;
    uint16_t field_8;
    uint16_t load_x;
    uint16_t load_y;
    uint16_t field_e;
    size_t target_area;
    size_t target_room;
} FusionAosTransition;

typedef struct {
    FusionAosTransition entries[FUSION_AOS_ROOM_TRANSITION_CAPACITY];
    size_t count;
} FusionAosTransitionList;

typedef struct {
    uint32_t descriptor_pointer;
    uint8_t layer;
    uint8_t field_0;
    uint8_t field_1;
    uint16_t control;
    uint16_t field_4;
    uint16_t field_6;
    uint32_t metadata_pointer;
    bool has_dimensions;
    uint8_t width_screens;
    uint8_t height_screens;
} FusionAosBackground;

typedef struct {
    FusionAosBackground entries[FUSION_AOS_BACKGROUND_COUNT];
} FusionAosBackgroundList;

typedef enum {
    FUSION_AOS_TILEMAP_ABSENT,
    FUSION_AOS_TILEMAP_UNSUPPORTED_AFFINE,
    FUSION_AOS_TILEMAP_DECODED
} FusionAosTilemapStatus;

typedef struct {
    FusionAosTilemapStatus status;
    uint32_t metadata_pointer;
    uint8_t width_screens;
    uint8_t height_screens;
    uint16_t flags;
    uint32_t blocks_pointer;
    uint32_t map_pointer;
    uint16_t maximum_block;
    size_t width_tiles;
    size_t height_tiles;
    uint16_t *cells;
    size_t cell_count;
} FusionAosBackgroundTilemap;

typedef enum {
    FUSION_AOS_COLLISION_ABSENT,
    FUSION_AOS_COLLISION_UNSUPPORTED_AFFINE,
    FUSION_AOS_COLLISION_NONE,
    FUSION_AOS_COLLISION_DECODED
} FusionAosCollisionStatus;

typedef struct {
    FusionAosCollisionStatus status;
    uint32_t metadata_pointer;
    uint8_t width_screens;
    uint8_t height_screens;
    uint16_t flags;
    uint32_t blocks_pointer;
    uint32_t collision_pointer;
    uint32_t map_pointer;
    uint16_t maximum_block;
    size_t width_tiles;
    size_t height_tiles;
    uint8_t *cells;
    size_t cell_count;
} FusionAosCollisionMap;

typedef struct {
    uint32_t entry_pointer;
    uint32_t resource_pointer;
    uint8_t parameters[3];
} FusionAosLoadEntry;

typedef struct {
    FusionAosLoadEntry entries[FUSION_AOS_LOAD_ENTRY_CAPACITY];
    size_t count;
} FusionAosLoadList;

typedef struct {
    uint8_t vram[FUSION_AOS_VRAM_SIZE];
    uint8_t palette[FUSION_AOS_PALETTE_SIZE];
    size_t graphics_load_count;
    size_t palette_load_count;
} FusionAosVideoMemory;

typedef struct {
    size_t width_pixels;
    size_t height_pixels;
    uint8_t *rgba;
    size_t pixel_count;
    size_t unresolved_count;
} FusionAosRenderedBackground;

typedef struct {
    const FusionAosBackground *background;
    const FusionAosRenderedBackground *rendered;
} FusionAosCompositeLayer;

typedef struct {
    size_t width_pixels;
    size_t height_pixels;
    uint8_t *rgb;
    size_t pixel_count;
} FusionAosCompositeBackground;

typedef struct {
    uint8_t *data;
    size_t size;
} FusionAosBitmap24;

typedef struct {
    uint8_t *data;
    size_t size;
} FusionAosRuntimeRoomPayload;

typedef struct {
    uint32_t entry_pointer;
    int16_t x;
    int16_t y;
    uint8_t persistent_index;
    uint8_t kind;
    uint8_t entity_id;
    uint8_t flags;
    uint16_t parameters[2];
} FusionAosEntity;

typedef struct {
    FusionAosEntity entries[FUSION_AOS_ENTITY_CAPACITY];
    size_t count;
} FusionAosEntityList;

typedef struct {
    const char *id;
    const char *name;
    const char *phase;
    uint8_t area;
    uint8_t room;
    uint8_t enemy_id;
} FusionAosBossSpec;

typedef struct {
    const FusionAosBossSpec *spec;
    uint8_t map_x;
    uint8_t map_y;
    uint32_t room_pointer;
    uint32_t entity_pointer;
    uint32_t enemy_table_pointer;
    uint16_t health;
} FusionAosBossRoom;

typedef struct {
    size_t native_room_count;
    size_t mapped_cells;
    size_t transition_count;
    size_t entity_count;
    size_t boss_room_count;
    size_t savepoint_count;
    size_t warp_count;
} FusionAosWorldSummary;

bool fusion_aos_read_area_directory(const FusionRom *rom,
                                    FusionAosAreaDirectory *directory,
                                    char *error, size_t error_size);
bool fusion_aos_read_map(const FusionRom *rom,
                         const FusionAosAreaDirectory *directory,
                         FusionAosMap *map, char *error, size_t error_size);
bool fusion_aos_read_room_descriptor(const FusionRom *rom,
                                     const FusionAosAreaDirectory *directory,
                                     size_t area, size_t room,
                                     FusionAosRoomDescriptor *descriptor,
                                     char *error, size_t error_size);
bool fusion_aos_read_room_transitions(const FusionRom *rom,
                                      const FusionAosAreaDirectory *directory,
                                      const FusionAosRoomDescriptor *descriptor,
                                      FusionAosTransitionList *transitions,
                                      char *error, size_t error_size);
bool fusion_aos_read_room_backgrounds(const FusionRom *rom,
                                      const FusionAosRoomDescriptor *descriptor,
                                      FusionAosBackgroundList *backgrounds,
                                      char *error, size_t error_size);
bool fusion_aos_read_background_tiles(const FusionRom *rom,
                                      const FusionAosBackground *background,
                                      FusionAosBackgroundTilemap *tilemap,
                                      char *error, size_t error_size);
void fusion_aos_free_background_tilemap(FusionAosBackgroundTilemap *tilemap);
bool fusion_aos_read_background_collision(const FusionRom *rom,
                                          const FusionAosBackground *background,
                                          FusionAosCollisionMap *collision,
                                          char *error, size_t error_size);
void fusion_aos_free_collision_map(FusionAosCollisionMap *collision);
bool fusion_aos_read_load_list(const FusionRom *rom, uint32_t pointer,
                               FusionAosLoadList *list,
                               char *error, size_t error_size);
bool fusion_aos_load_video_memory(const FusionRom *rom,
                                  const FusionAosRoomDescriptor *descriptor,
                                  FusionAosVideoMemory *memory,
                                  char *error, size_t error_size);
/* One object sprite frame rendered from the ROM: tile sheet `graphics`,
 * palette bank `bank` of `palette`, record `frame` of the frame list
 * `frames`, as a cropped 32-bit BMP (freed by the caller) whose top-left is
 * at (left, top) from the entity's position. */
bool fusion_aos_render_object_frame(const FusionRom *rom, uint32_t graphics, uint32_t palette,
                                    uint8_t bank, uint32_t frames, uint8_t frame,
                                    uint8_t **bmp, size_t *bmp_size, int *left, int *top,
                                    char *error, size_t error_size);
bool fusion_aos_render_background(const FusionAosBackground *background,
                                  const FusionAosBackgroundTilemap *tilemap,
                                  const FusionAosVideoMemory *memory,
                                  FusionAosRenderedBackground *rendered,
                                  char *error, size_t error_size);
void fusion_aos_free_rendered_background(FusionAosRenderedBackground *rendered);
bool fusion_aos_composite_backgrounds(
    const FusionAosCompositeLayer *layers, size_t layer_count,
    FusionAosCompositeBackground *composite,
    char *error, size_t error_size);
void fusion_aos_free_composite_background(
    FusionAosCompositeBackground *composite);
bool fusion_aos_serialize_composite_bmp24(
    const FusionAosCompositeBackground *composite,
    FusionAosBitmap24 *bitmap, char *error, size_t error_size);
void fusion_aos_free_bitmap24(FusionAosBitmap24 *bitmap);
bool fusion_aos_read_entity_list(const FusionRom *rom, uint32_t pointer,
                                 FusionAosEntityList *list,
                                 char *error, size_t error_size);
bool fusion_aos_serialize_runtime_room(
    size_t area, size_t room, const FusionAosCollisionMap *collision,
    const FusionAosTransitionList *transitions,
    const FusionAosEntityList *entities, uint8_t room_flags,
    FusionAosRuntimeRoomPayload *payload, char *error, size_t error_size);
void fusion_aos_free_runtime_room_payload(
    FusionAosRuntimeRoomPayload *payload);
const FusionAosBossSpec *fusion_aos_boss_spec(size_t index);
bool fusion_aos_read_boss_room(const FusionRom *rom,
                               const FusionAosAreaDirectory *directory,
                               const FusionAosBossSpec *spec,
                               FusionAosBossRoom *boss,
                               char *error, size_t error_size);
bool fusion_aos_read_world_summary(const FusionRom *rom,
                                   FusionAosWorldSummary *summary,
                                   char *error, size_t error_size);
bool fusion_aos_write_world_catalog(const FusionRom *rom, const char *path,
                                    char *error, size_t error_size);
bool fusion_aos_write_room_index(const FusionRom *rom, const char *path,
                                 char *error, size_t error_size);

typedef struct {
    size_t room_count;
    size_t skipped_count;
    size_t room_bytes;
    size_t background_bytes;
    /* The skipped rooms in export order, "area/room: reason". */
    char skipped[FUSION_AOS_EXPORT_SKIP_CAPACITY][160];
} FusionAosExportSummary;

/* Writes the complete native runtime-room export into OUTPUT (the runtime
 * rooms folder, created when missing): for each room
 * with a decoded BG1 collision table, area_<AA>_room_<RRR>/room.tsv
 * (AOSROOM-NATIVE v4) and background.bmp (the static-origin composite), then
 * export_index.tsv, every file atomically and never through a symlink. */
bool fusion_aos_export_runtime_rooms(const FusionRom *rom, const char *output,
                                     FusionAosExportSummary *summary,
                                     char *error, size_t error_size);

typedef struct {
    size_t soma_animations;
    size_t knife_animations;
    size_t frames;
    size_t unique_bmps;
    size_t removed_stale_objects;
} FusionAosSomaExportSummary;

/* Writes the canonical content-addressed Soma/knife animation library to the
 * supplied private runtime folder. */
bool fusion_aos_export_soma(const FusionRom *rom, const char *output,
                            FusionAosSomaExportSummary *summary,
                            char *error, size_t error_size);

typedef struct {
    size_t weapons;
    size_t sequences;
    size_t frames;
    size_t unique_bmps;
    size_t outside_components;
    size_t skipped_count;
    size_t removed_stale_objects;
    char skipped[FUSION_AOS_EXPORT_SKIP_CAPACITY][160];
} FusionAosWeaponExportSummary;

/* Writes the complete weapon records and frame metadata into METADATA_OUTPUT,
 * and the canonical content-addressed weapon sprites into RUNTIME_OUTPUT. */
bool fusion_aos_export_weapons(const FusionRom *rom,
                               const char *metadata_output,
                               const char *runtime_output,
                               FusionAosWeaponExportSummary *summary,
                               char *error, size_t error_size);

typedef struct {
    size_t sequences;
    size_t frames;
    size_t unique_bmps;
    size_t removed_stale_objects;
} FusionAosObjectExportSummary;

/* Writes traced enemy statistics and frame metadata into METADATA_OUTPUT,
 * and the content-addressed door, enemy and effect sprites into RUNTIME_OUTPUT. */
bool fusion_aos_export_objects(const FusionRom *rom,
                               const char *metadata_output,
                               const char *runtime_output,
                               FusionAosObjectExportSummary *summary,
                               char *error, size_t error_size);

#endif /* FUSION_ASSETS_AOS_H */
