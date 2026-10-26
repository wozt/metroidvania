/* SPDX-License-Identifier: GPL-3.0-only */
/* Authored narrative sources (formerly scripts/validate_story_assets.py and
 * the story helpers of scripts/editor_backend.py). */
#ifndef FUSION_EDITOR_BACKEND_STORY_H
#define FUSION_EDITOR_BACKEND_STORY_H

#include "common.h"

/* A set of identifiers, kept as a JSON array of unique strings. */
bool feb_set_contains(const FusionJson *set, const char *value);

/* validate_timeline() / validate_scene(): the record count. */
bool feb_validate_timeline(const FusionJson *data, int64_t *count, FebError *error);
bool feb_validate_scene(const FusionJson *data, int64_t *count, FebError *error);
/* Parses a TOML file of at most ``limit`` bytes. */
bool feb_load_toml(const char *path, size_t limit, FusionJson *data, char **text,
                   FebError *error);
/* _story_reference_ids(): the validated timeline event or cutscene IDs. */
bool feb_story_ids(const char *root, const char *kind, FusionJson *ids, FebError *error);
/* _timeline_flag_ids(): every flag a timeline event sets. */
bool feb_timeline_flags(const char *root, FusionJson *flags, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_STORY_H */
