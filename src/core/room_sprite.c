/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/room_sprite.h"
#include <stdio.h>
#include <string.h>

static const char *const names[] = {"idle", "run", "jump", "attack"};
static const char *const actors[] = {"samus", "soma"};
/* MZM's verified Power Suit standing cycle holds each of its four frames for
 * 16 updates at 60 Hz; its running cycle holds ten frames for two updates. */
static const float samus_idle_frame_rate = 60.f / 16.f;
static const float samus_run_frame_rate = 60.f / 2.f;
static const unsigned frame_counts[FUSION_CHARACTER_COUNT][SPRITE_STATE_COUNT] = {
    [CHARACTER_SAMUS] = {4, 10, 4, 4},
    [CHARACTER_SOMA] = {4, 4, 4, 4},
};

void room_sprites_close(RoomSprites *sprites)
{
    unsigned c, s, f;
    if (!sprites) return;
    for (c = 0; c < FUSION_CHARACTER_COUNT; ++c)
        for (s = 0; s < SPRITE_STATE_COUNT; ++s)
            for (f = 0; f < SPRITE_MAX_FRAMES; ++f) {
                SDL_DestroyTexture(sprites->frames[c][s][f]);
                sprites->frames[c][s][f] = NULL;
            }
    sprites->renderer = NULL;
    sprites->attempted = false;
}

void room_sprites_load(RoomSprites *sprites, SDL_Renderer *renderer)
{
    unsigned c, s, f;
    if (!sprites || !renderer) return;
    if (sprites->attempted && sprites->renderer == renderer) return;
    room_sprites_close(sprites);
    sprites->renderer = renderer;
    sprites->attempted = true;
    for (c = 0; c < FUSION_CHARACTER_COUNT; ++c)
        for (s = 0; s < SPRITE_STATE_COUNT; ++s)
            for (f = 0; f < SPRITE_MAX_FRAMES; ++f) {
                char path[256];
                SDL_Surface *surface;
                SDL_Texture *texture;
                snprintf(path, sizeof(path),
                    "assets/extracted/sprites/%s/%s_%u.bmp", actors[c], names[s], f);
                surface = SDL_LoadBMP(path);
                if (!surface) continue;
                texture = SDL_CreateTextureFromSurface(renderer, surface);
                SDL_DestroySurface(surface);
                if (!texture) continue;
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
                sprites->frames[c][s][f] = texture;
            }
}

bool room_sprites_draw(RoomSprites *sprites, SDL_Renderer *renderer,
                       CharacterKind character, int action, float elapsed,
                       bool facing_left, float center_x, float feet_y,
                       float target_height)
{
    SDL_Texture *texture = NULL;
    float width, height;
    SDL_FRect dst;
    int n;
    unsigned count, idx;
    if (!sprites || !renderer || (unsigned)character >= FUSION_CHARACTER_COUNT ||
        action < 0 || action >= SPRITE_STATE_COUNT || target_height <= 0) return false;
    room_sprites_load(sprites, renderer);
    /* Use an available frame of the action; fall back to idle for incomplete sheets. */
    count = frame_counts[character][action];
    float frame_rate = 9.f;
    if (character == CHARACTER_SAMUS && action == SPRITE_IDLE)
        frame_rate = samus_idle_frame_rate;
    else if (character == CHARACTER_SAMUS && action == SPRITE_RUN)
        frame_rate = samus_run_frame_rate;
    n = elapsed > 0 ? (int)(elapsed * frame_rate) : 0;
    idx = (unsigned)n % count;
    for (unsigned i = 0; i < count; ++i) {
        texture = sprites->frames[character][action][(idx + i) % count];
        if (texture) break;
    }
    if (!texture) texture = sprites->frames[character][SPRITE_IDLE][0];
    if (!texture || !SDL_GetTextureSize(texture, &width, &height) || height <= 0) return false;
    width *= target_height / height;
    dst = (SDL_FRect){center_x - width * .5f, feet_y - target_height, width, target_height};
    return SDL_RenderTextureRotated(renderer, texture, NULL, &dst, 0, NULL,
                                   facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
