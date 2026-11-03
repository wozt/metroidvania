/* SPDX-License-Identifier: GPL-3.0-only */
/* The SDL frontends' host: see fusion_host.h. */
#include "fusion_host.h"

#include <stdio.h>
#include <string.h>

enum { MAX_ARGUMENTS = 64, ARGUMENT_BYTES = 16384 };

static SDL_Window *shared_window;
static bool queued;
static int queued_count;
static char *queued_arguments[MAX_ARGUMENTS + 1];
static char argument_bytes[ARGUMENT_BYTES];

bool fusion_host_init(SDL_InitFlags flags) {
    return shared_window ? SDL_InitSubSystem(flags) : SDL_Init(flags);
}

void fusion_host_quit(void) {
    if (!shared_window) SDL_Quit();
}

SDL_Window *fusion_host_create_window(const char *title, int width, int height,
                                      SDL_WindowFlags flags) {
    if (!shared_window) return SDL_CreateWindow(title, width, height, flags);
    /* A window has one renderer: the previous frontend's goes. */
    SDL_Renderer *renderer = SDL_GetRenderer(shared_window);
    if (renderer) SDL_DestroyRenderer(renderer);
    return shared_window;
}

void fusion_host_destroy_window(SDL_Window *window) {
    if (window && window != shared_window) SDL_DestroyWindow(window);
}

bool fusion_host_launch(const char *const *arguments) {
    if (!shared_window) {
        SDL_Process *process = SDL_CreateProcess(arguments, false);
        if (!process) return false;
        SDL_DestroyProcess(process);
        return true;
    }
    size_t used = 0;
    int count = 0;
    for (; arguments[count]; ++count) {
        size_t length = strlen(arguments[count]) + 1;
        if (count >= MAX_ARGUMENTS || used + length > ARGUMENT_BYTES) {
            SDL_SetError("too many arguments to launch");
            return false;
        }
    }
    /* The arguments may point into the buffer of the previous launch. */
    static char copy[ARGUMENT_BYTES];
    char *pointers[MAX_ARGUMENTS];
    for (int k = 0; k < count; ++k) {
        size_t length = strlen(arguments[k]) + 1;
        memcpy(copy + used, arguments[k], length);
        pointers[k] = copy + used;
        used += length;
    }
    memcpy(argument_bytes, copy, used);
    for (int k = 0; k < count; ++k) queued_arguments[k] = argument_bytes + (pointers[k] - copy);
    queued_arguments[count] = NULL;
    queued_count = count;
    queued = true;
    return true;
}

void fusion_host_embed(SDL_Window *window) {
    shared_window = window;
}

bool fusion_host_embedded(void) {
    return shared_window != NULL;
}

bool fusion_host_take_launch(int *argc, char ***argv, const char **program) {
    if (!queued) return false;
    queued = false;
    *argc = queued_count;
    *argv = queued_arguments;
    const char *name = queued_arguments[0];
    const char *slash = strrchr(name, '/');
    const char *backslash = strrchr(name, '\\');
    if (backslash && (!slash || backslash > slash)) slash = backslash;
    *program = slash ? slash + 1 : name;
    return true;
}
