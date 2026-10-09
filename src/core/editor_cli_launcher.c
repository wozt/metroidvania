/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/editor_cli_launcher.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif

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
    execvp(forwarded[0], forwarded);
    fprintf(stderr, "Unable to launch editor CLI: %s\n", strerror(errno));
    free(forwarded);
    return 127;
}
