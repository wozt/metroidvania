/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_PIPELINE_INTERNAL_H
#define FUSION_PIPELINE_INTERNAL_H

#include "core/json.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    FusionJson document;
} FusionRebuildState;

bool fusion_rebuild_state_load(const char *path, FusionRebuildState *state,
                               char *error, size_t error_size);
bool fusion_rebuild_state_save(const char *path, const FusionRebuildState *state,
                               char *error, size_t error_size);
void fusion_rebuild_state_free(FusionRebuildState *state);

#endif /* FUSION_PIPELINE_INTERNAL_H */
