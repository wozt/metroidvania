/* SPDX-License-Identifier: GPL-3.0-only */
/* The verified case-grid world overview and its bounded preview batches
 * (formerly scripts/world_overview.py and scripts/mzm_world_atlas.py). The
 * Zero Mission positions are original minimap anchors, not room extents;
 * the Aria positions are native 64x35 minimap cells. No synthetic geometry.
 * ROM-derived pixels stay private under assets/extracted. */
#define _DEFAULT_SOURCE
#include "world_overview.h"

#include "aria_render.h"
#include "catalog.h"
#include "fusion/assets/rom.h"
#include "generated.h"
#include "mzm_render.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

static const char *const areas[] = {"Brinstar", "Kraid",  "Norfair", "Ridley",
                                    "Tourian",  "Crateria", "Chozodia"};
enum { AREA_COUNT = 7 };

typedef struct {
    char *data;
    size_t size, capacity;
    bool failed;
} Buffer;

static void append(Buffer *buffer, const char *format, ...) {
    if (buffer->failed) return;
    va_list arguments;
    for (;;) {
        size_t room = buffer->capacity - buffer->size;
        va_start(arguments, format);
        int length = vsnprintf(buffer->data ? buffer->data + buffer->size : NULL, room, format,
                               arguments);
        va_end(arguments);
        if (length < 0) {
            buffer->failed = true;
            return;
        }
        if ((size_t)length < room) {
            buffer->size += (size_t)length;
            return;
        }
        size_t capacity = buffer->capacity ? buffer->capacity * 2 : 4096;
        while (capacity - buffer->size <= (size_t)length) capacity *= 2;
        char *grown = realloc(buffer->data, capacity);
        if (!grown) {
            buffer->failed = true;
            return;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
}

static int area_index(const char *name) {
    for (int index = 0; index < AREA_COUNT; ++index)
        if (!strcmp(areas[index], name)) return index;
    return -1;
}

static bool is_decimal(const char *text) {
    if (!*text) return false;
    for (; *text; ++text)
        if (!isdigit((unsigned char)*text)) return false;
    return true;
}

static const char *skip_space(const char *cursor) {
    while (*cursor && isspace((unsigned char)*cursor)) ++cursor;
    return cursor;
}

static const char *parse_number(const char *cursor, int *value) {
    if (!isdigit((unsigned char)*cursor)) return NULL;
    *value = atoi(cursor);
    while (isdigit((unsigned char)*cursor)) ++cursor;
    return cursor;
}

static char *read_source(const char *repository, const char *relative, FebError *error) {
    char path[FEB_PATH_CAPACITY], *text = NULL;
    if (!feb_join(path, repository, relative) || !feb_read_text(path, 64 * 1024 * 1024, &text, NULL)) {
        feb_fail(error, FEB_EXIT_OPERATION, "[Errno 2] No such file or directory: '%s'", path);
        return NULL;
    }
    return text;
}

static bool write_text(const char *repository, const char *relative, const Buffer *text,
                       FebError *error) {
    if (text->failed) return feb_oom(error);
    return feb_write_generated(repository, relative, (const uint8_t *)(text->data ? text->data : ""),
                               text->size, NULL, error);
}

/* ---- Door tables ------------------------------------------------------- */

typedef struct {
    int index;
    char type[256];
    int source_room, destination_door, x_start, x_end, y_start, y_end;
} Door;

typedef struct {
    char area[32];
    Door *doors;
    size_t count;
} DoorTable;

static void free_tables(DoorTable *tables, size_t count) {
    for (size_t index = 0; index < count; ++index) free(tables[index].doors);
    free(tables);
}

static const DoorTable *table_of(const DoorTable *tables, size_t count, const char *area) {
    for (size_t index = 0; index < count; ++index)
        if (!strcmp(tables[index].area, area)) return &tables[index];
    return NULL;
}

/* decode_door_tables(): every native door entry of the seven game areas. */
static bool door_tables(const char *source, const FusionJson *rooms, DoorTable **tables,
                        size_t *count, FebError *error) {
    *tables = NULL;
    *count = 0;
    int counts[AREA_COUNT] = {0};
    for (size_t index = 0; index < rooms->count; ++index) {
        int area = area_index(fusion_json_member(&rooms->items[index], "area")->string);
        if (area >= 0) ++counts[area];
    }
    static const char *const required[] = {"type", "sourceRoom", "destinationDoor", "xStart",
                                           "xEnd", "yStart", "yEnd"};
    const char *prefix = "const struct Door s";
    bool ok = true;
    for (const char *cursor = source; ok && (cursor = strstr(cursor, prefix)) != NULL;) {
        const char *name = cursor + strlen(prefix), *probe = name;
        cursor += 1;
        while (isalnum((unsigned char)*probe)) ++probe;
        if (probe - name < 6 || strncmp(probe - 5, "Doors", 5) || *probe != '[') continue;
        int declared = 0;
        const char *next = parse_number(probe + 1, &declared);
        if (!next || *next != ']') continue;
        next = skip_space(next + 1);
        if (*next != '=') continue;
        next = skip_space(next + 1);
        if (*next != '{') continue;
        const char *end = strstr(next + 1, "\n};");
        if (!end) continue;
        cursor = end;
        char area[32];
        snprintf(area, sizeof area, "%.*s", (int)(probe - 5 - name), name);
        int area_number = area_index(area);
        if (area_number < 0) continue;
        if (table_of(*tables, *count, area)) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "duplicate door table: %s", area);
            break;
        }
        DoorTable *grown = realloc(*tables, (*count + 1) * sizeof *grown);
        if (!grown) {
            ok = feb_oom(error);
            break;
        }
        *tables = grown;
        DoorTable *table = &(*tables)[(*count)++];
        *table = (DoorTable){{0}, NULL, 0};
        snprintf(table->area, sizeof table->area, "%s", area);
        int index = 0;
        for (const char *open = next + 1; ok && open < end; ++open) {
            if (*open != '{') continue;
            const char *close = open + 1;
            while (close < end && *close != '{' && *close != '}') ++close;
            if (close >= end || *close != '}') continue;
            FusionJson fields = fusion_json_object();
            for (const char *dot = open + 1; ok && dot < close; ++dot) {
                if (*dot != '.' || !isalnum((unsigned char)dot[1])) continue;
                const char *key = dot + 1, *key_end = key;
                while (key_end < close && isalnum((unsigned char)*key_end)) ++key_end;
                const char *value = key_end;
                while (value < close && isspace((unsigned char)*value)) ++value;
                if (value >= close || *value != '=') continue;
                ++value;
                while (value < close && isspace((unsigned char)*value)) ++value;
                const char *value_end = value;
                while (value_end < close && *value_end != ',' && *value_end != '\n') ++value_end;
                if (value_end == value) continue;
                const char *trimmed = value_end;
                while (trimmed > value && isspace((unsigned char)trimmed[-1])) --trimmed;
                char key_text[64], value_text[256];
                snprintf(key_text, sizeof key_text, "%.*s", (int)(key_end - key), key);
                snprintf(value_text, sizeof value_text, "%.*s", (int)(trimmed - value), value);
                ok = feb_put_string(&fields, key_text, value_text) || feb_oom(error);
                dot = value_end - 1;
            }
            for (size_t key = 0; ok && key < COUNT(required); ++key)
                if (!fusion_json_member(&fields, required[key]))
                    ok = feb_fail(error, FEB_EXIT_OPERATION, "%s door %d: missing fields", area,
                                  index);
            const char *type = ok ? fusion_json_member(&fields, "type")->string : "";
            bool type_ok = ok && *type;
            for (const char *c = type; type_ok && *c; ++c)
                if (!(isupper((unsigned char)*c) || isdigit((unsigned char)*c) || *c == '_' ||
                      *c == ' ' || *c == '|'))
                    type_ok = false;
            if (ok && !type_ok)
                ok = feb_fail(error, FEB_EXIT_OPERATION, "%s door %d: unsupported door type", area,
                              index);
            int values[6] = {0};
            for (size_t key = 1; ok && key < COUNT(required); ++key) {
                const char *text = fusion_json_member(&fields, required[key])->string;
                if (!is_decimal(text))
                    ok = feb_fail(error, FEB_EXIT_OPERATION,
                                  "%s door %d: %s is not a decimal integer", area, index,
                                  required[key]);
                else values[key - 1] = atoi(text);
            }
            if (ok && values[0] >= counts[area_number])
                ok = feb_fail(error, FEB_EXIT_OPERATION, "%s door %d: unknown source room", area,
                              index);
            Door *doors = ok ? realloc(table->doors, (table->count + 1) * sizeof *doors) : NULL;
            if (ok && !doors) ok = feb_oom(error);
            if (ok) {
                table->doors = doors;
                Door *door = &doors[table->count++];
                door->index = index;
                snprintf(door->type, sizeof door->type, "%s", type);
                door->source_room = values[0];
                door->destination_door = values[1];
                door->x_start = values[2];
                door->x_end = values[3];
                door->y_start = values[4];
                door->y_end = values[5];
            }
            fusion_json_free(&fields);
            ++index;
            open = close;
        }
        if (ok && (int)table->count != declared)
            ok = feb_fail(error, FEB_EXIT_OPERATION, "%s: %zu door entries != declared %d", area,
                          table->count, declared);
    }
    for (int area = 0; ok && area < AREA_COUNT; ++area)
        if (!table_of(*tables, *count, areas[area]))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "missing original area door table");
    if (!ok) {
        free_tables(*tables, *count);
        *tables = NULL;
        *count = 0;
    }
    return ok;
}

/* ---- mzm_world_atlas.run() --------------------------------------------- */

static bool world_atlas(const char *repository, FebError *error) {
    FusionJson rooms;
    if (!feb_native_catalog(repository, "zero_mission", &rooms, error)) return false;
    char *source = read_source(repository, "third_party/mzm/src/data/rooms_data.c", error);
    DoorTable *tables = NULL;
    size_t table_count = 0;
    bool ok = source && door_tables(source, &rooms, &tables, &table_count, error);
    free(source);
    FusionJson atlas = fusion_json_object(), list = fusion_json_array(),
               connections = fusion_json_array();
    Buffer tsv = {0};
    append(&tsv, "# MV_MZM_WORLD_ATLAS_1\n# R|area|index|mapX|mapY|tileset|music|saveRelated\n"
                 "# D|area|sourceRoom|destinationRoom|sourceDoor|destinationDoor\n");
    int64_t unresolved = 0, entries = 0;
    for (size_t index = 0; ok && index < rooms.count; ++index) {
        const FusionJson *room = &rooms.items[index];
        const char *area = fusion_json_member(room, "area")->string;
        if (area_index(area) < 0) continue;
        const FusionJson *fields = fusion_json_member(room, "fields");
        static const char *const numeric[] = {"mapX", "mapY", "tileset"};
        for (size_t field = 0; ok && field < 3; ++field)
            if (!is_decimal(fusion_json_member(fields, numeric[field])->string))
                ok = feb_fail(error, FEB_EXIT_OPERATION, "nondecimal %s for %s", numeric[field],
                              fusion_json_member(room, "native_id")->string);
        if (!ok) break;
        int x = atoi(fusion_json_member(fields, "mapX")->string);
        int y = atoi(fusion_json_member(fields, "mapY")->string);
        if (x >= 128 || y >= 128) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid minimap anchor: %s",
                          fusion_json_member(room, "native_id")->string);
            break;
        }
        const char *music = fusion_json_member(fields, "musicTrack")->string;
        const FusionJson *bg3 = fusion_json_member(fields, "pBg3Data");
        bool save = strstr(music, "SAVE") || (bg3 && strstr(bg3->string, "SaveRoom"));
        int64_t number = fusion_json_member(room, "room")->integer;
        FusionJson entry = fusion_json_object();
        ok = (feb_put_string(&entry, "area", area) && feb_put_int(&entry, "index", number) &&
              feb_put_int(&entry, "x", x) && feb_put_int(&entry, "y", y) &&
              feb_put_int(&entry, "tileset", atoi(fusion_json_member(fields, "tileset")->string)) &&
              feb_put_string(&entry, "music", music) && feb_put_bool(&entry, "save_related", save) &&
              fusion_json_push(&list, entry)) || feb_oom(error);
        append(&tsv, "R|%s|%lld|%d|%d|%d|%s|%d\n", area, (long long)number, x, y,
               atoi(fusion_json_member(fields, "tileset")->string), music, save ? 1 : 0);
    }
    Buffer edges = {0};
    for (int area = 0; ok && area < AREA_COUNT; ++area) {
        const DoorTable *table = table_of(tables, table_count, areas[area]);
        entries += (int64_t)table->count;
        for (size_t index = 0; ok && index < table->count; ++index) {
            const Door *door = &table->doors[index];
            if (strstr(door->type, "DOOR_TYPE_AREA_CONNECTION") ||
                door->destination_door >= (int)table->count) {
                ++unresolved;
                continue;
            }
            const Door *destination = &table->doors[door->destination_door];
            if (destination->source_room == door->source_room) continue;
            FusionJson edge = fusion_json_object();
            ok = (feb_put_string(&edge, "area", areas[area]) &&
                  feb_put_int(&edge, "from", door->source_room) &&
                  feb_put_int(&edge, "to", destination->source_room) &&
                  feb_put_int(&edge, "door", door->index) &&
                  feb_put_int(&edge, "destination_door", door->destination_door) &&
                  feb_put_string(&edge, "type", door->type) && feb_put_int(&edge, "x", door->x_start) &&
                  feb_put_int(&edge, "y", door->y_start) && fusion_json_push(&connections, edge)) ||
                 feb_oom(error);
            append(&edges, "D|%s|%d|%d|%d|%d\n", areas[area], door->source_room,
                   destination->source_room, door->index, door->destination_door);
        }
    }
    append(&tsv, "%s", edges.data ? edges.data : "");
    free(edges.data);
    if (ok) {
        static const char *const warnings[] = {
            "Only original intra-area door links resolved",
            "Area connections and event-dependent redirects not resolved",
            "Room minimap anchors are NOT room bounds", "No Castlevania world map yet"};
        FusionJson summary = fusion_json_object(), notes = fusion_json_array();
        for (size_t index = 0; ok && index < COUNT(warnings); ++index)
            ok = fusion_json_push(&notes, fusion_json_string(warnings[index]));
        ok = (ok && feb_put_int(&summary, "door_entries", entries) &&
              feb_put_int(&summary, "unresolved_area_or_unknown_doors", unresolved) &&
              feb_put_string(&atlas, "format", "MV_MZM_WORLD_ATLAS_1") &&
              feb_put(&atlas, "rooms", list) && feb_put(&atlas, "connections", connections) &&
              feb_put(&atlas, "summary", summary) && feb_put(&atlas, "warnings", notes)) ||
             feb_oom(error);
        list = connections = fusion_json_null();
        char *json = NULL;
        size_t size = 0;
        Buffer text = {0};
        if (ok && fusion_json_write(&atlas, 2, &json, &size)) append(&text, "%s\n", json);
        else if (ok) ok = feb_oom(error);
        free(json);
        ok = ok && write_text(repository, "rooms/metroid/world_atlas.json", &text, error) &&
             write_text(repository, "rooms/metroid/world_atlas.tsv", &tsv, error);
        free(text.data);
    }
    free(tsv.data);
    fusion_json_free(&list);
    fusion_json_free(&connections);
    fusion_json_free(&atlas);
    free_tables(tables, table_count);
    fusion_json_free(&rooms);
    return ok;
}

/* ---- The Zero Mission index -------------------------------------------- */

typedef struct {
    int area, room, x, y, save, warp;
} Anchor;

typedef struct {
    int area, room, x, y, save, warp, provenance, tile;
    char note[192];
    bool has_note;
} Row;

/* build_mzm(): the R rows of the world atlas TSV. */
static bool parse_anchors(const char *text, Anchor **anchors, size_t *count, FebError *error) {
    *anchors = NULL;
    *count = 0;
    char *copy = strdup(text);
    if (!copy) return feb_oom(error);
    bool ok = true;
    for (char *line = copy, *next; ok && line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        size_t length = strlen(line);
        if (length && line[length - 1] == '\r') line[--length] = '\0';
        if (!length || line[0] == '#') continue;
        char *columns[16];
        int fields = 0;
        for (char *part = line;; ) {
            if (fields < 16) columns[fields] = part;
            ++fields;
            char *bar = strchr(part, '|');
            if (!bar) break;
            *bar = '\0';
            part = bar + 1;
        }
        if (strcmp(columns[0], "R")) continue;
        int area = fields == 8 ? area_index(columns[1]) : -1;
        if (area < 0) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "bad MZM room row");
            break;
        }
        int room = atoi(columns[2]), x = atoi(columns[3]), y = atoi(columns[4]);
        int save = atoi(columns[7]);
        if (room < 0 || room >= 256 || x < 0 || x >= 128 || y < 0 || y >= 128 ||
            (save != 0 && save != 1)) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid MZM room coordinate");
            break;
        }
        Anchor *grown = realloc(*anchors, (*count + 1) * sizeof *grown);
        if (!grown) {
            ok = feb_oom(error);
            break;
        }
        *anchors = grown;
        (*anchors)[(*count)++] = (Anchor){area, room, x, y, save, 0};
    }
    free(copy);
    if (ok && !*count) ok = feb_fail(error, FEB_EXIT_OPERATION, "empty MZM room atlas");
    if (!ok) {
        free(*anchors);
        *anchors = NULL;
        *count = 0;
    }
    return ok;
}

static const Anchor *origin_of(const Anchor *anchors, size_t count, int area, int room) {
    const Anchor *found = NULL;
    for (size_t index = 0; index < count; ++index)
        if (anchors[index].area == area && anchors[index].room == room) found = &anchors[index];
    return found;  /* the last anchor wins, as a dict comprehension */
}

/* mzm_global_doors(). */
static bool global_doors(const DoorTable *tables, size_t table_count, const Anchor *anchors,
                         size_t anchor_count, Buffer *text) {
    append(text, "# area|room|door_index|map_x|map_y|native_type|destination_room|"
                 "destination_door\n");
    for (int area = 0; area < AREA_COUNT; ++area) {
        const DoorTable *table = table_of(tables, table_count, areas[area]);
        for (size_t index = 0; index < table->count; ++index) {
            const Door *door = &table->doors[index];
            if (strstr(door->type, "DOOR_TYPE_NONE")) continue;
            const Anchor *origin = origin_of(anchors, anchor_count, area, door->source_room);
            if (!origin) continue;
            int x = origin->x + (door->x_start - 2 > 0 ? door->x_start - 2 : 0) / 15;
            int y = origin->y + (door->y_start - 2 > 0 ? door->y_start - 2 : 0) / 10;
            char destination_room[16] = "-", destination_door[16] = "-";
            int target = door->destination_door;
            if (!strstr(door->type, "DOOR_TYPE_AREA_CONNECTION") && target >= 0 &&
                target < (int)table->count && !strstr(table->doors[target].type, "DOOR_TYPE_NONE")) {
                snprintf(destination_room, sizeof destination_room, "%d",
                         table->doors[target].source_room);
                snprintf(destination_door, sizeof destination_door, "%d", target);
            }
            if (x < 0 || x >= 32 || y < 0 || y >= 32) continue;
            char type[256];
            snprintf(type, sizeof type, "%s", door->type);
            for (char *c = type; *c; ++c)
                if (*c == '|') *c = '+';
            append(text, "%d|%d|%d|%d|%d|%s|%s|%s\n", area, door->source_room, door->index, x, y,
                   type, destination_room, destination_door);
        }
    }
    return !text->failed;
}

/* native_mzm_minimap_cells(): the original 32x32 pause-screen minimaps. */
static bool minimap_cells(const char *repository, Row **cells, size_t *count, FebError *error) {
    *cells = NULL;
    *count = 0;
    char raw[FEB_PATH_CAPACITY];
    snprintf(raw, sizeof raw, "%s/assets/extracted/metroid/raw/data/menus/pause_screen", repository);
    bool missing = false;
    for (int area = 0; area < AREA_COUNT; ++area) {
        char path[FEB_PATH_CAPACITY + 64], lower[32];
        snprintf(lower, sizeof lower, "%s", areas[area]);
        for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
        snprintf(path, sizeof path, "%s/%s_minimap.tt", raw, lower);
        long long size = 0;
        if (!feb_is_regular(path, &size)) missing = true;
    }
    FusionRom rom = {0};
    FusionJson database = fusion_json_null();
    bool have_rom = false;
    if (missing) {
        char rom_path[FEB_PATH_CAPACITY], database_path[FEB_PATH_CAPACITY], detail[256];
        long long size = 0;
        if (feb_join(rom_path, repository, "roms/Metroid - Zero Mission (USA).gba") &&
            feb_join(database_path, repository, "third_party/mzm/database.json") &&
            feb_is_regular(rom_path, &size) && feb_is_regular(database_path, &size)) {
            if (!fusion_rom_open(FUSION_ASSET_GAME_METROID, rom_path, &rom, detail, sizeof detail))
                return feb_fail(error, FEB_EXIT_OPERATION, "%s", detail);
            if (!feb_load_json(database_path, 256 * 1024 * 1024, &database, detail, sizeof detail)) {
                fusion_rom_close(&rom);
                return feb_fail(error, FEB_EXIT_OPERATION, "%s", detail);
            }
            have_rom = true;
        }
    }
    bool ok = true;
    for (int area = 0; ok && area < AREA_COUNT; ++area) {
        char path[FEB_PATH_CAPACITY + 64], lower[32], filename[64];
        snprintf(lower, sizeof lower, "%s", areas[area]);
        for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
        snprintf(filename, sizeof filename, "%s_minimap.tt", lower);
        snprintf(path, sizeof path, "%s/%s", raw, filename);
        uint8_t *payload = NULL;
        size_t payload_size = 0;
        long long size = 0;
        if (feb_is_symlink(path)) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "symlinked private MZM minimap source refused");
            break;
        }
        if (feb_is_regular(path, &size)) {
            if (size > 16384) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "oversized private MZM minimap source");
                break;
            }
            if (!feb_read_text(path, 16384, (char **)&payload, &payload_size)) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "cannot read %s", path);
                break;
            }
        } else if (have_rom) {
            const FusionJson *item = NULL;
            for (size_t index = 0; index < database.count; ++index) {
                const FusionJson *value = fusion_json_member(&database.items[index], "path");
                if (!value || value->kind != FUSION_JSON_STRING ||
                    strncmp(value->string, "menus/pause_screen/", 19))
                    continue;
                const char *slash = strrchr(value->string, '/');
                if (!strcmp(slash + 1, filename)) item = &database.items[index];
            }
            if (!item) continue;
            unsigned long offset =
                strtoul(fusion_json_member(fusion_json_member(item, "addr"), "us")->string, NULL, 16);
            const FusionJson *width = fusion_json_member(item, "size");
            unsigned long length =
                strtoul(fusion_json_member(item, "count")->string, NULL, 16) *
                (width->kind == FUSION_JSON_NUMBER ? (unsigned long)width->integer
                                                   : strtoul(width->string, NULL, 10));
            if (!length || length > 16384 || offset + length > rom.size) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid verified MZM minimap database offset");
                break;
            }
            payload = malloc(length);
            if (!payload) {
                ok = feb_oom(error);
                break;
            }
            memcpy(payload, rom.data + offset, length);
            payload_size = length;
        } else {
            continue;
        }
        uint8_t *decoded = NULL;
        size_t decoded_size = 0;
        ok = feb_lz77(payload, payload_size, 2048, &decoded, &decoded_size, error);
        free(payload);
        if (ok && decoded_size != 32 * 32 * 2)
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid native MZM minimap size for %s",
                          areas[area]);
        for (int y = 0; ok && y < 32; ++y)
            for (int x = 0; ok && x < 32; ++x) {
                int tile = decoded[(y * 32 + x) * 2] | (decoded[(y * 32 + x) * 2 + 1] << 8);
                if ((tile & 0x03FF) == 0x140) continue;
                Row *grown = realloc(*cells, (*count + 1) * sizeof *grown);
                if (!grown) {
                    ok = feb_oom(error);
                    break;
                }
                *cells = grown;
                (*cells)[(*count)++] = (Row){area, 999, x, y, 0, 0, 3, tile, "", false};
            }
        free(decoded);
    }
    fusion_json_free(&database);
    if (have_rom) fusion_rom_close(&rom);
    if (!ok) {
        free(*cells);
        *cells = NULL;
        *count = 0;
    }
    return ok;
}

/* A set of (area, x, y) points or (x, y) cells, kept small and sorted. */
typedef struct {
    int (*items)[3];
    size_t count, capacity;
} Points;

static bool points_has(const Points *points, int a, int b, int c) {
    for (size_t index = 0; index < points->count; ++index)
        if (points->items[index][0] == a && points->items[index][1] == b &&
            points->items[index][2] == c)
            return true;
    return false;
}

static bool points_add(Points *points, int a, int b, int c) {
    if (points_has(points, a, b, c)) return true;
    if (points->count == points->capacity) {
        size_t capacity = points->capacity ? points->capacity * 2 : 16;
        int (*grown)[3] = realloc(points->items, capacity * sizeof *grown);
        if (!grown) return false;
        points->items = grown;
        points->capacity = capacity;
    }
    points->items[points->count][0] = a;
    points->items[points->count][1] = b;
    points->items[points->count][2] = c;
    ++points->count;
    return true;
}

/* native_mzm_clip_dimensions(): exact Clipdata bounds of the descriptors. */
typedef struct {
    int area, room, width, height;
} Dimension;

static bool clip_dimensions(const char *repository, Dimension **dimensions, size_t *count,
                            FebError *error) {
    *dimensions = NULL;
    *count = 0;
    char path[FEB_PATH_CAPACITY], *text = NULL;
    long long size = 0;
    snprintf(path, sizeof path, "%s/assets/extracted/rooms/metroid/rooms.tsv", repository);
    if (!feb_is_regular(path, &size)) return true;
    if (!feb_read_text(path, 64 * 1024 * 1024, &text, NULL))
        return feb_fail(error, FEB_EXIT_OPERATION, "cannot read %s", path);
    bool ok = true;
    for (char *line = text, *next; ok && line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        if (!*line || line[0] == '#') continue;
        char *columns[12];
        int fields = 0;
        for (char *part = line;;) {
            if (fields < 12) columns[fields] = part;
            ++fields;
            char *bar = strchr(part, '|');
            if (!bar) break;
            *bar = '\0';
            part = bar + 1;
        }
        if (fields != 10) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "malformed verified MZM descriptor catalog");
            break;
        }
        int area = area_index(columns[0]);
        if (area < 0) continue;
        char *end = NULL;
        long room = strtol(columns[1], &end, 10);
        if (end == columns[1] || *end) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid MZM room ID in descriptor catalog");
            break;
        }
        if (room < 0 || room >= 256) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "MZM descriptor room index out of bounds");
            break;
        }
        FebBlockMap map = {0};
        FebError ignored = {0};
        if (!feb_mzm_room_map(repository, columns[6], &map, &ignored)) continue;
        bool replaced = false;
        for (size_t index = 0; index < *count; ++index)
            if ((*dimensions)[index].area == area && (*dimensions)[index].room == room) {
                (*dimensions)[index].width = (int)map.width;
                (*dimensions)[index].height = (int)map.height;
                replaced = true;
            }
        if (!replaced) {
            Dimension *grown = realloc(*dimensions, (*count + 1) * sizeof *grown);
            if (!grown) ok = feb_oom(error);
            else {
                *dimensions = grown;
                (*dimensions)[(*count)++] = (Dimension){area, (int)room, (int)map.width,
                                                        (int)map.height};
            }
        }
        feb_block_map_free(&map);
    }
    free(text);
    if (!ok) {
        free(*dimensions);
        *dimensions = NULL;
        *count = 0;
    }
    return ok;
}

/* The *.c files of one pinned room directory, in sorted order. */
static char **room_files(const char *repository, const char *area, size_t *count) {
    char directory[FEB_PATH_CAPACITY], lower[32];
    snprintf(lower, sizeof lower, "%s", area);
    for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
    snprintf(directory, sizeof directory, "%s/third_party/mzm/src/data/rooms/%s", repository, lower);
    *count = 0;
    DIR *handle = opendir(directory);
    if (!handle) return NULL;
    char **files = NULL;
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        size_t length = strlen(entry->d_name);
        if (length < 3 || strcmp(entry->d_name + length - 2, ".c")) continue;
        char **grown = realloc(files, (*count + 1) * sizeof *grown);
        if (!grown) break;
        files = grown;
        size_t size = strlen(directory) + length + 2;
        files[*count] = malloc(size);
        if (!files[*count]) break;
        snprintf(files[*count], size, "%s/%s", directory, entry->d_name);
        ++*count;
    }
    closedir(handle);
    for (size_t pass = 1; pass < *count; ++pass)
        for (size_t at = pass; at > 0 && strcmp(files[at - 1], files[at]) > 0; --at) {
            char *swap = files[at];
            files[at] = files[at - 1];
            files[at - 1] = swap;
        }
    return files;
}

static void free_files(char **files, size_t count) {
    for (size_t index = 0; index < count; ++index) free(files[index]);
    free(files);
}

typedef struct {
    int area, room;
    int (*regions)[8];
    size_t count;
} Scroll;

static void free_scrolls(Scroll *scrolls, size_t count) {
    for (size_t index = 0; index < count; ++index) free(scrolls[index].regions);
    free(scrolls);
}

/* parse_mzm_scroll_regions() over the concatenated sources of an area. */
static bool scroll_regions(const char *source, int area, Scroll **scrolls, size_t *count,
                           FebError *error) {
    const char *prefix = "const u8 s";
    for (const char *cursor = source; (cursor = strstr(cursor, prefix)) != NULL;) {
        const char *name = cursor + strlen(prefix), *probe = name;
        cursor += 1;
        while (isalpha((unsigned char)*probe)) ++probe;
        int number = 0, declared = 0;
        if (probe == name || *probe != '_') continue;
        const char *next = parse_number(probe + 1, &number);
        if (!next || strncmp(next, "_Scrolls[SCROLL_DATA_SIZE(", 26)) continue;
        next = parse_number(next + 26, &declared);
        if (!next || strncmp(next, ")]", 2)) continue;
        next = skip_space(next + 2);
        if (*next != '=') continue;
        next = skip_space(next + 1);
        if (*next != '{') continue;
        const char *end = strstr(next + 1, "\n};");
        if (!end) continue;
        cursor = end;
        char found_area[32];
        snprintf(found_area, sizeof found_area, "%.*s", (int)(probe - name), name);
        if (strcmp(found_area, areas[area]))
            return feb_fail(error, FEB_EXIT_OPERATION, "unexpected MZM scroll area %s, wanted %s",
                            found_area, areas[area]);
        /* Comments removed, then UCHAR_MAX or standalone decimal tokens. */
        int *values = NULL;
        size_t value_count = 0;
        bool comment = false;
        for (const char *c = next + 1; c < end;) {
            if (comment) {
                if (*c == '\n') comment = false;
                ++c;
                continue;
            }
            if (c[0] == '/' && c + 1 < end && c[1] == '/') {
                comment = true;
                c += 2;
                continue;
            }
            int token = -1;
            const char *after = c + 1;
            if (!strncmp(c, "UCHAR_MAX", 9) && c + 9 <= end) {
                token = 255;
                after = c + 9;
            } else if (isdigit((unsigned char)*c) &&
                       (c == next + 1 || !(isalnum((unsigned char)c[-1]) || c[-1] == '_'))) {
                const char *run = c;
                while (run < end && isdigit((unsigned char)*run)) ++run;
                if (run >= end || !(isalnum((unsigned char)*run) || *run == '_'))
                    token = atoi(c);
                after = run;
            }
            if (token >= 0) {
                int *grown = realloc(values, (value_count + 1) * sizeof *grown);
                if (!grown) {
                    free(values);
                    return feb_oom(error);
                }
                values = grown;
                values[value_count++] = token;
            }
            c = after;
        }
        bool ok = value_count == 2 + (size_t)declared * 8 && values[1] == declared;
        if (!ok) {
            free(values);
            return feb_fail(error, FEB_EXIT_OPERATION, "incomplete %s scroll data", areas[area]);
        }
        int room = values[0];
        bool duplicate = false;
        for (size_t index = 0; index < *count; ++index)
            if ((*scrolls)[index].area == area && (*scrolls)[index].room == room) duplicate = true;
        if (room < 0 || room >= 256 || duplicate) {
            free(values);
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid or duplicate %s scroll room %d",
                            areas[area], room);
        }
        Scroll scroll = {area, room, calloc((size_t)(declared ? declared : 1), sizeof *scroll.regions),
                         (size_t)declared};
        if (!scroll.regions) {
            free(values);
            return feb_oom(error);
        }
        for (int region = 0; region < declared; ++region) {
            const int *item = values + 2 + region * 8;
            bool valid = item[0] <= item[1] && item[2] <= item[3] &&
                         (item[6] <= 3 || item[6] == 255);
            for (int field = 0; field < 8; ++field)
                if (item[field] > 255) valid = false;
            if (!valid) {
                free(scroll.regions);
                free(values);
                return feb_fail(error, FEB_EXIT_OPERATION, "invalid %s scroll bounds for room %d",
                                areas[area], room);
            }
            if (item[6] == 255 && item[7] != 255) {
                free(scroll.regions);
                free(values);
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "orphan %s scroll extension for room %d", areas[area], room);
            }
            memcpy(scroll.regions[region], item, sizeof scroll.regions[region]);
        }
        free(values);
        Scroll *grown = realloc(*scrolls, (*count + 1) * sizeof *grown);
        if (!grown) {
            free(scroll.regions);
            return feb_oom(error);
        }
        *scrolls = grown;
        (*scrolls)[(*count)++] = scroll;
    }
    return true;
}

static bool all_scroll_regions(const char *repository, Scroll **scrolls, size_t *count,
                               FebError *error) {
    *scrolls = NULL;
    *count = 0;
    for (int area = 0; area < AREA_COUNT; ++area) {
        size_t file_count = 0;
        char **files = room_files(repository, areas[area], &file_count);
        if (!files) continue;
        Buffer source = {0};
        for (size_t index = 0; index < file_count; ++index) {
            char *text = NULL;
            if (!feb_read_text(files[index], 64 * 1024 * 1024, &text, NULL)) {
                free(source.data);
                free_files(files, file_count);
                return feb_fail(error, FEB_EXIT_OPERATION, "cannot read %s", files[index]);
            }
            append(&source, "%s%s", index ? "\n" : "", text);
            free(text);
        }
        free_files(files, file_count);
        if (source.failed) {
            free(source.data);
            return feb_oom(error);
        }
        bool ok = scroll_regions(source.data ? source.data : "", area, scrolls, count, error);
        free(source.data);
        if (!ok) {
            free_scrolls(*scrolls, *count);
            *scrolls = NULL;
            *count = 0;
            return false;
        }
    }
    return true;
}

/* _scroll_local_cells(). */
static bool scroll_local_cells(const Scroll *scroll, Points *cells) {
    for (size_t index = 0; index < scroll->count; ++index) {
        const int *region = scroll->regions[index];
        int variants[2][4], count = 1;
        memcpy(variants[0], region, sizeof variants[0]);
        int direction = region[6], extension = region[7];
        if (direction <= 3 && extension != 255) {
            memcpy(variants[1], region, sizeof variants[1]);
            variants[1][direction] = extension;
            count = 2;
        }
        for (int variant = 0; variant < count; ++variant) {
            int x_start = variants[variant][0], x_end = variants[variant][1];
            int y_start = variants[variant][2], y_end = variants[variant][3];
            if (x_start > x_end || y_start > y_end) continue;
            for (int y = y_start; y <= y_end; ++y)
                for (int x = x_start; x <= x_end; ++x)
                    if (!points_add(cells, 0, (x - 2 > 0 ? x - 2 : 0) / 15,
                                    (y - 2 > 0 ? y - 2 : 0) / 10))
                        return false;
        }
    }
    return true;
}

typedef struct {
    int area, room;
    Points points;
} Evidence;

static Points *evidence_for(Evidence **evidence, size_t *count, int area, int room) {
    for (size_t index = 0; index < *count; ++index)
        if ((*evidence)[index].area == area && (*evidence)[index].room == room)
            return &(*evidence)[index].points;
    Evidence *grown = realloc(*evidence, (*count + 1) * sizeof *grown);
    if (!grown) return NULL;
    *evidence = grown;
    (*evidence)[*count] = (Evidence){area, room, {0}};
    return &(*evidence)[(*count)++].points;
}

static const Points *evidence_of(const Evidence *evidence, size_t count, int area, int room) {
    for (size_t index = 0; index < count; ++index)
        if (evidence[index].area == area && evidence[index].room == room)
            return &evidence[index].points;
    return NULL;
}

/* native_mzm_room_evidence(): doors and sprite placements per room. */
static bool room_evidence(const char *repository, const DoorTable *tables, size_t table_count,
                          const Anchor *anchors, size_t anchor_count, Evidence **evidence,
                          size_t *count, FebError *error) {
    *evidence = NULL;
    *count = 0;
    for (int area = 0; area < AREA_COUNT; ++area) {
        const DoorTable *table = table_of(tables, table_count, areas[area]);
        for (size_t index = 0; index < table->count; ++index) {
            const Door *door = &table->doors[index];
            if (strstr(door->type, "DOOR_TYPE_NONE")) continue;
            const Anchor *origin = origin_of(anchors, anchor_count, area, door->source_room);
            if (!origin) continue;
            Points *points = evidence_for(evidence, count, area, door->source_room);
            if (!points) return feb_oom(error);
            for (int y = door->y_start; y <= door->y_end; ++y)
                for (int x = door->x_start; x <= door->x_end; ++x)
                    if (!points_add(points, area, origin->x + (x - 2 > 0 ? x - 2 : 0) / 15,
                                    origin->y + (y - 2 > 0 ? y - 2 : 0) / 10))
                        return feb_oom(error);
        }
        size_t file_count = 0;
        char **files = room_files(repository, areas[area], &file_count);
        for (size_t file = 0; file < file_count; ++file) {
            char *source = NULL;
            if (!feb_read_text(files[file], 64 * 1024 * 1024, &source, NULL)) {
                free_files(files, file_count);
                return feb_fail(error, FEB_EXIT_OPERATION, "cannot read %s", files[file]);
            }
            /* PLACEMENT_RE and PLACEMENT_ITEM_RE. */
            const char *prefix = "const u8 s";
            for (const char *cursor = source; (cursor = strstr(cursor, prefix)) != NULL;) {
                const char *name = cursor + strlen(prefix), *probe = name;
                cursor += 1;
                while (isalnum((unsigned char)*probe)) ++probe;
                int room = 0, variant = 0;
                if (probe == name || *probe != '_') continue;
                const char *next = parse_number(probe + 1, &room);
                if (!next || strncmp(next, "_Spriteset", 10)) continue;
                next = parse_number(next + 10, &variant);
                if (!next || *next != '[') continue;
                const char *bracket = next + 1;
                while (*bracket && *bracket != ']') ++bracket;
                if (bracket == next + 1 || *bracket != ']') continue;
                const char *after = skip_space(bracket + 1);
                if (*after != '=') continue;
                after = skip_space(after + 1);
                if (*after != '{') continue;
                const char *body = after + 1, *end = NULL;
                for (const char *line = strchr(body, '\n'); line; line = strchr(line + 1, '\n'))
                    if (!strncmp(skip_space(line + 1), "};", 2)) {
                        end = line;
                        break;
                    }
                if (!end) continue;
                cursor = end;
                const Anchor *origin = origin_of(anchors, anchor_count, area, room);
                if ((size_t)(probe - name) != strlen(areas[area]) ||
                    strncmp(name, areas[area], strlen(areas[area])) || !origin)
                    continue;
                Points *points = evidence_for(evidence, count, area, room);
                if (!points) {
                    free(source);
                    free_files(files, file_count);
                    return feb_oom(error);
                }
                for (const char *item = body; item < end; ++item) {
                    if (!isdigit((unsigned char)*item) ||
                        (item > body && isdigit((unsigned char)item[-1])))
                        continue;
                    int values[3];
                    const char *scan = parse_number(item, &values[0]);
                    while (scan < end && isspace((unsigned char)*scan)) ++scan;
                    if (*scan != ',') continue;
                    scan = parse_number(skip_space(scan + 1), &values[1]);
                    if (!scan) continue;
                    while (scan < end && isspace((unsigned char)*scan)) ++scan;
                    if (*scan != ',') continue;
                    scan = skip_space(scan + 1);
                    if (strncmp(scan, "SPRITESET_IDX(", 14)) continue;
                    scan = parse_number(scan + 14, &values[2]);
                    if (!scan || *scan != ')') continue;
                    if (!points_add(points, area,
                                    origin->x + (values[1] - 2 > 0 ? values[1] - 2 : 0) / 15,
                                    origin->y + (values[0] - 2 > 0 ? values[0] - 2 : 0) / 10)) {
                        free(source);
                        free_files(files, file_count);
                        return feb_oom(error);
                    }
                    item = scan;
                }
            }
            free(source);
        }
        free_files(files, file_count);
    }
    return true;
}

typedef struct {
    int area, x, y;
    int *rooms;
    size_t room_count;
    Points points;
    bool save, warp;
} Family;

static int compare_ints(const void *left, const void *right) {
    return *(const int *)left - *(const int *)right;
}

/* sorted(set(rooms)) into ``out``; the unique count is returned. */
static size_t unique_rooms(const int *rooms, size_t count, int *out) {
    memcpy(out, rooms, count * sizeof *out);
    qsort(out, count, sizeof *out, compare_ints);
    size_t unique = 0;
    for (size_t index = 0; index < count; ++index)
        if (!unique || out[unique - 1] != out[index]) out[unique++] = out[index];
    return unique;
}

static bool family_evidenced(const Family *family, const Evidence *evidence, size_t count,
                             int area, int x, int y) {
    for (size_t index = 0; index < family->room_count; ++index) {
        const Points *points = evidence_of(evidence, count, family->area, family->rooms[index]);
        if (points && points_has(points, area, x, y)) return true;
    }
    return false;
}

static int compare_rows(const void *left, const void *right) {
    const Row *a = left, *b = right;
    if (a->area != b->area) return a->area - b->area;
    if (a->x != b->x) return a->x - b->x;
    return a->y - b->y;
}

static int compare_families(const void *left, const void *right) {
    const Family *a = *(Family *const *)left, *b = *(Family *const *)right;
    if (a->area != b->area) return a->area - b->area;
    if (a->x != b->x) return a->x - b->x;
    return a->y - b->y;
}

static void list_text(Buffer *buffer, const int *values, size_t count, const char *separator) {
    for (size_t index = 0; index < count; ++index)
        append(buffer, "%s%d", index ? separator : "", values[index]);
}

/* resolve_mzm_minimap_cells(): engine geometry joined to the original
 * minimap occupancy without guessing owners. */
static bool resolve_cells(const Anchor *anchors, size_t anchor_count, const Dimension *dimensions,
                          size_t dimension_count, Row *native, size_t native_count,
                          const Scroll *scrolls, size_t scroll_count, const Evidence *evidence,
                          size_t evidence_count, Row **rows, size_t *row_count, FusionJson *report,
                          FebError *error) {
    *rows = NULL;
    *row_count = 0;
    if (!anchor_count || !native_count)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "MZM ownership resolution needs anchors and native cells");
    for (size_t index = 0; index < native_count; ++index)
        for (size_t other = index + 1; other < native_count; ++other)
            if (native[index].area == native[other].area && native[index].x == native[other].x &&
                native[index].y == native[other].y)
                return feb_fail(error, FEB_EXIT_OPERATION, "duplicate native MZM minimap coordinate");
    Family *families = calloc(anchor_count, sizeof *families);
    size_t family_count = 0;
    int64_t scroll_bounded = 0;
    bool ok = families != NULL || feb_oom(error);
    for (size_t index = 0; ok && index < anchor_count; ++index) {
        const Anchor *anchor = &anchors[index];
        Family *family = NULL;
        for (size_t other = 0; other < family_count; ++other)
            if (families[other].area == anchor->area && families[other].x == anchor->x &&
                families[other].y == anchor->y)
                family = &families[other];
        if (!family) {
            family = &families[family_count++];
            *family = (Family){anchor->area, anchor->x, anchor->y, NULL, 0, {0}, false, false};
        }
        int *grown = realloc(family->rooms, (family->room_count + 1) * sizeof *grown);
        if (!grown) {
            ok = feb_oom(error);
            break;
        }
        family->rooms = grown;
        family->rooms[family->room_count++] = anchor->room;
        family->save |= anchor->save != 0;
        family->warp |= anchor->warp != 0;
        const Scroll *scroll = NULL;
        for (size_t other = 0; other < scroll_count; ++other)
            if (scrolls[other].area == anchor->area && scrolls[other].room == anchor->room)
                scroll = &scrolls[other];
        Points local = {0};
        if (scroll) {
            ok = scroll_local_cells(scroll, &local) || feb_oom(error);
            ++scroll_bounded;
        } else {
            const Dimension *bounds = NULL;
            for (size_t other = 0; other < dimension_count; ++other)
                if (dimensions[other].area == anchor->area && dimensions[other].room == anchor->room)
                    bounds = &dimensions[other];
            if (!bounds) continue;
            int width = bounds->width, height = bounds->height;
            if (width < 19 || height < 14 || (width - 4) % 15 || (height - 4) % 10) continue;
            int screens_x = (width - 4) / 15, screens_y = (height - 4) / 10;
            if (screens_x < 1 || screens_x > 32 || screens_y < 1 || screens_y > 32) continue;
            for (int y = 0; ok && y < screens_y; ++y)
                for (int x = 0; ok && x < screens_x; ++x)
                    ok = points_add(&local, 0, x, y) || feb_oom(error);
        }
        for (size_t cell = 0; ok && cell < local.count; ++cell) {
            int x = anchor->x + local.items[cell][1], y = anchor->y + local.items[cell][2];
            for (size_t other = 0; other < native_count; ++other)
                if (native[other].area == anchor->area && native[other].x == x &&
                    native[other].y == y) {
                    ok = points_add(&family->points, anchor->area, x, y) || feb_oom(error);
                    break;
                }
        }
        free(local.items);
    }
    qsort(native, native_count, sizeof *native, compare_rows);
    FusionJson ambiguities = fusion_json_array();
    int64_t owned = 0, resolutions = 0;
    int *scratch = malloc((anchor_count + 1) * sizeof *scratch);
    size_t *candidates = malloc((family_count + 1) * sizeof *candidates);
    if (ok && (!scratch || !candidates)) ok = feb_oom(error);
    *rows = calloc(native_count + family_count + 1, sizeof **rows);
    if (ok && !*rows) ok = feb_oom(error);
    for (size_t index = 0; ok && index < native_count; ++index) {
        const Row *source = &native[index];
        size_t candidate_count = 0;
        for (size_t family = 0; family < family_count; ++family)
            if (points_has(&families[family].points, source->area, source->x, source->y))
                candidates[candidate_count++] = family;
        long anchored = -1;
        for (size_t family = 0; family < family_count; ++family)
            if (families[family].area == source->area && families[family].x == source->x &&
                families[family].y == source->y)
                anchored = (long)family;
        long owner = -1;
        if (candidate_count == 1) {
            owner = (long)candidates[0];
        } else if (anchored >= 0) {
            owner = anchored;
        } else if (candidate_count > 1) {
            long evidenced = -1;
            size_t matches = 0;
            for (size_t candidate = 0; candidate < candidate_count; ++candidate)
                if (family_evidenced(&families[candidates[candidate]], evidence, evidence_count,
                                     source->area, source->x, source->y)) {
                    evidenced = (long)candidates[candidate];
                    ++matches;
                }
            if (matches == 1) {
                owner = evidenced;
                ++resolutions;
            }
        }
        if (owner < 0 && !candidate_count) {
            long witnessed = -1;
            size_t matches = 0;
            for (size_t family = 0; family < family_count; ++family)
                if (family_evidenced(&families[family], evidence, evidence_count, source->area,
                                     source->x, source->y)) {
                    witnessed = (long)family;
                    ++matches;
                }
            if (matches == 1) {
                owner = witnessed;
                ++resolutions;
            }
        }
        Row *row = &(*rows)[(*row_count)++];
        if (owner >= 0) {
            const Family *family = &families[owner];
            size_t unique = unique_rooms(family->rooms, family->room_count, scratch);
            bool provenance = points_has(&family->points, source->area, source->x, source->y);
            for (size_t room = 0; !provenance && room < unique; ++room) {
                const Points *points = evidence_of(evidence, evidence_count, source->area,
                                                   scratch[room]);
                if (points && points_has(points, source->area, source->x, source->y))
                    provenance = true;
            }
            *row = (Row){source->area, scratch[0], source->x, source->y, family->save,
                         family->warp, provenance ? 1 : 0, source->tile, "-", true};
            if (unique > 1) {
                Buffer note = {0};
                list_text(&note, scratch, unique, ",");
                snprintf(row->note, sizeof row->note, "%s", note.data ? note.data : "");
                free(note.data);
            }
            ++owned;
        } else {
            size_t total = 0;
            for (size_t candidate = 0; candidate < candidate_count; ++candidate) {
                const Family *family = &families[candidates[candidate]];
                memcpy(scratch + total, family->rooms, family->room_count * sizeof *scratch);
                total += family->room_count;
            }
            size_t unique = unique_rooms(scratch, total, scratch);
            *row = (Row){source->area, 999, source->x, source->y, 0, 0, 3, source->tile,
                         "unassigned", true};
            if (unique) {
                Buffer note = {0};
                append(&note, "ambiguous:");
                list_text(&note, scratch, unique, ",");
                snprintf(row->note, sizeof row->note, "%s", note.data ? note.data : "");
                free(note.data);
                FusionJson record = fusion_json_object(), rooms_list = fusion_json_array(),
                           origins = fusion_json_array();
                for (size_t room = 0; ok && room < unique; ++room)
                    ok = fusion_json_push(&rooms_list, fusion_json_int(scratch[room]));
                for (size_t candidate = 0; ok && candidate < candidate_count; ++candidate) {
                    FusionJson pair = fusion_json_array();
                    ok = fusion_json_push(&pair, fusion_json_int(families[candidates[candidate]].x)) &&
                         fusion_json_push(&pair, fusion_json_int(families[candidates[candidate]].y)) &&
                         fusion_json_push(&origins, pair);
                }
                ok = (ok && feb_put_int(&record, "area", source->area) &&
                      feb_put_int(&record, "x", source->x) && feb_put_int(&record, "y", source->y) &&
                      feb_put(&record, "candidate_rooms", rooms_list) &&
                      feb_put(&record, "candidate_origins", origins) &&
                      fusion_json_push(&ambiguities, record)) || feb_oom(error);
            }
        }
    }
    /* Origins absent from the pause minimap stay anchor-only records. */
    Family **ordered = malloc((family_count + 1) * sizeof *ordered);
    if (ok && !ordered) ok = feb_oom(error);
    for (size_t index = 0; ok && index < family_count; ++index) ordered[index] = &families[index];
    if (ok) qsort(ordered, family_count, sizeof *ordered, compare_families);
    for (size_t index = 0; ok && index < family_count; ++index) {
        const Family *family = ordered[index];
        bool native_cell = false;
        for (size_t cell = 0; cell < native_count && !native_cell; ++cell)
            native_cell = native[cell].area == family->area && native[cell].x == family->x &&
                          native[cell].y == family->y;
        if (native_cell) continue;
        size_t unique = unique_rooms(family->rooms, family->room_count, scratch);
        Row *row = &(*rows)[(*row_count)++];
        *row = (Row){family->area, scratch[0], family->x, family->y, family->save, family->warp, 0,
                     0, "-", true};
        if (unique > 1) {
            Buffer note = {0};
            list_text(&note, scratch, unique, ",");
            snprintf(row->note, sizeof row->note, "%s", note.data ? note.data : "");
            free(note.data);
        }
    }
    free(ordered);
    Points all = {0};
    for (size_t index = 0; ok && index < evidence_count; ++index)
        for (size_t point = 0; ok && point < evidence[index].points.count; ++point)
            ok = points_add(&all, evidence[index].points.items[point][0],
                            evidence[index].points.items[point][1],
                            evidence[index].points.items[point][2]) || feb_oom(error);
    FusionJson variants = fusion_json_array();
    for (size_t index = 0; ok && index < family_count; ++index) {
        size_t unique = unique_rooms(families[index].rooms, families[index].room_count, scratch);
        if (unique <= 1) continue;
        FusionJson entry = fusion_json_object(), list = fusion_json_array();
        for (size_t room = 0; ok && room < unique; ++room)
            ok = fusion_json_push(&list, fusion_json_int(scratch[room]));
        ok = (ok && feb_put_int(&entry, "area", families[index].area) &&
              feb_put_int(&entry, "map_x", families[index].x) &&
              feb_put_int(&entry, "map_y", families[index].y) && feb_put(&entry, "rooms", list) &&
              fusion_json_push(&variants, entry)) || feb_oom(error);
    }
    *report = fusion_json_object();
    int64_t ambiguous = (int64_t)ambiguities.count;
    ok = ok && ((feb_put_string(report, "format", "MV_MZM_MINIMAP_OWNERSHIP_1") &&
                 feb_put_int(report, "native_cells", (int64_t)native_count) &&
                 feb_put_int(report, "owned_native_cells", owned) &&
                 feb_put_int(report, "unassigned_native_cells", (int64_t)native_count - owned) &&
                 feb_put_int(report, "ambiguous_native_cells", ambiguous) &&
                 feb_put_int(report, "scroll_bounded_rooms", scroll_bounded) &&
                 feb_put_int(report, "native_evidence_cells", (int64_t)all.count) &&
                 feb_put_int(report, "native_evidence_resolutions", resolutions) &&
                 feb_put(report, "variant_families", variants) &&
                 feb_put(report, "ambiguities", ambiguities) &&
                 feb_put_string(report, "method",
                                "RoomEntryRom origins plus native room scroll bounds, with exact "
                                "Clipdata dimensions as fallback, door/sprite disambiguation, and "
                                "original pause-minimap occupancy")) ||
                feb_oom(error));
    free(all.items);
    free(scratch);
    free(candidates);
    for (size_t index = 0; families && index < family_count; ++index) {
        free(families[index].rooms);
        free(families[index].points.items);
    }
    free(families);
    if (!ok) {
        fusion_json_free(report);
        free(*rows);
        *rows = NULL;
        *row_count = 0;
    }
    return ok;
}

/* output_rows(): the world overview TSV. */
static bool output_rows(const Row *rows, size_t count, Buffer *text) {
    append(text, "# Original map cells; provenance 0=anchor, 1=MZM geometry, 2=Aria, 3=MZM "
                 "minimap tile.\n# area|room|x|y|save|warp|provenance|native_tile|"
                 "variants_or_ambiguity\n");
    for (size_t index = 0; index < count; ++index) {
        const Row *row = &rows[index];
        append(text, "%d|%d|%d|%d|%d|%d|%d|%d%s%s\n", row->area, row->room, row->x, row->y,
               row->save, row->warp, row->provenance, row->tile, row->has_note ? "|" : "",
               row->has_note ? row->note : "");
    }
    return !text->failed;
}

static bool index_mzm(const char *repository, size_t *cell_count, FebError *error) {
    char path[FEB_PATH_CAPACITY], *atlas = NULL;
    long long size = 0;
    snprintf(path, sizeof path, "%s/assets/extracted/rooms/metroid/world_atlas.tsv", repository);
    if (!feb_is_regular(path, &size) && !world_atlas(repository, error)) return false;
    if (!feb_read_text(path, 64 * 1024 * 1024, &atlas, NULL))
        return feb_fail(error, FEB_EXIT_OPERATION, "[Errno 2] No such file or directory: '%s'", path);
    Anchor *anchors = NULL;
    size_t anchor_count = 0;
    bool ok = parse_anchors(atlas, &anchors, &anchor_count, error);
    free(atlas);
    FusionJson rooms = fusion_json_null();
    DoorTable *tables = NULL;
    size_t table_count = 0;
    char *source = NULL;
    ok = ok && feb_native_catalog(repository, "zero_mission", &rooms, error) &&
         (source = read_source(repository, "third_party/mzm/src/data/rooms_data.c", error)) &&
         door_tables(source, &rooms, &tables, &table_count, error);
    free(source);
    Buffer doors = {0};
    ok = ok && (global_doors(tables, table_count, anchors, anchor_count, &doors) || feb_oom(error)) &&
         write_text(repository, "world_overview/mzm_doors.tsv", &doors, error);
    free(doors.data);
    Row *native = NULL, *rows = NULL;
    size_t native_count = 0, row_count = 0;
    ok = ok && minimap_cells(repository, &native, &native_count, error);
    if (ok && native_count) {
        Dimension *dimensions = NULL;
        Scroll *scrolls = NULL;
        Evidence *evidence = NULL;
        size_t dimension_count = 0, scroll_count = 0, evidence_count = 0;
        FusionJson report = fusion_json_null();
        ok = clip_dimensions(repository, &dimensions, &dimension_count, error) &&
             all_scroll_regions(repository, &scrolls, &scroll_count, error) &&
             room_evidence(repository, tables, table_count, anchors, anchor_count, &evidence,
                           &evidence_count, error) &&
             resolve_cells(anchors, anchor_count, dimensions, dimension_count, native, native_count,
                           scrolls, scroll_count, evidence, evidence_count, &rows, &row_count,
                           &report, error);
        if (ok) {
            char *json = NULL;
            Buffer text = {0};
            if (fusion_json_write(&report, 2, &json, NULL)) append(&text, "%s\n", json);
            else ok = feb_oom(error);
            free(json);
            ok = ok && write_text(repository, "world_overview/mzm_ownership.json", &text, error);
            free(text.data);
        }
        fusion_json_free(&report);
        free(dimensions);
        free_scrolls(scrolls, scroll_count);
        for (size_t index = 0; index < evidence_count; ++index) free(evidence[index].points.items);
        free(evidence);
    } else if (ok) {
        /* Without the minimaps, never draw a faux complete world. */
        rows = calloc(anchor_count, sizeof *rows);
        if (!rows) ok = feb_oom(error);
        for (size_t index = 0; ok && index < anchor_count; ++index)
            rows[row_count++] = (Row){anchors[index].area, anchors[index].room, anchors[index].x,
                                      anchors[index].y, anchors[index].save, anchors[index].warp,
                                      0, 0, "", false};
    }
    Buffer text = {0};
    ok = ok && (output_rows(rows, row_count, &text) || feb_oom(error)) &&
         write_text(repository, "world_overview/mzm.tsv", &text, error);
    free(text.data);
    *cell_count = row_count;
    free(rows);
    free(native);
    free_tables(tables, table_count);
    fusion_json_free(&rooms);
    free(anchors);
    return ok;
}

static bool index_aria(const char *repository, size_t *cell_count, FebError *error) {
    char path[FEB_PATH_CAPACITY], detail[256];
    long long size = 0;
    FusionJson catalog = fusion_json_null();
    FusionRom rom = {0};
    bool opened = false;
    snprintf(path, sizeof path, "%s/assets/extracted/rooms/aria/world.json", repository);
    if (feb_is_regular(path, &size)) {
        if (!feb_load_json(path, 256 * 1024 * 1024, &catalog, detail, sizeof detail))
            return feb_fail(error, FEB_EXIT_OPERATION, "%s", detail);
    } else {
        if (!feb_aria_open_world(repository, &rom, &catalog, error)) return false;
        opened = true;
    }
    const FusionJson *format = fusion_json_member(&catalog, "format");
    bool ok = (format && format->kind == FUSION_JSON_STRING &&
               !strcmp(format->string, "MV_AOS_WORLD_2")) ||
              feb_fail(error, FEB_EXIT_OPERATION, "wrong Aria world format");
    Row *rows = NULL;
    size_t count = 0, capacity = 0;
    const FusionJson *rooms = ok ? fusion_json_member(&catalog, "rooms") : NULL;
    if (ok && (!rooms || rooms->kind != FUSION_JSON_ARRAY))
        ok = feb_fail(error, FEB_EXIT_OPERATION, "Aria catalog has no native room map cells");
    for (size_t index = 0; ok && index < rooms->count; ++index) {
        const FusionJson *room = &rooms->items[index];
        const FusionJson *cells = fusion_json_member(room, "map_cells");
        if (!cells || cells->kind != FUSION_JSON_ARRAY) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria room map_cells");
            break;
        }
        for (size_t cell = 0; ok && cell < cells->count; ++cell) {
            const FusionJson *item = &cells->items[cell];
            int area = (int)fusion_json_member(room, "engine_area")->integer;
            int number = (int)fusion_json_member(room, "room")->integer;
            int x = (int)fusion_json_member(item, "map_x")->integer;
            int y = (int)fusion_json_member(item, "map_y")->integer;
            if (area < 0 || area >= 12 || number < 0 || number >= 1000 || x < 0 || x >= 64 ||
                y < 0 || y >= 35) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria original minimap cell");
                break;
            }
            for (size_t other = 0; ok && other < count; ++other)
                if (rows[other].x == x && rows[other].y == y)
                    ok = feb_fail(error, FEB_EXIT_OPERATION,
                                  "duplicate Aria original minimap coordinate");
            if (ok && count == capacity) {
                capacity = capacity ? capacity * 2 : 512;
                Row *grown = realloc(rows, capacity * sizeof *grown);
                if (!grown) ok = feb_oom(error);
                else rows = grown;
            }
            if (ok)
                rows[count++] = (Row){area, number, x, y,
                                      fusion_json_member(item, "save")->boolean ? 1 : 0,
                                      fusion_json_member(item, "warp")->boolean ? 1 : 0, 2, 0, "",
                                      false};
        }
    }
    /* output_rows() appends only the default Aria provenance. */
    Buffer text = {0};
    if (ok && !count) ok = feb_fail(error, FEB_EXIT_OPERATION, "empty Aria map");
    const FusionJson *mapped = ok ? fusion_json_member(&catalog, "mapped_cells") : NULL;
    if (ok && mapped && mapped->kind != FUSION_JSON_NULL &&
        (mapped->kind != FUSION_JSON_NUMBER || mapped->integer != (int64_t)count))
        ok = feb_fail(error, FEB_EXIT_OPERATION,
                      "Aria mapped_cells count differs from verified native cells");
    if (ok) {
        append(&text, "# Original map cells; provenance 0=anchor, 1=MZM geometry, 2=Aria, 3=MZM "
                      "minimap tile.\n# area|room|x|y|save|warp|provenance|native_tile|"
                      "variants_or_ambiguity\n");
        for (size_t index = 0; index < count; ++index)
            append(&text, "%d|%d|%d|%d|%d|%d|2\n", rows[index].area, rows[index].room,
                   rows[index].x, rows[index].y, rows[index].save, rows[index].warp);
        ok = write_text(repository, "world_overview/aria.tsv", &text, error);
    }
    free(text.data);
    free(rows);
    *cell_count = count;
    fusion_json_free(&catalog);
    if (opened) fusion_rom_close(&rom);
    return ok;
}

/* ---- Preview batches --------------------------------------------------- */

typedef struct {
    int values[8];
    int fields;
} TsvRow;

/* The integer fields of the world overview rows (int() of the first 8). */
static bool read_rows(const char *repository, const char *world, TsvRow **rows, size_t *count,
                      FebError *error) {
    char path[FEB_PATH_CAPACITY], *text = NULL;
    snprintf(path, sizeof path, "%s/assets/extracted/world_overview/%s.tsv", repository, world);
    *rows = NULL;
    *count = 0;
    if (!feb_read_text(path, 64 * 1024 * 1024, &text, NULL))
        return feb_fail(error, FEB_EXIT_OPERATION, "[Errno 2] No such file or directory: '%s'", path);
    bool ok = true;
    for (char *line = text, *next; ok && line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        if (!*line || line[0] == '#') continue;
        TsvRow row = {{0}, 0};
        char *part = line;
        for (; part && row.fields < 8; ++row.fields) {
            char *bar = strchr(part, '|');
            if (bar) *bar = '\0';
            char *end = NULL;
            row.values[row.fields] = (int)strtol(part, &end, 10);
            if (end == part || *end) {
                ok = feb_fail(error, FEB_EXIT_OPERATION,
                              "invalid literal for int() with base 10: '%s'", part);
                break;
            }
            part = bar ? bar + 1 : NULL;
        }
        TsvRow *grown = ok ? realloc(*rows, (*count + 1) * sizeof *grown) : NULL;
        if (ok && !grown) ok = feb_oom(error);
        if (ok) {
            *rows = grown;
            (*rows)[(*count)++] = row;
        }
    }
    free(text);
    if (!ok) {
        free(*rows);
        *rows = NULL;
        *count = 0;
    }
    return ok;
}

static void thumbnail_path(const char *repository, bool mzm, int area, int room, char *path,
                           size_t size) {
    if (mzm) {
        char lower[32];
        snprintf(lower, sizeof lower, "%s", areas[area]);
        for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
        snprintf(path, size, "%s/assets/extracted/rooms/metroid/previews/%s_%03d_bg1.bmp",
                 repository, lower, room);
    } else {
        snprintf(path, size, "%s/assets/extracted/rooms/aria/previews/area_%02d_room_%03d_composite.bmp",
                 repository, area, room);
    }
}

/* mzm_screen_preview(): one playable 240x160 screen at 60x40, or false. */
static bool screen_preview(const uint8_t *bmp, size_t size, int local_x, int local_y,
                           uint8_t **out, size_t *out_size, FebError *error) {
    if (local_x < 0 || local_x >= 32 || local_y < 0 || local_y >= 32 || size < 54) return false;
    uint32_t offset = bmp[10] | (bmp[11] << 8) | ((uint32_t)bmp[12] << 16) | ((uint32_t)bmp[13] << 24);
    if (bmp[0] != 'B' || bmp[1] != 'M' || offset != 54) return false;
    int32_t width = (int32_t)(bmp[18] | (bmp[19] << 8) | (bmp[20] << 16) | ((uint32_t)bmp[21] << 24));
    int32_t height = (int32_t)(bmp[22] | (bmp[23] << 8) | (bmp[24] << 16) | ((uint32_t)bmp[25] << 24));
    unsigned planes = bmp[26] | (bmp[27] << 8), bits = bmp[28] | (bmp[29] << 8);
    uint32_t compression = bmp[30] | (bmp[31] << 8) | ((uint32_t)bmp[32] << 16) |
                           ((uint32_t)bmp[33] << 24);
    if (width <= 0 || width > 4096 || height <= 0 || height > 4096 || planes != 1 || bits != 24 ||
        compression || width < 304 || height < 224 || (width - 64) % 240 || (height - 64) % 160)
        return false;
    size_t pitch = ((size_t)width * 3 + 3) & ~(size_t)3;
    if (size != 54 + pitch * (size_t)height) return false;
    if ((local_x + 1) * 240 + 64 > width || (local_y + 1) * 160 + 64 > height) return false;
    uint8_t rgb[60 * 40 * 3];
    for (int py = 0; py < 40; ++py) {
        int source_y = 32 + local_y * 160 + py * 4 + 2;
        for (int px = 0; px < 60; ++px) {
            int source_x = 32 + local_x * 240 + px * 4 + 2;
            const uint8_t *pixel = bmp + 54 + (size_t)(height - 1 - source_y) * pitch +
                                   (size_t)source_x * 3;
            uint8_t *destination = rgb + (py * 60 + px) * 3;
            destination[0] = pixel[2];
            destination[1] = pixel[1];
            destination[2] = pixel[0];
        }
    }
    return feb_bmp24(60, 40, rgb, out, out_size, error);
}

/* generate_mzm_case_previews(): per-screen previews of the owned cells. */
static bool case_previews(const char *repository, int area, const TsvRow *rows, size_t count,
                          FebError *error) {
    FusionJson rooms;
    if (!feb_native_catalog(repository, "zero_mission", &rooms, error)) return false;
    int *done_rooms = malloc((count + 1) * sizeof *done_rooms);
    size_t done_count = 0;
    bool ok = done_rooms != NULL || feb_oom(error);
    for (size_t index = 0; ok && index < count; ++index) {
        const TsvRow *row = &rows[index];
        int room = row->values[1];
        int provenance = row->fields >= 7 ? row->values[6] : 0;
        if (row->values[0] != area || room == 999 || provenance != 1) continue;
        const FusionJson *source = NULL;
        for (size_t entry = 0; entry < rooms.count; ++entry) {
            const FusionJson *candidate = &rooms.items[entry];
            const FusionJson *fields = fusion_json_member(candidate, "fields");
            if (area_index(fusion_json_member(candidate, "area")->string) == area &&
                fusion_json_member(candidate, "room")->integer == room &&
                is_decimal(fusion_json_member(fields, "mapX")->string) &&
                is_decimal(fusion_json_member(fields, "mapY")->string))
                source = candidate;
        }
        if (!source) continue;
        bool seen = false;
        for (size_t other = 0; other < done_count; ++other)
            if (done_rooms[other] == room) seen = true;
        if (seen) continue;
        done_rooms[done_count++] = room;
        char path[FEB_PATH_CAPACITY];
        long long size = 0;
        thumbnail_path(repository, true, area, room, path, sizeof path);
        if (!feb_is_regular(path, &size) || feb_is_symlink(path) || size > 64000000) continue;
        uint8_t *bmp = NULL;
        size_t bmp_size = 0;
        if (!feb_read_text(path, 64000000, (char **)&bmp, &bmp_size)) continue;
        const FusionJson *fields = fusion_json_member(source, "fields");
        int ox = atoi(fusion_json_member(fields, "mapX")->string);
        int oy = atoi(fusion_json_member(fields, "mapY")->string);
        for (size_t other = 0; ok && other < count; ++other) {
            const TsvRow *cell = &rows[other];
            int cell_provenance = cell->fields >= 7 ? cell->values[6] : 0;
            if (cell->values[0] != area || cell->values[1] != room || cell_provenance != 1)
                continue;
            uint8_t *preview = NULL;
            size_t preview_size = 0;
            if (!screen_preview(bmp, bmp_size, cell->values[2] - ox, cell->values[3] - oy,
                                &preview, &preview_size, error))
                continue;
            char relative[160];
            snprintf(relative, sizeof relative,
                     "world_overview/mzm_cells/area_%02d_room_%03d_x_%02d_y_%02d.bmp", area, room,
                     cell->values[2], cell->values[3]);
            ok = feb_write_generated(repository, relative, preview, preview_size, NULL, error);
            free(preview);
        }
        free(bmp);
    }
    free(done_rooms);
    fusion_json_free(&rooms);
    return ok;
}

static bool generate_previews(const char *repository, bool mzm, int area, int budget, int *done,
                              int *failed, FebError *error) {
    const char *world = mzm ? "mzm" : "aria";
    TsvRow *rows = NULL;
    size_t count = 0;
    *done = *failed = 0;
    if (!read_rows(repository, world, &rows, &count, error)) return false;
    int *seen = malloc((count + 1) * sizeof *seen), *failures = malloc((count + 1) * sizeof *failures);
    size_t seen_count = 0, failure_count = 0;
    bool ok = (seen && failures) || feb_oom(error);
    FusionRom rom = {0};
    FusionJson catalog = fusion_json_null();
    bool opened = false;
    for (size_t index = 0; ok && index < count; ++index) {
        int row_area = rows[index].values[0], room = rows[index].values[1];
        bool skip = row_area != area;
        for (size_t other = 0; !skip && other < seen_count; ++other)
            if (seen[other] == room) skip = true;
        for (size_t other = 0; !skip && other < failure_count; ++other)
            if (failures[other] == room) skip = true;
        if (skip) continue;
        seen[seen_count++] = room;
        char path[FEB_PATH_CAPACITY];
        long long size = 0;
        thumbnail_path(repository, mzm, row_area, room, path, sizeof path);
        if (feb_is_regular(path, &size)) continue;
        if (*done + *failed >= budget) break;
        FebError failure = {0};
        FusionJson result = fusion_json_null();
        bool rendered;
        if (mzm) {
            rendered = feb_mzm_decode_room(repository, areas[row_area], room, true, &result,
                                           &failure);
        } else {
            if (!opened && !feb_aria_open_world(repository, &rom, &catalog, &failure)) {
                printf("MISSING PRIVATE INPUT for %s %d:%d: %s\n", world, row_area, room,
                       failure.message);
                fflush(stdout);
                break;
            }
            opened = true;
            rendered = feb_aria_render_loaded(repository, &rom, &catalog, row_area, room, true,
                                              &result, &failure);
        }
        if (rendered) {
            fusion_json_free(&result);
            ++*done;
            continue;
        }
        /* An OSError (a missing source) or missing raw extraction is temporary. */
        if (strstr(failure.message, "missing/unsafe extracted MZM resource:") ||
            !strncmp(failure.message, "[Errno", 6)) {
            printf("MISSING PRIVATE INPUT for %s %d:%d: %s\n", world, row_area, room,
                   failure.message);
            fflush(stdout);
            break;
        }
        ++*failed;
        failures[failure_count++] = room;
        printf("SKIPPED original %s %d:%d: %s\n", world, row_area, room, failure.message);
        fflush(stdout);
    }
    if (opened) {
        fusion_json_free(&catalog);
        fusion_rom_close(&rom);
    }
    ok = ok && (!mzm || case_previews(repository, area, rows, count, error));
    if (ok) {
        qsort(failures, failure_count, sizeof *failures, compare_ints);
        Buffer text = {0};
        append(&text, "[");
        for (size_t index = 0; index < failure_count; ++index)
            append(&text, "%s%d", index ? ", " : "", failures[index]);
        append(&text, "]\n");
        char relative[96];
        snprintf(relative, sizeof relative, "world_overview/%s_%02d_unsupported.json", world, area);
        ok = write_text(repository, relative, &text, error);
        free(text.data);
    }
    free(seen);
    free(failures);
    free(rows);
    return ok;
}

/* The rooms of an area still without a thumbnail, outside the failures. */
static bool remaining_previews(const char *repository, bool mzm, int area, int *remaining) {
    const char *world = mzm ? "mzm" : "aria";
    TsvRow *rows = NULL;
    size_t count = 0;
    FebError ignored = {0};
    *remaining = 0;
    if (!read_rows(repository, world, &rows, &count, &ignored)) return false;
    char path[FEB_PATH_CAPACITY], *text = NULL;
    snprintf(path, sizeof path, "%s/assets/extracted/world_overview/%s_%02d_unsupported.json",
             repository, world, area);
    FusionJson failures = fusion_json_array();
    if (feb_read_text(path, 64 * 1024 * 1024, &text, NULL)) {
        FusionJson parsed;
        char detail[128];
        if (fusion_json_parse(text, strlen(text), &parsed, detail, sizeof detail)) {
            fusion_json_free(&failures);
            failures = parsed;
        }
        free(text);
    }
    int *seen = malloc((count + 1) * sizeof *seen);
    size_t seen_count = 0;
    for (size_t index = 0; seen && index < count; ++index) {
        int room = rows[index].values[1];
        if (rows[index].values[0] != area || room == 999) continue;
        bool duplicate = false;
        for (size_t other = 0; other < seen_count; ++other)
            if (seen[other] == room) duplicate = true;
        if (duplicate) continue;
        seen[seen_count++] = room;
        bool failed = false;
        for (size_t other = 0; failures.kind == FUSION_JSON_ARRAY && other < failures.count; ++other)
            if (failures.items[other].kind == FUSION_JSON_NUMBER &&
                failures.items[other].integer == room)
                failed = true;
        if (failed) continue;
        long long size = 0;
        thumbnail_path(repository, mzm, area, room, path, sizeof path);
        if (!feb_is_regular(path, &size)) ++*remaining;
    }
    free(seen);
    fusion_json_free(&failures);
    free(rows);
    return true;
}

int feb_world_overview_main(int argc, char **argv, const char *repository) {
    const char *world = NULL;
    long area = 0, budget = 6;
    const char *usage = "usage: fusion_world_overview [-h] --world {mzm,aria} [--area AREA] "
                        "[--budget BUDGET]\n";
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index], *value = NULL;
        if (!strcmp(argument, "-h") || !strcmp(argument, "--help")) {
            fputs(usage, stdout);
            puts("\nBuild the verified case-grid world overview and bounded native preview "
                 "batches.");
            return 0;
        }
        const char *names[] = {"--world", "--area", "--budget"};
        int matched = -1;
        for (int name = 0; name < 3; ++name) {
            size_t length = strlen(names[name]);
            if (!strncmp(argument, names[name], length) &&
                (argument[length] == '=' || !argument[length])) {
                matched = name;
                value = argument[length] == '=' ? argument + length + 1
                        : index + 1 < argc ? argv[++index] : NULL;
            }
        }
        char *end = NULL;
        if (matched < 0 || !value) {
            fprintf(stderr, "%sfusion_world_overview: error: unrecognized arguments: %s\n", usage,
                    argument);
            return 2;
        }
        if (matched == 0) {
            if (strcmp(value, "mzm") && strcmp(value, "aria")) {
                fprintf(stderr, "%sfusion_world_overview: error: argument --world: invalid choice: "
                                "'%s' (choose from mzm, aria)\n", usage, value);
                return 2;
            }
            world = value;
        } else {
            long number = strtol(value, &end, 10);
            if (end == value || *end) {
                fprintf(stderr, "%sfusion_world_overview: error: argument %s: invalid int value: "
                                "'%s'\n", usage, names[matched], value);
                return 2;
            }
            if (matched == 1) area = number;
            else budget = number;
        }
    }
    if (!world) {
        fprintf(stderr, "%sfusion_world_overview: error: the following arguments are required: "
                        "--world\n", usage);
        return 2;
    }
    bool mzm = !strcmp(world, "mzm");
    if (budget < 0 || budget > 32) {
        fprintf(stderr, "%sfusion_world_overview: error: preview budget must be 0..32\n", usage);
        return 2;
    }
    if (area < 0 || area >= (mzm ? 7 : 12)) {
        fprintf(stderr, "%sfusion_world_overview: error: invalid area\n", usage);
        return 2;
    }
    FebError error = {0};
    size_t count = 0;
    int done = 0, failed = 0, remaining = 0;
    bool ok = mzm ? index_mzm(repository, &count, &error) : index_aria(repository, &count, &error);
    ok = ok && generate_previews(repository, mzm, (int)area, (int)budget, &done, &failed, &error);
    if (!ok) {
        fprintf(stderr, "%sfusion_world_overview: error: %s\n", usage, error.message);
        return 2;
    }
    remaining_previews(repository, mzm, (int)area, &remaining);
    printf("%s: %zu verified map cells; generated %d previews; %d unsupported; remaining=%d\n",
           world, count, done, failed, remaining);
    fflush(stdout);
    return 0;
}
