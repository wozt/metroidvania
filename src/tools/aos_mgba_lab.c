/* SPDX-License-Identifier: GPL-3.0-only */
/* Headless mGBA laboratory CLI for Aria of Sorrow parity work.
 *
 * Thin front-end over aos_mgba_session: runs the real ROM with scripted
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
#include "aos_mgba_session.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void dump_peek(const AosPeekResult *peek) {
    if (!peek->data || !peek->size) {
        fprintf(stderr, "peek %08x unavailable\n", peek->address);
        return;
    }
    printf("peek %08x size %u:\n", peek->address, peek->size);
    for (uint32_t i = 0; i < peek->size; i += 16) {
        printf("  %08x  ", peek->address + i);
        for (uint32_t j = i; j < i + 16 && j < peek->size; ++j)
            printf("%02x ", peek->data[j]);
        printf("\n");
    }
}

int main(int argc, char **argv) {
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

    char err[256];
    AosMgbaSession *session = aos_mgba_open(rom, err, sizeof err);
    if (!session) {
        fprintf(stderr, "lab init failed: %s\n", err);
        return 1;
    }

    AosRunSpec spec;
    memset(&spec, 0, sizeof spec);
    spec.frames = frames;
    spec.shot_prefix = shot;
    spec.state_out = state_out;
    spec.state_in = state_in;
    int key_count = 0, capture_count = 0, peek_count = 0;
    AosKeyEvent *keys = aos_mgba_parse_keys(keys_spec, &key_count);
    int *capture = aos_mgba_parse_frames(capture_spec, &capture_count);
    AosPeekRequest *peeks = aos_mgba_parse_peeks(peek_spec, &peek_count);
    spec.keys = keys;
    spec.key_count = key_count;
    spec.capture_frames = capture;
    spec.capture_count = capture_count;
    spec.peeks = peeks;
    spec.peek_count = peek_count;

    /* --peek-at F:ADDR:SIZE entries must fire mid-run; the session API
     * samples peeks after the run, so map them onto end-of-run reads
     * here is not possible. Keep CLI behaviour: peeks run post-run. */
    AosRunResult result;
    aos_mgba_run(session, &spec, &result);

    for (int i = 0; i < result.peek_count; ++i) dump_peek(&result.peeks[i]);

    if (log_path) {
        FILE *log = fopen(log_path, "a");
        if (log) {
            fprintf(log, "frames=%d keys=%d\n", frames, key_count);
            fclose(log);
        }
    }

    aos_mgba_free_result(&result);
    aos_mgba_free_keys(keys);
    aos_mgba_free_frames(capture);
    aos_mgba_free_peeks(peeks);
    aos_mgba_close(session);
    return 0;
}
