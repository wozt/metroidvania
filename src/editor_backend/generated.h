/* SPDX-License-Identifier: GPL-3.0-only */
/* The private generated-asset writer of the editor tools (formerly
 * scripts/import_game_assets.py::write_generated) and BMP encoding. */
#ifndef FUSION_EDITOR_BACKEND_GENERATED_H
#define FUSION_EDITOR_BACKEND_GENERATED_H

#include "common.h"

#include <stdint.h>

/* safe_output(): ``repository``/assets/extracted/``relative``, refusing
 * unsafe names and symlink components. */
bool feb_generated_path(const char *repository, const char *relative, char *path,
                        FebError *error);
/* write_generated(): an atomic write skipped when the bytes are unchanged;
 * ``written`` (optional) tells whether the file changed. */
bool feb_write_generated(const char *repository, const char *relative, const uint8_t *data,
                         size_t size, bool *written, FebError *error);
/* bmp24(): a bottom-up 24-bit BMP of top-down RGB pixels. */
bool feb_bmp24(int64_t width, int64_t height, const uint8_t *rgb, uint8_t **bmp, size_t *size,
               FebError *error);

/* png_rgba(): an RGBA PNG whose IDAT is zlib.compress(rows, 9). */
bool feb_png_rgba(int64_t width, int64_t height, const uint8_t *pixels, uint8_t **png,
                  size_t *size);
/* The same with another zlib level (zlib.compress(rows, level)). */
bool feb_png_rgba_level(int64_t width, int64_t height, const uint8_t *pixels, int level,
                        uint8_t **png, size_t *size);

#endif /* FUSION_EDITOR_BACKEND_GENERATED_H */
