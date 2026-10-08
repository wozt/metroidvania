#ifndef FUSION_CORE_TRANSITION_H
#define FUSION_CORE_TRANSITION_H

#include "core/types.h"

#include <stdbool.h>
#include <stdint.h>

#define FUSION_TRANSITION_OBSERVATION_VERSION 1u
#define FUSION_TRANSITION_PLAN_VERSION 2u

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

typedef enum {
    FUSION_TRANSITION_ANCHOR_NONE = 0,
    FUSION_TRANSITION_ANCHOR_MZM_BRINSTAR_DOOR_60 = 1,
    FUSION_TRANSITION_ANCHOR_ARIA_ENTRANCE_STAGED_ARRIVAL = 2,
} FusionTransitionAnchorId;

typedef enum {
    FUSION_TRANSITION_ARRIVAL_NONE = 0,
    FUSION_TRANSITION_ARRIVAL_MZM_DOOR,
    FUSION_TRANSITION_ARRIVAL_ARIA_STAGED_ROOM,
} FusionTransitionArrivalKind;

/*
 * MZM door arrivals use target_area and target_door. Aria staged-room arrivals
 * use target_room_pointer plus camera/player coordinates. Unused native fields
 * must remain zero; target_position is the derived common Q16.16 coordinate.
 */
typedef struct {
    uint32_t version;
    FusionTransitionObservation source;
    WorldKind target_world;
    CharacterKind target_character;
    uint32_t target_fields;
    FusionTransitionAnchorId target_anchor;
    FusionTransitionArrivalKind arrival_kind;
    uint8_t target_area;
    uint8_t target_room;
    uint8_t target_door;
    uint32_t target_room_pointer;
    uint16_t target_camera_x;
    uint16_t target_camera_y;
    uint16_t target_player_x;
    uint16_t target_player_y;
    uint32_t target_position_x_q16;
    uint32_t target_position_y_q16;
    int32_t target_health;
    int32_t target_max_health;
} FusionTransitionPlan;

typedef enum {
    FUSION_TRANSITION_TRANSACTION_REJECTED = 0,
    FUSION_TRANSITION_TRANSACTION_COMMITTED,
    FUSION_TRANSITION_TRANSACTION_ROLLED_BACK,
    FUSION_TRANSITION_TRANSACTION_ROLLBACK_FAILED,
} FusionTransitionTransactionResult;

/*
 * Preflight and verify must be side-effect free. Apply may partially mutate the
 * target when it fails, so every post-checkpoint failure triggers rollback.
 * Checkpoint ownership is always released through dispose_checkpoint.
 */
typedef struct {
    bool (*preflight)(void *context, const FusionTransitionPlan *plan);
    bool (*capture_checkpoint)(void *context, void **checkpoint);
    bool (*apply)(void *context, const FusionTransitionPlan *plan);
    bool (*verify)(void *context, const FusionTransitionPlan *plan);
    bool (*rollback)(void *context, void *checkpoint);
    void (*dispose_checkpoint)(void *context, void *checkpoint);
} FusionTransitionTargetOps;

bool fusion_transition_observe_mzm(const MzmStateView *view,
                                   FusionTransitionObservation *out);
bool fusion_transition_observe_aria(const AriaStateView *view,
                                    FusionTransitionObservation *out);
bool fusion_transition_mzm_arrival_evidence_valid(const MzmStateView *view);
bool fusion_transition_aria_arrival_evidence_valid(const AriaStateView *view);
bool fusion_transition_observation_valid(
    const FusionTransitionObservation *observation);
bool fusion_transition_plan_build(const FusionTransitionObservation *source,
                                  WorldKind target_world,
                                  FusionTransitionPlan *out);
bool fusion_transition_plan_valid(const FusionTransitionPlan *plan);
const char *fusion_transition_anchor_name(FusionTransitionAnchorId anchor);
FusionTransitionTransactionResult fusion_transition_apply_transaction(
    const FusionTransitionPlan *plan,
    const FusionTransitionTargetOps *ops,
    void *context);

#endif
