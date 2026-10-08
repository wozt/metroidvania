#ifndef FUSION_ARIA_SDL3_VIDEO_H
#define FUSION_ARIA_SDL3_VIDEO_H

#include <SDL3/SDL.h>
#include "aria/runtime.h"

/* Host-only renderer. It does not execute original game logic. */
typedef struct {
    SDL_Texture *texture;
    SDL_Renderer *renderer; /* Borrowed; not owned by this object. */
} AriaSDL3Video;

bool aria_sdl3_video_open(AriaSDL3Video *video, SDL_Renderer *renderer);
bool aria_sdl3_video_present(AriaSDL3Video *video, const AriaFrameView *frame,
                             const SDL_FRect *destination);
void aria_sdl3_video_close(AriaSDL3Video *video);

#endif
