/* SPDX-License-Identifier: GPL-3.0-only */
/* The frontends' host: standalone by default, and the launch queue of the
 * embedded mode (no window or process is created here). */
#include "fusion_host.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    int argc;
    char **argv;
    const char *program;
    assert(!fusion_host_embedded() && !fusion_host_take_launch(&argc, &argv, &program));
    /* Embedded (any window pointer: the queue never touches it). */
    fusion_host_embed((SDL_Window *)&argc);
    assert(fusion_host_embedded());
    const char *first[] = {"/opt/build/fusion_aria_title_preview", "--rom", "a b.gba", NULL};
    assert(fusion_host_launch(first));
    assert(fusion_host_take_launch(&argc, &argv, &program));
    assert(argc == 3 && !strcmp(program, "fusion_aria_title_preview") &&
           !strcmp(argv[2], "a b.gba") && argv[3] == NULL);
    assert(!fusion_host_take_launch(&argc, &argv, &program));
    /* A launch from the running frontend's own arguments (the runtime
     * coming back to its frontend) is copied before the queue is rewritten. */
    const char *second[] = {argv[0], "--game-over", argv[1], argv[2], NULL};
    assert(fusion_host_launch(second));
    assert(fusion_host_take_launch(&argc, &argv, &program));
    assert(argc == 4 && !strcmp(argv[0], "/opt/build/fusion_aria_title_preview") &&
           !strcmp(argv[1], "--game-over") && !strcmp(argv[3], "a b.gba"));
    fusion_host_embed(NULL);
    assert(!fusion_host_embedded());
    puts("fusion_host: ok");
    return 0;
}
