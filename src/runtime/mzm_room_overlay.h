/* SPDX-License-Identifier: GPL-3.0-only */
/* The map editor's private room overlays in the Zero Mission runtime. The
 * editor saves an edited room's BG1 and BG2 block layers as a complete
 * MVNATIVE document (core/native_map) under
 * assets/extracted/overrides/metroid/<area>_<NNN>.mvnative. A room loaded
 * from the native export takes those layers when the document names the
 * same room and tileset and its layers have the room's dimensions; any
 * other document is rejected and the native room stays as it is. Removing
 * the file restores the reconstructed original.
 *
 * The room's project document (overrides/metroid/entities/<area>_<NNN>.json,
 * schema metroidvania.project-room-data) carries the collision painted in
 * the editor, sparse 16-pixel cells of seven types. Each cell takes the
 * smallest native Clipdata value whose type and behavior are exactly the
 * type's native counterpart (the editor's collision-capabilities): solid
 * CLIPDATA_TYPE_SOLID, one_way PASS_THROUGH_BOTTOM, hazard an air block of
 * CLIP_BEHAVIOR_LAVA, slope_up RIGHT_STEEP_FLOOR_SLOPE (rising to the
 * right), slope_down LEFT_STEEP_FLOOR_SLOPE, water an air block of
 * CLIP_BEHAVIOR_WATER, air CLIPDATA_TYPE_AIR. Its entities referencing a
 * native sprite (PSPRITE_*) join the room's sprites. Doors and events stay
 * native. No SDL. */
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

/* The native Clipdata tables (sClipdataCollisionTypes and the
 * behaviors) of values 0 .. count - 1. */
typedef struct {
    const unsigned char *types;
    const unsigned short *behaviors;
    int count;
} MzmClipTable;

enum { MZM_PROJECT_COLLISION_TYPES = 7 };
extern const char *const mzm_project_collision_names[MZM_PROJECT_COLLISION_TYPES];

/* The Clipdata value and type for a project type (index into the names);
 * false when the tables have no exact native counterpart. */
bool mzm_project_collision_native(const MzmClipTable *table, int project_type,
                                  unsigned short *raw, unsigned char *clip_type);
/* <directory>/entities/<area in lower case>_<NNN>.json */
bool mzm_room_document_path(char *out, size_t size, const char *directory, const char *area,
                            int room);
/* Applies the document's collision cells through `set` (block x, y, its
 * Clipdata value and type), in the document's order. */
typedef void (*MzmCollisionSetter)(void *context, int x, int y, unsigned short raw,
                                   unsigned char clip_type);
MzmOverlayResult mzm_room_collision_apply(const char *path, const char *area, int room,
                                          int width_px, int height_px, const MzmClipTable *table,
                                          MzmCollisionSetter set, void *context, int *changed,
                                          char *message, size_t message_size);

/* A project entity whose native reference is a Zero Mission sprite: its
 * block and its sprite name (PSPRITE_ removed). */
typedef struct {
    int x, y;
    char name[64];
} MzmProjectSprite;

/* The document's entities that are sprites (references "unassigned" are
 * skipped): their count, or -1 when the document is ignored (another room,
 * other dimensions, an entity outside, a reference that is not a sprite,
 * more than `max`). 0 without a document. */
int mzm_room_project_sprites(const char *path, const char *area, int room, int width_px,
                             int height_px, MzmProjectSprite *sprites, int max, char *message,
                             size_t message_size);

#endif
