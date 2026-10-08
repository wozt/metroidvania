#ifndef FUSION_GBA_TRANSITION_TARGET_H
#define FUSION_GBA_TRANSITION_TARGET_H

#include "core/transition.h"
#include "gba/runtime.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    GbaRuntime *runtime;
    WorldKind world;
    const GbaRuntimeSnapshot *active_checkpoint;
    unsigned loader_frames;
    char error[192];
} GbaTransitionTarget;

void gba_transition_target_init(GbaTransitionTarget *target,
                                GbaRuntime *runtime, WorldKind world);
bool gba_transition_target_bootstrap(GbaTransitionTarget *target);
const FusionTransitionTargetOps *gba_transition_target_ops(void);

#endif
