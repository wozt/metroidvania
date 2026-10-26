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

#endif /* FUSION_EDITOR_BACKEND_DRAFTS_H */
