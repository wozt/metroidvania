/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ASSETS_CORE_SPRITE_LIBRARY_H
#define FUSION_ASSETS_CORE_SPRITE_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "export.h"

typedef struct {
    char digest[65];
    size_t size;
} FusionSpriteObject;

typedef struct {
    char output[FUSION_EXPORT_PATH_CAPACITY];
    char objects[FUSION_EXPORT_PATH_CAPACITY];
    char index_prefix[256];
    char *rows;
    size_t rows_size;
    size_t rows_capacity;
    FusionSpriteObject *packed;
    size_t packed_count;
    size_t packed_capacity;
    size_t sequences;
    size_t frames;
} FusionSpriteLibrary;

typedef struct {
    size_t sequences;
    size_t frames;
    size_t unique_bmps;
    size_t removed_stale_objects;
} FusionSpriteLibrarySummary;

bool fusion_sprite_library_init(FusionSpriteLibrary *library,
                                const char *output, const char *index_prefix,
                                char *error, size_t error_size);
bool fusion_sprite_library_begin(FusionSpriteLibrary *library,
                                 char *error, size_t error_size);
bool fusion_sprite_library_add(FusionSpriteLibrary *library, const char *key,
                               size_t frame, unsigned ticks, int offset_x,
                               int offset_y, const uint8_t *bmp, size_t bmp_size,
                               char *error, size_t error_size);
bool fusion_sprite_library_finish(FusionSpriteLibrary *library,
                                  FusionSpriteLibrarySummary *summary,
                                  char *error, size_t error_size);
void fusion_sprite_library_free(FusionSpriteLibrary *library);

/* Crops non-zero-alpha RGBA pixels around the supplied draw origin and emits
 * the exact 32-bit BITMAPV4 format used by the private sprite libraries. */
bool fusion_sprite_bmp32_crop(const uint8_t *rgba, size_t width, size_t height,
                              int origin_x, int origin_y, uint8_t **bmp,
                              size_t *bmp_size, int *left, int *top,
                              char *error, size_t error_size);

/* Crops non-zero palette indices around the draw origin and emits an 8-bit
 * BMP whose palette holds ``color_count`` RGB triplets (index 0 is the
 * transparent color and the remaining entries are zero). */
bool fusion_sprite_bmp8_crop(const uint8_t *indices, size_t width, size_t height,
                             int origin_x, int origin_y, const uint8_t *colors,
                             size_t color_count, uint8_t **bmp, size_t *bmp_size,
                             int *left, int *top, char *error, size_t error_size);

#endif /* FUSION_ASSETS_CORE_SPRITE_LIBRARY_H */
