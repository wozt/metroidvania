/* SPDX-License-Identifier: GPL-3.0-only */
/* The original room catalogs the editor lists: the Zero Mission RoomEntryRom
 * declarations of the pinned decompilation (formerly
 * scripts/import_mzm_rooms.py::decode_room_descriptors) and the Aria rooms of
 * the native world catalog (rooms/aria/world.json). */
#include "catalog.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char room_source[] = "third_party/mzm/src/data/rooms_data.c";
static const char *const required_fields[] = {
    "tileset", "pBg1Data", "pBg2Data", "pClipData", "pDefaultSpriteData",
    "defaultSpriteset", "mapX", "mapY", "musicTrack"};

static int strcasecmp_ascii(const char *left, const char *right) {
    for (;; ++left, ++right) {
        int a = tolower((unsigned char)*left), b = tolower((unsigned char)*right);
        if (a != b || !a) return a - b;
    }
}

static bool is_word(char c) {
    return isalnum((unsigned char)c) != 0;
}

static bool is_token(const char *value) {
    if (!*value) return false;
    for (; *value; ++value)
        if (!is_word(*value) && *value != '_') return false;
    return true;
}

static const char *skip_space(const char *cursor, const char *end) {
    while (cursor < end && isspace((unsigned char)*cursor)) ++cursor;
    return cursor;
}

static char *copy_range(const char *start, const char *end) {
    while (start < end && isspace((unsigned char)*start)) ++start;
    while (end > start && isspace((unsigned char)end[-1])) --end;
    size_t length = (size_t)(end - start);
    char *text = malloc(length + 1);
    if (text) {
        memcpy(text, start, length);
        text[length] = '\0';
    }
    return text;
}

/* FIELD_RE over one entry: ".name = value" up to a comma, newline or '}'. */
static bool parse_fields(const char *body, const char *end, FusionJson *fields) {
    for (const char *cursor = body; cursor < end; ++cursor) {
        if (*cursor != '.') continue;
        const char *name = cursor + 1, *name_end = name;
        while (name_end < end && is_word(*name_end)) ++name_end;
        if (name_end == name) continue;
        const char *equals = skip_space(name_end, end);
        if (equals >= end || *equals != '=') continue;
        const char *value = skip_space(equals + 1, end);
        const char *value_end = value;
        while (value_end < end && *value_end != ',' && *value_end != '\n' && *value_end != '}')
            ++value_end;
        if (value_end == value) continue;
        char key[128];
        snprintf(key, sizeof key, "%.*s", (int)(name_end - name), name);
        char *text = copy_range(value, value_end);
        if (!text || !fusion_json_set(fields, key, fusion_json_string(text))) {
            free(text);
            return false;
        }
        free(text);
        cursor = value_end - 1;
    }
    return true;
}

static bool add_room(FusionJson *rooms, const char *area, long index, FusionJson fields) {
    char id[128], lower[64];
    snprintf(lower, sizeof lower, "%s", area);
    for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
    snprintf(id, sizeof id, "mzm:%s:%03ld", lower, index);
    FusionJson room = fusion_json_object();
    bool ok = feb_put_string(&room, "world", "zero_mission") &&
              feb_put_string(&room, "area", area) && feb_put(&room, "area_index", fusion_json_null()) &&
              feb_put_int(&room, "room", index) && feb_put_string(&room, "native_id", id) &&
              feb_put_string(&room, "native_reference", room_source) &&
              feb_put(&room, "fields", fields);
    if (!ok) {
        fusion_json_free(&room);
        return false;
    }
    return fusion_json_push(rooms, room);
}

/* One area's entries: "[index] = { ... }" with a brace-free body. */
static bool parse_area(const char *area, long expected, const char *section,
                       const char *end, FusionJson *rooms, FebError *error) {
    long found = 0;
    for (const char *cursor = section; cursor < end; ++cursor) {
        if (*cursor != '[') continue;
        const char *digits = cursor + 1, *digits_end = digits;
        while (digits_end < end && isdigit((unsigned char)*digits_end)) ++digits_end;
        if (digits_end == digits || digits_end >= end || *digits_end != ']') continue;
        const char *equals = skip_space(digits_end + 1, end);
        if (equals >= end || *equals != '=') continue;
        const char *brace = skip_space(equals + 1, end);
        if (brace >= end || *brace != '{') continue;
        const char *close = brace + 1;
        while (close < end && *close != '{' && *close != '}') ++close;
        if (close >= end || *close != '}') continue;
        long index = strtol(digits, NULL, 10);
        if (index != found)
            return feb_fail(error, FEB_EXIT_OPERATION, "nonconsecutive %s room index %ld", area,
                            index);
        FusionJson fields = fusion_json_object();
        if (!parse_fields(brace + 1, close, &fields)) {
            fusion_json_free(&fields);
            return feb_oom(error);
        }
        for (size_t field = 0; field < sizeof required_fields / sizeof required_fields[0];
             ++field) {
            if (!fusion_json_member(&fields, required_fields[field])) {
                fusion_json_free(&fields);
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "%s %ld: required decomp fields absent", area, index);
            }
        }
        for (size_t field = 0; field < fields.count; ++field) {
            if (!is_token(fields.items[field].string)) {
                fusion_json_free(&fields);
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "%s %ld: unsupported field expression", area, index);
            }
        }
        if (!add_room(rooms, area, index, fields)) return feb_oom(error);
        ++found;
        cursor = close;
    }
    if (found != expected)
        return feb_fail(error, FEB_EXIT_OPERATION, "incomplete %s room table: %ld != %ld", area,
                        found, expected);
    return true;
}

static bool zero_mission_catalog(const char *repository, FusionJson *rooms, FebError *error) {
    char path[FEB_PATH_CAPACITY];
    char *text = NULL;
    size_t size = 0;
    if (!feb_join(path, repository, room_source) ||
        !feb_read_text(path, 16 * 1024 * 1024, &text, &size))
        return feb_fail(error, FEB_EXIT_OPERATION, "[Errno 2] No such file or directory: '%s'",
                        path);
    static const char marker[] = "const struct RoomEntryRom s";
    char areas[16][64];
    size_t area_count = 0;
    const char *end = text + size;
    bool ok = true;
    for (const char *cursor = text; ok && (cursor = strstr(cursor, marker)) != NULL;) {
        const char *name = cursor + sizeof marker - 1, *run = name;
        cursor += 1;
        while (run < end && is_word(*run)) ++run;
        size_t length = (size_t)(run - name);
        if (length <= strlen("RoomEntries") ||
            strncmp(run - strlen("RoomEntries"), "RoomEntries", strlen("RoomEntries")) ||
            run >= end || *run != '[')
            continue;
        const char *digits = run + 1, *digits_end = digits;
        while (digits_end < end && isdigit((unsigned char)*digits_end)) ++digits_end;
        if (digits_end == digits || digits_end >= end || *digits_end != ']') continue;
        const char *equals = skip_space(digits_end + 1, end);
        if (equals >= end || *equals != '=') continue;
        const char *brace = skip_space(equals + 1, end);
        if (brace >= end || *brace != '{') continue;
        const char *close = strstr(brace + 1, "\n};");
        if (!close) continue;
        char area[64];
        snprintf(area, sizeof area, "%.*s", (int)(length - strlen("RoomEntries")), name);
        for (size_t index = 0; index < area_count; ++index)
            if (!strcmp(areas[index], area))
                ok = feb_fail(error, FEB_EXIT_OPERATION, "duplicate room area: %s", area);
        if (ok && area_count < 16) snprintf(areas[area_count++], sizeof areas[0], "%s", area);
        if (ok)
            ok = parse_area(area, strtol(digits, NULL, 10), brace + 1, close, rooms, error);
        cursor = close + 3;
    }
    free(text);
    if (ok && !area_count)
        ok = feb_fail(error, FEB_EXIT_OPERATION,
                      "no original Zero Mission RoomEntryRom tables found");
    return ok;
}

static bool aria_catalog(const char *repository, FusionJson *rooms, FebError *error) {
    char path[FEB_PATH_CAPACITY], detail[512];
    FusionJson world = {0};
    if (!feb_join(path, repository, "assets/extracted/rooms/aria/world.json") ||
        !feb_load_json(path, 64 * 1024 * 1024, &world, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "missing native Aria world catalog %s; run ./build/fusion_rebuild "
                        "--task aria_world", path);
    const FusionJson *list = fusion_json_member(&world, "rooms");
    bool ok = list && list->kind == FUSION_JSON_ARRAY;
    for (size_t index = 0; ok && index < list->count; ++index) {
        const FusionJson *source = &list->items[index];
        const FusionJson *area = fusion_json_member(source, "area");
        const FusionJson *engine = fusion_json_member(source, "engine_area");
        const FusionJson *number = fusion_json_member(source, "room");
        const FusionJson *pointer = fusion_json_member(source, "source_pointer");
        const FusionJson *backgrounds = fusion_json_member(source, "backgrounds");
        const FusionJson *entities = fusion_json_member(source, "entities");
        const FusionJson *transitions = fusion_json_member(source, "transitions");
        if (!area || !engine || !number || !pointer || !backgrounds || !entities ||
            !transitions) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid native Aria world catalog");
            break;
        }
        char id[64];
        snprintf(id, sizeof id, "aria:%02lld:%03lld", (long long)engine->integer,
                 (long long)number->integer);
        FusionJson room = fusion_json_object(), copy;
        ok = feb_put_string(&room, "world", "aria") &&
             fusion_json_clone(area, &copy) && feb_put(&room, "area", copy) &&
             feb_put_int(&room, "area_index", engine->integer) &&
             feb_put_int(&room, "room", number->integer) && feb_put_string(&room, "native_id", id) &&
             fusion_json_clone(pointer, &copy) && feb_put(&room, "native_reference", copy) &&
             fusion_json_clone(backgrounds, &copy) && feb_put(&room, "backgrounds", copy) &&
             feb_put_int(&room, "entity_count", (int64_t)entities->count) &&
             feb_put_int(&room, "transition_count", (int64_t)transitions->count) &&
             fusion_json_push(rooms, room);
        if (!ok) feb_oom(error);
    }
    if (ok && !list) ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid native Aria world catalog");
    fusion_json_free(&world);
    return ok;
}

bool feb_native_catalog(const char *repository, const char *world, FusionJson *rooms,
                        FebError *error) {
    *rooms = fusion_json_array();
    bool ok = !strcmp(world, "zero_mission") ? zero_mission_catalog(repository, rooms, error)
                                             : aria_catalog(repository, rooms, error);
    if (!ok) fusion_json_free(rooms);
    return ok;
}

bool feb_filter_native_rooms(const char *repository, const char *world,
                             const FusionJson *area, FusionJson *rooms, FebError *error) {
    if (!feb_native_catalog(repository, world, rooms, error)) return false;
    if (!area) return true;
    size_t count = 0;
    const char *const *names = feb_areas(world, &count);
    int64_t index = -1;
    const char *name = NULL;
    bool aria = !strcmp(world, "aria");
    bool decimal = area->kind == FUSION_JSON_STRING && area->string[0];
    for (const char *c = area->kind == FUSION_JSON_STRING ? area->string : ""; *c; ++c)
        if (!isdigit((unsigned char)*c)) decimal = false;
    if (area->kind == FUSION_JSON_NUMBER && area->integral && area->integer >= 0) decimal = true;
    if (aria || decimal) {
        if (!feb_integer(area, "area", 0, (int64_t)count - 1, &index, error)) {
            fusion_json_free(rooms);
            return false;
        }
        name = names[index];
    } else {
        for (size_t candidate = 0; candidate < count; ++candidate) {
            if (area->kind == FUSION_JSON_STRING &&
                !strcasecmp_ascii(names[candidate], area->string))
                name = names[candidate];
        }
        if (!name) {
            fusion_json_free(rooms);
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown Zero Mission area");
        }
    }
    FusionJson filtered = fusion_json_array();
    for (size_t room = 0; room < rooms->count; ++room) {
        FusionJson *item = &rooms->items[room];
        bool keep = aria ? fusion_json_member(item, "area_index")->integer == index
                         : !strcmp(fusion_json_member(item, "area")->string, name);
        if (!keep) continue;
        if (!fusion_json_push(&filtered, *item)) {
            fusion_json_free(&filtered);
            fusion_json_free(rooms);
            return feb_oom(error);
        }
        *item = fusion_json_null();
    }
    fusion_json_free(rooms);
    *rooms = filtered;
    return true;
}
