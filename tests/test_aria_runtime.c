#include "aria/runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t pixels[ARIA_FRAME_WIDTH * ARIA_FRAME_HEIGHT];
static int opens, steps, closes;
static uint16_t last_keys;
static int fail_open;
static int bad_frame;

static bool fake_open(void **ctx, const char *path)
{
    (void)path;
    ++opens;
    *ctx = pixels;
    return !fail_open;
}
static bool fake_step(void *ctx, AriaFrameInput input)
{
    assert(ctx == pixels);
    ++steps;
    last_keys = input.keys_held;
    return true;
}
static bool fake_frame(void *ctx, AriaFrameView *out)
{
    assert(ctx == pixels);
    *out = (AriaFrameView){pixels, ARIA_FRAME_WIDTH,
                           ARIA_FRAME_HEIGHT,
                           bad_frame ? 1u : ARIA_FRAME_WIDTH};
    return true;
}
static void fake_close(void *ctx)
{
    assert(ctx == pixels);
    ++closes;
}
static const AriaRuntimeDriver driver = {
    fake_open, fake_step, fake_frame, fake_close
};

int main(void)
{
    AriaRuntime rt = {0};
    AriaFrameView frame;
    const AriaFrameInput input = {0x0041};
    assert(!aria_runtime_step(&rt, input));
    assert(!aria_runtime_frame(&rt, &frame));
    assert(!aria_runtime_open(&rt, NULL, "private.gba"));
    assert(!aria_runtime_open(&rt, &driver, ""));
    fail_open = 1;
    assert(!aria_runtime_open(&rt, &driver, "private.gba"));
    assert(closes == 1);
    fail_open = 0;
    assert(aria_runtime_open(&rt, &driver, "private.gba"));
    assert(!aria_runtime_open(&rt, &driver, "private.gba"));
    assert(aria_runtime_step(&rt, input));
    assert(steps == 1 && last_keys == 0x0041);
    assert(aria_runtime_frame(&rt, &frame));
    assert(frame.width == 240 && frame.height == 160);
    bad_frame = 1;
    memset(&frame, 0xff, sizeof(frame));
    assert(!aria_runtime_frame(&rt, &frame));
    assert(frame.rgba8888 == NULL);
    aria_runtime_close(&rt);
    aria_runtime_close(&rt);
    assert(closes == 2);
    assert(!aria_runtime_step(&rt, input));
    assert(aria_runtime_open(&rt, &driver, "private.gba"));
    aria_runtime_close(&rt);
    assert(closes == 3);
    puts("Aria runtime contract tests passed (fake driver only).");
    return 0;
}
