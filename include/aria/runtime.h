#ifndef FUSION_ARIA_RUNTIME_H
#define FUSION_ARIA_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The API is independent of SDL, ROM contents and any recompilation tool. */
#define ARIA_FRAME_WIDTH 240u
#define ARIA_FRAME_HEIGHT 160u

typedef struct {
    uint16_t keys_held; /* Native GBA keypad bit layout, active-high here. */
} AriaFrameInput;

typedef struct {
    const uint32_t *rgba8888;
    uint32_t width;
    uint32_t height;
    uint32_t stride_pixels;
} AriaFrameView;

typedef struct AriaRuntime AriaRuntime;

typedef struct {
    bool (*open)(void **context, const char *rom_path);
    bool (*step)(void *context, AriaFrameInput input);
    bool (*frame)(void *context, AriaFrameView *out);
    void (*close)(void *context);
} AriaRuntimeDriver;

struct AriaRuntime {
    const AriaRuntimeDriver *driver;
    void *context;
    bool active;
};

/* Only a separately reviewed, GPL-compatible driver may be passed here. */
bool aria_runtime_open(AriaRuntime *runtime, const AriaRuntimeDriver *driver,
                       const char *locally_validated_rom_path);
bool aria_runtime_step(AriaRuntime *runtime, AriaFrameInput input);
bool aria_runtime_frame(AriaRuntime *runtime, AriaFrameView *out);
void aria_runtime_close(AriaRuntime *runtime);

#endif
