#ifndef FUSION_GBA_TRANSITION_TARGET_H
#define FUSION_GBA_TRANSITION_TARGET_H

#include "core/transition.h"
#include "aria/state.h"
#include "gba/runtime.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
uint16_t gba_transition_aria_settle_input(unsigned frame, bool control_enabled);
bool gba_transition_aria_same_room_compatible(
    const AriaStateView *view, const FusionTransitionPlan *plan);
const FusionTransitionTargetOps *gba_transition_target_ops(void);

#endif
