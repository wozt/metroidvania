/* SPDX-License-Identifier: GPL-3.0-only */
/* Project-authored room drafts and their private map placements (formerly
 * scripts/authored_rooms.py and scripts/map_placements.py). */
#ifndef FUSION_EDITOR_BACKEND_DRAFTS_H
#define FUSION_EDITOR_BACKEND_DRAFTS_H

#include "common.h"

/* validate_room(): the strict v1 draft check. */
bool feb_validate_draft(const FusionJson *room, FebError *error);
/* list_rooms(): every draft below assets/extracted/authored_rooms, in path
 * order, optionally of one world, as a JSON array. */
bool feb_list_drafts(const char *root, const char *world, FusionJson *rooms,
                     FebError *error);
/* _draft_by_id(): the draft with that identity, copied into ``room``. */
bool feb_draft_by_id(const char *root, const FusionJson *identity, FusionJson *room,
                     FebError *error);
/* map_placements.load(): the validated placements and the drafts. */
bool feb_load_placements(const char *root, FusionJson *placements, FusionJson *rooms,
                         FebError *error);

/* new_room(): a validated, unplayable draft. */
bool feb_new_draft(const char *world, int64_t area, const FusionJson *slug,
                   const FusionJson *name, int64_t width, int64_t height, FusionJson *room,
                   FebError *error);
/* save_room(): create-only persistence; ``path`` receives the file. */
bool feb_save_draft(const char *root, const FusionJson *room, char *path, FebError *error);
/* map_placements.plan_place(): the placement and the updated list. */
bool feb_plan_placement(const char *root, const char *id, int64_t x, int64_t y,
                        FusionJson *placement, FusionJson *updated, FusionJson *rooms,
                        FebError *error);
/* map_placements._write(): the sorted placements, atomically. */
bool feb_write_placements(const char *root, FusionJson *entries, const FusionJson *rooms,
                          FebError *error);
bool feb_placements_path(const char *root, char *path);

#endif /* FUSION_EDITOR_BACKEND_DRAFTS_H */
