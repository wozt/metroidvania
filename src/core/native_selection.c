/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/native_selection.h"
#include <stdint.h>
#include <stdlib.h>

bool native_selection_move(NativeMap *map, unsigned layer,
                           int x0, int y0, int x1, int y1,
                           int dx, int dy)
{
    int w, h;
    size_t count;
    uint16_t *copy;
    if (!map || layer >= 2 || (!dx && !dy)) return false;
    w = (int)map->width[layer];
    h = (int)map->height[layer];
    if (w < 1 || h < 1 || (unsigned)w * (unsigned)h > NATIVE_MAX_CELLS ||
        dx <= -w || dx >= w || dy <= -h || dy >= h ||
        x0 < 0 || y0 < 0 || x1 < x0 || y1 < y0 || x1 >= w || y1 >= h ||
        x0 + dx < 0 || x1 + dx >= w || y0 + dy < 0 || y1 + dy >= h)
        return false;
    count = (size_t)(x1 - x0 + 1) * (size_t)(y1 - y0 + 1);
    copy = malloc(count * sizeof(*copy));
    if (!copy) return false;
    size_t i = 0;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            copy[i++] = map->blocks[layer][(size_t)y * (size_t)w + (size_t)x];
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            map->blocks[layer][(size_t)y * (size_t)w + (size_t)x] = 0;
    i = 0;
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            map->blocks[layer][(size_t)(y + dy) * (size_t)w +
                               (size_t)(x + dx)] = copy[i++];
    free(copy);
    return true;
}
