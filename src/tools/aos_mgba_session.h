/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MGBA_SESSION_H
#define AOS_MGBA_SESSION_H

/* Shared headless mGBA session for the Aria laboratory tools.
 *
 * One session owns one emulated core, its software renderer and its
 * framebuffer. Runs are executed synchronously on the emulation thread
 * through aos_mgba_run(). Everything stays local: no network, save
 * states are plain buffers/files owned by the caller.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AOS_MGBA_WIDTH 240
#define AOS_MGBA_HEIGHT 160

typedef struct AosMgbaSession AosMgbaSession;

/* level-based key mask applied from `frame` until the next event */
typedef struct {
    int frame;
    uint32_t mask; /* A=1 B=2 SELECT=4 START=8 RIGHT=16 LEFT=32 UP=64
                    * DOWN=128 R=256 L=512 */
} AosKeyEvent;

typedef struct {
    int frame;      /* frame at which to sample, -1 = after the run */
    uint32_t address;
    uint32_t size;
} AosPeekRequest;

typedef struct {
    uint32_t address;
    uint8_t *data;  /* malloc'd, caller frees */
    uint32_t size;  /* bytes actually returned */
} AosPeekResult;

typedef struct {
    int frames;
    const AosKeyEvent *keys;
    int key_count;
    const char *shot_prefix;   /* may be NULL; writes _%05d.ppm per capture */
    const int *capture_frames; /* NULL/0 = last frame only */
    int capture_count;
    const char *state_in;      /* path, may be NULL */
    const char *state_out;     /* path, may be NULL */
    const AosPeekRequest *peeks;
    int peek_count;
    double fps_cap;           /* max emulated fps; 0 = unthrottled */
    /* Optional live framebuffer mirror (W*H u32, 0xAABBGGRR) plus a
     * sequence counter bumped on every copy; both may be NULL. */
    uint32_t *fb_mirror;
    uint64_t *fb_seq;
    unsigned fb_interval;     /* copy every N frames; 0 = every frame */
} AosRunSpec;

typedef struct {
    AosPeekResult *peeks; /* peek_count entries */
    int peek_count;
    bool state_out_ok;
} AosRunResult;

/* Open a session and load the ROM. On failure returns NULL and fills err. */
AosMgbaSession *aos_mgba_open(const char *rom_path, char *err, size_t err_size);
void aos_mgba_close(AosMgbaSession *session);

/* Current framebuffer: AOS_MGBA_WIDTH*AOS_MGBA_HEIGHT pixels, 0xAABBGGRR. */
const uint32_t *aos_mgba_framebuffer(const AosMgbaSession *session);

/* Run one job synchronously. result may be NULL. */
bool aos_mgba_run(AosMgbaSession *session, const AosRunSpec *spec,
                  AosRunResult *result);
void aos_mgba_free_result(AosRunResult *result);

/* Serialize/restore the whole emulated state (mGBA format). */
bool aos_mgba_save_state(AosMgbaSession *session, const char *path);
bool aos_mgba_load_state(AosMgbaSession *session, const char *path);

/* Read `size` bytes at `address`; returns malloc'd buffer or NULL. */
uint8_t *aos_mgba_read_mem(AosMgbaSession *session, uint32_t address,
                           uint32_t size);

/* Load a state from a memory blob (e.g. a database record). */
bool aos_mgba_load_state_blob(AosMgbaSession *session, const void *blob,
                              size_t size);
/* Serialize state into a malloc'd buffer; caller frees. */
uint8_t *aos_mgba_save_state_blob(AosMgbaSession *session, size_t *out_size);

/* Parse "f:m,f:m" key schedule; returns malloc'd array. */
AosKeyEvent *aos_mgba_parse_keys(const char *spec, int *count);
void aos_mgba_free_keys(AosKeyEvent *keys);

/* Parse "f1,f2" frame list. */
int *aos_mgba_parse_frames(const char *spec, int *count);
void aos_mgba_free_frames(int *frames);

/* Parse "ADDR:SIZE" or "F:ADDR:SIZE" comma list. */
AosPeekRequest *aos_mgba_parse_peeks(const char *spec, int *count);
void aos_mgba_free_peeks(AosPeekRequest *peeks);

#ifdef __cplusplus
}
#endif

#endif /* AOS_MGBA_SESSION_H */
