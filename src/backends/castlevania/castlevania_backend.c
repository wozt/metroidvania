#include "core/backend.h"
#include "core/room_sim.h"

#include <stdlib.h>

static bool init(FusionBackend *backend, SessionState *session)
{
    const RoomDefinition room = {
        .title = "CASTLEVANIA TEST ROOM (BACKEND SIMULE)",
        .background = {32, 15, 35, 255}, .accent = {185, 55, 70, 255},
        .solids = {{0,500,960,40}, {170,440,140,20}, {340,390,130,20},
                   {510,340,145,20}, {0,0,20,540}, {940,0,20,540}, {700,470,28,30}},
        .solid_count = 7,
        .hazard = {580,480,60,20}, .target = {790,425,42,75}, .portal = {885,375,35,125},
        .gravity = 1450.0f,
        .move_speed = {190.0f, 215.0f}, .jump_speed = {430.0f, 475.0f},
        .actor_width = {28.0f, 24.0f}, .actor_height = {50.0f, 46.0f},
        .attack_range = {210.0f, 105.0f}
    };
    RoomRuntime *runtime = calloc(1, sizeof(*runtime));
    (void)session;
    if (!runtime) return false;
    room_runtime_init(runtime, &room);
    backend->state = runtime;
    return true;
}

static bool enter(FusionBackend *backend, SessionState *session)
{ room_enter(backend->state, &session->worlds[WORLD_CASTLEVANIA]); return true; }
static void tick(FusionBackend *backend, SessionState *session, const FusionInput *input, float dt)
{ room_tick(backend->state, session, input, dt); }
static void render(FusionBackend *backend, const SessionState *session, SDL_Renderer *renderer, bool debug, float fps)
{ room_render(backend->state, session, renderer, debug, fps); }
static void leave(FusionBackend *backend, SessionState *session)
{ room_leave(backend->state, &session->worlds[WORLD_CASTLEVANIA]); }
static void shutdown(FusionBackend *backend) { free(backend->state); backend->state = NULL; }

FusionBackend castlevania_backend_create(void)
{
    static const FusionBackendOps ops = {init, enter, tick, render, leave, shutdown};
    return (FusionBackend){"Backend Castlevania simulé", WORLD_CASTLEVANIA, &ops, NULL};
}
