/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's room overlays: see mzm_room_overlay.h. */
#include "mzm_room_overlay.h"

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
