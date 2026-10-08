#ifndef FUSION_TILE_PAINT_H
#define FUSION_TILE_PAINT_H

#include "core/tilemap.h"
#include <stdbool.h>

typedef enum {
    FUSION_PAINT_PENCIL = 0,
    FUSION_PAINT_ERASER = 1,
    FUSION_PAINT_FILL = 2,
    FUSION_PAINT_PICK = 3
} FusionPaintTool;

/* Returns true only when a cell changes. PICK reports the selected tile
 * through picked (which can be NULL), without modifying the room. */
bool fusion_tile_paint(FusionTilemap *map, unsigned layer, int x, int y,
                       FusionPaintTool tool, unsigned brush, unsigned *picked);
/* Bresenham, for contiguous strokes when pointer events skip grid cells. */
bool fusion_tile_stroke(FusionTilemap *map, unsigned layer, int from_x, int from_y,
                        int to_x, int to_y, FusionPaintTool tool, unsigned brush);
#endif
