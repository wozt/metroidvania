/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "core/editor_cli_launcher.h"

#include "../editor_backend/backend.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif
#ifndef FUSION_BINARY_DIR
#error "FUSION_BINARY_DIR must identify the build directory"
#endif

const char *fusion_editor_cli_program(void)
{
    return FUSION_BINARY_DIR "/fusion_editor_cli";
}

const char *fusion_world_overview_program(void)
{
    return FUSION_BINARY_DIR "/fusion_world_overview";
}

int fusion_editor_cli_launch(int argc, char **argv, bool remove_headless)
{
    static const char script[] = FUSION_SOURCE_DIR "/scripts/editor_cli.py";
    char **forwarded;
    int source;
    int target = 0;

    if (argc < 1 || !argv) {
        fputs("Invalid editor CLI arguments\n", stderr);
        return 127;
    }
    /* The native backend runs every ported command; the others, the help
     * text and batches still go to the Python backend. */
    if (!getenv("FUSION_EDITOR_PYTHON_BACKEND")) {
        char **native = calloc((size_t)argc + 1U, sizeof(*native));
        int count = 0;
        if (!native) {
            fputs("Editor CLI argument allocation failed\n", stderr);
            return 127;
        }
        for (source = 0; source < argc; ++source) {
            if (source && remove_headless && strcmp(argv[source], "--headless") == 0) continue;
            native[count++] = argv[source];
        }
        bool handled = false;
        int code = feb_cli_main(count, native, FUSION_SOURCE_DIR, &handled);
        free(native);
        if (handled) return code;
    }
    forwarded = calloc((size_t)argc + 2U, sizeof(*forwarded));
    if (!forwarded) {
        fputs("Editor CLI argument allocation failed\n", stderr);
        return 127;
    }
    forwarded[target++] = (char *)"python3";
    forwarded[target++] = (char *)script;
    for (source = 1; source < argc; ++source) {
        if (remove_headless && strcmp(argv[source], "--headless") == 0) {
            continue;
        }
        forwarded[target++] = argv[source];
    }
    forwarded[target] = NULL;
    if (setenv("FUSION_NATIVE_MAP_TOOL",
               FUSION_BINARY_DIR "/fusion_native_map_cli", 1) != 0) {
        fprintf(stderr, "Unable to configure native map CLI: %s\n", strerror(errno));
        free(forwarded);
        return 127;
    }
    execvp(forwarded[0], forwarded);
    fprintf(stderr, "Unable to launch editor CLI: %s\n", strerror(errno));
    free(forwarded);
    return 127;
}
