/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/tilemap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
}

static bool line(FILE *file, char *out, size_t size)
{
    size_t n;
    if (!fgets(out, (int)size, file)) return false;
    n = strlen(out);
    if (n && out[n - 1] == '\n') out[--n] = 0;
    else if (!feof(file)) return false; /* Refuse truncated lines. */
    if (n && out[n - 1] == '\r') out[n - 1] = 0;
    return true;
}

bool fusion_tilemap_valid(const FusionTilemap *map)
{
    unsigned l, y, x;
    if (!map || !map->room_id[0] ||
        !memchr(map->room_id, 0, sizeof(map->room_id)) ||
        (map->world != WORLD_METROID && map->world != WORLD_CASTLEVANIA))
        return false;
    for (x = 0; map->room_id[x]; ++x)
        if (!((map->room_id[x] >= 'a' && map->room_id[x] <= 'z') ||
              (map->room_id[x] >= '0' && map->room_id[x] <= '9') ||
              map->room_id[x] == ':' || map->room_id[x] == '_'))
            return false;
    for (l = 0; l < FUSION_TILE_LAYERS; ++l)
        for (y = 0; y < FUSION_TILE_ROWS; ++y)
            for (x = 0; x < FUSION_TILE_COLS; ++x)
                if (map->tiles[l][y][x] >= FUSION_TILE_TYPES) return false;
    return true;
}

bool fusion_tilemap_load(FusionTilemap *out, const char *path,
                         char *error, size_t error_size)
{
    static const char *names[] = {"LAYER BACK", "LAYER TERRAIN", "LAYER FRONT"};
    FusionTilemap map = {0};
    FILE *file;
    char row[128], id[64], extra[2];
    unsigned layer, y, x;
    int world;
    if (!out || !path || !path[0]) {
        fail(error, error_size, "invalid tilemap arguments"); return false;
    }
    file = fopen(path, "r");
    if (!file) { fail(error, error_size, "cannot open tilemap"); return false; }
    if (!line(file, row, sizeof(row)) || strcmp(row, "MVTILE 1") != 0 ||
        !line(file, row, sizeof(row)) ||
        sscanf(row, "ROOM %63s %d %1s", id, &world, extra) != 2 ||
        (world != 0 && world != 1) ||
        !line(file, row, sizeof(row)) || strcmp(row, "SIZE 30 17") != 0)
        goto invalid;
    snprintf(map.room_id, sizeof(map.room_id), "%s", id);
    map.world = (WorldKind)world;
    for (layer = 0; layer < FUSION_TILE_LAYERS; ++layer) {
        if (!line(file, row, sizeof(row)) || strcmp(row, names[layer]))
            goto invalid;
        for (y = 0; y < FUSION_TILE_ROWS; ++y) {
            if (!line(file, row, sizeof(row)) ||
                strlen(row) != FUSION_TILE_COLS) goto invalid;
            for (x = 0; x < FUSION_TILE_COLS; ++x) {
                if (row[x] < '0' || row[x] >= '0' + (int)FUSION_TILE_TYPES)
                    goto invalid;
                map.tiles[layer][y][x] = (uint8_t)(row[x] - '0');
            }
        }
    }
    if (fgetc(file) != EOF || ferror(file) || !fusion_tilemap_valid(&map))
        goto invalid;
    fclose(file);
    *out = map;
    fail(error, error_size, "");
    return true;
invalid:
    fclose(file);
    fail(error, error_size, "invalid MVTILE 1 tilemap data");
    return false;
}

bool fusion_tilemap_save(const FusionTilemap *map, const char *path,
                         char *error, size_t error_size)
{
    static const char *names[] = {"LAYER BACK", "LAYER TERRAIN", "LAYER FRONT"};
    char temp[512];
    FILE *file;
    unsigned layer, y, x;
    int failed;
    if (!fusion_tilemap_valid(map) || !path || !path[0] ||
        snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp)) {
        fail(error, error_size, "invalid tilemap output"); return false;
    }
    file = fopen(temp, "w");
    if (!file) { fail(error, error_size, "cannot write tilemap"); return false; }
    fprintf(file, "MVTILE 1\nROOM %s %d\nSIZE 30 17\n",
            map->room_id, (int)map->world);
    for (layer = 0; layer < FUSION_TILE_LAYERS; ++layer) {
        fprintf(file, "%s\n", names[layer]);
        for (y = 0; y < FUSION_TILE_ROWS; ++y) {
            for (x = 0; x < FUSION_TILE_COLS; ++x)
                fputc('0' + map->tiles[layer][y][x], file);
            fputc('\n', file);
        }
    }
    failed = ferror(file);
    if (fclose(file) != 0) failed = 1;
    if (failed || rename(temp, path) != 0) {
        remove(temp);
        fail(error, error_size, "tilemap atomic save failed");
        return false;
    }
    fail(error, error_size, "");
    return true;
}

bool fusion_tile_color(WorldKind world, unsigned layer, unsigned tile,
                       uint8_t rgba[4])
{
    static const uint8_t metroid[3][5][3] = {
        {{0,0,0},{22,38,50},{29,57,65},{36,70,71},{54,84,74}},
        {{0,0,0},{68,91,96},{86,117,111},{112,128,102},{143,105,73}},
        {{0,0,0},{43,76,76},{63,113,106},{128,105,69},{175,131,71}}
    };
    static const uint8_t aria[3][5][3] = {
        {{0,0,0},{36,24,49},{53,31,62},{71,40,72},{85,53,70}},
        {{0,0,0},{92,65,93},{114,71,100},{137,94,110},{160,128,113}},
        {{0,0,0},{64,44,77},{106,65,109},{142,92,112},{180,125,137}}
    };
    const uint8_t (*palette)[5][3];
    if (!rgba || layer >= FUSION_TILE_LAYERS ||
        tile >= FUSION_TILE_TYPES ||
        (world != WORLD_METROID && world != WORLD_CASTLEVANIA)) return false;
    palette = world == WORLD_METROID ? metroid : aria;
    rgba[0] = palette[layer][tile][0];
    rgba[1] = palette[layer][tile][1];
    rgba[2] = palette[layer][tile][2];
    rgba[3] = tile ? 255 : 0;
    return true;
}
