/* SPDX-License-Identifier: GPL-3.0-only */
/* The raw ROM blocks the pinned decompilation databases identify (formerly
 * scripts/import_game_assets.py --scope raw). Each database lists a path, a
 * US address and a count of fixed-size items; the bytes are copied, not
 * decoded, below <world>/raw/, and shared/manifests/import_catalog.json
 * records every block. All offsets of a world are validated before any of
 * its bytes are written. */
#include "fusion/assets/raw_import.h"

#include "export.h"
#include "core/json.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    size_t offset;
    size_t length;
    const char *role;
} RawEntry;

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

static void set_error(char *error, size_t error_size, const char *format, ...) {
    if (!error || error_size == 0) return;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error, error_size, format, arguments);
    va_end(arguments);
    error[error_size - 1] = '\0';
}

static bool text_append(Text *text, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    va_list copy;
    va_copy(copy, arguments);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(arguments);
        return false;
    }
    size_t total = text->size + (size_t)needed + 1;
    if (total > text->capacity) {
        size_t next = text->capacity ? text->capacity : 4096;
        while (next < total) next *= 2;
        char *grown = realloc(text->data, next);
        if (!grown) {
            va_end(arguments);
            return false;
        }
        text->data = grown;
        text->capacity = next;
    }
    vsnprintf(text->data + text->size, text->capacity - text->size, format, arguments);
    va_end(arguments);
    text->size += (size_t)needed;
    return true;
}

/* A relative path of [A-Za-z0-9_.-] components, none of them "." or "..". */
static bool safe_path(const char *value) {
    if (!value || !*value || value[0] == '/') return false;
    const char *part = value;
    for (;;) {
        const char *end = strchr(part, '/');
        size_t length = end ? (size_t)(end - part) : strlen(part);
        if (!length || (length == 1 && part[0] == '.') ||
            (length == 2 && part[0] == '.' && part[1] == '.'))
            return false;
        for (size_t index = 0; index < length; ++index) {
            char c = part[index];
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-'))
                return false;
        }
        if (!end) return true;
        part = end + 1;
    }
}

static const char *classify(const char *directory, const char *path) {
    char lower[FUSION_EXPORT_PATH_CAPACITY];
    snprintf(lower, sizeof lower, "%s/%s", directory, path);
    for (char *c = lower; *c; ++c)
        if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
    if (strstr(lower, "direct_sound") || strstr(lower, "sound") || strstr(lower, "music"))
        return "raw_audio_or_sound_data";
    if (strstr(lower, "room") || strstr(lower, "bg") || strstr(lower, "clip"))
        return "raw_room_or_background_data";
    if (strstr(lower, "sprite") || strstr(lower, "obj") || strstr(lower, "oam"))
        return "raw_sprite_or_object_data";
    if (strstr(lower, "cutscene") || strstr(lower, "scene")) return "raw_cutscene_data";
    return "raw_unclassified_data";
}

/* int(text, 16): an optional 0x prefix and hexadecimal digits. */
static bool parse_hex(const char *text, size_t *value) {
    if (!text) return false;
    const char *digits = text;
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits += 2;
    if (!*digits) return false;
    size_t result = 0;
    for (const char *c = digits; *c; ++c) {
        int digit = *c >= '0' && *c <= '9' ? *c - '0'
                  : *c >= 'a' && *c <= 'f' ? *c - 'a' + 10
                  : *c >= 'A' && *c <= 'F' ? *c - 'A' + 10 : -1;
        if (digit < 0 || result > (SIZE_MAX - (size_t)digit) / 16) return false;
        result = result * 16 + (size_t)digit;
    }
    *value = result;
    return true;
}

static const char *string_member(const FusionJson *object, const char *key) {
    const FusionJson *member = fusion_json_member(object, key);
    return member && member->kind == FUSION_JSON_STRING ? member->string : NULL;
}

static int compare_paths(const void *left, const void *right) {
    const RawEntry *const *a = left, *const *b = right;
    return strcmp((*a)->path, (*b)->path);
}

/* The blocks of one world's database, validated against its ROM. */
static bool read_entries(const FusionJson *database, const FusionRom *rom, bool metroid,
                         RawEntry **entries, size_t *count, char *error,
                         size_t error_size) {
    *entries = NULL;
    *count = 0;
    if (database->kind != FUSION_JSON_ARRAY) {
        set_error(error, error_size, "invalid decomp database format");
        return false;
    }
    *entries = calloc(database->count ? database->count : 1, sizeof **entries);
    if (!*entries) {
        set_error(error, error_size, "cannot allocate the raw import");
        return false;
    }
    for (size_t number = 0; number < database->count; ++number) {
        const FusionJson *item = &database->items[number];
        const char *directory = metroid ? string_member(item, "dir") : "data";
        if (metroid && !fusion_json_member(item, "dir")) directory = "data";
        const char *path = string_member(item, "path");
        if (!safe_path(directory) || !safe_path(path)) {
            set_error(error, error_size, "unsafe extracted path in decomp entry %zu", number);
            return false;
        }
        const char *address = string_member(fusion_json_member(item, "addr"), "us");
        if (!address) continue;
        const FusionJson *count_member = fusion_json_member(item, "count");
        const char *count_text = count_member && count_member->kind == FUSION_JSON_OBJECT
            ? string_member(count_member, "us")
            : count_member && count_member->kind == FUSION_JSON_STRING
                ? count_member->string : NULL;
        const FusionJson *size_member = fusion_json_member(item, "size");
        size_t offset = 0, items = 0;
        if (!parse_hex(address, &offset) || !parse_hex(count_text, &items) || !size_member ||
            size_member->kind != FUSION_JSON_NUMBER || !size_member->integral ||
            size_member->integer < 0 ||
            (size_member->integer && items > SIZE_MAX / (size_t)size_member->integer)) {
            set_error(error, error_size, "invalid decomp entry %zu: %s", number, path);
            return false;
        }
        size_t length = items * (size_t)size_member->integer;
        if (offset > rom->size || length > rom->size - offset) {
            set_error(error, error_size, "decomp entry %zu exceeds ROM boundaries: %s",
                      number, path);
            return false;
        }
        RawEntry *entry = &(*entries)[(*count)++];
        int written = snprintf(entry->path, sizeof entry->path, "%s/raw/%s/%s",
                               metroid ? "metroid" : "aria", directory, path);
        if (written < 0 || (size_t)written >= sizeof entry->path) {
            set_error(error, error_size, "extracted path too long: %s", path);
            return false;
        }
        entry->offset = offset;
        entry->length = length;
        entry->role = classify(directory, path);
    }
    const RawEntry **sorted = malloc((*count ? *count : 1) * sizeof *sorted);
    if (!sorted) {
        set_error(error, error_size, "cannot allocate the raw import");
        return false;
    }
    for (size_t index = 0; index < *count; ++index) sorted[index] = &(*entries)[index];
    qsort(sorted, *count, sizeof *sorted, compare_paths);
    for (size_t index = 1; index < *count; ++index) {
        if (!strcmp(sorted[index]->path, sorted[index - 1]->path)) {
            set_error(error, error_size, "duplicate extracted file: %s", sorted[index]->path);
            free(sorted);
            return false;
        }
    }
    free(sorted);
    return true;
}

/* Creates every parent directory of ``relative`` below ``root``, refusing
 * symlinks, and writes the bytes atomically. */
static bool write_below(const char *root, const char *relative, const uint8_t *data,
                        size_t size, char *error, size_t error_size) {
    char path[FUSION_EXPORT_PATH_CAPACITY];
    if (!fusion_export_join(path, root, relative)) {
        set_error(error, error_size, "extracted path too long: %s", relative);
        return false;
    }
    if (!fusion_export_ensure_directory(root, error, error_size)) return false;
    for (char *slash = path + strlen(root) + 1; (slash = strchr(slash, '/')) != NULL;
         ++slash) {
        *slash = '\0';
        bool ok = fusion_export_ensure_directory(path, error, error_size);
        *slash = '/';
        if (!ok) return false;
    }
    return fusion_export_write_file(path, data, size, error, error_size);
}

bool fusion_write_private_file(const char *root, const char *relative,
                               const uint8_t *data, size_t size,
                               char *error, size_t error_size) {
    if (!root || !safe_path(relative)) {
        set_error(error, error_size, "unsafe private output path");
        return false;
    }
    return write_below(root, relative, data, size, error, error_size);
}

static bool write_world(const char *root, const FusionRom *rom, const RawEntry *entries,
                        size_t count, char *error, size_t error_size) {
    for (size_t index = 0; index < count; ++index)
        if (!write_below(root, entries[index].path, rom->data + entries[index].offset,
                         entries[index].length, error, error_size))
            return false;
    return true;
}

static int compare_roles(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

/* The manifest, serialized like json.dumps(indent=2). */
static bool write_manifest(Text *text, const RawEntry *const *worlds,
                           const size_t *counts, size_t *bytes) {
    const char *roles[5] = {"raw_audio_or_sound_data", "raw_cutscene_data",
                            "raw_room_or_background_data", "raw_sprite_or_object_data",
                            "raw_unclassified_data"};
    qsort(roles, 5, sizeof roles[0], compare_roles);
    size_t role_counts[5] = {0}, total = 0;
    *bytes = 0;
    bool ok = text_append(text,
        "{\n"
        "  \"format\": \"MV_ASSET_IMPORT_2\",\n"
        "  \"provenance\": \"user-supplied verified USA ROMs, pinned decomp database offsets\",\n"
        "  \"warning\": \"Private extracted Nintendo/Konami data. NEVER commit or redistribute.\",\n"
        "  \"sources\": {\n"
        "    \"metroid\": {\n"
        "      \"expected_sha1\": \"%s\",\n"
        "      \"decomp\": \"mzm\"\n"
        "    },\n"
        "    \"aria\": {\n"
        "      \"expected_sha1\": \"%s\",\n"
        "      \"decomp\": \"cvaos\"\n"
        "    }\n"
        "  },\n"
        "  \"raw_entries\": [",
        fusion_asset_game_spec(FUSION_ASSET_GAME_METROID)->sha1,
        fusion_asset_game_spec(FUSION_ASSET_GAME_ARIA)->sha1);
    for (size_t world = 0; ok && world < 2; ++world) {
        for (size_t index = 0; ok && index < counts[world]; ++index) {
            const RawEntry *entry = &worlds[world][index];
            ok = text_append(text,
                "%s\n    {\n"
                "      \"path\": \"%s\",\n"
                "      \"rom_offset\": %zu,\n"
                "      \"length\": %zu,\n"
                "      \"role\": \"%s\",\n"
                "      \"status\": \"RAW_UNDECODED\"\n"
                "    }",
                total ? "," : "", entry->path, entry->offset, entry->length, entry->role);
            ++total;
            *bytes += entry->length;
            for (size_t role = 0; role < 5; ++role)
                if (!strcmp(roles[role], entry->role)) ++role_counts[role];
        }
    }
    ok = ok && text_append(text,
        "%s],\n"
        "  \"summary\": {\n"
        "    \"raw_blocks\": %zu,\n"
        "    \"raw_bytes\": %zu,\n"
        "    \"raw_categories\": {",
        total ? "\n  " : "", total, *bytes);
    bool first = true;
    for (size_t role = 0; ok && role < 5; ++role) {
        if (!role_counts[role]) continue;
        ok = text_append(text, "%s\n      \"%s\": %zu", first ? "" : ",", roles[role],
                         role_counts[role]);
        first = false;
    }
    return ok && text_append(text, "%s}\n  }\n}\n", first ? "" : "\n    ");
}

bool fusion_raw_import(const FusionRom *metroid, const FusionRom *aria,
                       const char *metroid_database, size_t metroid_database_size,
                       const char *aria_database, size_t aria_database_size,
                       const char *output_root, FusionRawImportSummary *summary,
                       char *error, size_t error_size) {
    if (summary) *summary = (FusionRawImportSummary){0};
    if (!metroid || metroid->game != FUSION_ASSET_GAME_METROID || !aria ||
        aria->game != FUSION_ASSET_GAME_ARIA || !metroid_database || !aria_database ||
        !output_root || !summary) {
        set_error(error, error_size, "invalid raw import request");
        return false;
    }
    FusionJson databases[2] = {{0}};
    RawEntry *entries[2] = {NULL, NULL};
    size_t counts[2] = {0, 0}, bytes = 0;
    Text manifest = {0};
    bool ok = fusion_json_parse(metroid_database, metroid_database_size, &databases[0],
                                error, error_size) &&
              fusion_json_parse(aria_database, aria_database_size, &databases[1],
                                error, error_size);
    const FusionRom *roms[2] = {metroid, aria};
    for (size_t world = 0; ok && world < 2; ++world)
        ok = read_entries(&databases[world], roms[world], world == 0, &entries[world],
                          &counts[world], error, error_size) &&
             write_world(output_root, roms[world], entries[world], counts[world], error,
                         error_size);
    if (ok) {
        const RawEntry *worlds[2] = {entries[0], entries[1]};
        ok = write_manifest(&manifest, worlds, counts, &bytes);
        if (!ok) set_error(error, error_size, "cannot serialize the import manifest");
    }
    ok = ok && write_below(output_root, "shared/manifests/import_catalog.json",
                           (const uint8_t *)manifest.data, manifest.size, error, error_size);
    if (ok) {
        *summary = (FusionRawImportSummary){counts[0], counts[1], bytes};
        if (error && error_size) error[0] = '\0';
    }
    fusion_json_free(&databases[0]);
    fusion_json_free(&databases[1]);
    free(entries[0]);
    free(entries[1]);
    free(manifest.data);
    return ok;
}
