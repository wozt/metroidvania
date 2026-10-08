#include "gba/runtime.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    GbaRuntime runtime = {0};
    GbaFrameView frame;
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
    gba_runtime_leave(&runtime);
    gba_runtime_close(&runtime);
    gba_runtime_close(&runtime);

    puts("GBA runtime failure-path tests passed.");
    return 0;
}
