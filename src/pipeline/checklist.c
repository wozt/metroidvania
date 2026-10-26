/* SPDX-License-Identifier: GPL-3.0-only */
/* Native renderer for the tracked reconstruction checklist and Aria backlog. */
#define _POSIX_C_SOURCE 200809L

#include "internal.h"

#include "output.h"
#include "sha256.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum { GAME_MZM, GAME_AOS, GAME_COUNT };
enum {
    COL_GAME, COL_ID, COL_CATEGORY, COL_STATUS, COL_TITLE, COL_ROUTINES,
    COL_SOURCES, COL_TESTS, COL_DEPENDENCIES, COL_DIVERGENCES, COL_NOTES,
    COL_NEXT_ACTION, COL_IN_GAME, COL_EMULATOR, COL_PLAYED, COLUMN_COUNT
};

typedef struct {
    char *values[COLUMN_COUNT];
    size_t line;
} Feature;

typedef struct {
    Feature *items;
    size_t count;
    size_t capacity;
} FeatureList;

typedef struct {
    const char *id;
    const char *title;
} Category;

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

typedef struct {
    FusionJson manifest;
    FusionJson routines[GAME_COUNT];
    char *annotations_text;
    FeatureList features[GAME_COUNT];
} ChecklistData;

static const char inventory_schema[] = "metroidvania-native-inventory-v6";
static const char record_schema[] = "metroidvania-native-inventory-records-v6";
static const char annotation_header[] =
    "game\tid\tcategory\tstatus\ttitle\tnative_routines\tlocal_sources\ttests\t"
    "dependencies\tdivergences\tnotes\tnext_action\tin_game_check\t"
    "emulator_compared\tplayed_through";
static const char unassigned_header[] =
    "id\tsymbol\tcategory\tcategory_evidence\tsuggested_category\t"
    "suggestion_evidence\tkind\taddress\tsource\tline\tdirect_callers\t"
    "direct_callees\n";

static const Category categories[] = {
    {"startup", "Startup and initialization"}, {"title", "Title screen"},
    {"menus", "Menus and options"}, {"input", "Controls and input"},
    {"interface", "HUD and interface"}, {"player", "Player state machine"},
    {"physics", "Physics"}, {"animation", "Animations"},
    {"collision", "Collisions, hitboxes and hurtboxes"},
    {"weapons", "Weapons and projectiles"}, {"upgrades", "Upgrades and equipment"},
    {"abilities", "Powers and abilities"}, {"enemies", "Enemies"},
    {"bosses", "Bosses"}, {"items", "Items and drops"},
    {"environment", "Environment interactions"},
    {"rooms", "Rooms, areas and maps"}, {"camera", "Camera and scrolling"},
    {"transitions", "Transitions"}, {"events", "Events and scripts"},
    {"cutscenes", "Cutscenes"}, {"audio", "Music and sound effects"},
    {"inventory", "Inventory and progression data"},
    {"progression", "Progression and unlock conditions"},
    {"saves", "Save and load"}, {"death", "Death and game over"},
    {"victory", "Victory conditions"}, {"endings", "Endings and credits"},
    {"technical", "GBA-specific technical systems"},
    {"data", "Native data definitions"},
    {"unclassified", "Unclassified source routines"},
};

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

static bool text_reserve(Text *text, size_t extra) {
    if (extra > SIZE_MAX - text->size - 1) return false;
    size_t required = text->size + extra + 1;
    if (required <= text->capacity) return true;
    size_t capacity = text->capacity ? text->capacity : 4096;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2) return false;
        capacity *= 2;
    }
    char *grown = realloc(text->data, capacity);
    if (!grown) return false;
    text->data = grown;
    text->capacity = capacity;
    return true;
}

static bool text_append_n(Text *text, const char *value, size_t length) {
    if (!text_reserve(text, length)) return false;
    memcpy(text->data + text->size, value, length);
    text->size += length;
    text->data[text->size] = '\0';
    return true;
}

static bool text_append(Text *text, const char *value) {
    return text_append_n(text, value, strlen(value));
}

static bool text_format(Text *text, const char *format, ...) {
    va_list arguments, copy;
    va_start(arguments, format);
    va_copy(copy, arguments);
    int length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0 || !text_reserve(text, (size_t)length)) {
        va_end(arguments);
        return false;
    }
    vsnprintf(text->data + text->size, text->capacity - text->size,
              format, arguments);
    va_end(arguments);
    text->size += (size_t)length;
    return true;
}

static bool resolve_path(char output[4096], const char *root, const char *path) {
    int length = path[0] == '/'
                     ? snprintf(output, 4096, "%s", path)
                     : snprintf(output, 4096, "%s%s%s", root,
                                root[0] && root[strlen(root) - 1] == '/' ? "" : "/",
                                path);
    return length >= 0 && length < 4096;
}

static bool read_file(const char *path, char **data, size_t *size,
                      char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (!file) return set_error(error, error_size, "cannot load %s", path);
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
        return set_error(error, error_size, "cannot load %s", path);
    }
    fclose(file);
    bytes[length] = '\0';
    *data = bytes;
    *size = (size_t)length;
    return true;
}

static const FusionJson *member(const FusionJson *object, const char *name,
                                FusionJsonKind kind) {
    const FusionJson *value = fusion_json_member(object, name);
    return value && value->kind == kind ? value : NULL;
}

static const char *string_member(const FusionJson *object, const char *name) {
    const FusionJson *value = member(object, name, FUSION_JSON_STRING);
    return value ? value->string : "";
}

static int64_t int_member(const FusionJson *object, const char *name) {
    const FusionJson *value = member(object, name, FUSION_JSON_NUMBER);
    return value && value->integral ? value->integer : 0;
}

static int game_index(const char *id) {
    if (!strcmp(id, "mzm")) return GAME_MZM;
    if (!strcmp(id, "aos")) return GAME_AOS;
    return -1;
}

static const char *game_id(int game) {
    return game == GAME_MZM ? "mzm" : "aos";
}

static const FusionJson *game_manifest(const ChecklistData *data, int game) {
    const FusionJson *games = member(&data->manifest, "games", FUSION_JSON_OBJECT);
    return games ? member(games, game_id(game), FUSION_JSON_OBJECT) : NULL;
}

static bool sha256_matches(const char *data, size_t size, const char *expected) {
    uint8_t digest[32];
    char hexadecimal[65];
    static const char digits[] = "0123456789abcdef";
    fusion_sha256_digest((const uint8_t *)data, size, digest);
    for (size_t index = 0; index < 32; ++index) {
        hexadecimal[index * 2] = digits[digest[index] >> 4];
        hexadecimal[index * 2 + 1] = digits[digest[index] & 15];
    }
    hexadecimal[64] = '\0';
    return !strcmp(hexadecimal, expected);
}

static bool safe_shard_name(const char *path) {
    return path[0] && !strchr(path, '/') && !strchr(path, '\\') &&
           strcmp(path, ".") && strcmp(path, "..");
}

static bool load_inventory(ChecklistData *data, const char *path,
                           char *error, size_t error_size) {
    char *bytes = NULL;
    size_t size = 0;
    if (!read_file(path, &bytes, &size, error, error_size)) return false;
    bool ok = fusion_json_parse(bytes, size, &data->manifest, error, error_size);
    free(bytes);
    if (!ok) return false;
    if (strcmp(string_member(&data->manifest, "schema"), inventory_schema))
        return set_error(error, error_size, "unsupported native inventory schema");
    char directory[4096];
    int length = snprintf(directory, sizeof directory, "%s", path);
    if (length < 0 || (size_t)length >= sizeof directory)
        return set_error(error, error_size, "inventory path is too long");
    char *slash = strrchr(directory, '/');
    if (slash) *slash = '\0';
    else snprintf(directory, sizeof directory, ".");
    for (int game = 0; game < GAME_COUNT; ++game) {
        data->routines[game] = fusion_json_array();
        const FusionJson *game_data = game_manifest(data, game);
        const FusionJson *files = member(game_data, "record_files", FUSION_JSON_ARRAY);
        if (!game_data || !files)
            return set_error(error, error_size, "invalid native inventory manifest");
        for (size_t index = 0; index < files->count; ++index) {
            const FusionJson *record = &files->items[index];
            const char *collection = string_member(record, "collection");
            bool keep_routines = !strcmp(collection, "routines");
            const char *name = string_member(record, "path");
            const char *expected = string_member(record, "sha256");
            if (!safe_shard_name(name) || strlen(expected) != 64)
                return set_error(error, error_size, "invalid native inventory shard");
            char shard_path[4096];
            length = snprintf(shard_path, sizeof shard_path, "%s/%s", directory, name);
            if (length < 0 || (size_t)length >= sizeof shard_path)
                return set_error(error, error_size, "inventory shard path is too long");
            char *shard_bytes = NULL;
            size_t shard_size = 0;
            if (!read_file(shard_path, &shard_bytes, &shard_size, error, error_size))
                return false;
            if (!sha256_matches(shard_bytes, shard_size, expected)) {
                free(shard_bytes);
                return set_error(error, error_size,
                                 "native inventory shard checksum mismatch: %s", name);
            }
            FusionJson shard = {0};
            if (!fusion_json_parse(shard_bytes, shard_size, &shard,
                                   error, error_size)) {
                free(shard_bytes);
                return false;
            }
            free(shard_bytes);
            const FusionJson *records = member(&shard, "records", FUSION_JSON_ARRAY);
            if (strcmp(string_member(&shard, "schema"), record_schema) ||
                strcmp(string_member(&shard, "game"), game_id(game)) || !records) {
                fusion_json_free(&shard);
                return set_error(error, error_size, "invalid native inventory shard: %s",
                                 name);
            }
            if (strcmp(string_member(&shard, "collection"), collection) ||
                int_member(record, "count") != (int64_t)records->count) {
                fusion_json_free(&shard);
                return set_error(error, error_size,
                                 "invalid native inventory shard count: %s", name);
            }
            if (keep_routines) {
                for (size_t item = 0; item < records->count; ++item) {
                    FusionJson copy;
                    if (!fusion_json_clone(&records->items[item], &copy) ||
                        !fusion_json_push(&data->routines[game], copy)) {
                        fusion_json_free(&shard);
                        return set_error(error, error_size, "out of memory");
                    }
                }
            }
            fusion_json_free(&shard);
        }
    }
    return true;
}

static bool feature_add(FeatureList *list, Feature feature) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity ? list->capacity * 2 : 128;
        Feature *items = realloc(list->items, capacity * sizeof *items);
        if (!items) return false;
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = feature;
    return true;
}

static size_t split_columns(char *line, char **fields, size_t count) {
    size_t found = 0;
    char *cursor = line;
    while (found < count) {
        fields[found++] = cursor;
        char *tab = strchr(cursor, '\t');
        if (!tab) break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return found;
}

static bool list_contains(const char *list, const char *wanted) {
    size_t wanted_length = strlen(wanted);
    const char *item = list;
    while (*item) {
        const char *end = strchr(item, '|');
        size_t length = end ? (size_t)(end - item) : strlen(item);
        if (length == wanted_length && !memcmp(item, wanted, length)) return true;
        if (!end) break;
        item = end + 1;
    }
    return false;
}

static bool known_category(const char *category) {
    for (size_t index = 0; index < sizeof categories / sizeof categories[0]; ++index)
        if (!strcmp(categories[index].id, category)) return true;
    return false;
}

static bool known_status(const char *status) {
    static const char *const statuses[] = {
        "missing", "partial", "functional", "validated", "research", "blocked",
    };
    for (size_t index = 0; index < sizeof statuses / sizeof statuses[0]; ++index)
        if (!strcmp(statuses[index], status)) return true;
    return false;
}

static bool load_annotations(ChecklistData *data, const char *path,
                             char *error, size_t error_size) {
    size_t size = 0;
    if (!read_file(path, &data->annotations_text, &size, error, error_size)) return false;
    char *line = data->annotations_text;
    char *newline = strchr(line, '\n');
    if (!newline) return set_error(error, error_size, "%s: invalid header", path);
    *newline = '\0';
    if (strcmp(line, annotation_header))
        return set_error(error, error_size, "%s: invalid annotation header", path);
    const char *previous_id = "";
    size_t line_number = 2;
    for (line = newline + 1; line < data->annotations_text + size && *line;
         line_number++) {
        newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        if (*line) {
            Feature feature = {.line = line_number};
            if (split_columns(line, feature.values, COLUMN_COUNT) != COLUMN_COUNT ||
                strchr(feature.values[COLUMN_COUNT - 1], '\t'))
                return set_error(error, error_size,
                                 "%s:%zu: expected %d columns", path, line_number,
                                 COLUMN_COUNT);
            int game = game_index(feature.values[COL_GAME]);
            if (game < 0)
                return set_error(error, error_size, "%s:%zu: unknown game: %s",
                                 path, line_number, feature.values[COL_GAME]);
            size_t game_length = strlen(feature.values[COL_GAME]);
            if (strncmp(feature.values[COL_ID], feature.values[COL_GAME], game_length) ||
                feature.values[COL_ID][game_length] != '.')
                return set_error(error, error_size,
                                 "%s:%zu: feature id must start with game", path,
                                 line_number);
            if (strcmp(feature.values[COL_ID], previous_id) <= 0)
                return set_error(error, error_size,
                                 "%s:%zu: rows are out of order or duplicated", path,
                                 line_number);
            previous_id = feature.values[COL_ID];
            for (size_t column = 0; column < COLUMN_COUNT; ++column) {
                bool list = column == COL_ROUTINES || column == COL_SOURCES ||
                            column == COL_TESTS || column == COL_DEPENDENCIES ||
                            column == COL_IN_GAME;
                if (!list && strchr(feature.values[column], '|'))
                    return set_error(error, error_size,
                                     "%s:%zu: '|' is not allowed in column %zu",
                                     path, line_number, column + 1);
            }
            if ((feature.values[COL_EMULATOR][0] &&
                 strcmp(feature.values[COL_EMULATOR], "yes")) ||
                (feature.values[COL_PLAYED][0] &&
                 strcmp(feature.values[COL_PLAYED], "yes")))
                return set_error(error, error_size,
                                 "%s:%zu: evidence must be empty or 'yes'", path,
                                 line_number);
            if (!feature_add(&data->features[game], feature))
                return set_error(error, error_size, "out of memory");
        }
        if (!newline) break;
        line = newline + 1;
    }
    return true;
}

static const FusionJson *find_routine(const ChecklistData *data, int game,
                                      const char *id) {
    const FusionJson *routines = &data->routines[game];
    size_t low = 0, high = routines->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const FusionJson *routine = &routines->items[middle];
        int comparison = strcmp(string_member(routine, "id"), id);
        if (comparison < 0) low = middle + 1;
        else high = middle;
    }
    if (low < routines->count &&
        !strcmp(string_member(&routines->items[low], "id"), id))
        return &routines->items[low];
    return NULL;
}

static size_t symbol_count(const ChecklistData *data, int game, const char *symbol) {
    size_t count = 0;
    const FusionJson *routines = &data->routines[game];
    for (size_t index = 0; index < routines->count; ++index)
        count += !strcmp(string_member(&routines->items[index], "symbol"), symbol);
    return count;
}

static bool feature_id_exists(const ChecklistData *data, const char *id) {
    for (int game = 0; game < GAME_COUNT; ++game)
        for (size_t index = 0; index < data->features[game].count; ++index)
            if (!strcmp(data->features[game].items[index].values[COL_ID], id)) return true;
    return false;
}

static bool validate_path_list(const char *root, const Feature *feature, int column,
                               char *error, size_t error_size) {
    const char *item = feature->values[column];
    while (*item) {
        const char *end = strchr(item, '|');
        size_t length = end ? (size_t)(end - item) : strlen(item);
        char relative[4096], absolute[4096];
        if (!length || length >= sizeof relative) return false;
        memcpy(relative, item, length);
        relative[length] = '\0';
        struct stat status;
        if (!resolve_path(absolute, root, relative) || stat(absolute, &status) ||
            !S_ISREG(status.st_mode))
            return set_error(error, error_size, "%s: missing referenced path: %s",
                             feature->values[COL_ID], relative);
        if (!end) break;
        item = end + 1;
    }
    return true;
}

static bool validate_data(const ChecklistData *data, const char *root,
                          char *error, size_t error_size) {
    for (int game = 0; game < GAME_COUNT; ++game) {
        for (size_t index = 0; index < data->features[game].count; ++index) {
            const Feature *feature = &data->features[game].items[index];
            if (!known_status(feature->values[COL_STATUS]))
                return set_error(error, error_size, "%s: invalid status",
                                 feature->values[COL_ID]);
            if (!known_category(feature->values[COL_CATEGORY]))
                return set_error(error, error_size, "%s: invalid category",
                                 feature->values[COL_ID]);
            const char *symbol = feature->values[COL_ROUTINES];
            while (*symbol) {
                const char *end = strchr(symbol, '|');
                size_t length = end ? (size_t)(end - symbol) : strlen(symbol);
                char name[512];
                if (!length || length >= sizeof name)
                    return set_error(error, error_size, "%s: invalid native symbol",
                                     feature->values[COL_ID]);
                memcpy(name, symbol, length);
                name[length] = '\0';
                size_t matches = symbol_count(data, game, name);
                if (!matches)
                    return set_error(error, error_size,
                                     "%s: native symbol not found: %s",
                                     feature->values[COL_ID], name);
                if (matches > 1)
                    printf("warning: %s: symbol %s has %zu definitions\n",
                           feature->values[COL_ID], name, matches);
                if (!end) break;
                symbol = end + 1;
            }
            const char *dependency = feature->values[COL_DEPENDENCIES];
            while (*dependency) {
                const char *end = strchr(dependency, '|');
                size_t length = end ? (size_t)(end - dependency) : strlen(dependency);
                char id[512];
                if (!length || length >= sizeof id)
                    return set_error(error, error_size, "%s: invalid dependency",
                                     feature->values[COL_ID]);
                memcpy(id, dependency, length);
                id[length] = '\0';
                if (!feature_id_exists(data, id))
                    return set_error(error, error_size, "%s: unknown dependency: %s",
                                     feature->values[COL_ID], id);
                if (!end) break;
                dependency = end + 1;
            }
            if (!validate_path_list(root, feature, COL_SOURCES, error, error_size) ||
                !validate_path_list(root, feature, COL_TESTS, error, error_size))
                return false;
            bool complete = !strcmp(feature->values[COL_STATUS], "functional") ||
                            !strcmp(feature->values[COL_STATUS], "validated");
            if (complete && !feature->values[COL_TESTS][0])
                return set_error(error, error_size, "%s: status requires tests",
                                 feature->values[COL_ID]);
            if (complete && !feature->values[COL_IN_GAME][0])
                return set_error(error, error_size,
                                 "%s: status requires an in-game check",
                                 feature->values[COL_ID]);
            if (!strcmp(feature->values[COL_STATUS], "validated") &&
                strcmp(feature->values[COL_EMULATOR], "yes"))
                return set_error(error, error_size,
                                 "%s: validated requires an emulator comparison",
                                 feature->values[COL_ID]);
        }
    }
    return true;
}

static bool assigned_symbol(const ChecklistData *data, int game, const char *symbol) {
    for (size_t index = 0; index < data->features[game].count; ++index)
        if (list_contains(data->features[game].items[index].values[COL_ROUTINES], symbol))
            return true;
    return false;
}

static size_t attached_count(const ChecklistData *data, int game) {
    size_t count = 0;
    for (size_t index = 0; index < data->routines[game].count; ++index)
        count += assigned_symbol(data, game,
                                 string_member(&data->routines[game].items[index], "symbol"));
    return count;
}

static const char *status_mark(const char *status) {
    if (!strcmp(status, "missing")) return "[ ]";
    if (!strcmp(status, "partial")) return "[~]";
    if (!strcmp(status, "functional")) return "[+]";
    if (!strcmp(status, "validated")) return "[x]";
    if (!strcmp(status, "research")) return "[?]";
    return "[!]";
}

static bool append_pipe_list(Text *text, const char *list,
                             const char *prefix, const char *suffix,
                             const char *separator) {
    const char *item = list;
    bool first = true;
    while (*item) {
        const char *end = strchr(item, '|');
        size_t length = end ? (size_t)(end - item) : strlen(item);
        if (!first && !text_append(text, separator)) return false;
        if (!text_append(text, prefix) || !text_append_n(text, item, length) ||
            !text_append(text, suffix)) return false;
        first = false;
        if (!end) break;
        item = end + 1;
    }
    return true;
}

static int64_t category_count(const FusionJson *game_data, const char *category) {
    const FusionJson *statistics = member(game_data, "statistics", FUSION_JSON_OBJECT);
    const FusionJson *counts = member(statistics, "by_category", FUSION_JSON_OBJECT);
    return counts ? int_member(counts, category) : 0;
}

static size_t evidence_count(const ChecklistData *data, int game,
                             const char *category, const char *evidence) {
    size_t count = 0;
    for (size_t index = 0; index < data->routines[game].count; ++index) {
        const FusionJson *routine = &data->routines[game].items[index];
        if (!strcmp(string_member(routine, "category"), category) &&
            !strcmp(string_member(routine, "category_evidence"), evidence)) ++count;
    }
    return count;
}

static bool render_feature(Text *text, const ChecklistData *data, int game,
                           const Feature *feature) {
    if (!text_format(text, "%s **`%s` — %s.** %s\n\nNative evidence: ",
                     status_mark(feature->values[COL_STATUS]),
                     feature->values[COL_ID], feature->values[COL_TITLE],
                     feature->values[COL_NOTES])) return false;
    bool found = false;
    for (size_t index = 0; index < data->routines[game].count; ++index) {
        const FusionJson *routine = &data->routines[game].items[index];
        if (!list_contains(feature->values[COL_ROUTINES],
                           string_member(routine, "symbol"))) continue;
        if (found && !text_append(text, ", ")) return false;
        if (!text_format(text, "`%s`", string_member(routine, "id"))) return false;
        found = true;
    }
    if (!found && !text_append(text, "not attached yet")) return false;
    if (!text_append(text, ".  \nLocal implementation: ")) return false;
    if (feature->values[COL_SOURCES][0]) {
        const char *item = feature->values[COL_SOURCES];
        bool first = true;
        while (*item) {
            const char *end = strchr(item, '|');
            size_t length = end ? (size_t)(end - item) : strlen(item);
            if (!first && !text_append(text, ", ")) return false;
            if (!text_append(text, "[`") || !text_append_n(text, item, length) ||
                !text_append(text, "`](../") || !text_append_n(text, item, length) ||
                !text_append(text, ")")) return false;
            first = false;
            if (!end) break;
            item = end + 1;
        }
    } else if (!text_append(text, "none recorded")) return false;
    if (!text_append(text, ".  \nTests: ")) return false;
    if (feature->values[COL_TESTS][0]) {
        const char *item = feature->values[COL_TESTS];
        bool first = true;
        while (*item) {
            const char *end = strchr(item, '|');
            size_t length = end ? (size_t)(end - item) : strlen(item);
            if (!first && !text_append(text, ", ")) return false;
            if (!text_append(text, "[`") || !text_append_n(text, item, length) ||
                !text_append(text, "`](../") || !text_append_n(text, item, length) ||
                !text_append(text, ")")) return false;
            first = false;
            if (!end) break;
            item = end + 1;
        }
    } else if (!text_append(text, "none recorded")) return false;
    if (!text_append(text, ".\n")) return false;
    if (feature->values[COL_DEPENDENCIES][0]) {
        if (!text_append(text, "Dependencies: ") ||
            !append_pipe_list(text, feature->values[COL_DEPENDENCIES], "`", "`", ", ") ||
            !text_append(text, ".  \n")) return false;
    }
    if (feature->values[COL_DIVERGENCES][0] &&
        !text_format(text, "Known divergences: %s  \n",
                     feature->values[COL_DIVERGENCES])) return false;
    if (feature->values[COL_NEXT_ACTION][0] &&
        !text_format(text, "Next action: %s\n",
                     feature->values[COL_NEXT_ACTION])) return false;
    bool has_evidence = feature->values[COL_IN_GAME][0] ||
                        !strcmp(feature->values[COL_EMULATOR], "yes") ||
                        !strcmp(feature->values[COL_PLAYED], "yes");
    if (has_evidence) {
        if (!text_append(text, "Evidence: ")) return false;
        bool separator = false;
        if (feature->values[COL_IN_GAME][0]) {
            if (!text_append(text, "in game: ") ||
                !append_pipe_list(text, feature->values[COL_IN_GAME], "`", "`", ", "))
                return false;
            separator = true;
        }
        if (!strcmp(feature->values[COL_EMULATOR], "yes")) {
            if (separator && !text_append(text, "; ")) return false;
            if (!text_append(text, "compared with an emulator")) return false;
            separator = true;
        }
        if (!strcmp(feature->values[COL_PLAYED], "yes")) {
            if (separator && !text_append(text, "; ")) return false;
            if (!text_append(text, "played through")) return false;
        }
        if (!text_append(text, ".\n")) return false;
    }
    return text_append(text, "\n");
}

static bool render_checklist(const ChecklistData *data, Text *text) {
    static const char introduction[] =
        "# Native parity checklist\n\n"
        "This file is generated by `build/fusion_rebuild --checklist` from\n"
        "the automatic source inventory and the canonical human annotations in\n"
        "`data/native_parity/annotations.tsv`. Do not edit it directly; edit the\n"
        "TSV (one row per feature, tab-separated columns, `|`-joined lists) and\n"
        "regenerate this view.\n\n"
        "> Scope warning: this static inventory indexes C/assembly functions, header\n"
        "> declarations, top-level data, named aggregate types, object-like constants\n"
        "> and function-pointer table dispatch when it is lexically provable. Dynamic\n"
        "> callbacks and runtime observations remain open. Counts are discovery\n"
        "> coverage, not game fidelity percentages.\n\n"
        "## Status legend\n\n"
        "- `[ ]` not implemented\n- `[~]` partially implemented\n"
        "- `[+]` ported, unit-tested and checked in game\n"
        "- `[x]` behaviorally compared against an emulator\n"
        "- `[?]` research or identification required\n"
        "- `[!]` blocked or known problem\n\n"
        "## Automatic inventory summary\n\n"
        "| Game | Source revision | Routines | Addressed | Call edges (indirect) | Annotated features |\n"
        "|---|---:|---:|---:|---:|---:|\n";
    if (!text_append(text, introduction)) return false;
    for (int game = 0; game < GAME_COUNT; ++game) {
        const FusionJson *game_data = game_manifest(data, game);
        const FusionJson *stats = member(game_data, "statistics", FUSION_JSON_OBJECT);
        if (!text_format(text, "| %s | `%.12s` | %lld | %lld | %lld (%lld) | %zu |\n",
                         string_member(game_data, "title"),
                         string_member(game_data, "source_revision"),
                         (long long)int_member(stats, "routines"),
                         (long long)int_member(stats, "with_source_address"),
                         (long long)int_member(stats, "resolved_call_edges"),
                         (long long)int_member(stats, "resolved_indirect_call_edges"),
                         data->features[game].count)) return false;
    }
    if (!text_append(text,
        "\n| Game | Header declarations | Data symbols | Pointer tables | Named types | Constants |\n"
        "|---|---:|---:|---:|---:|---:|\n")) return false;
    for (int game = 0; game < GAME_COUNT; ++game) {
        const FusionJson *game_data = game_manifest(data, game);
        const FusionJson *stats = member(game_data, "statistics", FUSION_JSON_OBJECT);
        if (!text_format(text, "| %s | %lld | %lld | %lld | %lld | %lld |\n",
                         string_member(game_data, "title"),
                         (long long)int_member(stats, "declarations"),
                         (long long)int_member(stats, "data_symbols"),
                         (long long)int_member(stats, "pointer_tables"),
                         (long long)int_member(stats, "types"),
                         (long long)int_member(stats, "constants"))) return false;
    }
    if (!text_append(text,
        "\nThe machine-readable manifest and its checksummed record fragments, stable\n"
        "routine IDs and call edges are rooted at\n"
        "[`data/native_parity/inventory.json`](../data/native_parity/inventory.json).\n\n"
        "### Reviewed routine attachment\n\n"
        "| Game | Attached to features | Not yet attached |\n"
        "|---|---:|---:|\n")) return false;
    for (int game = 0; game < GAME_COUNT; ++game) {
        const FusionJson *game_data = game_manifest(data, game);
        size_t attached = attached_count(data, game);
        if (!text_format(text, "| %s | %zu | %zu |\n",
                         string_member(game_data, "title"), attached,
                         data->routines[game].count - attached)) return false;
    }
    if (!text_append(text,
        "\nThe complete generated Aria backlog is\n"
        "[`data/native_parity/aos_unassigned_routines.tsv`](../data/native_parity/aos_unassigned_routines.tsv).\n"
        "Routine attachment measures reviewed classification, not implementation fidelity.\n\n"))
        return false;
    for (int game = 0; game < GAME_COUNT; ++game) {
        const FusionJson *game_data = game_manifest(data, game);
        if (!text_format(text, "## %s\n\nPinned source: `%s` at `%s`.\n\n",
                         string_member(game_data, "title"),
                         string_member(game_data, "source_root"),
                         string_member(game_data, "source_revision"))) return false;
        for (size_t category_index = 0;
             category_index < sizeof categories / sizeof categories[0];
             ++category_index) {
            const Category *category = &categories[category_index];
            int64_t count = category_count(game_data, category->id);
            size_t feature_count = 0;
            for (size_t index = 0; index < data->features[game].count; ++index)
                feature_count += !strcmp(data->features[game].items[index]
                                             .values[COL_CATEGORY], category->id);
            if (!feature_count && count == 0 &&
                (!strcmp(category->id, "data") ||
                 !strcmp(category->id, "unclassified"))) continue;
            if (!text_format(text,
                             "### %s\n\n[?] **`%s.%s.inventory-boundary` — source coverage.** "
                             "The current classifier assigns %lld discovered routine(s) to "
                             "this category",
                             category->title, game_id(game), category->id,
                             (long long)count)) return false;
            size_t path = evidence_count(data, game, category->id, "path");
            size_t name = evidence_count(data, game, category->id, "name");
            size_t referrers = evidence_count(data, game, category->id, "referrers");
            if (path || name || referrers) {
                if (!text_append(text, " (")) return false;
                bool separator = false;
                if (path) {
                    if (!text_format(text, "%zu by source path", path)) return false;
                    separator = true;
                }
                if (name) {
                    if (separator && !text_append(text, ", ")) return false;
                    if (!text_format(text, "%zu by native name", name)) return false;
                    separator = true;
                }
                if (referrers) {
                    if (separator && !text_append(text, ", ")) return false;
                    if (!text_format(text, "%zu by exclusive referrers", referrers))
                        return false;
                }
                if (!text_append(text, ")")) return false;
            }
            if (!text_append(text,
                             ". Category completeness has not been established.\n\n"))
                return false;
            for (size_t index = 0; index < data->features[game].count; ++index) {
                const Feature *feature = &data->features[game].items[index];
                if (!strcmp(feature->values[COL_CATEGORY], category->id) &&
                    !render_feature(text, data, game, feature)) return false;
            }
        }
        const FusionJson *coverage = member(game_data, "coverage", FUSION_JSON_OBJECT);
        const FusionJson *missing = member(coverage, "not_yet_indexed", FUSION_JSON_ARRAY);
        if (!text_append(text, "### Known inventory gaps\n\n")) return false;
        if (missing) {
            for (size_t index = 0; index < missing->count; ++index)
                if (!text_format(text, "- [!] %s.\n", missing->items[index].string))
                    return false;
        }
        if (!text_append(text, "\n")) return false;
    }
    while (text->size && text->data[text->size - 1] == '\n') --text->size;
    text->data[text->size] = '\0';
    return text_append(text, "\n");
}

static ptrdiff_t routine_index(const ChecklistData *data, int game, const char *id) {
    const FusionJson *routine = find_routine(data, game, id);
    return routine ? routine - data->routines[game].items : -1;
}

static const char *target_category(const ChecklistData *data, int game,
                                   const char *target, const char *const *suggestions) {
    ptrdiff_t index = routine_index(data, game, target);
    if (index < 0) return NULL;
    if (suggestions[index]) return suggestions[index];
    const char *category = string_member(&data->routines[game].items[index], "category");
    return strcmp(category, "unclassified") ? category : NULL;
}

static void consider_target(const ChecklistData *data, int game, const char *target,
                            const char *const *suggestions,
                            const char **category, bool *mixed) {
    const char *candidate = target_category(data, game, target, suggestions);
    if (!candidate) return;
    if (!*category) *category = candidate;
    else if (strcmp(*category, candidate)) *mixed = true;
}

static bool render_unassigned(const ChecklistData *data, Text *text,
                              char *error, size_t error_size) {
    int game = GAME_AOS;
    size_t count = data->routines[game].count;
    const char **suggestions = calloc(count, sizeof *suggestions);
    const char **batch = calloc(count, sizeof *batch);
    size_t *incoming = calloc(count, sizeof *incoming);
    if (!suggestions || !batch || !incoming) {
        free(suggestions); free(batch); free(incoming);
        return set_error(error, error_size, "out of memory");
    }
    for (;;) {
        memset(batch, 0, count * sizeof *batch);
        size_t additions = 0;
        for (size_t index = 0; index < count; ++index) {
            const FusionJson *routine = &data->routines[game].items[index];
            if (strcmp(string_member(routine, "category"), "unclassified") ||
                suggestions[index]) continue;
            const char *category = NULL;
            bool mixed = false;
            const FusionJson *calls = member(routine, "calls", FUSION_JSON_ARRAY);
            const FusionJson *references = member(routine, "address_references",
                                                   FUSION_JSON_ARRAY);
            const FusionJson *callbacks = member(routine, "member_callbacks",
                                                  FUSION_JSON_ARRAY);
            if (calls) for (size_t item = 0; item < calls->count; ++item)
                consider_target(data, game, calls->items[item].string, suggestions,
                                &category, &mixed);
            if (references) for (size_t item = 0; item < references->count; ++item)
                consider_target(data, game, references->items[item].string, suggestions,
                                &category, &mixed);
            if (callbacks) for (size_t item = 0; item < callbacks->count; ++item)
                consider_target(data, game,
                                string_member(&callbacks->items[item], "target"),
                                suggestions, &category, &mixed);
            if (category && !mixed) {
                batch[index] = category;
                ++additions;
            }
        }
        if (!additions) break;
        for (size_t index = 0; index < count; ++index)
            if (batch[index]) suggestions[index] = batch[index];
    }
    for (size_t index = 0; index < count; ++index) {
        const FusionJson *calls = member(&data->routines[game].items[index], "calls",
                                         FUSION_JSON_ARRAY);
        if (!calls) continue;
        for (size_t item = 0; item < calls->count; ++item) {
            ptrdiff_t target = routine_index(data, game, calls->items[item].string);
            if (target >= 0) ++incoming[target];
        }
    }
    bool ok = text_append(text, unassigned_header);
    for (size_t index = 0; index < count && ok; ++index) {
        const FusionJson *routine = &data->routines[game].items[index];
        if (assigned_symbol(data, game, string_member(routine, "symbol"))) continue;
        const FusionJson *calls = member(routine, "calls", FUSION_JSON_ARRAY);
        ok = text_format(text, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%lld\t%zu\t%zu\n",
                         string_member(routine, "id"), string_member(routine, "symbol"),
                         string_member(routine, "category"),
                         string_member(routine, "category_evidence"),
                         suggestions[index] ? suggestions[index] : "",
                         suggestions[index] ? "exclusive_callees" : "",
                         string_member(routine, "kind"), string_member(routine, "address"),
                         string_member(routine, "source"),
                         (long long)int_member(routine, "line"), incoming[index],
                         calls ? calls->count : 0);
    }
    free(suggestions);
    free(batch);
    free(incoming);
    return ok ? true : set_error(error, error_size, "out of memory");
}

typedef struct {
    const char *data;
    size_t size;
} WriteContext;

static bool write_text(FILE *file, const void *opaque) {
    const WriteContext *context = opaque;
    return fwrite(context->data, 1, context->size, file) == context->size;
}

static bool write_or_check(const char *path, const Text *generated, bool check_only,
                           char *error, size_t error_size) {
    if (check_only) {
        char *existing = NULL;
        size_t size = 0;
        if (!read_file(path, &existing, &size, error, error_size)) return false;
        bool same = size == generated->size && !memcmp(existing, generated->data, size);
        free(existing);
        return same ? true : set_error(error, error_size,
                                       "generated checklist output is stale: %s", path);
    }
    WriteContext context = {generated->data, generated->size};
    return fusion_asset_write_atomic(path, write_text, &context, error, error_size);
}

static void data_free(ChecklistData *data) {
    fusion_json_free(&data->manifest);
    for (int game = 0; game < GAME_COUNT; ++game) {
        fusion_json_free(&data->routines[game]);
        free(data->features[game].items);
    }
    free(data->annotations_text);
    *data = (ChecklistData){0};
}

bool fusion_native_checklist(const char *root,
                             const char *inventory_path,
                             const char *annotations_path,
                             const char *checklist_path,
                             const char *unassigned_path,
                             bool check_only,
                             char *error, size_t error_size) {
    if (!root || !inventory_path || !annotations_path || !checklist_path ||
        !unassigned_path)
        return set_error(error, error_size, "invalid native checklist request");
    char inventory[4096], annotations[4096], checklist[4096], unassigned[4096];
    if (!resolve_path(inventory, root, inventory_path) ||
        !resolve_path(annotations, root, annotations_path) ||
        !resolve_path(checklist, root, checklist_path) ||
        !resolve_path(unassigned, root, unassigned_path))
        return set_error(error, error_size, "native checklist path is too long");
    ChecklistData data = {0};
    Text markdown = {0}, backlog = {0};
    bool ok = load_inventory(&data, inventory, error, error_size) &&
              load_annotations(&data, annotations, error, error_size) &&
              validate_data(&data, root, error, error_size) &&
              render_checklist(&data, &markdown) &&
              render_unassigned(&data, &backlog, error, error_size);
    if (ok && (!markdown.data || !backlog.data))
        ok = set_error(error, error_size, "out of memory");
    if (ok) ok = write_or_check(checklist, &markdown, check_only, error, error_size) &&
                 write_or_check(unassigned, &backlog, check_only, error, error_size);
    if (!ok && error && error_size && !error[0])
        snprintf(error, error_size, "cannot render native checklist");
    free(markdown.data);
    free(backlog.data);
    data_free(&data);
    return ok;
}
