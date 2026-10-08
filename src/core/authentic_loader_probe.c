/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/authentic_loader_probe.h"

#include "aria/state.h"
#include "core/transition.h"
#include "gba/runtime.h"
#include "gba/transition_target.h"
#include "mzm/state.h"

#include <stdio.h>
#include <string.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

static bool bootstrap_and_observe_mzm(
    GbaRuntime *runtime, GbaTransitionTarget *target,
    FusionTransitionObservation *observation,
    char *error, size_t error_size)
{
    MzmStateView state;
    if (!gba_runtime_enter(runtime)) {
        set_error(error, error_size, "cannot enter the MZM loader probe");
        return false;
    }
    if (!gba_transition_target_bootstrap(target) ||
        !mzm_state_read(runtime, &state, error, error_size) ||
        !fusion_transition_observe_mzm(&state, observation)) {
        if (target->error[0]) set_error(error, error_size, target->error);
        gba_runtime_leave(runtime);
        return false;
    }
    gba_runtime_leave(runtime);
    return true;
}

static bool bootstrap_and_observe_aria(
    GbaRuntime *runtime, GbaTransitionTarget *target,
    FusionTransitionObservation *observation,
    char *error, size_t error_size)
{
    AriaStateView state;
    if (!gba_runtime_enter(runtime)) {
        set_error(error, error_size, "cannot enter the Aria loader probe");
        return false;
    }
    if (!gba_transition_target_bootstrap(target) ||
        !aria_state_read(runtime, &state, error, error_size) ||
        !fusion_transition_observe_aria(&state, observation)) {
        if (target->error[0]) set_error(error, error_size, target->error);
        gba_runtime_leave(runtime);
        return false;
    }
    gba_runtime_leave(runtime);
    return true;
}

static bool probe_mzm_target(GbaRuntime *runtime, GbaTransitionTarget *target,
                             const FusionTransitionPlan *plan,
                             char *error, size_t error_size)
{
    GbaRuntimeSnapshot original = {0};
    MzmStateView before;
    MzmStateView loaded;
    MzmStateView restored;
    FusionTransitionTransactionResult result;
    char failure[256] = {0};
    char restore_error[256] = {0};
    bool success = false;

    if (!gba_runtime_enter(runtime)) {
        set_error(error, error_size, "cannot enter the MZM target probe");
        return false;
    }
    if (!mzm_state_read(runtime, &before, error, error_size) ||
        !gba_runtime_capture(runtime, &original, error, error_size))
        goto cleanup;
    result = fusion_transition_apply_transaction(
        plan, gba_transition_target_ops(), target);
    if (result != FUSION_TRANSITION_TRANSACTION_COMMITTED) {
        snprintf(error, error_size, "MZM loader transaction failed (%d): %s",
                 (int)result, target->error);
        goto cleanup;
    }
    if (!mzm_state_read(runtime, &loaded, error, error_size)) goto cleanup;
    printf("Authentic loader: MZM frames=%u mode=%u room=%u:%u door=%u "
           "position=%u,%u health=%u/%u\n",
           target->loader_frames, loaded.game_mode,
           loaded.area, loaded.room, loaded.last_door,
           loaded.x_subpixels, loaded.y_subpixels,
           loaded.current_energy, loaded.max_energy);
    success = true;

cleanup:
    if (!success && error && error_size && error[0])
        snprintf(failure, sizeof(failure), "%s", error);
    if (original.data) {
        if (!gba_runtime_restore(runtime, &original,
                                 restore_error, sizeof(restore_error)) ||
            !mzm_state_read(runtime, &restored,
                            restore_error, sizeof(restore_error)) ||
            restored.game_mode != before.game_mode ||
            restored.x_subpixels != before.x_subpixels ||
            restored.y_subpixels != before.y_subpixels ||
            restored.current_energy != before.current_energy ||
            restored.max_energy != before.max_energy) {
            set_error(error, error_size,
                      "MZM loader probe could not restore its source state");
            success = false;
        }
    }
    if (!success && failure[0]) set_error(error, error_size, failure);
    gba_runtime_snapshot_dispose(&original);
    gba_runtime_leave(runtime);
    if (success) puts("Authentic loader rollback: MZM verified");
    return success;
}

static bool probe_aria_target(GbaRuntime *runtime, GbaTransitionTarget *target,
                              const FusionTransitionPlan *plan,
                              char *error, size_t error_size)
{
    GbaRuntimeSnapshot original = {0};
    AriaStateView before;
    AriaStateView loaded;
    AriaStateView restored;
    FusionTransitionTransactionResult result;
    char failure[256] = {0};
    char restore_error[256] = {0};
    bool success = false;

    if (!gba_runtime_enter(runtime)) {
        set_error(error, error_size, "cannot enter the Aria target probe");
        return false;
    }
    if (!aria_state_read(runtime, &before, error, error_size) ||
        !gba_runtime_capture(runtime, &original, error, error_size))
        goto cleanup;
    result = fusion_transition_apply_transaction(
        plan, gba_transition_target_ops(), target);
    if (result != FUSION_TRANSITION_TRANSACTION_COMMITTED) {
        snprintf(error, error_size, "Aria loader transaction failed (%d): %s",
                 (int)result, target->error);
        goto cleanup;
    }
    if (!aria_state_read(runtime, &loaded, error, error_size)) goto cleanup;
    printf("Authentic loader: Aria frames=%u room=%u:%u "
           "position=%u,%u health=%d/%u control=%s\n",
           target->loader_frames, loaded.area, loaded.room,
           loaded.x_position_fixed / 65536,
           loaded.y_position_fixed / 65536,
           loaded.current_hp, loaded.max_hp,
           loaded.player_control_enabled ? "enabled" : "disabled");
    success = true;

cleanup:
    if (!success && error && error_size && error[0])
        snprintf(failure, sizeof(failure), "%s", error);
    if (original.data) {
        if (!gba_runtime_restore(runtime, &original,
                                 restore_error, sizeof(restore_error)) ||
            !aria_state_read(runtime, &restored,
                             restore_error, sizeof(restore_error)) ||
            restored.x_position_fixed != before.x_position_fixed ||
            restored.y_position_fixed != before.y_position_fixed ||
            restored.current_hp != before.current_hp ||
            restored.max_hp != before.max_hp) {
            set_error(error, error_size,
                      "Aria loader probe could not restore its source state");
            success = false;
        }
    }
    if (!success && failure[0]) set_error(error, error_size, failure);
    gba_runtime_snapshot_dispose(&original);
    gba_runtime_leave(runtime);
    if (success) puts("Authentic loader rollback: Aria verified");
    return success;
}

bool authentic_loader_probe_run(const char *metroid_path,
                                 const char *aria_path,
                                 char *error, size_t error_size)
{
    GbaRuntime runtimes[2] = {{0}};
    GbaTransitionTarget targets[2];
    FusionTransitionObservation observations[2];
    FusionTransitionPlan plans[2];
    bool success = false;

    if (!metroid_path || !aria_path) {
        set_error(error, error_size, "invalid authentic loader probe paths");
        return false;
    }
    if (!gba_runtime_open(&runtimes[WORLD_METROID], metroid_path,
                          error, error_size) ||
        !gba_runtime_open(&runtimes[WORLD_CASTLEVANIA], aria_path,
                          error, error_size))
        goto cleanup;
    gba_transition_target_init(&targets[WORLD_METROID],
                               &runtimes[WORLD_METROID], WORLD_METROID);
    gba_transition_target_init(&targets[WORLD_CASTLEVANIA],
                               &runtimes[WORLD_CASTLEVANIA], WORLD_CASTLEVANIA);
    if (!bootstrap_and_observe_mzm(
            &runtimes[WORLD_METROID], &targets[WORLD_METROID],
            &observations[WORLD_METROID], error, error_size) ||
        !bootstrap_and_observe_aria(
            &runtimes[WORLD_CASTLEVANIA], &targets[WORLD_CASTLEVANIA],
            &observations[WORLD_CASTLEVANIA], error, error_size) ||
        !fusion_transition_plan_build(&observations[WORLD_METROID],
                                      WORLD_CASTLEVANIA,
                                      &plans[WORLD_CASTLEVANIA]) ||
        !fusion_transition_plan_build(&observations[WORLD_CASTLEVANIA],
                                      WORLD_METROID,
                                      &plans[WORLD_METROID])) {
        if (!error || !error_size || !error[0])
            set_error(error, error_size,
                      "cannot build authentic loader probe plans");
        goto cleanup;
    }
    if (!probe_aria_target(&runtimes[WORLD_CASTLEVANIA],
                           &targets[WORLD_CASTLEVANIA],
                           &plans[WORLD_CASTLEVANIA], error, error_size) ||
        !probe_mzm_target(&runtimes[WORLD_METROID],
                          &targets[WORLD_METROID],
                          &plans[WORLD_METROID], error, error_size))
        goto cleanup;
    success = true;
    set_error(error, error_size, "");

cleanup:
    gba_runtime_close(&runtimes[WORLD_CASTLEVANIA]);
    gba_runtime_close(&runtimes[WORLD_METROID]);
    return success;
}
