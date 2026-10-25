/* SPDX-License-Identifier: GPL-3.0-only */
/* Private multi-file exports: real directories, atomic writes that refuse
 * symlinks, and the export_index.tsv of scripts/export_index.py. */
#define _POSIX_C_SOURCE 200809L  /* lstat */

#include "export.h"

#include "output.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

static void set_error(char *error, size_t error_size, const char *format,
                      const char *detail) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, format, detail ? detail : "");
    error[error_size - 1] = '\0';
}

static bool is_symlink(const char *path) {
#ifdef _WIN32
    (void)path;
    return false;
#else
    struct stat info;
    return lstat(path, &info) == 0 && S_ISLNK(info.st_mode);
#endif
}

bool fusion_export_ensure_directory(const char *path, char *error,
                                    size_t error_size) {
    if (is_symlink(path)) {
        set_error(error, error_size, "symlink private asset component refused: %s",
                  path);
        return false;
    }
    struct stat info;
    if (stat(path, &info) == 0) {
        if (S_ISDIR(info.st_mode)) return true;
        set_error(error, error_size, "private asset parent is not a directory: %s",
                  path);
        return false;
    }
#ifdef _WIN32
    int made = _mkdir(path);
#else
    int made = mkdir(path, 0777);
#endif
    if (made != 0 && errno != EEXIST) {
        set_error(error, error_size, "cannot create private asset directory: %s",
                  path);
        return false;
    }
    return true;
}

bool fusion_export_join(char *output, const char *directory, const char *name) {
    int written = snprintf(output, FUSION_EXPORT_PATH_CAPACITY, "%s/%s", directory, name);
    return written > 0 && written < FUSION_EXPORT_PATH_CAPACITY;
}

typedef struct {
    const uint8_t *data;
    size_t size;
} ExportBytes;

static bool write_bytes(FILE *file, const void *context) {
    const ExportBytes *bytes = context;
    return fwrite(bytes->data, 1, bytes->size, file) == bytes->size;
}

bool fusion_export_write_file(const char *path, const uint8_t *data, size_t size,
                              char *error, size_t error_size) {
    char temporary[FUSION_EXPORT_PATH_CAPACITY + 8];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    if (is_symlink(path) || is_symlink(temporary)) {
        set_error(error, error_size, "symlink output refused: %s", path);
        return false;
    }
    ExportBytes bytes = {data, size};
    char detail[128];
    if (!fusion_asset_write_atomic(path, write_bytes, &bytes, detail,
                                   sizeof detail)) {
        set_error(error, error_size, "cannot write %s", path);
        return false;
    }
    return true;
}

static int compare_skipped(const void *left, const void *right) {
    const FusionExportSkip *a = left, *b = right;
    int order = strcmp(a->item, b->item);
    return order ? order : strcmp(a->reason, b->reason);
}

static int compare_exported(const void *left, const void *right) {
    return strcmp(((const FusionExportRoom *)left)->folder,
                  ((const FusionExportRoom *)right)->folder);
}

bool fusion_export_write_room_index(const char *output, FusionExportRoom *rooms,
                                    size_t room_count, FusionExportSkip *skipped,
                                    size_t skipped_count, char *error,
                                    size_t error_size) {
    qsort(rooms, room_count, sizeof *rooms, compare_exported);
    qsort(skipped, skipped_count, sizeof *skipped, compare_skipped);
    size_t capacity = 64 + room_count * 2 * 128 + skipped_count * 160;
    char *text = malloc(capacity);
    if (!text) {
        set_error(error, error_size, "cannot allocate the export index%s", NULL);
        return false;
    }
    size_t used = (size_t)snprintf(text, capacity,
                                   "schema\tmetroidvania-export-index-v1\n");
    for (size_t index = 0; index < room_count; ++index) {
        used += (size_t)snprintf(text + used, capacity - used,
                                 "file\t%s/background.bmp\t%s\n"
                                 "file\t%s/room.tsv\t%s\n",
                                 rooms[index].folder, rooms[index].sha_background,
                                 rooms[index].folder, rooms[index].sha_room);
    }
    for (size_t index = 0; index < skipped_count; ++index) {
        used += (size_t)snprintf(text + used, capacity - used, "skipped\t%s\t",
                                 skipped[index].item);
        bool space = false, started = false;
        for (const char *cursor = skipped[index].reason; *cursor; ++cursor) {
            if (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' ||
                *cursor == '\r' || *cursor == '\v' || *cursor == '\f') {
                space = started;
                continue;
            }
            if (space && used + 1 < capacity) text[used++] = ' ';
            if (used + 1 < capacity) text[used++] = *cursor;
            space = false;
            started = true;
        }
        if (used + 1 < capacity) text[used++] = '\n';
        text[used] = '\0';
    }
    char path[FUSION_EXPORT_PATH_CAPACITY];
    bool written = used < capacity &&
                   fusion_export_join(path, output, "export_index.tsv") &&
                   fusion_export_write_file(path, (const uint8_t *)text, used,
                                            error, error_size);
    free(text);
    return written;
}
