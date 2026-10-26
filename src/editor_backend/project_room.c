/* SPDX-License-Identifier: GPL-3.0-only */
#define _DEFAULT_SOURCE
#include "project_room.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

static const char schema_name[] = "metroidvania.project-room-data";
static const char legacy_schema[] = "metroidvania.project-room-entities";
enum { VERSION = 4, ROOM_BYTES = 4000000, SAVED_ROOM_LIMIT = 2048 };

const char *const feb_mzm_areas[7] = {
    "Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia"};
const char *const feb_collision_types[7] = {
    "solid", "one_way", "hazard", "slope_up", "slope_down", "water", "air"};
const char *const feb_condition_types[5] = {
    "story_flag", "event_complete", "entity_present", "transition_ready",
    "checkpoint_active"};

static const char *const kinds[] = {"ENEMY", "ITEM", "OBJECT"};
static const char *const door_types[] = {"normal", "boss", "locked", "portal", "save"};
static const char *const door_facings[] = {"left", "right", "up", "down"};
static const char *const event_kinds[] = {"EVENT", "TRIGGER"};
static const char *const trigger_types[] = {"room_load", "enter", "leave", "touch", "interact"};
static const char *const action_types[] = {"story", "spawn", "toggle", "checkpoint", "transition"};
static const char *const condition_modes[] = {"all", "any"};

static const char *const doc_keys[] = {
    "schema", "version", "world", "area", "room", "width_px", "height_px", "next_id",
    "entities", "collision", "next_door_id", "doors", "next_transition_id", "transitions",
    "next_event_id", "events"};
static const char *const entity_keys[] = {"id", "kind", "x", "y", "label", "native_type"};
static const char *const entity_settings_keys[] = {
    "id", "kind", "x", "y", "label", "native_type", "settings"};
static const char *const item_keys[] = {"item_id", "parameter_0", "parameter_1", "flags"};
static const char *const collision_keys[] = {"resolution_px", "cells"};
static const char *const cell_keys[] = {"x", "y", "type"};
static const char *const door_keys[] = {
    "id", "x", "y", "width", "height", "label", "door_type", "facing"};
static const char *const door_source_keys[] = {
    "id", "x", "y", "width", "height", "label", "door_type", "facing", "native_source"};
static const char *const native_source_keys[] = {"index", "variant", "native_type"};
static const char *const transition_keys[] = {
    "id", "source_door_id", "target_world", "target_area", "target_room", "target_door_id",
    "spawn_x", "spawn_y"};
static const char *const event_v3_keys[] = {
    "id", "kind", "x", "y", "width", "height", "label", "trigger_type", "action_type",
    "action_ref", "once"};
static const char *const event_keys[] = {
    "id", "kind", "x", "y", "width", "height", "label", "trigger_type", "action_type",
    "action_ref", "once", "condition_mode", "conditions"};
static const char *const condition_keys[] = {"type", "ref", "negated"};

/* ARIA_PICKUP_LIMITS: the highest item id of each documented subtype. */
static const int aria_pickup_limits[9] = {0, 255, 31, 58, 44, 55, 24, 35, 5};

static bool has_keys(const FusionJson *object, const char *const *keys, size_t count) {
    if (!object || object->kind != FUSION_JSON_OBJECT || object->count != count) return false;
    for (size_t index = 0; index < count; ++index)
        if (!fusion_json_member(object, keys[index])) return false;
    return true;
}

static bool is_string(const FusionJson *value, const char *text) {
    return value && value->kind == FUSION_JSON_STRING && !strcmp(value->string, text);
}

static bool one_of(const FusionJson *value, const char *const *list, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (is_string(value, list[index])) return true;
    return false;
}

static bool is_int(const FusionJson *value) {
    return value && value->kind == FUSION_JSON_NUMBER && value->integral;
}

/* _int(): an integer (never a boolean) in range. */
static bool check_int(const FusionJson *value, int64_t low, int64_t high, const char *label,
                      int64_t *result, FebError *error) {
    if (!is_int(value) || value->integer < low || value->integer > high)
        return feb_fail(error, FEB_EXIT_OPERATION, "%s must be an integer in [%lld, %lld]", label,
                        (long long)low, (long long)high);
    if (result) *result = value->integer;
    return true;
}

static int64_t integer(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    return value ? value->integer : 0;
}

/* _text(): 1..limit printable characters without outer space or separators. */
static bool text_valid(const FusionJson *value, size_t limit) {
    if (!value || value->kind != FUSION_JSON_STRING) return false;
    size_t length = feb_utf8_length(value->string);
    uint32_t first = 0, last = 0;
    if (length < 1 || length > limit || !feb_utf8_edges(value->string, &first, &last) ||
        feb_python_space(first) || feb_python_space(last) || strpbrk(value->string, "|\t"))
        return false;
    for (const unsigned char *c = (const unsigned char *)value->string; *c; ++c)
        if (*c < 32 || *c == 127) return false;
    return true;
}

static bool token_valid(const FusionJson *value, size_t limit, const char *extra) {
    if (!value || value->kind != FUSION_JSON_STRING) return false;
    size_t length = strlen(value->string);
    if (length < 1 || length > limit) return false;
    for (const char *c = value->string; *c; ++c)
        if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
              strchr(extra, *c)))
            return false;
    return true;
}

/* NATIVE_TYPE and ACTION_REFERENCE: [A-Za-z0-9_.:-]. */
static bool native_type_valid(const FusionJson *value) {
    return token_valid(value, 64, "_.:-");
}

static bool reference_valid(const FusionJson *value) {
    return token_valid(value, 80, "_.:-");
}

static bool aria_area_valid(const char *area) {
    size_t length = strlen(area);
    if (!length || length > 2) return false;
    for (const char *c = area; *c; ++c)
        if (*c < '0' || *c > '9') return false;
    if (length == 2 && area[0] == '0') return false;
    return atoi(area) <= 11;
}

bool feb_room_scope(const FusionJson *world, const FusionJson *area, int64_t room,
                    int64_t width, int64_t height, FebError *error) {
    if (!is_string(world, "mzm") && !is_string(world, "aria"))
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported game");
    if (is_string(world, "mzm") && !one_of(area, feb_mzm_areas, 7))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Zero Mission area");
    if (is_string(world, "aria") &&
        (!area || area->kind != FUSION_JSON_STRING || !aria_area_valid(area->string)))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria area");
    FusionJson value = fusion_json_int(room);
    if (!check_int(&value, 0, 999, "room", NULL, error)) return false;
    value = fusion_json_int(width);
    if (!check_int(&value, 16, 16384, "width_px", NULL, error)) return false;
    value = fusion_json_int(height);
    if (!check_int(&value, 16, 16384, "height_px", NULL, error)) return false;
    if (width % 16 || height % 16 || width * height > 6144 * 256)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid room cell geometry");
    return true;
}

static bool scope_check(const FebRoomScope *scope, FebError *error) {
    FusionJson world = {.kind = FUSION_JSON_STRING, .string = (char *)scope->world};
    FusionJson area = {.kind = FUSION_JSON_STRING, .string = (char *)scope->area};
    return feb_room_scope(&world, &area, scope->room, scope->width, scope->height, error);
}

bool feb_room_path(const char *root, const FebRoomScope *scope, char *path, FebError *error) {
    if (!scope_check(scope, error)) return false;
    char file[96];
    if (!strcmp(scope->world, "mzm")) {
        char lower[32];
        snprintf(lower, sizeof lower, "%s", scope->area);
        for (char *c = lower; *c; ++c)
            if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
        snprintf(file, sizeof file, "%s_%03lld.json", lower, (long long)scope->room);
    } else {
        snprintf(file, sizeof file, "area_%02d_room_%03lld.json", atoi(scope->area),
                 (long long)scope->room);
    }
    int written = snprintf(path, FEB_PATH_CAPACITY, "%s/assets/extracted/overrides/%s/entities/%s",
                           root, !strcmp(scope->world, "mzm") ? "metroid" : "aria", file);
    if (written < 0 || written >= FEB_PATH_CAPACITY)
        return feb_fail(error, FEB_EXIT_OPERATION, "project room path is too long");
    return true;
}

bool feb_room_check_path(const char *root, const char *path, bool create, FebError *error) {
    if (feb_is_symlink(root))
        return feb_fail(error, FEB_EXIT_OPERATION, "project root symlink refused");
    size_t root_length = strlen(root);
    char current[FEB_PATH_CAPACITY];
    snprintf(current, sizeof current, "%s", path);
    for (char *slash = current + root_length + 1; (slash = strchr(slash, '/')) != NULL; ++slash) {
        *slash = '\0';
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION, "project entity parent symlink refused");
        struct stat info;
        bool exists = stat(current, &info) == 0;
        if (exists && !S_ISDIR(info.st_mode))
            return feb_fail(error, FEB_EXIT_OPERATION, "entity parent is not a directory");
        if (create && !exists && mkdir(current, 0777) != 0)
            return feb_fail(error, FEB_EXIT_OPERATION, "cannot create %s", current);
        *slash = '/';
    }
    if (feb_is_symlink(path))
        return feb_fail(error, FEB_EXIT_OPERATION, "project entity symlink refused");
    return true;
}

static bool put_list(FusionJson *object, const char *key) {
    return fusion_json_set(object, key, fusion_json_array());
}

static bool new_document(const FebRoomScope *scope, FusionJson *document) {
    *document = fusion_json_object();
    FusionJson collision = fusion_json_object();
    bool ok = feb_put_int(&collision, "resolution_px", !strcmp(scope->world, "mzm") ? 16 : 8) &&
              put_list(&collision, "cells");
    ok = ok && feb_put_string(document, "schema", schema_name) &&
         feb_put_int(document, "version", VERSION) &&
         feb_put_string(document, "world", scope->world) &&
         feb_put_string(document, "area", scope->area) &&
         feb_put_int(document, "room", scope->room) &&
         feb_put_int(document, "width_px", scope->width) &&
         feb_put_int(document, "height_px", scope->height) &&
         feb_put_int(document, "next_id", 1) && put_list(document, "entities") &&
         feb_put(document, "collision", collision) && feb_put_int(document, "next_door_id", 1) &&
         put_list(document, "doors") && feb_put_int(document, "next_transition_id", 1) &&
         put_list(document, "transitions") && feb_put_int(document, "next_event_id", 1) &&
         put_list(document, "events");
    if (!ok) fusion_json_free(document);
    return ok;
}

bool feb_room_migrate(FusionJson *document, FebError *error) {
    if (document->kind != FUSION_JSON_OBJECT) return true;
    const FusionJson *schema = fusion_json_member(document, "schema");
    const FusionJson *version = fusion_json_member(document, "version");
    bool numbered = version && version->kind == FUSION_JSON_NUMBER;
    if (is_string(schema, schema_name) && numbered && version->number == 3) {
        if (!has_keys(document, doc_keys, COUNT(doc_keys)))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "invalid version-3 project room document fields");
        FusionJson *events = fusion_json_member_mut(document, "events");
        bool valid = events->kind == FUSION_JSON_ARRAY;
        for (size_t index = 0; valid && index < events->count; ++index)
            valid = has_keys(&events->items[index], event_v3_keys, COUNT(event_v3_keys));
        if (!valid)
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid version-3 project event fields");
        for (size_t index = 0; index < events->count; ++index)
            if (!feb_put_string(&events->items[index], "condition_mode", "all") ||
                !put_list(&events->items[index], "conditions"))
                return feb_oom(error);
        return feb_put_int(document, "version", VERSION) || feb_oom(error);
    }
    if (is_string(schema, schema_name) && numbered && version->number == 2) {
        if (!has_keys(document, doc_keys, COUNT(doc_keys) - 2) ||
            fusion_json_member(document, "events") || fusion_json_member(document, "next_event_id"))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "invalid version-2 project room document fields");
        return (feb_put_int(document, "version", VERSION) &&
                feb_put_int(document, "next_event_id", 1) && put_list(document, "events")) ||
               feb_oom(error);
    }
    if (!is_string(schema, legacy_schema) || !numbered || version->number != 1) return true;
    static const char *const legacy_keys[] = {
        "schema", "version", "world", "area", "room", "width_px", "height_px", "next_id",
        "entities"};
    if (!has_keys(document, legacy_keys, COUNT(legacy_keys)))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid legacy project room document fields");
    FusionJson collision = fusion_json_object();
    bool ok = feb_put_int(&collision, "resolution_px",
                          is_string(fusion_json_member(document, "world"), "mzm") ? 16 : 8) &&
              put_list(&collision, "cells") &&
              feb_put_string(document, "schema", schema_name) &&
              feb_put_int(document, "version", VERSION) &&
              feb_put(document, "collision", collision) &&
              feb_put_int(document, "next_door_id", 1) && put_list(document, "doors") &&
              feb_put_int(document, "next_transition_id", 1) &&
              put_list(document, "transitions") && feb_put_int(document, "next_event_id", 1) &&
              put_list(document, "events");
    return ok || feb_oom(error);
}

/* item_settings(): a documented Aria pickup subtype and its record fields. */
static bool item_settings_valid(const FusionJson *native_type, const FusionJson *settings,
                                FebError *error) {
    const char *text = native_type && native_type->kind == FUSION_JSON_STRING
        ? native_type->string : "";
    const char *colon = strrchr(text, ':');
    bool family = colon && ((!strncmp(text, "pickup:", 7) && colon == text + 6) ||
                            (!strncmp(text, "hard-mode-pickup:", 17) && colon == text + 16) ||
                            (!strncmp(text, "all-souls-reward:", 17) && colon == text + 16));
    bool hex = family && strlen(colon + 1) == 2;
    for (const char *c = hex ? colon + 1 : ""; *c; ++c)
        if (!((*c >= '0' && *c <= '9') || (*c >= 'a' && *c <= 'f') || (*c >= 'A' && *c <= 'F')))
            hex = false;
    if (!hex)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "choose a valid Aria pickup native type before setting item fields");
    long subtype = strtol(colon + 1, NULL, 16);
    if (subtype < 0 || subtype >= (long)COUNT(aria_pickup_limits))
        return feb_fail(error, FEB_EXIT_OPERATION, "undocumented Aria pickup subtype");
    return check_int(fusion_json_member(settings, "item_id"), 0, aria_pickup_limits[subtype],
                     "item_id", NULL, error) &&
           check_int(fusion_json_member(settings, "parameter_0"), 0, 65535, "parameter_0", NULL,
                     error) &&
           check_int(fusion_json_member(settings, "parameter_1"), 0, 65535, "parameter_1", NULL,
                     error) &&
           check_int(fusion_json_member(settings, "flags"), 0, 255, "flags", NULL, error);
}

static bool has_id(const int64_t *ids, size_t count, int64_t id) {
    for (size_t index = 0; index < count; ++index)
        if (ids[index] == id) return true;
    return false;
}

static bool check_region(const FusionJson *item, const char *prefix, int64_t width,
                         int64_t height, int64_t resolution, FebError *error) {
    static const char *const axes[] = {"x", "y"};
    static const char *const extents[] = {"width", "height"};
    int64_t limits[2] = {width, height};
    char label[64];
    for (int axis = 0; axis < 2; ++axis) {
        int64_t value = 0;
        snprintf(label, sizeof label, "%s %s", prefix, axes[axis]);
        if (!check_int(fusion_json_member(item, axes[axis]), 0, limits[axis] - resolution, label,
                       &value, error))
            return false;
        if (value % resolution)
            return feb_fail(error, FEB_EXIT_OPERATION, "%s %s must align to collision resolution",
                            prefix, axes[axis]);
    }
    for (int extent = 0; extent < 2; ++extent) {
        int64_t value = 0;
        snprintf(label, sizeof label, "%s %s", prefix, extents[extent]);
        if (!check_int(fusion_json_member(item, extents[extent]), resolution, limits[extent],
                       label, &value, error))
            return false;
        if (value % resolution || integer(item, axes[extent]) + value > limits[extent])
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid %s %s", prefix, extents[extent]);
    }
    return true;
}

static bool validate_body(const FusionJson *doc, const char *world, int64_t width,
                          int64_t height, FebError *error) {
    int64_t next_id = 0;
    if (!check_int(fusion_json_member(doc, "next_id"), 1, 1000000, "next_id", &next_id, error))
        return false;
    const FusionJson *entities = fusion_json_member(doc, "entities");
    if (entities->kind != FUSION_JSON_ARRAY || entities->count > FEB_MAX_ENTITIES)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid project entity count");
    int64_t ids[FEB_MAX_EVENTS + FEB_MAX_ENTITIES];
    size_t id_count = 0;
    bool aria = !strcmp(world, "aria");
    for (size_t index = 0; index < entities->count; ++index) {
        const FusionJson *entity = &entities->items[index];
        if (!has_keys(entity, entity_keys, COUNT(entity_keys)) &&
            !has_keys(entity, entity_settings_keys, COUNT(entity_settings_keys)))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid project entity fields");
        const FusionJson *settings = fusion_json_member(entity, "settings");
        if (settings) {
            if (!aria || !is_string(fusion_json_member(entity, "kind"), "ITEM"))
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "typed pickup fields only belong to Aria items");
            if (!has_keys(settings, item_keys, COUNT(item_keys)))
                return feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria pickup settings");
            if (!item_settings_valid(fusion_json_member(entity, "native_type"), settings, error))
                return false;
        }
        int64_t id = 0;
        if (!check_int(fusion_json_member(entity, "id"), 1, next_id - 1, "entity id", &id, error))
            return false;
        if (has_id(ids, id_count, id))
            return feb_fail(error, FEB_EXIT_OPERATION, "duplicate entity id");
        ids[id_count++] = id;
        if (!one_of(fusion_json_member(entity, "kind"), kinds, COUNT(kinds)))
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown entity kind");
        /* Aria's native object-placement grid is 8px; Zero Mission uses 16px. */
        int64_t step = aria ? 8 : 16;
        static const char *const axes[] = {"x", "y"};
        int64_t limits[2] = {width, height};
        for (int axis = 0; axis < 2; ++axis) {
            int64_t value = 0;
            if (!check_int(fusion_json_member(entity, axes[axis]), 0, limits[axis] - 16,
                           axes[axis], &value, error))
                return false;
            if (value % step)
                return feb_fail(error, FEB_EXIT_OPERATION, "%s must align to %lld pixels",
                                axes[axis], (long long)step);
        }
        if (!text_valid(fusion_json_member(entity, "label"), 80))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid entity label");
        if (!native_type_valid(fusion_json_member(entity, "native_type")))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid native type token");
    }
    const FusionJson *collision = fusion_json_member(doc, "collision");
    int64_t resolution = aria ? 8 : 16;
    const FusionJson *declared = fusion_json_member(collision, "resolution_px");
    const FusionJson *cells = fusion_json_member(collision, "cells");
    if (!has_keys(collision, collision_keys, 2) || !declared ||
        declared->kind != FUSION_JSON_NUMBER || declared->number != (double)resolution ||
        cells->kind != FUSION_JSON_ARRAY)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid project collision document");
    int64_t grid_width = width / resolution, grid_height = height / resolution;
    if ((int64_t)cells->count > grid_width * grid_height)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid project collision cell count");
    unsigned char *seen = calloc((size_t)(grid_width * grid_height) + 1, 1);
    if (!seen) return feb_oom(error);
    for (size_t index = 0; index < cells->count; ++index) {
        const FusionJson *cell = &cells->items[index];
        int64_t x = 0, y = 0;
        if (!has_keys(cell, cell_keys, 3)) {
            free(seen);
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid project collision cell fields");
        }
        if (!check_int(fusion_json_member(cell, "x"), 0, grid_width - 1, "collision x", &x,
                       error) ||
            !check_int(fusion_json_member(cell, "y"), 0, grid_height - 1, "collision y", &y,
                       error)) {
            free(seen);
            return false;
        }
        if (seen[y * grid_width + x]) {
            free(seen);
            return feb_fail(error, FEB_EXIT_OPERATION, "duplicate project collision cell");
        }
        seen[y * grid_width + x] = 1;
        if (!one_of(fusion_json_member(cell, "type"), feb_collision_types, 7)) {
            free(seen);
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown project collision type");
        }
    }
    free(seen);
    int64_t next_door = 0;
    if (!check_int(fusion_json_member(doc, "next_door_id"), 1, 1000000, "next_door_id",
                   &next_door, error))
        return false;
    const FusionJson *doors = fusion_json_member(doc, "doors");
    if (doors->kind != FUSION_JSON_ARRAY || doors->count > FEB_MAX_DOORS)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid project door count");
    int64_t door_ids[FEB_MAX_DOORS];
    size_t door_count = 0;
    for (size_t index = 0; index < doors->count; ++index) {
        const FusionJson *door = &doors->items[index];
        if (!has_keys(door, door_keys, COUNT(door_keys)) &&
            !has_keys(door, door_source_keys, COUNT(door_source_keys)))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid project door fields");
        const FusionJson *source = fusion_json_member(door, "native_source");
        if (source) {
            if (!has_keys(source, native_source_keys, 3))
                return feb_fail(error, FEB_EXIT_OPERATION, "invalid native source reference");
            if (!check_int(fusion_json_member(source, "index"), 0, 999999, "native door index",
                           NULL, error))
                return false;
            if (!token_valid(fusion_json_member(source, "variant"), 47, "_-") ||
                !native_type_valid(fusion_json_member(source, "native_type")))
                return feb_fail(error, FEB_EXIT_OPERATION, "invalid native door source identity");
            for (size_t other = 0; other < index; ++other) {
                const FusionJson *earlier = fusion_json_member(&doors->items[other], "native_source");
                if (earlier && fusion_json_equal(earlier, source))
                    return feb_fail(error, FEB_EXIT_OPERATION,
                                    "native door cannot have multiple project overrides");
            }
        }
        int64_t id = 0;
        if (!check_int(fusion_json_member(door, "id"), 1, next_door - 1, "door id", &id, error))
            return false;
        if (has_id(door_ids, door_count, id))
            return feb_fail(error, FEB_EXIT_OPERATION, "duplicate project door id");
        door_ids[door_count++] = id;
        if (!check_region(door, "door", width, height, resolution, error)) return false;
        if (!text_valid(fusion_json_member(door, "label"), 80))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid door label");
        if (!one_of(fusion_json_member(door, "door_type"), door_types, COUNT(door_types)) ||
            !one_of(fusion_json_member(door, "facing"), door_facings, COUNT(door_facings)))
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown project door type or facing");
    }
    int64_t next_transition = 0;
    if (!check_int(fusion_json_member(doc, "next_transition_id"), 1, 1000000,
                   "next_transition_id", &next_transition, error))
        return false;
    const FusionJson *transitions = fusion_json_member(doc, "transitions");
    if (transitions->kind != FUSION_JSON_ARRAY || transitions->count > FEB_MAX_TRANSITIONS)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid project transition count");
    int64_t transition_ids[FEB_MAX_TRANSITIONS], sources[FEB_MAX_TRANSITIONS];
    size_t transition_count = 0;
    for (size_t index = 0; index < transitions->count; ++index) {
        const FusionJson *transition = &transitions->items[index];
        if (!has_keys(transition, transition_keys, COUNT(transition_keys)))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid project transition fields");
        int64_t id = 0, source = 0;
        if (!check_int(fusion_json_member(transition, "id"), 1, next_transition - 1,
                       "transition id", &id, error))
            return false;
        if (has_id(transition_ids, transition_count, id))
            return feb_fail(error, FEB_EXIT_OPERATION, "duplicate project transition id");
        if (!check_int(fusion_json_member(transition, "source_door_id"), 1, next_door - 1,
                       "source_door_id", &source, error))
            return false;
        if (!has_id(door_ids, door_count, source) || has_id(sources, transition_count, source))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "transition source door is missing or already linked");
        transition_ids[transition_count] = id;
        sources[transition_count++] = source;
        const FusionJson *target_world = fusion_json_member(transition, "target_world");
        const FusionJson *target_area = fusion_json_member(transition, "target_area");
        if (is_string(target_world, "mzm")) {
            if (!one_of(target_area, feb_mzm_areas, 7))
                return feb_fail(error, FEB_EXIT_OPERATION, "invalid transition target area");
        } else if (is_string(target_world, "aria")) {
            if (target_area->kind != FUSION_JSON_STRING || !aria_area_valid(target_area->string))
                return feb_fail(error, FEB_EXIT_OPERATION, "invalid transition target area");
        } else {
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid transition target world");
        }
        if (!check_int(fusion_json_member(transition, "target_room"), 0, 999, "target_room",
                       NULL, error) ||
            !check_int(fusion_json_member(transition, "target_door_id"), 0, 999999,
                       "target_door_id", NULL, error) ||
            !check_int(fusion_json_member(transition, "spawn_x"), 0, 16383, "spawn_x", NULL,
                       error) ||
            !check_int(fusion_json_member(transition, "spawn_y"), 0, 16383, "spawn_y", NULL,
                       error))
            return false;
    }
    int64_t next_event = 0;
    if (!check_int(fusion_json_member(doc, "next_event_id"), 1, 1000000, "next_event_id",
                   &next_event, error))
        return false;
    const FusionJson *events = fusion_json_member(doc, "events");
    if (events->kind != FUSION_JSON_ARRAY || events->count > FEB_MAX_EVENTS)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid project event count");
    id_count = 0;
    for (size_t index = 0; index < events->count; ++index) {
        const FusionJson *event = &events->items[index];
        if (!has_keys(event, event_keys, COUNT(event_keys)))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid project event fields");
        int64_t id = 0;
        if (!check_int(fusion_json_member(event, "id"), 1, next_event - 1, "event id", &id, error))
            return false;
        if (has_id(ids, id_count, id))
            return feb_fail(error, FEB_EXIT_OPERATION, "duplicate project event id");
        ids[id_count++] = id;
        if (!one_of(fusion_json_member(event, "kind"), event_kinds, COUNT(event_kinds)))
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown project event kind");
        if (!check_region(event, "event", width, height, resolution, error)) return false;
        if (!text_valid(fusion_json_member(event, "label"), 80))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid event label");
        if (!one_of(fusion_json_member(event, "trigger_type"), trigger_types,
                    COUNT(trigger_types)))
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown project trigger type");
        if (!one_of(fusion_json_member(event, "action_type"), action_types, COUNT(action_types)))
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown project action type");
        if (!reference_valid(fusion_json_member(event, "action_ref")))
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid project action reference");
        if (fusion_json_member(event, "once")->kind != FUSION_JSON_BOOL)
            return feb_fail(error, FEB_EXIT_OPERATION, "event once must be boolean");
        const FusionJson *mode = fusion_json_member(event, "condition_mode");
        if (!one_of(mode, condition_modes, COUNT(condition_modes)))
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown project event condition mode");
        const FusionJson *conditions = fusion_json_member(event, "conditions");
        if (conditions->kind != FUSION_JSON_ARRAY ||
            conditions->count > FEB_MAX_EVENT_CONDITIONS)
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid project event condition count");
        if (!conditions->count && !is_string(mode, "all"))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "an event without conditions must use all mode");
        for (size_t entry = 0; entry < conditions->count; ++entry) {
            const FusionJson *condition = &conditions->items[entry];
            if (!has_keys(condition, condition_keys, 3))
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "invalid project event condition fields");
            if (!one_of(fusion_json_member(condition, "type"), feb_condition_types, 5))
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "unknown project event condition type");
            if (!reference_valid(fusion_json_member(condition, "ref")))
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "invalid project event condition reference");
            if (fusion_json_member(condition, "negated")->kind != FUSION_JSON_BOOL)
                return feb_fail(error, FEB_EXIT_OPERATION, "condition negated must be boolean");
            for (size_t other = 0; other < entry; ++other)
                if (fusion_json_equal(&conditions->items[other], condition))
                    return feb_fail(error, FEB_EXIT_OPERATION, "duplicate project event condition");
        }
    }
    return true;
}

bool feb_room_validate(const FusionJson *document, const FusionJson *world,
                       const FusionJson *area, const FusionJson *room,
                       const FusionJson *width, const FusionJson *height, FebError *error) {
    int64_t numbers[3];
    const FusionJson *values[3] = {room, width, height};
    static const char *const labels[3] = {"room", "width_px", "height_px"};
    static const int64_t lows[3] = {0, 16, 16}, highs[3] = {999, 16384, 16384};
    if (!is_string(world, "mzm") && !is_string(world, "aria"))
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported game");
    if (is_string(world, "mzm") && !one_of(area, feb_mzm_areas, 7))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Zero Mission area");
    if (is_string(world, "aria") &&
        (!area || area->kind != FUSION_JSON_STRING || !aria_area_valid(area->string)))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid Aria area");
    for (int index = 0; index < 3; ++index)
        if (!check_int(values[index], lows[index], highs[index], labels[index], &numbers[index],
                       error))
            return false;
    if (numbers[1] % 16 || numbers[2] % 16 || numbers[1] * numbers[2] > 6144 * 256)
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid room cell geometry");
    if (!has_keys(document, doc_keys, COUNT(doc_keys)))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid project entity document fields");
    const FusionJson *version = fusion_json_member(document, "version");
    if (!is_string(fusion_json_member(document, "schema"), schema_name) || !is_int(version) ||
        version->integer != VERSION ||
        !fusion_json_equal(fusion_json_member(document, "world"), world) ||
        !fusion_json_equal(fusion_json_member(document, "area"), area) ||
        !fusion_json_equal(fusion_json_member(document, "room"), room) ||
        !fusion_json_equal(fusion_json_member(document, "width_px"), width) ||
        !fusion_json_equal(fusion_json_member(document, "height_px"), height))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "project entity document belongs to another room or geometry");
    return validate_body(document, world->string, numbers[1], numbers[2], error);
}

bool feb_room_validate_scope(const FusionJson *document, const FebRoomScope *scope,
                             FebError *error) {
    FusionJson world = fusion_json_string(scope->world), area = fusion_json_string(scope->area);
    FusionJson room = fusion_json_int(scope->room), width = fusion_json_int(scope->width);
    FusionJson height = fusion_json_int(scope->height);
    bool ok = world.string && area.string &&
              feb_room_validate(document, &world, &area, &room, &width, &height, error);
    fusion_json_free(&world);
    fusion_json_free(&area);
    return ok;
}

/* A document checked against its own scope. */
static bool validate_self(const FusionJson *document, FebError *error) {
    return feb_room_validate(document, fusion_json_member(document, "world"),
                             fusion_json_member(document, "area"),
                             fusion_json_member(document, "room"),
                             fusion_json_member(document, "width_px"),
                             fusion_json_member(document, "height_px"), error);
}

static bool stage_path(const char *root, char *path, bool *staged, FebError *error) {
    *staged = false;
    const char *token = getenv("MV_EDITOR_ROOM_STAGE");
    if (!token) return true;
    bool valid = strlen(token) == 36;
    for (size_t index = 0; valid && index < 36; ++index) {
        char c = token[index];
        valid = index == 8 || index == 13 || index == 18 || index == 23
            ? c == '-' : (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    }
    if (!valid) return feb_fail(error, FEB_EXIT_OPERATION, "invalid private room editor stage token");
    snprintf(path, FEB_PATH_CAPACITY, "%s/assets/extracted/.editor_staging/%s.json", root, token);
    *staged = true;
    return true;
}

static bool read_document(const char *path, FusionJson *document, const char *invalid_file,
                          const char *unreadable, FebError *error) {
    long long size = 0;
    if (!feb_is_regular(path, &size) || size > ROOM_BYTES)
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", invalid_file);
    char detail[256];
    if (!feb_load_json(path, ROOM_BYTES, document, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", unreadable);
    return true;
}

static bool path_exists(const char *path) {
    struct stat info;
    return stat(path, &info) == 0;
}

bool feb_room_load(const char *root, const FebRoomScope *scope, FusionJson *document,
                   FebError *error) {
    char path[FEB_PATH_CAPACITY], staged_path[FEB_PATH_CAPACITY];
    bool staged = false;
    if (!feb_room_path(root, scope, path, error) ||
        !feb_room_check_path(root, path, false, error) ||
        !stage_path(root, staged_path, &staged, error))
        return false;
    if (staged) {
        if (!feb_room_check_path(root, staged_path, false, error)) return false;
        if (path_exists(staged_path)) snprintf(path, sizeof path, "%s", staged_path);
    }
    if (!path_exists(path)) return new_document(scope, document) || feb_oom(error);
    if (!read_document(path, document, "invalid project room data file",
                       "unreadable project entity document", error))
        return false;
    if (!feb_room_migrate(document, error) || !feb_room_validate_scope(document, scope, error)) {
        fusion_json_free(document);
        return false;
    }
    return true;
}

bool feb_room_save(const char *root, const FusionJson *document, char *path, FebError *error) {
    if (!validate_self(document, error)) return false;
    bool staged = false;
    if (!stage_path(root, path, &staged, error)) return false;
    if (!staged) {
        FebRoomScope scope = {.world = fusion_json_member(document, "world")->string,
                              .room = integer(document, "room"),
                              .width = integer(document, "width_px"),
                              .height = integer(document, "height_px")};
        snprintf(scope.area, sizeof scope.area, "%s", fusion_json_member(document, "area")->string);
        if (!feb_room_path(root, &scope, path, error)) return false;
    }
    if (!feb_room_check_path(root, path, true, error)) return false;
    char *text = NULL;
    size_t size = 0;
    if (!fusion_json_write(document, 2, &text, &size)) return feb_oom(error);
    char parent[FEB_PATH_CAPACITY], temporary[FEB_PATH_CAPACITY + 32];
    snprintf(parent, sizeof parent, "%s", path);
    *strrchr(parent, '/') = '\0';
    snprintf(temporary, sizeof temporary, "%s/.project-entities-XXXXXX", parent);
    int descriptor = mkstemp(temporary);
    bool ok = descriptor >= 0;
    if (ok) {
        ok = write(descriptor, text, size) == (ssize_t)size && write(descriptor, "\n", 1) == 1 &&
             fsync(descriptor) == 0;
        ok = close(descriptor) == 0 && ok;
        ok = ok && feb_room_check_path(root, path, false, error);
        if (ok && rename(temporary, path) != 0) ok = false;
        if (!ok) unlink(temporary);
    }
    free(text);
    if (!ok && !error->message[0])
        return feb_fail(error, FEB_EXIT_OPERATION, "cannot write %s", path);
    return ok;
}

bool feb_room_saved(const char *root, const char *world, const char *area, int64_t room,
                    bool required, const char *const messages[4], FusionJson *document,
                    FebError *error) {
    *document = fusion_json_null();
    FebRoomScope scope = {.world = world, .room = room, .width = 16, .height = 16};
    snprintf(scope.area, sizeof scope.area, "%s", area);
    char path[FEB_PATH_CAPACITY];
    if (!feb_room_path(root, &scope, path, error) ||
        !feb_room_check_path(root, path, false, error))
        return false;
    if (!required && !path_exists(path)) return true;
    long long size = 0;
    if (!feb_is_regular(path, &size) || size > ROOM_BYTES)
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", messages[0]);
    char detail[256];
    FusionJson raw;
    if (!feb_load_json(path, ROOM_BYTES, &raw, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", messages[1]);
    if (raw.kind != FUSION_JSON_OBJECT) {
        fusion_json_free(&raw);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", messages[2]);
    }
    if (!feb_room_migrate(&raw, error)) {
        fusion_json_free(&raw);
        return false;
    }
    const FusionJson *width = fusion_json_member(&raw, "width_px");
    const FusionJson *height = fusion_json_member(&raw, "height_px");
    if (!is_int(width) || !is_int(height)) {
        fusion_json_free(&raw);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", messages[3]);
    }
    scope.width = width->integer;
    scope.height = height->integer;
    if (!feb_room_validate_scope(&raw, &scope, error)) {
        fusion_json_free(&raw);
        return false;
    }
    *document = raw;
    return true;
}

typedef struct {
    char world[8];
    char area[32];
    int64_t room;
    FusionJson document;
    bool loaded;
} CachedRoom;

static bool cached_room(const char *root, CachedRoom **cache, size_t *count, const char *world,
                        const char *area, int64_t room, const FusionJson **document,
                        FebError *error) {
    for (size_t index = 0; index < *count; ++index) {
        CachedRoom *item = &(*cache)[index];
        if (!strcmp(item->world, world) && !strcmp(item->area, area) && item->room == room) {
            *document = item->loaded ? &item->document : NULL;
            return true;
        }
    }
    CachedRoom *grown = realloc(*cache, (*count + 1) * sizeof *grown);
    if (!grown) return feb_oom(error);
    *cache = grown;
    CachedRoom *item = &(*cache)[(*count)++];
    *item = (CachedRoom){0};
    snprintf(item->world, sizeof item->world, "%s", world);
    snprintf(item->area, sizeof item->area, "%s", area);
    item->room = room;
    static const char *const messages[4] = {
        "invalid saved project room document", "invalid saved project room JSON",
        "invalid saved project room object", "invalid saved project room geometry"};
    if (!feb_room_saved(root, world, area, room, false, messages, &item->document, error)) {
        --*count;
        return false;
    }
    item->loaded = item->document.kind == FUSION_JSON_OBJECT;
    *document = item->loaded ? &item->document : NULL;
    return true;
}

static int compare_strings(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

static int compare_connections(const void *left, const void *right) {
    const FusionJson *a = left, *b = right;
    int order = strcmp(fusion_json_member(a, "area")->string, fusion_json_member(b, "area")->string);
    if (order) return order;
    int64_t x = integer(a, "room"), y = integer(b, "room");
    if (x != y) return x < y ? -1 : 1;
    x = integer(a, "door_id");
    y = integer(b, "door_id");
    return x < y ? -1 : x > y;
}

bool feb_room_connections(const char *root, const char *world, const char *area,
                          const int64_t *room, FusionJson *connections, FebError *error) {
    *connections = fusion_json_array();
    if (strcmp(world, "mzm") && strcmp(world, "aria"))
        return feb_fail(error, FEB_EXIT_OPERATION, "unsupported project world");
    if (room && !area) return feb_fail(error, FEB_EXIT_OPERATION, "room filter requires area");
    char folder[FEB_PATH_CAPACITY];
    snprintf(folder, sizeof folder, "%s/assets/extracted/overrides/%s/entities", root,
             !strcmp(world, "mzm") ? "metroid" : "aria");
    if (feb_is_symlink(folder))
        return feb_fail(error, FEB_EXIT_OPERATION, "symlink project room folder refused");
    char **paths = NULL;
    size_t path_count = 0;
    if (room) {
        FebRoomScope scope = {.world = world, .room = *room, .width = 16, .height = 16};
        snprintf(scope.area, sizeof scope.area, "%s", area);
        paths = calloc(1, sizeof *paths);
        char path[FEB_PATH_CAPACITY];
        if (!paths || !feb_room_path(root, &scope, path, error)) {
            free(paths);
            return paths ? false : feb_oom(error);
        }
        paths[path_count++] = strdup(path);
    } else if (feb_is_directory(folder)) {
        DIR *directory = opendir(folder);
        struct dirent *entry;
        while (directory && (entry = readdir(directory)) != NULL) {
            size_t length = strlen(entry->d_name);
            if (length < 5 || strcmp(entry->d_name + length - 5, ".json")) continue;
            char **grown = realloc(paths, (path_count + 1) * sizeof *grown);
            if (!grown) break;
            paths = grown;
            char path[FEB_PATH_CAPACITY + NAME_MAX + 2];
            snprintf(path, sizeof path, "%s/%s", folder, entry->d_name);
            paths[path_count++] = strdup(path);
        }
        if (directory) closedir(directory);
        qsort(paths, path_count, sizeof *paths, compare_strings);
    }
    bool ok = path_count <= SAVED_ROOM_LIMIT ||
              feb_fail(error, FEB_EXIT_OPERATION, "too many project room documents");
    CachedRoom *cache = NULL;
    size_t cache_count = 0;
    for (size_t index = 0; ok && index < path_count; ++index) {
        const char *path = paths[index];
        if (!path_exists(path) && !feb_is_symlink(path)) continue;
        long long size = 0;
        if (feb_is_symlink(path) || !feb_is_regular(path, &size) || size > ROOM_BYTES) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid project room listing entry");
            break;
        }
        FusionJson metadata;
        char detail[256];
        if (!feb_load_json(path, ROOM_BYTES, &metadata, detail, sizeof detail)) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid project room listing JSON");
            break;
        }
        const FusionJson *document_area = fusion_json_member(&metadata, "area");
        const FusionJson *document_room = fusion_json_member(&metadata, "room");
        if (metadata.kind != FUSION_JSON_OBJECT ||
            !is_string(fusion_json_member(&metadata, "world"), world)) {
            fusion_json_free(&metadata);
            ok = feb_fail(error, FEB_EXIT_OPERATION, "project room world mismatch");
            break;
        }
        if (!document_area || document_area->kind != FUSION_JSON_STRING || !is_int(document_room)) {
            fusion_json_free(&metadata);
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid project room listing scope");
            break;
        }
        char a[32], expected[FEB_PATH_CAPACITY];
        snprintf(a, sizeof a, "%s", document_area->string);
        int64_t n = document_room->integer;
        fusion_json_free(&metadata);
        FebRoomScope scope = {.world = world, .room = n, .width = 16, .height = 16};
        snprintf(scope.area, sizeof scope.area, "%s", a);
        if (!feb_room_path(root, &scope, expected, error)) {
            ok = false;
            break;
        }
        if (strcmp(path, expected)) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "project room filename does not match identity");
            break;
        }
        if (area && strcmp(a, area)) continue;
        const FusionJson *doc = NULL;
        if (!cached_room(root, &cache, &cache_count, world, a, n, &doc, error)) {
            ok = false;
            break;
        }
        if (!doc) continue;
        FusionJson own;
        if (!fusion_json_clone(doc, &own)) {
            ok = feb_oom(error);
            break;
        }
        const FusionJson *doors = fusion_json_member(&own, "doors");
        const FusionJson *transitions = fusion_json_member(&own, "transitions");
        for (size_t entry = 0; ok && entry < doors->count; ++entry) {
            const FusionJson *door = &doors->items[entry];
            int64_t door_id = integer(door, "id");
            const FusionJson *link = NULL;
            for (size_t other = 0; other < transitions->count; ++other)
                if (integer(&transitions->items[other], "source_door_id") == door_id)
                    link = &transitions->items[other];
            bool reciprocal = false;
            const char *state = "missing";
            if (link) {
                const char *target_world = fusion_json_member(link, "target_world")->string;
                bool interworld = strcmp(world, target_world) != 0;
                if (integer(link, "target_door_id")) {
                    const FusionJson *target = NULL;
                    if (!cached_room(root, &cache, &cache_count, target_world,
                                     fusion_json_member(link, "target_area")->string,
                                     integer(link, "target_room"), &target, error)) {
                        ok = false;
                        break;
                    }
                    bool found = false;
                    const FusionJson *target_doors = target ? fusion_json_member(target, "doors") : NULL;
                    for (size_t other = 0; target_doors && other < target_doors->count; ++other)
                        if (integer(&target_doors->items[other], "id") == integer(link, "target_door_id"))
                            found = true;
                    if (!found) {
                        state = "invalid";
                    } else {
                        const FusionJson *back = fusion_json_member(target, "transitions");
                        for (size_t other = 0; other < back->count; ++other) {
                            const FusionJson *t = &back->items[other];
                            if (integer(t, "source_door_id") == integer(link, "target_door_id") &&
                                is_string(fusion_json_member(t, "target_world"), world) &&
                                is_string(fusion_json_member(t, "target_area"), a) &&
                                integer(t, "target_room") == n &&
                                integer(t, "target_door_id") == door_id)
                                reciprocal = true;
                        }
                        state = interworld ? "interworld" : reciprocal ? "reciprocal" : "simple";
                    }
                } else {
                    state = interworld ? "interworld" : "simple";
                }
            }
            FusionJson record = fusion_json_object();
            ok = feb_put_string(&record, "world", world) && feb_put_string(&record, "area", a) &&
                 feb_put_int(&record, "room", n) && feb_put_int(&record, "door_id", door_id) &&
                 feb_put_string(&record, "target_world",
                                link ? fusion_json_member(link, "target_world")->string : "-") &&
                 feb_put_string(&record, "target_area",
                                link ? fusion_json_member(link, "target_area")->string : "-") &&
                 feb_put_int(&record, "target_room", link ? integer(link, "target_room") : 0) &&
                 feb_put_int(&record, "target_door_id",
                             link ? integer(link, "target_door_id") : 0) &&
                 feb_put_string(&record, "state", state) &&
                 feb_put_bool(&record, "reciprocal", reciprocal) &&
                 fusion_json_push(connections, record);
            if (!ok && !error->message[0]) feb_oom(error);
        }
        fusion_json_free(&own);
    }
    for (size_t index = 0; index < cache_count; ++index) fusion_json_free(&cache[index].document);
    free(cache);
    for (size_t index = 0; index < path_count; ++index) free(paths[index]);
    free(paths);
    if (!ok) {
        fusion_json_free(connections);
        return false;
    }
    qsort(connections->items, connections->count, sizeof *connections->items, compare_connections);
    return true;
}

bool feb_collision_capabilities(const char *world, FusionJson *result) {
    static const char *const references[2][7][2] = {
        {{"verified_exact", "CLIPDATA_TYPE_SOLID"},
         {"verified_exact", "CLIPDATA_TYPE_PASS_THROUGH_BOTTOM"},
         {"verified_family", "CLIP_BEHAVIOR_LAVA/WEAK_ACID/STRONG_ACID"},
         {"verified_family", "CLIPDATA_TYPE_*_FLOOR_SLOPE"},
         {"verified_family", "CLIPDATA_TYPE_*_FLOOR_SLOPE"},
         {"verified_exact", "CLIP_BEHAVIOR_WATER"},
         {"verified_exact", "CLIPDATA_TYPE_AIR"}},
        {{"verified_exact", "collision byte: top + sides/bottom"},
         {"verified_exact", "collision byte: top only"},
         {"verified_family", "collision byte: damage effect"},
         {"verified_family", "collision byte: floor slope + horizontal flip"},
         {"verified_family", "collision byte: floor slope + horizontal flip"},
         {"verified_exact", "collision byte: water bit"},
         {"verified_exact", "collision byte: blank shape"}},
    };
    bool aria = !strcmp(world, "aria");
    *result = fusion_json_object();
    FusionJson types = fusion_json_array();
    bool ok = true;
    for (size_t index = 0; ok && index < 7; ++index) {
        FusionJson type = fusion_json_object();
        ok = feb_put_string(&type, "type", feb_collision_types[index]) &&
             feb_put_string(&type, "project_authoring", "available") &&
             feb_put_string(&type, "native_reference_status", references[aria][index][0]) &&
             feb_put_string(&type, "native_reference", references[aria][index][1]) &&
             fusion_json_push(&types, type);
    }
    ok = ok && feb_put_string(result, "world", world) &&
         feb_put_int(result, "resolution_px", aria ? 8 : 16) &&
         feb_put_string(result, "native_encoder", "unavailable") && feb_put(result, "types", types);
    if (!ok) fusion_json_free(result);
    return ok;
}

/* Replaces ``document`` with ``candidate`` once the candidate validates. */
static bool commit(FusionJson *document, FusionJson *candidate, FebError *error) {
    if (!validate_self(candidate, error)) {
        fusion_json_free(candidate);
        return false;
    }
    fusion_json_free(document);
    *document = *candidate;
    return true;
}

static FusionJson *find_item(FusionJson *document, const char *list_key, int64_t id) {
    FusionJson *list = fusion_json_member_mut(document, list_key);
    for (size_t index = 0; list && index < list->count; ++index)
        if (integer(&list->items[index], "id") == id) return &list->items[index];
    return NULL;
}

static bool reference_named(const FusionJson *document, const char *action, const char *ref,
                            int64_t skip) {
    const FusionJson *events = fusion_json_member(document, "events");
    for (size_t index = 0; index < events->count; ++index) {
        const FusionJson *event = &events->items[index];
        if (skip && integer(event, "id") == skip) continue;
        if (is_string(fusion_json_member(event, "action_type"), action) &&
            is_string(fusion_json_member(event, "action_ref"), ref))
            return true;
    }
    return false;
}

static bool condition_named(const FusionJson *document, const char *type, const char *ref,
                            int64_t skip) {
    const FusionJson *events = fusion_json_member(document, "events");
    for (size_t index = 0; index < events->count; ++index) {
        const FusionJson *event = &events->items[index];
        if (skip && integer(event, "id") == skip) continue;
        const FusionJson *conditions = fusion_json_member(event, "conditions");
        for (size_t entry = 0; entry < conditions->count; ++entry)
            if (is_string(fusion_json_member(&conditions->items[entry], "type"), type) &&
                is_string(fusion_json_member(&conditions->items[entry], "ref"), ref))
                return true;
    }
    return false;
}

bool feb_entity_move(FusionJson *document, int64_t id, int64_t x, int64_t y, FebError *error) {
    if (!validate_self(document, error)) return false;
    FusionJson candidate;
    if (!fusion_json_clone(document, &candidate)) return feb_oom(error);
    FusionJson *entity = find_item(&candidate, "entities", id);
    if (!entity) {
        fusion_json_free(&candidate);
        return feb_fail(error, FEB_EXIT_OPERATION, "unknown project entity id");
    }
    if (!feb_put_int(entity, "x", x) || !feb_put_int(entity, "y", y)) {
        fusion_json_free(&candidate);
        return feb_oom(error);
    }
    return commit(document, &candidate, error);
}

bool feb_entity_delete(FusionJson *document, int64_t id, FebError *error) {
    if (!validate_self(document, error)) return false;
    if (!find_item(document, "entities", id))
        return feb_fail(error, FEB_EXIT_OPERATION, "unknown project entity id");
    char ref[64];
    snprintf(ref, sizeof ref, "entity:%lld", (long long)id);
    if (reference_named(document, "spawn", ref, 0))
        return feb_fail(error, FEB_EXIT_OPERATION, "entity is referenced by a project event");
    if (condition_named(document, "entity_present", ref, 0))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "entity is referenced by a project event condition");
    return feb_room_remove(document, "entities", id, "unknown project entity id", error) ||
           false;
}

bool feb_collision_get(const FusionJson *document, int64_t x, int64_t y, const char **type,
                       FebError *error) {
    if (!validate_self(document, error)) return false;
    const FusionJson *collision = fusion_json_member(document, "collision");
    int64_t resolution = integer(collision, "resolution_px");
    FusionJson value = fusion_json_int(x);
    if (!check_int(&value, 0, integer(document, "width_px") / resolution - 1, "collision x", NULL,
                   error))
        return false;
    value = fusion_json_int(y);
    if (!check_int(&value, 0, integer(document, "height_px") / resolution - 1, "collision y",
                   NULL, error))
        return false;
    *type = "empty";
    const FusionJson *cells = fusion_json_member(collision, "cells");
    for (size_t index = 0; index < cells->count; ++index)
        if (integer(&cells->items[index], "x") == x && integer(&cells->items[index], "y") == y) {
            *type = fusion_json_member(&cells->items[index], "type")->string;
            break;
        }
    return true;
}

/* The grid of a document's sparse cells: type index + 1, zero when empty. */
static bool cell_grid(const FusionJson *document, unsigned char **grid, int64_t *width,
                      int64_t *height, FebError *error) {
    const FusionJson *collision = fusion_json_member(document, "collision");
    int64_t resolution = integer(collision, "resolution_px");
    *width = integer(document, "width_px") / resolution;
    *height = integer(document, "height_px") / resolution;
    *grid = calloc((size_t)(*width * *height) + 1, 1);
    if (!*grid) return feb_oom(error);
    const FusionJson *cells = fusion_json_member(collision, "cells");
    for (size_t index = 0; index < cells->count; ++index) {
        const FusionJson *cell = &cells->items[index];
        for (unsigned char type = 0; type < 7; ++type)
            if (is_string(fusion_json_member(cell, "type"), feb_collision_types[type]))
                (*grid)[integer(cell, "y") * *width + integer(cell, "x")] = (unsigned char)(type + 1);
    }
    return true;
}

/* The candidate with cells rebuilt from the grid, sorted by (y, x). */
static bool commit_grid(FusionJson *document, const unsigned char *grid, int64_t width,
                        int64_t height, FebError *error) {
    FusionJson candidate, cells = fusion_json_array();
    if (!fusion_json_clone(document, &candidate)) return feb_oom(error);
    bool ok = true;
    for (int64_t y = 0; ok && y < height; ++y) {
        for (int64_t x = 0; ok && x < width; ++x) {
            unsigned char type = grid[y * width + x];
            if (!type) continue;
            FusionJson cell = fusion_json_object();
            ok = feb_put_int(&cell, "x", x) && feb_put_int(&cell, "y", y) &&
                 feb_put_string(&cell, "type", feb_collision_types[type - 1]) &&
                 fusion_json_push(&cells, cell);
        }
    }
    FusionJson *collision = fusion_json_member_mut(&candidate, "collision");
    ok = ok && fusion_json_set(collision, "cells", cells);
    if (!ok) {
        fusion_json_free(&candidate);
        return feb_oom(error);
    }
    return commit(document, &candidate, error);
}

static bool collision_type_index(const FusionJson *type, unsigned char *index, FebError *error) {
    for (unsigned char candidate = 0; candidate < 7; ++candidate)
        if (is_string(type, feb_collision_types[candidate])) {
            *index = candidate;
            return true;
        }
    return feb_fail(error, FEB_EXIT_OPERATION, "unknown project collision type");
}

static bool region_bounds(int64_t grid_width, int64_t grid_height, int64_t x, int64_t y,
                          int64_t width, int64_t height, FebError *error) {
    FusionJson value = fusion_json_int(x);
    if (!check_int(&value, 0, grid_width - 1, "collision x", NULL, error)) return false;
    value = fusion_json_int(y);
    if (!check_int(&value, 0, grid_height - 1, "collision y", NULL, error)) return false;
    value = fusion_json_int(width);
    if (!check_int(&value, 1, grid_width - x, "collision width", NULL, error)) return false;
    value = fusion_json_int(height);
    return check_int(&value, 1, grid_height - y, "collision height", NULL, error);
}

bool feb_collision_fill(FusionJson *document, int64_t x, int64_t y, int64_t width,
                        int64_t height, const FusionJson *type, int64_t *changed,
                        FebError *error) {
    unsigned char type_index = 0, *grid = NULL;
    int64_t grid_width = 0, grid_height = 0;
    if (!validate_self(document, error) || !collision_type_index(type, &type_index, error) ||
        !cell_grid(document, &grid, &grid_width, &grid_height, error))
        return false;
    bool ok = region_bounds(grid_width, grid_height, x, y, width, height, error);
    *changed = 0;
    for (int64_t cell_y = y; ok && cell_y < y + height; ++cell_y)
        for (int64_t cell_x = x; cell_x < x + width; ++cell_x) {
            unsigned char *cell = &grid[cell_y * grid_width + cell_x];
            if (*cell != type_index + 1) {
                *cell = (unsigned char)(type_index + 1);
                ++*changed;
            }
        }
    ok = ok && commit_grid(document, grid, grid_width, grid_height, error);
    free(grid);
    return ok;
}

bool feb_collision_stroke(FusionJson *document, const int64_t (*points)[2], size_t count,
                          const FusionJson *type, int64_t *changed, FebError *error) {
    unsigned char type_index = 0, *grid = NULL;
    int64_t grid_width = 0, grid_height = 0;
    if (!validate_self(document, error) || !collision_type_index(type, &type_index, error))
        return false;
    if (count < 1 || count > 4096)
        return feb_fail(error, FEB_EXIT_OPERATION, "collision stroke must contain 1..4096 points");
    if (!cell_grid(document, &grid, &grid_width, &grid_height, error)) return false;
    bool ok = true;
    for (size_t index = 0; ok && index < count; ++index) {
        FusionJson value = fusion_json_int(points[index][0]);
        ok = check_int(&value, 0, grid_width - 1, "collision x", NULL, error);
        value = fusion_json_int(points[index][1]);
        ok = ok && check_int(&value, 0, grid_height - 1, "collision y", NULL, error);
    }
    *changed = 0;
    for (size_t index = 0; ok && index < count; ++index) {
        unsigned char *cell = &grid[points[index][1] * grid_width + points[index][0]];
        if (*cell != type_index + 1) {
            *cell = (unsigned char)(type_index + 1);
            ++*changed;
        }
    }
    ok = ok && commit_grid(document, grid, grid_width, grid_height, error);
    free(grid);
    return ok;
}

bool feb_collision_clear(FusionJson *document, int64_t x, int64_t y, int64_t width,
                         int64_t height, int64_t *changed, FebError *error) {
    if (!validate_self(document, error)) return false;
    const FusionJson *collision = fusion_json_member(document, "collision");
    int64_t resolution = integer(collision, "resolution_px");
    if (!region_bounds(integer(document, "width_px") / resolution,
                       integer(document, "height_px") / resolution, x, y, width, height, error))
        return false;
    FusionJson candidate;
    if (!fusion_json_clone(document, &candidate)) return feb_oom(error);
    FusionJson *cells = fusion_json_member_mut(fusion_json_member_mut(&candidate, "collision"),
                                               "cells");
    FusionJson retained = fusion_json_array();
    bool ok = true;
    for (size_t index = 0; ok && index < cells->count; ++index) {
        int64_t cell_x = integer(&cells->items[index], "x");
        int64_t cell_y = integer(&cells->items[index], "y");
        if (x <= cell_x && cell_x < x + width && y <= cell_y && cell_y < y + height) continue;
        ok = fusion_json_push(&retained, cells->items[index]);
        cells->items[index] = fusion_json_null();
    }
    *changed = (int64_t)cells->count - (int64_t)retained.count;
    ok = ok && fusion_json_set(fusion_json_member_mut(&candidate, "collision"), "cells", retained);
    if (!ok) {
        fusion_json_free(&candidate);
        return feb_oom(error);
    }
    return commit(document, &candidate, error);
}

bool feb_room_append(FusionJson *document, const char *list_key, const char *counter_key,
                     size_t limit, const char *limit_message, FusionJson item,
                     FusionJson *created, FebError *error) {
    if (!validate_self(document, error)) {
        fusion_json_free(&item);
        return false;
    }
    if (fusion_json_member(document, list_key)->count >= limit) {
        fusion_json_free(&item);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", limit_message);
    }
    int64_t id = integer(document, counter_key);
    FusionJson entry = fusion_json_object();
    bool ok = feb_put_int(&entry, "id", id);
    for (size_t index = 0; ok && index < item.count; ++index) {
        ok = fusion_json_set(&entry, item.keys[index], item.items[index]);
        item.items[index] = fusion_json_null();
    }
    fusion_json_free(&item);
    FusionJson candidate, copy;
    ok = ok && fusion_json_clone(document, &candidate);
    if (!ok) {
        fusion_json_free(&entry);
        return feb_oom(error);
    }
    if (!fusion_json_clone(&entry, created) || !feb_put_int(&candidate, counter_key, id + 1) ||
        !fusion_json_clone(&entry, &copy) ||
        !fusion_json_push(fusion_json_member_mut(&candidate, list_key), copy)) {
        fusion_json_free(&entry);
        fusion_json_free(&candidate);
        return feb_oom(error);
    }
    fusion_json_free(&entry);
    if (!commit(document, &candidate, error)) {
        fusion_json_free(created);
        return false;
    }
    return true;
}

bool feb_room_update(FusionJson *document, const char *list_key, int64_t id,
                     const FusionJson *changes, const char *const *editable,
                     const char *fields_message, const char *unknown_message,
                     FusionJson *changed, FebError *error) {
    if (!validate_self(document, error)) return false;
    bool allowed = changes->count > 0;
    for (size_t index = 0; allowed && index < changes->count; ++index) {
        bool found = false;
        for (const char *const *field = editable; *field; ++field)
            if (!strcmp(*field, changes->keys[index])) found = true;
        allowed = found;
    }
    if (!allowed) return feb_fail(error, FEB_EXIT_OPERATION, "%s", fields_message);
    FusionJson candidate;
    if (!fusion_json_clone(document, &candidate)) return feb_oom(error);
    FusionJson *item = find_item(&candidate, list_key, id);
    if (!item) {
        fusion_json_free(&candidate);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", unknown_message);
    }
    for (size_t index = 0; index < changes->count; ++index) {
        FusionJson copy;
        if (!fusion_json_clone(&changes->items[index], &copy) ||
            !fusion_json_set(item, changes->keys[index], copy)) {
            fusion_json_free(&candidate);
            return feb_oom(error);
        }
    }
    if (!fusion_json_clone(item, changed)) {
        fusion_json_free(&candidate);
        return feb_oom(error);
    }
    if (!commit(document, &candidate, error)) {
        fusion_json_free(changed);
        return false;
    }
    return true;
}

bool feb_room_remove(FusionJson *document, const char *list_key, int64_t id,
                     const char *unknown_message, FebError *error) {
    FusionJson candidate;
    if (!fusion_json_clone(document, &candidate)) return feb_oom(error);
    FusionJson *list = fusion_json_member_mut(&candidate, list_key);
    size_t kept = 0;
    bool found = false;
    for (size_t index = 0; index < list->count; ++index) {
        if (integer(&list->items[index], "id") == id) {
            fusion_json_free(&list->items[index]);
            found = true;
            continue;
        }
        list->items[kept++] = list->items[index];
    }
    list->count = kept;
    if (!found) {
        fusion_json_free(&candidate);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", unknown_message);
    }
    /* Entity deletion stores the list without revalidating, as before. */
    if (!strcmp(list_key, "entities")) {
        fusion_json_free(document);
        *document = candidate;
        return true;
    }
    return commit(document, &candidate, error);
}

bool feb_door_adopt(FusionJson *document, FusionJson source, int64_t x, int64_t y,
                    int64_t width, int64_t height, FusionJson *door, FebError *error) {
    if (!validate_self(document, error)) {
        fusion_json_free(&source);
        return false;
    }
    const FusionJson *doors = fusion_json_member(document, "doors");
    for (size_t index = 0; index < doors->count; ++index) {
        const FusionJson *existing = fusion_json_member(&doors->items[index], "native_source");
        if (existing && fusion_json_equal(existing, &source)) {
            fusion_json_free(&source);
            return fusion_json_clone(&doors->items[index], door) || feb_oom(error);
        }
    }
    char label[96];
    snprintf(label, sizeof label, "Native door %lld override",
             (long long)integer(&source, "index"));
    const char *facing = x == 0 ? "left"
        : x + width == integer(document, "width_px") ? "right"
        : y == 0 ? "up" : y + height == integer(document, "height_px") ? "down" : "left";
    FusionJson item = fusion_json_object();
    bool ok = feb_put_int(&item, "x", x) && feb_put_int(&item, "y", y) &&
              feb_put_int(&item, "width", width) && feb_put_int(&item, "height", height) &&
              feb_put_string(&item, "label", label) && feb_put_string(&item, "door_type", "normal") &&
              feb_put_string(&item, "facing", facing) && feb_put(&item, "native_source", source);
    if (!ok) {
        fusion_json_free(&item);
        return feb_oom(error);
    }
    return feb_room_append(document, "doors", "next_door_id", FEB_MAX_DOORS,
                           "project door limit reached", item, door, error);
}
