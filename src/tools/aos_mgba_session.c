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
    /* Normalized (stable-sorted, validated) frame schedules. run_on_thread
     * consumes these instead of spec->keys/spec->pokes so events are
     * always applied chronologically. Owned by aos_mgba_run. */
    AosKeyEvent *keys;
    int key_count;
    AosPokeEvent *pokes;
    int poke_count;
    /* Run failure latch: set on the emulation thread, read by
     * aos_mgba_run after the job completes. */
    bool failed;
    char error[256];
};
static struct RunCtx g_ctx;

struct BlobJob {
    const void *blob;
    size_t size;   /* bytes supplied; must equal core->stateSize */
    bool *ok;
};
static struct BlobJob g_blob_job;

struct SaveJob {
    uint8_t *blob;
    bool *ok;
};
static struct SaveJob g_save_job;

/* Write a P6 PPM of the 240x160 framebuffer. Returns false on any
 * I/O failure (open, write or close) so a missing/partial capture can
 * never be reported as success. */
static bool write_ppm(const char *path, const uint32_t *pixels) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "capture: cannot open %s\n", path);
        return false;
    }
    bool ok = fprintf(f, "P6\n%d %d\n255\n", AOS_MGBA_WIDTH,
                      AOS_MGBA_HEIGHT) > 0;
    for (int y = 0; ok && y < AOS_MGBA_HEIGHT; ++y) {
        for (int x = 0; x < AOS_MGBA_WIDTH; ++x) {
            uint32_t c = pixels[y * AOS_MGBA_WIDTH + x];
            if (fputc(c & 0xFF, f) == EOF ||          /* R: low byte */
                fputc((c >> 8) & 0xFF, f) == EOF ||
                fputc((c >> 16) & 0xFF, f) == EOF) { /* B: third byte */
                ok = false;
                break;
            }
        }
    }
    if (fclose(f) != 0) ok = false;
    if (!ok)
        fprintf(stderr, "capture: write failed for %s\n", path);
    return ok;
}

static bool is_capture_frame(const AosRunSpec *spec, int frame) {
    if (spec->capture_count == 0) return frame == spec->frames - 1;
    for (int i = 0; i < spec->capture_count; ++i)
        if (spec->capture_frames[i] == frame) return true;
    return false;
}

/* Load a serialized state file on the emulation thread. The blob must
 * be exactly core->stateSize bytes: mGBA's loadState consumes a fixed
 * stateSize buffer, so a truncated or oversized file is rejected before
 * it can corrupt the core. */
static bool load_state_file_on_thread(struct mCore *core, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "state: cannot open %s\n", path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    size_t expect = core->stateSize(core);
    if (size <= 0 || (size_t)size != expect) {
        fprintf(stderr, "state: %s bad size %ld (expected %zu)\n",
                path, size, expect);
        fclose(f);
        return false;
    }
    uint8_t *blob = malloc((size_t)size);
    bool ok = blob && fread(blob, 1, (size_t)size, f) == (size_t)size &&
              core->loadState(core, blob);
    fclose(f);
    free(blob);
    if (!ok)
        fprintf(stderr, "state: load failed for %s\n", path);
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

/* Record the first run failure; later failures are ignored so the
 * reported reason stays the root cause. */
static void run_fail(const char *fmt, ...) {
    if (g_ctx.failed) return;
    g_ctx.failed = true;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_ctx.error, sizeof g_ctx.error, fmt, ap);
    va_end(ap);
    fprintf(stderr, "lab: %s\n", g_ctx.error);
}

static void run_on_thread(struct mCoreThread *thread) {
    AosMgbaSession *s = g_ctx.session;
    const AosRunSpec *spec = g_ctx.spec;
    struct mCore *core = thread->core;
    uint32_t keys = 0;
    int key_index = 0, poke_index = 0;

    /* An explicitly requested checkpoint that cannot be loaded must
     * fail the run: continuing from a fresh boot would silently
     * invalidate every capture and state written afterwards. */
    if (spec->state_in &&
        !load_state_file_on_thread(core, spec->state_in)) {
        run_fail("state load failed for %s", spec->state_in);
        return;
    }

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
        if (g_ctx.failed) break;   /* stop wasting frames after a failure */
        /* g_ctx.keys/g_ctx.pokes are stable-sorted by frame, so every
         * event fires exactly at its scheduled frame in order; several
         * events on one frame apply in input order (last mask wins). */
        while (key_index < g_ctx.key_count &&
               g_ctx.keys[key_index].frame == frame) {
            keys = g_ctx.keys[key_index].mask;
            ++key_index;
        }
        core->setKeys(core, keys);
        core->runFrame(core);
        while (poke_index < g_ctx.poke_count &&
               g_ctx.pokes[poke_index].frame == frame) {
            const AosPokeEvent *pk = &g_ctx.pokes[poke_index];
            size_t avail = 0;
            void *block = mCoreGetMemoryBlock(core, pk->address, &avail);
            if (block && pk->size <= avail)
                memcpy(block, pk->data, pk->size);
            ++poke_index;
        }
        if (frame == 0 && g_ctx.result && !g_ctx.result->fb_first) {
            g_ctx.result->fb_first = malloc(AOS_MGBA_WIDTH *
                                            AOS_MGBA_HEIGHT *
                                            sizeof(uint32_t));
            if (g_ctx.result->fb_first)
                memcpy(g_ctx.result->fb_first, s->framebuffer,
                       AOS_MGBA_WIDTH * AOS_MGBA_HEIGHT *
                       sizeof(uint32_t));
        }
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
            if (!write_ppm(path, s->framebuffer))
                run_fail("screenshot capture failed for %s", path);
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

    if (spec->state_out) {
        bool out_ok = save_state_file_on_thread(core, spec->state_out);
        if (g_ctx.result) g_ctx.result->state_out_ok = out_ok;
        if (!out_ok)
            run_fail("state save failed for %s", spec->state_out);
    }
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
    /* mGBA loadState consumes exactly stateSize bytes; reject any other
     * length instead of letting it read past the caller's buffer. */
    if (g_blob_job.size != thread->core->stateSize(thread->core)) {
        *g_blob_job.ok = false;
        return;
    }
    *g_blob_job.ok = thread->core->loadState(thread->core, g_blob_job.blob);
    (void)thread;
}

static void blob_save_on_thread(struct mCoreThread *thread) {
    *g_save_job.ok = thread->core->saveState(thread->core, g_save_job.blob);
    (void)thread;
}

/* ---- schedule normalization -----------------------------------------
 * Frame-indexed schedules must be applied chronologically. The run loop
 * matches events with `event.frame == frame`, so an out-of-order list
 * would skip every event whose frame had already passed. These helpers
 * stable-sort by frame (same-frame events keep input order, so the last
 * mask wins deterministically) and reject negative frames. */

typedef struct { AosKeyEvent ev; int ord; } KeySortEnt;
static int key_sort_cmp(const void *a, const void *b) {
    const KeySortEnt *x = a, *y = b;
    if (x->ev.frame != y->ev.frame) return x->ev.frame - y->ev.frame;
    return x->ord - y->ord;
}

AosKeyEvent *aos_mgba_normalize_keys(const AosKeyEvent *in, int count,
                                     int *out_count) {
    *out_count = 0;
    if (count <= 0) return NULL;
    if (!in) { *out_count = -1; return NULL; }
    KeySortEnt *tmp = malloc((size_t)count * sizeof *tmp);
    if (!tmp) { *out_count = -1; return NULL; }
    for (int i = 0; i < count; ++i) {
        if (in[i].frame < 0) {           /* invalid schedule */
            free(tmp);
            *out_count = -1;
            return NULL;
        }
        tmp[i].ev = in[i];
        tmp[i].ord = i;
    }
    qsort(tmp, (size_t)count, sizeof *tmp, key_sort_cmp);
    AosKeyEvent *out = malloc((size_t)count * sizeof *out);
    if (!out) { free(tmp); *out_count = -1; return NULL; }
    for (int i = 0; i < count; ++i) out[i] = tmp[i].ev;
    free(tmp);
    *out_count = count;
    return out;
}

typedef struct { AosPokeEvent ev; int ord; } PokeSortEnt;
static int poke_sort_cmp(const void *a, const void *b) {
    const PokeSortEnt *x = a, *y = b;
    if (x->ev.frame != y->ev.frame) return x->ev.frame - y->ev.frame;
    return x->ord - y->ord;
}

AosPokeEvent *aos_mgba_normalize_pokes(const AosPokeEvent *in, int count,
                                       int *out_count) {
    *out_count = 0;
    if (count <= 0) return NULL;
    if (!in) { *out_count = -1; return NULL; }
    PokeSortEnt *tmp = malloc((size_t)count * sizeof *tmp);
    if (!tmp) { *out_count = -1; return NULL; }
    for (int i = 0; i < count; ++i) {
        if (in[i].frame < 0) {
            free(tmp);
            *out_count = -1;
            return NULL;
        }
        tmp[i].ev = in[i];   /* shallow copy: data pointer shared, not owned */
        tmp[i].ord = i;
    }
    qsort(tmp, (size_t)count, sizeof *tmp, poke_sort_cmp);
    AosPokeEvent *out = malloc((size_t)count * sizeof *out);
    if (!out) { free(tmp); *out_count = -1; return NULL; }
    for (int i = 0; i < count; ++i) out[i] = tmp[i].ev;
    free(tmp);
    *out_count = count;
    return out;
}

bool aos_mgba_run(AosMgbaSession *s, const AosRunSpec *spec,
                  AosRunResult *result) {
    if (!s || !spec) return false;
    if (result) memset(result, 0, sizeof *result);
    g_ctx.session = s;
    g_ctx.spec = spec;
    g_ctx.result = result;
    g_ctx.failed = false;
    g_ctx.error[0] = '\0';
    g_ctx.keys = NULL;
    g_ctx.key_count = 0;
    g_ctx.pokes = NULL;
    g_ctx.poke_count = 0;

    /* Normalize schedules up front: an invalid (negative-frame) spec
     * fails the run before a single frame executes. */
    if (spec->key_count > 0) {
        g_ctx.keys = aos_mgba_normalize_keys(spec->keys, spec->key_count,
                                             &g_ctx.key_count);
        if (!g_ctx.keys) {
            if (result) {
                result->ok = false;
                snprintf(result->error, sizeof result->error,
                         "invalid key schedule");
            }
            return false;
        }
    }
    if (spec->poke_count > 0) {
        g_ctx.pokes = aos_mgba_normalize_pokes(spec->pokes, spec->poke_count,
                                               &g_ctx.poke_count);
        if (!g_ctx.pokes) {
            free(g_ctx.keys);
            g_ctx.keys = NULL;
            if (result) {
                result->ok = false;
                snprintf(result->error, sizeof result->error,
                         "invalid poke schedule");
            }
            return false;
        }
    }

    mCoreThreadRunFunction(&s->thread, run_on_thread);

    /* run_on_thread owns g_ctx.keys/pokes copies now; free them. The
     * poke data buffers are caller-owned (aos_mgba_free_pokes), so only
     * the array shells are released here. */
    free(g_ctx.keys);
    free(g_ctx.pokes);
    g_ctx.keys = NULL;
    g_ctx.pokes = NULL;

    bool ok = !g_ctx.failed;
    if (result) {
        result->ok = ok;
        if (g_ctx.failed)
            snprintf(result->error, sizeof result->error, "%s",
                     g_ctx.error);
    }
    return ok;
}

void aos_mgba_free_result(AosRunResult *result) {
    if (!result) return;
    for (int i = 0; i < result->peek_count; ++i) free(result->peeks[i].data);
    free(result->peeks);
    result->peeks = NULL;
    result->peek_count = 0;
    free(result->fb_first);
    result->fb_first = NULL;
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
    /* Reject blobs whose size cannot be a valid serialized state before
     * handing them to the core (loadState reads exactly stateSize). */
    if (!s || !blob || size != s->core->stateSize(s->core)) return false;
    bool ok = false;
    g_blob_job.blob = blob;
    g_blob_job.size = size;
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
        /* "frame:mask" — the mask is a decimal AOS/GBA key bitmask by
         * default; a 0x prefix selects hex (strtoul base 0). The test
         * schedules emit decimal masks (RIGHT = 16, A|B = 3), so a
         * plain %x sscanf silently corrupted every mask >= 10
         * ("16" -> 0x16 = RIGHT|SELECT|B, "19" -> 0x19 =
         * RIGHT|START|A), which opened the native quick map and soul
         * menu mid-comparison. */
        char *colon = strchr(tok, ':');
        if (!colon) continue;
        *colon = '\0';
        char *end = NULL;
        long frame = strtol(tok, &end, 10);
        if (end == tok) continue;
        unsigned long mask = strtoul(colon + 1, &end, 0);
        if (end == colon + 1) continue;
        if (*count == capacity) {
            capacity *= 2;
            events = realloc(events, capacity * sizeof *events);
        }
        events[*count].frame = (int)frame;
        events[*count].mask = (unsigned)mask;
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

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

AosPokeEvent *aos_mgba_parse_pokes(const char *spec, int *count) {
    *count = 0;
    if (!spec || !*spec) return NULL;
    int capacity = 8;
    AosPokeEvent *list = malloc(capacity * sizeof *list);
    char *copy = strdup(spec);
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        /* "frame:addr:hexbytes" — hexbytes is a plain byte string
         * ("0a0b0c" or "0A0B0C"); odd trailing nibble is rejected. */
        char *c1 = strchr(tok, ':');
        if (!c1) continue;
        char *c2 = strchr(c1 + 1, ':');
        if (!c2) continue;
        *c1 = '\0';
        *c2 = '\0';
        char *end = NULL;
        long frame = strtol(tok, &end, 10);
        if (end == tok) continue;
        unsigned long addr = strtoul(c1 + 1, &end, 16);
        if (end == c1 + 1) continue;
        const char *hex = c2 + 1;
        size_t nibbles = strlen(hex);
        if (nibbles % 2) continue;
        uint32_t size = (uint32_t)(nibbles / 2);
        if (!size) continue;
        uint8_t *data = malloc(size);
        if (!data) continue;
        bool ok = true;
        for (uint32_t i = 0; i < size; ++i) {
            int hi = hexval(hex[2 * i]), lo = hexval(hex[2 * i + 1]);
            if (hi < 0 || lo < 0) { ok = false; break; }
            data[i] = (uint8_t)((hi << 4) | lo);
        }
        if (!ok) { free(data); continue; }
        if (*count == capacity) {
            capacity *= 2;
            list = realloc(list, capacity * sizeof *list);
        }
        list[*count].frame = (int)frame;
        list[*count].address = (uint32_t)addr;
        list[*count].data = data;
        list[*count].size = size;
        ++*count;
    }
    free(copy);
    return list;
}

void aos_mgba_free_pokes(AosPokeEvent *pokes, int count) {
    if (!pokes) return;
    for (int i = 0; i < count; ++i) free(pokes[i].data);
    free(pokes);
}
