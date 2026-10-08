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

static bool bootstrap_aria(GbaTransitionTarget *target)
{
    AriaStateView state;
    bool entered_game = false;
    unsigned frame;
    for (frame = 1; frame <= 12000; ++frame) {
        uint16_t keys = 0;
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
        if (state.gameplay_state_ready) {
            target->loader_frames = frame;
            return true;
        }
    }
    set_error(target, "Aria bootstrap did not reach gameplay");
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

static bool apply_aria_loader(GbaTransitionTarget *target,
                              const FusionTransitionPlan *plan)
{
    uint8_t phase[2] = {0, 2};
    uint8_t location[2] = {plan->target_area, plan->target_room};
    uint8_t staged[8];
    uint8_t room_pointer[4];
    uint8_t health[2];
    AriaStateView state;
    unsigned frame;

    write_u16(staged, 0, plan->target_camera_x);
    write_u16(staged, 2, plan->target_camera_y);
    write_u16(staged, 4, plan->target_player_x);
    write_u16(staged, 6, plan->target_player_y);
    write_u32(room_pointer, 0, plan->target_room_pointer);
    if (!checkpointed_write(target, ARIA_LOCATION_ADDRESS,
                            location, sizeof(location)) ||
        !checkpointed_write(target, ARIA_STAGED_ARRIVAL_ADDRESS,
                            staged, sizeof(staged)) ||
        !checkpointed_write(target, ARIA_STAGED_ROOM_POINTER_ADDRESS,
                            room_pointer, sizeof(room_pointer)) ||
        !checkpointed_write(target, ARIA_PHASE_ADDRESS,
                            phase, sizeof(phase)))
        return false;

    for (frame = 1; frame <= TARGET_LOADER_FRAME_LIMIT; ++frame) {
        if (!gba_runtime_step(target->runtime, 0) ||
            !aria_state_read(target->runtime, &state,
                             target->error, sizeof(target->error)))
            return false;
        if (state.gameplay_state_ready &&
            state.area == plan->target_area &&
            state.room == plan->target_room &&
            state.x_position_fixed == plan->target_position_x_q16 &&
            state.y_position_fixed == plan->target_position_y_q16)
            break;
    }
    if (frame > TARGET_LOADER_FRAME_LIMIT) {
        char detail[sizeof(target->error)];
        snprintf(detail, sizeof(detail),
                 "Aria timeout %u: mode=%u:%u phase=%u:%u ready=%u ctl=%u "
                 "room=%u:%u pos=%08x,%08x expected=%08x,%08x "
                 "staged=%08x:%u,%u:%u,%u",
                 TARGET_LOADER_FRAME_LIMIT,
                 state.game_mode, state.game_mode_stage,
                 state.in_game_phase, state.in_game_phase_stage,
                 (unsigned)state.gameplay_state_ready,
                 (unsigned)state.player_control_enabled,
                 state.area, state.room,
                 (unsigned)state.x_position_fixed,
                 (unsigned)state.y_position_fixed,
                 (unsigned)plan->target_position_x_q16,
                 (unsigned)plan->target_position_y_q16,
                 (unsigned)state.staged_room_pointer,
                 state.staged_camera_x, state.staged_camera_y,
                 state.staged_player_x, state.staged_player_y);
        set_error(target, detail);
        return false;
    }
    target->loader_frames = frame;
    write_u16(health, 0, (uint16_t)plan->target_max_health);
    if (!checkpointed_write(target, ARIA_MAX_HP_ADDRESS,
                            health, sizeof(health)))
        return false;
    write_u16(health, 0, (uint16_t)plan->target_health);
    if (!checkpointed_write(target, ARIA_CURRENT_HP_ADDRESS,
                            health, sizeof(health)) ||
        !gba_runtime_step(target->runtime, 0)) {
        set_error(target, "Aria health import did not complete");
        return false;
    }
    return true;
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
