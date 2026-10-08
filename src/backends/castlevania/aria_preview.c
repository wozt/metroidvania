/* SPDX-License-Identifier: GPL-3.0-only */
#include "aria/preview.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint32_t pixels[ARIA_FRAME_WIDTH * ARIA_FRAME_HEIGHT];
    unsigned frame;
} PreviewState;

static bool demo_open(void **context, const char *rom_path)
{
    PreviewState *state;
    if (!context || !rom_path || !rom_path[0]) return false;
    state = calloc(1, sizeof(*state));
    if (!state) return false;
    *context = state;
    return true;
}
static bool demo_step(void *context, AriaFrameInput input)
{
    PreviewState *state = context;
    unsigned x, y;
    if (!state) return false;
    ++state->frame;
    for (y = 0; y < ARIA_FRAME_HEIGHT; ++y) {
        for (x = 0; x < ARIA_FRAME_WIDTH; ++x) {
            uint8_t red = (uint8_t)((x + state->frame) & 255u);
            uint8_t green = (uint8_t)((y + (input.keys_held & 255u)) & 255u);
            uint8_t blue = (uint8_t)(((x ^ y) + state->frame / 2u) & 255u);
            state->pixels[y * ARIA_FRAME_WIDTH + x] =
                ((uint32_t)red << 24) | ((uint32_t)green << 16) |
                ((uint32_t)blue << 8) | 255u;
        }
    }
    return true;
}
static bool demo_frame(void *context, AriaFrameView *out)
{
    PreviewState *state = context;
    if (!state || !out || !state->frame) return false;
    *out = (AriaFrameView){state->pixels, ARIA_FRAME_WIDTH,
                           ARIA_FRAME_HEIGHT, ARIA_FRAME_WIDTH};
    return true;
}
static void demo_close(void *context) { free(context); }
static const AriaRuntimeDriver demo_driver = {
    demo_open, demo_step, demo_frame, demo_close
};

bool aria_preview_open(AriaPreview *preview, SDL_Renderer *renderer)
{
    if (!preview || !renderer || preview->runtime.active || preview->video.texture)
        return false;
    if (!aria_sdl3_video_open(&preview->video, renderer)) return false;
    /* This is deliberately NOT the user's ROM path. Driver never reads files. */
    if (!aria_runtime_open(&preview->runtime, &demo_driver, "SYNTHETIC-PREVIEW")) {
        aria_sdl3_video_close(&preview->video);
        return false;
    }
    return true;
}

bool aria_preview_draw(AriaPreview *preview, SDL_Renderer *renderer,
                       uint16_t keys_held, int width, int height)
{
    AriaFrameView frame;
    SDL_FRect dst;
    float scale;
    if (!preview || !renderer || !preview->runtime.active || width <= 0 || height <= 0)
        return false;
    if (!aria_runtime_step(&preview->runtime, (AriaFrameInput){keys_held}) ||
        !aria_runtime_frame(&preview->runtime, &frame)) return false;
    scale = SDL_min((float)width / ARIA_FRAME_WIDTH,
                    (float)height / ARIA_FRAME_HEIGHT);
    dst.w = ARIA_FRAME_WIDTH * scale;
    dst.h = ARIA_FRAME_HEIGHT * scale;
    dst.x = ((float)width - dst.w) * 0.5f;
    dst.y = ((float)height - dst.h) * 0.5f;
    if (!SDL_SetRenderDrawColor(renderer, 10, 10, 16, 255) ||
        !SDL_RenderClear(renderer)) return false;
    return aria_sdl3_video_present(&preview->video, &frame, &dst);
}
void aria_preview_close(AriaPreview *preview)
{
    if (!preview) return;
    aria_runtime_close(&preview->runtime);
    aria_sdl3_video_close(&preview->video);
}
