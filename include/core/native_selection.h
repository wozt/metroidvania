#ifndef FUSION_NATIVE_SELECTION_H
#define FUSION_NATIVE_SELECTION_H
#include "core/native_map.h"
/* Rectangular selection and in-place move, in metatile coordinates.
 * A move clears the source to block 0 and then places the captured tiles.
 * Target must fit entirely in the layer. Returns false without modification
 * on invalid coordinates, a zero offset or an unavailable layer. */
bool native_selection_move(NativeMap *map, unsigned layer,
                           int x0, int y0, int x1, int y1,
                           int dx, int dy);
#endif
