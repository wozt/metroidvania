#include "aria/state.h"
#include "core/transition.h"
#include "mzm/state.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    MzmStateView mzm = {
        .area = 2,
        .room = 7,
        .x_subpixels = 470,
        .y_subpixels = 639,
        .current_energy = 299,
        .max_energy = 399,
        .gameplay_active = true,
        .values_plausible = true,
        .gameplay_state_ready = true,
    };
    AriaStateView aria = {
        .area = 11,
        .room = 63,
        .x_position_fixed = UINT32_C(0x00780000),
        .y_position_fixed = UINT32_C(0x008f0000),
        .current_character = 0,
        .current_hp = 320,
        .max_hp = 420,
        .gameplay_active = true,
        .values_plausible = true,
        .gameplay_state_ready = true,
    };
    FusionTransitionObservation observation = {0};
    FusionTransitionObservation unchanged;

    assert(!fusion_transition_observe_mzm(NULL, &observation));
    assert(!fusion_transition_observe_mzm(&mzm, NULL));
    assert(fusion_transition_observe_mzm(&mzm, &observation));
    assert(fusion_transition_observation_valid(&observation));
    assert(observation.version == FUSION_TRANSITION_OBSERVATION_VERSION);
    assert(observation.source_world == WORLD_METROID);
    assert(observation.source_character == CHARACTER_SAMUS);
    assert(observation.source_area == 2 && observation.source_room == 7);
    assert(observation.position_x_q16 == UINT32_C(0x00758000));
    assert(observation.position_y_q16 == UINT32_C(0x009fc000));
    assert(observation.health == 299 && observation.max_health == 399);
    assert(observation.source_local_fields ==
           (FUSION_TRANSITION_FIELD_LOCATION |
            FUSION_TRANSITION_FIELD_POSITION));
    assert(observation.character_owned_fields ==
           FUSION_TRANSITION_FIELD_HEALTH);
    assert(observation.shared_fields == 0);

    assert(fusion_transition_observe_aria(&aria, &observation));
    assert(fusion_transition_observation_valid(&observation));
    assert(observation.source_world == WORLD_CASTLEVANIA);
    assert(observation.source_character == CHARACTER_SOMA);
    assert(observation.source_area == 11 && observation.source_room == 63);
    assert(observation.position_x_q16 == UINT32_C(0x00780000));
    assert(observation.position_y_q16 == UINT32_C(0x008f0000));
    assert(observation.health == 320 && observation.max_health == 420);

    unchanged = observation;
    aria.current_character = 1;
    assert(!fusion_transition_observe_aria(&aria, &observation));
    assert(observation.source_world == unchanged.source_world);
    assert(observation.source_character == unchanged.source_character);
    aria.current_character = 0;
    aria.gameplay_state_ready = false;
    assert(!fusion_transition_observe_aria(&aria, &observation));
    aria.gameplay_state_ready = true;
    aria.values_plausible = false;
    assert(!fusion_transition_observe_aria(&aria, &observation));
    mzm.gameplay_state_ready = false;
    assert(!fusion_transition_observe_mzm(&mzm, &observation));

    observation = unchanged;
    observation.shared_fields = FUSION_TRANSITION_FIELD_HEALTH;
    assert(!fusion_transition_observation_valid(&observation));
    observation = unchanged;
    observation.source_character = CHARACTER_SAMUS;
    assert(!fusion_transition_observation_valid(&observation));
    observation = unchanged;
    observation.health = observation.max_health + 1;
    assert(!fusion_transition_observation_valid(&observation));
    observation = unchanged;
    observation.version++;
    assert(!fusion_transition_observation_valid(&observation));

    puts("Transition observation tests passed.");
    return 0;
}
