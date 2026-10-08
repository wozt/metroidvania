/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/authentic_interactive.h"

#include "aria/state.h"
#include "core/roundtrip_guard.h"
#include "core/transition.h"
#include "gba/runtime.h"
#include "gba/transition_target.h"
#include "mzm/state.h"

#include <SDL3/SDL.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { WINDOW_WIDTH = 960, WINDOW_HEIGHT = 640, WORLD_COUNT = 2 };

typedef struct {
    GbaRuntime runtimes[WORLD_COUNT];
    GbaTransitionTarget targets[WORLD_COUNT];
    GbaRuntimeSnapshot paused_snapshots[WORLD_COUNT];
    MzmStateView paused_mzm;
    AriaStateView paused_aria;
    bool paused[WORLD_COUNT];
    bool visited[WORLD_COUNT];
    WorldKind active;
} AuthenticInteractive;

static void set_error(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
}

static const char *world_label(WorldKind world)
{
    return world == WORLD_METROID ? "Metroid: Zero Mission" :
           "Castlevania: Aria of Sorrow";
}

static bool read_source(AuthenticInteractive *session, WorldKind world,
                        FusionTransitionObservation *out,
                        MzmStateView *mzm, AriaStateView *aria,
                        char *error, size_t error_size)
{
    if (world == WORLD_METROID)
        return mzm_state_read(&session->runtimes[world], mzm,
                              error, error_size) &&
               fusion_transition_observe_mzm(mzm, out);
    return aria_state_read(&session->runtimes[world], aria,
                           error, error_size) &&
           fusion_transition_observe_aria(aria, out);
}

/* The already bootstrapped native engines remain allocated and retain their
 * private emulated RAM while suspended. Only one may execute a frame. */
static bool bootstrap(AuthenticInteractive *session, WorldKind world,
                      char *error, size_t error_size)
{
    GbaRuntime *runtime = &session->runtimes[world];
    bool ok;
    if (!gba_runtime_enter(runtime)) {
        set_error(error, error_size, "cannot enter native runtime bootstrap");
        return false;
    }
    ok = gba_transition_target_bootstrap(&session->targets[world]);
    if (!ok) set_error(error, error_size, session->targets[world].error);
    gba_runtime_leave(runtime);
    return ok;
}

static bool metroid_unchanged(const MzmStateView *before,
                              const MzmStateView *after)
{
    return before->game_mode == after->game_mode &&
           before->gameplay_state_ready == after->gameplay_state_ready &&
           before->area == after->area && before->room == after->room &&
           before->last_door == after->last_door &&
           before->x_subpixels == after->x_subpixels &&
           before->y_subpixels == after->y_subpixels &&
           before->current_energy == after->current_energy &&
           before->max_energy == after->max_energy;
}

static bool resume_matches(AuthenticInteractive *session, WorldKind world,
                           char *error, size_t error_size)
{
    GbaRuntimeSnapshot actual = {0};
    bool valid = false;
    if (!session->paused[world] || !session->paused_snapshots[world].data) {
        set_error(error, error_size, "destination has no paused checkpoint");
        return false;
    }
    if (world == WORLD_METROID) {
        MzmStateView state;
        valid = mzm_state_read(&session->runtimes[world], &state,
                               error, error_size) &&
                metroid_unchanged(&session->paused_mzm, &state) &&
                gba_runtime_capture(&session->runtimes[world], &actual,
                                    error, error_size) &&
                fusion_roundtrip_snapshot_equal(
                    &session->paused_snapshots[world], &actual);
    } else {
        AriaStateView state;
        /* The Aria decoder compares named game fields; comparing serialized
         * states after loadState was disproved by the ROM-backed roundtrip. */
        valid = aria_state_read(&session->runtimes[world], &state,
                                error, error_size) &&
                fusion_roundtrip_aria_view_equal(&session->paused_aria, &state);
    }
    gba_runtime_snapshot_dispose(&actual);
    if (!valid && (!error || !error_size || !error[0]))
        set_error(error, error_size, "suspended world changed during handoff");
    return valid;
}

typedef enum { SWITCH_OK, SWITCH_REJECTED, SWITCH_FATAL } SwitchResult;

static SwitchResult switch_world(AuthenticInteractive *session,
                                 char *error, size_t error_size)
{
    const WorldKind from = session->active;
    const WorldKind to = from == WORLD_METROID
        ? WORLD_CASTLEVANIA : WORLD_METROID;
    GbaRuntimeSnapshot source_checkpoint = {0};
    FusionTransitionObservation observation = {0};
    FusionTransitionPlan plan = {0};
    MzmStateView source_mzm = {0};
    AriaStateView source_aria = {0};
    FusionRoundtripSwitchAction action;
    FusionTransitionTransactionResult transaction;
    bool destination_entered = false;
    bool source_left = false;
    bool fatal = false;

    if (!fusion_roundtrip_exclusive(&session->runtimes[from],
                                    &session->runtimes[to])) {
        set_error(error, error_size, "world handoff requires exclusive runtime");
        return SWITCH_FATAL;
    }
    action = fusion_roundtrip_switch_action(&session->runtimes[from],
                                            &session->runtimes[to],
                                            session->visited[to]);
    if (action == FUSION_ROUNDTRIP_SWITCH_REJECTED ||
        !read_source(session, from, &observation, &source_mzm, &source_aria,
                     error, error_size) ||
        !gba_runtime_capture(&session->runtimes[from], &source_checkpoint,
                             error, error_size)) {
        if (!error[0]) set_error(error, error_size,
                                 "source is not gameplay-ready for handoff");
        goto rejected;
    }
    if (action == FUSION_ROUNDTRIP_SWITCH_ARRIVAL &&
        !fusion_transition_plan_build(&observation, to, &plan)) {
        set_error(error, error_size, "cannot construct destination arrival plan");
        goto rejected;
    }

    gba_runtime_leave(&session->runtimes[from]);
    source_left = true;
    if (!gba_runtime_enter(&session->runtimes[to])) {
        set_error(error, error_size, "cannot activate destination runtime");
        goto rejected;
    }
    destination_entered = true;
    if (!fusion_roundtrip_exclusive(&session->runtimes[to],
                                    &session->runtimes[from])) {
        set_error(error, error_size, "overlapping native execution rejected");
        fatal = true;
        goto rejected;
    }

    if (action == FUSION_ROUNDTRIP_SWITCH_RESUME) {
        if (!resume_matches(session, to, error, error_size))
            goto rejected;
        printf("Interactive: resumed %s without overwriting its state\n",
               world_label(to));
    } else {
        transaction = fusion_transition_apply_transaction(
            &plan, gba_transition_target_ops(), &session->targets[to]);
        if (transaction != FUSION_TRANSITION_TRANSACTION_COMMITTED) {
            snprintf(error, error_size,
                     "arrival transaction %d: %.160s",
                     (int)transaction, session->targets[to].error);
            if (transaction == FUSION_TRANSITION_TRANSACTION_ROLLBACK_FAILED)
                fatal = true;
            goto rejected;
        }
        printf("Interactive: first arrival %s room=%u:%u health=%d/%d "
               "(native character, no guest swap)\n",
               world_label(to), plan.target_area, plan.target_room,
               plan.target_health, plan.target_max_health);
    }

    /* Commit only AFTER the target has been verified. The source was never
     * stepped after its checkpoint, and its state remains resident in mGBA. */
    gba_runtime_snapshot_dispose(&session->paused_snapshots[from]);
    session->paused_snapshots[from] = source_checkpoint;
    source_checkpoint.data = NULL;
    source_checkpoint.size = 0;
    session->paused[from] = true;
    if (from == WORLD_METROID)
        session->paused_mzm = source_mzm;
    else
        session->paused_aria = source_aria;

    if (action == FUSION_ROUNDTRIP_SWITCH_RESUME) {
        gba_runtime_snapshot_dispose(&session->paused_snapshots[to]);
        session->paused[to] = false;
    }
    session->visited[to] = true;
    session->active = to;
    printf("Interactive: active=%s, other world suspended\n", world_label(to));
    fflush(stdout);
    return SWITCH_OK;

rejected:
    if (destination_entered) gba_runtime_leave(&session->runtimes[to]);
    if (source_left) {
        MzmStateView check_mzm = {0};
        AriaStateView check_aria = {0};
        FusionTransitionObservation check_observation = {0};
        if (!gba_runtime_enter(&session->runtimes[from]) ||
            !read_source(session, from, &check_observation,
                         &check_mzm, &check_aria, error, error_size) ||
            (from == WORLD_METROID &&
             !metroid_unchanged(&source_mzm, &check_mzm)) ||
            (from == WORLD_CASTLEVANIA &&
             !fusion_roundtrip_aria_view_equal(&source_aria, &check_aria)))
            fatal = true;
    }
    gba_runtime_snapshot_dispose(&source_checkpoint);
    if (fatal) {
        set_error(error, error_size,
                  "fatal handoff failure: rollback/resume integrity unverified");
        return SWITCH_FATAL;
    }
    return SWITCH_REJECTED;
}

static uint16_t keyboard_keys(const bool *keys)
{
    return (uint16_t)(
        ((keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_X]) ? 1u : 0u) |
        ((keys[SDL_SCANCODE_J] || keys[SDL_SCANCODE_Z]) ? (1u << 1) : 0u) |
        (keys[SDL_SCANCODE_RSHIFT] ? (1u << 2) : 0u) |
        (keys[SDL_SCANCODE_RETURN] ? (1u << 3) : 0u) |
        ((keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) ? (1u << 4) : 0u) |
        ((keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_Q] ||
          keys[SDL_SCANCODE_A]) ? (1u << 5) : 0u) |
        (keys[SDL_SCANCODE_UP] ? (1u << 6) : 0u) |
        (keys[SDL_SCANCODE_DOWN] ? (1u << 7) : 0u) |
        (keys[SDL_SCANCODE_I] ? (1u << 8) : 0u) |
        (keys[SDL_SCANCODE_U] ? (1u << 9) : 0u));
}

static bool draw_gba(SDL_Renderer *renderer, SDL_Texture *texture,
                     GbaRuntime *runtime, char *error, size_t error_size)
{
    GbaFrameView frame;
    SDL_FRect rect;
    int width = 0, height = 0;
    float x_scale, y_scale, scale;
    if (!gba_runtime_frame(runtime, &frame) ||
        !SDL_GetRenderOutputSize(renderer, &width, &height)) {
        set_error(error, error_size, "cannot obtain active GBA frame/output size");
        return false;
    }
    x_scale = (float)width / GBA_FRAME_WIDTH;
    y_scale = (float)height / GBA_FRAME_HEIGHT;
    scale = x_scale < y_scale ? x_scale : y_scale;
    rect.w = GBA_FRAME_WIDTH * scale;
    rect.h = GBA_FRAME_HEIGHT * scale;
    rect.x = ((float)width - rect.w) * 0.5f;
    rect.y = ((float)height - rect.h) * 0.5f;
    if (!SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255) ||
        !SDL_RenderClear(renderer) ||
        !SDL_UpdateTexture(texture, NULL, frame.rgba32,
                           (int)(frame.stride_pixels * sizeof(uint32_t))) ||
        !SDL_RenderTexture(renderer, texture, NULL, &rect) ||
        !SDL_RenderPresent(renderer)) {
        set_error(error, error_size, SDL_GetError());
        return false;
    }
    return true;
}

bool authentic_interactive_run(const char *metroid_path, const char *aria_path,
                              char *error, size_t error_size)
{
    AuthenticInteractive session = {0};
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    bool initialized_sdl = false;
    bool running = true;
    bool success = false;
    unsigned world;

    if (!error || !error_size || !metroid_path || !aria_path) return false;
    error[0] = '\0';
    session.active = WORLD_METROID;
    session.visited[WORLD_METROID] = true;
    if (!gba_runtime_open(&session.runtimes[WORLD_METROID], metroid_path,
                          error, error_size) ||
        !gba_runtime_open(&session.runtimes[WORLD_CASTLEVANIA], aria_path,
                          error, error_size))
        goto cleanup;
    for (world = 0; world < WORLD_COUNT; ++world)
        gba_transition_target_init(&session.targets[world],
                                   &session.runtimes[world], (WorldKind)world);
    puts("Interactive: bootstrapping verified native rooms; no guest swap");
    if (!bootstrap(&session, WORLD_METROID, error, error_size) ||
        !bootstrap(&session, WORLD_CASTLEVANIA, error, error_size) ||
        !gba_runtime_enter(&session.runtimes[WORLD_METROID]) ||
        !fusion_roundtrip_exclusive(&session.runtimes[WORLD_METROID],
                                    &session.runtimes[WORLD_CASTLEVANIA])) {
        if (!error[0]) set_error(error, error_size,
                                 "interactive initial runtime activation failed");
        goto cleanup;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        set_error(error, error_size, SDL_GetError());
        goto cleanup;
    }
    initialized_sdl = true;
    window = SDL_CreateWindow("Metroidvania Fusion - authentic interactive",
                              WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           (int)GBA_FRAME_WIDTH,
                                           (int)GBA_FRAME_HEIGHT) : NULL;
    if (!window || !renderer || !texture ||
        !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
        set_error(error, error_size, SDL_GetError());
        goto cleanup;
    }
    (void)SDL_SetRenderVSync(renderer, 1);
    SDL_SetWindowTitle(window,
                       "Metroidvania Fusion - Metroid (M: switch, Esc: quit)");
    puts("Interactive: ready. M switches native world; Esc quits; "
         "arrows move, Space=A, J=B, Enter=Start, Shift=Select.");
    fflush(stdout);

    while (running) {
        SDL_Event event;
        bool switch_requested = false;
        const bool *keys;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_ESCAPE) running = false;
                if (event.key.key == SDLK_M) switch_requested = true;
            }
        }
        if (!running) break;
        if (switch_requested) {
            SwitchResult result = switch_world(&session, error, error_size);
            if (result == SWITCH_FATAL) goto cleanup;
            if (result == SWITCH_REJECTED) {
                fprintf(stderr, "Interactive: switch rejected: %s\n", error);
                error[0] = '\0';
            } else {
                SDL_SetWindowTitle(window, session.active == WORLD_METROID
                    ? "Metroidvania Fusion - Metroid (M: switch, Esc: quit)"
                    : "Metroidvania Fusion - Aria (native Soma; M: switch)");
            }
            /* Never feed the switch key to either emulated GBA. */
        }
        keys = SDL_GetKeyboardState(NULL);
        if (!gba_runtime_step(&session.runtimes[session.active],
                              keyboard_keys(keys)) ||
            !draw_gba(renderer, texture, &session.runtimes[session.active],
                      error, error_size)) {
            if (!error[0]) set_error(error, error_size,
                                     "native interactive frame failed");
            goto cleanup;
        }
        /* Real-time-ish diagnostic preview, not yet a production scheduler. */
        SDL_Delay(16);
    }
    success = true;
    set_error(error, error_size, "");

cleanup:
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (initialized_sdl) SDL_Quit();
    for (world = 0; world < WORLD_COUNT; ++world) {
        gba_runtime_leave(&session.runtimes[world]);
        gba_runtime_snapshot_dispose(&session.paused_snapshots[world]);
        gba_runtime_close(&session.runtimes[world]);
    }
    return success;
}
