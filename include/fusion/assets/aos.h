/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded Aria of Sorrow metadata readers. */
#ifndef FUSION_ASSETS_AOS_H
#define FUSION_ASSETS_AOS_H

#include "fusion/assets/rom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { FUSION_AOS_AREA_COUNT = 12 };

typedef struct {
    uint32_t room_table_pointer;
    size_t room_count;
} FusionAosAreaEntry;

typedef struct {
    FusionAosAreaEntry areas[FUSION_AOS_AREA_COUNT];
    size_t total_rooms;
} FusionAosAreaDirectory;

bool fusion_aos_read_area_directory(const FusionRom *rom,
                                    FusionAosAreaDirectory *directory,
                                    char *error, size_t error_size);

#endif /* FUSION_ASSETS_AOS_H */
