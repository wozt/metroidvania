/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's room overlays: see mzm_room_overlay.h. */
#include "mzm_room_overlay.h"

#include "core/json.h"
#include "core/native_map.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool lower_area(char *out, size_t size, const char *area) {
    size_t length = strlen(area);
    if (!length || length >= size) return false;
    for (size_t k = 0; k < length; ++k) out[k] = (char)tolower((unsigned char)area[k]);
    out[length] = '\0';
    return true;
}

bool mzm_room_overlay_path(char *out, size_t size, const char *directory, const char *area,
                           int room) {
    char lower[48];
    if (!lower_area(lower, sizeof lower, area) || room < 0 || room > 999) return false;
    int written = snprintf(out, size, "%s/%s_%03d.mvnative", directory, lower, room);
    return written > 0 && (size_t)written < size;
}

MzmOverlayResult mzm_room_overlay_apply(const char *path, const char *area, int room, int tileset,
                                        MzmOverlayLayer layers[2], int *changed, char *message,
                                        size_t message_size) {
    if (changed) *changed = 0;
    if (message && message_size) message[0] = '\0';
    FILE *probe = fopen(path, "rb");
    if (!probe) return MZM_OVERLAY_NONE;
    fclose(probe);
    NativeMap *map = calloc(1, sizeof *map);
    char error[128] = "";
    if (!map || !native_map_load(map, path, error, sizeof error)) {
        snprintf(message, message_size, "editor overlay %s ignored: %s", path,
                 map ? error : "out of memory");
        free(map);
        return MZM_OVERLAY_REJECTED;
    }
    char lower[48], identity[64];
    snprintf(identity, sizeof identity, "mzm:%s:%03d",
             lower_area(lower, sizeof lower, area) ? lower : "?", room);
    const char *reason = NULL;
    if (strcmp(map->room_id, identity)) reason = "it names another room";
    else if ((int)map->tileset != tileset) reason = "its tileset differs from the room's";
    for (int l = 0; !reason && l < 2; ++l)
        if (layers[l].present && ((int)map->width[l] != layers[l].width ||
                                  (int)map->height[l] != layers[l].height))
            reason = l ? "its BG2 has other dimensions" : "its BG1 has other dimensions";
    if (reason) {
        snprintf(message, message_size, "editor overlay %s ignored: %s", path, reason);
        free(map);
        return MZM_OVERLAY_REJECTED;
    }
    int counts[2] = {0, 0};
    for (int l = 0; l < 2; ++l) {
        if (!layers[l].present) continue;
        int cells = layers[l].width * layers[l].height;
        for (int k = 0; k < cells; ++k) {
            if (layers[l].cells[k] != map->blocks[l][k]) ++counts[l];
            layers[l].cells[k] = map->blocks[l][k];
        }
    }
    if (changed) *changed = counts[0] + counts[1];
    snprintf(message, message_size, "editor overlay %s: %d BG1 and %d BG2 blocks changed", path,
             counts[0], counts[1]);
    free(map);
    return MZM_OVERLAY_APPLIED;
}

const char *const mzm_project_collision_names[MZM_PROJECT_COLLISION_TYPES] = {
    "solid", "one_way", "hazard", "slope_up", "slope_down", "water", "air"};

/* ClipdataType and ClipdataBehavior values of the pinned decompilation. */
enum {
    CLIPDATA_AIR = 0, CLIPDATA_SOLID = 1, CLIPDATA_LEFT_STEEP = 2, CLIPDATA_RIGHT_STEEP = 3,
    CLIPDATA_PASS_THROUGH_BOTTOM = 12, BEHAVIOR_NONE = 0, BEHAVIOR_WATER = 0x40,
    BEHAVIOR_LAVA = 0x41,
};

bool mzm_project_collision_native(const MzmClipTable *table, int project_type,
                                  unsigned short *raw, unsigned char *clip_type) {
    static const struct { unsigned char type; unsigned short behavior; } wanted[7] = {
        {CLIPDATA_SOLID, BEHAVIOR_NONE},       {CLIPDATA_PASS_THROUGH_BOTTOM, BEHAVIOR_NONE},
        {CLIPDATA_AIR, BEHAVIOR_LAVA},         {CLIPDATA_RIGHT_STEEP, BEHAVIOR_NONE},
        {CLIPDATA_LEFT_STEEP, BEHAVIOR_NONE},  {CLIPDATA_AIR, BEHAVIOR_WATER},
        {CLIPDATA_AIR, BEHAVIOR_NONE},
    };
    if (!table || project_type < 0 || project_type >= MZM_PROJECT_COLLISION_TYPES) return false;
    for (int value = 0; value < table->count; ++value)
        if (table->types[value] == wanted[project_type].type &&
            table->behaviors[value] == wanted[project_type].behavior) {
            *raw = (unsigned short)value;
            *clip_type = wanted[project_type].type;
            return true;
        }
    return false;
}

bool mzm_room_document_path(char *out, size_t size, const char *directory, const char *area,
                            int room) {
    char lower[48];
    if (!lower_area(lower, sizeof lower, area) || room < 0 || room > 999) return false;
    int written = snprintf(out, size, "%s/entities/%s_%03d.json", directory, lower, room);
    return written > 0 && (size_t)written < size;
}

static bool integer_member(const FusionJson *object, const char *key, int64_t *value) {
    const FusionJson *member = object ? fusion_json_member(object, key) : NULL;
    if (!member || member->kind != FUSION_JSON_NUMBER || !member->integral) return false;
    *value = member->integer;
    return true;
}

static bool string_member(const FusionJson *object, const char *key, const char *value) {
    const FusionJson *member = fusion_json_member(object, key);
    return member && member->kind == FUSION_JSON_STRING && !strcmp(member->string, value);
}

/* The room's project document: NONE without one (or for a version-1
 * document), REJECTED with *reason when it does not describe this room. */
static MzmOverlayResult load_document(const char *path, const char *area, int room, int width_px,
                                      int height_px, FusionJson *doc, char *message,
                                      size_t message_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return MZM_OVERLAY_NONE;
    enum { LIMIT = 4000000 };
    char *text = malloc(LIMIT + 1);
    size_t size = text ? fread(text, 1, LIMIT + 1, f) : 0;
    fclose(f);
    char error[128] = "";
    if (!text || size > LIMIT || !fusion_json_parse(text, size, doc, error, sizeof error)) {
        snprintf(message, message_size, "editor document %s ignored: %s", path,
                 !text ? "out of memory" : size > LIMIT ? "too large" : error);
        free(text);
        return MZM_OVERLAY_REJECTED;
    }
    free(text);
    /* Version 1 documents (project-room-entities) predate the room data. */
    if (string_member(doc, "schema", "metroidvania.project-room-entities")) {
        fusion_json_free(doc);
        return MZM_OVERLAY_NONE;
    }
    int64_t version = 0, doc_room = -1, width = 0, height = 0;
    const char *reason = NULL;
    if (!string_member(doc, "schema", "metroidvania.project-room-data") ||
        !integer_member(doc, "version", &version) || version < 2 || version > 4)
        reason = "not a project room document of versions 2 to 4";
    else if (!string_member(doc, "world", "mzm") || !string_member(doc, "area", area) ||
             !integer_member(doc, "room", &doc_room) || doc_room != room)
        reason = "it names another room";
    else if (!integer_member(doc, "width_px", &width) || width != width_px ||
             !integer_member(doc, "height_px", &height) || height != height_px)
        reason = "its dimensions differ from the room's";
    if (reason) {
        snprintf(message, message_size, "editor document %s ignored: %s", path, reason);
        fusion_json_free(doc);
        return MZM_OVERLAY_REJECTED;
    }
    return MZM_OVERLAY_APPLIED;
}

MzmOverlayResult mzm_room_collision_apply(const char *path, const char *area, int room,
                                          int width_px, int height_px, const MzmClipTable *table,
                                          MzmCollisionSetter set, void *context, int *changed,
                                          char *message, size_t message_size) {
    if (changed) *changed = 0;
    if (message && message_size) message[0] = '\0';
    FusionJson doc = {0};
    MzmOverlayResult loaded = load_document(path, area, room, width_px, height_px, &doc, message,
                                            message_size);
    if (loaded != MZM_OVERLAY_APPLIED) return loaded;
    int64_t resolution = 0;
    const FusionJson *collision = fusion_json_member(&doc, "collision");
    const FusionJson *cells = collision ? fusion_json_member(collision, "cells") : NULL;
    const char *reason = NULL;
    if (!integer_member(collision, "resolution_px", &resolution) || resolution != 16 ||
        !cells || cells->kind != FUSION_JSON_ARRAY)
        reason = "its collision is not a 16-pixel cell list";
    int columns = width_px / 16, rows = height_px / 16;
    for (size_t k = 0; !reason && k < cells->count; ++k) {
        const FusionJson *cell = &cells->items[k];
        int64_t x = -1, y = -1;
        const FusionJson *type = fusion_json_member(cell, "type");
        int index = -1;
        for (int t = 0; type && type->kind == FUSION_JSON_STRING && t < MZM_PROJECT_COLLISION_TYPES; ++t)
            if (!strcmp(type->string, mzm_project_collision_names[t])) index = t;
        unsigned short raw;
        unsigned char clip_type;
        if (!integer_member(cell, "x", &x) || !integer_member(cell, "y", &y) || x < 0 ||
            y < 0 || x >= columns || y >= rows)
            reason = "a cell lies outside the room";
        else if (index < 0)
            reason = "a cell has an unknown type";
        else if (!mzm_project_collision_native(table, index, &raw, &clip_type))
            reason = "the Clipdata tables have no value for one of its types";
    }
    if (reason) {
        snprintf(message, message_size, "editor collision %s ignored: %s", path, reason);
        fusion_json_free(&doc);
        return MZM_OVERLAY_REJECTED;
    }
    int counts[MZM_PROJECT_COLLISION_TYPES] = {0};
    for (size_t k = 0; k < cells->count; ++k) {
        const FusionJson *cell = &cells->items[k];
        int64_t x = 0, y = 0;
        integer_member(cell, "x", &x);
        integer_member(cell, "y", &y);
        const char *name = fusion_json_member(cell, "type")->string;
        int index = 0;
        for (int t = 0; t < MZM_PROJECT_COLLISION_TYPES; ++t)
            if (!strcmp(name, mzm_project_collision_names[t])) index = t;
        unsigned short raw;
        unsigned char clip_type;
        mzm_project_collision_native(table, index, &raw, &clip_type);
        if (set) set(context, (int)x, (int)y, raw, clip_type);
        ++counts[index];
    }
    if (changed) *changed = (int)cells->count;
    snprintf(message, message_size,
             "editor collision %s: %zu cells (%d solid, %d one-way, %d hazard, %d slope, "
             "%d water, %d air)",
             path, cells->count, counts[0], counts[1], counts[2], counts[3] + counts[4],
             counts[5], counts[6]);
    fusion_json_free(&doc);
    return MZM_OVERLAY_APPLIED;
}

int mzm_room_project_sprites(const char *path, const char *area, int room, int width_px,
                             int height_px, MzmProjectSprite *sprites, int max, char *message,
                             size_t message_size) {
    if (message && message_size) message[0] = '\0';
    FusionJson doc = {0};
    MzmOverlayResult loaded = load_document(path, area, room, width_px, height_px, &doc, message,
                                            message_size);
    if (loaded == MZM_OVERLAY_NONE) return 0;
    if (loaded == MZM_OVERLAY_REJECTED) return -1;
    const FusionJson *entities = fusion_json_member(&doc, "entities");
    const char *reason = NULL;
    int count = 0, unassigned = 0;
    if (!entities || entities->kind != FUSION_JSON_ARRAY) reason = "it has no entity list";
    for (size_t k = 0; !reason && entities && k < entities->count; ++k) {
        const FusionJson *entity = &entities->items[k];
        const FusionJson *type = fusion_json_member(entity, "native_type");
        int64_t x = -1, y = -1;
        if (!type || type->kind != FUSION_JSON_STRING || !integer_member(entity, "x", &x) ||
            !integer_member(entity, "y", &y)) {
            reason = "an entity lacks its position or native reference";
        } else if (!strcmp(type->string, "unassigned")) {
            ++unassigned;
        } else if (strncmp(type->string, "PSPRITE_", 8) || strlen(type->string + 8) >= 64 ||
                   !type->string[8]) {
            reason = "an entity's native reference is not a Zero Mission sprite";
        } else if (x < 0 || y < 0 || x >= width_px || y >= height_px) {
            reason = "an entity lies outside the room";
        } else if (count < max) {
            sprites[count].x = (int)(x / 16);
            sprites[count].y = (int)(y / 16);
            snprintf(sprites[count].name, sizeof sprites[count].name, "%s", type->string + 8);
            ++count;
        } else {
            reason = "it has more entities than the room's sprite slots";
        }
    }
    fusion_json_free(&doc);
    if (reason) {
        snprintf(message, message_size, "editor entities %s ignored: %s", path, reason);
        return -1;
    }
    snprintf(message, message_size, "editor entities %s: %d sprites (%d unassigned skipped)", path,
             count, unassigned);
    return count;
}
