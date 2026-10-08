#include "core/backend.h"
#include "core/session.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    SessionState session;
    FusionBackend invalid = gba_backend_create((WorldKind)99, "missing.gba");
    FusionBackend missing = gba_backend_create(WORLD_METROID, "missing.gba");

    session_init(&session);
    assert(invalid.failed);
    assert(invalid.error[0]);
    invalid.ops->shutdown(&invalid);

    assert(!missing.failed);
    assert(!missing.ops->init(&missing, &session));
    assert(missing.failed);
    assert(missing.error[0]);
    missing.ops->shutdown(&missing);
    missing.ops->shutdown(&missing);

    puts("mGBA backend failure-path tests passed.");
    return 0;
}
