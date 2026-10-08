#ifndef FUSION_GBA_RUNTIME_H
#define FUSION_GBA_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GBA_FRAME_WIDTH 240u
#define GBA_FRAME_HEIGHT 160u

typedef struct {
    const uint32_t *rgba32;
    uint32_t width;
    uint32_t height;
    uint32_t stride_pixels;
} GbaFrameView;

typedef struct {
    void *core;
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride_pixels;
    bool core_initialized;
    bool config_initialized;
    bool rom_loaded;
    bool active;
} GbaRuntime;

/* Callers must zero-initialize the runtime before its first open. */
bool gba_runtime_open(GbaRuntime *runtime, const char *rom_path,
                      char *error, size_t error_size);
bool gba_runtime_enter(GbaRuntime *runtime);
void gba_runtime_leave(GbaRuntime *runtime);
bool gba_runtime_step(GbaRuntime *runtime, uint16_t keys_held);
bool gba_runtime_frame(const GbaRuntime *runtime, GbaFrameView *out);
void gba_runtime_close(GbaRuntime *runtime);

#endif
