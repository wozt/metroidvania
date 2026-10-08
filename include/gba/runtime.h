#ifndef FUSION_GBA_RUNTIME_H
#define FUSION_GBA_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GBA_FRAME_WIDTH 240u
#define GBA_FRAME_HEIGHT 160u
#define GBA_SNAPSHOT_MAX_SIZE (16u * 1024u * 1024u)
#define GBA_MEMORY_READ_MAX_SIZE 4096u
#define GBA_MEMORY_WRITE_MAX_SIZE 16u

typedef struct {
    const uint32_t *rgba32;
    uint32_t width;
    uint32_t height;
    uint32_t stride_pixels;
} GbaFrameView;

typedef struct {
    uint8_t *data;
    size_t size;
} GbaRuntimeSnapshot;

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
bool gba_runtime_read_memory(const GbaRuntime *runtime, uint32_t address,
                             void *out, size_t size);
/* Capture requires a zero-initialized snapshot and transfers ownership to it. */
bool gba_runtime_capture(GbaRuntime *runtime, GbaRuntimeSnapshot *snapshot,
                         char *error, size_t error_size);
/* Diagnostic writes require a matching in-memory checkpoint for rollback. */
bool gba_runtime_write_memory_checkpointed(
    GbaRuntime *runtime, const GbaRuntimeSnapshot *checkpoint,
    uint32_t address, const void *data, size_t size);
bool gba_runtime_restore(GbaRuntime *runtime,
                         const GbaRuntimeSnapshot *snapshot,
                         char *error, size_t error_size);
void gba_runtime_snapshot_dispose(GbaRuntimeSnapshot *snapshot);
void gba_runtime_close(GbaRuntime *runtime);

#endif
