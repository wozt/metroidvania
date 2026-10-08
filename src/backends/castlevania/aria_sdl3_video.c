#include "aria/sdl3_video.h"

#include <limits.h>
#include <stddef.h>

bool aria_sdl3_video_open(AriaSDL3Video *video, SDL_Renderer *renderer)
{
    if (!video || !renderer || video->texture)
        return false;
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             (int)ARIA_FRAME_WIDTH, (int)ARIA_FRAME_HEIGHT);
    if (!texture)
        return false;
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    video->renderer = renderer;
    video->texture = texture;
    return true;
}

bool aria_sdl3_video_present(AriaSDL3Video *video, const AriaFrameView *frame,
                             const SDL_FRect *destination)
{
    if (!video || !video->renderer || !video->texture || !frame || !destination ||
        !frame->rgba8888 || frame->width != ARIA_FRAME_WIDTH ||
        frame->height != ARIA_FRAME_HEIGHT ||
        frame->stride_pixels < frame->width ||
        frame->stride_pixels > INT_MAX / (int)sizeof(uint32_t) ||
        destination->w <= 0 || destination->h <= 0)
        return false;
    /* The runtime owns the framebuffer. UpdateTexture copies it into SDL. */
    if (!SDL_UpdateTexture(video->texture, NULL, frame->rgba8888,
                           (int)(frame->stride_pixels * sizeof(uint32_t))))
        return false;
    return SDL_RenderTexture(video->renderer, video->texture, NULL, destination);
}

void aria_sdl3_video_close(AriaSDL3Video *video)
{
    if (!video)
        return;
    if (video->texture)
        SDL_DestroyTexture(video->texture);
    video->texture = NULL;
    video->renderer = NULL;
}
