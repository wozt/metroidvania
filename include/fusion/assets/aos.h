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
    FUSION_AOS_MAP_WIDTH = 64,
    FUSION_AOS_MAP_HEIGHT = 35,
    FUSION_AOS_MAP_CELL_CAPACITY = FUSION_AOS_MAP_WIDTH * FUSION_AOS_MAP_HEIGHT
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

typedef struct {
    uint32_t entry_pointer;
    uint32_t resource_pointer;
    uint8_t parameters[3];
} FusionAosLoadEntry;

typedef struct {
    FusionAosLoadEntry entries[FUSION_AOS_LOAD_ENTRY_CAPACITY];
    size_t count;
} FusionAosLoadList;

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
bool fusion_aos_read_load_list(const FusionRom *rom, uint32_t pointer,
                               FusionAosLoadList *list,
                               char *error, size_t error_size);

#endif /* FUSION_ASSETS_AOS_H */
