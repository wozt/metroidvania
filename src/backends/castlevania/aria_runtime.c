#include "aria/runtime.h"

#include <string.h>

bool aria_runtime_open(AriaRuntime *runtime, const AriaRuntimeDriver *driver,
                       const char *rom_path)
{
    void *context = NULL;
    if (!runtime || runtime->active || !driver || !driver->open ||
        !driver->step || !driver->frame || !driver->close ||
        !rom_path || !rom_path[0])
        return false;
    if (!driver->open(&context, rom_path)) {
        if (context)
            driver->close(context);
        return false;
    }
    if (!context)
        return false;
    runtime->driver = driver;
    runtime->context = context;
    runtime->active = true;
    return true;
}

bool aria_runtime_step(AriaRuntime *runtime, AriaFrameInput input)
{
    return runtime && runtime->active && runtime->driver->step(runtime->context, input);
}

bool aria_runtime_frame(AriaRuntime *runtime, AriaFrameView *out)
{
    AriaFrameView candidate = {0};
    if (out)
        memset(out, 0, sizeof(*out));
    if (!runtime || !runtime->active || !out ||
        !runtime->driver->frame(runtime->context, &candidate))
        return false;
    if (!candidate.rgba8888 || candidate.width != ARIA_FRAME_WIDTH ||
        candidate.height != ARIA_FRAME_HEIGHT ||
        candidate.stride_pixels < candidate.width)
        return false;
    *out = candidate;
    return true;
}

void aria_runtime_close(AriaRuntime *runtime)
{
    if (!runtime || !runtime->active)
        return;
    runtime->driver->close(runtime->context);
    runtime->driver = NULL;
    runtime->context = NULL;
    runtime->active = false;
}
