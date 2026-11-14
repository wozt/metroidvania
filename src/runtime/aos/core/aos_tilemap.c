/* SPDX-License-Identifier: GPL-3.0-only */
/* See aos_tilemap.h for the native routines this ports. */
#include "aos_tilemap.h"

bool aos_room_tilemap_load(AosRoomTilemap *tilemap, const FusionRom *rom,
                           const FusionAosAreaDirectory *directory,
                           size_t area, size_t room,
                           char *error, size_t error_size) {
    if (!tilemap || !rom || !directory) return false;
    *tilemap = (AosRoomTilemap){0};
    FusionAosRoomDescriptor descriptor;
    FusionAosBackgroundList backgrounds;
    if (!fusion_aos_read_room_descriptor(rom, directory, area, room,
                                         &descriptor, error, error_size) ||
        !fusion_aos_read_room_backgrounds(rom, &descriptor, &backgrounds,
                                          error, error_size)) {
        return false;
    }
    for (size_t index = 0; index < FUSION_AOS_BACKGROUND_COUNT; ++index) {
        if (!fusion_aos_read_background_tiles(
                rom, &backgrounds.entries[index],
                &tilemap->layers[index], error, error_size)) {
            aos_room_tilemap_free(tilemap);
            return false;
        }
    }
    return true;
}

void aos_room_tilemap_free(AosRoomTilemap *tilemap) {
    if (!tilemap) return;
    for (size_t index = 0; index < FUSION_AOS_BACKGROUND_COUNT; ++index)
        fusion_aos_free_background_tilemap(&tilemap->layers[index]);
    *tilemap = (AosRoomTilemap){0};
}

FusionAosTilemapStatus aos_tilemap_status(const AosRoomTilemap *tilemap,
                                          int bg) {
    if (!tilemap || bg < 1 || bg > FUSION_AOS_BACKGROUND_COUNT)
        return FUSION_AOS_TILEMAP_ABSENT;
    return tilemap->layers[bg - 1].status;
}

uint16_t aos_tilemap_lookup(const AosRoomTilemap *tilemap, int bg,
                            uint16_t tile_x, uint16_t tile_y) {
    if (!tilemap || bg < 1 || bg > FUSION_AOS_BACKGROUND_COUNT)
        return 0;
    const FusionAosBackgroundTilemap *layer = &tilemap->layers[bg - 1];
    if (layer->status != FUSION_AOS_TILEMAP_DECODED || !layer->cells)
        return 0;
    /* The native sub_08001800 indexes the block map without a bounds
     * check; the decoded array is indexed the same way, so out-of-map
     * coordinates (unreachable for clamped scroll) report an empty
     * block instead of reading adjacent data. */
    if ((size_t)tile_x >= layer->width_tiles ||
        (size_t)tile_y >= layer->height_tiles)
        return 0;
    return layer->cells[(size_t)tile_x + (size_t)tile_y * layer->width_tiles];
}

static size_t tilemap_span(const AosRoomTilemap *tilemap, int bg,
                           uint16_t fixed, uint16_t start, bool vary_y,
                           size_t count, uint16_t *out, size_t cap) {
    if (out == NULL) return count;
    size_t written = count < cap ? count : cap;
    for (size_t i = 0; i < written; ++i) {
        uint16_t tile_x = vary_y ? fixed : (uint16_t)(start + i);
        uint16_t tile_y = vary_y ? (uint16_t)(start + i) : fixed;
        out[i] = aos_tilemap_lookup(tilemap, bg, tile_x, tile_y);
    }
    return written;
}

size_t aos_tilemap_column(const AosRoomTilemap *tilemap, int bg,
                          uint16_t tile_x, uint16_t tile_y, size_t count,
                          uint16_t *out, size_t cap) {
    return tilemap_span(tilemap, bg, tile_x, tile_y, true, count, out, cap);
}

size_t aos_tilemap_row(const AosRoomTilemap *tilemap, int bg,
                       uint16_t tile_x, uint16_t tile_y, size_t count,
                       uint16_t *out, size_t cap) {
    return tilemap_span(tilemap, bg, tile_y, tile_x, false, count, out, cap);
}
