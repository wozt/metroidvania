/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/backend.h"

#include <stdlib.h>

void fusion_backend_snapshot_dispose(FusionBackendSnapshot *snapshot)
{
    if (!snapshot) return;
    free(snapshot->data);
    snapshot->version = 0;
    snapshot->world = WORLD_METROID;
    snapshot->data = NULL;
    snapshot->size = 0;
}
