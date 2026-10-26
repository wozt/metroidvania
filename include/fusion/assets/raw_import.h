/* SPDX-License-Identifier: GPL-3.0-only */
/* The raw ROM blocks identified by the pinned decompilation databases. */
#ifndef FUSION_ASSETS_RAW_IMPORT_H
#define FUSION_ASSETS_RAW_IMPORT_H

#include "fusion/assets/rom.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    size_t metroid_blocks;
    size_t aria_blocks;
    size_t bytes;
} FusionRawImportSummary;

/* Copies every block of both databases (JSON arrays of path, addr.us, count
 * and size records) below OUTPUT_ROOT/<world>/raw/ and writes
 * OUTPUT_ROOT/shared/manifests/import_catalog.json. A world's offsets are all
 * validated before any of its bytes are written; paths are restricted to
 * [A-Za-z0-9_.-] components and symlinks are refused. */
bool fusion_raw_import(const FusionRom *metroid, const FusionRom *aria,
                       const char *metroid_database, size_t metroid_database_size,
                       const char *aria_database, size_t aria_database_size,
                       const char *output_root, FusionRawImportSummary *summary,
                       char *error, size_t error_size);

/* Writes ROOT/RELATIVE atomically, creating its parent directories and
 * refusing symlinks and unsafe path components. */
bool fusion_write_private_file(const char *root, const char *relative,
                               const uint8_t *data, size_t size,
                               char *error, size_t error_size);

#endif /* FUSION_ASSETS_RAW_IMPORT_H */
