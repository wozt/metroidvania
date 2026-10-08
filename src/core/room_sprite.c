/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/room_sprite.h"
#include <stdio.h>
#include <string.h>

static const char *const names[] = {
    "idle", "run", "jump", "attack", "run_start", "run_stop"
};
static const char *const actors[] = {"samus", "soma"};
static const unsigned frame_counts[FUSION_CHARACTER_COUNT][SPRITE_STATE_COUNT] = {
    [CHARACTER_SAMUS] = {4, 10, 8, 3, 0, 0},
    [CHARACTER_SOMA] = {4, 17, 12, 11, 3, 9},
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
        [SPRITE_RUN] = {4, 3, 4, 3, 4, 3, 4, 3, 4, 3, 4, 2, 2, 3, 3, 4, 3},
        [SPRITE_JUMP] = {5, 5, 7, 7, 2, 5, 5, 5, 5, 3, 5, 7},
        [SPRITE_ATTACK] = {3, 2, 3, 6, 2, 3, 3, 5, 7, 7, 7},
        [SPRITE_RUN_START] = {2, 3, 3},
        [SPRITE_RUN_STOP] = {4, 5, 8, 7, 7, 9, 13, 14, 19},
    },
};

unsigned room_sprite_duration_ticks(CharacterKind character, int action)
{
    unsigned cycle = 0;
    if ((unsigned)character >= FUSION_CHARACTER_COUNT ||
        action < 0 || action >= SPRITE_STATE_COUNT)
        return 0;
    for (unsigned i = 0; i < frame_counts[character][action]; ++i)
        cycle += frame_durations[character][action][i];
    return cycle;
}

unsigned room_sprite_frame_index(CharacterKind character, int action, float elapsed)
{
    unsigned count;
    unsigned cycle;
    unsigned tick;
    if ((unsigned)character >= FUSION_CHARACTER_COUNT ||
        action < 0 || action >= SPRITE_STATE_COUNT)
        return 0;
    count = frame_counts[character][action];
    cycle = room_sprite_duration_ticks(character, action);
    if (!count || !cycle) return 0;
    /* Avoid losing exact frame boundaries to binary float representation. */
    tick = (unsigned)(elapsed > 0 ? elapsed * 60.f + 0.0001f : 0) % cycle;
    for (unsigned i = 0; i < count; ++i) {
        unsigned duration = frame_durations[character][action][i];
        if (tick < duration) return i;
        tick -= duration;
    }
    return 0;
}

int room_sprite_select_animation(CharacterKind character, int current,
                                 bool moving, bool on_ground, bool attacking,
                                 float elapsed)
{
    unsigned elapsed_ticks = (unsigned)(elapsed > 0 ? elapsed * 60.f + 0.0001f : 0);
    if (attacking) return SPRITE_ATTACK;
    if (!on_ground) return SPRITE_JUMP;
    if (character != CHARACTER_SOMA) return moving ? SPRITE_RUN : SPRITE_IDLE;
    if (moving) {
        if (current == SPRITE_RUN) return SPRITE_RUN;
        if (current == SPRITE_RUN_START &&
            elapsed_ticks < room_sprite_duration_ticks(character, SPRITE_RUN_START))
            return SPRITE_RUN_START;
        return current == SPRITE_RUN_START ? SPRITE_RUN : SPRITE_RUN_START;
    }
    if (current == SPRITE_RUN || current == SPRITE_RUN_START)
        return SPRITE_RUN_STOP;
    if (current == SPRITE_RUN_STOP &&
        elapsed_ticks < room_sprite_duration_ticks(character, SPRITE_RUN_STOP))
        return SPRITE_RUN_STOP;
    return SPRITE_IDLE;
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
