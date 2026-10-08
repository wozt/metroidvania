/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/roundtrip_guard.h"

#include <string.h>

bool fusion_roundtrip_exclusive(const GbaRuntime *active,
                                const GbaRuntime *suspended)
{
    return active && suspended && active != suspended &&
           active->active && !suspended->active;
}

bool fusion_roundtrip_snapshot_equal(const GbaRuntimeSnapshot *a,
                                     const GbaRuntimeSnapshot *b)
{
    return a && b && a->data && b->data && a->size &&
           a->size == b->size && !memcmp(a->data, b->data, a->size);
}
