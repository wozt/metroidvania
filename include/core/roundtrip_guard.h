#ifndef FUSION_ROUNDTRIP_GUARD_H
#define FUSION_ROUNDTRIP_GUARD_H

#include "gba/runtime.h"

#include <stdbool.h>

/* Exactly one runtime may be running during a diagnostic handoff. */
bool fusion_roundtrip_exclusive(const GbaRuntime *active,
                                const GbaRuntime *suspended);
/* Full saved-state equality is stronger than matching only coordinates. */
bool fusion_roundtrip_snapshot_equal(const GbaRuntimeSnapshot *a,
                                     const GbaRuntimeSnapshot *b);

#endif
