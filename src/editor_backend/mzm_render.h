/* SPDX-License-Identifier: GPL-3.0-only */
/* The partial Zero Mission BG1/BG2 room renderer of the editor (formerly
 * scripts/mzm_room_render.py), reading the private raw imports. */
#ifndef FUSION_EDITOR_BACKEND_MZM_RENDER_H
#define FUSION_EDITOR_BACKEND_MZM_RENDER_H

#include "common.h"

#include <stdint.h>

enum { FEB_MZM_MAX_RAW = 128 * 1024, FEB_MZM_MAX_BLOCKS = 6144 };

typedef struct {
    int64_t width, height;
    uint16_t *blocks;
} FebBlockMap;

typedef struct {
    uint8_t *gfx;          /* decompressed 4bpp tiles */
    size_t gfx_size;
    uint8_t *palette;
    size_t palette_size;
    uint16_t (*table)[4];  /* metatiles */
    size_t entries;
} FebMzmTileset;

void feb_block_map_free(FebBlockMap *map);
void feb_mzm_tileset_free(FebMzmTileset *tileset);

/* read_raw(): metroid/raw/data/``name``, bounded and not a symlink. */
bool feb_mzm_read_raw(const char *repository, const char *name, uint8_t **data, size_t *size,
                      FebError *error);
/* lz77(): a GBA BIOS 0x10 stream of at most ``limit`` expanded bytes. */
bool feb_lz77(const uint8_t *data, size_t size, size_t limit, uint8_t **out, size_t *out_size,
              FebError *error);
/* rle_room(): the room RLE of one blob. */
bool feb_mzm_rle(const uint8_t *data, size_t size, FebBlockMap *map, FebError *error);
/* tileset_blobs(): the raw graphics, palette and tilemap resources. */
bool feb_mzm_tileset_blobs(const char *repository, int64_t index, uint8_t *blobs[3],
                          size_t sizes[3], FebError *error);
/* read_source_room(): the RoomEntryRom record (area in any case). */
bool feb_mzm_source_room(const char *repository, const char *area, int64_t number,
                         FusionJson *room, FebError *error);
/* rle_room(room_blob(symbol)). */
bool feb_mzm_room_map(const char *repository, const char *symbol, FebBlockMap *map,
                      FebError *error);
/* tileset_blobs() decoded: lz77 graphics, palette and metatiles. */
bool feb_mzm_tileset(const char *repository, int64_t index, FebMzmTileset *tileset,
                     FebError *error);
/* graphic_base(): the character-block origin of the BG1 blocks. */
bool feb_mzm_graphic_base(const FebMzmTileset *tileset, const FebBlockMap *bg1, int64_t *base,
                          int64_t *coverage, FebError *error);
/* render_layer(); ``visibility`` is optional. */
bool feb_mzm_render_layer(int64_t width, int64_t height, const uint16_t *blocks,
                          const FebMzmTileset *tileset, int64_t base, uint8_t **rgb,
                          uint8_t *visibility, int64_t *unresolved, int64_t *painted,
                          FebError *error);
/* collision_preview(): the diagnostic Clipdata BMP. */
bool feb_mzm_collision_preview(const FebBlockMap *map, uint8_t **bmp, size_t *size,
                               FebError *error);
/* decode_room(): the render report, writing the previews on request. */
bool feb_mzm_decode_room(const char *repository, const char *area, int64_t number,
                         bool write_outputs, FusionJson *result, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_MZM_RENDER_H */
