/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ROOM_SPRITE_H
#define FUSION_ROOM_SPRITE_H
#include <SDL3/SDL.h>
#include <stdbool.h>
#include "core/types.h"

enum { SPRITE_IDLE, SPRITE_RUN, SPRITE_JUMP, SPRITE_ATTACK, SPRITE_STATE_COUNT };
enum { SPRITE_FRAMES = 4 };

typedef struct {
    SDL_Texture *frames[FUSION_CHARACTER_COUNT][SPRITE_STATE_COUNT][SPRITE_FRAMES];
    SDL_Renderer *renderer;
    bool attempted;
} RoomSprites;

void room_sprites_load(RoomSprites *sprites, SDL_Renderer *renderer);
bool room_sprites_draw(RoomSprites *sprites, SDL_Renderer *renderer,
                       CharacterKind character, int action, float elapsed,
                       bool facing_left, float center_x, float feet_y,
                       float target_height);
void room_sprites_close(RoomSprites *sprites);
#endif
