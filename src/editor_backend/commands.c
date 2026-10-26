/* SPDX-License-Identifier: GPL-3.0-only */
/* execute() of the native editor backend: the command tables and the
 * commands ported from scripts/editor_backend.py. */
#define _DEFAULT_SOURCE
#include "backend.h"

#include "catalog.h"
#include "commands_room.h"
#include "common.h"
#include "drafts.h"
#include "object_catalog.h"
#include "workspace.h"
#include "mzm_render.h"
#include "mzm_workspace.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* CAPABILITIES of the former Python backend, in its order. */
static const char *const capabilities[][2] = {
    {"project-info", "available"},
    {"project-validate", "available"},
    {"project-audit", "available_structural_and_render"},
    {"list-worlds", "available"},
    {"list-areas", "available"},
    {"list-rooms", "available"},
    {"list-assets", "available_native_object_metadata"},
    {"room-list", "available"},
    {"room-inspect", "available"},
    {"room-export", "available_saved_project_room_package_only"},
    {"room-open", "available_partial_native_workspace"},
    {"room-create", "available_draft_only"},
    {"room-validate", "available_draft_only"},
    {"room-render", "available_partial_native_render"},
    {"room-audit", "available_partial_native_render"},
    {"room-place", "available_draft_only"},
    {"room-move", "available_draft_only"},
    {"room-unplace", "available_draft_only"},
    {"placement-list", "available_draft_only"},
    {"entity-list", "available_project_markers_only"},
    {"entity-inspect", "available_project_markers_only"},
    {"entity-create", "available_project_markers_only"},
    {"entity-move", "available_project_markers_only"},
    {"entity-update", "available_project_markers_only"},
    {"entity-delete", "available_project_markers_only"},
    {"entity-assign", "available_project_markers_only"},
    {"entity-catalog", "available_native_reference_metadata"},
    {"entity-item-settings", "available_project_markers_only"},
    {"story-validate", "available_project_data_only"},
    {"story-save", "available_project_data_only"},
    {"layer-list", "available_private_native_workspace"},
    {"tile-get", "available_private_native_workspace"},
    {"tile-set", "available_private_native_workspace"},
    {"tile-fill", "available_private_native_workspace"},
    {"collision-capabilities", "available_project_and_native_reference_metadata"},
    {"collision-list", "available_project_room_data_only"},
    {"collision-get", "available_project_room_data_only"},
    {"collision-set", "available_project_room_data_only"},
    {"collision-fill", "available_project_room_data_only"},
    {"collision-stroke", "available_project_room_data_only"},
    {"collision-clear", "available_project_room_data_only"},
    {"collision-validate", "available_project_room_data_only"},
    {"event-list", "available_project_room_data_only"},
    {"event-inspect", "available_project_room_data_only"},
    {"event-validate", "available_project_room_data_only"},
    {"event-create", "available_project_room_data_only"},
    {"event-update", "available_project_room_data_only"},
    {"event-delete", "available_project_room_data_only"},
    {"door-list", "available_project_room_data_only"},
    {"door-target-list", "available_saved_project_door_targets_only"},
    {"connection-list", "available_saved_project_connections_only"},
    {"door-return-plan", "available_project_room_data_only"},
    {"door-inspect", "available_project_room_data_only"},
    {"door-create", "available_project_room_data_only"},
    {"door-adopt", "available_project_room_data_only"},
    {"door-update", "available_project_room_data_only"},
    {"door-delete", "available_project_room_data_only"},
    {"door-link", "available_project_room_data_only"},
    {"transition-list", "available_project_room_data_only"},
    {"transition-create", "available_project_room_data_only"},
    {"transition-update", "available_project_room_data_only"},
    {"transition-delete", "available_project_room_data_only"},
    {"transition-validate", "available_project_room_data_only"},
    {"object-definition-create", "unavailable_schema_pending"},
    {"cutscene-create", "unavailable_backend_extraction_pending"},
    {"audio-list", "unavailable_native_inventory_pending"},
    {"audio-render", "unavailable_decoder_pending"},
    {"native-gameplay", "unavailable_engines_not_implemented"},
};

/* COMMAND_FIELDS: each command's allowed options, sorted. */
static const struct {
    const char *command;
    const char *const *fields;
} command_fields[] = {
    {"capabilities", (const char *const[]){NULL}},
    {"project-info", (const char *const[]){NULL}},
    {"project-validate", (const char *const[]){NULL}},
    {"project-audit", (const char *const[]){"workers", NULL}},
    {"list-worlds", (const char *const[]){NULL}},
    {"list-areas", (const char *const[]){"world", NULL}},
    {"list-rooms", (const char *const[]){"area", "world", NULL}},
    {"list-assets", (const char *const[]){"world", NULL}},
    {"room-list", (const char *const[]){"area", "source", "world", NULL}},
    {"room-inspect", (const char *const[]){"area", "room", "source", "world", NULL}},
    {"room-export", (const char *const[]){"area", "room", "world", NULL}},
    {"room-open", (const char *const[]){"area", "room", "world", NULL}},
    {"room-create", (const char *const[]){
        "area", "height_screens", "name", "slug", "width_screens", "world", NULL
    }},
    {"room-validate", (const char *const[]){"room", NULL}},
    {"room-render", (const char *const[]){"area", "room", "world", NULL}},
    {"room-audit", (const char *const[]){"area", "report", "workers", "world", NULL}},
    {"room-place", (const char *const[]){"room", "x", "y", NULL}},
    {"room-move", (const char *const[]){"room", "x", "y", NULL}},
    {"room-unplace", (const char *const[]){"confirm", "room", NULL}},
    {"placement-list", (const char *const[]){"world", NULL}},
    {"entity-list", (const char *const[]){
        "area", "height", "preview", "room", "width", "world", NULL
    }},
    {"entity-inspect", (const char *const[]){
        "area", "height", "id", "room", "width", "world", NULL
    }},
    {"entity-create", (const char *const[]){
        "area", "flags", "height", "item_id", "kind", "label", "native_type", "parameter_0",
        "parameter_1", "room", "width", "world", "x", "y", NULL
    }},
    {"entity-move", (const char *const[]){
        "area", "height", "id", "room", "width", "world", "x", "y", NULL
    }},
    {"entity-update", (const char *const[]){
        "area", "flags", "height", "id", "item_id", "label", "native_type", "parameter_0",
        "parameter_1", "room", "width", "world", "x", "y", NULL
    }},
    {"entity-delete", (const char *const[]){
        "area", "confirm", "height", "id", "room", "width", "world", NULL
    }},
    {"entity-assign", (const char *const[]){
        "area", "flags", "height", "id", "item_id", "native_type", "parameter_0",
        "parameter_1", "room", "width", "world", NULL
    }},
    {"entity-catalog", (const char *const[]){"kind", "world", NULL}},
    {"entity-item-settings", (const char *const[]){
        "area", "height", "id", "room", "width", "world", NULL
    }},
    {"story-validate", (const char *const[]){"input", "kind", NULL}},
    {"story-save", (const char *const[]){"input", "kind", "target", NULL}},
    {"layer-list", (const char *const[]){"area", "room", "world", NULL}},
    {"tile-get", (const char *const[]){"area", "layer", "room", "world", "x", "y", NULL}},
    {"tile-set", (const char *const[]){
        "area", "layer", "room", "tile_id", "world", "x", "y", NULL
    }},
    {"tile-fill", (const char *const[]){
        "area", "layer", "room", "tile_id", "world", "x", "y", NULL
    }},
    {"collision-capabilities", (const char *const[]){"world", NULL}},
    {"collision-list", (const char *const[]){"area", "height", "room", "width", "world", NULL}},
    {"collision-get", (const char *const[]){
        "area", "height", "room", "width", "world", "x", "y", NULL
    }},
    {"collision-set", (const char *const[]){
        "area", "collision_type", "height", "room", "width", "world", "x", "y", NULL
    }},
    {"collision-fill", (const char *const[]){
        "area", "collision_type", "fill_height", "fill_width", "height", "room", "width",
        "world", "x", "y", NULL
    }},
    {"collision-stroke", (const char *const[]){
        "area", "collision_type", "height", "points", "room", "width", "world", NULL
    }},
    {"collision-clear", (const char *const[]){
        "area", "confirm", "fill_height", "fill_width", "height", "room", "width", "world",
        "x", "y", NULL
    }},
    {"collision-validate", (const char *const[]){
        "area", "height", "room", "width", "world", NULL
    }},
    {"event-list", (const char *const[]){"area", "height", "room", "width", "world", NULL}},
    {"event-inspect", (const char *const[]){
        "area", "height", "id", "room", "width", "world", NULL
    }},
    {"event-validate", (const char *const[]){"area", "height", "room", "width", "world", NULL}},
    {"event-create", (const char *const[]){
        "action_ref", "action_type", "area", "condition_mode", "conditions", "event_kind",
        "height", "label", "once", "region_height", "region_width", "room", "trigger_type",
        "width", "world", "x", "y", NULL
    }},
    {"event-update", (const char *const[]){
        "action_ref", "action_type", "area", "condition_mode", "conditions", "event_kind",
        "height", "id", "label", "once", "region_height", "region_width", "room",
        "trigger_type", "width", "world", "x", "y", NULL
    }},
    {"event-delete", (const char *const[]){
        "area", "confirm", "height", "id", "room", "width", "world", NULL
    }},
    {"door-list", (const char *const[]){"area", "height", "room", "width", "world", NULL}},
    {"door-target-list", (const char *const[]){
        "target_area", "target_room", "target_world", NULL
    }},
    {"connection-list", (const char *const[]){"area", "room", "world", NULL}},
    {"door-return-plan", (const char *const[]){
        "area", "room", "source_door_id", "world", NULL
    }},
    {"door-inspect", (const char *const[]){
        "area", "height", "id", "room", "width", "world", NULL
    }},
    {"door-create", (const char *const[]){
        "area", "door_height", "door_type", "door_width", "facing", "height", "label",
        "room", "width", "world", "x", "y", NULL
    }},
    {"door-adopt", (const char *const[]){
        "area", "height", "native_index", "native_type", "native_variant", "room", "width",
        "world", NULL
    }},
    {"door-update", (const char *const[]){
        "area", "door_height", "door_type", "door_width", "facing", "height", "id", "label",
        "room", "width", "world", "x", "y", NULL
    }},
    {"door-delete", (const char *const[]){
        "area", "confirm", "height", "id", "room", "width", "world", NULL
    }},
    {"door-link", (const char *const[]){
        "area", "height", "room", "source_door_id", "spawn_x", "spawn_y", "target_area",
        "target_door_id", "target_room", "target_world", "width", "world", NULL
    }},
    {"transition-list", (const char *const[]){
        "area", "height", "room", "width", "world", NULL
    }},
    {"transition-create", (const char *const[]){
        "area", "height", "room", "source_door_id", "spawn_x", "spawn_y", "target_area",
        "target_door_id", "target_room", "target_world", "width", "world", NULL
    }},
    {"transition-update", (const char *const[]){
        "area", "height", "id", "room", "source_door_id", "spawn_x", "spawn_y",
        "target_area", "target_door_id", "target_room", "target_world", "width", "world",
        NULL
    }},
    {"transition-delete", (const char *const[]){
        "area", "confirm", "height", "id", "room", "width", "world", NULL
    }},
    {"transition-validate", (const char *const[]){
        "area", "height", "room", "width", "world", NULL
    }},
};

static const char *const mutating_commands[] = {
    "collision-clear", "collision-fill", "collision-set", "collision-stroke", "door-adopt",
    "door-create", "door-delete", "door-link", "door-update", "entity-assign",
    "entity-create", "entity-delete", "entity-move", "entity-update", "event-create",
    "event-delete", "event-update", "room-create", "room-export", "room-move", "room-open",
    "room-place", "room-unplace", "story-save", "tile-fill", "tile-set",
    "transition-create", "transition-delete", "transition-update",
};

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

/* The commands implemented here; the others still run in Python. */
static const char *const native_commands[] = {
    "capabilities", "project-info", "list-worlds", "list-areas", "list-rooms",
    "room-list", "room-inspect", "room-validate", "placement-list", "room-create",
    "room-place", "room-move", "room-unplace", "list-assets", "entity-catalog",
    "room-render", "room-open",
};

bool feb_is_native(const char *command) {
    if (command && (feb_is_room_command(command) || feb_is_workspace_command(command)))
        return true;
    for (size_t index = 0; command && index < COUNT(native_commands); ++index)
        if (!strcmp(native_commands[index], command)) return true;
    return false;
}

const char *const *feb_command_fields(const char *command, bool *known) {
    for (size_t index = 0; command && index < COUNT(command_fields); ++index) {
        if (!strcmp(command_fields[index].command, command)) {
            *known = true;
            return command_fields[index].fields;
        }
    }
    *known = false;
    return NULL;
}

const char *feb_capability(const char *command) {
    for (size_t index = 0; command && index < COUNT(capabilities); ++index)
        if (!strcmp(capabilities[index][0], command)) return capabilities[index][1];
    return NULL;
}

static int compare_strings(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

bool feb_unknown_options(const FusionJson *options, const char *const *fields,
                         char *unknown, size_t size) {
    const char *names[128];
    size_t count = 0;
    for (size_t index = 0; index < options->count && count < COUNT(names); ++index) {
        bool allowed = false;
        for (const char *const *field = fields; *field; ++field)
            if (!strcmp(*field, options->keys[index])) allowed = true;
        if (!allowed) names[count++] = options->keys[index];
    }
    qsort(names, count, sizeof names[0], compare_strings);
    unknown[0] = '\0';
    for (size_t index = 0; index < count; ++index) {
        size_t used = strlen(unknown);
        snprintf(unknown + used, size - used, "%s%s", index ? ", " : "", names[index]);
    }
    return count == 0;
}

bool feb_is_mutating(const char *command) {
    for (size_t index = 0; command && index < COUNT(mutating_commands); ++index)
        if (!strcmp(mutating_commands[index], command)) return true;
    return false;
}

/* _validated_root(): an existing directory reached without symlinks. */
static bool validated_root(const char *value, char *resolved, FebError *error) {
    char absolute[FEB_PATH_CAPACITY];
    if (!feb_absolute(value, absolute))
        return feb_fail(error, FEB_EXIT_OPERATION, "project root path is too long");
    /* Path() drops empty and "." components but keeps "..". */
    char copy[FEB_PATH_CAPACITY], current[FEB_PATH_CAPACITY] = "";
    snprintf(copy, sizeof copy, "%s", value);
    for (char *part = strtok(copy, "/"); part; part = strtok(NULL, "/"))
        if (!strcmp(part, ".."))
            return feb_fail(error, FEB_EXIT_OPERATION, "project root path traversal refused");
    snprintf(copy, sizeof copy, "%s", absolute);
    for (char *part = strtok(copy, "/"); part; part = strtok(NULL, "/")) {
        if (!strcmp(part, ".")) continue;
        size_t used = strlen(current);
        snprintf(current + used, sizeof current - used, "/%s", part);
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "symlink project root component refused: %s", current);
    }
    if (!feb_is_directory(absolute))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "project root must be an existing non-symlink directory");
    if (!realpath(absolute, resolved))
        return feb_fail(error, FEB_EXIT_OPERATION, "cannot resolve the project root");
    return true;
}

static bool command_capabilities(FusionJson *data) {
    FusionJson commands = fusion_json_object();
    for (size_t index = 0; index < COUNT(capabilities); ++index)
        if (!feb_put_string(&commands, capabilities[index][0], capabilities[index][1])) {
            fusion_json_free(&commands);
            return false;
        }
    return feb_put_string(data, "backend_version", FEB_BACKEND_VERSION) &&
           feb_put(data, "commands", commands);
}

static bool world_list(FusionJson *worlds) {
    *worlds = fusion_json_array();
    for (size_t index = 0; index < FEB_WORLD_COUNT; ++index)
        if (!fusion_json_push(worlds, fusion_json_string(feb_world_ids[index]))) return false;
    return true;
}

static bool command_project_info(const char *root, FusionJson *data) {
    FusionJson worlds;
    return feb_put_string(data, "name", "Metroid Vania") &&
           feb_put_string(data, "backend_version", FEB_BACKEND_VERSION) &&
           feb_put_string(data, "root", root) &&
           feb_put_string(data, "gameplay_status", "not_implemented") &&
           world_list(&worlds) && feb_put(data, "worlds", worlds);
}

static bool command_list_worlds(FusionJson *data) {
    FusionJson worlds = fusion_json_array();
    for (size_t index = 0; index < FEB_WORLD_COUNT; ++index) {
        size_t count = 0;
        feb_areas(feb_world_ids[index], &count);
        FusionJson world = fusion_json_object();
        if (!feb_put_string(&world, "id", feb_world_ids[index]) ||
            !feb_put_int(&world, "areas", (int64_t)count) ||
            !feb_put_string(&world, "engine_adapter", "unavailable") ||
            !fusion_json_push(&worlds, world)) {
            fusion_json_free(&worlds);
            return false;
        }
    }
    return feb_put(data, "worlds", worlds);
}

static bool command_list_areas(const FusionJson *options, FusionJson *data, FebError *error) {
    const char *world = NULL;
    if (!feb_world(feb_option(options, "world"), &world, error)) return false;
    size_t count = 0;
    const char *const *names = feb_areas(world, &count);
    FusionJson areas = fusion_json_array();
    for (size_t index = 0; index < count; ++index) {
        FusionJson area = fusion_json_object();
        if (!feb_put_int(&area, "index", (int64_t)index) ||
            !feb_put_string(&area, "name", names[index]) || !fusion_json_push(&areas, area)) {
            fusion_json_free(&areas);
            return feb_oom(error);
        }
    }
    if (!feb_put_string(data, "world", world) || !feb_put(data, "areas", areas))
        return feb_oom(error);
    return true;
}

static const char *source_option(const FusionJson *options, FebError *error) {
    const FusionJson *source = feb_option(options, "source");
    if (!source) return "native";
    if (source->kind == FUSION_JSON_STRING &&
        (!strcmp(source->string, "native") || !strcmp(source->string, "draft")))
        return source->string;
    feb_fail(error, FEB_EXIT_OPERATION, "source must be native or draft");
    return NULL;
}

static bool command_room_list(const char *root, const char *repository,
                              const FusionJson *options, FusionJson *data, FebError *error) {
    const char *source = source_option(options, error);
    if (!source) return false;
    const char *world = NULL;
    FusionJson rooms;
    if (!strcmp(source, "native")) {
        if (!feb_world(feb_option(options, "world"), &world, error) ||
            !feb_filter_native_rooms(repository, world, feb_option(options, "area"), &rooms,
                                     error))
            return false;
    } else {
        const FusionJson *world_option = feb_option(options, "world");
        if (world_option && world_option->kind != FUSION_JSON_NULL &&
            !feb_world(world_option, &world, error))
            return false;
        if (!feb_list_drafts(root, world, &rooms, error)) return false;
        const FusionJson *area_option = feb_option(options, "area");
        if (area_option && area_option->kind != FUSION_JSON_NULL) {
            int64_t area = 0;
            if (!feb_integer(area_option, "area", 0, 99, &area, error)) {
                fusion_json_free(&rooms);
                return false;
            }
            FusionJson filtered = fusion_json_array();
            for (size_t index = 0; index < rooms.count; ++index) {
                const FusionJson *descriptor = fusion_json_member(&rooms.items[index], "area");
                if (fusion_json_member(descriptor, "index")->integer != area) continue;
                if (!fusion_json_push(&filtered, rooms.items[index])) {
                    fusion_json_free(&filtered);
                    fusion_json_free(&rooms);
                    return feb_oom(error);
                }
                rooms.items[index] = fusion_json_null();
            }
            fusion_json_free(&rooms);
            rooms = filtered;
        }
    }
    int64_t count = (int64_t)rooms.count;
    if (!feb_put_string(data, "world", world) || !feb_put_string(data, "source", source) ||
        !feb_put_int(data, "count", count) || !feb_put(data, "rooms", rooms))
        return feb_oom(error);
    return true;
}

static bool command_room_inspect(const char *root, const char *repository,
                                 const FusionJson *options, FusionJson *data, FebError *error) {
    const FusionJson *source = feb_option(options, "source");
    FusionJson room;
    if (source && source->kind == FUSION_JSON_STRING && !strcmp(source->string, "draft")) {
        if (!feb_draft_by_id(root, feb_option(options, "room"), &room, error)) return false;
        if (!feb_put_string(data, "source", "draft") || !feb_put(data, "room", room))
            return feb_oom(error);
        return true;
    }
    const char *world = NULL;
    int64_t number = 0;
    FusionJson rooms;
    if (!feb_world(feb_option(options, "world"), &world, error) ||
        !feb_integer(feb_option(options, "room"), "room", 0, 999, &number, error) ||
        !feb_filter_native_rooms(repository, world, feb_option(options, "area"), &rooms, error))
        return false;
    size_t matches = 0, match = 0;
    for (size_t index = 0; index < rooms.count; ++index) {
        if (fusion_json_member(&rooms.items[index], "room")->integer == number) {
            ++matches;
            match = index;
        }
    }
    if (matches != 1) {
        fusion_json_free(&rooms);
        return feb_fail(error, FEB_EXIT_OPERATION, "native room identity is missing or ambiguous");
    }
    room = rooms.items[match];
    rooms.items[match] = fusion_json_null();
    fusion_json_free(&rooms);
    FusionJson source_copy;
    if (source) {
        if (!fusion_json_clone(source, &source_copy)) {
            fusion_json_free(&room);
            return feb_oom(error);
        }
    } else {
        source_copy = fusion_json_string("native");
    }
    if (!feb_put(data, "source", source_copy) || !feb_put(data, "room", room))
        return feb_oom(error);
    return true;
}

static bool command_room_validate(const char *root, const FusionJson *options,
                                  FusionJson *data, FebError *error) {
    FusionJson room;
    if (!feb_draft_by_id(root, feb_option(options, "room"), &room, error)) return false;
    bool ok = feb_put_string(data, "room", fusion_json_member(&room, "id")->string) &&
              feb_put_string(data, "status", "valid_draft_not_playable");
    fusion_json_free(&room);
    return ok || feb_oom(error);
}

static const FusionJson *draft_named(const FusionJson *rooms, const char *id) {
    const FusionJson *found = NULL;
    for (size_t index = 0; index < rooms->count; ++index)
        if (!strcmp(fusion_json_member(&rooms->items[index], "id")->string, id))
            found = &rooms->items[index];
    return found;
}

static bool command_placement_list(const char *root, const FusionJson *options,
                                   FusionJson *data, FebError *error) {
    const char *world = NULL;
    const FusionJson *world_option = feb_option(options, "world");
    if (world_option && world_option->kind != FUSION_JSON_NULL &&
        !feb_world(world_option, &world, error))
        return false;
    FusionJson entries, rooms;
    if (!feb_load_placements(root, &entries, &rooms, error)) return false;
    FusionJson placements = fusion_json_array();
    bool ok = true;
    for (size_t index = 0; ok && index < entries.count; ++index) {
        const FusionJson *entry = &entries.items[index];
        const FusionJson *room =
            draft_named(&rooms, fusion_json_member(entry, "id")->string);
        const char *room_world = fusion_json_member(room, "world")->string;
        if (world && strcmp(room_world, world)) continue;
        const FusionJson *geometry = fusion_json_member(room, "geometry");
        FusionJson record;
        ok = fusion_json_clone(entry, &record) &&
             feb_put_string(&record, "name", fusion_json_member(room, "name")->string) &&
             feb_put_string(&record, "world", room_world) &&
             feb_put_int(&record, "area",
                         fusion_json_member(fusion_json_member(room, "area"), "index")->integer) &&
             feb_put_int(&record, "width_screens",
                         fusion_json_member(geometry, "width_screens")->integer) &&
             feb_put_int(&record, "height_screens",
                         fusion_json_member(geometry, "height_screens")->integer) &&
             fusion_json_push(&placements, record);
    }
    fusion_json_free(&entries);
    fusion_json_free(&rooms);
    if (!ok) {
        fusion_json_free(&placements);
        return feb_oom(error);
    }
    int64_t count = (int64_t)placements.count;
    if (!feb_put_string(data, "world", world) || !feb_put_int(data, "count", count) ||
        !feb_put(data, "placements", placements))
        return feb_oom(error);
    return true;
}

static bool command_room_create(const char *root, const FusionJson *options, bool dry_run,
                                FusionJson *data, FebError *error) {
    const char *world = NULL;
    if (!feb_world(feb_option(options, "world"), &world, error)) return false;
    size_t count = 0;
    feb_areas(world, &count);
    FusionJson one = fusion_json_int(1);
    const FusionJson *width_option = feb_option(options, "width_screens");
    const FusionJson *height_option = feb_option(options, "height_screens");
    int64_t area = 0, width = 0, height = 0;
    FusionJson room;
    if (!feb_integer(feb_option(options, "area"), "area", 0, (int64_t)count - 1, &area, error) ||
        !feb_integer(width_option ? width_option : &one, "width_screens", 1, 8, &width, error) ||
        !feb_integer(height_option ? height_option : &one, "height_screens", 1, 8, &height,
                     error) ||
        !feb_new_draft(world, area, feb_option(options, "slug"), feb_option(options, "name"),
                       width, height, &room, error))
        return false;
    char path[FEB_PATH_CAPACITY];
    if (!dry_run && !feb_save_draft(root, &room, path, error)) {
        fusion_json_free(&room);
        return false;
    }
    if (!feb_put(data, "room", room) || !feb_put_bool(data, "persisted", !dry_run) ||
        (!dry_run && !feb_put_string(data, "path", path)))
        return feb_oom(error);
    return true;
}

static bool command_room_place(const char *root, const FusionJson *options, bool dry_run,
                               FusionJson *data, FebError *error) {
    const FusionJson *identity = feb_option(options, "room");
    if (!identity || identity->kind != FUSION_JSON_STRING || !identity->string[0])
        return feb_fail(error, FEB_EXIT_OPERATION, "room draft identity is required");
    int64_t x = 0, y = 0;
    if (!feb_integer(feb_option(options, "x"), "x", 0, 127, &x, error) ||
        !feb_integer(feb_option(options, "y"), "y", 0, 127, &y, error))
        return false;
    FusionJson placement, updated, rooms;
    if (!feb_plan_placement(root, identity->string, x, y, &placement, &updated, &rooms, error))
        return false;
    bool ok = true;
    char path[FEB_PATH_CAPACITY];
    if (!dry_run)
        ok = feb_write_placements(root, &updated, &rooms, error) &&
             (feb_placements_path(root, path) || feb_oom(error));
    fusion_json_free(&updated);
    fusion_json_free(&rooms);
    if (!ok) {
        fusion_json_free(&placement);
        return false;
    }
    if (!feb_put(data, "placement", placement) || !feb_put_bool(data, "persisted", !dry_run) ||
        (!dry_run && !feb_put_string(data, "path", path)))
        return feb_oom(error);
    return true;
}

static bool command_room_unplace(const char *root, const FusionJson *options, bool dry_run,
                                 FusionJson *data, FebError *error) {
    const FusionJson *identity = feb_option(options, "room");
    const FusionJson *confirm = feb_option(options, "confirm");
    if (!confirm || confirm->kind != FUSION_JSON_BOOL || !confirm->boolean)
        return feb_fail(error, FEB_EXIT_OPERATION, "room-unplace requires --confirm=true");
    FusionJson draft, entries, rooms;
    if (!feb_draft_by_id(root, identity, &draft, error)) return false;
    fusion_json_free(&draft);
    if (!feb_load_placements(root, &entries, &rooms, error)) return false;
    FusionJson kept = fusion_json_array();
    bool found = false, ok = true;
    for (size_t index = 0; ok && index < entries.count; ++index) {
        if (!strcmp(fusion_json_member(&entries.items[index], "id")->string, identity->string)) {
            found = true;
            continue;
        }
        ok = fusion_json_push(&kept, entries.items[index]) || feb_oom(error);
        entries.items[index] = fusion_json_null();
    }
    fusion_json_free(&entries);
    char path[FEB_PATH_CAPACITY];
    if (ok && !dry_run && found) ok = feb_write_placements(root, &kept, &rooms, error);
    fusion_json_free(&kept);
    fusion_json_free(&rooms);
    if (!ok) return false;
    if (!feb_put_string(data, "room", identity->string) ||
        !feb_put_bool(data, dry_run ? "would_remove" : "removed", found) ||
        !feb_put_bool(data, "persisted", !dry_run) ||
        (!dry_run && (!feb_placements_path(root, path) || !feb_put_string(data, "path", path))))
        return feb_oom(error);
    return true;
}

static bool option_copy(const FusionJson *options, const char *key, FusionJson *copy) {
    const FusionJson *value = feb_option(options, key);
    if (!value) {
        *copy = fusion_json_null();
        return true;
    }
    return fusion_json_clone(value, copy);
}

static bool command_list_assets(const char *repository, const FusionJson *options,
                                FusionJson *data, FebError *error) {
    const FusionJson *value = feb_option(options, "world");
    const char *world = NULL;
    if (value && value->kind != FUSION_JSON_NULL && !feb_world(value, &world, error)) return false;
    FusionJson records, copy;
    if (!feb_object_records(repository, world, &records, error)) return false;
    int64_t count = (int64_t)records.count;
    if (!option_copy(options, "world", &copy) || !feb_put(data, "world", copy) ||
        !feb_put_string(data, "kind", "native_object_metadata") ||
        !feb_put_int(data, "count", count) || !feb_put(data, "assets", records))
        return feb_oom(error);
    return true;
}

static bool command_entity_catalog(const char *repository, const FusionJson *options,
                                   FusionJson *data, FebError *error) {
    const char *world = NULL;
    if (!feb_world(feb_option(options, "world"), &world, error)) return false;
    const FusionJson *kind = feb_option(options, "kind");
    FusionJson records, copy;
    if (!feb_editor_catalog_options(repository, !strcmp(world, "zero_mission") ? "mzm" : "aria",
                                    kind && kind->kind == FUSION_JSON_STRING ? kind->string : NULL,
                                    &records, error))
        return false;
    int64_t count = (int64_t)records.count;
    if (!feb_put_string(data, "world", world) || !option_copy(options, "kind", &copy) ||
        !feb_put(data, "kind", copy) || !feb_put_int(data, "count", count) ||
        !feb_put(data, "definitions", records))
        return feb_oom(error);
    return true;
}

static bool command_room_render(const char *repository, const FusionJson *options, bool dry_run,
                                FusionJson *data, FebError *error) {
    const char *world = NULL;
    int64_t id = 0;
    if (!feb_world(feb_option(options, "world"), &world, error) ||
        !feb_integer(feb_option(options, "room"), "room", 0, 999, &id, error))
        return false;
    if (strcmp(world, "zero_mission"))
        return feb_fail(error, FEB_EXIT_UNAVAILABLE, "the Aria room renderer is not ported");
    FusionJson rooms, result;
    if (!feb_filter_native_rooms(repository, world, feb_option(options, "area"), &rooms, error))
        return false;
    const FusionJson *match = NULL;
    size_t matches = 0;
    for (size_t index = 0; index < rooms.count; ++index)
        if (fusion_json_member(&rooms.items[index], "room")->integer == id) {
            match = &rooms.items[index];
            ++matches;
        }
    bool ok = matches == 1 ||
              feb_fail(error, FEB_EXIT_OPERATION,
                       "Zero Mission room identity is missing or ambiguous");
    ok = ok && feb_mzm_decode_room(repository, fusion_json_member(match, "area")->string, id,
                                   !dry_run, &result, error);
    fusion_json_free(&rooms);
    if (!ok) return false;
    return (feb_put(data, "render", result) && feb_put_bool(data, "persisted", !dry_run)) ||
           feb_oom(error);
}

static bool command_room_open(const char *root, const char *repository, const FusionJson *options,
                              bool dry_run, FusionJson *data, FebError *error) {
    const char *world = NULL;
    int64_t id = 0;
    if (!feb_world(feb_option(options, "world"), &world, error) ||
        !feb_integer(feb_option(options, "room"), "room", 0, 999, &id, error))
        return false;
    FusionJson rooms, workspace;
    if (!feb_filter_native_rooms(repository, world, feb_option(options, "area"), &rooms, error))
        return false;
    const FusionJson *match = NULL;
    size_t matches = 0;
    for (size_t index = 0; index < rooms.count; ++index)
        if (fusion_json_member(&rooms.items[index], "room")->integer == id) {
            match = &rooms.items[index];
            ++matches;
        }
    bool ok = matches == 1 ||
              feb_fail(error, FEB_EXIT_OPERATION, "native room identity is missing or ambiguous");
    if (ok && dry_run) {
        ok = (feb_put_string(data, "room", fusion_json_member(match, "native_id")->string) &&
              feb_put_bool(data, "persisted", false) &&
              feb_put_string(data, "status", "validated_native_workspace_import")) ||
             feb_oom(error);
        fusion_json_free(&rooms);
        return ok;
    }
    ok = ok && feb_require_repository_root(root, repository, error);
    if (ok && strcmp(world, "zero_mission"))
        ok = feb_fail(error, FEB_EXIT_UNAVAILABLE, "the Aria workroom export is not ported");
    ok = ok && feb_mzm_workspace_export(repository, fusion_json_member(match, "area")->string, id,
                                        &workspace, error);
    if (ok) {
        FusionJson created = fusion_json_array();
        static const char *const keys[] = {"atlas", "base", "workroom", "override", "collision",
                                           "annotations"};
        for (size_t key = 0; ok && key < sizeof keys / sizeof *keys; ++key) {
            const FusionJson *value = fusion_json_member(&workspace, keys[key]);
            struct stat status;
            if (!value || value->kind != FUSION_JSON_STRING || !value->string[0] ||
                stat(value->string, &status))
                continue;
            ok = fusion_json_push(&created, fusion_json_string(value->string));
        }
        ok = (ok && feb_put_string(data, "room", fusion_json_member(match, "native_id")->string) &&
              feb_put(data, "workspace", workspace) && feb_put_bool(data, "persisted", true) &&
              feb_put(data, "created_paths", created)) || feb_oom(error);
    }
    fusion_json_free(&rooms);
    return ok;
}

bool feb_execute(const char *command, const FusionJson *options, const char *root_option,
                 const char *repository, bool dry_run, FusionJson *data, FebError *error) {
    *data = fusion_json_object();
    bool known = false;
    const char *const *fields = feb_command_fields(command, &known);
    if (!known) {
        const char *capability = feb_capability(command);
        if (capability) return feb_fail(error, FEB_EXIT_UNAVAILABLE, "%s", capability);
        return feb_fail(error, FEB_EXIT_OPERATION, "unknown command: %s", command);
    }
    char unknown[FEB_MESSAGE_CAPACITY] = "";
    if (!feb_unknown_options(options, fields, unknown, sizeof unknown))
        return feb_fail(error, FEB_EXIT_OPERATION, "unknown option(s) for %s: %s", command,
                        unknown);
    char root[FEB_PATH_CAPACITY];
    if (!validated_root(root_option, root, error)) return false;
    bool ok;
    if (!strcmp(command, "capabilities")) ok = command_capabilities(data) || feb_oom(error);
    else if (!strcmp(command, "project-info")) ok = command_project_info(root, data) || feb_oom(error);
    else if (!strcmp(command, "list-worlds")) ok = command_list_worlds(data) || feb_oom(error);
    else if (!strcmp(command, "list-areas")) ok = command_list_areas(options, data, error);
    else if (!strcmp(command, "list-rooms") || !strcmp(command, "room-list"))
        ok = command_room_list(root, repository, options, data, error);
    else if (!strcmp(command, "room-inspect"))
        ok = command_room_inspect(root, repository, options, data, error);
    else if (!strcmp(command, "room-validate"))
        ok = command_room_validate(root, options, data, error);
    else if (!strcmp(command, "placement-list"))
        ok = command_placement_list(root, options, data, error);
    else if (!strcmp(command, "room-create"))
        ok = command_room_create(root, options, dry_run, data, error);
    else if (!strcmp(command, "room-place") || !strcmp(command, "room-move"))
        ok = command_room_place(root, options, dry_run, data, error);
    else if (!strcmp(command, "room-unplace"))
        ok = command_room_unplace(root, options, dry_run, data, error);
    else if (!strcmp(command, "room-open"))
        ok = command_room_open(root, repository, options, dry_run, data, error);
    else if (!strcmp(command, "room-render"))
        ok = command_room_render(repository, options, dry_run, data, error);
    else if (!strcmp(command, "list-assets"))
        ok = command_list_assets(repository, options, data, error);
    else if (!strcmp(command, "entity-catalog"))
        ok = command_entity_catalog(repository, options, data, error);
    else if (feb_is_workspace_command(command))
        ok = feb_workspace_command(command, root, repository, options, dry_run, data, error);
    else if (feb_is_room_command(command))
        ok = feb_room_command(command, options, root, repository, dry_run, data, error);
    else ok = feb_fail(error, FEB_EXIT_UNAVAILABLE, "%s is not ported to the native backend",
                       command);
    if (!ok) fusion_json_free(data);
    return ok;
}
