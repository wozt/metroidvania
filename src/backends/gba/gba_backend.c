/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/backend.h"
#include "aria/state.h"
#include "gba/runtime.h"
#include "mzm/state.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    const char *rom_path;
    GbaRuntime runtime;
    SDL_Texture *texture;
} GbaBackendState;

static void fail(FusionBackend *backend, const char *message)
{
    backend->failed = true;
    snprintf(backend->error, sizeof(backend->error), "%s", message);
}

static bool init(FusionBackend *backend, SessionState *session)
{
    GbaBackendState *state = backend->state;
    (void)session;
    if (!state) {
        fail(backend, "cannot allocate the mGBA backend state");
        return false;
    }
    if (!gba_runtime_open(&state->runtime, state->rom_path,
                          backend->error, sizeof(backend->error))) {
        backend->failed = true;
        return false;
    }
    return true;
}

static bool enter(FusionBackend *backend, SessionState *session)
{
    GbaBackendState *state = backend->state;
    (void)session;
    if (!state || !gba_runtime_enter(&state->runtime)) {
        fail(backend, "cannot enter the mGBA backend");
        return false;
    }
    return true;
}

static uint16_t input_keys(const FusionInput *input)
{
    return (uint16_t)(
        (input->jump_held ? (1u << 0) : 0u) |
        (input->attack_held ? (1u << 1) : 0u) |
        (input->select_held ? (1u << 2) : 0u) |
        (input->start_held ? (1u << 3) : 0u) |
        (input->right ? (1u << 4) : 0u) |
        (input->left ? (1u << 5) : 0u) |
        (input->up ? (1u << 6) : 0u) |
        (input->down ? (1u << 7) : 0u) |
        (input->right_shoulder_held ? (1u << 8) : 0u) |
        (input->left_shoulder_held ? (1u << 9) : 0u));
}

static void tick(FusionBackend *backend, SessionState *session,
                 const FusionInput *input, float dt)
{
    GbaBackendState *state = backend->state;
    (void)session;
    (void)dt;
    if (!backend->failed &&
        (!state || !input ||
         !gba_runtime_step(&state->runtime, input_keys(input))))
        fail(backend, "mGBA frame execution failed");
}

static bool present(GbaBackendState *state, SDL_Renderer *renderer,
                    const GbaFrameView *frame)
{
    int output_width;
    int output_height;
    float scale;
    SDL_FRect destination;

    if (!state->texture) {
        state->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           (int)GBA_FRAME_WIDTH,
                                           (int)GBA_FRAME_HEIGHT);
        if (!state->texture) return false;
        SDL_SetTextureScaleMode(state->texture, SDL_SCALEMODE_NEAREST);
    }
    if (!SDL_GetRenderOutputSize(renderer, &output_width, &output_height))
        return false;
    scale = SDL_min((float)output_width / GBA_FRAME_WIDTH,
                    (float)output_height / GBA_FRAME_HEIGHT);
    destination.w = GBA_FRAME_WIDTH * scale;
    destination.h = GBA_FRAME_HEIGHT * scale;
    destination.x = ((float)output_width - destination.w) * 0.5f;
    destination.y = ((float)output_height - destination.h) * 0.5f;
    return SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) &&
           SDL_RenderClear(renderer) &&
           SDL_UpdateTexture(state->texture, NULL, frame->rgba32,
                             (int)(frame->stride_pixels * sizeof(uint32_t))) &&
           SDL_RenderTexture(renderer, state->texture, NULL, &destination);
}

static void render(FusionBackend *backend, const SessionState *session,
                   SDL_Renderer *renderer, bool debug_overlay, float fps)
{
    GbaBackendState *state = backend->state;
    GbaFrameView frame;
    (void)session;
    (void)debug_overlay;
    (void)fps;
    if (!backend->failed &&
        (!state || !renderer ||
         !gba_runtime_frame(&state->runtime, &frame) ||
         !present(state, renderer, &frame)))
        fail(backend, "mGBA framebuffer presentation failed");
}

static bool capture_state(FusionBackend *backend, FusionBackendSnapshot *out)
{
    GbaBackendState *state = backend->state;
    GbaRuntimeSnapshot runtime_snapshot = {0};
    if (!state || !out || out->data || out->size || backend->failed) {
        snprintf(backend->error, sizeof(backend->error),
                 "invalid backend snapshot capture state");
        return false;
    }
    if (!gba_runtime_capture(&state->runtime, &runtime_snapshot,
                             backend->error, sizeof(backend->error)))
        return false;
    out->version = FUSION_BACKEND_SNAPSHOT_VERSION;
    out->world = backend->world;
    out->data = runtime_snapshot.data;
    out->size = runtime_snapshot.size;
    return true;
}

static bool restore_state(FusionBackend *backend,
                          const FusionBackendSnapshot *snapshot)
{
    GbaBackendState *state = backend->state;
    GbaRuntimeSnapshot runtime_snapshot;
    if (!state || !snapshot || snapshot->version != FUSION_BACKEND_SNAPSHOT_VERSION ||
        snapshot->world != backend->world || !snapshot->data || !snapshot->size ||
        backend->failed) {
        snprintf(backend->error, sizeof(backend->error),
                 "backend snapshot does not match the active world");
        return false;
    }
    runtime_snapshot.data = snapshot->data;
    runtime_snapshot.size = snapshot->size;
    return gba_runtime_restore(&state->runtime, &runtime_snapshot,
                               backend->error, sizeof(backend->error));
}

static void leave(FusionBackend *backend, SessionState *session)
{
    GbaBackendState *state = backend->state;
    (void)session;
    if (state) gba_runtime_leave(&state->runtime);
}

static void shutdown(FusionBackend *backend)
{
    GbaBackendState *state = backend->state;
    if (state) {
        if (state->texture) SDL_DestroyTexture(state->texture);
        gba_runtime_close(&state->runtime);
        free(state);
    }
    backend->state = NULL;
}

FusionBackend gba_backend_create(WorldKind world, const char *rom_path)
{
    static const FusionBackendOps ops = {
        .init = init,
        .enter_world = enter,
        .tick = tick,
        .render = render,
        .capture_state = capture_state,
        .restore_state = restore_state,
        .leave_world = leave,
        .shutdown = shutdown,
    };
    GbaBackendState *state;
    FusionBackend backend = {
        .name = world == WORLD_METROID ? "Authentic Metroid mGBA backend"
                                       : "Authentic Castlevania mGBA backend",
        .world = world,
        .ops = &ops,
    };

    if ((world != WORLD_METROID && world != WORLD_CASTLEVANIA) ||
        !rom_path || !rom_path[0]) {
        fail(&backend, "invalid mGBA backend configuration");
        return backend;
    }
    state = calloc(1, sizeof(*state));
    if (!state) {
        fail(&backend, "cannot allocate the mGBA backend state");
        return backend;
    }
    state->rom_path = rom_path;
    backend.state = state;
    return backend;
}

bool gba_backend_read_mzm_state(FusionBackend *backend, MzmStateView *out)
{
    GbaBackendState *state;
    if (!backend || backend->world != WORLD_METROID || !backend->state ||
        !out || backend->failed) {
        if (backend)
            snprintf(backend->error, sizeof(backend->error),
                     "invalid MZM state view request");
        return false;
    }
    state = backend->state;
    return mzm_state_read(&state->runtime, out,
                          backend->error, sizeof(backend->error));
}

bool gba_backend_read_aria_state(FusionBackend *backend, AriaStateView *out)
{
    GbaBackendState *state;
    if (!backend || backend->world != WORLD_CASTLEVANIA || !backend->state ||
        !out || backend->failed) {
        if (backend)
            snprintf(backend->error, sizeof(backend->error),
                     "invalid Aria state view request");
        return false;
    }
    state = backend->state;
    return aria_state_read(&state->runtime, out,
                           backend->error, sizeof(backend->error));
}
