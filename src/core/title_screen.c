/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/title_screen.h"

#include <SDL3/SDL.h>

static void text(SDL_Renderer *renderer, float x, float y,
                 const char *message, SDL_Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderDebugText(renderer, x, y, message);
}

static void card(SDL_Renderer *renderer, SDL_FRect bounds,
                 SDL_Color color, bool selected)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, &bounds);
    SDL_SetRenderDrawColor(renderer, selected ? 250 : 110,
                           selected ? 238 : 110, selected ? 208 : 130, 255);
    SDL_RenderRect(renderer, &bounds);
}

bool fusion_title_choose(SDL_Renderer *renderer, WorldKind *world)
{
    WorldKind selected = WORLD_METROID;
    if (!renderer || !world) return false;
    for (;;) {
        SDL_Event event;
        int w = 960, h = 540;
        float unit, cx, cy;
        SDL_FRect left, right;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) return false;
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                switch (event.key.key) {
                case SDLK_ESCAPE: return false;
                case SDLK_LEFT: case SDLK_1: selected = WORLD_METROID; break;
                case SDLK_RIGHT: case SDLK_2: selected = WORLD_CASTLEVANIA; break;
                case SDLK_TAB:
                    selected = selected == WORLD_METROID
                        ? WORLD_CASTLEVANIA : WORLD_METROID;
                    break;
                case SDLK_RETURN: case SDLK_SPACE: *world = selected; return true;
                default: break;
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                float px = event.button.x;
                float py = event.button.y;
                int ww = 960, hh = 540;
                (void)SDL_GetRenderOutputSize(renderer, &ww, &hh);
                if (py >= (float)hh * .41f && py <= (float)hh * .81f) {
                    selected = px < (float)ww * .50f
                        ? WORLD_METROID : WORLD_CASTLEVANIA;
                    *world = selected;
                    return true;
                }
            }
        }
        (void)SDL_GetRenderOutputSize(renderer, &w, &h);
        unit = (float)w / 960.f;
        cx = (float)w * .5f;
        cy = (float)h * .5f;
        left = (SDL_FRect){cx - 340.f * unit, cy - 49.f * unit,
                           315.f * unit, 185.f * unit};
        right = (SDL_FRect){cx + 25.f * unit, cy - 49.f * unit,
                            315.f * unit, 185.f * unit};
        SDL_SetRenderDrawColor(renderer, 8, 11, 22, 255);
        SDL_RenderClear(renderer);
        text(renderer, cx - 89.f * unit, cy - 162.f * unit,
             "M E T R O I D   V A N I A", (SDL_Color){238,222,190,255});
        text(renderer, cx - 91.f * unit, cy - 135.f * unit,
             "TWO WORLDS / ONE JOURNEY", (SDL_Color){152,162,185,255});
        card(renderer, left, (SDL_Color){21,52,58,255},
             selected == WORLD_METROID);
        card(renderer, right, (SDL_Color){55,25,51,255},
             selected == WORLD_CASTLEVANIA);
        text(renderer, left.x + 20.f, left.y + 43.f, "01  METROID",
             (SDL_Color){156,241,216,255});
        text(renderer, left.x + 20.f, left.y + 71.f, "Start in Zero Mission world",
             (SDL_Color){220,230,227,255});
        text(renderer, right.x + 20.f, right.y + 43.f, "02  CASTLEVANIA",
             (SDL_Color){251,174,210,255});
        text(renderer, right.x + 20.f, right.y + 71.f, "Start in Aria of Sorrow world",
             (SDL_Color){237,219,229,255});
        text(renderer, cx - 146.f * unit, cy + 173.f * unit,
             "LEFT/RIGHT: SELECT    ENTER: START    ESC: EXIT",
             (SDL_Color){183,187,204,255});
        text(renderer, cx - 144.f * unit, cy + 200.f * unit,
             "PC ENGINE PROTOTYPE - MAPS ARE DEMONSTRATIONS",
             (SDL_Color){138,147,164,255});
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}
