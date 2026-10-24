/* SPDX-License-Identifier: GPL-3.0-only */
#include "output.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static bool replace_file(const char *temporary, const char *path) {
#ifdef _WIN32
    return MoveFileExA(temporary, path,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, path) == 0;
#endif
}

bool fusion_asset_write_atomic(const char *path, FusionAssetWriteBody write_body,
                               const void *context,
                               char *error, size_t error_size) {
    if (!path || !write_body) {
        set_error(error, error_size, "invalid native output request");
        return false;
    }
    size_t path_length = strlen(path);
    if (path_length > SIZE_MAX - 5) {
        set_error(error, error_size, "native output path is too long");
        return false;
    }
    char *temporary = malloc(path_length + 5);
    if (!temporary) {
        set_error(error, error_size, "cannot allocate native output path");
        return false;
    }
    snprintf(temporary, path_length + 5, "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    if (!file) {
        free(temporary);
        set_error(error, error_size, "cannot open native temporary output");
        return false;
    }
    bool written = write_body(file, context);
    bool flushed = fflush(file) == 0;
    bool closed = fclose(file) == 0;
    if (!written || !flushed || !closed || !replace_file(temporary, path)) {
        remove(temporary);
        free(temporary);
        set_error(error, error_size, "cannot atomically write native output");
        return false;
    }
    free(temporary);
    if (error && error_size > 0) error[0] = '\0';
    return true;
}
