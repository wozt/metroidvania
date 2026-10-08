#include "core/backend.h"
#include "core/session.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    SessionState session;
    FusionBackend invalid = gba_backend_create((WorldKind)99, "missing.gba");
    FusionBackend missing = gba_backend_create(WORLD_METROID, "missing.gba");
    FusionBackendSnapshot snapshot = {0};

    session_init(&session);
    assert(invalid.failed);
    assert(invalid.error[0]);
    invalid.ops->shutdown(&invalid);

    assert(!missing.failed);
    assert(!missing.ops->init(&missing, &session));
    assert(missing.failed);
    assert(missing.error[0]);
    assert(!missing.ops->capture_state(&missing, &snapshot));
    missing.ops->shutdown(&missing);
    missing.ops->shutdown(&missing);

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
