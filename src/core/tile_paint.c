/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/tile_paint.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static bool inside(int x, int y)
{
    return x >= 0 && y >= 0 && x < (int)FUSION_TILE_COLS &&
           y < (int)FUSION_TILE_ROWS;
}

bool fusion_tile_paint(FusionTilemap *map, unsigned layer, int x, int y,
                       FusionPaintTool tool, unsigned brush, unsigned *picked)
{
    uint8_t *cell;
    uint8_t target;
    int queue[FUSION_TILE_COLS * FUSION_TILE_ROWS];
    size_t head = 0, tail = 0;
    if (!map || !fusion_tilemap_valid(map) || layer >= FUSION_TILE_LAYERS ||
        !inside(x, y) || tool < FUSION_PAINT_PENCIL || tool > FUSION_PAINT_PICK ||
        brush >= FUSION_TILE_TYPES) return false;
    cell = &map->tiles[layer][y][x];
    if (tool == FUSION_PAINT_PICK) {
        if (picked) *picked = *cell;
        return false;
    }
    if (tool == FUSION_PAINT_ERASER) brush = 0;
    if (*cell == brush) return false;
    if (tool != FUSION_PAINT_FILL) {
        *cell = (uint8_t)brush;
        return true;
    }
    target = *cell;
    *cell = (uint8_t)brush;
    queue[tail++] = y * (int)FUSION_TILE_COLS + x;
    while (head < tail) {
        int index = queue[head++];
        int cx = index % (int)FUSION_TILE_COLS;
        int cy = index / (int)FUSION_TILE_COLS;
        static const int directions[4][2] = {{-1,0}, {1,0}, {0,-1}, {0,1}};
        size_t d;
        for (d = 0; d < 4; ++d) {
            int nx = cx + directions[d][0];
            int ny = cy + directions[d][1];
            if (inside(nx, ny) && map->tiles[layer][ny][nx] == target) {
                map->tiles[layer][ny][nx] = (uint8_t)brush;
                /* Each cell enters the queue at most once: it is changed
                 * before enqueue. */
                if (tail >= FUSION_TILE_COLS * FUSION_TILE_ROWS) return false;
                queue[tail++] = ny * (int)FUSION_TILE_COLS + nx;
            }
        }
    }
    return true;
}

bool fusion_tile_stroke(FusionTilemap *map, unsigned layer, int from_x, int from_y,
                        int to_x, int to_y, FusionPaintTool tool, unsigned brush)
{
    int dx, dy, sx, sy, error;
    bool changed = false;
    if (tool != FUSION_PAINT_PENCIL && tool != FUSION_PAINT_ERASER) return false;
    if (!inside(from_x, from_y) || !inside(to_x, to_y)) return false;
    dx = abs(to_x - from_x);
    dy = -abs(to_y - from_y);
    sx = from_x < to_x ? 1 : -1;
    sy = from_y < to_y ? 1 : -1;
    error = dx + dy;
    for (;;) {
        changed |= fusion_tile_paint(map, layer, from_x, from_y, tool, brush, NULL);
        if (from_x == to_x && from_y == to_y) break;
        {
            int e2 = 2 * error;
            if (e2 >= dy) { error += dy; from_x += sx; }
            if (e2 <= dx) { error += dx; from_y += sy; }
        }
    }
    return changed;
}
