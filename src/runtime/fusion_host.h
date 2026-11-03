/* SPDX-License-Identifier: GPL-3.0-only */
/* What the SDL frontends ask of their host: SDL's subsystems, their window
 * and the launch of another frontend. A standalone frontend gets SDL as
 * is (SDL_Init, SDL_CreateWindow, SDL_Quit, a new process). Embedded in the
 * Metroid Vania launcher (fusion_host_embed), they share its window: the
 * window is never created nor destroyed by them, SDL is never quit, and a
 * launch is queued for the launcher, which runs that frontend's main in the
 * same process and window once the current one returns. */
#ifndef FUSION_HOST_H
#define FUSION_HOST_H

#include <SDL3/SDL.h>
#include <stdbool.h>

bool fusion_host_init(SDL_InitFlags flags);
void fusion_host_quit(void);
/* Embedded: the shared window (titled `title`, its renderer released). */
SDL_Window *fusion_host_create_window(const char *title, int width, int height,
                                      SDL_WindowFlags flags);
void fusion_host_destroy_window(SDL_Window *window);
/* Starts the program of `arguments` (NULL-terminated, [0] its path);
 * embedded, queues it. False when it cannot. */
bool fusion_host_launch(const char *const *arguments);

/* The launcher's side. */
void fusion_host_embed(SDL_Window *window);
bool fusion_host_embedded(void);
/* The queued launch, if any (its arguments stay valid until the next
 * launch); the program's base name in *program. */
bool fusion_host_take_launch(int *argc, char ***argv, const char **program);

#endif
