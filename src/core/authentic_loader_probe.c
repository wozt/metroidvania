/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/authentic_loader_probe.h"

#include "aria/state.h"
#include "core/transition.h"
#include "core/roundtrip_guard.h"
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


/* Snapshot bytes after a loadState can differ even when the restored game
 * behaves identically. Record a full rendered frame on each of several
 * identically-input replay frames; compare before and after the handoff. */
enum { ARIA_ROUNDTRIP_REPLAY_FRAMES = 16 };

static bool sample_aria_replay(GbaRuntime *runtime,
                               uint64_t hashes[ARIA_ROUNDTRIP_REPLAY_FRAMES],
                               char *error, size_t error_size)
{
    unsigned frame;
    for (frame = 0; frame < ARIA_ROUNDTRIP_REPLAY_FRAMES; ++frame) {
        GbaFrameView view;
        if (!gba_runtime_step(runtime, 0) ||
            !gba_runtime_frame(runtime, &view) ||
            !fusion_roundtrip_frame_hash(&view, &hashes[frame])) {
            set_error(error, error_size, "roundtrip: Aria replay frame unavailable");
            return false;
        }
    }
    return true;
}

/* ROM-backed, opt-in lifecycle proof, intentionally distinct from a playable
 * guest-character transfer. The Aria instance still contains its native Soma
 * entity; the imported health and location are diagnostic only. */
bool authentic_roundtrip_probe_run(const char *metroid_path,
                                   const char *aria_path,
                                   char *error, size_t error_size)
{
    GbaRuntime runtimes[2] = {{0}};
    GbaTransitionTarget targets[2];
    GbaRuntimeSnapshot source_checkpoint = {0};
    GbaRuntimeSnapshot target_checkpoint = {0};
    GbaRuntimeSnapshot returned_checkpoint = {0};
    GbaRuntimeSnapshot target_restored_checkpoint = {0};
    FusionTransitionObservation observations[2];
    FusionTransitionPlan plan = {0};
    MzmStateView source_before = {0};
    MzmStateView source_returned = {0};
    AriaStateView target_before = {0};
    AriaStateView target_arrived = {0};
    AriaStateView target_restored = {0};
    AriaStateView target_reference_restored = {0};
    uint64_t target_reference_frames[ARIA_ROUNDTRIP_REPLAY_FRAMES] = {0};
    uint64_t target_replayed_frames[ARIA_ROUNDTRIP_REPLAY_FRAMES] = {0};
    bool target_byte_identical;
    unsigned replay_frame;
    FusionTransitionTransactionResult transaction;
    bool success = false;
    bool cleanup_ok = true;
    char cleanup_error[256] = {0};
    unsigned world;
    if (!metroid_path || !aria_path || !error || !error_size)
        return false;
    error[0] = '\0';
    if (!gba_runtime_open(&runtimes[WORLD_METROID], metroid_path,
                          error, error_size) ||
        !gba_runtime_open(&runtimes[WORLD_CASTLEVANIA], aria_path,
                          error, error_size))
        goto cleanup;
    gba_transition_target_init(&targets[WORLD_METROID],
                               &runtimes[WORLD_METROID], WORLD_METROID);
    gba_transition_target_init(&targets[WORLD_CASTLEVANIA],
                               &runtimes[WORLD_CASTLEVANIA], WORLD_CASTLEVANIA);
    if (!bootstrap_and_observe_mzm(&runtimes[WORLD_METROID],
                                   &targets[WORLD_METROID],
                                   &observations[WORLD_METROID],
                                   error, error_size) ||
        !bootstrap_and_observe_aria(&runtimes[WORLD_CASTLEVANIA],
                                    &targets[WORLD_CASTLEVANIA],
                                    &observations[WORLD_CASTLEVANIA],
                                    error, error_size))
        goto cleanup;
    if (runtimes[WORLD_METROID].active ||
        runtimes[WORLD_CASTLEVANIA].active ||
        !fusion_transition_plan_build(&observations[WORLD_METROID],
                                      WORLD_CASTLEVANIA, &plan)) {
        set_error(error, error_size, "roundtrip: invalid bootstrap/plan");
        goto cleanup;
    }

    /* MZM -> suspended. No other runtime may execute while this checkpoint
     * is captured, and no source frame may execute during the Aria visit. */
    if (!gba_runtime_enter(&runtimes[WORLD_METROID]) ||
        !fusion_roundtrip_exclusive(&runtimes[WORLD_METROID],
                                    &runtimes[WORLD_CASTLEVANIA]) ||
        !mzm_state_read(&runtimes[WORLD_METROID], &source_before,
                        error, error_size) ||
        !gba_runtime_capture(&runtimes[WORLD_METROID],
                             &source_checkpoint, error, error_size)) {
        if (!error[0]) set_error(error, error_size,
                                 "roundtrip: MZM checkpoint rejected");
        goto cleanup;
    }
    printf("Roundtrip: MZM departure room=%u:%u hp=%u/%u checkpoint=%zu\n",
           source_before.area, source_before.room, source_before.current_energy,
           source_before.max_energy, source_checkpoint.size);
    gba_runtime_leave(&runtimes[WORLD_METROID]);

    /* Visit Aria, use the already verified same-room arrival transaction,
     * and retain an OUTER checkpoint so a successful commit remains reversible. */
    if (!gba_runtime_enter(&runtimes[WORLD_CASTLEVANIA]) ||
        !fusion_roundtrip_exclusive(&runtimes[WORLD_CASTLEVANIA],
                                    &runtimes[WORLD_METROID]) ||
        !aria_state_read(&runtimes[WORLD_CASTLEVANIA], &target_before,
                         error, error_size) ||
        !gba_runtime_capture(&runtimes[WORLD_CASTLEVANIA],
                             &target_checkpoint, error, error_size)) {
        if (!error[0]) set_error(error, error_size,
                                 "roundtrip: Aria checkpoint rejected");
        goto cleanup;
    }
    /* Establish a reference replay from a loadState-restored baseline.
     * Rewind before running the actual target transaction. */
    if (!gba_runtime_restore(&runtimes[WORLD_CASTLEVANIA],
                             &target_checkpoint, error, error_size) ||
        !sample_aria_replay(&runtimes[WORLD_CASTLEVANIA],
                            target_reference_frames, error, error_size) ||
        !gba_runtime_restore(&runtimes[WORLD_CASTLEVANIA],
                             &target_checkpoint, error, error_size) ||
        !aria_state_read(&runtimes[WORLD_CASTLEVANIA],
                         &target_reference_restored, error, error_size) ||
        !fusion_roundtrip_aria_view_equal(&target_before,
                                          &target_reference_restored)) {
        if (!error[0]) set_error(error, error_size,
                  "roundtrip: Aria checkpoint failed baseline restore");
        goto cleanup;
    }
    transaction = fusion_transition_apply_transaction(
        &plan, gba_transition_target_ops(), &targets[WORLD_CASTLEVANIA]);
    if (transaction != FUSION_TRANSITION_TRANSACTION_COMMITTED) {
        snprintf(error, error_size,
                 "roundtrip: Aria arrival failed (%d): %.160s",
                 (int)transaction, targets[WORLD_CASTLEVANIA].error);
        goto cleanup;
    }
    if (!aria_state_read(&runtimes[WORLD_CASTLEVANIA], &target_arrived,
                         error, error_size) ||
        !target_arrived.gameplay_state_ready ||
        target_arrived.x_position_fixed != plan.target_position_x_q16 ||
        target_arrived.y_position_fixed != plan.target_position_y_q16 ||
        target_arrived.current_hp != plan.target_health ||
        target_arrived.max_hp != plan.target_max_health) {
        set_error(error, error_size,
                  "roundtrip: Aria committed state verification failed");
        goto cleanup;
    }
    printf("Roundtrip: Aria arrived room=%u:%u hp=%d/%u (native Soma entity)\n",
           target_arrived.area, target_arrived.room,
           target_arrived.current_hp, target_arrived.max_hp);
    gba_runtime_leave(&runtimes[WORLD_CASTLEVANIA]);

    /* Aria -> MZM: resume the SUSPENDED source, not a newly booted instance.
     * Its complete mGBA saved-state bytes must be unchanged across the visit. */
    if (!gba_runtime_enter(&runtimes[WORLD_METROID]) ||
        !fusion_roundtrip_exclusive(&runtimes[WORLD_METROID],
                                    &runtimes[WORLD_CASTLEVANIA]) ||
        !gba_runtime_capture(&runtimes[WORLD_METROID],
                             &returned_checkpoint, error, error_size) ||
        !mzm_state_read(&runtimes[WORLD_METROID], &source_returned,
                        error, error_size) ||
        !fusion_roundtrip_snapshot_equal(&source_checkpoint,
                                         &returned_checkpoint) ||
        source_returned.area != source_before.area ||
        source_returned.room != source_before.room ||
        source_returned.x_subpixels != source_before.x_subpixels ||
        source_returned.y_subpixels != source_before.y_subpixels ||
        source_returned.current_energy != source_before.current_energy ||
        source_returned.max_energy != source_before.max_energy) {
        if (!error[0]) set_error(error, error_size,
                                 "roundtrip: suspended MZM state changed");
        goto cleanup;
    }
    printf("Roundtrip: MZM resumed unchanged checkpoint=%zu position=%u,%u\n",
           returned_checkpoint.size, source_returned.x_subpixels,
           source_returned.y_subpixels);
    /* Prove one native frame can execute on re-entry, then undo that frame. */
    if (!gba_runtime_step(&runtimes[WORLD_METROID], 0) ||
        !gba_runtime_restore(&runtimes[WORLD_METROID], &source_checkpoint,
                             error, error_size)) {
        if (!error[0]) set_error(error, error_size,
                                 "roundtrip: MZM resume/restore failed");
        goto cleanup;
    }
    gba_runtime_leave(&runtimes[WORLD_METROID]);

    /* Restore Aria's original (pre-import) state even after a successful
     * transaction. Compare decoded memory AND deterministic replay. A byte
     * comparison is still reported, but re-serializing a loaded mGBA state
     * is not assumed to reproduce identical internal serialized metadata. */
    if (!gba_runtime_enter(&runtimes[WORLD_CASTLEVANIA]) ||
        !fusion_roundtrip_exclusive(&runtimes[WORLD_CASTLEVANIA],
                                    &runtimes[WORLD_METROID]) ||
        !gba_runtime_restore(&runtimes[WORLD_CASTLEVANIA],
                             &target_checkpoint, error, error_size) ||
        !gba_runtime_capture(&runtimes[WORLD_CASTLEVANIA],
                             &target_restored_checkpoint, error, error_size) ||
        !aria_state_read(&runtimes[WORLD_CASTLEVANIA], &target_restored,
                         error, error_size)) {
        if (!error[0]) set_error(error, error_size,
                                 "roundtrip: Aria restore/capture failed");
        goto cleanup;
    }
    target_byte_identical = fusion_roundtrip_snapshot_equal(
        &target_checkpoint, &target_restored_checkpoint);
    if (!fusion_roundtrip_aria_view_equal(&target_before, &target_restored)) {
        set_error(error, error_size,
                  "roundtrip: Aria decoded state differs after restore");
        goto cleanup;
    }
    if (!sample_aria_replay(&runtimes[WORLD_CASTLEVANIA],
                            target_replayed_frames, error, error_size))
        goto cleanup;
    for (replay_frame = 0; replay_frame < ARIA_ROUNDTRIP_REPLAY_FRAMES;
         ++replay_frame) {
        if (target_reference_frames[replay_frame] !=
            target_replayed_frames[replay_frame]) {
            snprintf(error, error_size,
                     "roundtrip: Aria replay diverged at frame %u "
                     "(expected=%016llx actual=%016llx)",
                     replay_frame + 1,
                     (unsigned long long)target_reference_frames[replay_frame],
                     (unsigned long long)target_replayed_frames[replay_frame]);
            goto cleanup;
        }
    }
    if (!gba_runtime_restore(&runtimes[WORLD_CASTLEVANIA],
                             &target_checkpoint, error, error_size) ||
        !aria_state_read(&runtimes[WORLD_CASTLEVANIA], &target_restored,
                         error, error_size) ||
        !fusion_roundtrip_aria_view_equal(&target_before, &target_restored)) {
        if (!error[0]) set_error(error, error_size,
                      "roundtrip: Aria post-replay restore failed");
        goto cleanup;
    }
    gba_runtime_leave(&runtimes[WORLD_CASTLEVANIA]);
    printf("Roundtrip: Aria outer rollback verified checkpoint=%zu "
           "state=match replay=%u frames serialized-bytes=%s\n",
           target_restored_checkpoint.size, ARIA_ROUNDTRIP_REPLAY_FRAMES,
           target_byte_identical ? "identical" : "different");
    puts("Roundtrip: MZM -> Aria -> MZM lifecycle verified; "
         "guest character transfer NOT implemented");
    success = true;
    set_error(error, error_size, "");

cleanup:
    /* Always restore both probe-owned checkpoints before releasing the ROMs.
     * The generic transaction owns and disposes its own inner checkpoint. */
    gba_runtime_leave(&runtimes[WORLD_METROID]);
    gba_runtime_leave(&runtimes[WORLD_CASTLEVANIA]);
    for (world = 0; world < 2; ++world) {
        GbaRuntimeSnapshot *saved = world == WORLD_METROID
            ? &source_checkpoint : &target_checkpoint;
        if (!saved->data) continue;
        if (!gba_runtime_enter(&runtimes[world]) ||
            !gba_runtime_restore(&runtimes[world], saved,
                                 cleanup_error, sizeof(cleanup_error))) {
            cleanup_ok = false;
        }
        gba_runtime_leave(&runtimes[world]);
    }
    if (!cleanup_ok) {
        char message[256];
        snprintf(message, sizeof(message),
                 "roundtrip: cleanup checkpoint restore failed: %.170s",
                 cleanup_error);
        set_error(error, error_size, message);
        success = false;
    }
    gba_runtime_snapshot_dispose(&target_restored_checkpoint);
    gba_runtime_snapshot_dispose(&returned_checkpoint);
    gba_runtime_snapshot_dispose(&target_checkpoint);
    gba_runtime_snapshot_dispose(&source_checkpoint);
    gba_runtime_close(&runtimes[WORLD_CASTLEVANIA]);
    gba_runtime_close(&runtimes[WORLD_METROID]);
    return success;
}
