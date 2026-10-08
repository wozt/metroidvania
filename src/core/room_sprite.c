/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/room_sprite.h"
#include <stdio.h>
#include <string.h>

static const char *const names[] = {"idle", "run", "jump", "attack"};
static const char *const actors[] = {"samus", "soma"};
static const unsigned frame_counts[FUSION_CHARACTER_COUNT][SPRITE_STATE_COUNT] = {
    [CHARACTER_SAMUS] = {4, 10, 8, 3},
    [CHARACTER_SOMA] = {4, 4, 4, 4},
};
/* Verified durations use the games' native 60 Hz update unit. */
static const unsigned char frame_durations[FUSION_CHARACTER_COUNT]
                                                  [SPRITE_STATE_COUNT]
                                                  [SPRITE_MAX_FRAMES] = {
    [CHARACTER_SAMUS] = {
        [SPRITE_IDLE] = {16, 16, 16, 16},
        [SPRITE_RUN] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
        [SPRITE_JUMP] = {2, 1, 2, 1, 2, 1, 2, 1},
        [SPRITE_ATTACK] = {2, 2, 4},
    },
    [CHARACTER_SOMA] = {
        [SPRITE_IDLE] = {30, 11, 11, 11},
        /* Temporary timing for states whose authentic graphics are absent. */
        [SPRITE_RUN] = {7, 7, 7, 6},
        [SPRITE_JUMP] = {7, 7, 7, 6},
        [SPRITE_ATTACK] = {7, 7, 7, 6},
    },
};

unsigned room_sprite_frame_index(CharacterKind character, int action, float elapsed)
{
    unsigned count;
    unsigned cycle = 0;
    unsigned tick;
    if ((unsigned)character >= FUSION_CHARACTER_COUNT ||
        action < 0 || action >= SPRITE_STATE_COUNT)
        return 0;
    count = frame_counts[character][action];
    for (unsigned i = 0; i < count; ++i)
        cycle += frame_durations[character][action][i];
    /* Avoid losing exact frame boundaries to binary float representation. */
    tick = (unsigned)(elapsed > 0 ? elapsed * 60.f + 0.0001f : 0) % cycle;
    for (unsigned i = 0; i < count; ++i) {
        unsigned duration = frame_durations[character][action][i];
        if (tick < duration) return i;
        tick -= duration;
    }
    return 0;
}

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
    unsigned count, idx;
    if (!sprites || !renderer || (unsigned)character >= FUSION_CHARACTER_COUNT ||
        action < 0 || action >= SPRITE_STATE_COUNT || target_height <= 0) return false;
    room_sprites_load(sprites, renderer);
    /* Use an available frame of the action; fall back to idle for incomplete sheets. */
    count = frame_counts[character][action];
    idx = room_sprite_frame_index(character, action, elapsed);
    for (unsigned i = 0; i < count; ++i) {
        texture = sprites->frames[character][action][(idx + i) % count];
        if (texture) break;
    }
    if (!texture) {
        count = frame_counts[character][SPRITE_IDLE];
        idx = room_sprite_frame_index(character, SPRITE_IDLE, elapsed);
        for (unsigned i = 0; i < count; ++i) {
            texture = sprites->frames[character][SPRITE_IDLE][(idx + i) % count];
            if (texture) break;
        }
    }
    if (!texture || !SDL_GetTextureSize(texture, &width, &height) || height <= 0) return false;
    width *= target_height / height;
    dst = (SDL_FRect){center_x - width * .5f, feet_y - target_height, width, target_height};
    return SDL_RenderTextureRotated(renderer, texture, NULL, &dst, 0, NULL,
                                   facing_left ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
