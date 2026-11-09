/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's room overlays in the Zero Mission runtime: applied when the
 * document fits the room, ignored otherwise, absent without a document. */
#include "mzm_room_overlay.h"

#include "core/native_map.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static NativeMap map;

static void write_map(const char *path, const char *room, unsigned tileset, unsigned w, unsigned h) {
    memset(&map, 0, sizeof map);
    snprintf(map.room_id, sizeof map.room_id, "%s", room);
    snprintf(map.atlas, sizeof map.atlas, "rooms/metroid/tilesets/17_atlas.bmp");
    map.tileset = tileset;
    map.tile_count = 528;
    for (unsigned l = 0; l < 2; ++l) {
        map.width[l] = w;
        map.height[l] = h;
        for (unsigned k = 0; k < w * h; ++k) map.blocks[l][k] = (uint16_t)(l ? 7 : k);
    }
    char error[128];
    assert(native_map_save(&map, path, error, sizeof error));
}

int main(void) {
    char path[256], message[512];
    assert(mzm_room_overlay_path(path, sizeof path, ".", "Brinstar", 7) &&
           !strcmp(path, "./brinstar_007.mvnative"));
    snprintf(path, sizeof path, "test-mzm-room-overlay.mvnative");
    remove(path);
    unsigned short bg1[4 * 3], bg2[4 * 3];
    for (int k = 0; k < 12; ++k) bg1[k] = bg2[k] = 7;
    MzmOverlayLayer layers[2] = {{true, 4, 3, bg1}, {true, 4, 3, bg2}};
    int changed = -1;
    /* No document: the native room as it is. */
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_NONE && changed == 0);
    /* The room's own document: its blocks replace the native ones. */
    write_map(path, "mzm:brinstar:007", 17, 4, 3);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_APPLIED);
    assert(changed == 11 && bg1[0] == 0 && bg1[11] == 11 && bg2[5] == 7 &&
           strstr(message, "11 BG1 and 0 BG2"));
    /* Another room, another tileset, other dimensions: ignored, untouched. */
    for (int k = 0; k < 12; ++k) bg1[k] = 9;
    write_map(path, "mzm:brinstar:008", 17, 4, 3);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_REJECTED && bg1[0] == 9 &&
           strstr(message, "another room"));
    write_map(path, "mzm:brinstar:007", 18, 4, 3);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_REJECTED && bg1[0] == 9);
    write_map(path, "mzm:brinstar:007", 17, 5, 3);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_REJECTED && bg1[0] == 9 &&
           strstr(message, "dimensions"));
    /* A layer the room does not have is left out. */
    write_map(path, "mzm:brinstar:007", 17, 4, 3);
    layers[1].present = false;
    for (int k = 0; k < 12; ++k) bg2[k] = 1;
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_APPLIED && bg2[0] == 1);
    /* A damaged document is ignored. */
    FILE *f = fopen(path, "w");
    fputs("MVNATIVE 1\nROOM mzm:brinstar:007\n", f);
    fclose(f);
    assert(mzm_room_overlay_apply(path, "Brinstar", 7, 17, layers, &changed, message,
                                  sizeof message) == MZM_OVERLAY_REJECTED);
    remove(path);
    puts("mzm_room_overlay: ok");
    return 0;
}
