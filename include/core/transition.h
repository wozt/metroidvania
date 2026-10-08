#ifndef FUSION_CORE_TRANSITION_H
#define FUSION_CORE_TRANSITION_H

#include "core/types.h"

#include <stdbool.h>
#include <stdint.h>

#define FUSION_TRANSITION_OBSERVATION_VERSION 1u

typedef struct MzmStateView MzmStateView;
typedef struct AriaStateView AriaStateView;

typedef enum {
    FUSION_TRANSITION_FIELD_LOCATION = 1u << 0,
    FUSION_TRANSITION_FIELD_POSITION = 1u << 1,
    FUSION_TRANSITION_FIELD_HEALTH = 1u << 2,
} FusionTransitionField;

typedef struct {
    uint32_t version;
    WorldKind source_world;
    CharacterKind source_character;
    uint32_t present_fields;
    uint32_t source_local_fields;
    uint32_t character_owned_fields;
    uint32_t shared_fields;
    uint8_t source_area;
    uint8_t source_room;
    uint32_t position_x_q16;
    uint32_t position_y_q16;
    int32_t health;
    int32_t max_health;
} FusionTransitionObservation;

bool fusion_transition_observe_mzm(const MzmStateView *view,
                                   FusionTransitionObservation *out);
bool fusion_transition_observe_aria(const AriaStateView *view,
                                    FusionTransitionObservation *out);
bool fusion_transition_observation_valid(
    const FusionTransitionObservation *observation);

#endif
