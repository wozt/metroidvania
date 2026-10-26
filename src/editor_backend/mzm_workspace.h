/* SPDX-License-Identifier: GPL-3.0-only */
/* The private editable Zero Mission workroom and its read-only previews. */
#ifndef FUSION_EDITOR_BACKEND_MZM_WORKSPACE_H
#define FUSION_EDITOR_BACKEND_MZM_WORKSPACE_H

#include "common.h"

/* One MV_ROOM_ANNOTATIONS_1 row. */
typedef struct {
    const char *kind;
    int64_t index, x, y, width, height;
    const char *variant, *native_type, *label, *details;
} FebAnnotation;

/* serialize(): the annotation TSV text. */
bool feb_annotations_serialize(const FebAnnotation *rows, size_t count, char **text,
                               size_t *size, FebError *error);
/* export(): the block maps, atlas, previews and annotations of one room;
 * ``info`` is the workspace description. */
bool feb_mzm_workspace_export(const char *repository, const char *area, int64_t number,
                              FusionJson *info, FebError *error);

#endif /* FUSION_EDITOR_BACKEND_MZM_WORKSPACE_H */
