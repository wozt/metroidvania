/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's collision in Aria's runtime: see aos_room_overlay.h. */
#include "aos_room_overlay.h"

#include "../../project_room_document.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const project_types[7] = {"solid", "one_way", "hazard", "slope_up",
                                             "slope_down", "water", "air"};

bool aos_room_document_path(char *out, size_t size, const char *directory, int area, int room) {
    if (area < 0 || area > 99 || room < 0 || room > 999) return false;
    int written = snprintf(out, size, "%s/entities/area_%02d_room_%03d.json", directory, area, room);
    return written > 0 && (size_t)written < size;
}

bool aos_project_collision_byte(int project_type, uint8_t *byte) {
    static const int16_t bytes[7] = {0x03, 0x01, -1, 0x45, 0x41, -1, 0x00};
    if (project_type < 0 || project_type >= 7 || bytes[project_type] < 0) return false;
    *byte = (uint8_t)bytes[project_type];
    return true;
}

static int type_index(const FusionJson *cell) {
    const FusionJson *type = fusion_json_member(cell, "type");
    for (int t = 0; type && type->kind == FUSION_JSON_STRING && t < 7; ++t)
        if (!strcmp(type->string, project_types[t])) return t;
    return -1;
}

AosOverlayResult aos_room_collision_apply(const char *path, int area, int room, int width_cells,
                                          int height_cells, uint8_t *cells, int *changed,
                                          char *message, size_t message_size) {
    if (changed) *changed = 0;
    if (message && message_size) message[0] = '\0';
    char area_text[8];
    snprintf(area_text, sizeof area_text, "%d", area);
    FusionJson doc = {0};
    ProjectDocumentResult loaded =
        project_room_document_load(path, "aria", area_text, room, width_cells * 8,
                                   height_cells * 8, &doc, message, message_size);
    if (loaded == PROJECT_DOCUMENT_NONE) return AOS_OVERLAY_NONE;
    if (loaded == PROJECT_DOCUMENT_REJECTED) return AOS_OVERLAY_REJECTED;
    int64_t resolution = 0;
    const FusionJson *collision = fusion_json_member(&doc, "collision");
    const FusionJson *list = collision ? fusion_json_member(collision, "cells") : NULL;
    const char *reason = NULL;
    if (!project_room_integer(collision, "resolution_px", &resolution) || resolution != 8 || !list ||
        list->kind != FUSION_JSON_ARRAY)
        reason = "its collision is not an 8-pixel cell list";
    for (size_t k = 0; !reason && k < list->count; ++k) {
        int64_t x = -1, y = -1;
        int index = type_index(&list->items[k]);
        uint8_t byte;
        if (!project_room_integer(&list->items[k], "x", &x) ||
            !project_room_integer(&list->items[k], "y", &y) || x < 0 || y < 0 ||
            x >= width_cells || y >= height_cells)
            reason = "a cell lies outside the room";
        else if (index < 0)
            reason = "a cell has an unknown type";
        else if (!aos_project_collision_byte(index, &byte))
            reason = "the runtime has no water or damage cells for its hazard or water cells";
    }
    if (reason) {
        snprintf(message, message_size, "editor collision %s ignored: %s", path, reason);
        fusion_json_free(&doc);
        return AOS_OVERLAY_REJECTED;
    }
    int counts[7] = {0};
    for (size_t k = 0; k < list->count; ++k) {
        int64_t x = 0, y = 0;
        project_room_integer(&list->items[k], "x", &x);
        project_room_integer(&list->items[k], "y", &y);
        int index = type_index(&list->items[k]);
        uint8_t byte = 0;
        aos_project_collision_byte(index, &byte);
        cells[y * width_cells + x] = byte;
        ++counts[index];
    }
    if (changed) *changed = (int)list->count;
    snprintf(message, message_size,
             "editor collision %s: %zu cells (%d solid, %d one-way, %d slope, %d air)", path,
             list->count, counts[0], counts[1], counts[3] + counts[4], counts[6]);
    fusion_json_free(&doc);
    return AOS_OVERLAY_APPLIED;
}

/* "<family>:<HH>": the native kind and id, false for any other token. */
static bool entity_reference(const char *token, int *kind, int *id) {
    static const char *const families[] = {"enemy", "special-object", "generic-candle", "pickup",
                                           "hard-mode-pickup", "all-souls-reward"};
    const char *colon = strrchr(token, ':');
    if (!colon || strlen(colon + 1) != 2) return false;
    char *end = NULL;
    long value = strtol(colon + 1, &end, 16);
    if (*end || value < 0 || value > 0xFF) return false;
    for (int f = 0; f < 6; ++f)
        if (strlen(families[f]) == (size_t)(colon - token) &&
            !strncmp(token, families[f], (size_t)(colon - token))) {
            *kind = f + 1;
            *id = (int)value;
            return true;
        }
    return false;
}

int aos_room_project_entities(const char *path, int area, int room, int width_cells,
                              int height_cells, AosProjectEntity *entities, int max,
                              char *message, size_t message_size) {
    if (message && message_size) message[0] = '\0';
    char area_text[8];
    snprintf(area_text, sizeof area_text, "%d", area);
    FusionJson doc = {0};
    ProjectDocumentResult loaded =
        project_room_document_load(path, "aria", area_text, room, width_cells * 8,
                                   height_cells * 8, &doc, message, message_size);
    if (loaded == PROJECT_DOCUMENT_NONE) return 0;
    if (loaded == PROJECT_DOCUMENT_REJECTED) return -1;
    const FusionJson *list = fusion_json_member(&doc, "entities");
    const char *reason = NULL;
    int count = 0, unassigned = 0;
    if (!list || list->kind != FUSION_JSON_ARRAY) reason = "it has no entity list";
    for (size_t k = 0; !reason && k < list->count; ++k) {
        const FusionJson *entity = &list->items[k];
        const FusionJson *type = fusion_json_member(entity, "native_type");
        int64_t x = -1, y = -1;
        int kind = 0, id = 0;
        if (!type || type->kind != FUSION_JSON_STRING || !project_room_integer(entity, "x", &x) ||
            !project_room_integer(entity, "y", &y)) {
            reason = "an entity lacks its position or native reference";
        } else if (!strcmp(type->string, "unassigned")) {
            ++unassigned;
        } else if (!entity_reference(type->string, &kind, &id)) {
            reason = "an entity's native reference is not an Aria entity";
        } else if (x < 0 || y < 0 || x >= width_cells * 8 || y >= height_cells * 8) {
            reason = "an entity lies outside the room";
        } else if (count >= max) {
            reason = "it has more entities than the room's entity slots";
        } else {
            AosProjectEntity *out = &entities[count++];
            *out = (AosProjectEntity){.kind = kind, .id = id, .x = (int)x + 8, .y = (int)y + 8};
            const FusionJson *settings = fusion_json_member(entity, "settings");
            int64_t item = 0, parameter = 0, flags = 0;
            if (settings && settings->kind == FUSION_JSON_OBJECT) {
                if (!project_room_integer(settings, "item_id", &item) ||
                    !project_room_integer(settings, "parameter_0", &parameter) ||
                    !project_room_integer(settings, "flags", &flags) || item < 0 ||
                    item > 0xFFFF || parameter < 0 || parameter > 0xFFFF || flags < 0 ||
                    flags > 0xFF) {
                    reason = "an item's settings are malformed";
                    break;
                }
                out->param0 = (int)parameter;
                out->param1 = (int)item;
                out->flags = (int)flags;
            }
        }
    }
    fusion_json_free(&doc);
    if (reason) {
        snprintf(message, message_size, "editor entities %s ignored: %s", path, reason);
        return -1;
    }
    snprintf(message, message_size, "editor entities %s: %d entities (%d unassigned skipped)",
             path, count, unassigned);
    return count;
}

static const char *text_member(const FusionJson *object, const char *key) {
    const FusionJson *member = object ? fusion_json_member(object, key) : NULL;
    return member && member->kind == FUSION_JSON_STRING ? member->string : "";
}

static const FusionJson *door_by_id(const FusionJson *doors, int64_t id) {
    for (size_t k = 0; doors && doors->kind == FUSION_JSON_ARRAY && k < doors->count; ++k) {
        int64_t value = 0;
        if (project_room_integer(&doors->items[k], "id", &value) && value == id)
            return &doors->items[k];
    }
    return NULL;
}

static bool door_rectangle(const FusionJson *door, int64_t r[4], int *facing) {
    static const char *const facings[] = {"left", "right", "up", "down"};
    if (!project_room_integer(door, "x", &r[0]) || !project_room_integer(door, "y", &r[1]) ||
        !project_room_integer(door, "width", &r[2]) ||
        !project_room_integer(door, "height", &r[3]) || r[2] <= 0 || r[3] <= 0)
        return false;
    *facing = -1;
    for (int k = 0; k < 4; ++k)
        if (!strcmp(text_member(door, "facing"), facings[k])) *facing = k;
    return *facing >= 0;
}

static void arrival_beside(const int64_t r[4], int facing, int *x, int *y) {
    switch (facing) {
    case 0: *x = (int)(r[0] + r[2] + 16); *y = (int)(r[1] + r[3]); break;
    case 1: *x = (int)(r[0] - 16); *y = (int)(r[1] + r[3]); break;
    case 2: *x = (int)(r[0] + r[2] / 2); *y = (int)(r[1] + r[3] + 48); break;
    default: *x = (int)(r[0] + r[2] / 2); *y = (int)(r[1] - 16); break;
    }
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
}

int aos_room_project_doors(const char *path, const char *directory, int area, int room,
                           int width_cells, int height_cells, int native_count,
                           AosProjectDoor *doors, int max, char *message, size_t message_size) {
    if (message && message_size) message[0] = '\0';
    char area_text[8];
    snprintf(area_text, sizeof area_text, "%d", area);
    FusionJson doc = {0};
    ProjectDocumentResult loaded =
        project_room_document_load(path, "aria", area_text, room, width_cells * 8,
                                   height_cells * 8, &doc, message, message_size);
    if (loaded == PROJECT_DOCUMENT_NONE) return 0;
    if (loaded == PROJECT_DOCUMENT_REJECTED) return -1;
    const FusionJson *list = fusion_json_member(&doc, "doors");
    const FusionJson *transitions = fusion_json_member(&doc, "transitions");
    const char *reason = NULL;
    int count = 0, other_world = 0;
    if (!list || list->kind != FUSION_JSON_ARRAY || !transitions ||
        transitions->kind != FUSION_JSON_ARRAY)
        reason = "it has no door or transition list";
    for (size_t k = 0; !reason && k < transitions->count; ++k) {
        const FusionJson *transition = &transitions->items[k];
        int64_t source_id = 0, target_room = -1, target_door = 0, spawn_x = 0, spawn_y = 0, r[4];
        const FusionJson *source = NULL;
        int facing;
        if (!project_room_integer(transition, "source_door_id", &source_id) ||
            !(source = door_by_id(list, source_id)) || !door_rectangle(source, r, &facing)) {
            reason = "a transition's source door is missing or malformed";
            break;
        }
        if (strcmp(text_member(transition, "target_world"), "aria")) {
            ++other_world;
            continue;
        }
        const char *target_area_text = text_member(transition, "target_area");
        char *end = NULL;
        long target_area = strtol(target_area_text, &end, 10);
        if (!target_area_text[0] || *end || target_area < 0 || target_area > 99 ||
            !project_room_integer(transition, "target_room", &target_room) || target_room < 0 ||
            target_room > 999 || !project_room_integer(transition, "target_door_id", &target_door) ||
            !project_room_integer(transition, "spawn_x", &spawn_x) ||
            !project_room_integer(transition, "spawn_y", &spawn_y)) {
            reason = "a transition's target is malformed";
            break;
        }
        if (r[0] < 0 || r[1] < 0 || r[0] + r[2] > width_cells * 8 ||
            r[1] + r[3] > height_cells * 8 || r[0] % 8 || r[1] % 8 || r[2] % 8 || r[3] % 8) {
            reason = "a door is not an 8-pixel rectangle inside the room";
            break;
        }
        if (count >= max) {
            reason = "it has more transitions than the room's door slots";
            break;
        }
        AosProjectDoor *door = &doors[count];
        *door = (AosProjectDoor){.id = (int)source_id, .native_index = -1, .x = (int)r[0],
                                 .y = (int)r[1], .width = (int)r[2], .height = (int)r[3],
                                 .facing = facing, .target_area = (int)target_area,
                                 .target_room = (int)target_room, .spawn_x = (int)spawn_x,
                                 .spawn_y = (int)spawn_y};
        const FusionJson *native = fusion_json_member(source, "native_source");
        if (native && !strncmp(text_member(native, "variant"), "transition-", 11)) {
            long index = strtol(text_member(native, "native_type"), &end, 10);
            if (!text_member(native, "native_type")[0] || *end || index < 0 ||
                index >= native_count) {
                reason = "an adopted transition is not one of the room's entries";
                break;
            }
            door->native_index = (int)index;
        }
        if (!spawn_x && !spawn_y) {
            char target_path[1024], ignored[256], target_text[8];
            FusionJson target = {0};
            const FusionJson *arrival = NULL;
            int64_t tr[4];
            int target_facing;
            snprintf(target_text, sizeof target_text, "%ld", target_area);
            if (target_door && directory &&
                aos_room_document_path(target_path, sizeof target_path, directory,
                                       (int)target_area, (int)target_room) &&
                project_room_document_load(target_path, "aria", target_text, (int)target_room,
                                           -1, -1, &target, ignored, sizeof ignored) ==
                    PROJECT_DOCUMENT_LOADED) {
                arrival = door_by_id(fusion_json_member(&target, "doors"), target_door);
                if (arrival && door_rectangle(arrival, tr, &target_facing))
                    arrival_beside(tr, target_facing, &door->spawn_x, &door->spawn_y);
                else
                    arrival = NULL;
                fusion_json_free(&target);
            }
            if (!arrival) {
                reason = "a transition has neither a spawn point nor a saved target door";
                break;
            }
        }
        ++count;
    }
    fusion_json_free(&doc);
    if (reason) {
        snprintf(message, message_size, "editor doors %s ignored: %s", path, reason);
        return -1;
    }
    snprintf(message, message_size,
             "editor doors %s: %d transitions (%d to another world skipped)", path, count,
             other_world);
    return count;
}
