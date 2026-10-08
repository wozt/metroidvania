#ifndef FUSION_CORE_TILEMAP_H
#define FUSION_CORE_TILEMAP_H

#include "core/types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* PC-only project-authored example tile data, not GBA graphics. */
#define FUSION_TILE_COLS 30u
#define FUSION_TILE_ROWS 17u
#define FUSION_TILE_SIZE 32u
#define FUSION_TILE_LAYERS 3u
#define FUSION_TILE_TYPES 5u

typedef enum {
    FUSION_LAYER_BACK = 0,
    FUSION_LAYER_TERRAIN = 1,
    FUSION_LAYER_FRONT = 2
} FusionTileLayer;

typedef struct {
    char room_id[64];
    WorldKind world;
    uint8_t tiles[FUSION_TILE_LAYERS][FUSION_TILE_ROWS][FUSION_TILE_COLS];
} FusionTilemap;

bool fusion_tilemap_valid(const FusionTilemap *map);
bool fusion_tilemap_load(FusionTilemap *out, const char *path,
                         char *error, size_t error_size);
bool fusion_tilemap_save(const FusionTilemap *map, const char *path,
                         char *error, size_t error_size);
/* Transparent tile zero; remaining colors are shared by SDL3 and GTK4. */
bool fusion_tile_color(WorldKind world, unsigned layer, unsigned tile,
                       uint8_t rgba[4]);

#endif
