#ifndef FUSION_CORE_BACKEND_H
#define FUSION_CORE_BACKEND_H

#include <SDL3/SDL.h>

#include <stddef.h>

#include "core/types.h"

typedef struct FusionBackend FusionBackend;
typedef struct MzmStateView MzmStateView;

#define FUSION_BACKEND_SNAPSHOT_VERSION 1u

typedef struct {
    uint32_t version;
    WorldKind world;
    uint8_t *data;
    size_t size;
} FusionBackendSnapshot;

/* Snapshot callbacks are optional. Capture requires a zero-initialized output. */
typedef struct {
    bool (*init)(FusionBackend *backend, SessionState *session);
    bool (*enter_world)(FusionBackend *backend, SessionState *session);
    void (*tick)(FusionBackend *backend, SessionState *session,
                 const FusionInput *input, float dt);
    void (*render)(FusionBackend *backend, const SessionState *session,
                   SDL_Renderer *renderer, bool debug_overlay, float fps);
    bool (*capture_state)(FusionBackend *backend, FusionBackendSnapshot *out);
    bool (*restore_state)(FusionBackend *backend,
                          const FusionBackendSnapshot *snapshot);
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
bool gba_backend_read_mzm_state(FusionBackend *backend, MzmStateView *out);
/* Disposes snapshot data allocated by a backend; safe to call repeatedly. */
void fusion_backend_snapshot_dispose(FusionBackendSnapshot *snapshot);

#endif
