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

bool fusion_aos_read_area_directory(const FusionRom *rom,
                                    FusionAosAreaDirectory *directory,
                                    char *error, size_t error_size);
bool fusion_aos_read_map(const FusionRom *rom,
                         const FusionAosAreaDirectory *directory,
                         FusionAosMap *map, char *error, size_t error_size);

#endif /* FUSION_ASSETS_AOS_H */
