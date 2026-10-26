/* SPDX-License-Identifier: GPL-3.0-only */
/* Unified private room authoring data (formerly
 * scripts/project_room_entities.py and scripts/project_connection_graph.py).
 * Worlds are the internal "mzm" and "aria"; areas are a Zero Mission area
 * name or a decimal Aria area index. */
#ifndef FUSION_EDITOR_BACKEND_PROJECT_ROOM_H
#define FUSION_EDITOR_BACKEND_PROJECT_ROOM_H

#include "common.h"

enum {
    FEB_MAX_ENTITIES = 256,
    FEB_MAX_DOORS = 128,
    FEB_MAX_TRANSITIONS = 128,
    FEB_MAX_EVENTS = 256,
    FEB_MAX_EVENT_CONDITIONS = 16,
};

extern const char *const feb_mzm_areas[7];
extern const char *const feb_collision_types[7];
extern const char *const feb_condition_types[5];

typedef struct {
    const char *world;      /* mzm or aria */
    char area[32];
    int64_t room, width, height;
} FebRoomScope;

/* _scope(): the room identity and geometry. */
bool feb_room_scope(const FusionJson *world, const FusionJson *area, int64_t room,
                    int64_t width, int64_t height, FebError *error);
/* path_for(): the saved document of a room. */
bool feb_room_path(const char *root, const FebRoomScope *scope, char *path, FebError *error);
/* _check_path(): the components below the root are directories, not
 * symlinks, created on request. */
bool feb_room_check_path(const char *root, const char *path, bool create, FebError *error);
bool feb_room_migrate(FusionJson *document, FebError *error);
/* validate(): the strict v4 document check for a scope. */
bool feb_room_validate(const FusionJson *document, const FusionJson *world,
                       const FusionJson *area, const FusionJson *room,
                       const FusionJson *width, const FusionJson *height, FebError *error);
bool feb_room_validate_scope(const FusionJson *document, const FebRoomScope *scope,
                             FebError *error);
/* load(): the staged or saved document, or a new one. */
bool feb_room_load(const char *root, const FebRoomScope *scope, FusionJson *document,
                   FebError *error);
/* save(): the atomic write; ``path`` receives the written file. */
bool feb_room_save(const char *root, const FusionJson *document, char *path, FebError *error);
/* A saved document validated at the geometry it records; ``*document``
 * stays null when it is absent and not required. ``messages`` are the
 * unreadable-file, invalid-JSON, non-object and geometry failures. */
bool feb_room_saved(const char *root, const char *world, const char *area, int64_t room,
                    bool required, const char *const messages[4], FusionJson *document,
                    FebError *error);
bool feb_room_connections(const char *root, const char *world, const char *area,
                          const int64_t *room, FusionJson *connections, FebError *error);
bool feb_collision_capabilities(const char *world, FusionJson *result);

/* The in-memory operations; each validates the document before and after. */
bool feb_entity_move(FusionJson *document, int64_t id, int64_t x, int64_t y, FebError *error);
bool feb_entity_delete(FusionJson *document, int64_t id, FebError *error);
bool feb_collision_get(const FusionJson *document, int64_t x, int64_t y, const char **type,
                       FebError *error);
bool feb_collision_fill(FusionJson *document, int64_t x, int64_t y, int64_t width,
                        int64_t height, const FusionJson *type, int64_t *changed,
                        FebError *error);
bool feb_collision_stroke(FusionJson *document, const int64_t (*points)[2], size_t count,
                          const FusionJson *type, int64_t *changed, FebError *error);
bool feb_collision_clear(FusionJson *document, int64_t x, int64_t y, int64_t width,
                         int64_t height, int64_t *changed, FebError *error);
/* Appends ``item`` (moved) to ``list_key`` with the next ``counter_key`` id;
 * the stored copy is returned in ``created``. */
bool feb_room_append(FusionJson *document, const char *list_key, const char *counter_key,
                     size_t limit, const char *limit_message, FusionJson item,
                     FusionJson *created, FebError *error);
/* Replaces fields of the item with that id (``changes`` keys must be
 * editable); the updated copy is returned in ``changed``. */
bool feb_room_update(FusionJson *document, const char *list_key, int64_t id,
                     const FusionJson *changes, const char *const *editable,
                     const char *fields_message, const char *unknown_message,
                     FusionJson *changed, FebError *error);
bool feb_room_remove(FusionJson *document, const char *list_key, int64_t id,
                     const char *unknown_message, FebError *error);
bool feb_door_adopt(FusionJson *document, FusionJson source, int64_t x, int64_t y,
                    int64_t width, int64_t height, FusionJson *door, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_PROJECT_ROOM_H */
