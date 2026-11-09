/* SPDX-License-Identifier: GPL-3.0-only */
/* The map editor's project collision in the Aria of Sorrow runtime. The
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
 * outside the room. Removing the document restores the native room. No SDL. */
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

#endif
