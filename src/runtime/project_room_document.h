/* SPDX-License-Identifier: GPL-3.0-only */
/* The map editor's project room documents (schema
 * metroidvania.project-room-data, versions 2 to 4), as the gameplay
 * runtimes read them: overrides/<world>/entities/<room>.json under the
 * extracted assets. A document is used only when it names the runtime's
 * world, area and room and has the room's dimensions. No SDL. */
#ifndef PROJECT_ROOM_DOCUMENT_H
#define PROJECT_ROOM_DOCUMENT_H

#include "core/json.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    PROJECT_DOCUMENT_NONE,      /* no document (or a version-1 entity list) */
    PROJECT_DOCUMENT_LOADED,
    PROJECT_DOCUMENT_REJECTED   /* unreadable or another room: `message` says why */
} ProjectDocumentResult;

/* Loads the document at `path` into *doc (to free with fusion_json_free when
 * LOADED). Its dimensions are not checked when width_px is negative. */
ProjectDocumentResult project_room_document_load(const char *path, const char *world,
                                                 const char *area, int room, int width_px,
                                                 int height_px, FusionJson *doc, char *message,
                                                 size_t message_size);
/* An integral number member. */
bool project_room_integer(const FusionJson *object, const char *key, int64_t *value);
/* A string member equal to `value`. */
bool project_room_string_is(const FusionJson *object, const char *key, const char *value);

#endif
