#ifndef FUSION_ROUNDTRIP_GUARD_H
#define FUSION_ROUNDTRIP_GUARD_H

#include "gba/runtime.h"
#include "aria/state.h"

#include <stdbool.h>

/* Exactly one runtime may be running during a diagnostic handoff. */
bool fusion_roundtrip_exclusive(const GbaRuntime *active,
                                const GbaRuntime *suspended);
/* Full saved-state equality is stronger than matching only coordinates. */
bool fusion_roundtrip_snapshot_equal(const GbaRuntimeSnapshot *a,
                                     const GbaRuntimeSnapshot *b);

/* Decoded state equality is checked separately from serialized bytes:
 * mGBA state serialization may canonicalize some internal fields on load. */
bool fusion_roundtrip_aria_view_equal(const AriaStateView *a,
                                      const AriaStateView *b);
/* Deterministic full-frame hashes reject an incorrectly restored visual state. */
bool fusion_roundtrip_frame_hash(const GbaFrameView *frame, uint64_t *out);


/* Select first-entry transactional arrival or preservation of a paused world. */
typedef enum {
    FUSION_ROUNDTRIP_SWITCH_REJECTED = 0,
    FUSION_ROUNDTRIP_SWITCH_ARRIVAL,
    FUSION_ROUNDTRIP_SWITCH_RESUME,
} FusionRoundtripSwitchAction;
FusionRoundtripSwitchAction fusion_roundtrip_switch_action(
    const GbaRuntime *source, const GbaRuntime *destination,
    bool destination_visited);

#endif
