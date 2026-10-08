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
