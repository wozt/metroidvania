/* SPDX-License-Identifier: GPL-3.0-only */
/* Diagnostic Aria title display host. All original artwork stays outside Git.
 * This is NOT the original GBA title renderer or an integrated boot sequence. */
#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aos_save.h"
#include "aos_title_fade.h"
#include "aos_title_input.h"
#include "aos_menu_preview.h"

enum { GBA_W = 240, GBA_H = 160, SCALE = 3 };
enum { KEY_A = 1, KEY_B = 2, KEY_START = 8, KEY_UP = 64, KEY_DOWN = 128 };

typedef struct {
    bool present;
    AosSaveData data;
    char path[1024];
} SlotView;

static uint16_t keymask(const bool *keys) {
    uint16_t held = 0;
    if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_SPACE]) held |= KEY_A;
    if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_BACKSPACE]) held |= KEY_B;
    if (keys[SDL_SCANCODE_RETURN]) held |= KEY_START;
    if (keys[SDL_SCANCODE_UP]) held |= KEY_UP;
    if (keys[SDL_SCANCODE_DOWN]) held |= KEY_DOWN;
    return held;
}

static bool slot_paths(SlotView slots[AOS_SAVE_SLOT_COUNT], const char *directory) {
    for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i) {
        int used = snprintf(slots[i].path, sizeof slots[i].path, "%s%saria-slot-%u.sav",
                            directory, directory[strlen(directory) - 1] == '/' ? "" : "/",
                            i + 1);
        if (used < 0 || (size_t)used >= sizeof slots[i].path) return false;
        slots[i].present = aos_save_load(slots[i].path, &slots[i].data, NULL, 0) &&
                           slots[i].data.slot == i;
    }
    return true;
}

static bool launch_runtime(const char *runtime, const char *save_path,
                           char *error, size_t error_size) {
    const char *arguments[] = {runtime, "--save", save_path, NULL};
    SDL_Process *process = SDL_CreateProcess(arguments, false);
    if (!process) {
        snprintf(error, error_size, "LAUNCH FAILED: %.90s", SDL_GetError());
        return false;
    }
    SDL_DestroyProcess(process);
    error[0] = '\0';
    return true;
}

int main(int argc, char **argv) {
    const char *image_path = NULL;
    const char *runtime_path = NULL, *save_directory_arg = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--title-bmp") && i + 1 < argc) image_path = argv[++i];
        else if (!strcmp(argv[i], "--runtime") && i + 1 < argc) runtime_path = argv[++i];
        else if (!strcmp(argv[i], "--save-dir") && i + 1 < argc) save_directory_arg = argv[++i];
        else {
            fprintf(stderr,
                    "Usage: fusion_aria_title_preview [--title-bmp FILE] [--runtime FILE] "
                    "[--save-dir DIR]\n");
            return 2;
        }
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    char runtime_buffer[1024];
    if (!runtime_path) {
        const char *base_path = SDL_GetBasePath();
        if (!base_path) {
            fprintf(stderr, "Cannot resolve runtime directory: %s\n", SDL_GetError());
            SDL_Quit();
            return 2;
        }
        int used = snprintf(runtime_buffer, sizeof runtime_buffer, "%sfusion_aria_runtime",
                            base_path);
        if (used < 0 || (size_t)used >= sizeof runtime_buffer) {
            fprintf(stderr, "Runtime path is too long\n");
            SDL_Quit();
            return 2;
        }
        runtime_path = runtime_buffer;
    }
    char *preference_path = NULL;
    const char *save_directory = save_directory_arg;
    if (!save_directory) {
        preference_path = SDL_GetPrefPath("wozt", "metroidvania");
        save_directory = preference_path;
    } else if (!SDL_CreateDirectory(save_directory)) {
        fprintf(stderr, "Cannot create save directory: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }
    SlotView slots[AOS_SAVE_SLOT_COUNT] = {0};
    if (!save_directory || !*save_directory || !slot_paths(slots, save_directory)) {
        fprintf(stderr, "Cannot prepare Aria save slots\n");
        SDL_free(preference_path);
        SDL_Quit();
        return 2;
    }
    SDL_Window *window = SDL_CreateWindow("Aria title - reconstruction preview",
                                           GBA_W * SCALE, GBA_H * SCALE, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    if (!renderer) {
        fprintf(stderr, "Unable to create SDL renderer: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        SDL_free(preference_path);
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
            SDL_free(preference_path);
            SDL_Quit();
            return 2;
        }
        art = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
        if (!art) {
            fprintf(stderr, "Cannot create title texture: %s\n", SDL_GetError());
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_free(preference_path);
            SDL_Quit();
            return 2;
        }
    }
    AosTitleFade fade = {0};
    AosTitleInput input = {0};
    AosMenuPreview menu = {0};
    aos_menu_preview_init(&menu);
    for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i)
        aos_menu_preview_set_slot(&menu, i, slots[i].present);
    aos_title_fade_init(&fade);
    aos_title_fade_begin(&fade);
    aos_title_input_enter(&input);
    bool running = true;
    bool interactive = false;
    uint16_t previous = 0;
    uint64_t next_frame = SDL_GetTicksNS();
    unsigned frame = 0;
    char launch_error[128] = "";
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
                unsigned slot = 0;
                AosPreviewAction action = aos_menu_preview_take_action(&menu, &slot);
                if (action != AOS_PREVIEW_ACTION_NONE) {
                    bool ready = true;
                    if (action == AOS_PREVIEW_ACTION_CREATE) {
                        aos_save_init_new(&slots[slot].data, slot);
                        ready = aos_save_store(slots[slot].path, &slots[slot].data,
                                               launch_error, sizeof launch_error);
                        if (ready) {
                            slots[slot].present = true;
                            aos_menu_preview_set_slot(&menu, slot, true);
                        }
                    }
                    if (ready && launch_runtime(runtime_path, slots[slot].path,
                                                launch_error, sizeof launch_error))
                        running = false;
                }
            }
        }
        if (!running) break;
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
        /* Host text, not the original Aria menu art or transition rendering. */
        if (menu.screen != AOS_PREVIEW_TITLE) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 10, 12, 24, 235);
            SDL_FRect sheet = {12, 15, 216, 130};
            SDL_RenderFillRect(renderer, &sheet);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 245, 235, 210, 255);
            if (menu.screen == AOS_PREVIEW_MENU) {
                const char *entries[] = {"NEW GAME", "CONTINUE", "OPTIONS", "BACK"};
                SDL_RenderDebugText(renderer, 30, 24, "ARIA MAIN MENU");
                for (unsigned i = 0; i < 4; ++i) {
                    SDL_RenderDebugText(renderer, 32, 47 + 20 * (float)i, entries[i]);
                    if (menu.selected == i)
                        SDL_RenderDebugText(renderer, 19, 47 + 20 * (float)i, ">");
                }
            } else if (menu.screen == AOS_PREVIEW_FILE_SELECT) {
                SDL_RenderDebugText(renderer, 28, 25,
                    menu.file_mode == AOS_PREVIEW_NEW_GAME ? "NEW GAME - SELECT FILE"
                                                          : "CONTINUE - SELECT FILE");
                for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i) {
                    char line[80];
                    if (slots[i].present)
                        snprintf(line, sizeof line, "FILE %u  A%02u R%03u  HP %d", i + 1,
                                 slots[i].data.area, slots[i].data.room, slots[i].data.hp);
                    else
                        snprintf(line, sizeof line, "FILE %u  EMPTY", i + 1);
                    SDL_RenderDebugText(renderer, 32, 50 + 22 * (float)i, line);
                    if (menu.slot_selected == i)
                        SDL_RenderDebugText(renderer, 19, 50 + 22 * (float)i, ">");
                }
                SDL_RenderDebugText(renderer, 25, 120, "A: SELECT   B: BACK");
            } else if (menu.screen == AOS_PREVIEW_OVERWRITE) {
                char prompt[64];
                snprintf(prompt, sizeof prompt, "OVERWRITE FILE %u?", menu.slot_selected + 1);
                SDL_RenderDebugText(renderer, 48, 45, prompt);
                SDL_RenderDebugText(renderer, 65, 75, menu.confirm_yes ? "> YES" : "  YES");
                SDL_RenderDebugText(renderer, 65, 95, menu.confirm_yes ? "  NO" : "> NO");
            } else if (menu.screen == AOS_PREVIEW_OPTIONS) {
                SDL_RenderDebugText(renderer, 30, 38, "OPTIONS");
                SDL_RenderDebugText(renderer, 20, 64,
                    menu.show_background ? "ART: VISIBLE" : "ART: HIDDEN");
                SDL_RenderDebugText(renderer, 20, 84, "A: TOGGLE ART");
                SDL_RenderDebugText(renderer, 20, 104, "B: BACK");
            }
            if (launch_error[0]) SDL_RenderDebugText(renderer, 18, 135, launch_error);
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
    SDL_free(preference_path);
    SDL_Quit();
    return 0;
}
