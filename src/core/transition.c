/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/transition.h"

#include "aria/state.h"
#include "mzm/state.h"

enum {
    TRANSITION_PRESENT_FIELDS = FUSION_TRANSITION_FIELD_LOCATION |
                                FUSION_TRANSITION_FIELD_POSITION |
                                FUSION_TRANSITION_FIELD_HEALTH,
    TRANSITION_SOURCE_LOCAL_FIELDS = FUSION_TRANSITION_FIELD_LOCATION |
                                     FUSION_TRANSITION_FIELD_POSITION,
    TRANSITION_CHARACTER_FIELDS = FUSION_TRANSITION_FIELD_HEALTH,
    TRANSITION_TARGET_FIELDS = FUSION_TRANSITION_FIELD_LOCATION |
                               FUSION_TRANSITION_FIELD_POSITION |
                               FUSION_TRANSITION_FIELD_HEALTH,
};

typedef struct {
    FusionTransitionAnchorId id;
    FusionTransitionArrivalKind arrival_kind;
    WorldKind world;
    uint8_t area;
    uint8_t room;
    uint8_t door;
    uint32_t room_pointer;
    uint16_t camera_x;
    uint16_t camera_y;
    uint16_t player_x;
    uint16_t player_y;
    uint32_t x_q16;
    uint32_t y_q16;
} TransitionAnchor;

static const TransitionAnchor MZM_BRINSTAR_DOOR_60_ANCHOR = {
    .id = FUSION_TRANSITION_ANCHOR_MZM_BRINSTAR_DOOR_60,
    .arrival_kind = FUSION_TRANSITION_ARRIVAL_MZM_DOOR,
    .world = WORLD_METROID,
    .area = 0,
    .room = 28,
    .door = 60,
    .x_q16 = UINT32_C(0x00480000),
    .y_q16 = UINT32_C(0x007fc000),
};

static const TransitionAnchor ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR = {
    .id = FUSION_TRANSITION_ANCHOR_ARIA_ENTRANCE_STAGED_ARRIVAL,
    .arrival_kind = FUSION_TRANSITION_ARRIVAL_ARIA_STAGED_ROOM,
    .world = WORLD_CASTLEVANIA,
    .area = 0,
    .room = 0,
    .room_pointer = UINT32_C(0x0850ef9c),
    .camera_x = 0x20,
    .camera_y = 0x200,
    .player_x = 0x78,
    .player_y = 0x8d,
    .x_q16 = UINT32_C(0x00980000),
    .y_q16 = UINT32_C(0x028d0000),
};

static FusionTransitionObservation observation_base(
    WorldKind world, CharacterKind character)
{
    FusionTransitionObservation observation = {
        .version = FUSION_TRANSITION_OBSERVATION_VERSION,
        .source_world = world,
        .source_character = character,
        .present_fields = TRANSITION_PRESENT_FIELDS,
        .source_local_fields = TRANSITION_SOURCE_LOCAL_FIELDS,
        .character_owned_fields = TRANSITION_CHARACTER_FIELDS,
        .shared_fields = 0,
    };
    return observation;
}

bool fusion_transition_observation_valid(
    const FusionTransitionObservation *observation)
{
    if (!observation ||
        observation->version != FUSION_TRANSITION_OBSERVATION_VERSION ||
        observation->present_fields != TRANSITION_PRESENT_FIELDS ||
        observation->source_local_fields != TRANSITION_SOURCE_LOCAL_FIELDS ||
        observation->character_owned_fields != TRANSITION_CHARACTER_FIELDS ||
        observation->shared_fields != 0 || observation->max_health <= 0 ||
        observation->health < 0 ||
        observation->health > observation->max_health ||
        (!observation->position_x_q16 && !observation->position_y_q16))
        return false;
    if (observation->source_world == WORLD_METROID)
        return observation->source_character == CHARACTER_SAMUS &&
               observation->source_area < 7;
    if (observation->source_world == WORLD_CASTLEVANIA)
        return observation->source_character == CHARACTER_SOMA &&
               observation->source_area < 16 && observation->source_room < 64;
    return false;
}

bool fusion_transition_observe_mzm(const MzmStateView *view,
                                   FusionTransitionObservation *out)
{
    FusionTransitionObservation observation;
    if (!view || !out || !view->gameplay_active || !view->values_plausible ||
        !view->gameplay_state_ready)
        return false;
    observation = observation_base(WORLD_METROID, CHARACTER_SAMUS);
    observation.source_area = view->area;
    observation.source_room = view->room;
    observation.position_x_q16 = (uint32_t)view->x_subpixels << 14;
    observation.position_y_q16 = (uint32_t)view->y_subpixels << 14;
    observation.health = view->current_energy;
    observation.max_health = view->max_energy;
    if (!fusion_transition_observation_valid(&observation)) return false;
    *out = observation;
    return true;
}

bool fusion_transition_observe_aria(const AriaStateView *view,
                                    FusionTransitionObservation *out)
{
    FusionTransitionObservation observation;
    if (!view || !out || !view->gameplay_active || !view->values_plausible ||
        !view->gameplay_state_ready ||
        view->current_character != 0)
        return false;
    observation = observation_base(WORLD_CASTLEVANIA, CHARACTER_SOMA);
    observation.source_area = view->area;
    observation.source_room = view->room;
    observation.position_x_q16 = view->x_position_fixed;
    observation.position_y_q16 = view->y_position_fixed;
    observation.health = view->current_hp;
    observation.max_health = view->max_hp;
    if (!fusion_transition_observation_valid(&observation)) return false;
    *out = observation;
    return true;
}

bool fusion_transition_mzm_arrival_evidence_valid(const MzmStateView *view)
{
    return view && view->gameplay_active && view->values_plausible &&
           view->gameplay_state_ready &&
           view->area == MZM_BRINSTAR_DOOR_60_ANCHOR.area &&
           view->room == MZM_BRINSTAR_DOOR_60_ANCHOR.room &&
           view->last_door == MZM_BRINSTAR_DOOR_60_ANCHOR.door;
}

bool fusion_transition_aria_arrival_evidence_valid(const AriaStateView *view)
{
    return view && view->gameplay_active && view->values_plausible &&
           view->gameplay_state_ready && view->staged_arrival_plausible &&
           view->area == ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR.area &&
           view->room == ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR.room &&
           view->staged_room_pointer ==
               ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR.room_pointer &&
           view->staged_camera_x ==
               ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR.camera_x &&
           view->staged_camera_y ==
               ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR.camera_y &&
           view->staged_player_x ==
               ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR.player_x &&
           view->staged_player_y ==
               ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR.player_y;
}

static const TransitionAnchor *anchor_for_world(WorldKind world)
{
    if (world == WORLD_METROID) return &MZM_BRINSTAR_DOOR_60_ANCHOR;
    if (world == WORLD_CASTLEVANIA)
        return &ARIA_ENTRANCE_STAGED_ARRIVAL_ANCHOR;
    return NULL;
}

const char *fusion_transition_anchor_name(FusionTransitionAnchorId anchor)
{
    if (anchor == FUSION_TRANSITION_ANCHOR_MZM_BRINSTAR_DOOR_60)
        return "MZM Brinstar door 60";
    if (anchor == FUSION_TRANSITION_ANCHOR_ARIA_ENTRANCE_STAGED_ARRIVAL)
        return "Aria Entrance staged arrival";
    return "Unknown transition anchor";
}

bool fusion_transition_plan_valid(const FusionTransitionPlan *plan)
{
    const TransitionAnchor *anchor;
    if (!plan || plan->version != FUSION_TRANSITION_PLAN_VERSION ||
        !fusion_transition_observation_valid(&plan->source) ||
        plan->target_world == plan->source.source_world ||
        plan->target_character != plan->source.source_character ||
        plan->target_fields != TRANSITION_TARGET_FIELDS ||
        plan->target_health != plan->source.health ||
        plan->target_max_health != plan->source.max_health ||
        plan->target_health <= 0)
        return false;
    anchor = anchor_for_world(plan->target_world);
    return anchor && plan->target_world == anchor->world &&
           plan->target_anchor == anchor->id &&
           plan->arrival_kind == anchor->arrival_kind &&
           plan->target_area == anchor->area &&
           plan->target_room == anchor->room &&
           plan->target_door == anchor->door &&
           plan->target_room_pointer == anchor->room_pointer &&
           plan->target_camera_x == anchor->camera_x &&
           plan->target_camera_y == anchor->camera_y &&
           plan->target_player_x == anchor->player_x &&
           plan->target_player_y == anchor->player_y &&
           plan->target_position_x_q16 == anchor->x_q16 &&
           plan->target_position_y_q16 == anchor->y_q16;
}

bool fusion_transition_plan_build(const FusionTransitionObservation *source,
                                  WorldKind target_world,
                                  FusionTransitionPlan *out)
{
    const TransitionAnchor *anchor;
    FusionTransitionPlan plan = {0};
    if (!source || !out || !fusion_transition_observation_valid(source) ||
        source->source_world == target_world)
        return false;
    anchor = anchor_for_world(target_world);
    if (!anchor) return false;
    plan.version = FUSION_TRANSITION_PLAN_VERSION;
    plan.source = *source;
    plan.target_world = target_world;
    plan.target_character = source->source_character;
    plan.target_fields = TRANSITION_TARGET_FIELDS;
    plan.target_anchor = anchor->id;
    plan.arrival_kind = anchor->arrival_kind;
    plan.target_area = anchor->area;
    plan.target_room = anchor->room;
    plan.target_door = anchor->door;
    plan.target_room_pointer = anchor->room_pointer;
    plan.target_camera_x = anchor->camera_x;
    plan.target_camera_y = anchor->camera_y;
    plan.target_player_x = anchor->player_x;
    plan.target_player_y = anchor->player_y;
    plan.target_position_x_q16 = anchor->x_q16;
    plan.target_position_y_q16 = anchor->y_q16;
    plan.target_health = source->health;
    plan.target_max_health = source->max_health;
    if (!fusion_transition_plan_valid(&plan)) return false;
    *out = plan;
    return true;
}

FusionTransitionTransactionResult fusion_transition_apply_transaction(
    const FusionTransitionPlan *plan,
    const FusionTransitionTargetOps *ops,
    void *context)
{
    void *checkpoint = NULL;
    bool captured;
    bool applied;
    bool verified;
    bool restored;

    if (!fusion_transition_plan_valid(plan) || !ops || !ops->preflight ||
        !ops->capture_checkpoint || !ops->apply || !ops->verify ||
        !ops->rollback || !ops->dispose_checkpoint ||
        !ops->preflight(context, plan))
        return FUSION_TRANSITION_TRANSACTION_REJECTED;

    captured = ops->capture_checkpoint(context, &checkpoint);
    if (!captured || !checkpoint) {
        if (checkpoint) ops->dispose_checkpoint(context, checkpoint);
        return FUSION_TRANSITION_TRANSACTION_REJECTED;
    }

    applied = ops->apply(context, plan);
    verified = applied && ops->verify(context, plan);
    if (verified) {
        ops->dispose_checkpoint(context, checkpoint);
        return FUSION_TRANSITION_TRANSACTION_COMMITTED;
    }

    restored = ops->rollback(context, checkpoint);
    ops->dispose_checkpoint(context, checkpoint);
    return restored ? FUSION_TRANSITION_TRANSACTION_ROLLED_BACK
                    : FUSION_TRANSITION_TRANSACTION_ROLLBACK_FAILED;
}
