/* SPDX-License-Identifier: GPL-3.0-only */
/* Diagnostic presentation host for the integrated native Aria boot and menu
 * controllers. All original artwork stays outside Git; rendering is not the
 * native GBA renderer. */
#include <SDL3/SDL.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "aos_game_lifecycle.h"
#include "aos_intro_font.h"
#include "aos_intro_graphics.h"
#include "aos_intro_resources.h"
#include "aos_main_menu_graphics.h"
#include "aos_menu_save_bridge.h"
#include "aos_save.h"

enum { GBA_W = 240, GBA_H = 160, SCALE = 3 };

typedef struct {
    bool present;
    AosSaveData data;
    char path[1024];
} SlotView;

static uint16_t keymask(const bool *keys) {
    uint16_t held = 0;
    if (keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_SPACE]) held |= AOS_KEY_A;
    if (keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_BACKSPACE]) held |= AOS_KEY_B;
    if (keys[SDL_SCANCODE_RETURN]) held |= AOS_KEY_START;
    if (keys[SDL_SCANCODE_RIGHT]) held |= AOS_KEY_RIGHT;
    if (keys[SDL_SCANCODE_LEFT]) held |= AOS_KEY_LEFT;
    if (keys[SDL_SCANCODE_UP]) held |= AOS_KEY_UP;
    if (keys[SDL_SCANCODE_DOWN]) held |= AOS_KEY_DOWN;
    if (keys[SDL_SCANCODE_S]) held |= AOS_KEY_R;
    if (keys[SDL_SCANCODE_A]) held |= AOS_KEY_L;
    return held;
}

static bool persist_bridge(AosMenuSaveBridge *bridge,
                           SlotView slots[AOS_SAVE_SLOT_COUNT], char *error,
                           size_t error_size) {
    for (unsigned slot = 0; slot < AOS_SAVE_SLOT_COUNT; ++slot) {
        if (bridge->removed_mask & (1u << slot)) {
            if (remove(slots[slot].path) != 0 && errno != ENOENT) {
                snprintf(error, error_size, "DELETE FAILED: %.80s", slots[slot].path);
                return false;
            }
        }
        if ((bridge->changed_mask & (1u << slot)) &&
            !aos_save_store(slots[slot].path, &bridge->slots[slot], error, error_size))
            return false;
        slots[slot].present = bridge->present[slot];
        slots[slot].data = bridge->slots[slot];
    }
    return true;
}

static void display_name(char out[9], const uint8_t encoded[8]) {
    for (unsigned i = 0; i < 8; ++i) {
        uint8_t value = encoded[i];
        out[i] = value >= 2 && value <= 27 ? (char)('A' + value - 2) :
                 value == 0 ? ' ' : '?';
    }
    out[8] = '\0';
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

static void render_intro_text(SDL_Renderer *renderer,
                              const AosIntroTextTiles *tiles,
                              int16_t vertical, const uint8_t rgb[3]) {
    SDL_SetRenderDrawColor(renderer, rgb[0], rgb[1], rgb[2], 255);
    for (unsigned y = 0; y < AOS_INTRO_TEXT_CANVAS_SIZE; ++y) {
        unsigned screen_y = (unsigned)((int)y - vertical) & 0xFFu;
        if (screen_y >= GBA_H) continue;
        for (unsigned x = 0; x < GBA_W; ++x) {
            if (aos_intro_text_tiles_pixel(tiles, x, y))
                SDL_RenderPoint(renderer, (float)x, (float)screen_y);
        }
    }
}

static bool create_intro_textures(
    SDL_Renderer *renderer, const AosIntroGraphics *graphics,
    SDL_Texture *textures[AOS_INTRO_BACKGROUND_COUNT]) {
    for (unsigned index = 0; index < AOS_INTRO_BACKGROUND_COUNT; ++index) {
        const FusionAosRenderedBackground *background =
            &graphics->backgrounds[index];
        textures[index] = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
            (int)background->width_pixels, (int)background->height_pixels);
        if (!textures[index] ||
            !SDL_UpdateTexture(textures[index], NULL, background->rgba,
                               (int)(background->width_pixels * 4)) ||
            !SDL_SetTextureScaleMode(textures[index], SDL_SCALEMODE_NEAREST) ||
            !SDL_SetTextureBlendMode(textures[index], SDL_BLENDMODE_BLEND))
            return false;
    }
    return true;
}

static void destroy_intro_textures(
    SDL_Texture *textures[AOS_INTRO_BACKGROUND_COUNT]) {
    for (unsigned index = 0; index < AOS_INTRO_BACKGROUND_COUNT; ++index) {
        if (textures[index]) SDL_DestroyTexture(textures[index]);
        textures[index] = NULL;
    }
}

static void render_wrapped_background(
    SDL_Renderer *renderer, SDL_Texture *texture,
    const FusionAosRenderedBackground *background,
    int32_t scroll_x, int32_t scroll_y) {
    int width = (int)background->width_pixels;
    int height = (int)background->height_pixels;
    int offset_x = scroll_x % width;
    int offset_y = scroll_y % height;
    if (offset_x < 0) offset_x += width;
    if (offset_y < 0) offset_y += height;
    for (int y = -offset_y; y < GBA_H; y += height) {
        for (int x = -offset_x; x < GBA_W; x += width) {
            SDL_FRect destination = {
                (float)x, (float)y, (float)width, (float)height};
            SDL_RenderTexture(renderer, texture, NULL, &destination);
        }
    }
}

static void render_intro_backgrounds(
    SDL_Renderer *renderer, const AosIntroGraphics *graphics,
    SDL_Texture *textures[AOS_INTRO_BACKGROUND_COUNT],
    const AosIntroCutscene *intro) {
    SDL_SetRenderDrawColor(renderer, graphics->backdrop_rgb[0],
                           graphics->backdrop_rgb[1],
                           graphics->backdrop_rgb[2], 255);
    SDL_FRect screen = {0, 0, GBA_W, GBA_H};
    SDL_RenderFillRect(renderer, &screen);
    for (int index = AOS_INTRO_BACKGROUND_COUNT - 1; index >= 0; --index) {
        unsigned layer = (unsigned)index + 1;
        render_wrapped_background(renderer, textures[index],
                                  &graphics->backgrounds[index],
                                  0,
                                  intro->bg_y[layer] >> 16);
    }
}

static bool create_main_menu_textures(
    SDL_Renderer *renderer, const AosMainMenuGraphics *graphics,
    SDL_Texture *textures[AOS_MAIN_GRAPHICS_SCREEN_COUNT]
                         [AOS_MAIN_GRAPHICS_LAYER_COUNT]) {
    for (unsigned screen = 0; screen < AOS_MAIN_GRAPHICS_SCREEN_COUNT;
         ++screen) {
        for (unsigned layer = 0; layer < AOS_MAIN_GRAPHICS_LAYER_COUNT;
             ++layer) {
            const FusionAosRenderedBackground *background =
                &graphics->backgrounds[screen][layer];
            textures[screen][layer] = SDL_CreateTexture(
                renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                (int)background->width_pixels, (int)background->height_pixels);
            if (!textures[screen][layer] ||
                !SDL_UpdateTexture(textures[screen][layer], NULL,
                                   background->rgba,
                                   (int)(background->width_pixels * 4)) ||
                !SDL_SetTextureScaleMode(textures[screen][layer],
                                         SDL_SCALEMODE_NEAREST) ||
                !SDL_SetTextureBlendMode(textures[screen][layer],
                                         SDL_BLENDMODE_BLEND))
                return false;
        }
    }
    return true;
}

static void destroy_main_menu_textures(
    SDL_Texture *textures[AOS_MAIN_GRAPHICS_SCREEN_COUNT]
                         [AOS_MAIN_GRAPHICS_LAYER_COUNT]) {
    for (unsigned screen = 0; screen < AOS_MAIN_GRAPHICS_SCREEN_COUNT;
         ++screen) {
        for (unsigned layer = 0; layer < AOS_MAIN_GRAPHICS_LAYER_COUNT;
             ++layer) {
            if (textures[screen][layer])
                SDL_DestroyTexture(textures[screen][layer]);
            textures[screen][layer] = NULL;
        }
    }
}

static AosMainGraphicsScreen main_menu_graphics_screen(
    const AosMainMenu *menu, int32_t stage) {
    if (menu->name_entry_active || stage == 8 || stage == 9)
        return AOS_MAIN_GRAPHICS_NAME;
    if (!menu->selected_slot_active) return AOS_MAIN_GRAPHICS_BASE;
    if (menu->selected_slot == 1) return AOS_MAIN_GRAPHICS_COPY;
    if (menu->selected_slot == 2) return AOS_MAIN_GRAPHICS_DELETE;
    return AOS_MAIN_GRAPHICS_PLAY;
}

static void render_main_menu_backgrounds(
    SDL_Renderer *renderer, const AosMainMenuGraphics *graphics,
    SDL_Texture *textures[AOS_MAIN_GRAPHICS_SCREEN_COUNT]
                         [AOS_MAIN_GRAPHICS_LAYER_COUNT],
    const AosMainMenu *menu, int32_t stage) {
    AosMainGraphicsScreen screen = main_menu_graphics_screen(menu, stage);
    SDL_SetRenderDrawColor(renderer, graphics->backdrop_rgb[0],
                           graphics->backdrop_rgb[1],
                           graphics->backdrop_rgb[2], 255);
    SDL_FRect viewport = {0, 0, GBA_W, GBA_H};
    SDL_RenderFillRect(renderer, &viewport);
    for (int layer = AOS_MAIN_GRAPHICS_LAYER_COUNT - 1; layer >= 0; --layer) {
        uint8_t alpha = layer == 1 ? 159 : 255;
        SDL_SetTextureAlphaMod(textures[screen][layer], alpha);
        render_wrapped_background(renderer, textures[screen][layer],
                                  &graphics->backgrounds[screen][layer],
                                  0, 0);
    }
}

int main(int argc, char **argv) {
    const char *image_path = NULL;
    const char *rom_path = NULL;
    const char *runtime_path = NULL, *save_directory_arg = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--title-bmp") && i + 1 < argc) image_path = argv[++i];
        else if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom_path = argv[++i];
        else if (!strcmp(argv[i], "--runtime") && i + 1 < argc) runtime_path = argv[++i];
        else if (!strcmp(argv[i], "--save-dir") && i + 1 < argc) save_directory_arg = argv[++i];
        else {
            fprintf(stderr,
                    "Usage: fusion_aria_title_preview [--title-bmp FILE] [--rom FILE] "
                    "[--runtime FILE] [--save-dir DIR]\n");
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
    FusionRom intro_rom = {0};
    AosIntroResources intro_resources = {0};
    AosIntroFont intro_font = {0};
    AosIntroTextTiles intro_tiles = {0};
    AosIntroGraphics intro_graphics = {0};
    SDL_Texture *intro_background_textures[AOS_INTRO_BACKGROUND_COUNT] = {0};
    AosMainMenuGraphics main_menu_graphics = {0};
    SDL_Texture *main_menu_textures[AOS_MAIN_GRAPHICS_SCREEN_COUNT]
                                   [AOS_MAIN_GRAPHICS_LAYER_COUNT] = {{0}};
    bool have_main_menu_graphics = false;
    bool have_intro_resources = false;
    if (rom_path) {
        char error[256];
        if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, rom_path, &intro_rom,
                             error, sizeof error) ||
            !aos_intro_resources_init(&intro_resources, &intro_rom, error,
                                      sizeof error) ||
            !aos_intro_font_init(&intro_font, &intro_rom, error,
                                 sizeof error) ||
            !aos_intro_graphics_init(&intro_graphics, &intro_rom, error,
                                     sizeof error) ||
            !create_intro_textures(renderer, &intro_graphics,
                                   intro_background_textures)) {
            if (!error[0])
                snprintf(error, sizeof error,
                         "cannot create introduction textures: %s",
                         SDL_GetError());
            fprintf(stderr, "Cannot load Aria introduction resources: %s\n", error);
            destroy_intro_textures(intro_background_textures);
            aos_intro_graphics_free(&intro_graphics);
            fusion_rom_close(&intro_rom);
            if (art) SDL_DestroyTexture(art);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_free(preference_path);
            SDL_Quit();
            return 2;
        }
        aos_intro_text_tiles_clear(&intro_tiles);
        have_intro_resources = true;
    }
    AosGameLifecycle lifecycle;
    AosMenuSaveBridge save_bridge;
    aos_game_lifecycle_init(&lifecycle, false);
    aos_menu_save_bridge_init(&save_bridge);
    for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i) {
        save_bridge.present[i] = slots[i].present;
        save_bridge.slots[i] = slots[i].data;
    }
    aos_menu_save_bridge_sync(&save_bridge, &lifecycle.main_menu);
    bool running = true;
    bool interactive = true;
    uint64_t next_frame = SDL_GetTicksNS();
    char launch_error[128] = "";
    int pending_launch_slot = -1;
    AosPortableStart pending_start = AOS_PORTABLE_START_NONE;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
                running = false;
        }
        if (!running) break;
        const uint16_t held = keymask(SDL_GetKeyboardState(NULL));
        aos_input_update(&lifecycle.input, held);
        aos_game_lifecycle_update(&lifecycle);
        if (intro_rom.data &&
            lifecycle.game_mode.mode == AOS_MODE_MAIN_MENU &&
            lifecycle.main_menu.assets_requested &&
            (!have_main_menu_graphics ||
             main_menu_graphics.language != lifecycle.main_menu.language)) {
            destroy_main_menu_textures(main_menu_textures);
            aos_main_menu_graphics_free(&main_menu_graphics);
            have_main_menu_graphics = aos_main_menu_graphics_init(
                &main_menu_graphics, &intro_rom,
                lifecycle.main_menu.language, launch_error,
                sizeof launch_error) &&
                create_main_menu_textures(renderer, &main_menu_graphics,
                                          main_menu_textures);
            if (!have_main_menu_graphics && !launch_error[0])
                snprintf(launch_error, sizeof launch_error,
                         "MAIN MENU TEXTURE ERROR: %.80s", SDL_GetError());
        }
        if (have_intro_resources &&
            lifecycle.game_mode.mode == AOS_MODE_INTRO_CUTSCENE &&
            lifecycle.intro_cutscene.entities_init_requested) {
            aos_intro_text_tiles_clear(&intro_tiles);
            if (!aos_intro_resources_bind(
                    &intro_resources, &lifecycle.intro_cutscene.entities,
                    lifecycle.reset.language, launch_error,
                    sizeof launch_error)) {
                have_intro_resources = false;
            }
        }
        AosIntroEntities *intro_entities = &lifecycle.intro_cutscene.entities;
        if (have_intro_resources &&
            lifecycle.game_mode.mode == AOS_MODE_INTRO_CUTSCENE &&
            intro_entities->text_clear_requested &&
            !aos_intro_font_render_line(&intro_font, &intro_tiles,
                                        intro_entities->text_draw_column,
                                        intro_entities->text_glyphs,
                                        intro_entities->text_glyph_count)) {
            snprintf(launch_error, sizeof launch_error,
                     "INVALID INTRODUCTION GLYPH");
            have_intro_resources = false;
        }
        if (!aos_menu_save_bridge_consume(&save_bridge, &lifecycle.main_menu)) {
            snprintf(launch_error, sizeof launch_error, "INVALID SLOT OPERATION");
        } else {
            if ((save_bridge.changed_mask || save_bridge.removed_mask) &&
                !persist_bridge(&save_bridge, slots, launch_error,
                                sizeof launch_error)) {
                pending_launch_slot = -1;
                pending_start = AOS_PORTABLE_START_NONE;
            }
            if (save_bridge.start != AOS_PORTABLE_START_NONE) {
                pending_launch_slot = save_bridge.launch_slot;
                pending_start = save_bridge.start;
            }
        }
        /* Loads reach IN_GAME directly. New files remain in this process for
         * the native introduction controller and launch only at its handoff. */
        bool destination_ready = pending_start != AOS_PORTABLE_START_NONE &&
                                 lifecycle.game_mode.mode == AOS_MODE_IN_GAME;
        if (destination_ready && pending_launch_slot >= 0 &&
            launch_runtime(runtime_path, slots[pending_launch_slot].path,
                           launch_error, sizeof launch_error)) {
            running = false;
        }
        if (!running) break;
        SDL_SetRenderDrawColor(renderer, 18, 14, 28, 255);
        SDL_RenderClear(renderer);
        bool in_main_menu = lifecycle.game_mode.mode == AOS_MODE_MAIN_MENU;
        bool in_intro = lifecycle.game_mode.mode == AOS_MODE_INTRO_CUTSCENE;
        if (in_intro && have_intro_resources) {
            render_intro_backgrounds(renderer, &intro_graphics,
                                     intro_background_textures,
                                     &lifecycle.intro_cutscene);
        } else if (in_main_menu && have_main_menu_graphics) {
            render_main_menu_backgrounds(
                renderer, &main_menu_graphics, main_menu_textures,
                &lifecycle.main_menu, lifecycle.game_mode.update_stage);
        } else if (art && lifecycle.game_mode.mode == AOS_MODE_TITLE_SCREEN) {
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
        /* Host state labels remain until the native menu entities and their
         * dynamic text/OAM streams are rendered. */
        if (in_main_menu) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 10, 12, 24,
                                   have_main_menu_graphics ? 125 : 235);
            SDL_FRect sheet = {12, 15, 216, 130};
            SDL_RenderFillRect(renderer, &sheet);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 245, 235, 210, 255);
            AosMainMenu *native_menu = &lifecycle.main_menu;
            if (native_menu->name_entry_active || lifecycle.game_mode.update_stage == 9) {
                char name[9];
                char cursor[48];
                display_name(name, native_menu->name_buffer);
                SDL_RenderDebugText(renderer, 28, 24, "ENTER NAME");
                SDL_RenderDebugText(renderer, 42, 52, name);
                snprintf(cursor, sizeof cursor, "SYMBOL %02u  POSITION %u",
                         native_menu->name_cursor, native_menu->name_position + 1);
                SDL_RenderDebugText(renderer, 28, 78, cursor);
                SDL_RenderDebugText(renderer, 20, 105, "D-PAD: SYMBOL  A: ADD");
                SDL_RenderDebugText(renderer, 20, 119, "B: ERASE  START: END");
            } else if (!native_menu->selected_slot_active) {
                const char *entries[] = {"PLAY", "COPY", "DELETE"};
                SDL_RenderDebugText(renderer, 30, 24, "ARIA MAIN MENU");
                for (unsigned i = 0; i < 3; ++i) {
                    SDL_RenderDebugText(renderer, 32, 47 + 20 * (float)i, entries[i]);
                    if (native_menu->selected_slot == i)
                        SDL_RenderDebugText(renderer, 19, 47 + 20 * (float)i, ">");
                }
                SDL_RenderDebugText(renderer, 25, 120, "A: SELECT   B: TITLE");
            } else {
                const char *operations[] = {"PLAY", "COPY", "DELETE"};
                char heading[48];
                snprintf(heading, sizeof heading, "%s - SELECT FILE",
                         operations[native_menu->selected_slot]);
                SDL_RenderDebugText(renderer, 28, 25, heading);
                uint8_t native_cursor = native_menu->file_cursor >= 6 ?
                    (uint8_t)(native_menu->file_cursor - 6) : native_menu->file_cursor;
                unsigned selected_file = native_cursor / 2;
                for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i) {
                    char line[80];
                    if (slots[i].present) {
                        char name[9];
                        display_name(name, slots[i].data.name);
                        snprintf(line, sizeof line, "FILE %u %.8s A%02u R%03u HP%d", i + 1,
                                 name, slots[i].data.area, slots[i].data.room,
                                 slots[i].data.hp);
                    } else {
                        snprintf(line, sizeof line, "FILE %u  EMPTY", i + 1);
                    }
                    SDL_RenderDebugText(renderer, 32, 50 + 22 * (float)i, line);
                    if (selected_file == i)
                        SDL_RenderDebugText(renderer, 19, 50 + 22 * (float)i, ">");
                }
                bool confirming = native_menu->confirm_active ||
                    (native_menu->selected_slot == 1 && native_menu->file_phase == 2) ||
                    (native_menu->selected_slot == 2 && native_menu->file_phase == 1);
                if (confirming) {
                    unsigned choice = native_menu->confirm_active ?
                        native_menu->confirm_choice : native_menu->file_cursor;
                    SDL_RenderDebugText(renderer, 55, 119,
                                        choice == 0 ? "> YES      NO" : "  YES    > NO");
                } else {
                    SDL_RenderDebugText(renderer, 25, 120, "A: SELECT   B: BACK");
                }
            }
            if (launch_error[0]) SDL_RenderDebugText(renderer, 18, 135, launch_error);
        } else {
            SDL_SetRenderDrawColor(renderer, 245, 235, 210, 255);
            if (in_intro) {
                char stage[48];
                char scroll[64];
                if (have_intro_resources) {
                    render_intro_text(renderer, &intro_tiles,
                                      intro_entities->bg0_vertical,
                                      intro_graphics.text_rgb);
                } else {
                    SDL_RenderDebugText(renderer, 58, 42,
                                        "ARIA INTRODUCTION");
                    snprintf(stage, sizeof stage,
                             "NATIVE CONTROLLER  STAGE %d",
                             lifecycle.game_mode.update_stage);
                    SDL_RenderDebugText(renderer, 42, 66, stage);
                    snprintf(scroll, sizeof scroll, "SCROLL %ld / %ld / %ld",
                             (long)(lifecycle.intro_cutscene.bg_y[1] >> 16),
                             (long)(lifecycle.intro_cutscene.bg_y[2] >> 16),
                             (long)(lifecycle.intro_cutscene.bg_y[3] >> 16));
                    SDL_RenderDebugText(renderer, 38, 82, scroll);
                    SDL_RenderDebugText(renderer, 20, 103,
                                        "ADD --rom FOR NATIVE TEXT");
                }
                SDL_RenderDebugText(renderer, 62, 132, "A OR START: SKIP");
            } else if (lifecycle.game_mode.mode == AOS_MODE_TITLE_SCREEN) {
                SDL_RenderDebugText(renderer, 72, 122, "PRESS START");
            } else if (lifecycle.game_mode.mode == AOS_MODE_LICENSED_BY_NINTENDO) {
                SDL_RenderDebugText(renderer, 48, 76, "LICENSED BY NINTENDO");
            } else if (lifecycle.game_mode.mode == AOS_MODE_KONAMI_LOGO) {
                SDL_RenderDebugText(renderer, 86, 76, "KONAMI");
            }
        }
        uint16_t brightness = in_main_menu ? lifecycle.main_menu.brightness :
            lifecycle.game_mode.mode == AOS_MODE_INTRO_CUTSCENE ?
                lifecycle.intro_cutscene.brightness :
            lifecycle.game_mode.mode == AOS_MODE_TITLE_SCREEN ?
                lifecycle.title_fade.brightness : lifecycle.startup_logo.brightness;
        uint8_t darkness = brightness > 16 ? 16 : (uint8_t)brightness;
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
        next_frame += 1000000000ull / 60ull;
        uint64_t now = SDL_GetTicksNS();
        if (now < next_frame) SDL_DelayNS(next_frame - now);
        else if (now - next_frame > 100000000ull) next_frame = now;
    }
    destroy_main_menu_textures(main_menu_textures);
    aos_main_menu_graphics_free(&main_menu_graphics);
    destroy_intro_textures(intro_background_textures);
    aos_intro_graphics_free(&intro_graphics);
    fusion_rom_close(&intro_rom);
    if (art) SDL_DestroyTexture(art);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_free(preference_path);
    SDL_Quit();
    return 0;
}
