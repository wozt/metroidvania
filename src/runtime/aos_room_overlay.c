/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's collision in Aria's runtime: see aos_room_overlay.h. */
#include "aos_room_overlay.h"

#include "project_room_document.h"

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
