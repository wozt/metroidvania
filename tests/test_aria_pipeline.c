/* GPL-3.0-only - Independent integration test; no game code or data. */
#include "aria/runtime.h"
#include "aria/sdl3_video.h"
#include <SDL3/SDL.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint32_t pixels[ARIA_FRAME_WIDTH * ARIA_FRAME_HEIGHT];
    unsigned frame_number;
} SyntheticDriver;

static bool synthetic_open(void **ctx, const char *rom_path)
{
    SyntheticDriver *driver;
    if (!ctx || !rom_path || !rom_path[0]) return false;
    driver = calloc(1, sizeof(*driver));
    if (!driver) return false;
    *ctx = driver;
    return true;
}
static bool synthetic_step(void *ctx, AriaFrameInput input)
{
    SyntheticDriver *driver = ctx;
    unsigned x, y;
    if (!driver) return false;
    ++driver->frame_number;
    for (y = 0; y < ARIA_FRAME_HEIGHT; ++y)
        for (x = 0; x < ARIA_FRAME_WIDTH; ++x) {
            uint8_t red = (uint8_t)((x + driver->frame_number) & 255u);
            uint8_t green = (uint8_t)((y + (input.keys_held & 255u)) & 255u);
            driver->pixels[y * ARIA_FRAME_WIDTH + x] =
                ((uint32_t)red << 24) | ((uint32_t)green << 16) | 0x000080ffu;
        }
    return true;
}
static bool synthetic_frame(void *ctx, AriaFrameView *out)
{
    SyntheticDriver *driver = ctx;
    if (!driver || !out || !driver->frame_number) return false;
    *out = (AriaFrameView){driver->pixels, ARIA_FRAME_WIDTH,
                           ARIA_FRAME_HEIGHT, ARIA_FRAME_WIDTH};
    return true;
}
static void synthetic_close(void *ctx) { free(ctx); }

int main(void)
{
    const AriaRuntimeDriver driver = {
        synthetic_open, synthetic_step, synthetic_frame, synthetic_close
    };
    AriaRuntime runtime = {0};
    AriaSDL3Video video = {0};
    SDL_Window *window;
    SDL_Renderer *renderer;
    AriaFrameView frame;
    const SDL_FRect destination = {0.f, 0.f, 480.f, 320.f};
    unsigned i;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("Aria pipeline test", 480, 320, SDL_WINDOW_HIDDEN);
    renderer = window ? SDL_CreateRenderer(window, "software") : NULL;
    if (!renderer) {
        fprintf(stderr, "SDL software renderer failed: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    assert(aria_sdl3_video_open(&video, renderer));
    assert(aria_runtime_open(&runtime, &driver, "synthetic-only-no-rom-read"));
    assert(!aria_runtime_frame(&runtime, &frame));
    for (i = 0; i < 10; ++i) {
        assert(aria_runtime_step(&runtime, (AriaFrameInput){(uint16_t)i}));
        assert(aria_runtime_frame(&runtime, &frame));
        assert(SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255));
        assert(SDL_RenderClear(renderer));
        assert(aria_sdl3_video_present(&video, &frame, &destination));
        assert(SDL_RenderPresent(renderer));
    }
    aria_runtime_close(&runtime);
    assert(!aria_runtime_step(&runtime, (AriaFrameInput){0}));
    aria_sdl3_video_close(&video);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("Aria runtime-to-SDL3 pipeline: 10 synthetic frames passed.");
    return 0;
}
