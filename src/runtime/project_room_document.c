/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's project room documents: see project_room_document.h. */
#include "project_room_document.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool project_room_integer(const FusionJson *object, const char *key, int64_t *value) {
    const FusionJson *member = object ? fusion_json_member(object, key) : NULL;
    if (!member || member->kind != FUSION_JSON_NUMBER || !member->integral) return false;
    *value = member->integer;
    return true;
}

bool project_room_string_is(const FusionJson *object, const char *key, const char *value) {
    const FusionJson *member = object ? fusion_json_member(object, key) : NULL;
    return member && member->kind == FUSION_JSON_STRING && !strcmp(member->string, value);
}

ProjectDocumentResult project_room_document_load(const char *path, const char *world,
                                                 const char *area, int room, int width_px,
                                                 int height_px, FusionJson *doc, char *message,
                                                 size_t message_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return PROJECT_DOCUMENT_NONE;
    enum { LIMIT = 4000000 };
    char *text = malloc(LIMIT + 1);
    size_t size = text ? fread(text, 1, LIMIT + 1, f) : 0;
    fclose(f);
    char error[128] = "";
    if (!text || size > LIMIT || !fusion_json_parse(text, size, doc, error, sizeof error)) {
        snprintf(message, message_size, "editor document %s ignored: %s", path,
                 !text ? "out of memory" : size > LIMIT ? "too large" : error);
        free(text);
        return PROJECT_DOCUMENT_REJECTED;
    }
    free(text);
    /* Version 1 documents (project-room-entities) predate the room data. */
    if (project_room_string_is(doc, "schema", "metroidvania.project-room-entities")) {
        fusion_json_free(doc);
        return PROJECT_DOCUMENT_NONE;
    }
    int64_t version = 0, doc_room = -1, width = 0, height = 0;
    const char *reason = NULL;
    if (!project_room_string_is(doc, "schema", "metroidvania.project-room-data") ||
        !project_room_integer(doc, "version", &version) || version < 2 || version > 4)
        reason = "not a project room document of versions 2 to 4";
    else if (!project_room_string_is(doc, "world", world) ||
             !project_room_string_is(doc, "area", area) ||
             !project_room_integer(doc, "room", &doc_room) || doc_room != room)
        reason = "it names another room";
    else if (width_px >= 0 &&
             (!project_room_integer(doc, "width_px", &width) || width != width_px ||
              !project_room_integer(doc, "height_px", &height) || height != height_px))
        reason = "its dimensions differ from the room's";
    if (reason) {
        snprintf(message, message_size, "editor document %s ignored: %s", path, reason);
        fusion_json_free(doc);
        return PROJECT_DOCUMENT_REJECTED;
    }
    return PROJECT_DOCUMENT_LOADED;
}
