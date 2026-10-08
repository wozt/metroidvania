#include "core/backend.h"
#include "core/room_sim.h"

#include <stdlib.h>

static bool init(FusionBackend *backend, SessionState *session)
{
    const RoomDefinition room = {
        .title = "METROID TEST ROOM (SIMULATED BACKEND)",
        .background = {12, 24, 42, 255}, .accent = {230, 115, 35, 255},
        .solids = {{0,500,960,40}, {210,420,180,20}, {490,350,170,20},
                   {720,455,34,45}, {0,0,20,540}, {940,0,20,540}},
        .solid_count = 6,
        .hazard = {425,480,50,20}, .target = {805,430,38,70}, .portal = {890,390,30,110},
        .gravity = 1250.0f,
        .move_speed = {250.0f, 205.0f}, .jump_speed = {510.0f, 450.0f},
        .actor_width = {26.0f, 24.0f}, .actor_height = {48.0f, 44.0f},
        .attack_range = {240.0f, 75.0f}
    };
    RoomRuntime *runtime = calloc(1, sizeof(*runtime));
    (void)session;
    if (!runtime) return false;
    room_runtime_init(runtime, &room);
    backend->state = runtime;
    return true;
}

static bool enter(FusionBackend *backend, SessionState *session)
{ room_enter(backend->state, &session->worlds[WORLD_METROID]); return true; }
static void tick(FusionBackend *backend, SessionState *session, const FusionInput *input, float dt)
{ room_tick(backend->state, session, input, dt); }
static void render(FusionBackend *backend, const SessionState *session, SDL_Renderer *renderer, bool debug, float fps)
{ room_render(backend->state, session, renderer, debug, fps); }
static void leave(FusionBackend *backend, SessionState *session)
{ room_leave(backend->state, &session->worlds[WORLD_METROID]); }
static void shutdown(FusionBackend *backend) { free(backend->state); backend->state = NULL; }

FusionBackend metroid_backend_create(void)
{
    static const FusionBackendOps ops = {init, enter, tick, render, leave, shutdown};
    return (FusionBackend){"Simulated Metroid backend", WORLD_METROID, &ops, NULL};
}
