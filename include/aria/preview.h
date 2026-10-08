/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ARIA_PREVIEW_H
#define FUSION_ARIA_PREVIEW_H
#include <SDL3/SDL.h>
#include "aria/runtime.h"
#include "aria/sdl3_video.h"

typedef struct {
    AriaRuntime runtime;
    AriaSDL3Video video;
} AriaPreview;

/* Synthetic test driver. NEVER displays or executes original game content. */
bool aria_preview_open(AriaPreview *preview, SDL_Renderer *renderer);
bool aria_preview_draw(AriaPreview *preview, SDL_Renderer *renderer,
                       uint16_t keys_held, int width, int height);
void aria_preview_close(AriaPreview *preview);
#endif
