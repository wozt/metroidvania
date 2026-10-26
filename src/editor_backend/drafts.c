/* SPDX-License-Identifier: GPL-3.0-only */
#define _DEFAULT_SOURCE
#include "drafts.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DRAFT_LIMIT = 4096,
    DRAFT_BYTES = 16384,
    PLACEMENT_LIMIT = 4096,
    PLACEMENT_BYTES = 500000,
};

static const char *const draft_keys[] = {
    "schema", "version", "id", "world", "area", "name", "slug", "origin", "status",
    "geometry", "layers", "collision", "spawn", "doors", "transitions", "entities",
    "events", "music", "engine_adapter"};
static const char *const empty_collections[] = {
    "layers", "doors", "transitions", "entities", "events"};
static const char *const empty_fields[] = {"collision", "spawn", "music", "engine_adapter"};
static const char *const geometry_keys[] = {
    "unit", "width_screens", "height_screens", "screen_width_px", "screen_height_px"};

/* The object has exactly these member names. */
static bool has_keys(const FusionJson *object, const char *const *keys, size_t count) {
    if (!object || object->kind != FUSION_JSON_OBJECT || object->count != count) return false;
    for (size_t index = 0; index < count; ++index)
        if (!fusion_json_member(object, keys[index])) return false;
    return true;
}

static bool is_int(const FusionJson *value) {
    return value && value->kind == FUSION_JSON_NUMBER && value->integral;
}

static bool number(const FusionJson *value, int64_t lower, int64_t upper, const char *label,
                   FebError *error) {
    if (!is_int(value) || value->integer < lower || value->integer > upper)
        return feb_fail(error, FEB_EXIT_OPERATION, "%s must be an integer in [%lld, %lld]",
                        label, (long long)lower, (long long)upper);
    return true;
}

static bool slug_valid(const char *slug) {
    size_t length = strlen(slug);
    if (length < 1 || length > 40 || slug[0] < 'a' || slug[0] > 'z') return false;
    for (size_t index = 1; index < length; ++index) {
        char c = slug[index];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return true;
}

/* _identity(): "<world>:<area:02d>:<slug>". */
static bool identity(const FusionJson *world, const FusionJson *area,
                     const FusionJson *slug, char *out, size_t size, FebError *error) {
    size_t count = 0;
    if (!world || world->kind != FUSION_JSON_STRING ||
        !feb_areas(world->string, &count))
        return feb_fail(error, FEB_EXIT_OPERATION, "unknown world");
    if (!number(area, 0, (int64_t)count - 1, "area", error)) return false;
    if (!slug || slug->kind != FUSION_JSON_STRING || !slug_valid(slug->string))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "slug must be lowercase ASCII, start with a letter and be <=40 chars");
    snprintf(out, size, "%s:%02lld:%s", world->string, (long long)area->integer,
             slug->string);
    return true;
}

static bool name_valid(const FusionJson *value) {
    if (!value || value->kind != FUSION_JSON_STRING) return false;
    size_t length = feb_utf8_length(value->string);
    uint32_t first = 0, last = 0;
    if (length < 1 || length > 80 || !feb_utf8_edges(value->string, &first, &last) ||
        feb_python_space(first) || feb_python_space(last) || strchr(value->string, '|'))
        return false;
    for (const unsigned char *c = (const unsigned char *)value->string; *c; ++c)
        if (*c < 32 || *c == 127) return false;
    return true;
}

bool feb_validate_draft(const FusionJson *room, FebError *error) {
    if (!has_keys(room, draft_keys, sizeof draft_keys / sizeof draft_keys[0]))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "authored-room v1 has missing or unexpected fields");
    const FusionJson *schema = fusion_json_member(room, "schema");
    const FusionJson *version = fusion_json_member(room, "version");
    if (schema->kind != FUSION_JSON_STRING || strcmp(schema->string, "metroidvania.authored-room") ||
        !is_int(version) || version->integer != 1)
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported authored-room schema/version");
    const FusionJson *world = fusion_json_member(room, "world");
    const FusionJson *area = fusion_json_member(room, "area");
    static const char *const area_keys[] = {"index", "name"};
    if (!has_keys(area, area_keys, 2))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid area descriptor");
    char expected[256];
    if (!identity(world, fusion_json_member(area, "index"), fusion_json_member(room, "slug"),
                  expected, sizeof expected, error))
        return false;
    size_t count = 0;
    const char *const *areas = feb_areas(world->string, &count);
    const FusionJson *area_name = fusion_json_member(area, "name");
    const FusionJson *id = fusion_json_member(room, "id");
    if (area_name->kind != FUSION_JSON_STRING ||
        strcmp(area_name->string, areas[fusion_json_member(area, "index")->integer]) ||
        id->kind != FUSION_JSON_STRING || strcmp(id->string, expected))
        return feb_fail(error, FEB_EXIT_OPERATION, "room id or area name does not match its world");
    if (!name_valid(fusion_json_member(room, "name")))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "name must be 1..80 printable characters without separators");
    const FusionJson *origin = fusion_json_member(room, "origin");
    const FusionJson *status = fusion_json_member(room, "status");
    if (origin->kind != FUSION_JSON_STRING || strcmp(origin->string, "project_authored") ||
        status->kind != FUSION_JSON_STRING || strcmp(status->string, "draft_not_playable"))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "draft provenance/status cannot claim engine readiness");
    const FusionJson *geometry = fusion_json_member(room, "geometry");
    if (!has_keys(geometry, geometry_keys, 5))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid authored room geometry descriptor");
    const FusionJson *unit = fusion_json_member(geometry, "unit");
    const FusionJson *width_px = fusion_json_member(geometry, "screen_width_px");
    const FusionJson *height_px = fusion_json_member(geometry, "screen_height_px");
    if (unit->kind != FUSION_JSON_STRING || strcmp(unit->string, "gba_screen") ||
        !is_int(width_px) || !is_int(height_px) || width_px->integer != 240 ||
        height_px->integer != 160)
        return feb_fail(error, FEB_EXIT_OPERATION, "GBA screen geometry must be 240x160");
    if (!number(fusion_json_member(geometry, "width_screens"), 1, 8, "width_screens", error) ||
        !number(fusion_json_member(geometry, "height_screens"), 1, 8, "height_screens", error))
        return false;
    for (size_t index = 0; index < sizeof empty_collections / sizeof empty_collections[0];
         ++index) {
        const FusionJson *value = fusion_json_member(room, empty_collections[index]);
        if (value->kind != FUSION_JSON_ARRAY || value->count)
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "%s requires a future versioned authoring adapter",
                            empty_collections[index]);
    }
    for (size_t index = 0; index < sizeof empty_fields / sizeof empty_fields[0]; ++index)
        if (fusion_json_member(room, empty_fields[index])->kind != FUSION_JSON_NULL)
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "%s requires a future versioned authoring adapter",
                            empty_fields[index]);
    return true;
}

static bool private_root(const char *root, char *out) {
    return feb_join(out, root, "assets/extracted/authored_rooms");
}

typedef struct {
    char world[NAME_MAX + 1];
    char area[NAME_MAX + 1];
    char file[NAME_MAX + 1];
} Candidate;

static int compare_candidates(const void *left, const void *right) {
    const Candidate *a = left, *b = right;
    int order = strcmp(a->world, b->world);
    if (!order) order = strcmp(a->area, b->area);
    if (!order) order = strcmp(a->file, b->file);
    return order;
}

static bool add_candidate(Candidate **items, size_t *count, size_t *capacity,
                          const Candidate *candidate) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 32;
        Candidate *grown = realloc(*items, next * sizeof *grown);
        if (!grown) return false;
        *items = grown;
        *capacity = next;
    }
    (*items)[(*count)++] = *candidate;
    return true;
}

/* glob("*" "/" "*" "/" "*.json"): every match below the two directory levels. */
static bool glob_drafts(const char *base, Candidate **items, size_t *count) {
    size_t capacity = 0;
    *items = NULL;
    *count = 0;
    DIR *worlds = opendir(base);
    if (!worlds) return true;
    struct dirent *world;
    bool ok = true;
    while (ok && (world = readdir(worlds)) != NULL) {
        if (!strcmp(world->d_name, ".") || !strcmp(world->d_name, "..")) continue;
        char world_path[FEB_PATH_CAPACITY];
        if (!feb_join(world_path, base, world->d_name) || !feb_is_directory(world_path))
            continue;
        DIR *areas = opendir(world_path);
        if (!areas) continue;
        struct dirent *area;
        while (ok && (area = readdir(areas)) != NULL) {
            if (!strcmp(area->d_name, ".") || !strcmp(area->d_name, "..")) continue;
            char area_path[FEB_PATH_CAPACITY];
            if (!feb_join(area_path, world_path, area->d_name) || !feb_is_directory(area_path))
                continue;
            DIR *files = opendir(area_path);
            if (!files) continue;
            struct dirent *file;
            while (ok && (file = readdir(files)) != NULL) {
                size_t length = strlen(file->d_name);
                if (length < 5 || strcmp(file->d_name + length - 5, ".json")) continue;
                Candidate candidate;
                snprintf(candidate.world, sizeof candidate.world, "%s", world->d_name);
                snprintf(candidate.area, sizeof candidate.area, "%s", area->d_name);
                snprintf(candidate.file, sizeof candidate.file, "%s", file->d_name);
                ok = add_candidate(items, count, &capacity, &candidate);
            }
            closedir(files);
        }
        closedir(areas);
    }
    closedir(worlds);
    if (ok) qsort(*items, *count, sizeof **items, compare_candidates);
    return ok;
}

/* load_room(): a bounded draft whose path matches its identity. */
static bool load_draft(const char *path, const char *base, FusionJson *room, FebError *error) {
    long long size = 0;
    if (feb_is_symlink(path) || !feb_is_regular(path, &size) || size > DRAFT_BYTES)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "refuse symlink, missing or oversized authored room");
    char detail[512];
    if (!feb_load_json(path, DRAFT_BYTES, room, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid authored room document: %s", detail);
    if (!feb_validate_draft(room, error)) {
        fusion_json_free(room);
        return false;
    }
    char expected[FEB_PATH_CAPACITY], resolved[PATH_MAX];
    const FusionJson *area = fusion_json_member(room, "area");
    snprintf(expected, sizeof expected, "%s/%s/%02lld/%s.json", base,
             fusion_json_member(room, "world")->string,
             (long long)fusion_json_member(area, "index")->integer,
             fusion_json_member(room, "slug")->string);
    if (!realpath(path, resolved) || strcmp(resolved, expected)) {
        fusion_json_free(room);
        return feb_fail(error, FEB_EXIT_OPERATION, "authored room path and identity disagree");
    }
    return true;
}

bool feb_list_drafts(const char *root, const char *world, FusionJson *rooms,
                     FebError *error) {
    *rooms = fusion_json_array();
    char base[FEB_PATH_CAPACITY];
    if (!private_root(root, base)) return feb_fail(error, FEB_EXIT_OPERATION, "path too long");
    if (feb_is_symlink(base))
        return feb_fail(error, FEB_EXIT_OPERATION, "private room directory is a symlink");
    Candidate *candidates = NULL;
    size_t count = 0;
    if (!glob_drafts(base, &candidates, &count)) return feb_oom(error);
    bool ok = true;
    for (size_t index = 0; ok && index < count; ++index) {
        if (world && strcmp(candidates[index].world, world)) continue;
        char path[FEB_PATH_CAPACITY + 3 * (NAME_MAX + 1)];
        snprintf(path, sizeof path, "%s/%s/%s/%s", base, candidates[index].world,
                 candidates[index].area, candidates[index].file);
        FusionJson room;
        ok = load_draft(path, base, &room, error);
        if (ok && !fusion_json_push(rooms, room)) ok = feb_oom(error);
        if (ok && rooms->count > DRAFT_LIMIT)
            ok = feb_fail(error, FEB_EXIT_OPERATION, "too many authored rooms");
    }
    free(candidates);
    if (!ok) fusion_json_free(rooms);
    return ok;
}

bool feb_draft_by_id(const char *root, const FusionJson *identity_value, FusionJson *room,
                     FebError *error) {
    if (!identity_value || identity_value->kind != FUSION_JSON_STRING ||
        !identity_value->string[0])
        return feb_fail(error, FEB_EXIT_OPERATION, "room draft identity is required");
    FusionJson rooms;
    if (!feb_list_drafts(root, NULL, &rooms, error)) return false;
    bool found = false;
    for (size_t index = 0; index < rooms.count && !found; ++index) {
        const FusionJson *id = fusion_json_member(&rooms.items[index], "id");
        if (!strcmp(id->string, identity_value->string)) {
            found = fusion_json_clone(&rooms.items[index], room);
            if (!found) {
                fusion_json_free(&rooms);
                return feb_oom(error);
            }
        }
    }
    fusion_json_free(&rooms);
    if (!found) return feb_fail(error, FEB_EXIT_OPERATION, "unknown project room draft");
    return true;
}

/* GRID: the original minimap sizes. */
static void grid(const char *world, int64_t *width, int64_t *height) {
    bool aria = !strcmp(world, "aria");
    *width = aria ? 64 : 32;
    *height = aria ? 35 : 32;
}

static const FusionJson *draft_named(const FusionJson *rooms, const char *id) {
    /* A Python dict keeps the last draft of a repeated identity. */
    const FusionJson *found = NULL;
    for (size_t index = 0; index < rooms->count; ++index)
        if (!strcmp(fusion_json_member(&rooms->items[index], "id")->string, id))
            found = &rooms->items[index];
    return found;
}

typedef struct {
    const char *world;
    int64_t area, x, y, width, height;
} Rect;

static bool placement_rect(const FusionJson *entry, const FusionJson *rooms, Rect *rect,
                           FebError *error) {
    static const char *const keys[] = {"id", "x", "y"};
    if (!has_keys(entry, keys, 3))
        return feb_fail(error, FEB_EXIT_OPERATION, "placement has missing/unknown fields");
    const FusionJson *id = fusion_json_member(entry, "id");
    const FusionJson *room = id->kind == FUSION_JSON_STRING ? draft_named(rooms, id->string)
                                                            : NULL;
    if (!room)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "placement must reference an existing private draft");
    const FusionJson *x = fusion_json_member(entry, "x"), *y = fusion_json_member(entry, "y");
    if (!is_int(x) || !is_int(y))
        return feb_fail(error, FEB_EXIT_OPERATION, "placement coordinates must be integers");
    const FusionJson *geometry = fusion_json_member(room, "geometry");
    rect->world = fusion_json_member(room, "world")->string;
    rect->area = fusion_json_member(fusion_json_member(room, "area"), "index")->integer;
    rect->x = x->integer;
    rect->y = y->integer;
    rect->width = fusion_json_member(geometry, "width_screens")->integer;
    rect->height = fusion_json_member(geometry, "height_screens")->integer;
    int64_t xmax, ymax;
    grid(rect->world, &xmax, &ymax);
    if (!(rect->x >= 0 && rect->y >= 0 && rect->x + rect->width <= xmax &&
          rect->y + rect->height <= ymax))
        return feb_fail(error, FEB_EXIT_OPERATION, "room footprint exceeds original minimap bounds");
    return true;
}

static bool rects_overlap(const Rect *a, const Rect *b) {
    bool aria = !strcmp(a->world, "aria");
    if (strcmp(a->world, b->world) || (!aria && a->area != b->area)) return false;
    return a->x < b->x + b->width && b->x < a->x + a->width &&
           a->y < b->y + b->height && b->y < a->y + a->height;
}

/* map_placements.validate(). */
static bool validate_placements(const FusionJson *data, const FusionJson *rooms,
                                FebError *error) {
    static const char *const keys[] = {"schema", "version", "placements"};
    const FusionJson *schema = fusion_json_member(data, "schema");
    const FusionJson *version = fusion_json_member(data, "version");
    const FusionJson *placements = fusion_json_member(data, "placements");
    if (!has_keys(data, keys, 3) || schema->kind != FUSION_JSON_STRING ||
        strcmp(schema->string, "metroidvania.map-placements") || !is_int(version) ||
        version->integer != 1 || placements->kind != FUSION_JSON_ARRAY ||
        placements->count > PLACEMENT_LIMIT)
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported private placement schema");
    Rect *rects = calloc(placements->count ? placements->count : 1, sizeof *rects);
    if (!rects) return feb_oom(error);
    for (size_t index = 0; index < placements->count; ++index) {
        const FusionJson *entry = &placements->items[index];
        if (!placement_rect(entry, rooms, &rects[index], error)) {
            free(rects);
            return false;
        }
        const char *id = fusion_json_member(entry, "id")->string;
        for (size_t other = 0; other < index; ++other) {
            if (!strcmp(fusion_json_member(&placements->items[other], "id")->string, id) ||
                rects_overlap(&rects[index], &rects[other])) {
                free(rects);
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "duplicate draft or overlapping private footprints");
            }
        }
    }
    free(rects);
    return true;
}

bool feb_load_placements(const char *root, FusionJson *placements, FusionJson *rooms,
                         FebError *error) {
    *placements = fusion_json_array();
    if (!feb_list_drafts(root, NULL, rooms, error)) return false;
    char base[FEB_PATH_CAPACITY], path[FEB_PATH_CAPACITY];
    if (!private_root(root, base) || !feb_join(path, base, "placements.json")) {
        fusion_json_free(rooms);
        return feb_fail(error, FEB_EXIT_OPERATION, "path too long");
    }
    long long size = 0;
    bool exists = feb_is_symlink(path) || feb_is_regular(path, &size) ||
                  feb_is_directory(path);
    if (feb_is_symlink(path) || (exists && !feb_is_regular(path, &size))) {
        fusion_json_free(rooms);
        return feb_fail(error, FEB_EXIT_OPERATION, "unsafe placements path");
    }
    if (exists && size > PLACEMENT_BYTES) {
        fusion_json_free(rooms);
        return feb_fail(error, FEB_EXIT_OPERATION, "oversized placements document");
    }
    FusionJson data = {0};
    if (exists) {
        char detail[512];
        if (!feb_load_json(path, PLACEMENT_BYTES, &data, detail, sizeof detail)) {
            fusion_json_free(rooms);
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "invalid private placements document: %s", detail);
        }
    } else {
        data = fusion_json_object();
        if (!feb_put_string(&data, "schema", "metroidvania.map-placements") ||
            !feb_put_int(&data, "version", 1) ||
            !feb_put(&data, "placements", fusion_json_array())) {
            fusion_json_free(&data);
            fusion_json_free(rooms);
            return feb_oom(error);
        }
    }
    if (!validate_placements(&data, rooms, error)) {
        fusion_json_free(&data);
        fusion_json_free(rooms);
        return false;
    }
    FusionJson *list = fusion_json_member_mut(&data, "placements");
    *placements = *list;
    *list = fusion_json_null();
    fusion_json_free(&data);
    return true;
}
