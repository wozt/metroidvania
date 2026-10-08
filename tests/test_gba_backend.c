#include "core/backend.h"
#include "core/session.h"
#include "aria/state.h"
#include "mzm/state.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    SessionState session;
    FusionBackend invalid = gba_backend_create((WorldKind)99, "missing.gba");
    FusionBackend missing = gba_backend_create(WORLD_METROID, "missing.gba");
    FusionBackend missing_aria =
        gba_backend_create(WORLD_CASTLEVANIA, "missing.gba");
    FusionBackendSnapshot snapshot = {0};
    MzmStateView state_view;
    AriaStateView aria_state_view;

    session_init(&session);
    assert(invalid.failed);
    assert(invalid.error[0]);
    invalid.ops->shutdown(&invalid);

    assert(!missing.failed);
    assert(!missing.ops->init(&missing, &session));
    assert(missing.failed);
    assert(missing.error[0]);
    assert(!missing.ops->capture_state(&missing, &snapshot));
    assert(!gba_backend_read_mzm_state(&missing, &state_view));
    assert(!gba_backend_read_mzm_state(&invalid, &state_view));
    assert(!gba_backend_read_aria_state(&missing, &aria_state_view));
    assert(!gba_backend_read_aria_state(&invalid, &aria_state_view));
    missing.ops->shutdown(&missing);
    missing.ops->shutdown(&missing);

    assert(!missing_aria.failed);
    assert(!gba_backend_read_aria_state(&missing_aria, &aria_state_view));
    assert(missing_aria.error[0]);
    assert(!missing_aria.ops->init(&missing_aria, &session));
    assert(missing_aria.failed);
    assert(!gba_backend_read_mzm_state(&missing_aria, &state_view));
    missing_aria.ops->shutdown(&missing_aria);

    snapshot.version = FUSION_BACKEND_SNAPSHOT_VERSION;
    snapshot.world = WORLD_CASTLEVANIA;
    snapshot.data = malloc(4);
    snapshot.size = 4;
    assert(snapshot.data);
    fusion_backend_snapshot_dispose(&snapshot);
    fusion_backend_snapshot_dispose(&snapshot);
    assert(!snapshot.data && snapshot.size == 0 && snapshot.version == 0);

    puts("mGBA backend failure-path tests passed.");
    return 0;
}
