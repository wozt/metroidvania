/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L

#include "fusion/pipeline/rebuild.h"

#include "internal.h"
#include "sha256.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} PathList;

static bool set_error(char *error, size_t size, const char *format, ...) {
    if (error && size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, size, format, arguments);
        va_end(arguments);
        error[size - 1] = '\0';
    }
    return false;
}

static char *copy_string(const char *text) {
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy) memcpy(copy, text, size);
    return copy;
}

static bool path_add(PathList *paths, const char *path) {
    if (paths->count == paths->capacity) {
        size_t capacity = paths->capacity ? paths->capacity * 2 : 32;
        char **items = realloc(paths->items, capacity * sizeof *items);
        if (!items) return false;
        paths->items = items;
        paths->capacity = capacity;
    }
    paths->items[paths->count] = copy_string(path);
    if (!paths->items[paths->count]) return false;
    ++paths->count;
    return true;
}

static void paths_free(PathList *paths) {
    for (size_t index = 0; index < paths->count; ++index) free(paths->items[index]);
    free(paths->items);
    *paths = (PathList){0};
}

static int compare_paths(const void *left, const void *right) {
    const char *const *a = left;
    const char *const *b = right;
    const char *ac = *a, *bc = *b;
    for (;;) {
        const char *ae = strchr(ac, '/');
        const char *be = strchr(bc, '/');
        size_t al = ae ? (size_t)(ae - ac) : strlen(ac);
        size_t bl = be ? (size_t)(be - bc) : strlen(bc);
        size_t common = al < bl ? al : bl;
        int comparison = memcmp(ac, bc, common);
        if (comparison) return comparison;
        if (al != bl) return al < bl ? -1 : 1;
        if (!ae || !be) return ae ? 1 : be ? -1 : 0;
        ac = ae + 1;
        bc = be + 1;
    }
}

static void paths_sort_unique(PathList *paths) {
    qsort(paths->items, paths->count, sizeof *paths->items, compare_paths);
    size_t kept = 0;
    for (size_t index = 0; index < paths->count; ++index) {
        if (kept && !strcmp(paths->items[kept - 1], paths->items[index])) {
            free(paths->items[index]);
            continue;
        }
        paths->items[kept++] = paths->items[index];
    }
    paths->count = kept;
}

static bool join_path(char *output, size_t size, const char *root,
                      const char *relative) {
    if (!root || !relative || relative[0] == '/' ||
        (isalpha((unsigned char)relative[0]) && relative[1] == ':'))
        return false;
    int length = snprintf(output, size, "%s%s%s", root,
                          root[0] && root[strlen(root) - 1] == '/' ? "" : "/",
                          relative);
    return length >= 0 && (size_t)length < size;
}

static bool is_regular(const char *path) {
    struct stat status;
    return !stat(path, &status) && S_ISREG(status.st_mode);
}

static bool has_source_suffix(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return false;
    char suffix[8];
    size_t length = strlen(dot);
    if (length >= sizeof suffix) return false;
    for (size_t index = 0; index <= length; ++index)
        suffix[index] = (char)tolower((unsigned char)dot[index]);
    return !strcmp(suffix, ".c") || !strcmp(suffix, ".h") ||
           !strcmp(suffix, ".s") || !strcmp(suffix, ".inc");
}

static bool collect_sources(const char *root, const char *relative,
                            PathList *paths, char *error, size_t error_size) {
    char absolute[4096];
    if (!join_path(absolute, sizeof absolute, root, relative))
        return set_error(error, error_size, "source path is too long");
    DIR *directory = opendir(absolute);
    if (!directory)
        return set_error(error, error_size, "cannot scan source directory: %s", relative);
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child_relative[4096], child_absolute[4096];
        int length = snprintf(child_relative, sizeof child_relative, "%s/%s",
                              relative, entry->d_name);
        if (length < 0 || (size_t)length >= sizeof child_relative ||
            !join_path(child_absolute, sizeof child_absolute, root, child_relative)) {
            closedir(directory);
            return set_error(error, error_size, "source path is too long");
        }
        struct stat status;
        if (lstat(child_absolute, &status)) {
            closedir(directory);
            return set_error(error, error_size, "cannot inspect source: %s", child_relative);
        }
        if (S_ISLNK(status.st_mode)) continue;
        if (S_ISDIR(status.st_mode)) {
            if (!collect_sources(root, child_relative, paths, error, error_size)) {
                closedir(directory);
                return false;
            }
        } else if (S_ISREG(status.st_mode) && has_source_suffix(child_relative) &&
                   !path_add(paths, child_relative)) {
            closedir(directory);
            return set_error(error, error_size, "out of memory");
        }
    }
    closedir(directory);
    return true;
}

static bool read_file(const char *path, char **data, size_t *size,
                      char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (!file) return set_error(error, error_size, "cannot read %s", path);
    if (fseek(file, 0, SEEK_END)) {
        fclose(file);
        return set_error(error, error_size, "cannot size %s", path);
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return set_error(error, error_size, "cannot size %s", path);
    }
    char *bytes = malloc((size_t)length + 1);
    if (!bytes || fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        return set_error(error, error_size, "cannot read %s", path);
    }
    fclose(file);
    bytes[length] = '\0';
    *data = bytes;
    *size = (size_t)length;
    return true;
}

static bool add_inventory_outputs(const char *root, PathList *paths,
                                  char *error, size_t error_size) {
    const char *manifest_path = "data/native_parity/inventory.json";
    if (!path_add(paths, manifest_path)) return set_error(error, error_size, "out of memory");
    char absolute[4096];
    if (!join_path(absolute, sizeof absolute, root, manifest_path))
        return set_error(error, error_size, "inventory path is too long");
    if (!is_regular(absolute)) return true;
    char *text = NULL;
    size_t size = 0;
    FusionJson manifest = {0};
    if (!read_file(absolute, &text, &size, error, error_size)) return false;
    bool parsed = fusion_json_parse(text, size, &manifest, error, error_size);
    free(text);
    if (!parsed) return false;
    const FusionJson *games = fusion_json_member(&manifest, "games");
    if (!games || games->kind != FUSION_JSON_OBJECT) {
        fusion_json_free(&manifest);
        return set_error(error, error_size, "invalid native inventory manifest");
    }
    for (size_t game = 0; game < games->count; ++game) {
        const FusionJson *files = fusion_json_member(&games->items[game], "record_files");
        if (!files || files->kind != FUSION_JSON_ARRAY) continue;
        for (size_t item = 0; item < files->count; ++item) {
            const FusionJson *path = fusion_json_member(&files->items[item], "path");
            if (!path || path->kind != FUSION_JSON_STRING ||
                strchr(path->string, '/') || strchr(path->string, '\\')) {
                fusion_json_free(&manifest);
                return set_error(error, error_size, "unsafe native inventory shard path");
            }
            char relative[4096];
            int length = snprintf(relative, sizeof relative, "data/native_parity/%s",
                                  path->string);
            if (length < 0 || (size_t)length >= sizeof relative ||
                !path_add(paths, relative)) {
                fusion_json_free(&manifest);
                return set_error(error, error_size, "out of memory");
            }
        }
    }
    fusion_json_free(&manifest);
    return true;
}

static bool task_inputs(const char *root, const FusionRebuildTask *task,
                        PathList *paths, char *error, size_t error_size) {
    for (size_t index = 0; index < task->input_count; ++index)
        if (!path_add(paths, task->inputs[index]))
            return set_error(error, error_size, "out of memory");
    if (task->path_kind == FUSION_REBUILD_PATHS_INVENTORY) {
        if (!collect_sources(root, "third_party/cvaos", paths, error, error_size) ||
            !collect_sources(root, "third_party/mzm", paths, error, error_size))
            return false;
        paths_sort_unique(paths);
    } else if (task->path_kind == FUSION_REBUILD_PATHS_CHECKLIST) {
        if (!add_inventory_outputs(root, paths, error, error_size)) return false;
        paths_sort_unique(paths);
    }
    return true;
}

static bool task_outputs(const char *root, const FusionRebuildTask *task,
                         PathList *paths, char *error, size_t error_size) {
    if (task->path_kind == FUSION_REBUILD_PATHS_INVENTORY)
        return add_inventory_outputs(root, paths, error, error_size);
    for (size_t index = 0; index < task->output_count; ++index)
        if (!path_add(paths, task->outputs[index]))
            return set_error(error, error_size, "out of memory");
    return true;
}

static void digest_hex(const uint8_t digest[32], char output[65]) {
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < 32; ++index) {
        output[index * 2] = digits[digest[index] >> 4];
        output[index * 2 + 1] = digits[digest[index] & 15];
    }
    output[64] = '\0';
}

static bool digest_file(FusionSha256 *digest, const char *absolute,
                        char *error, size_t error_size) {
    FILE *file = fopen(absolute, "rb");
    if (!file) return set_error(error, error_size, "missing input: %s", absolute);
    uint8_t buffer[65536];
    size_t count;
    while ((count = fread(buffer, 1, sizeof buffer, file)) != 0)
        fusion_sha256_update(digest, buffer, count);
    bool ok = !ferror(file);
    fclose(file);
    return ok ? true : set_error(error, error_size, "cannot read input: %s", absolute);
}

static bool file_sha256(const char *path, char output[65],
                        char *error, size_t error_size) {
    FusionSha256 context;
    uint8_t digest[32];
    fusion_sha256_init(&context);
    if (!digest_file(&context, path, error, error_size)) return false;
    fusion_sha256_final(&context, digest);
    digest_hex(digest, output);
    return true;
}

static bool command_output(const char *command, char **output, size_t *size,
                           char *error, size_t error_size) {
    FILE *pipe = popen(command, "r");
    if (!pipe) return set_error(error, error_size, "cannot run Git for rebuild stamp");
    size_t capacity = 256, used = 0;
    char *bytes = malloc(capacity);
    if (!bytes) {
        pclose(pipe);
        return set_error(error, error_size, "out of memory");
    }
    for (;;) {
        if (used == capacity) {
            capacity *= 2;
            char *grown = realloc(bytes, capacity);
            if (!grown) {
                free(bytes);
                pclose(pipe);
                return set_error(error, error_size, "out of memory");
            }
            bytes = grown;
        }
        size_t count = fread(bytes + used, 1, capacity - used, pipe);
        used += count;
        if (!count) break;
    }
    int status = pclose(pipe);
    if (status != 0) {
        free(bytes);
        return set_error(error, error_size, "Git failed while reading rebuild stamp");
    }
    *output = bytes;
    *size = used;
    return true;
}

static bool decomp_stamp(const char *root, const char *game, const char *folder,
                         FusionSha256 *digest, char *error, size_t error_size) {
    if (strchr(root, '"')) return set_error(error, error_size, "unsafe project root");
    char command[8192];
    int length = snprintf(command, sizeof command,
                          "git -C \"%s/%s\" rev-parse HEAD", root, folder);
    if (length < 0 || (size_t)length >= sizeof command)
        return set_error(error, error_size, "project path is too long");
    char *revision = NULL, *status = NULL;
    size_t revision_size = 0, status_size = 0;
    if (!command_output(command, &revision, &revision_size, error, error_size)) return false;
    while (revision_size && isspace((unsigned char)revision[revision_size - 1]))
        --revision_size;
    char stamp[256];
    length = snprintf(stamp, sizeof stamp, "%s:%.*s", game,
                      (int)revision_size, revision);
    free(revision);
    if (length < 0 || (size_t)length >= sizeof stamp)
        return set_error(error, error_size, "invalid Git revision");
    fusion_sha256_update(digest, (const uint8_t *)stamp, (size_t)length);
    fusion_sha256_update(digest, (const uint8_t *)"\n", 1);

    length = snprintf(command, sizeof command,
                      "git -C \"%s/%s\" status --porcelain --untracked-files=no",
                      root, folder);
    if (length < 0 || (size_t)length >= sizeof command ||
        !command_output(command, &status, &status_size, error, error_size))
        return false;
    uint8_t status_digest[32];
    char status_hex[65];
    fusion_sha256_digest((const uint8_t *)status, status_size, status_digest);
    free(status);
    digest_hex(status_digest, status_hex);
    fusion_sha256_update(digest, (const uint8_t *)status_hex, 64);
    fusion_sha256_update(digest, (const uint8_t *)"\n", 1);
    return true;
}

static bool input_digest(const char *root, const FusionRebuildTask *task,
                         const char *const *dependency_digests,
                         char output[65], size_t *input_count,
                         char *error, size_t error_size) {
    FusionSha256 context;
    fusion_sha256_init(&context);
    char heading[256];
    int length = snprintf(heading, sizeof heading, "%s:%u\n", task->name, task->version);
    if (length < 0 || (size_t)length >= sizeof heading)
        return set_error(error, error_size, "invalid rebuild task name");
    fusion_sha256_update(&context, (const uint8_t *)heading, (size_t)length);
    for (size_t index = 0; index < task->dependency_count; ++index) {
        fusion_sha256_update(&context, (const uint8_t *)dependency_digests[index],
                             strlen(dependency_digests[index]));
        fusion_sha256_update(&context, (const uint8_t *)"\n", 1);
    }
    if ((task->stamp_games & FUSION_REBUILD_STAMP_METROID) &&
        !decomp_stamp(root, "metroid", "third_party/mzm", &context,
                      error, error_size))
        return false;
    if ((task->stamp_games & FUSION_REBUILD_STAMP_ARIA) &&
        !decomp_stamp(root, "aria", "third_party/cvaos", &context,
                      error, error_size))
        return false;
    PathList inputs = {0};
    if (!task_inputs(root, task, &inputs, error, error_size)) {
        paths_free(&inputs);
        return false;
    }
    if (input_count) *input_count = inputs.count;
    for (size_t index = 0; index < inputs.count; ++index) {
        char absolute[4096];
        if (!join_path(absolute, sizeof absolute, root, inputs.items[index])) {
            paths_free(&inputs);
            return set_error(error, error_size, "input path is too long");
        }
        fusion_sha256_update(&context, (const uint8_t *)inputs.items[index],
                             strlen(inputs.items[index]));
        fusion_sha256_update(&context, (const uint8_t *)"\0", 1);
        if (!digest_file(&context, absolute, error, error_size)) {
            paths_free(&inputs);
            return false;
        }
        fusion_sha256_update(&context, (const uint8_t *)"\0", 1);
    }
    paths_free(&inputs);
    uint8_t bytes[32];
    fusion_sha256_final(&context, bytes);
    digest_hex(bytes, output);
    return true;
}

static bool output_digest(const char *root, const FusionRebuildTask *task,
                          char output[65], bool *exists, size_t *output_count,
                          char *error, size_t error_size) {
    PathList outputs = {0};
    *exists = false;
    if (!task_outputs(root, task, &outputs, error, error_size)) {
        paths_free(&outputs);
        return false;
    }
    paths_sort_unique(&outputs);
    if (output_count) *output_count = outputs.count;
    FusionSha256 context;
    fusion_sha256_init(&context);
    for (size_t index = 0; index < outputs.count; ++index) {
        char absolute[4096];
        if (!join_path(absolute, sizeof absolute, root, outputs.items[index])) {
            paths_free(&outputs);
            return set_error(error, error_size, "output path is too long");
        }
        if (!is_regular(absolute)) {
            paths_free(&outputs);
            return true;
        }
        fusion_sha256_update(&context, (const uint8_t *)outputs.items[index],
                             strlen(outputs.items[index]));
        fusion_sha256_update(&context, (const uint8_t *)"\0", 1);
        if (!digest_file(&context, absolute, error, error_size)) {
            paths_free(&outputs);
            return false;
        }
        fusion_sha256_update(&context, (const uint8_t *)"\0", 1);
    }
    paths_free(&outputs);
    uint8_t bytes[32];
    fusion_sha256_final(&context, bytes);
    digest_hex(bytes, output);
    *exists = true;
    return true;
}

static bool safe_relative(const char *path) {
    if (!path[0] || path[0] == '/' || path[0] == '\\' || strchr(path, ':')) return false;
    const char *part = path;
    for (const char *cursor = path;; ++cursor) {
        if (*cursor != '/' && *cursor != '\\' && *cursor != '\0') continue;
        size_t length = (size_t)(cursor - part);
        if (!length || (length == 1 && part[0] == '.') ||
            (length == 2 && part[0] == '.' && part[1] == '.')) return false;
        if (!*cursor) break;
        part = cursor + 1;
    }
    return true;
}

static size_t split_tabs(char *line, char **fields, size_t capacity) {
    size_t count = 0;
    char *cursor = line;
    while (count < capacity) {
        fields[count++] = cursor;
        char *tab = strchr(cursor, '\t');
        if (!tab) break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static bool validate_index(const char *root, const char *relative,
                           FusionRebuildValidation validation,
                           bool *valid, char *error, size_t error_size) {
    char absolute[4096];
    if (!join_path(absolute, sizeof absolute, root, relative))
        return set_error(error, error_size, "index path is too long");
    char *text = NULL;
    size_t size = 0;
    *valid = false;
    if (!is_regular(absolute)) return true;
    if (!read_file(absolute, &text, &size, error, error_size)) return false;
    const char *expected = validation == FUSION_REBUILD_VALIDATE_EXPORT_INDEX
                               ? "schema\tmetroidvania-export-index-v1"
                               : "schema\tmetroidvania-sprite-index-v1";
    char *line = text;
    char *newline = strchr(line, '\n');
    if (!newline) {
        free(text);
        return true;
    }
    *newline = '\0';
    if (strcmp(line, expected)) {
        free(text);
        return true;
    }
    char base[4096];
    snprintf(base, sizeof base, "%s", relative);
    char *slash = strrchr(base, '/');
    if (slash) *slash = '\0';
    for (line = newline + 1; line < text + size && *line;) {
        newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        if (*line) {
            char *fields[6] = {0};
            size_t count = split_tabs(line, fields, 6);
            if (validation == FUSION_REBUILD_VALIDATE_EXPORT_INDEX &&
                count == 3 && !strcmp(fields[0], "skipped")) {
                /* No file belongs to a documented skipped item. */
            } else {
                const char *path = NULL, *expected_hash = NULL;
                if (validation == FUSION_REBUILD_VALIDATE_EXPORT_INDEX) {
                    if (count != 3 || strcmp(fields[0], "file")) {
                        free(text);
                        return true;
                    }
                    path = fields[1];
                    expected_hash = fields[2];
                } else {
                    if (count != 6) {
                        free(text);
                        return true;
                    }
                    path = fields[5];
                }
                if (!safe_relative(path)) {
                    free(text);
                    return true;
                }
                char file_relative[4096];
                int length = validation == FUSION_REBUILD_VALIDATE_EXPORT_INDEX
                                 ? snprintf(file_relative, sizeof file_relative,
                                            "%s/%s", base, path)
                                 : snprintf(file_relative, sizeof file_relative, "%s", path);
                char file_absolute[4096], actual[65];
                if (length < 0 || (size_t)length >= sizeof file_relative ||
                    !join_path(file_absolute, sizeof file_absolute, root, file_relative) ||
                    !is_regular(file_absolute)) {
                    free(text);
                    return true;
                }
                if (!file_sha256(file_absolute, actual, error, error_size)) {
                    free(text);
                    return false;
                }
                if (validation == FUSION_REBUILD_VALIDATE_SPRITE_INDEX) {
                    const char *name = strrchr(path, '/');
                    name = name ? name + 1 : path;
                    const char *dot = strrchr(name, '.');
                    if (!dot || (size_t)(dot - name) != 64 ||
                        strncmp(actual, name, 64)) {
                        free(text);
                        return true;
                    }
                } else if (strcmp(actual, expected_hash)) {
                    free(text);
                    return true;
                }
            }
        }
        if (!newline) break;
        line = newline + 1;
    }
    free(text);
    *valid = true;
    return true;
}

static bool validate_outputs(const char *root, const FusionRebuildTask *task,
                             bool *valid, char *error, size_t error_size) {
    *valid = true;
    if (task->validation == FUSION_REBUILD_VALIDATE_NONE) return true;
    for (size_t index = 0; index < task->output_count; ++index) {
        const char *path = task->outputs[index];
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;
        bool is_index = task->validation == FUSION_REBUILD_VALIDATE_EXPORT_INDEX
                            ? !strcmp(name, "export_index.tsv")
                            : !strcmp(name, "runtime_index.tsv");
        if (!is_index) continue;
        bool index_valid = false;
        if (!validate_index(root, path, task->validation, &index_valid,
                            error, error_size))
            return false;
        if (!index_valid) {
            *valid = false;
            return true;
        }
    }
    return true;
}

static ptrdiff_t task_index(const FusionRebuildTask *tasks, size_t count,
                            const char *name) {
    for (size_t index = 0; index < count; ++index)
        if (!strcmp(tasks[index].name, name)) return (ptrdiff_t)index;
    return -1;
}

bool fusion_rebuild_plan_tasks(const char *root, const char *state_path,
                               const FusionRebuildTask *tasks, size_t task_count,
                               const char *const *selected, size_t selected_count,
                               bool force, FusionRebuildPlan *plan,
                               char *error, size_t error_size) {
    if (plan) *plan = (FusionRebuildPlan){0};
    if (!root || !state_path || !plan)
        return set_error(error, error_size, "invalid rebuild plan arguments");
    FusionRebuildOrder order;
    if (!fusion_rebuild_order_tasks(tasks, task_count, selected, selected_count,
                                    &order, error, error_size))
        return false;
    char state_absolute[4096];
    if (state_path[0] == '/') {
        int length = snprintf(state_absolute, sizeof state_absolute, "%s", state_path);
        if (length < 0 || (size_t)length >= sizeof state_absolute) {
            fusion_rebuild_order_free(&order);
            return set_error(error, error_size, "state path is too long");
        }
    } else if (!join_path(state_absolute, sizeof state_absolute, root, state_path)) {
        fusion_rebuild_order_free(&order);
        return set_error(error, error_size, "state path is too long");
    }
    FusionRebuildState state;
    if (!fusion_rebuild_state_load(state_absolute, &state, error, error_size)) {
        fusion_rebuild_order_free(&order);
        return false;
    }
    bool *pending = calloc(task_count, sizeof *pending);
    bool *skipped = calloc(task_count, sizeof *skipped);
    char (*digests)[65] = calloc(task_count, sizeof *digests);
    FusionRebuildPlanEntry *entries = calloc(order.count, sizeof *entries);
    if (!pending || !skipped || !digests || !entries) {
        free(pending); free(skipped); free(digests); free(entries);
        fusion_rebuild_state_free(&state);
        fusion_rebuild_order_free(&order);
        return set_error(error, error_size, "out of memory");
    }
    const FusionJson *state_tasks = fusion_json_member(&state.document, "tasks");
    bool ok = true;
    for (size_t position = 0; position < order.count && ok; ++position) {
        const FusionRebuildTask *task = order.items[position];
        ptrdiff_t index_value = task_index(tasks, task_count, task->name);
        size_t index = (size_t)index_value;
        FusionRebuildPlanEntry *entry = &entries[position];
        entry->task = task;
        const char *missing = NULL;
        for (size_t item = 0; item < task->requirement_count; ++item) {
            char absolute[4096];
            if (!join_path(absolute, sizeof absolute, root, task->requirements[item]) ||
                !is_regular(absolute)) {
                missing = task->requirements[item];
                break;
            }
        }
        bool dependency_skipped = false, dependency_pending = false;
        const char *dependency_digests[16] = {0};
        for (size_t item = 0; item < task->dependency_count; ++item) {
            ptrdiff_t dependency = task_index(tasks, task_count, task->dependencies[item]);
            if (dependency < 0) {
                ok = set_error(error, error_size, "%s: unknown dependency", task->name);
                break;
            }
            dependency_digests[item] = digests[dependency];
            dependency_skipped |= skipped[dependency];
            dependency_pending |= pending[dependency];
        }
        if (!ok) break;
        if (missing) {
            skipped[index] = true;
            entry->status = FUSION_REBUILD_SKIPPED_MISSING;
            snprintf(entry->detail, sizeof entry->detail, "missing %s", missing);
            continue;
        }
        if (dependency_skipped) {
            skipped[index] = true;
            entry->status = FUSION_REBUILD_SKIPPED_DEPENDENCY;
            snprintf(entry->detail, sizeof entry->detail, "dependency skipped");
            continue;
        }
        if (dependency_pending) {
            pending[index] = true;
            entry->status = FUSION_REBUILD_WOULD_REBUILD;
            continue;
        }
        if (!input_digest(root, task, dependency_digests, digests[index],
                          &entry->input_count,
                          error, error_size)) {
            ok = false;
            break;
        }
        snprintf(entry->input_digest, sizeof entry->input_digest, "%s", digests[index]);
        char current_output[65] = "";
        bool outputs_exist = false, outputs_valid = true;
        if (!output_digest(root, task, current_output, &outputs_exist,
                           &entry->output_count,
                           error, error_size) ||
            !validate_outputs(root, task, &outputs_valid, error, error_size)) {
            ok = false;
            break;
        }
        if (outputs_exist)
            snprintf(entry->output_digest, sizeof entry->output_digest, "%s",
                     current_output);
        const FusionJson *record = fusion_json_member(state_tasks, task->name);
        const FusionJson *recorded_digest = fusion_json_member(record, "digest");
        const FusionJson *recorded_output = fusion_json_member(record, "output_digest");
        bool rebuild = force || !recorded_digest ||
                       recorded_digest->kind != FUSION_JSON_STRING ||
                       strcmp(recorded_digest->string, digests[index]) ||
                       !outputs_exist || !recorded_output ||
                       recorded_output->kind != FUSION_JSON_STRING ||
                       strcmp(recorded_output->string, current_output) || !outputs_valid;
        pending[index] = rebuild;
        entry->status = rebuild ? FUSION_REBUILD_WOULD_REBUILD
                                : FUSION_REBUILD_UP_TO_DATE;
    }
    if (ok) {
        plan->entries = entries;
        plan->count = order.count;
    } else free(entries);
    free(pending);
    free(skipped);
    free(digests);
    fusion_rebuild_state_free(&state);
    fusion_rebuild_order_free(&order);
    return ok;
}

bool fusion_rebuild_plan(const char *root, const char *state_path,
                         const char *const *selected, size_t selected_count,
                         bool force, FusionRebuildPlan *plan,
                         char *error, size_t error_size) {
    size_t task_count = 0;
    const FusionRebuildTask *tasks = fusion_rebuild_tasks(&task_count);
    return fusion_rebuild_plan_tasks(root, state_path, tasks, task_count,
                                     selected, selected_count, force,
                                     plan, error, error_size);
}

static bool resolve_state_path(const char *root, const char *state_path,
                               char absolute[4096], char *error, size_t error_size) {
    if (state_path[0] == '/') {
        int length = snprintf(absolute, 4096, "%s", state_path);
        return length >= 0 && length < 4096
                   ? true : set_error(error, error_size, "state path is too long");
    }
    return join_path(absolute, 4096, root, state_path)
               ? true : set_error(error, error_size, "state path is too long");
}

static bool update_state_record(FusionRebuildState *state,
                                const FusionRebuildTask *task,
                                const char *input_hash, const char *output_hash,
                                const PathList *outputs,
                                char *error, size_t error_size) {
    FusionJson *tasks = fusion_json_member_mut(&state->document, "tasks");
    const FusionJson *existing = fusion_json_member(tasks, task->name);
    FusionJson record;
    if (existing && existing->kind == FUSION_JSON_OBJECT) {
        if (!fusion_json_clone(existing, &record))
            return set_error(error, error_size, "out of memory");
    } else record = fusion_json_object();
    FusionJson output_list = fusion_json_array();
    for (size_t index = 0; index < outputs->count; ++index) {
        FusionJson path = fusion_json_string(outputs->items[index]);
        if (!path.string || !fusion_json_push(&output_list, path)) {
            fusion_json_free(&output_list);
            fusion_json_free(&record);
            return set_error(error, error_size, "out of memory");
        }
    }
    if (!fusion_json_set(&record, "digest", fusion_json_string(input_hash)) ||
        !fusion_json_set(&record, "output_digest", fusion_json_string(output_hash))) {
        fusion_json_free(&output_list);
        fusion_json_free(&record);
        return set_error(error, error_size, "out of memory");
    }
    if (!fusion_json_set(&record, "outputs", output_list) ||
        !fusion_json_set(&record, "version", fusion_json_int(task->version))) {
        fusion_json_free(&record);
        return set_error(error, error_size, "out of memory");
    }
    if (!fusion_json_set(tasks, task->name, record))
        return set_error(error, error_size, "out of memory");
    return true;
}

bool fusion_rebuild_execute_tasks(const char *root, const char *state_path,
                                  const FusionRebuildTask *tasks, size_t task_count,
                                  const char *const *selected, size_t selected_count,
                                  bool force, FusionRebuildActionRunner runner,
                                  void *runner_user, FusionRebuildPlan *result,
                                  char *error, size_t error_size) {
    if (result) *result = (FusionRebuildPlan){0};
    if (!root || !state_path || !tasks || !runner || !result)
        return set_error(error, error_size, "invalid rebuild execution arguments");
    FusionRebuildOrder order;
    if (!fusion_rebuild_order_tasks(tasks, task_count, selected, selected_count,
                                    &order, error, error_size)) return false;
    char state_absolute[4096];
    if (!resolve_state_path(root, state_path, state_absolute, error, error_size)) {
        fusion_rebuild_order_free(&order);
        return false;
    }
    FusionRebuildState state;
    if (!fusion_rebuild_state_load(state_absolute, &state, error, error_size)) {
        fusion_rebuild_order_free(&order);
        return false;
    }
    bool *skipped = calloc(task_count, sizeof *skipped);
    char (*digests)[65] = calloc(task_count, sizeof *digests);
    FusionRebuildPlanEntry *entries = calloc(order.count, sizeof *entries);
    if (!skipped || !digests || !entries) {
        free(skipped); free(digests); free(entries);
        fusion_rebuild_state_free(&state);
        fusion_rebuild_order_free(&order);
        return set_error(error, error_size, "out of memory");
    }
    bool ok = true;
    for (size_t position = 0; position < order.count && ok; ++position) {
        const FusionRebuildTask *task = order.items[position];
        ptrdiff_t index_value = task_index(tasks, task_count, task->name);
        size_t index = (size_t)index_value;
        FusionRebuildPlanEntry *entry = &entries[position];
        entry->task = task;
        const char *missing = NULL;
        for (size_t item = 0; item < task->requirement_count; ++item) {
            char absolute[4096];
            if (!join_path(absolute, sizeof absolute, root, task->requirements[item]) ||
                !is_regular(absolute)) {
                missing = task->requirements[item];
                break;
            }
        }
        bool dependency_skipped = false;
        const char *dependency_digests[16] = {0};
        for (size_t item = 0; item < task->dependency_count; ++item) {
            ptrdiff_t dependency = task_index(tasks, task_count, task->dependencies[item]);
            if (dependency < 0) {
                ok = set_error(error, error_size, "%s: unknown dependency", task->name);
                break;
            }
            dependency_digests[item] = digests[dependency];
            dependency_skipped |= skipped[dependency];
        }
        if (!ok) break;
        if (missing) {
            skipped[index] = true;
            entry->status = FUSION_REBUILD_SKIPPED_MISSING;
            snprintf(entry->detail, sizeof entry->detail, "missing %s", missing);
            continue;
        }
        if (dependency_skipped) {
            skipped[index] = true;
            entry->status = FUSION_REBUILD_SKIPPED_DEPENDENCY;
            snprintf(entry->detail, sizeof entry->detail, "dependency skipped");
            continue;
        }
        if (!input_digest(root, task, dependency_digests, digests[index],
                          &entry->input_count, error, error_size)) {
            ok = false;
            break;
        }
        snprintf(entry->input_digest, sizeof entry->input_digest, "%s", digests[index]);
        char current_output[65] = "";
        bool outputs_exist = false, outputs_valid = true;
        if (!output_digest(root, task, current_output, &outputs_exist,
                           &entry->output_count, error, error_size) ||
            !validate_outputs(root, task, &outputs_valid, error, error_size)) {
            ok = false;
            break;
        }
        const FusionJson *state_tasks = fusion_json_member(&state.document, "tasks");
        const FusionJson *record = fusion_json_member(state_tasks, task->name);
        const FusionJson *recorded_digest = fusion_json_member(record, "digest");
        const FusionJson *recorded_output = fusion_json_member(record, "output_digest");
        bool rebuild = force || !recorded_digest ||
                       recorded_digest->kind != FUSION_JSON_STRING ||
                       strcmp(recorded_digest->string, digests[index]) ||
                       !outputs_exist || !recorded_output ||
                       recorded_output->kind != FUSION_JSON_STRING ||
                       strcmp(recorded_output->string, current_output) || !outputs_valid;
        if (!rebuild) {
            entry->status = FUSION_REBUILD_UP_TO_DATE;
            snprintf(entry->output_digest, sizeof entry->output_digest, "%s",
                     current_output);
            continue;
        }
        if (!runner(root, task, runner_user, error, error_size)) {
            if (error && error_size && !error[0])
                snprintf(error, error_size, "%s: rebuild action failed", task->name);
            ok = false;
            break;
        }
        outputs_exist = false;
        outputs_valid = true;
        if (!output_digest(root, task, current_output, &outputs_exist,
                           &entry->output_count, error, error_size) ||
            !validate_outputs(root, task, &outputs_valid, error, error_size)) {
            ok = false;
            break;
        }
        if (!outputs_exist) {
            ok = set_error(error, error_size,
                           "%s: task completed without all declared outputs", task->name);
            break;
        }
        if (!outputs_valid) {
            ok = set_error(error, error_size, "%s: invalid outputs after the run",
                           task->name);
            break;
        }
        PathList outputs = {0};
        if (!task_outputs(root, task, &outputs, error, error_size)) {
            paths_free(&outputs);
            ok = false;
            break;
        }
        paths_sort_unique(&outputs);
        ok = update_state_record(&state, task, digests[index], current_output,
                                 &outputs, error, error_size) &&
             fusion_rebuild_state_save(state_absolute, &state, error, error_size);
        paths_free(&outputs);
        if (!ok) break;
        entry->status = FUSION_REBUILD_REBUILT;
        snprintf(entry->output_digest, sizeof entry->output_digest, "%s", current_output);
    }
    if (ok) {
        result->entries = entries;
        result->count = order.count;
    } else free(entries);
    free(skipped);
    free(digests);
    fusion_rebuild_state_free(&state);
    fusion_rebuild_order_free(&order);
    return ok;
}

bool fusion_rebuild_execute(const char *root, const char *state_path,
                            const char *const *selected, size_t selected_count,
                            bool force, FusionRebuildPlan *result,
                            char *error, size_t error_size) {
    size_t task_count = 0;
    const FusionRebuildTask *tasks = fusion_rebuild_tasks(&task_count);
    return fusion_rebuild_execute_tasks(root, state_path, tasks, task_count,
                                        selected, selected_count, force,
                                        fusion_rebuild_run_registered_action, NULL,
                                        result, error, error_size);
}

const char *fusion_rebuild_plan_status_name(FusionRebuildPlanStatus status) {
    switch (status) {
        case FUSION_REBUILD_UP_TO_DATE: return "up to date";
        case FUSION_REBUILD_WOULD_REBUILD: return "would rebuild";
        case FUSION_REBUILD_REBUILT: return "rebuilt";
        case FUSION_REBUILD_SKIPPED_MISSING: return "skipped";
        case FUSION_REBUILD_SKIPPED_DEPENDENCY: return "skipped";
    }
    return "unknown";
}

void fusion_rebuild_plan_free(FusionRebuildPlan *plan) {
    if (!plan) return;
    free(plan->entries);
    *plan = (FusionRebuildPlan){0};
}
