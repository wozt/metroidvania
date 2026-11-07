/* SPDX-License-Identifier: GPL-3.0-only */
#define _GNU_SOURCE
/* Headless mGBA laboratory for Aria of Sorrow parity work.
 *
 * Runs the real ROM through libmgba with a software renderer, scripted
 * button input, framebuffer capture, save states and raw memory reads.
 * Everything stays local: no network, states are plain files.
 *
 * Usage:
 *   aos_mgba_lab --rom PATH [options]
 *     --frames N            run N frames (default 60)
 *     --keys "f:m,f:m"      setKeys mask at frame f (A=1 B=2 SEL=4 START=8
 *                           RIGHT=16 LEFT=32 UP=64 DOWN=128 R=256 L=512)
 *     --shot PREFIX         write PREFIX_<frame>.ppm at capture frames
 *     --capture "f,f,f"     frames to screenshot (default: last frame)
 *     --state-out FILE      save emulated state after the run
 *     --state-in FILE       load emulated state before the run
 *     --peek ADDR:SIZE      hex dump memory region after the run
 *     --peek-at F:ADDR:SIZE dump at frame F instead of end
 *     --log PATH            append one line of status per run
 */
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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAB_WIDTH GBA_VIDEO_HORIZONTAL_PIXELS
#define LAB_HEIGHT GBA_VIDEO_VERTICAL_PIXELS

struct KeyEvent {
    int frame;
    uint32_t mask;
};

struct PeekRequest {
    int frame; /* -1 = after run */
    uint32_t address;
    size_t size;
};

struct Lab {
    struct mCore *core;
    struct mCoreThread thread;
    struct GBAVideoSoftwareRenderer renderer;
    color_t *framebuffer;
    char error[256];
};

static void lab_fail(struct Lab *lab, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lab->error, sizeof lab->error, fmt, ap);
    va_end(ap);
}

static bool lab_init(struct Lab *lab, const char *rom_path) {
    memset(lab, 0, sizeof *lab);
    lab->framebuffer = calloc(LAB_WIDTH * LAB_HEIGHT, sizeof(color_t));
    if (!lab->framebuffer) {
        lab_fail(lab, "cannot allocate framebuffer");
        return false;
    }
    struct mCore *core = mCoreCreate(mPLATFORM_GBA);
    if (!core) {
        lab_fail(lab, "mCoreCreate failed");
        return false;
    }
    lab->core = core;
    mCoreConfigInit(&core->config, "gba");
    memset(&core->opts, 0, sizeof core->opts);
    core->opts.fpsTarget = 60.0f;
    if (!core->init(core)) {
        lab_fail(lab, "core init failed");
        return false;
    }
    struct GBA *gba = (struct GBA *)core->board;
    memset(&lab->renderer, 0, sizeof lab->renderer);
    GBAVideoSoftwareRendererCreate(&lab->renderer);
    lab->renderer.outputBuffer = lab->framebuffer;
    lab->renderer.outputBufferStride = LAB_WIDTH;
    GBAVideoAssociateRenderer(&gba->video, &lab->renderer.d);
    struct VFile *vf = VFileOpen(rom_path, O_RDONLY);
    if (!vf || !core->loadROM(core, vf)) {
        if (vf) vf->close(vf);
        lab_fail(lab, "cannot load ROM %s", rom_path);
        return false;
    }
    vf->close(vf);
    memset(&lab->thread, 0, sizeof lab->thread);
    lab->thread.core = core;
    if (!mCoreThreadStart(&lab->thread)) {
        lab_fail(lab, "emulation thread failed to start");
        return false;
    }
    return true;
}

static void lab_shutdown(struct Lab *lab) {
    if (lab->thread.core) mCoreThreadEnd(&lab->thread);
    free(lab->framebuffer);
    lab->framebuffer = NULL;
}

/* Runs on the emulation thread. */
struct RunJob {
    int frames;
    struct KeyEvent *keys;
    int key_count;
    const char *shot_prefix;
    int *capture_frames;
    int capture_count;
    const char *state_out;
    const char *state_in;
    struct PeekRequest *peeks;
    int peek_count;
    FILE *log;
};

static void write_ppm(const char *path, const color_t *pixels) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", LAB_WIDTH, LAB_HEIGHT);
    for (int y = 0; y < LAB_HEIGHT; ++y) {
        for (int x = 0; x < LAB_WIDTH; ++x) {
            color_t c = pixels[y * LAB_WIDTH + x];
            /* color_t is 0xAARRGGBB on little-endian builds. */
            fputc((c >> 16) & 0xFF, f);
            fputc((c >> 8) & 0xFF, f);
            fputc(c & 0xFF, f);
        }
    }
    fclose(f);
}

static bool is_capture_frame(const struct RunJob *job, int frame) {
    if (job->capture_count == 0) return frame == job->frames - 1;
    for (int i = 0; i < job->capture_count; ++i)
        if (job->capture_frames[i] == frame) return true;
    return false;
}

static void dump_peek(struct mCore *core, const struct PeekRequest *peek) {
    size_t size = 0;
    const uint8_t *data = mCoreGetMemoryBlock(core, peek->address, &size);
    if (!data || peek->size > size) {
        fprintf(stderr, "peek %08x unavailable (block %zu)\n",
                peek->address, size);
        return;
    }
    printf("peek %08x size %zu:\n", peek->address, peek->size);
    for (size_t i = 0; i < peek->size; i += 16) {
        printf("  %08x  ", peek->address + (uint32_t)i);
        for (size_t j = i; j < i + 16 && j < peek->size; ++j)
            printf("%02x ", data[j]);
        printf("\n");
    }
}

/* mCoreThreadRunFunction callback receives only the thread; the lab and job
 * pointers travel through file-scope context. */
struct ThreadCtx {
    struct Lab *lab;
    struct RunJob *job;
};

static struct ThreadCtx g_ctx;

static void execute_job(struct mCoreThread *thread) {
    struct Lab *lab = g_ctx.lab;
    struct RunJob *job = g_ctx.job;
    struct mCore *core = thread->core;
    uint32_t keys = 0;
    int key_index = 0;

    if (job->state_in) {
        size_t state_size = core->stateSize(core);
        void *state = malloc(state_size);
        struct VFile *vf = VFileOpen(job->state_in, O_RDONLY);
        bool ok = vf && vf->size(vf) == (off_t)state_size &&
                  vf->read(vf, state, state_size) == (ssize_t)state_size &&
                  core->loadState(core, state);
        if (vf) vf->close(vf);
        free(state);
        if (!ok) fprintf(stderr, "warning: state load failed for %s\n",
                         job->state_in);
    }

    for (int frame = 0; frame < job->frames; ++frame) {
        while (key_index < job->key_count &&
               job->keys[key_index].frame == frame) {
            keys = job->keys[key_index].mask;
            ++key_index;
        }
        /* key events are level-based: the last mask stays until changed.
         * Emit explicit release events from the schedule when mask=0. */
        core->setKeys(core, keys);
        core->runFrame(core);
        if (job->shot_prefix && is_capture_frame(job, frame)) {
            char path[1024];
            snprintf(path, sizeof path, "%s_%05d.ppm", job->shot_prefix,
                     frame);
            write_ppm(path, lab->framebuffer);
        }
        for (int i = 0; i < job->peek_count; ++i)
            if (job->peeks[i].frame == frame) dump_peek(core, &job->peeks[i]);
    }

    for (int i = 0; i < job->peek_count; ++i)
        if (job->peeks[i].frame < 0) dump_peek(core, &job->peeks[i]);

    if (job->state_out) {
        size_t state_size = core->stateSize(core);
        void *state = malloc(state_size);
        if (state && core->saveState(core, state)) {
            struct VFile *vf = VFileOpen(job->state_out,
                                         O_WRONLY | O_CREAT | O_TRUNC);
            if (vf) {
                vf->write(vf, state, state_size);
                vf->close(vf);
            }
        }
        free(state);
    }
    if (job->log) {
        fprintf(job->log, "frames=%d keys=%d\n", job->frames,
                job->key_count);
        fflush(job->log);
    }
    (void)thread;
}

static int parse_keys(const char *spec, struct KeyEvent **out, int *count) {
    *out = NULL;
    *count = 0;
    if (!spec || !*spec) return 0;
    int capacity = 16;
    struct KeyEvent *events = malloc(capacity * sizeof *events);
    char *copy = strdup(spec);
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        int frame = 0;
        unsigned mask = 0;
        if (sscanf(tok, "%d:%x", &frame, &mask) != 2 &&
            sscanf(tok, "%d:%u", &frame, &mask) != 2) {
            fprintf(stderr, "bad key event '%s'\n", tok);
            continue;
        }
        if (*count == capacity) {
            capacity *= 2;
            events = realloc(events, capacity * sizeof *events);
        }
        events[*count].frame = frame;
        events[*count].mask = mask;
        ++*count;
    }
    free(copy);
    *out = events;
    return *count;
}

static int parse_int_list(const char *spec, int **out, int *count) {
    *out = NULL;
    *count = 0;
    if (!spec || !*spec) return 0;
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
    *out = list;
    return *count;
}

static int parse_peeks(const char *spec, struct PeekRequest **out,
                       int *count) {
    *out = NULL;
    *count = 0;
    if (!spec || !*spec) return 0;
    int capacity = 8;
    struct PeekRequest *list = malloc(capacity * sizeof *list);
    char *copy = strdup(spec);
    char *save = NULL;
    for (char *tok = strtok_r(copy, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        /* formats: ADDR:SIZE or F:ADDR:SIZE */
        unsigned a = 0, s = 0;
        int f = -1;
        int fields = sscanf(tok, "%i:%x:%x", &f, &a, &s);
        if (fields == 2) {
            f = -1;
            sscanf(tok, "%x:%x", &a, &s);
        } else if (fields != 3) {
            fprintf(stderr, "bad peek '%s'\n", tok);
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
    *out = list;
    return *count;
}

static void null_log(struct mLogger *logger, int category,
                     enum mLogLevel level, const char *format,
                     va_list args) {
    (void)logger; (void)category; (void)level; (void)format; (void)args;
}

int main(int argc, char **argv) {
    /* Keep emulation quiet: DMA/BIOS traces would drown the tool output. */
    static struct mLogger quiet_logger;
    quiet_logger.log = null_log;
    quiet_logger.filter = NULL;
    mLogSetDefaultLogger(&quiet_logger);

    const char *rom = NULL, *shot = NULL, *state_out = NULL,
               *state_in = NULL, *keys_spec = NULL, *capture_spec = NULL,
               *peek_spec = NULL, *log_path = NULL;
    int frames = 60;
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(arg, "--rom") && value) { rom = value; ++i; }
        else if (!strcmp(arg, "--frames") && value) { frames = atoi(value); ++i; }
        else if (!strcmp(arg, "--keys") && value) { keys_spec = value; ++i; }
        else if (!strcmp(arg, "--shot") && value) { shot = value; ++i; }
        else if (!strcmp(arg, "--capture") && value) { capture_spec = value; ++i; }
        else if (!strcmp(arg, "--state-out") && value) { state_out = value; ++i; }
        else if (!strcmp(arg, "--state-in") && value) { state_in = value; ++i; }
        else if (!strcmp(arg, "--peek") && value) { peek_spec = value; ++i; }
        else if (!strcmp(arg, "--log") && value) { log_path = value; ++i; }
        else {
            fprintf(stderr, "unknown argument %s\n", arg);
            return 2;
        }
    }
    if (!rom) {
        fprintf(stderr, "usage: aos_mgba_lab --rom PATH [--frames N] ...\n");
        return 2;
    }

    struct Lab lab;
    if (!lab_init(&lab, rom)) {
        fprintf(stderr, "lab init failed: %s\n", lab.error);
        return 1;
    }

    struct RunJob job;
    memset(&job, 0, sizeof job);
    job.frames = frames;
    job.shot_prefix = shot;
    job.state_out = state_out;
    job.state_in = state_in;
    parse_keys(keys_spec, &job.keys, &job.key_count);
    parse_int_list(capture_spec, &job.capture_frames, &job.capture_count);
    parse_peeks(peek_spec, &job.peeks, &job.peek_count);
    FILE *log = NULL;
    if (log_path) {
        log = fopen(log_path, "a");
        job.log = log;
    }

    g_ctx.lab = &lab;
    g_ctx.job = &job;
    /* thread.userData is unused by mCoreThreadRunFunction; we use g_ctx. */
    mCoreThreadRunFunction(&lab.thread, execute_job);

    if (log) fclose(log);
    free(job.keys);
    free(job.capture_frames);
    free(job.peeks);
    lab_shutdown(&lab);
    return 0;
}
