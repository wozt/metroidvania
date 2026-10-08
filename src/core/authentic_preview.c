/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/authentic_preview.h"

#include "aria/state.h"
#include "gba/runtime.h"
#include "mzm/state.h"

#include <SDL3/SDL.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

enum {
    MZM_SAMUS_POSITION_ADDRESS = 0x030013e6,
    ARIA_PLAYER_POSITION_OFFSET = 0x40,
};

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

static void set_error_detail(char *error, size_t error_size,
                             const char *message, const char *detail)
{
    if (error && error_size)
        snprintf(error, error_size, "%s: %s", message, detail);
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

static bool ensure_directory(const char *path, char *error, size_t error_size)
{
    struct stat status;
    if (mkdir(path, 0755) == 0) return true;
    if (errno == EEXIST && stat(path, &status) == 0 && S_ISDIR(status.st_mode))
        return true;
    set_error_detail(error, error_size, "cannot create preview directory",
                     strerror(errno));
    return false;
}

static bool make_capture_path(char *path, size_t path_size,
                              const char *directory, const char *filename,
                              char *error, size_t error_size)
{
    int length = snprintf(path, path_size, "%s/%s", directory, filename);
    if (length >= 0 && (size_t)length < path_size) return true;
    set_error(error, error_size, "preview capture path is too long");
    return false;
}

static bool save_frame(const GbaRuntime *runtime, const char *path,
                       char *error, size_t error_size)
{
    GbaFrameView frame;
    SDL_Surface *surface;
    SDL_Surface *opaque_surface;
    bool saved;
    if (!gba_runtime_frame(runtime, &frame)) {
        set_error(error, error_size, "cannot read the preview framebuffer");
        return false;
    }
    surface = SDL_CreateSurfaceFrom((int)frame.width, (int)frame.height,
                                    SDL_PIXELFORMAT_RGBA32,
                                    (void *)frame.rgba32,
                                    (int)(frame.stride_pixels * sizeof(uint32_t)));
    if (!surface) {
        set_error_detail(error, error_size,
                         "cannot wrap the preview framebuffer", SDL_GetError());
        return false;
    }
    opaque_surface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGB24);
    if (!opaque_surface) {
        set_error_detail(error, error_size,
                         "cannot convert the preview framebuffer", SDL_GetError());
        SDL_DestroySurface(surface);
        return false;
    }
    saved = SDL_SaveBMP(opaque_surface, path);
    if (!saved)
        set_error_detail(error, error_size,
                         "cannot save the preview framebuffer", SDL_GetError());
    SDL_DestroySurface(opaque_surface);
    SDL_DestroySurface(surface);
    return saved;
}

static bool settle_mzm(GbaRuntime *runtime, MzmStateView *state,
                       char *error, size_t error_size)
{
    unsigned frame;
    for (frame = 0; frame < 60; ++frame) {
        if (!gba_runtime_step(runtime, 0) ||
            !mzm_state_read(runtime, state, error, error_size))
            return false;
    }
    if (state->gameplay_state_ready) return true;
    set_error(error, error_size,
              "MZM left its gameplay-ready state while settling the preview");
    return false;
}

static bool boot_mzm(GbaRuntime *runtime, MzmStateView *state,
                     char *error, size_t error_size)
{
    unsigned frame;
    for (frame = 1; frame <= 6000; ++frame) {
        if (!gba_runtime_step(runtime, 0) ||
            !mzm_state_read(runtime, state, error, error_size))
            return false;
        if (state->gameplay_state_ready)
            return settle_mzm(runtime, state, error, error_size);
    }
    set_error(error, error_size,
              "MZM did not reach a gameplay-ready preview state");
    return false;
}

static bool boot_aria(GbaRuntime *runtime, AriaStateView *state,
                      char *error, size_t error_size)
{
    unsigned frame;
    bool entered_game = false;
    for (frame = 1; frame <= 12000; ++frame) {
        uint16_t keys = 0;
        if (frame > 330 && frame % 90 == 0) {
            if (entered_game)
                keys = 1u << 0;
            else
                keys = ((frame / 90) & 1u) ? (1u << 0) : (1u << 3);
        }
        if (!gba_runtime_step(runtime, keys) ||
            !aria_state_read(runtime, state, error, error_size))
            return false;
        if (state->gameplay_active) entered_game = true;
        if (state->gameplay_state_ready) return true;
    }
    set_error(error, error_size,
              "Aria did not reach a gameplay-ready preview state");
    return false;
}

static bool preview_mzm(const char *rom_path, const char *output_directory,
                        char *error, size_t error_size)
{
    GbaRuntime runtime = {0};
    GbaRuntimeSnapshot checkpoint = {0};
    MzmStateView before = {0};
    MzmStateView immediate = {0};
    MzmStateView after = {0};
    MzmStateView restored = {0};
    uint8_t arrival[8] = {0};
    char path[512];
    char restore_error[256];
    bool captured = false;
    bool success = false;

    if (!gba_runtime_open(&runtime, rom_path, error, error_size) ||
        !gba_runtime_enter(&runtime)) {
        if (!error || !error_size || !error[0])
            set_error(error, error_size, "cannot enter the MZM preview runtime");
        goto cleanup;
    }
    if (!boot_mzm(&runtime, &before, error, error_size)) goto cleanup;
    if (before.area != 0 || before.room != 28 || before.last_door != 60) {
        set_error(error, error_size,
                  "MZM preview state does not match Brinstar door 60");
        goto cleanup;
    }
    if (!make_capture_path(path, sizeof(path), output_directory,
                           "mzm-before.bmp", error, error_size) ||
        !save_frame(&runtime, path, error, error_size) ||
        !gba_runtime_capture(&runtime, &checkpoint, error, error_size))
        goto cleanup;
    captured = true;

    write_u16(arrival, 0, 288);
    write_u16(arrival, 2, 511);
    write_u16(arrival, 4, 0);
    write_u16(arrival, 6, 0);
    if (!gba_runtime_write_memory_checkpointed(
            &runtime, &checkpoint, MZM_SAMUS_POSITION_ADDRESS,
            arrival, sizeof(arrival)) ||
        !mzm_state_read(&runtime, &immediate, error, error_size) ||
        immediate.x_subpixels != 288 || immediate.y_subpixels != 511 ||
        immediate.x_velocity != 0 || immediate.y_velocity != 0) {
        set_error(error, error_size,
                  "MZM rejected the checkpointed arrival preview");
        goto cleanup;
    }
    if (!gba_runtime_step(&runtime, 0) ||
        !mzm_state_read(&runtime, &after, error, error_size)) {
        set_error(error, error_size, "MZM preview frame execution failed");
        goto cleanup;
    }
    if (!make_capture_path(path, sizeof(path), output_directory,
                           "mzm-after.bmp", error, error_size) ||
        !save_frame(&runtime, path, error, error_size))
        goto cleanup;
    printf("Arrival preview: MZM room=%u:%u before=%u,%u injected=%u,%u "
           "after-one-frame=%u,%u\n",
           before.area, before.room, before.x_subpixels, before.y_subpixels,
           immediate.x_subpixels, immediate.y_subpixels,
           after.x_subpixels, after.y_subpixels);
    success = true;

cleanup:
    if (captured) {
        if (!gba_runtime_restore(&runtime, &checkpoint,
                                 restore_error, sizeof(restore_error)) ||
            !mzm_state_read(&runtime, &restored,
                            restore_error, sizeof(restore_error)) ||
            restored.x_subpixels != before.x_subpixels ||
            restored.y_subpixels != before.y_subpixels) {
            set_error_detail(error, error_size, "MZM preview rollback failed",
                             restore_error[0] ? restore_error : "state mismatch");
            success = false;
        }
    }
    if (success) puts("Arrival preview rollback: MZM verified");
    gba_runtime_snapshot_dispose(&checkpoint);
    gba_runtime_close(&runtime);
    return success;
}

static bool preview_aria(const char *rom_path, const char *output_directory,
                         char *error, size_t error_size)
{
    GbaRuntime runtime = {0};
    GbaRuntimeSnapshot checkpoint = {0};
    AriaStateView before = {0};
    AriaStateView immediate = {0};
    AriaStateView after = {0};
    AriaStateView context = {0};
    AriaStateView restored = {0};
    uint8_t player[16] = {0};
    char path[512];
    char restore_error[256];
    bool captured = false;
    bool success = false;

    if (!gba_runtime_open(&runtime, rom_path, error, error_size) ||
        !gba_runtime_enter(&runtime)) {
        if (!error || !error_size || !error[0])
            set_error(error, error_size, "cannot enter the Aria preview runtime");
        goto cleanup;
    }
    if (!boot_aria(&runtime, &before, error, error_size)) goto cleanup;
    if (before.area != 0 || before.room != 0 || !before.player_entity_valid ||
        before.staged_room_pointer != UINT32_C(0x0850ef9c)) {
        set_error(error, error_size,
                  "Aria preview state does not match the Entrance descriptor");
        goto cleanup;
    }
    if (!make_capture_path(path, sizeof(path), output_directory,
                           "aria-before.bmp", error, error_size) ||
        !save_frame(&runtime, path, error, error_size) ||
        !gba_runtime_capture(&runtime, &checkpoint, error, error_size))
        goto cleanup;
    captured = true;

    if (before.camera_x_fixed > UINT32_C(0x00980000) ||
        before.camera_y_fixed > UINT32_C(0x028d0000)) {
        set_error(error, error_size,
                  "Aria target arrival is outside the current camera window");
        goto cleanup;
    }
    write_u32(player, 0, UINT32_C(0x00980000) - before.camera_x_fixed);
    write_u32(player, 4, UINT32_C(0x028d0000) - before.camera_y_fixed);
    write_u32(player, 8, 0);
    write_u32(player, 12, 0);
    if (!gba_runtime_write_memory_checkpointed(
            &runtime, &checkpoint,
            before.player_entity_address + ARIA_PLAYER_POSITION_OFFSET,
            player, sizeof(player)) ||
        !aria_state_read(&runtime, &immediate, error, error_size) ||
        immediate.x_position_fixed != UINT32_C(0x00980000) ||
        immediate.y_position_fixed != UINT32_C(0x028d0000) ||
        immediate.x_velocity_fixed != 0 || immediate.y_velocity_fixed != 0) {
        set_error(error, error_size,
                  "Aria rejected the checkpointed arrival preview");
        goto cleanup;
    }
    if (!gba_runtime_step(&runtime, 0) ||
        !aria_state_read(&runtime, &after, error, error_size)) {
        set_error(error, error_size, "Aria preview frame execution failed");
        goto cleanup;
    }
    if (!make_capture_path(path, sizeof(path), output_directory,
                           "aria-after.bmp", error, error_size) ||
        !save_frame(&runtime, path, error, error_size))
        goto cleanup;
    {
        unsigned context_frame;
        for (context_frame = 0; context_frame < 180; ++context_frame) {
            uint16_t keys = (context_frame + 1) % 90 == 0 ? (1u << 0) : 0;
            if (!gba_runtime_step(&runtime, keys)) {
                set_error(error, error_size,
                          "Aria context frame execution failed");
                goto cleanup;
            }
        }
        if (!aria_state_read(&runtime, &context, error, error_size))
            goto cleanup;
        if (!make_capture_path(path, sizeof(path), output_directory,
                               "aria-context.bmp", error, error_size) ||
            !save_frame(&runtime, path, error, error_size))
            goto cleanup;
    }
    printf("Arrival preview: Aria room=%u:%u before=%u,%u injected=%u,%u "
           "after-one-frame=%u,%u\n",
           before.area, before.room,
           before.x_position_fixed / 65536, before.y_position_fixed / 65536,
           immediate.x_position_fixed / 65536,
           immediate.y_position_fixed / 65536,
           after.x_position_fixed / 65536, after.y_position_fixed / 65536);
    printf("Arrival preview context: Aria after 180 additional frames "
           "position=%u,%u control=%s\n",
           context.x_position_fixed / 65536,
           context.y_position_fixed / 65536,
           context.player_control_enabled ? "enabled" : "disabled");
    success = true;

cleanup:
    if (captured) {
        if (!gba_runtime_restore(&runtime, &checkpoint,
                                 restore_error, sizeof(restore_error)) ||
            !aria_state_read(&runtime, &restored,
                             restore_error, sizeof(restore_error)) ||
            restored.x_position_fixed != before.x_position_fixed ||
            restored.y_position_fixed != before.y_position_fixed) {
            set_error_detail(error, error_size, "Aria preview rollback failed",
                             restore_error[0] ? restore_error : "state mismatch");
            success = false;
        }
    }
    if (success) puts("Arrival preview rollback: Aria verified");
    gba_runtime_snapshot_dispose(&checkpoint);
    gba_runtime_close(&runtime);
    return success;
}

bool authentic_arrival_preview_generate(const char *metroid_path,
                                        const char *aria_path,
                                        const char *output_directory,
                                        char *error, size_t error_size)
{
    if (!metroid_path || !aria_path || !output_directory ||
        !output_directory[0]) {
        set_error(error, error_size, "invalid authentic preview arguments");
        return false;
    }
    if (!ensure_directory("captures", error, error_size) ||
        !ensure_directory(output_directory, error, error_size))
        return false;
    if (!preview_mzm(metroid_path, output_directory, error, error_size) ||
        !preview_aria(aria_path, output_directory, error, error_size))
        return false;
    printf("Arrival preview captures written to %s\n", output_directory);
    set_error(error, error_size, "");
    return true;
}
