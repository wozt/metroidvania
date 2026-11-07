/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_QUICK_MAP_H
#define AOS_QUICK_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/aos.h"

enum {
    AOS_QUICK_MAP_WIDTH = 64,
    AOS_QUICK_MAP_HEIGHT = 35,
    AOS_QUICK_MAP_CELLS = AOS_QUICK_MAP_WIDTH * AOS_QUICK_MAP_HEIGHT,
};

typedef struct {
    uint8_t vram[0x8000];
    uint8_t palette[0x200];
    uint16_t castle[AOS_QUICK_MAP_CELLS];
    uint8_t connections[32 * AOS_QUICK_MAP_HEIGHT];
    uint8_t explored[AOS_QUICK_MAP_CELLS];
    FusionAosAreaDirectory directory;
    uint8_t current_x;
    uint8_t current_y;
    bool has_current;
} AosQuickMap;

/* sub_08011338: loads the native quick-map pixels, palette, castle table and
 * edge table from a verified Aria ROM. No proprietary bytes are persisted. */
bool aos_quick_map_init(AosQuickMap *map, const FusionRom *rom,
                        char *error, size_t error_size);
/* sub_08011430's current screen discovery, using the source room descriptor. */
bool aos_quick_map_visit(AosQuickMap *map, const FusionRom *rom,
                         unsigned area, unsigned room,
                         int player_x, int player_y,
                         char *error, size_t error_size);
/* sub_08010D60 and its four edge writers. Castle Map 1-3 correspond to the
 * three item-inventory records at 0x1D-0x1F. */
bool aos_quick_map_render(AosQuickMap *map, bool castle_map_1,
                          bool castle_map_2, bool castle_map_3,
                          uint32_t animation_frame,
                          uint8_t *rgba, size_t rgba_size);

/* Schema-v13 persistence: the explored cells packed one bit per cell
 * (AOS_QUICK_MAP_CELLS / 8 = 280 bytes) so a save can carry the visited
 * castle. Unpacking an all-zero field clears the grid. */
enum { AOS_QUICK_MAP_PACKED_BYTES = AOS_QUICK_MAP_CELLS / 8 };
void aos_quick_map_pack_explored(const AosQuickMap *map, uint8_t *out);
void aos_quick_map_unpack_explored(AosQuickMap *map, const uint8_t *in);

#endif /* AOS_QUICK_MAP_H */
