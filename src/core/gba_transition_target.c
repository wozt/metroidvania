/* SPDX-License-Identifier: GPL-3.0-only */
#include "gba/transition_target.h"

#include "aria/state.h"
#include "mzm/state.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    MZM_CURRENT_DEMO_ADDRESS = 0x03000038,
    MZM_LOCATION_ADDRESS = 0x03000054,
    MZM_CURRENT_CUTSCENE_ADDRESS = 0x0300007d,
    MZM_PAUSE_FLAG_ADDRESS = 0x03000bf0,
    MZM_LOADING_FILE_ADDRESS = 0x03000c1d,
    MZM_GAME_MODE_ADDRESS = 0x03000c70,
    MZM_DEMO_STATE_ADDRESS = 0x030013d2,
    MZM_MAX_ENERGY_ADDRESS = 0x03001530,
    MZM_CURRENT_ENERGY_ADDRESS = 0x03001536,
    MZM_DOOR_OFFSET_ADDRESS = 0x0300550c,
    ARIA_PHASE_ADDRESS = 0x02000064,
    ARIA_LOCATION_ADDRESS = 0x0200009e,
    ARIA_STAGED_ARRIVAL_ADDRESS = 0x02000334,
    ARIA_STAGED_ROOM_POINTER_ADDRESS = 0x020003cc,
    ARIA_CURRENT_HP_ADDRESS = 0x0201327a,
    ARIA_MAX_HP_ADDRESS = 0x0201327e,
    TARGET_LOADER_FRAME_LIMIT = 600,
};

static void set_error(GbaTransitionTarget *target, const char *message)
{
    if (target)
        snprintf(target->error, sizeof(target->error), "%s", message);
}

static void write_u16(uint8_t *bytes, size_t offset, uint16_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *bytes, size_t offset, uint32_t value)
{
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
    bytes[offset + 2] = (uint8_t)(value >> 16);
    bytes[offset + 3] = (uint8_t)(value >> 24);
}

static bool checkpointed_write(GbaTransitionTarget *target, uint32_t address,
                               const void *data, size_t size)
{
    if (!target || !target->active_checkpoint ||
        !gba_runtime_write_memory_checkpointed(
            target->runtime, target->active_checkpoint, address, data, size)) {
        set_error(target, "checkpointed target write was rejected");
        return false;
    }
    return true;
}

static bool bootstrap_mzm(GbaTransitionTarget *target)
{
    MzmStateView state;
    unsigned frame;
    for (frame = 1; frame <= 6000; ++frame) {
        if (!gba_runtime_step(target->runtime, 0) ||
            !mzm_state_read(target->runtime, &state,
                            target->error, sizeof(target->error)))
            return false;
        if (state.gameplay_state_ready) {
            target->loader_frames = frame;
            return true;
        }
    }
    set_error(target, "MZM bootstrap did not reach gameplay");
    return false;
}

/* A single gameplay-ready frame is not proof of a stable destination.
 * During new-game startup, Aria can briefly enable player control and then
 * enter an introductory sequence.  Probe an unmodified runtime first, and
 * accept an Entrance destination only after sustained native readiness.
 * This function performs no WRAM writes and cannot bypass the cutscene.
 */
static bool bootstrap_aria(GbaTransitionTarget *target)
{
    enum { ARIA_BOOTSTRAP_READY_WINDOW = 90, ARIA_BOOTSTRAP_FRAME_LIMIT = 12000 };
    AriaStateView state = {0};
    bool entered_game = false;
    unsigned frame;
    unsigned ready_streak = 0;
    unsigned longest_streak = 0;
    unsigned first_ready = 0;

    for (frame = 1; frame <= ARIA_BOOTSTRAP_FRAME_LIMIT; ++frame) {
        uint16_t keys = 0;
        bool candidate;
        if (frame > 330 && frame % 90 == 0) {
            if (entered_game)
                keys = 1u << 0;
            else
                keys = ((frame / 90) & 1u) ? (1u << 0) : (1u << 3);
        }
        if (!gba_runtime_step(target->runtime, keys) ||
            !aria_state_read(target->runtime, &state,
                             target->error, sizeof(target->error)))
            return false;
        if (state.gameplay_active) entered_game = true;
        if (state.gameplay_state_ready && !first_ready) {
            first_ready = frame;
            fprintf(stderr, "Aria bootstrap: first ready frame=%u phase=%u:%u "
                    "room=%u:%u control=%u animation=%u:%u\n",
                    frame, state.in_game_phase, state.in_game_phase_stage,
                    state.area, state.room,
                    (unsigned)state.player_control_enabled,
                    state.animation_id, state.animation_frame);
        }
        candidate = state.gameplay_state_ready &&
                    state.player_entity_valid &&
                    state.player_control_enabled &&
                    state.area == 0 && state.room == 0 &&
                    state.staged_room_pointer == UINT32_C(0x0850ef9c);
        if (candidate) {
            if (ready_streak < ARIA_BOOTSTRAP_READY_WINDOW)
                ++ready_streak;
            if (ready_streak > longest_streak)
                longest_streak = ready_streak;
            if (ready_streak == ARIA_BOOTSTRAP_READY_WINDOW) {
                target->loader_frames = frame;
                fprintf(stderr, "Aria bootstrap: stable gameplay for %u "
                        "frames (first-ready=%u accepted=%u)\n",
                        ready_streak, first_ready, frame);
                return true;
            }
        } else {
            ready_streak = 0;
        }
    }
    /* The last observed state is intentionally reported, even if a brief
     * ready frame was seen earlier.  Do not proceed to arrival WRAM writes.
     */
    snprintf(target->error, sizeof(target->error),
             "Aria bootstrap not stable: first=%u longest=%u/%u "
             "lastphase=%u:%u ctl=%u room=%u:%u anim=%u:%u",
             first_ready, longest_streak, ARIA_BOOTSTRAP_READY_WINDOW,
             state.in_game_phase, state.in_game_phase_stage,
             (unsigned)state.player_control_enabled,
             state.area, state.room, state.animation_id, state.animation_frame);
    return false;
}

void gba_transition_target_init(GbaTransitionTarget *target,
                                GbaRuntime *runtime, WorldKind world)
{
    if (!target) return;
    memset(target, 0, sizeof(*target));
    target->runtime = runtime;
    target->world = world;
}

bool gba_transition_target_bootstrap(GbaTransitionTarget *target)
{
    if (!target || !target->runtime || target->active_checkpoint ||
        (target->world != WORLD_METROID &&
         target->world != WORLD_CASTLEVANIA)) {
        set_error(target, "invalid GBA transition bootstrap state");
        return false;
    }
    target->error[0] = '\0';
    if (target->world == WORLD_METROID) return bootstrap_mzm(target);
    return bootstrap_aria(target);
}

static bool target_preflight(void *context, const FusionTransitionPlan *plan)
{
    GbaTransitionTarget *target = context;
    if (!target || !target->runtime || target->active_checkpoint ||
        !fusion_transition_plan_valid(plan) ||
        plan->target_world != target->world || plan->target_health <= 0 ||
        plan->target_max_health <= 0 || plan->target_health > UINT16_MAX ||
        plan->target_max_health > UINT16_MAX) {
        set_error(target, "transition target preflight rejected the plan");
        return false;
    }
    if (target->world == WORLD_METROID) {
        MzmStateView state;
        if (!mzm_state_read(target->runtime, &state,
                            target->error, sizeof(target->error)) ||
            !state.gameplay_state_ready) {
            set_error(target, "MZM target is not gameplay-ready");
            return false;
        }
    } else {
        AriaStateView state;
        if (!aria_state_read(target->runtime, &state,
                             target->error, sizeof(target->error)) ||
            !state.gameplay_state_ready) {
            set_error(target, "Aria target is not gameplay-ready");
            return false;
        }
    }
    target->error[0] = '\0';
    return true;
}

static bool target_capture(void *context, void **checkpoint_out)
{
    GbaTransitionTarget *target = context;
    GbaRuntimeSnapshot *checkpoint;
    if (!target || !checkpoint_out || *checkpoint_out ||
        target->active_checkpoint) {
        set_error(target, "invalid target checkpoint capture state");
        return false;
    }
    checkpoint = calloc(1, sizeof(*checkpoint));
    if (!checkpoint) {
        set_error(target, "cannot allocate target checkpoint");
        return false;
    }
    if (!gba_runtime_capture(target->runtime, checkpoint,
                             target->error, sizeof(target->error))) {
        free(checkpoint);
        return false;
    }
    target->active_checkpoint = checkpoint;
    *checkpoint_out = checkpoint;
    return true;
}

static bool apply_mzm_loader(GbaTransitionTarget *target,
                             const FusionTransitionPlan *plan)
{
    uint8_t current_demo[4] = {0};
    uint8_t location[3] = {plan->target_area, plan->target_room,
                           plan->target_door};
    uint8_t mode[6] = {4, 0, 0, 0, 0, 1};
    uint8_t zero = 0;
    uint8_t zero16[2] = {0};
    uint8_t health[2];
    MzmStateView state;
    unsigned frame;

    if (!checkpointed_write(target, MZM_CURRENT_DEMO_ADDRESS,
                            current_demo, sizeof(current_demo)) ||
        !checkpointed_write(target, MZM_LOCATION_ADDRESS,
                            location, sizeof(location)) ||
        !checkpointed_write(target, MZM_CURRENT_CUTSCENE_ADDRESS,
                            &zero, sizeof(zero)) ||
        !checkpointed_write(target, MZM_PAUSE_FLAG_ADDRESS,
                            &zero, sizeof(zero)) ||
        !checkpointed_write(target, MZM_LOADING_FILE_ADDRESS,
                            &zero, sizeof(zero)) ||
        !checkpointed_write(target, MZM_GAME_MODE_ADDRESS,
                            mode, sizeof(mode)) ||
        !checkpointed_write(target, MZM_DEMO_STATE_ADDRESS,
                            &zero, sizeof(zero)) ||
        !checkpointed_write(target, MZM_DOOR_OFFSET_ADDRESS,
                            zero16, sizeof(zero16)))
        return false;

    for (frame = 1; frame <= TARGET_LOADER_FRAME_LIMIT; ++frame) {
        if (!gba_runtime_step(target->runtime, 0) ||
            !mzm_state_read(target->runtime, &state,
                            target->error, sizeof(target->error)))
            return false;
        if (state.game_mode == 4 && state.gameplay_state_ready &&
            state.area == plan->target_area &&
            state.room == plan->target_room &&
            state.last_door == plan->target_door &&
            state.x_subpixels == (plan->target_position_x_q16 >> 14) &&
            state.y_subpixels == (plan->target_position_y_q16 >> 14))
            break;
    }
    if (frame > TARGET_LOADER_FRAME_LIMIT) {
        set_error(target, "MZM native loader did not reach the planned arrival");
        return false;
    }
    target->loader_frames = frame;
    write_u16(health, 0, (uint16_t)plan->target_max_health);
    if (!checkpointed_write(target, MZM_MAX_ENERGY_ADDRESS,
                            health, sizeof(health)))
        return false;
    write_u16(health, 0, (uint16_t)plan->target_health);
    if (!checkpointed_write(target, MZM_CURRENT_ENERGY_ADDRESS,
                            health, sizeof(health)) ||
        !gba_runtime_step(target->runtime, 0)) {
        set_error(target, "MZM health import did not complete");
        return false;
    }
    return true;
}

/* An already-loaded target room can accept an in-room arrival safely.
 * This is NOT a cross-room loader: requiring matching room descriptors avoids
 * the invalid Aria phase-3 path that loses the player-entity pointer.
 */
/* Aria's normal Entrance context preview already sends isolated A pulses
 * every 90 frames while startup scripting settles.  Reuse that schedule
 * ONLY when the engine has not enabled character control; never hold A. */
uint16_t gba_transition_aria_settle_input(unsigned frame, bool control_enabled)
{
    return frame && !control_enabled && frame % 90u == 0u ? UINT16_C(1) : 0;
}

bool gba_transition_aria_same_room_compatible(const AriaStateView *view,
                                              const FusionTransitionPlan *plan)
{
    if (!view || !plan || !fusion_transition_plan_valid(plan) ||
        plan->target_world != WORLD_CASTLEVANIA ||
        plan->arrival_kind != FUSION_TRANSITION_ARRIVAL_ARIA_STAGED_ROOM ||
        !view->gameplay_state_ready || !view->player_entity_valid ||
        !view->player_entity_address || view->current_character != 0 ||
        view->area != plan->target_area || view->room != plan->target_room ||
        view->staged_room_pointer != plan->target_room_pointer ||
        view->staged_camera_x != plan->target_camera_x ||
        view->staged_camera_y != plan->target_camera_y ||
        view->staged_player_x != plan->target_player_x ||
        view->staged_player_y != plan->target_player_y ||
        view->camera_x_fixed > plan->target_position_x_q16 ||
        view->camera_y_fixed > plan->target_position_y_q16)
        return false;
    return true;
}

static bool apply_aria_loader(GbaTransitionTarget *target,
                              const FusionTransitionPlan *plan)
{
    enum { ARIA_PLAYER_POSITION_OFFSET = 0x40 };
    AriaStateView before = {0};
    AriaStateView immediate = {0};
    AriaStateView after = {0};
    uint8_t player[16] = {0};
    uint8_t health[2];

    if (!aria_state_read(target->runtime, &before,
                         target->error, sizeof(target->error)))
        return false;
    if (!gba_transition_aria_same_room_compatible(&before, plan)) {
        set_error(target, "Aria target requires a verified loaded Entrance room; "
                  "cross-room loading is not implemented");
        return false;
    }

    /* Reuse the ROM-tested in-room placement from authentic_preview.c.
     * Entity coordinates are camera-relative Q16.16; velocities are cleared.
     * All writes are checkpoint-protected by the target transaction.
     */
    write_u32(player, 0, plan->target_position_x_q16 - before.camera_x_fixed);
    write_u32(player, 4, plan->target_position_y_q16 - before.camera_y_fixed);
    write_u32(player, 8, 0);
    write_u32(player, 12, 0);
    if (!checkpointed_write(target,
                            before.player_entity_address + ARIA_PLAYER_POSITION_OFFSET,
                            player, sizeof(player)))
        return false;
    if (!aria_state_read(target->runtime, &immediate,
                         target->error, sizeof(target->error)) ||
        immediate.x_position_fixed != plan->target_position_x_q16 ||
        immediate.y_position_fixed != plan->target_position_y_q16 ||
        immediate.x_velocity_fixed != 0 || immediate.y_velocity_fixed != 0) {
        set_error(target, "Aria in-room placement failed immediate verification");
        return false;
    }

    write_u16(health, 0, (uint16_t)plan->target_max_health);
    if (!checkpointed_write(target, ARIA_MAX_HP_ADDRESS,
                            health, sizeof(health)))
        return false;
    write_u16(health, 0, (uint16_t)plan->target_health);
    if (!checkpointed_write(target, ARIA_CURRENT_HP_ADDRESS,
                            health, sizeof(health)))
        return false;

    /* The staged arrival descriptor is input to the room loader, not a
     * persistent post-frame invariant.  Only require its exact identity at
     * preflight, before touching emulated memory.  The running game may
     * update staging fields on the next frame.  Verify the stable state
     * instead: actual player, room, position and imported health.
     *
     * The initial Entrance transition may temporarily disable control; let
     * it settle within a bounded window, but never accept a missing player,
     * mismatched location, drift, or altered health.  Any failure invokes
     * the existing checkpoint rollback at the transaction boundary.
     */
    {
        enum { ARIA_IN_ROOM_SETTLE_LIMIT = 540 };
        unsigned frame;
        for (frame = 1; frame <= ARIA_IN_ROOM_SETTLE_LIMIT; ++frame) {
            const bool previously_controllable = frame == 1
                ? before.player_control_enabled : after.player_control_enabled;
            const uint16_t keys = gba_transition_aria_settle_input(
                frame, previously_controllable);
            if (!gba_runtime_step(target->runtime, keys) ||
                !aria_state_read(target->runtime, &after,
                                 target->error, sizeof(target->error))) {
                set_error(target, "Aria in-room arrival frame execution failed");
                return false;
            }
            if (!after.gameplay_active || !after.values_plausible ||
                !after.player_entity_valid || after.current_character != 0 ||
                after.area != plan->target_area ||
                after.room != plan->target_room ||
                after.x_position_fixed != plan->target_position_x_q16 ||
                after.y_position_fixed != plan->target_position_y_q16 ||
                after.current_hp != plan->target_health ||
                after.max_hp != plan->target_max_health) {
                char detail[sizeof(target->error)];
                snprintf(detail, sizeof(detail),
                         "Aria arrival frame %u mismatch: phase=%u:%u "
                         "ready=%u ctl=%u player=%08x room=%u:%u "
                         "pos=%08x,%08x hp=%d/%u",
                         frame, after.in_game_phase, after.in_game_phase_stage,
                         (unsigned)after.gameplay_state_ready,
                         (unsigned)after.player_control_enabled,
                         (unsigned)after.player_entity_address,
                         after.area, after.room,
                         (unsigned)after.x_position_fixed,
                         (unsigned)after.y_position_fixed,
                         after.current_hp, after.max_hp);
                set_error(target, detail);
                return false;
            }
            /* Record the normal startup state and any input pulse.  This is
             * an observation, not proof of a completed cross-room loader. */
            if (frame == 1 || keys || frame % 90u == 0u) {
                fprintf(stderr, "Aria settle frame=%u A=%u phase=%u:%u "
                        "ready=%u ctl=%u room=%u:%u pos=%08x,%08x "
                        "animation=%u:%u\n",
                        frame, (unsigned)(keys != 0),
                        after.in_game_phase, after.in_game_phase_stage,
                        (unsigned)after.gameplay_state_ready,
                        (unsigned)after.player_control_enabled,
                        after.area, after.room,
                        (unsigned)after.x_position_fixed,
                        (unsigned)after.y_position_fixed,
                        after.animation_id, after.animation_frame);
            }
            if (after.gameplay_state_ready) {
                target->loader_frames = frame;
                fprintf(stderr, "Aria arrival: verified in-room placement "
                        "after %u frame(s), native room loader not invoked\n",
                        frame);
                return true;
            }
        }
        {
            char detail[sizeof(target->error)];
            snprintf(detail, sizeof(detail),
                     "Aria in-room settle timeout: phase=%u:%u ready=%u "
                     "ctl=%u player=%08x pos=%08x,%08x hp=%d/%u",
                     after.in_game_phase, after.in_game_phase_stage,
                     (unsigned)after.gameplay_state_ready,
                     (unsigned)after.player_control_enabled,
                     (unsigned)after.player_entity_address,
                     (unsigned)after.x_position_fixed,
                     (unsigned)after.y_position_fixed,
                     after.current_hp, after.max_hp);
            set_error(target, detail);
        }
        return false;
    }
}

static bool target_apply(void *context, const FusionTransitionPlan *plan)
{
    GbaTransitionTarget *target = context;
    if (!target || !target->active_checkpoint) {
        set_error(target, "target apply requires an active checkpoint");
        return false;
    }
    if (target->world == WORLD_METROID)
        return apply_mzm_loader(target, plan);
    return apply_aria_loader(target, plan);
}

static bool target_verify(void *context, const FusionTransitionPlan *plan)
{
    GbaTransitionTarget *target = context;
    if (!target || !plan) return false;
    if (target->world == WORLD_METROID) {
        MzmStateView state;
        return mzm_state_read(target->runtime, &state,
                              target->error, sizeof(target->error)) &&
               state.game_mode == 4 && state.gameplay_state_ready &&
               state.area == plan->target_area &&
               state.room == plan->target_room &&
               state.last_door == plan->target_door &&
               state.x_subpixels == (plan->target_position_x_q16 >> 14) &&
               state.y_subpixels == (plan->target_position_y_q16 >> 14) &&
               state.current_energy == plan->target_health &&
               state.max_energy == plan->target_max_health;
    }
    {
        AriaStateView state;
        return aria_state_read(target->runtime, &state,
                               target->error, sizeof(target->error)) &&
               state.gameplay_state_ready &&
               state.area == plan->target_area &&
               state.room == plan->target_room &&
               state.x_position_fixed == plan->target_position_x_q16 &&
               state.y_position_fixed == plan->target_position_y_q16 &&
               state.current_hp == plan->target_health &&
               state.max_hp == plan->target_max_health;
    }
}

static bool target_rollback(void *context, void *checkpoint_pointer)
{
    GbaTransitionTarget *target = context;
    GbaRuntimeSnapshot *checkpoint = checkpoint_pointer;
    char operation_error[sizeof(target->error)];
    char restore_error[sizeof(target->error)];
    bool restored;
    if (!target || !checkpoint) return false;
    snprintf(operation_error, sizeof(operation_error), "%s", target->error);
    restored = gba_runtime_restore(target->runtime, checkpoint,
                                   restore_error, sizeof(restore_error));
    if (!restored)
        snprintf(target->error, sizeof(target->error),
                 "target rollback failed: %.160s", restore_error);
    else
        snprintf(target->error, sizeof(target->error), "%s", operation_error);
    return restored;
}

static void target_dispose(void *context, void *checkpoint_pointer)
{
    GbaTransitionTarget *target = context;
    GbaRuntimeSnapshot *checkpoint = checkpoint_pointer;
    if (target && target->active_checkpoint == checkpoint)
        target->active_checkpoint = NULL;
    if (!checkpoint) return;
    gba_runtime_snapshot_dispose(checkpoint);
    free(checkpoint);
}

const FusionTransitionTargetOps *gba_transition_target_ops(void)
{
    static const FusionTransitionTargetOps ops = {
        .preflight = target_preflight,
        .capture_checkpoint = target_capture,
        .apply = target_apply,
        .verify = target_verify,
        .rollback = target_rollback,
        .dispose_checkpoint = target_dispose,
    };
    return &ops;
}
