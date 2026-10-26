/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "internal.h"

#include "output.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char schema[] = "metroidvania-rebuild-state-v1";

static void set_error(char *error, size_t size, const char *message) {
    if (!error || !size) return;
    snprintf(error, size, "%s", message);
    error[size - 1] = '\0';
}

static bool read_file(const char *path, char **data, size_t *size,
                      char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error(error, error_size, "cannot read rebuild state");
        return false;
    }
    if (fseek(file, 0, SEEK_END) || ftell(file) < 0) {
        fclose(file);
        set_error(error, error_size, "cannot size rebuild state");
        return false;
    }
    long length = ftell(file);
    rewind(file);
    char *bytes = malloc((size_t)length + 1);
    if (!bytes || fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        set_error(error, error_size, "cannot read rebuild state");
        return false;
    }
    bytes[length] = '\0';
    fclose(file);
    *data = bytes;
    *size = (size_t)length;
    return true;
}

static bool default_state(FusionRebuildState *state) {
    state->document = fusion_json_object();
    return fusion_json_set(&state->document, "schema", fusion_json_string(schema)) &&
           fusion_json_set(&state->document, "tasks", fusion_json_object());
}

bool fusion_rebuild_state_load(const char *path, FusionRebuildState *state,
                               char *error, size_t error_size) {
    if (!path || !state) return false;
    *state = (FusionRebuildState){0};
    struct stat status;
    if (lstat(path, &status)) {
        if (errno != ENOENT) {
            set_error(error, error_size, "cannot inspect rebuild state");
            return false;
        }
        if (!default_state(state)) {
            set_error(error, error_size, "out of memory");
            return false;
        }
        return true;
    }
    if (S_ISLNK(status.st_mode) || !S_ISREG(status.st_mode)) {
        set_error(error, error_size, "rebuild state must be a regular file, not a symlink");
        return false;
    }
    char *data = NULL;
    size_t size = 0;
    if (!read_file(path, &data, &size, error, error_size)) {
        return false;
    }
    char parse_error[256] = "";
    bool parsed = fusion_json_parse(data, size, &state->document,
                                    parse_error, sizeof parse_error);
    free(data);
    if (!parsed) {
        if (error && error_size)
            snprintf(error, error_size, "cannot read rebuild state: %s", parse_error);
        return false;
    }
    const FusionJson *state_schema = fusion_json_member(&state->document, "schema");
    const FusionJson *tasks = fusion_json_member(&state->document, "tasks");
    if (state->document.kind != FUSION_JSON_OBJECT || !state_schema ||
        state_schema->kind != FUSION_JSON_STRING || strcmp(state_schema->string, schema) ||
        !tasks || tasks->kind != FUSION_JSON_OBJECT) {
        fusion_rebuild_state_free(state);
        set_error(error, error_size, "unsupported rebuild state schema");
        return false;
    }
    return true;
}

static int compare_members(const void *left, const void *right) {
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}

static bool sorted_clone(const FusionJson *source, FusionJson *copy) {
    if (source->kind != FUSION_JSON_OBJECT && source->kind != FUSION_JSON_ARRAY)
        return fusion_json_clone(source, copy);
    *copy = source->kind == FUSION_JSON_OBJECT ? fusion_json_object() : fusion_json_array();
    if (source->kind == FUSION_JSON_ARRAY) {
        for (size_t index = 0; index < source->count; ++index) {
            FusionJson item;
            if (!sorted_clone(&source->items[index], &item) ||
                !fusion_json_push(copy, item)) {
                fusion_json_free(copy);
                return false;
            }
        }
        return true;
    }
    const char **keys = malloc(source->count * sizeof *keys);
    if (source->count && !keys) return false;
    for (size_t index = 0; index < source->count; ++index) keys[index] = source->keys[index];
    qsort(keys, source->count, sizeof *keys, compare_members);
    for (size_t index = 0; index < source->count; ++index) {
        const FusionJson *value = fusion_json_member(source, keys[index]);
        FusionJson item;
        if (!sorted_clone(value, &item) || !fusion_json_set(copy, keys[index], item)) {
            free(keys);
            fusion_json_free(copy);
            return false;
        }
    }
    free(keys);
    return true;
}

typedef struct {
    const char *text;
    size_t size;
} WriteContext;

static bool write_body(FILE *file, const void *opaque) {
    const WriteContext *context = opaque;
    return fwrite(context->text, 1, context->size, file) == context->size &&
           fputc('\n', file) != EOF;
}

static bool ensure_parent_directory(const char *path, char *error,
                                    size_t error_size) {
    char *copy = strdup(path);
    if (!copy) {
        set_error(error, error_size, "out of memory");
        return false;
    }
    char *last = strrchr(copy, '/');
    if (!last) {
        free(copy);
        return true;
    }
    if (last == copy) {
        free(copy);
        return true;
    }
    *last = '\0';
    char *cursor = copy + (copy[0] == '/');
    for (;;) {
        char *slash = strchr(cursor, '/');
        if (slash) *slash = '\0';
        struct stat status;
        if (lstat(copy, &status)) {
            if (errno != ENOENT || mkdir(copy, 0755)) {
                free(copy);
                set_error(error, error_size, "cannot create rebuild state directory");
                return false;
            }
        } else if (S_ISLNK(status.st_mode) || !S_ISDIR(status.st_mode)) {
            free(copy);
            set_error(error, error_size,
                      "rebuild state parent must not contain a symlink");
            return false;
        }
        if (!slash) break;
        *slash = '/';
        cursor = slash + 1;
    }
    free(copy);
    return true;
}

static bool reject_symlink_output(const char *path, char *error,
                                  size_t error_size) {
    struct stat status;
    if (!lstat(path, &status)) {
        if (S_ISLNK(status.st_mode)) {
            set_error(error, error_size, "rebuild state output must not be a symlink");
            return false;
        }
    } else if (errno != ENOENT) {
        set_error(error, error_size, "cannot inspect rebuild state output");
        return false;
    }
    size_t length = strlen(path);
    char *temporary = malloc(length + 5);
    if (!temporary) {
        set_error(error, error_size, "out of memory");
        return false;
    }
    snprintf(temporary, length + 5, "%s.tmp", path);
    bool safe = lstat(temporary, &status) != 0;
    if (!safe && S_ISLNK(status.st_mode))
        set_error(error, error_size,
                  "rebuild temporary state output must not be a symlink");
    else if (!safe && !S_ISREG(status.st_mode))
        set_error(error, error_size,
                  "rebuild temporary state output must be a regular file");
    else if (safe && errno != ENOENT) {
        safe = false;
        set_error(error, error_size, "cannot inspect rebuild temporary state output");
    } else safe = true;
    free(temporary);
    return safe;
}

bool fusion_rebuild_state_save(const char *path, const FusionRebuildState *state,
                               char *error, size_t error_size) {
    FusionJson sorted = {0};
    char *text = NULL;
    size_t size = 0;
    if (!path || !state) {
        set_error(error, error_size, "invalid rebuild state output");
        return false;
    }
    if (!ensure_parent_directory(path, error, error_size) ||
        !reject_symlink_output(path, error, error_size)) return false;
    if (!sorted_clone(&state->document, &sorted) ||
        !fusion_json_write(&sorted, 2, &text, &size)) {
        fusion_json_free(&sorted);
        free(text);
        set_error(error, error_size, "cannot serialize rebuild state");
        return false;
    }
    fusion_json_free(&sorted);
    WriteContext context = {text, size};
    bool ok = fusion_asset_write_atomic(path, write_body, &context,
                                        error, error_size);
    free(text);
    return ok;
}

void fusion_rebuild_state_free(FusionRebuildState *state) {
    if (!state) return;
    fusion_json_free(&state->document);
    *state = (FusionRebuildState){0};
}
