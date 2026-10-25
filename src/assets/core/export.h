/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ASSETS_CORE_EXPORT_H
#define FUSION_ASSETS_CORE_EXPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { FUSION_EXPORT_PATH_CAPACITY = 1024 };

/* One exported room folder: its room.tsv and background.bmp digests. */
typedef struct {
    char folder[40];
    char sha_room[65];
    char sha_background[65];
} FusionExportRoom;

typedef struct {
    char item[40];
    char reason[96];
} FusionExportSkip;

/* private_path(create=True): a real directory, created when missing; a
 * symlink or a file in its place is refused. */
bool fusion_export_ensure_directory(const char *path, char *error,
                                    size_t error_size);
/* DIRECTORY/NAME into a FUSION_EXPORT_PATH_CAPACITY buffer. */
bool fusion_export_join(char *output, const char *directory, const char *name);
/* sprite_library.write_atomic: no symlinked target or temporary. */
bool fusion_export_write_file(const char *path, const uint8_t *data,
                              size_t size, char *error, size_t error_size);
/* export_index.write: every room.tsv and background.bmp sorted by path,
 * then the skipped rooms sorted, each reason's whitespace collapsed. Sorts
 * both arrays in place. */
bool fusion_export_write_room_index(const char *output, FusionExportRoom *rooms,
                                    size_t room_count, FusionExportSkip *skipped,
                                    size_t skipped_count, char *error,
                                    size_t error_size);

#endif /* FUSION_ASSETS_CORE_EXPORT_H */
