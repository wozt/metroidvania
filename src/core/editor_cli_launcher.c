/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/editor_cli_launcher.h"

#include "../editor_backend/backend.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    int source;

    if (argc < 1 || !argv) {
        fputs("Invalid editor CLI arguments\n", stderr);
        return 127;
    }
    char **native = calloc((size_t)argc + 1U, sizeof(*native));
    if (!native) {
        fputs("Editor CLI argument allocation failed\n", stderr);
        return 127;
    }
    int count = 0;
    for (source = 0; source < argc; ++source) {
        if (source && remove_headless && strcmp(argv[source], "--headless") == 0) continue;
        native[count++] = argv[source];
    }
    bool handled = false;
    int code = feb_cli_main(count, native, FUSION_SOURCE_DIR, &handled);
    free(native);
    if (!handled) {
        fputs("Editor command is not implemented by the native backend\n", stderr);
        return 127;
    }
    return code;
}
