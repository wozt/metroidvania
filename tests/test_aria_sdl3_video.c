#include "aria/sdl3_video.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t pixels[ARIA_FRAME_WIDTH * ARIA_FRAME_HEIGHT];

int main(void)
{
    SDL_Surface *surface = SDL_CreateSurface(480, 320, SDL_PIXELFORMAT_RGBA8888);
    assert(surface);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    assert(renderer);
    AriaSDL3Video video = {0};
    AriaFrameView frame = {pixels, ARIA_FRAME_WIDTH, ARIA_FRAME_HEIGHT, ARIA_FRAME_WIDTH};
    SDL_FRect dest = {0.0f, 0.0f, 480.0f, 320.0f};
    pixels[0] = 0xff00ffffu;
    assert(!aria_sdl3_video_present(&video, &frame, &dest));
    assert(aria_sdl3_video_open(&video, renderer));
    assert(!aria_sdl3_video_open(&video, renderer));
    assert(aria_sdl3_video_present(&video, &frame, &dest));
    frame.stride_pixels = 1;
    assert(!aria_sdl3_video_present(&video, &frame, &dest));
    frame.stride_pixels = UINT32_MAX;
    assert(!aria_sdl3_video_present(&video, &frame, &dest));
    frame.stride_pixels = ARIA_FRAME_WIDTH;
    frame.rgba8888 = NULL;
    assert(!aria_sdl3_video_present(&video, &frame, &dest));
    aria_sdl3_video_close(&video);
    aria_sdl3_video_close(&video);
    assert(aria_sdl3_video_open(&video, renderer));
    aria_sdl3_video_close(&video);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    puts("Aria SDL3 framebuffer bridge tests passed.");
    return 0;
}
