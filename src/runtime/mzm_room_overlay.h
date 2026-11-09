/* SPDX-License-Identifier: GPL-3.0-only */
/* The map editor's private room overlays in the Zero Mission runtime. The
 * editor saves an edited room's BG1 and BG2 block layers as a complete
 * MVNATIVE document (core/native_map) under
 * assets/extracted/overrides/metroid/<area>_<NNN>.mvnative. A room loaded
 * from the native export takes those layers when the document names the
 * same room and tileset and its layers have the room's dimensions; any
 * other document is rejected and the native room stays as it is. Removing
 * the file restores the reconstructed original. Only the block graphics are
 * applied: Clipdata, doors and objects stay native (the editor's project
 * collision and entities have no engine adapter yet). No SDL. */
#ifndef MZM_ROOM_OVERLAY_H
#define MZM_ROOM_OVERLAY_H

#include <stdbool.h>
#include <stddef.h>

#define MZM_ROOM_OVERLAY_DIRECTORY "assets/extracted/overrides/metroid"

typedef struct {
    bool present;            /* the room has this layer */
    int width, height;       /* in blocks */
    unsigned short *cells;   /* width x height block (sheet) indices */
} MzmOverlayLayer;

typedef enum {
    MZM_OVERLAY_NONE,        /* no document for this room */
    MZM_OVERLAY_APPLIED,
    MZM_OVERLAY_REJECTED     /* a document that does not fit: ignored */
} MzmOverlayResult;

/* <directory>/<area in lower case>_<NNN>.mvnative; false when too long. */
bool mzm_room_overlay_path(char *out, size_t size, const char *directory, const char *area,
                           int room);
/* Applies the document at `path` to the room's BG1 and BG2 (layers[0],
 * layers[1]); `message` tells what was applied or why it was rejected, and
 * *changed counts the blocks that differ from the native ones. */
MzmOverlayResult mzm_room_overlay_apply(const char *path, const char *area, int room, int tileset,
                                        MzmOverlayLayer layers[2], int *changed, char *message,
                                        size_t message_size);

#endif
