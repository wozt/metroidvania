#include "aria/state.h"
#include "core/transition.h"
#include "mzm/state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int value;
    bool preflight_succeeds;
    bool capture_succeeds;
    bool apply_succeeds;
    bool verify_succeeds;
    bool rollback_succeeds;
    unsigned preflight_calls;
    unsigned capture_calls;
    unsigned apply_calls;
    unsigned verify_calls;
    unsigned rollback_calls;
    unsigned dispose_calls;
} FakeTransitionTarget;

static FakeTransitionTarget fake_target(void)
{
    FakeTransitionTarget target = {
        .value = 17,
        .preflight_succeeds = true,
        .capture_succeeds = true,
        .apply_succeeds = true,
        .verify_succeeds = true,
        .rollback_succeeds = true,
    };
    return target;
}

static bool fake_preflight(void *context, const FusionTransitionPlan *plan)
{
    FakeTransitionTarget *target = context;
    ++target->preflight_calls;
    return target->preflight_succeeds && plan->target_world == WORLD_CASTLEVANIA;
}

static bool fake_capture(void *context, void **checkpoint)
{
    FakeTransitionTarget *target = context;
    int *saved;
    ++target->capture_calls;
    if (!target->capture_succeeds) return false;
    saved = malloc(sizeof(*saved));
    if (!saved) return false;
    *saved = target->value;
    *checkpoint = saved;
    return true;
}

static bool fake_apply(void *context, const FusionTransitionPlan *plan)
{
    FakeTransitionTarget *target = context;
    ++target->apply_calls;
    target->value = plan->target_health;
    return target->apply_succeeds;
}

static bool fake_verify(void *context, const FusionTransitionPlan *plan)
{
    FakeTransitionTarget *target = context;
    ++target->verify_calls;
    return target->verify_succeeds && target->value == plan->target_health;
}

static bool fake_rollback(void *context, void *checkpoint)
{
    FakeTransitionTarget *target = context;
    ++target->rollback_calls;
    if (!target->rollback_succeeds) return false;
    target->value = *(const int *)checkpoint;
    return true;
}

static void fake_dispose(void *context, void *checkpoint)
{
    FakeTransitionTarget *target = context;
    ++target->dispose_calls;
    free(checkpoint);
}

static const FusionTransitionTargetOps FAKE_TARGET_OPS = {
    .preflight = fake_preflight,
    .capture_checkpoint = fake_capture,
    .apply = fake_apply,
    .verify = fake_verify,
    .rollback = fake_rollback,
    .dispose_checkpoint = fake_dispose,
};

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
    FusionTransitionObservation dead;
    FusionTransitionPlan mzm_to_aria = {0};
    FusionTransitionPlan aria_to_mzm = {0};
    FusionTransitionPlan unchanged_plan;
    FusionTransitionTransactionResult result;
    FakeTransitionTarget target;

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
    assert(fusion_transition_plan_build(&observation, WORLD_CASTLEVANIA,
                                        &mzm_to_aria));
    assert(fusion_transition_plan_valid(&mzm_to_aria));
    assert(mzm_to_aria.target_world == WORLD_CASTLEVANIA);
    assert(mzm_to_aria.target_character == CHARACTER_SAMUS);
    assert(mzm_to_aria.target_anchor ==
           FUSION_TRANSITION_ANCHOR_ARIA_ENTRANCE_PROBE);
    assert(mzm_to_aria.target_area == 0 && mzm_to_aria.target_room == 0);
    assert(mzm_to_aria.target_position_x_q16 == UINT32_C(0x00a80000));
    assert(mzm_to_aria.target_position_y_q16 == UINT32_C(0x02bf0000));
    assert(mzm_to_aria.target_health == 299 &&
           mzm_to_aria.target_max_health == 399);
    assert(strcmp(fusion_transition_anchor_name(mzm_to_aria.target_anchor),
                  "Aria Entrance probe") == 0);

    assert(fusion_transition_observe_aria(&aria, &observation));
    assert(fusion_transition_observation_valid(&observation));
    assert(observation.source_world == WORLD_CASTLEVANIA);
    assert(observation.source_character == CHARACTER_SOMA);
    assert(observation.source_area == 11 && observation.source_room == 63);
    assert(observation.position_x_q16 == UINT32_C(0x00780000));
    assert(observation.position_y_q16 == UINT32_C(0x008f0000));
    assert(observation.health == 320 && observation.max_health == 420);
    assert(fusion_transition_plan_build(&observation, WORLD_METROID,
                                        &aria_to_mzm));
    assert(fusion_transition_plan_valid(&aria_to_mzm));
    assert(aria_to_mzm.target_character == CHARACTER_SOMA);
    assert(aria_to_mzm.target_anchor ==
           FUSION_TRANSITION_ANCHOR_MZM_BRINSTAR_PROBE);
    assert(aria_to_mzm.target_area == 0 && aria_to_mzm.target_room == 28);
    assert(aria_to_mzm.target_position_x_q16 == UINT32_C(0x00758000));
    assert(aria_to_mzm.target_position_y_q16 == UINT32_C(0x009fc000));
    assert(aria_to_mzm.target_health == 320 &&
           aria_to_mzm.target_max_health == 420);
    assert(strcmp(fusion_transition_anchor_name(aria_to_mzm.target_anchor),
                  "MZM Brinstar probe") == 0);
    assert(strcmp(fusion_transition_anchor_name(FUSION_TRANSITION_ANCHOR_NONE),
                  "Unknown transition anchor") == 0);

    unchanged_plan = mzm_to_aria;
    assert(!fusion_transition_plan_build(&mzm_to_aria.source, WORLD_METROID,
                                         &mzm_to_aria));
    assert(memcmp(&mzm_to_aria, &unchanged_plan, sizeof(mzm_to_aria)) == 0);
    mzm_to_aria.target_room = 1;
    assert(!fusion_transition_plan_valid(&mzm_to_aria));
    mzm_to_aria = unchanged_plan;
    mzm_to_aria.target_health--;
    assert(!fusion_transition_plan_valid(&mzm_to_aria));
    mzm_to_aria = unchanged_plan;

    dead = mzm_to_aria.source;
    dead.health = 0;
    assert(fusion_transition_observation_valid(&dead));
    assert(!fusion_transition_plan_build(&dead, WORLD_CASTLEVANIA,
                                         &mzm_to_aria));
    assert(memcmp(&mzm_to_aria, &unchanged_plan, sizeof(mzm_to_aria)) == 0);

    target = fake_target();
    result = fusion_transition_apply_transaction(&mzm_to_aria,
                                                 &FAKE_TARGET_OPS, &target);
    assert(result == FUSION_TRANSITION_TRANSACTION_COMMITTED);
    assert(target.value == mzm_to_aria.target_health);
    assert(target.preflight_calls == 1 && target.capture_calls == 1);
    assert(target.apply_calls == 1 && target.verify_calls == 1);
    assert(target.rollback_calls == 0 && target.dispose_calls == 1);

    target = fake_target();
    target.preflight_succeeds = false;
    result = fusion_transition_apply_transaction(&mzm_to_aria,
                                                 &FAKE_TARGET_OPS, &target);
    assert(result == FUSION_TRANSITION_TRANSACTION_REJECTED);
    assert(target.value == 17 && target.capture_calls == 0);

    target = fake_target();
    target.capture_succeeds = false;
    result = fusion_transition_apply_transaction(&mzm_to_aria,
                                                 &FAKE_TARGET_OPS, &target);
    assert(result == FUSION_TRANSITION_TRANSACTION_REJECTED);
    assert(target.value == 17 && target.apply_calls == 0);

    target = fake_target();
    target.apply_succeeds = false;
    result = fusion_transition_apply_transaction(&mzm_to_aria,
                                                 &FAKE_TARGET_OPS, &target);
    assert(result == FUSION_TRANSITION_TRANSACTION_ROLLED_BACK);
    assert(target.value == 17 && target.verify_calls == 0);
    assert(target.rollback_calls == 1 && target.dispose_calls == 1);

    target = fake_target();
    target.verify_succeeds = false;
    result = fusion_transition_apply_transaction(&mzm_to_aria,
                                                 &FAKE_TARGET_OPS, &target);
    assert(result == FUSION_TRANSITION_TRANSACTION_ROLLED_BACK);
    assert(target.value == 17 && target.verify_calls == 1);
    assert(target.rollback_calls == 1 && target.dispose_calls == 1);

    target = fake_target();
    target.verify_succeeds = false;
    target.rollback_succeeds = false;
    result = fusion_transition_apply_transaction(&mzm_to_aria,
                                                 &FAKE_TARGET_OPS, &target);
    assert(result == FUSION_TRANSITION_TRANSACTION_ROLLBACK_FAILED);
    assert(target.value == mzm_to_aria.target_health);
    assert(target.rollback_calls == 1 && target.dispose_calls == 1);

    mzm_to_aria.version++;
    target = fake_target();
    result = fusion_transition_apply_transaction(&mzm_to_aria,
                                                 &FAKE_TARGET_OPS, &target);
    assert(result == FUSION_TRANSITION_TRANSACTION_REJECTED);
    assert(target.preflight_calls == 0);
    assert(fusion_transition_apply_transaction(&unchanged_plan, NULL, &target) ==
           FUSION_TRANSITION_TRANSACTION_REJECTED);
    mzm_to_aria = unchanged_plan;

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

    puts("Transition contract tests passed.");
    return 0;
}
