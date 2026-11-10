/* SPDX-License-Identifier: GPL-3.0-only */
/* The map editor's project collision, entities and transitions in the Aria
 * of Sorrow runtime. The
 * room's project document (overrides/aria/entities/area_<AA>_room_<NNN>.json
 * of the extracted assets, schema metroidvania.project-room-data) carries
 * sparse 8-pixel collision cells of seven types; each replaces the room's
 * native BG1 collision byte with the byte of its type, as the runtime's
 * collision walks (sub_08001A00 .. sub_080022E8) read them: solid 0x03
 * (floor and ceiling or wall bits), one_way 0x01 (floor bit only), slope_up
 * 0x45 (a one-pixel 45-degree floor slope, horizontally flipped, rising to
 * the right), slope_down 0x41 (falling to the right), air 0x00. The runtime
 * has no water or damage cell yet, so a document with water or hazard cells
 * is ignored, as is one of another room or other dimensions or with a cell
 * outside the room. Its entities with an Aria native reference join the
 * room's entity records (see AosProjectEntity), its transitions to Aria
 * rooms redirect screen-edge entries or add doors (see AosProjectDoor).
 * Removing the document
 * restores the native room. No SDL. */
#ifndef AOS_ROOM_OVERLAY_H
#define AOS_ROOM_OVERLAY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define AOS_ROOM_OVERLAY_DIRECTORY "assets/extracted/overrides/aria"

typedef enum {
    AOS_OVERLAY_NONE,      /* no document for this room */
    AOS_OVERLAY_APPLIED,
    AOS_OVERLAY_REJECTED   /* a document that does not fit: ignored */
} AosOverlayResult;

/* <directory>/entities/area_<AA>_room_<NNN>.json; false when too long. */
bool aos_room_document_path(char *out, size_t size, const char *directory, int area, int room);
/* The native collision byte of a project type (index into
 * mzm_project_collision_names' order: solid, one_way, hazard, slope_up,
 * slope_down, water, air); false for the types the runtime cannot play. */
bool aos_project_collision_byte(int project_type, uint8_t *byte);
/* Applies the document's collision cells to the width_cells x height_cells
 * byte layer; *changed counts the cells written, `message` tells what was
 * applied or why the document was ignored. */
AosOverlayResult aos_room_collision_apply(const char *path, int area, int room, int width_cells,
                                          int height_cells, uint8_t *cells, int *changed,
                                          char *message, size_t message_size);

/* A project entity with an Aria native reference, as a room entity (E
 * line): the reference "<family>:<HH>" gives the kind (enemy 1,
 * special-object 2, generic-candle 3, pickup 4, hard-mode-pickup 5,
 * all-souls-reward 6) and the id; the position is the entity's centre, 8
 * pixels right of and below the editor's 16-pixel box. A pickup's settings
 * give its record fields: param0 (var_a, the collection flag) parameter_0,
 * param1 (var_b, the item) item_id, and flags; parameter_1 is not used.
 * Other entities have no parameters. */
typedef struct {
    int kind, id, x, y, param0, param1, flags;
} AosProjectEntity;

/* The document's entities with a native reference ("unassigned" ones are
 * skipped): their count, -1 when the document is ignored (another room,
 * other dimensions, an entity outside the room, an unknown reference, more
 * than `max`), 0 without a document. */
int aos_room_project_entities(const char *path, int area, int room, int width_cells,
                              int height_cells, AosProjectEntity *entities, int max,
                              char *message, size_t message_size);

/* A project transition within Aria, from its source door (pixels). A door
 * adopted from a native screen-edge transition (native_source variant
 * transition-<edge>, native_type its T entry) redirects that entry:
 * native_index is the entry's index; any other door (native_index -1) is
 * a transition over its rectangle. */
typedef struct {
    int id, native_index;
    int x, y, width, height;
    int facing;              /* 0 left, 1 right, 2 up, 3 down */
    int target_area, target_room;
    /* Soma's feet in the target room: the transition's spawn point or,
     * without one (0, 0), beside the saved target door as in Zero Mission
     * (one block past its inner edge at its bottom, centred under an up
     * door three blocks down, or one block over a down door). */
    int spawn_x, spawn_y;
} AosProjectDoor;

/* The document's transitions to Aria rooms (those to Zero Mission are
 * skipped); `directory` holds the target rooms' documents. Their count, -1
 * when the document is ignored (another room, other dimensions, a door
 * outside the room or off the 8-pixel grid, a native entry index out of
 * `native_count`, a transition without an arrival, more than `max`), 0
 * without a document. */
int aos_room_project_doors(const char *path, const char *directory, int area, int room,
                           int width_cells, int height_cells, int native_count,
                           AosProjectDoor *doors, int max, char *message, size_t message_size);

#endif
