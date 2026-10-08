#ifndef FUSION_CORE_BACKEND_H
#define FUSION_CORE_BACKEND_H

#include <SDL3/SDL.h>

#include "core/types.h"

typedef struct FusionBackend FusionBackend;

typedef struct {
    bool (*init)(FusionBackend *backend, SessionState *session);
    bool (*enter_world)(FusionBackend *backend, SessionState *session);
    void (*tick)(FusionBackend *backend, SessionState *session,
                 const FusionInput *input, float dt);
    void (*render)(FusionBackend *backend, const SessionState *session,
                   SDL_Renderer *renderer, bool debug_overlay, float fps);
    void (*leave_world)(FusionBackend *backend, SessionState *session);
    void (*shutdown)(FusionBackend *backend);
} FusionBackendOps;

struct FusionBackend {
    const char *name;
    WorldKind world;
    const FusionBackendOps *ops;
    void *state;
    bool failed;
    char error[160];
};

FusionBackend metroid_backend_create(void);
FusionBackend castlevania_backend_create(void);
FusionBackend gba_backend_create(WorldKind world, const char *rom_path);

#endif
