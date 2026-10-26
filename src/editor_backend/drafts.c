/* SPDX-License-Identifier: GPL-3.0-only */
#define _DEFAULT_SOURCE
#include "drafts.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
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

bool feb_new_draft(const char *world, int64_t area, const FusionJson *slug,
                   const FusionJson *name, int64_t width, int64_t height, FusionJson *room,
                   FebError *error) {
    FusionJson world_value = fusion_json_string(world), area_value = fusion_json_int(area);
    char id[256];
    bool ok = world_value.string &&
              identity(&world_value, &area_value, slug, id, sizeof id, error);
    fusion_json_free(&world_value);
    if (!ok) return false;
    if (!name_valid(name))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "name must be 1..80 printable characters without separators");
    FusionJson width_value = fusion_json_int(width), height_value = fusion_json_int(height);
    if (!number(&width_value, 1, 8, "width_screens", error) ||
        !number(&height_value, 1, 8, "height_screens", error))
        return false;
    size_t count = 0;
    const char *const *areas = feb_areas(world, &count);
    FusionJson descriptor = fusion_json_object(), geometry = fusion_json_object();
    *room = fusion_json_object();
    ok = feb_put_int(&descriptor, "index", area) && feb_put_string(&descriptor, "name", areas[area]) &&
         feb_put_string(&geometry, "unit", "gba_screen") &&
         feb_put_int(&geometry, "width_screens", width) &&
         feb_put_int(&geometry, "height_screens", height) &&
         feb_put_int(&geometry, "screen_width_px", 240) &&
         feb_put_int(&geometry, "screen_height_px", 160) &&
         feb_put_string(room, "schema", "metroidvania.authored-room") &&
         feb_put_int(room, "version", 1) && feb_put_string(room, "id", id) &&
         feb_put_string(room, "world", world) && feb_put(room, "area", descriptor) &&
         feb_put_string(room, "name", name->string) && feb_put_string(room, "slug", slug->string) &&
         feb_put_string(room, "origin", "project_authored") &&
         feb_put_string(room, "status", "draft_not_playable") &&
         feb_put(room, "geometry", geometry) && feb_put(room, "layers", fusion_json_array()) &&
         feb_put(room, "collision", fusion_json_null()) &&
         feb_put(room, "spawn", fusion_json_null()) && feb_put(room, "doors", fusion_json_array()) &&
         feb_put(room, "transitions", fusion_json_array()) &&
         feb_put(room, "entities", fusion_json_array()) &&
         feb_put(room, "events", fusion_json_array()) &&
         feb_put(room, "music", fusion_json_null()) &&
         feb_put(room, "engine_adapter", fusion_json_null());
    if (!ok) {
        fusion_json_free(room);
        return feb_oom(error);
    }
    if (!feb_validate_draft(room, error)) {
        fusion_json_free(room);
        return false;
    }
    return true;
}

/* _safe_parents(): the parents below the root, created, never symlinks. */
static bool safe_parents(const char *root, const char *path, FebError *error) {
    if (feb_is_symlink(root))
        return feb_fail(error, FEB_EXIT_OPERATION, "project root may not be a symlink");
    char current[FEB_PATH_CAPACITY];
    snprintf(current, sizeof current, "%s", path);
    for (char *slash = current + strlen(root) + 1; (slash = strchr(slash, '/')) != NULL; ++slash) {
        *slash = '\0';
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION, "symlink parent refused: %s", current);
        struct stat info;
        bool exists = stat(current, &info) == 0;
        if (exists && !S_ISDIR(info.st_mode))
            return feb_fail(error, FEB_EXIT_OPERATION, "not a directory: %s", current);
        if (!exists && mkdir(current, 0777) != 0 && errno != EEXIST)
            return feb_fail(error, FEB_EXIT_OPERATION, "[Errno %d] %s: '%s'", errno,
                            strerror(errno), current);
        *slash = '/';
    }
    return true;
}

bool feb_save_draft(const char *root, const FusionJson *room, char *path, FebError *error) {
    if (!feb_validate_draft(room, error)) return false;
    char base[FEB_PATH_CAPACITY];
    if (!private_root(root, base)) return feb_fail(error, FEB_EXIT_OPERATION, "path too long");
    char relative[256];
    snprintf(relative, sizeof relative, "%s/%02lld/%s.json",
             fusion_json_member(room, "world")->string,
             (long long)fusion_json_member(fusion_json_member(room, "area"), "index")->integer,
             fusion_json_member(room, "slug")->string);
    if (!feb_join(path, base, relative)) return feb_fail(error, FEB_EXIT_OPERATION, "path too long");
    if (!safe_parents(root, path, error)) return false;
    char *text = NULL;
    size_t size = 0;
    if (!fusion_json_write(room, 2, &text, &size)) return feb_oom(error);
    /* Exclusive creation also refuses an existing symlink at the destination. */
    int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (descriptor < 0) {
        int code = errno;
        free(text);
        return feb_fail(error, FEB_EXIT_OPERATION, "[Errno %d] %s: '%s'", code, strerror(code),
                        path);
    }
    bool ok = write(descriptor, text, size) == (ssize_t)size && write(descriptor, "\n", 1) == 1;
    ok = close(descriptor) == 0 && ok;
    free(text);
    if (!ok) return feb_fail(error, FEB_EXIT_OPERATION, "cannot write %s", path);
    return true;
}

bool feb_placements_path(const char *root, char *path) {
    char base[FEB_PATH_CAPACITY];
    return private_root(root, base) && feb_join(path, base, "placements.json");
}

typedef struct {
    int64_t area, x, y;
} Cell;

/* _occupancy(): the original minimap cells of a world. */
static bool occupancy(const char *root, const char *world, Cell **cells, size_t *count,
                      FebError *error) {
    char source[FEB_PATH_CAPACITY];
    snprintf(source, sizeof source, "%s/assets/extracted/world_overview/%s", root,
             !strcmp(world, "aria") ? "aria.tsv" : "mzm.tsv");
    long long size = 0;
    char *text = NULL;
    if (feb_is_symlink(source) || !feb_is_regular(source, &size) || size > 2000000 ||
        !feb_read_text(source, 2000000, &text, NULL))
        return feb_fail(error, FEB_EXIT_OPERATION, "generate verified native overview first: %s",
                        source);
    bool aria = !strcmp(world, "aria");
    size_t areas = 0;
    feb_areas(world, &areas);
    int64_t width, height;
    grid(world, &width, &height);
    if (!aria) width = height = 128;
    *cells = NULL;
    *count = 0;
    size_t capacity = 0, rows = 0;
    bool ok = true;
    for (char *line = text; ok && line && *line;) {
        char *end = strchr(line, '\n');
        if (end) *end = '\0';
        if (line[0] && line[0] != '#') {
            char *parts[9];
            size_t fields = 0;
            parts[fields++] = line;
            for (char *c = line; *c; ++c)
                if (*c == '|') {
                    *c = '\0';
                    if (fields < 9) parts[fields] = c + 1;
                    ++fields;
                }
            if (fields < 6 || fields > 8) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid original map index row");
                break;
            }
            int64_t values[6];
            for (size_t index = 0; ok && index < 6; ++index) {
                /* int(): Python accepts surrounding space and a sign. */
                char *tail = NULL;
                const char *text_value = parts[index];
                while (*text_value == ' ' || *text_value == '\t') ++text_value;
                values[index] = strtoll(text_value, &tail, 10);
                while (tail && (*tail == ' ' || *tail == '\t')) ++tail;
                if (!*text_value || !tail || *tail)
                    ok = feb_fail(error, FEB_EXIT_OPERATION,
                                  "invalid original map index coordinate");
            }
            if (!ok) break;
            if (!(values[0] >= 0 && values[0] < (int64_t)areas && values[2] >= 0 &&
                  values[2] < width && values[3] >= 0 && values[3] < height && values[1] >= 0 &&
                  values[1] < 1000 && (values[4] == 0 || values[4] == 1) &&
                  (values[5] == 0 || values[5] == 1))) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "out-of-bounds or corrupt original map index");
                break;
            }
            if (*count == capacity) {
                size_t next = capacity ? capacity * 2 : 256;
                Cell *grown = realloc(*cells, next * sizeof *grown);
                if (!grown) {
                    ok = feb_oom(error);
                    break;
                }
                *cells = grown;
                capacity = next;
            }
            (*cells)[(*count)++] = (Cell){aria ? 0 : values[0], values[2], values[3]};
            if (++rows > 16384) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "oversized original map index");
                break;
            }
        }
        line = end ? end + 1 : NULL;
    }
    free(text);
    if (ok && !*count) ok = feb_fail(error, FEB_EXIT_OPERATION,
                                    "empty original map index; placement refused");
    if (!ok) {
        free(*cells);
        *cells = NULL;
    }
    return ok;
}

bool feb_plan_placement(const char *root, const char *id, int64_t x, int64_t y,
                        FusionJson *placement, FusionJson *updated, FusionJson *rooms,
                        FebError *error) {
    FusionJson entries;
    if (!feb_load_placements(root, &entries, rooms, error)) return false;
    *placement = fusion_json_object();
    bool ok = feb_put_string(placement, "id", id) && feb_put_int(placement, "x", x) &&
              feb_put_int(placement, "y", y);
    if (!ok) {
        fusion_json_free(&entries);
        fusion_json_free(rooms);
        fusion_json_free(placement);
        return feb_oom(error);
    }
    Rect rect;
    Cell *cells = NULL;
    size_t count = 0;
    ok = placement_rect(placement, rooms, &rect, error) &&
         occupancy(root, rect.world, &cells, &count, error);
    bool aria = ok && !strcmp(rect.world, "aria");
    for (int64_t px = rect.x; ok && px < rect.x + rect.width; ++px)
        for (int64_t py = rect.y; ok && py < rect.y + rect.height; ++py)
            for (size_t index = 0; ok && index < count; ++index)
                if (cells[index].area == (aria ? 0 : rect.area) && cells[index].x == px &&
                    cells[index].y == py)
                    ok = feb_fail(error, FEB_EXIT_OPERATION,
                                  "placement overlaps an original minimap tile at (%lld,%lld)",
                                  (long long)px, (long long)py);
    free(cells);
    *updated = fusion_json_array();
    for (size_t index = 0; ok && index < entries.count; ++index) {
        if (!strcmp(fusion_json_member(&entries.items[index], "id")->string, id)) continue;
        ok = fusion_json_push(updated, entries.items[index]) || feb_oom(error);
        entries.items[index] = fusion_json_null();
    }
    FusionJson copy;
    ok = ok && ((fusion_json_clone(placement, &copy) && fusion_json_push(updated, copy)) ||
                feb_oom(error));
    fusion_json_free(&entries);
    if (ok) {
        FusionJson data = fusion_json_object(), list;
        ok = (feb_put_string(&data, "schema", "metroidvania.map-placements") &&
              feb_put_int(&data, "version", 1) && fusion_json_clone(updated, &list) &&
              feb_put(&data, "placements", list)) || feb_oom(error);
        ok = ok && validate_placements(&data, rooms, error);
        fusion_json_free(&data);
    }
    if (!ok) {
        fusion_json_free(placement);
        fusion_json_free(updated);
        fusion_json_free(rooms);
    }
    return ok;
}

static int compare_placements(const void *left, const void *right) {
    return strcmp(fusion_json_member(left, "id")->string, fusion_json_member(right, "id")->string);
}

bool feb_write_placements(const char *root, FusionJson *entries, const FusionJson *rooms,
                          FebError *error) {
    /* sorted() is stable; identities are unique. */
    qsort(entries->items, entries->count, sizeof *entries->items, compare_placements);
    FusionJson data = fusion_json_object(), list;
    bool ok = (feb_put_string(&data, "schema", "metroidvania.map-placements") &&
               feb_put_int(&data, "version", 1) && fusion_json_clone(entries, &list) &&
               feb_put(&data, "placements", list)) || feb_oom(error);
    ok = ok && validate_placements(&data, rooms, error);
    char path[FEB_PATH_CAPACITY];
    ok = ok && (feb_placements_path(root, path) ||
                feb_fail(error, FEB_EXIT_OPERATION, "path too long"));
    long long size = 0;
    bool exists = ok && (feb_is_symlink(path) || feb_is_regular(path, &size) || feb_is_directory(path));
    if (ok && (feb_is_symlink(path) || (exists && !feb_is_regular(path, &size))))
        ok = feb_fail(error, FEB_EXIT_OPERATION, "unsafe placements path");
    if (ok && exists && size > PLACEMENT_BYTES)
        ok = feb_fail(error, FEB_EXIT_OPERATION, "oversized placements document");
    ok = ok && safe_parents(root, path, error);
    char *text = NULL;
    size_t length = 0;
    ok = ok && (fusion_json_write(&data, 2, &text, &length) || feb_oom(error));
    fusion_json_free(&data);
    if (!ok) {
        free(text);
        return false;
    }
    char temporary[FEB_PATH_CAPACITY + 32];
    snprintf(temporary, sizeof temporary, "%s", path);
    *strrchr(temporary, '/') = '\0';
    size_t used = strlen(temporary);
    snprintf(temporary + used, sizeof temporary - used, "/.placements-XXXXXX.tmp");
    int descriptor = mkstemps(temporary, 4);
    ok = descriptor >= 0;
    if (ok) {
        ok = write(descriptor, text, length) == (ssize_t)length && write(descriptor, "\n", 1) == 1 &&
             fsync(descriptor) == 0;
        ok = close(descriptor) == 0 && ok;
        if (ok && feb_is_symlink(path)) {
            unlink(temporary);
            free(text);
            return feb_fail(error, FEB_EXIT_OPERATION, "placements destination became symlink");
        }
        if (!ok || rename(temporary, path) != 0) {
            unlink(temporary);
            ok = false;
        }
    }
    free(text);
    return ok || feb_fail(error, FEB_EXIT_OPERATION, "cannot write %s", path);
}
