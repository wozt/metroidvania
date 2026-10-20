/* SPDX-License-Identifier: GPL-3.0-only */
/* Diagnostic Aria title display host. All original artwork stays outside Git.
 * This is NOT the original GBA title renderer or an integrated boot sequence. */
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aos_title_fade.h"
#include "aos_title_input.h"
#include "aos_menu_preview.h"

enum { GBA_W = 240, GBA_H = 160, SCALE = 3 };
enum { KEY_A = 1, KEY_B = 2, KEY_START = 8, KEY_UP = 64, KEY_DOWN = 128 };

static uint16_t keymask(const bool *keys) {
    uint16_t held = 0;
    if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_SPACE]) held |= KEY_A;
    if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_BACKSPACE]) held |= KEY_B;
    if (keys[SDL_SCANCODE_RETURN]) held |= KEY_START;
    if (keys[SDL_SCANCODE_UP]) held |= KEY_UP;
    if (keys[SDL_SCANCODE_DOWN]) held |= KEY_DOWN;
    return held;
}

int main(int argc, char **argv) {
    const char *image_path = NULL;
    if (argc == 3 && strcmp(argv[1], "--title-bmp") == 0)
        image_path = argv[2];
    else if (argc != 1) {
        fprintf(stderr, "Usage: fusion_aria_title_preview [--title-bmp /path/to/title.bmp]\n");
        return 2;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("Aria title - reconstruction preview",
                                           GBA_W * SCALE, GBA_H * SCALE, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    if (!renderer) {
        fprintf(stderr, "Unable to create SDL renderer: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderLogicalPresentation(renderer, GBA_W, GBA_H,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_Texture *art = NULL;
    if (image_path) {
        SDL_Surface *surface = SDL_LoadBMP(image_path);
        if (!surface) {
            fprintf(stderr, "Cannot load local title BMP: %s\n", SDL_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 2;
        }
        art = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
        if (!art) {
            fprintf(stderr, "Cannot create title texture: %s\n", SDL_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 2;
        }
    }
    AosTitleFade fade = {0};
    AosTitleInput input = {0};
    AosMenuPreview menu = {0};
    aos_menu_preview_init(&menu);
    aos_title_fade_init(&fade);
    aos_title_fade_begin(&fade);
    aos_title_input_enter(&input);
    bool running = true;
    bool interactive = false;
    uint16_t previous = 0;
    uint64_t next_frame = SDL_GetTicksNS();
    unsigned frame = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
                running = false;
        }
        if (!running) break;
        const uint16_t held = keymask(SDL_GetKeyboardState(NULL));
        const uint16_t edge = held & (uint16_t)~previous;
        previous = held;
        if (fade.stage == 2) {
            aos_title_fade_tick(&fade);
        } else {
            interactive = true;
            if (menu.screen == AOS_PREVIEW_TITLE) {
                AosTitleAction action = aos_title_input_tick(&input, edge,
                    (uint16_t)((frame % 6u == 0u) ? held : 0u), false, false, false);
                if (action == AOS_TITLE_EXIT)
                    aos_menu_preview_press(&menu, KEY_START);
            } else {
                aos_menu_preview_tick(&menu, held, edge);
            }
        }
        SDL_SetRenderDrawColor(renderer, 18, 14, 28, 255);
        SDL_RenderClear(renderer);
        if (art && menu.show_background) {
            SDL_FRect dst = {0, 0, GBA_W, GBA_H};
            SDL_RenderTexture(renderer, art, NULL, &dst);
        } else {
            /* Deliberately plain placeholders, not invented Konami artwork. */
            SDL_SetRenderDrawColor(renderer, 65, 58, 82, 255);
            SDL_FRect plate = {12, 35, 216, 90};
            SDL_RenderFillRect(renderer, &plate);
            SDL_SetRenderDrawColor(renderer, 140, 130, 155, 255);
            SDL_FRect stripe = {23, 76, 194, 7};
            SDL_RenderFillRect(renderer, &stripe);
        }
        /* SDL diagnostic text, not original Aria menu art or game startup. */
        if (menu.screen != AOS_PREVIEW_TITLE) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 10, 12, 24, 235);
            SDL_FRect sheet = {12, 15, 216, 130};
            SDL_RenderFillRect(renderer, &sheet);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 245, 235, 210, 255);
            if (menu.screen == AOS_PREVIEW_MENU) {
                const char *entries[] = {"NEW GAME", "CONTINUE", "OPTIONS", "BACK"};
                SDL_RenderDebugText(renderer, 30, 24, "ARIA MENU PREVIEW");
                for (unsigned i = 0; i < 4; ++i) {
                    SDL_RenderDebugText(renderer, 32, 47 + 20 * (float)i, entries[i]);
                    if (menu.selected == i)
                        SDL_RenderDebugText(renderer, 19, 47 + 20 * (float)i, ">");
                }
            } else {
                const char *names[] = {"NEW GAME", "CONTINUE", "OPTIONS"};
                SDL_RenderDebugText(renderer, 30, 38, names[menu.notice_kind]);
                if (menu.notice_kind == 2) {
                    SDL_RenderDebugText(renderer, 20, 64,
                        menu.show_background ? "ART: VISIBLE" : "ART: HIDDEN");
                    SDL_RenderDebugText(renderer, 20, 84, "A: TOGGLE ART");
                } else {
                    SDL_RenderDebugText(renderer, 30, 64, "NOT IMPLEMENTED YET");
                }
                SDL_RenderDebugText(renderer, 30, 96, "A/B: BACK");
            }
        }
        uint8_t darkness = (uint8_t)(fade.stage == 2 ? fade.brightness : 0);
        if (darkness > 0) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0,
                                   (uint8_t)(darkness * 255u / 16u));
            SDL_FRect whole = {0, 0, GBA_W, GBA_H};
            SDL_RenderFillRect(renderer, &whole);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
        /* Diagnostic 'ready' light; not part of native GBA imagery. */
        if (interactive) {
            SDL_SetRenderDrawColor(renderer, 120, 200, 145, 255);
            SDL_FRect marker = {227, 147, 5, 5};
            SDL_RenderFillRect(renderer, &marker);
        }
        SDL_RenderPresent(renderer);
        ++frame;
        next_frame += 1000000000ull / 60ull;
        uint64_t now = SDL_GetTicksNS();
        if (now < next_frame) SDL_DelayNS(next_frame - now);
        else if (now - next_frame > 100000000ull) next_frame = now;
    }
    if (art) SDL_DestroyTexture(art);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
