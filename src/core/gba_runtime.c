/* SPDX-License-Identifier: GPL-3.0-only */
#include "gba/runtime.h"

#include <mgba/core/core.h>
#include <mgba/core/log.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(color_t) == sizeof(uint32_t),
               "mGBA must be built with 32-bit color output");

static void discard_log(struct mLogger *logger, int category,
                        enum mLogLevel level, const char *format, va_list args)
{
    (void)logger;
    (void)category;
    (void)level;
    (void)format;
    (void)args;
}

static struct mLogger quiet_logger = {
    .log = discard_log,
    .filter = NULL,
};

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error && error_size) snprintf(error, error_size, "%s", message);
}

bool gba_runtime_open(GbaRuntime *runtime, const char *rom_path,
                      char *error, size_t error_size)
{
    struct mCore *core;
    struct mCoreOptions options = {0};
    unsigned width = 0;
    unsigned height = 0;

    if (!runtime || !rom_path || !rom_path[0]) {
        set_error(error, error_size, "invalid GBA runtime arguments");
        return false;
    }
    if (runtime->core || runtime->pixels) {
        set_error(error, error_size, "GBA runtime is already open");
        return false;
    }

    mLogSetDefaultLogger(&quiet_logger);
    core = mCoreFind(rom_path);
    if (!core) {
        set_error(error, error_size, "mGBA did not recognize the ROM");
        return false;
    }
    runtime->core = core;
    if (!core->init(core)) {
        set_error(error, error_size, "mGBA core initialization failed");
        /* mGBA exposes no destructor for a core whose init call failed. */
        runtime->core = NULL;
        gba_runtime_close(runtime);
        return false;
    }
    runtime->core_initialized = true;

    core->desiredVideoDimensions(core, &width, &height);
    if (width != GBA_FRAME_WIDTH || height != GBA_FRAME_HEIGHT) {
        set_error(error, error_size, "mGBA returned an unsupported video size");
        gba_runtime_close(runtime);
        return false;
    }
    runtime->pixels = calloc((size_t)width * height, sizeof(*runtime->pixels));
    if (!runtime->pixels) {
        set_error(error, error_size, "cannot allocate the GBA framebuffer");
        gba_runtime_close(runtime);
        return false;
    }
    runtime->width = width;
    runtime->height = height;
    runtime->stride_pixels = width;
    core->setVideoBuffer(core, (color_t *)runtime->pixels, width);

    if (!mCoreLoadFile(core, rom_path)) {
        set_error(error, error_size, "mGBA could not load the ROM");
        gba_runtime_close(runtime);
        return false;
    }
    runtime->rom_loaded = true;

    mCoreInitConfig(core, "metroidvania-fusion");
    runtime->config_initialized = true;
    mCoreConfigMap(&core->config, &options);
    options.useBios = false;
    options.skipBios = true;
    options.videoSync = false;
    options.audioSync = false;
    options.mute = true;
    options.logLevel = mLOG_FATAL;
    mCoreConfigLoadDefaults(&core->config, &options);
    mCoreLoadConfig(core);
    mCoreConfigFreeOpts(&options);
    core->reset(core);
    set_error(error, error_size, "");
    return true;
}

bool gba_runtime_enter(GbaRuntime *runtime)
{
    if (!runtime || !runtime->core || !runtime->rom_loaded || runtime->active)
        return false;
    runtime->active = true;
    return true;
}

void gba_runtime_leave(GbaRuntime *runtime)
{
    if (runtime) runtime->active = false;
}

bool gba_runtime_step(GbaRuntime *runtime, uint16_t keys_held)
{
    struct mCore *core;
    if (!runtime || !runtime->active || !runtime->core || !runtime->rom_loaded)
        return false;
    core = runtime->core;
    core->setKeys(core, keys_held & 0x03ffu);
    core->runFrame(core);
    return true;
}

bool gba_runtime_frame(const GbaRuntime *runtime, GbaFrameView *out)
{
    if (!runtime || !out || !runtime->active || !runtime->pixels ||
        runtime->width != GBA_FRAME_WIDTH || runtime->height != GBA_FRAME_HEIGHT)
        return false;
    *out = (GbaFrameView){runtime->pixels, runtime->width, runtime->height,
                          runtime->stride_pixels};
    return true;
}

static bool is_wram_range(uint32_t address, size_t size,
                          uint32_t start, uint32_t end)
{
    size_t region_size = (size_t)(end - start);
    return address >= start && size <= region_size &&
           (size_t)(address - start) <= region_size - size;
}

bool gba_runtime_read_memory(const GbaRuntime *runtime, uint32_t address,
                             void *out, size_t size)
{
    struct mCore *core;
    uint8_t *bytes = out;
    size_t index;
    if (!runtime || !runtime->active || !runtime->core || !runtime->rom_loaded ||
        !out || !size || size > GBA_MEMORY_READ_MAX_SIZE ||
        (!is_wram_range(address, size, UINT32_C(0x02000000), UINT32_C(0x02040000)) &&
         !is_wram_range(address, size, UINT32_C(0x03000000), UINT32_C(0x03008000))))
        return false;
    core = runtime->core;
    for (index = 0; index < size; ++index)
        bytes[index] = (uint8_t)core->busRead8(core, address + (uint32_t)index);
    return true;
}

bool gba_runtime_capture(GbaRuntime *runtime, GbaRuntimeSnapshot *snapshot,
                         char *error, size_t error_size)
{
    struct mCore *core;
    size_t state_size;
    uint8_t *data;
    if (!runtime || !snapshot || snapshot->data || snapshot->size ||
        !runtime->active || !runtime->core || !runtime->rom_loaded) {
        set_error(error, error_size, "invalid GBA snapshot capture state");
        return false;
    }
    core = runtime->core;
    state_size = core->stateSize(core);
    if (!state_size || state_size > GBA_SNAPSHOT_MAX_SIZE) {
        set_error(error, error_size, "mGBA returned an invalid snapshot size");
        return false;
    }
    data = malloc(state_size);
    if (!data) {
        set_error(error, error_size, "cannot allocate the GBA snapshot");
        return false;
    }
    if (!core->saveState(core, data)) {
        free(data);
        set_error(error, error_size, "mGBA snapshot capture failed");
        return false;
    }
    snapshot->data = data;
    snapshot->size = state_size;
    set_error(error, error_size, "");
    return true;
}

bool gba_runtime_restore(GbaRuntime *runtime,
                         const GbaRuntimeSnapshot *snapshot,
                         char *error, size_t error_size)
{
    struct mCore *core;
    size_t state_size;
    if (!runtime || !snapshot || !snapshot->data || !snapshot->size ||
        snapshot->size > GBA_SNAPSHOT_MAX_SIZE || !runtime->active ||
        !runtime->core || !runtime->rom_loaded) {
        set_error(error, error_size, "invalid GBA snapshot restore state");
        return false;
    }
    core = runtime->core;
    state_size = core->stateSize(core);
    if (snapshot->size != state_size) {
        set_error(error, error_size, "GBA snapshot size does not match the runtime");
        return false;
    }
    if (!core->loadState(core, snapshot->data)) {
        set_error(error, error_size, "mGBA snapshot restore failed");
        return false;
    }
    set_error(error, error_size, "");
    return true;
}

void gba_runtime_snapshot_dispose(GbaRuntimeSnapshot *snapshot)
{
    if (!snapshot) return;
    free(snapshot->data);
    snapshot->data = NULL;
    snapshot->size = 0;
}

void gba_runtime_close(GbaRuntime *runtime)
{
    struct mCore *core;
    if (!runtime) return;
    core = runtime->core;
    runtime->active = false;
    if (core) {
        if (runtime->rom_loaded) core->unloadROM(core);
        if (runtime->config_initialized) mCoreConfigDeinit(&core->config);
        if (runtime->core_initialized) core->deinit(core);
    }
    free(runtime->pixels);
    memset(runtime, 0, sizeof(*runtime));
}
