/* SPDX-License-Identifier: GPL-3.0-only */
/* Checks that the generated tracking documents match their registries,
 * without writing anything or running any test (formerly
 * scripts/check_generated.py):
 *
 * - fusion_checklist --check: docs/NATIVE_PARITY_CHECKLIST.md against
 *   data/native_parity/annotations.tsv and the native inventory;
 * - fusion_dashboard --check: the registry-derived part of the dashboard
 *   data against both registries and the tracked files;
 * - fusion_sound_ids --check: src/runtime/mzm_sound_ids.h against the pinned
 *   decompilation's Sound enum.
 *
 * Regenerate with ./build/fusion_rebuild --checklist and
 * ./build/fusion_dashboard --run-tests. */
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif
#ifndef FUSION_BINARY_DIR
#error "FUSION_BINARY_DIR must identify the build directory"
#endif

/* Runs one checker in the repository; its own output is passed through. */
static int run(const char *program, const char *label) {
    pid_t child = fork();
    if (child < 0) return -1;
    if (!child) {
        if (chdir(FUSION_SOURCE_DIR)) _exit(126);
        execl(program, program, "--check", (char *)NULL);
        fprintf(stderr, "cannot run %s\n", label);
        _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static int exists(const char *path) {
    struct stat status;
    return stat(path, &status) == 0;
}

int main(int argc, char **argv) {
    (void)argv;
    if (argc > 1) {
        fputs("usage: fusion_check_generated (no arguments)\n", stderr);
        return 2;
    }
    int failures = 0;
    if (run(FUSION_BINARY_DIR "/fusion_checklist", "the native checklist check")) ++failures;
    if (run(FUSION_BINARY_DIR "/fusion_dashboard", "the dashboard check")) ++failures;
    if (exists(FUSION_SOURCE_DIR "/third_party/mzm/include/constants/audio.h") &&
        exists(FUSION_BINARY_DIR "/fusion_sound_ids") &&
        run(FUSION_BINARY_DIR "/fusion_sound_ids", "the sound identifier check"))
        ++failures;
    fflush(stdout);
    if (failures) {
        fprintf(stderr, "%d generated document(s) stale\n", failures);
        return 1;
    }
    puts("Generated documents are fresh");
    return 0;
}
