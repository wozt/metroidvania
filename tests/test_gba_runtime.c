#include "gba/runtime.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    GbaRuntime runtime = {0};
    GbaFrameView frame;
    GbaRuntimeSnapshot snapshot = {0};
    char error[128];

    assert(!gba_runtime_open(NULL, "missing.gba", error, sizeof(error)));
    assert(!gba_runtime_open(&runtime, NULL, error, sizeof(error)));
    assert(!gba_runtime_open(&runtime, "", error, sizeof(error)));
    assert(!gba_runtime_open(&runtime, "this-file-does-not-exist.gba",
                             error, sizeof(error)));
    assert(strstr(error, "recognize") != NULL);
    assert(!gba_runtime_enter(&runtime));
    assert(!gba_runtime_step(&runtime, 0));
    assert(!gba_runtime_frame(&runtime, &frame));
    assert(!gba_runtime_read_memory(&runtime, UINT32_C(0x03000000),
                                    error, 1));
    assert(!gba_runtime_read_memory(&runtime, UINT32_C(0x04000000),
                                    error, 1));
    assert(!gba_runtime_read_memory(&runtime, UINT32_C(0x03000000),
                                    error, GBA_MEMORY_READ_MAX_SIZE + 1u));
    assert(!gba_runtime_capture(&runtime, &snapshot, error, sizeof(error)));
    assert(!gba_runtime_restore(&runtime, &snapshot, error, sizeof(error)));
    snapshot.data = malloc(1);
    snapshot.size = 1;
    assert(snapshot.data);
    gba_runtime_snapshot_dispose(&snapshot);
    gba_runtime_snapshot_dispose(&snapshot);
    assert(!snapshot.data && snapshot.size == 0);
    gba_runtime_leave(&runtime);
    gba_runtime_close(&runtime);
    gba_runtime_close(&runtime);

    puts("GBA runtime failure-path tests passed.");
    return 0;
}
