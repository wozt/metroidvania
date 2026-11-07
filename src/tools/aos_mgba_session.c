/* SPDX-License-Identifier: GPL-3.0-only */
#define _GNU_SOURCE
/* Shared headless mGBA session implementation (see aos_mgba_session.h). */

#include "aos_mgba_session.h"

#include <mgba/core/core.h>
#include <mgba/core/thread.h>
#include <mgba/core/config.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/video.h>
#include <mgba/internal/gba/renderers/video-software.h>
#include <mgba/internal/gba/input.h>
#include <mgba-util/vfs.h>
#include <mgba/core/log.h>

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AosMgbaSession {
    struct mCore *core;
    struct mCoreThread thread;
    struct GBAVideoSoftwareRenderer renderer;
    uint32_t *framebuffer;
    char error[256];
};

static void session_fail(AosMgbaSession *s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s->error, sizeof s->error, fmt, ap);
    va_end(ap);
}

static void null_log(struct mLogger *logger, int category,
                     enum mLogLevel level, const char *format,
                     va_list args) {
    (void)logger; (void)category; (void)level; (void)format; (void)args;
}

static void quiet_mgba_logs(void) {
    static struct mLogger quiet_logger;
    quiet_logger.log = null_log;
    quiet_logger.filter = NULL;
    mLogSetDefaultLogger(&quiet_logger);
}

AosMgbaSession *aos_mgba_open(const char *rom_path, char *err, size_t err_size) {
    quiet_mgba_logs();
    AosMgbaSession *s = calloc(1, sizeof *s);
    if (!s) return NULL;
    s->framebuffer = calloc(AOS_MGBA_WIDTH * AOS_MGBA_HEIGHT, sizeof(uint32_t));
    if (!s->framebuffer) {
        session_fail(s, "cannot allocate framebuffer");
        goto fail;
    }
    s->core = mCoreCreate(mPLATFORM_GBA);
    if (!s->core) {
        session_fail(s, "mCoreCreate failed");
        goto fail;
    }
    mCoreConfigInit(&s->core->config, "gba");
    memset(&s->core->opts, 0, sizeof s->core->opts);
    s->core->opts.fpsTarget = 60.0f;
    if (!s->core->init(s->core)) {
        session_fail(s, "core init failed");
        goto fail;
    }
    struct GBA *gba = (struct GBA *)s->core->board;
    memset(&s->renderer, 0, sizeof s->renderer);
    GBAVideoSoftwareRendererCreate(&s->renderer);
    s->renderer.outputBuffer = s->framebuffer;
    s->renderer.outputBufferStride = AOS_MGBA_WIDTH;
    GBAVideoAssociateRenderer(&gba->video, &s->renderer.d);
    struct VFile *vf = VFileOpen(rom_path, O_RDONLY);
    if (!vf || !s->core->loadROM(s->core, vf)) {
        if (vf) vf->close(vf);
        session_fail(s, "cannot load ROM %s", rom_path);
        goto fail;
    }
    vf->close(vf);
    /* Pick up a battery save sitting next to the ROM (<name>.sav) in
     * temporary mode so trials never rewrite the player's real save. */
    {
        size_t rl = strlen(rom_path);
        char sav[1200];
        if (rl + 5 < sizeof sav) {
            memcpy(sav, rom_path, rl);
            memcpy(sav + rl, ".sav", 5);
            mCoreLoadSaveFile(s->core, sav, true);
        }
    }
    memset(&s->thread, 0, sizeof s->thread);
    s->thread.core = s->core;
    if (!mCoreThreadStart(&s->thread)) {
        session_fail(s, "emulation thread failed to start");
        goto fail;
    }
    return s;

fail:
    if (err && err_size) snprintf(err, err_size, "%s", s->error);
    aos_mgba_close(s);
    return NULL;
}

void aos_mgba_close(AosMgbaSession *s) {
    if (!s) return;
    if (s->thread.core) mCoreThreadEnd(&s->thread);
    free(s->framebuffer);
    free(s);
}

const uint32_t *aos_mgba_framebuffer(const AosMgbaSession *s) {
    return s ? s->framebuffer : NULL;
}

/* The emulation thread accepts one job at a time; all public calls funnel
 * through file-scope job slots. */
struct RunCtx {
    AosMgbaSession *session;
    const AosRunSpec *spec;
    AosRunResult *result;
};
static struct RunCtx g_ctx;

struct BlobJob {
    const void *blob;
    bool *ok;
};
static struct BlobJob g_blob_job;

struct SaveJob {
    uint8_t *blob;
    bool *ok;
};
static struct SaveJob g_save_job;

static void write_ppm(const char *path, const uint32_t *pixels) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", AOS_MGBA_WIDTH, AOS_MGBA_HEIGHT);
    for (int y = 0; y < AOS_MGBA_HEIGHT; ++y) {
        for (int x = 0; x < AOS_MGBA_WIDTH; ++x) {
            uint32_t c = pixels[y * AOS_MGBA_WIDTH + x];
            fputc(c & 0xFF, f);        /* R: low byte (0xAABBGGRR) */
            fputc((c >> 8) & 0xFF, f);
            fputc(c & 0xFF, f);
        }
    }
    fclose(f);
}

static bool is_capture_frame(const AosRunSpec *spec, int frame) {
    if (spec->capture_count == 0) return frame == spec->frames - 1;
    for (int i = 0; i < spec->capture_count; ++i)
        if (spec->capture_frames[i] == frame) return true;
    return false;
}

/* Called on the emulation thread: load/save must go through the core
 * directly here — aos_mgba_load/save_state_blob would deadlock by
 * dispatching another function onto the same thread. */
static bool load_state_file_on_thread(struct mCore *core, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *blob = malloc(size > 0 ? (size_t)size : 1);
    bool ok = blob && fread(blob, 1, (size_t)size, f) == (size_t)size &&
              core->loadState(core, blob);
    fclose(f);
    free(blob);
    return ok;
}

static bool save_state_file_on_thread(struct mCore *core, const char *path) {
    size_t state_size = core->stateSize(core);
    uint8_t *blob = malloc(state_size);
    if (!blob) return false;
    bool ok = core->saveState(core, blob);
    FILE *f = ok ? fopen(path, "wb") : NULL;
    ok = ok && f && fwrite(blob, 1, state_size, f) == state_size;
    if (f) fclose(f);
    free(blob);
    return ok;
}

static void run_on_thread(struct mCoreThread *thread) {
    AosMgbaSession *s = g_ctx.session;
    const AosRunSpec *spec = g_ctx.spec;
    struct mCore *core = thread->core;
    uint32_t keys = 0;
    int key_index = 0;

    if (spec->state_in &&
        !load_state_file_on_thread(core, spec->state_in))
        fprintf(stderr, "warning: state load failed for %s\n",
                spec->state_in);

    if (g_ctx.result && spec->peek_count > 0) {
        g_ctx.result->peeks = calloc(spec->peek_count, sizeof(AosPeekResult));
        g_ctx.result->peek_count = spec->peek_count;
    }

    /* Optional fps cap: sleep so emulation never exceeds spec->fps_cap.
     * 0 means unthrottled (max speed). */
    double frame_budget = spec->fps_cap > 0.0 ? 1.0 / spec->fps_cap : 0.0;
    struct timespec next_frame;
    clock_gettime(CLOCK_MONOTONIC, &next_frame);

    for (int frame = 0; frame < spec->frames; ++frame) {
        while (key_index < spec->key_count &&
               spec->keys[key_index].frame == frame) {
            keys = spec->keys[key_index].mask;
            ++key_index;
        }
        core->setKeys(core, keys);
        core->runFrame(core);
        if (spec->fb_mirror &&
            (!spec->fb_interval || frame % spec->fb_interval == 0)) {
            memcpy(spec->fb_mirror, s->framebuffer,
                   AOS_MGBA_WIDTH * AOS_MGBA_HEIGHT * sizeof(uint32_t));
            if (spec->fb_seq) ++*spec->fb_seq;
        }
        if (frame_budget > 0.0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double ahead = (double)(next_frame.tv_sec - now.tv_sec) +
                           (double)(next_frame.tv_nsec - now.tv_nsec) / 1e9;
            if (ahead > 0.0) {
                struct timespec ts = {
                    .tv_sec = (time_t)ahead,
                    .tv_nsec = (long)((ahead - (time_t)ahead) * 1e9)
                };
                nanosleep(&ts, NULL);
            }
            /* schedule the next slot one budget after the previous one */
            next_frame.tv_nsec += (long)(frame_budget * 1e9);
            while (next_frame.tv_nsec >= 1000000000L) {
                next_frame.tv_nsec -= 1000000000L;
                ++next_frame.tv_sec;
            }
            clock_gettime(CLOCK_MONOTONIC, &now);
            if ((double)(next_frame.tv_sec - now.tv_sec) +
                (double)(next_frame.tv_nsec - now.tv_nsec) / 1e9 < -1.0) {
                /* fell behind by more than a second: resync */
                next_frame = now;
            }
        }
        if (spec->shot_prefix && is_capture_frame(spec, frame)) {
            char path[1024];
            snprintf(path, sizeof path, "%s_%05d.ppm", spec->shot_prefix,
                     frame);
            write_ppm(path, s->framebuffer);
        }
        /* Sample mid-run peeks whose frame matches. */
        if (g_ctx.result)
            for (int i = 0; i < spec->peek_count; ++i) {
                const AosPeekRequest *req = &spec->peeks[i];
                if (req->frame != frame || g_ctx.result->peeks[i].data)
                    continue;
                size_t avail = 0;
                const uint8_t *data =
                    mCoreGetMemoryBlock(core, req->address, &avail);
                uint32_t want = req->size < avail ? req->size : (uint32_t)avail;
                if (data && want) {
                    g_ctx.result->peeks[i].address = req->address;
                    g_ctx.result->peeks[i].size = want;
                    g_ctx.result->peeks[i].data = malloc(want);
                    memcpy(g_ctx.result->peeks[i].data, data, want);
                }
            }
    }

    /* Post-run peeks (frame < 0) plus any mid-run request whose frame
     * never arrived. */
    if (g_ctx.result)
        for (int i = 0; i < spec->peek_count; ++i) {
            const AosPeekRequest *req = &spec->peeks[i];
            if (req->frame >= 0 && g_ctx.result->peeks[i].data) continue;
            if (req->frame >= spec->frames) continue;
            size_t avail = 0;
            const uint8_t *data =
                mCoreGetMemoryBlock(core, req->address, &avail);
            uint32_t want = req->size < avail ? req->size : (uint32_t)avail;
            if (data && want) {
                free(g_ctx.result->peeks[i].data);
                g_ctx.result->peeks[i].address = req->address;
                g_ctx.result->peeks[i].size = want;
                g_ctx.result->peeks[i].data = malloc(want);
                memcpy(g_ctx.result->peeks[i].data, data, want);
            }
        }

    if (spec->state_out && g_ctx.result)
        g_ctx.result->state_out_ok =
            save_state_file_on_thread(core, spec->state_out);
    /* snapshot video registers for tilemap/OCR analysis */
    if (g_ctx.result) {
        struct GBA *gba = (struct GBA *)core->board;
        if (gba) {
            g_ctx.result->video_regs[0] = gba->memory.io[0]; /* DISPCNT */
            for (int i = 0; i < 4; ++i)
                g_ctx.result->video_regs[1 + i] =
                    gba->memory.io[4 + i]; /* BGCNT0..3 */
        }
    }
    (void)thread;
}

static void blob_load_on_thread(struct mCoreThread *thread) {
    *g_blob_job.ok = thread->core->loadState(thread->core, g_blob_job.blob);
    (void)thread;
}

static void blob_save_on_thread(struct mCoreThread *thread) {
    *g_save_job.ok = thread->core->saveState(thread->core, g_save_job.blob);
    (void)thread;
}

bool aos_mgba_run(AosMgbaSession *s, const AosRunSpec *spec,
                  AosRunResult *result) {
    if (!s || !spec) return false;
    if (result) memset(result, 0, sizeof *result);
    g_ctx.session = s;
    g_ctx.spec = spec;
    g_ctx.result = result;
    mCoreThreadRunFunction(&s->thread, run_on_thread);
    return true;
}

void aos_mgba_free_result(AosRunResult *result) {
    if (!result) return;
    for (int i = 0; i < result->peek_count; ++i) free(result->peeks[i].data);
    free(result->peeks);
    result->peeks = NULL;
    result->peek_count = 0;
}

bool aos_mgba_save_state(AosMgbaSession *s, const char *path) {
    size_t size = 0;
    uint8_t *blob = aos_mgba_save_state_blob(s, &size);
    if (!blob) return false;
    FILE *f = fopen(path, "wb");
    bool ok = f && fwrite(blob, 1, size, f) == size;
    if (f) fclose(f);
    free(blob);
    return ok;
}

bool aos_mgba_load_state(AosMgbaSession *s, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *blob = malloc(size > 0 ? (size_t)size : 1);
    bool ok = blob && fread(blob, 1, (size_t)size, f) == (size_t)size &&
              aos_mgba_load_state_blob(s, blob, (size_t)size);
    fclose(f);
    free(blob);
    return ok;
}

bool aos_mgba_load_state_blob(AosMgbaSession *s, const void *blob,
                              size_t size) {
    (void)size; /* mGBA loadState consumes exactly stateSize bytes */
    bool ok = false;
    g_blob_job.blob = blob;
    g_blob_job.ok = &ok;
    mCoreThreadRunFunction(&s->thread, blob_load_on_thread);
    return ok;
}

uint8_t *aos_mgba_save_state_blob(AosMgbaSession *s, size_t *out_size) {
    size_t state_size = s->core->stateSize(s->core);
    uint8_t *blob = malloc(state_size);
    if (!blob) return NULL;
    bool ok = false;
    g_save_job.blob = blob;
    g_save_job.ok = &ok;
    mCoreThreadRunFunction(&s->thread, blob_save_on_thread);
    if (!ok) {
        free(blob);
        return NULL;
    }
    if (out_size) *out_size = state_size;
    return blob;
}

AosKeyEvent *aos_mgba_parse_keys(const char *spec, int *count) {
    *count = 0;
    if (!spec || !*spec) return NULL;
    int capacity = 16;
    AosKeyEvent *events = malloc(capacity * sizeof *events);
    char *copy = strdup(spec);
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        int frame = 0;
        unsigned mask = 0;
        if (sscanf(tok, "%d:%x", &frame, &mask) != 2 &&
            sscanf(tok, "%d:%u", &frame, &mask) != 2)
            continue;
        if (*count == capacity) {
            capacity *= 2;
            events = realloc(events, capacity * sizeof *events);
        }
        events[*count].frame = frame;
        events[*count].mask = mask;
        ++*count;
    }
    free(copy);
    return events;
}

void aos_mgba_free_keys(AosKeyEvent *keys) { free(keys); }

int *aos_mgba_parse_frames(const char *spec, int *count) {
    *count = 0;
    if (!spec || !*spec) return NULL;
    int capacity = 16;
    int *list = malloc(capacity * sizeof *list);
    char *copy = strdup(spec);
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        if (*count == capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof *list);
        }
        list[*count] = atoi(tok);
        ++*count;
    }
    free(copy);
    return list;
}

void aos_mgba_free_frames(int *frames) { free(frames); }

AosPeekRequest *aos_mgba_parse_peeks(const char *spec, int *count) {
    *count = 0;
    if (!spec || !*spec) return NULL;
    int capacity = 8;
    AosPeekRequest *list = malloc(capacity * sizeof *list);
    char *copy = strdup(spec);
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        unsigned a = 0, s = 0;
        int f = -1;
        int fields = sscanf(tok, "%i:%x:%x", &f, &a, &s);
        if (fields == 2) {
            f = -1;
            sscanf(tok, "%x:%x", &a, &s);
        } else if (fields != 3) {
            continue;
        }
        if (*count == capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof *list);
        }
        list[*count].frame = f;
        list[*count].address = a;
        list[*count].size = s;
        ++*count;
    }
    free(copy);
    return list;
}

void aos_mgba_free_peeks(AosPeekRequest *peeks) { free(peeks); }
