/* SPDX-License-Identifier: GPL-3.0-only */
/* Diagnostic presentation host for the integrated native Aria boot and menu
 * controllers. All original artwork stays outside Git; rendering is not the
 * native GBA renderer. */
#include "fusion_host.h"
#include <SDL3/SDL.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aos_audio.h"
#include "aos_game_lifecycle.h"
#include "aos_intro_font.h"
#include "aos_intro_graphics.h"
#include "aos_intro_resources.h"
#include "aos_logo_graphics.h"
#include "aos_main_menu_graphics.h"
#include "aos_main_menu_objects.h"
#include "aos_main_menu_sprites.h"
#include "aos_main_menu_files.h"
#include "aos_stats.h"
#include "aos_title_graphics.h"
#include "gba_input.h"
#include "aos_game_over_graphics.h"
#include "aos_text_window.h"
#include "aos_menu_bg0.h"
#include "aos_menu_save_bridge.h"
#include "aos_mp2k.h"
#include "aos_save.h"

enum { GBA_W = 240, GBA_H = 160, SCALE = 3 };

#define AUDIO_TABLE "assets/extracted/aria/sound/runtime/song_table.tsv"
#define AUDIO_BANK "assets/extracted/aria/sound/runtime/sound_bank.bin"

static void start_mp2k_song(void *context, uint16_t song, const AosAudioSongInfo *info) {
    (void)song;
    (void)aos_mp2k_player_start(context, info->player, info->header_pointer);
}

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
                           SlotView slots[AOS_SAVE_SLOT_COUNT],
                           SlotView quicks[AOS_SAVE_SLOT_COUNT], char *error,
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
        if (bridge->removed_quick_mask & (1u << slot)) {
            if (remove(quicks[slot].path) != 0 && errno != ENOENT) {
                snprintf(error, error_size, "DELETE FAILED: %.80s", quicks[slot].path);
                return false;
            }
        }
        slots[slot].present = bridge->present[slot];
        slots[slot].data = bridge->slots[slot];
        quicks[slot].present = bridge->quick_present[slot];
        quicks[slot].data = bridge->quicks[slot];
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

/* A file's summary as SRAM's + 0x190 would hold it, from a portable save.
 * Not kept by the portable format: the completion flag (+ 0x2D), the icon
 * set (+ 0x2E) and the map completion (+ 0x32), here 0. */
static AosMenuFileSummary file_summary(const SlotView *view) {
    AosMenuFileSummary summary = {0};
    if (!view->present) return summary;
    uint64_t frames = view->data.play_frames;
    summary.play_time = frames == 0 ? 1u : frames > UINT32_MAX ? UINT32_MAX : (uint32_t)frames;
    memcpy(summary.name, view->data.name, sizeof summary.name);
    summary.area = view->data.area;
    summary.room = (uint8_t)view->data.room;
    summary.character = view->data.character;
    summary.gold = view->data.gold;
    /* sub_08033CAC's levels from the experience. */
    int32_t level = 1;
    while (level < 99 && (uint32_t)aos_stats_experience_for_level(level + 1) <= view->data.experience)
        ++level;
    summary.level = (uint8_t)level;
    return summary;
}

static bool slot_paths(SlotView slots[AOS_SAVE_SLOT_COUNT],
                       SlotView quicks[AOS_SAVE_SLOT_COUNT], const char *directory) {
    for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i) {
        int used = snprintf(slots[i].path, sizeof slots[i].path, "%s%saria-slot-%u.sav",
                            directory, directory[strlen(directory) - 1] == '/' ? "" : "/",
                            i + 1);
        if (used < 0 || (size_t)used >= sizeof slots[i].path) return false;
        slots[i].present = aos_save_load(slots[i].path, &slots[i].data, NULL, 0) &&
                           slots[i].data.slot == i;
        if (!aos_save_quick_path(quicks[i].path, sizeof quicks[i].path, slots[i].path))
            return false;
        quicks[i].present = aos_save_load(quicks[i].path, &quicks[i].data, NULL, 0) &&
                            quicks[i].data.slot == i;
    }
    return true;
}

/* The runtime gets this frontend's command line to come back to after a
 * game over (--frontend, then one --frontend-arg per argument). */
static int frontend_argc;
static char **frontend_argv;

static bool launch_runtime(const char *runtime, const char *rom_path,
                           const char *load_path, const char *save_path,
                           bool consume_save,
                           char *error, size_t error_size) {
    const char *arguments[64];
    int count = 0;
    arguments[count++] = runtime;
    arguments[count++] = "--save";
    arguments[count++] = load_path;
    if (rom_path) {
        arguments[count++] = "--rom";
        arguments[count++] = rom_path;
    }
    if (strcmp(load_path, save_path)) {
        arguments[count++] = "--save-target";
        arguments[count++] = save_path;
    }
    if (consume_save) arguments[count++] = "--consume-save";
    arguments[count++] = "--frontend";
    arguments[count++] = frontend_argv[0];
    for (int i = 1; i < frontend_argc && count + 3 < 64; ++i) {
        if (!strcmp(frontend_argv[i], "--game-over")) continue;
        arguments[count++] = "--frontend-arg";
        arguments[count++] = frontend_argv[i];
    }
    arguments[count] = NULL;
    if (!fusion_host_launch(arguments)) {
        snprintf(error, error_size, "LAUNCH FAILED: %.90s", SDL_GetError());
        return false;
    }
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

typedef struct {
    const AosIntroFont *font;
    AosIntroTextTiles *tiles;
    bool valid;
} MenuTextTarget;

static void menu_text_glyph(void *user, uint16_t pixel_x, uint16_t pixel_y,
                            uint8_t code, uint8_t left, uint8_t width) {
    (void)left;
    (void)width;
    MenuTextTarget *target = user;
    target->valid &= aos_intro_font_draw_glyph(target->font, target->tiles,
                                               pixel_x, pixel_y, code);
}

static void menu_text_clear(void *user, uint16_t pixel_x, uint16_t pixel_y) {
    MenuTextTarget *target = user;
    aos_intro_text_tiles_clear_rect(target->tiles, pixel_x, pixel_y, 6, 12);
}

/* Prints the menu's text window as the native controllers do: the window
 * (0, 1, 0x25, 3) of GameModeMainMenuUpdate's setup, cleared (sub_08040FE0)
 * and printed entry by entry from its text cell (sub_0804066C, sub_08041318). */
static bool render_menu_texts(AosIntroResources *resources,
                              const AosIntroFont *font,
                              AosIntroTextTiles *tiles, uint8_t language,
                              const AosMainMenuText *texts, size_t count,
                              char *error, size_t error_size) {
    MenuTextTarget target = {font, tiles, true};
    AosTextCursor cursor;
    aos_text_cursor_init_a(&cursor);
    aos_intro_text_tiles_clear(tiles);
    aos_text_set_window(&cursor, 0, 1, 0x25, 3);
    aos_text_clear_window(&cursor, menu_text_clear, &target);
    for (size_t index = 0; index < count; ++index) {
        const uint8_t *entry;
        size_t entry_size;
        if (!aos_intro_resources_entry(resources, language, texts[index].id,
                                       &entry, &entry_size)) {
            snprintf(error, error_size, "MENU TEXT 0x%X OUTSIDE THE ROM",
                     texts[index].id);
            return false;
        }
        if (texts[index].row >= 6) {
            /* The area names: context B (8-pixel rows), sub_08040970(0, row,
             * 0x1E, 1), sub_08041204, then sub_08041338. */
            AosTextCursor line;
            aos_text_cursor_init_b(&line);
            aos_text_set_window(&line, 0, texts[index].row, 0x1E, 1);
            aos_text_clear_window(&line, menu_text_clear, &target);
            aos_text_print(&line, font->metrics, entry, entry_size, 0, menu_text_glyph, &target);
            continue;
        }
        aos_text_set_cursor(&cursor, texts[index].column, texts[index].row);
        aos_text_print(&cursor, font->metrics, entry, entry_size, 0,
                       menu_text_glyph, &target);
    }
    if (!target.valid) snprintf(error, error_size, "INVALID MENU TEXT GLYPH");
    return target.valid;
}

/* Uploads the native menu BG0 (text tiles, language bank, palettes 12 and
 * 13, map) as an RGBA layer; transparent pixels stay transparent. */
static void upload_menu_bg0(SDL_Texture *texture, const AosMenuBg0 *bg0) {
    static uint8_t pixels[GBA_W * GBA_H * 4];
    for (unsigned y = 0; y < GBA_H; ++y)
        for (unsigned x = 0; x < GBA_W; ++x) {
            uint8_t *out = pixels + (y * GBA_W + x) * 4;
            uint16_t color;
            if (!aos_menu_bg0_pixel(bg0, x, y, &color)) {
                memset(out, 0, 4);
                continue;
            }
            out[0] = (uint8_t)(((color & 31) * 255) / 31);
            out[1] = (uint8_t)((((color >> 5) & 31) * 255) / 31);
            out[2] = (uint8_t)((((color >> 10) & 31) * 255) / 31);
            out[3] = 255;
        }
    SDL_UpdateTexture(texture, NULL, pixels, GBA_W * 4);
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

static void render_intro_actors(SDL_Renderer *renderer,
                                SDL_Texture *texture,
                                const AosIntroGraphics *graphics,
                                const AosIntroEntities *actors,
                                uint16_t alpha_blend) {
    if (!texture) return;
    static AosObjLayer layer;
    static uint8_t pixels[GBA_W * GBA_H * 4];
    aos_obj_layer_clear(&layer);
    const struct {
        uint8_t entity;
        uint8_t sheet;
        uint8_t native_layer;
        bool semi_transparent;
    } draws[] = {
        {AOS_INTRO_ENTITY_MAIN, 0, 0x1A, false},
        {AOS_INTRO_ENTITY_SCROLL_FOLLOWER, 1, 0x16, false},
        {AOS_INTRO_ENTITY_AFFINE_RISE, 1, 0x1A, false},
        {AOS_INTRO_ENTITY_ORBIT, 1, 0x16, false},
        {AOS_INTRO_ENTITY_PANEL_LEFT, 1, 0x19, true},
        {AOS_INTRO_ENTITY_PANEL_RIGHT, 1, 0x19, true},
    };
    for (unsigned index = 0; index < sizeof draws / sizeof draws[0]; ++index) {
        const AosIntroEntity *actor = &actors->entities[draws[index].entity];
        uint8_t record = actor->animation_frame;
        if (!actor->alive || !actor->visible ||
            (actor->affine && actor->scale == 0) ||
            record >= graphics->actors[draws[index].sheet].frame_count)
            continue;
        int32_t x = actor->x, y = actor->y;
        AosObjSprite sprite = {
            .sheet = draws[index].sheet,
            .record = record,
            .palette_row = actor->palette_row,
            .priority = (uint8_t)(draws[index].native_layer <= 6 ? 0 :
                                  draws[index].native_layer == 7 ? 1 :
                                  draws[index].native_layer <= 0x16 ? 2 : 3),
            .mode = draws[index].semi_transparent ? 1 : 0,
            .affine = actor->affine,
            .pa = 0x100,
            .pd = 0x100,
            .x = (int16_t)(x < 0 ? -((-x) >> 16) : x >> 16),
            .y = (int16_t)(y < 0 ? -((-y) >> 16) : y >> 16),
        };
        if (actor->affine) {
            uint32_t matrix = 0x1000000u / actor->scale;
            if (matrix > 0x7FFFu) matrix = 0x7FFFu;
            sprite.pa = (int16_t)matrix;
            sprite.pd = (int16_t)matrix;
        }
        aos_obj_layer_draw(&layer, graphics->actors, &sprite);
        /* sub_08008FEC changes the integer halfword by 0x80 between its
         * two draws. The two panel records therefore meet from opposite
         * screen edges; this is a 128-pixel repeat, not a subpixel offset. */
        if (actor->double_draw) {
            sprite.x = (int16_t)(sprite.x +
                (actor->variant == 0 ? -0x80 : 0x80));
            aos_obj_layer_draw(&layer, graphics->actors, &sprite);
        }
    }
    /* BLDALPHA's EVA coefficient drives semi-transparent OBJ pixels; the
     * controller merges stage and entity writes into alpha_blend. */
    unsigned first_alpha = alpha_blend & 0x1Fu;
    if (first_alpha > 16) first_alpha = 16;
    for (int index = 0; index < GBA_W * GBA_H; ++index) {
        uint16_t color = graphics->object_palette[layer.index[index]];
        pixels[index * 4] = (uint8_t)((color & 31) * 255 / 31);
        pixels[index * 4 + 1] =
            (uint8_t)(((color >> 5) & 31) * 255 / 31);
        pixels[index * 4 + 2] =
            (uint8_t)(((color >> 10) & 31) * 255 / 31);
        pixels[index * 4 + 3] = layer.index[index]
                                    ? (uint8_t)(layer.semi[index]
                                                    ? first_alpha * 255 / 16
                                                    : 255)
                                    : 0;
    }
    SDL_UpdateTexture(texture, NULL, pixels, GBA_W * 4);
    SDL_FRect screen = {0, 0, GBA_W, GBA_H};
    SDL_RenderTexture(renderer, texture, NULL, &screen);
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

/* BG1 with the H-blank BG1VOFS of the file panels on its split lines. */
static void render_split_layer(SDL_Renderer *renderer, SDL_Texture *texture,
                               const FusionAosRenderedBackground *background,
                               const AosMainMenuFilePanel *panel) {
    int width = (int)background->width_pixels, height = (int)background->height_pixels;
    for (int y = 0; y < GBA_H;) {
        bool split = y >= panel->split_first && y < panel->split_first + panel->split_count;
        int end = split ? panel->split_first + panel->split_count
                        : y < panel->split_first ? panel->split_first : GBA_H;
        if (end > GBA_H) end = GBA_H;
        int source = (y + (split ? panel->split_vofs : 0)) % height;
        int rows = end - y;
        if (rows > height - source) rows = height - source;
        for (int x = 0; x < GBA_W; x += width) {
            SDL_FRect from = {0, (float)source, (float)width, (float)rows};
            SDL_FRect to = {(float)x, (float)y, (float)width, (float)rows};
            SDL_RenderTexture(renderer, texture, &from, &to);
        }
        y += rows;
    }
}

static void render_main_menu_backgrounds(
    SDL_Renderer *renderer, const AosMainMenuGraphics *graphics,
    SDL_Texture *textures[AOS_MAIN_GRAPHICS_SCREEN_COUNT]
                         [AOS_MAIN_GRAPHICS_LAYER_COUNT],
    const AosMainMenu *menu, int32_t stage, const AosMainMenuFilePanel *panel) {
    AosMainGraphicsScreen screen = main_menu_graphics_screen(menu, stage);
    SDL_SetRenderDrawColor(renderer, graphics->backdrop_rgb[0],
                           graphics->backdrop_rgb[1],
                           graphics->backdrop_rgb[2], 255);
    SDL_FRect viewport = {0, 0, GBA_W, GBA_H};
    SDL_RenderFillRect(renderer, &viewport);
    for (int layer = AOS_MAIN_GRAPHICS_LAYER_COUNT - 1; layer >= 0; --layer) {
        uint8_t alpha = layer == 1 ? 159 : 255;
        SDL_SetTextureAlphaMod(textures[screen][layer], alpha);
        if (layer == 0 && panel && panel->split_vofs)
            render_split_layer(renderer, textures[screen][layer],
                               &graphics->backgrounds[screen][layer], panel);
        else
            render_wrapped_background(renderer, textures[screen][layer],
                                      &graphics->backgrounds[screen][layer], 0, 0);
    }
}

int aria_title_preview_main(int argc, char **argv) {
    const char *image_path = NULL;
    const char *rom_path = NULL;
    const char *runtime_path = NULL, *save_directory_arg = NULL;
    bool start_game_over = false, start_main_menu = false, mute = false;
    long screenshot_frame = -1;
    /* --press KEY@FRAME,...: scripted presses (a, b, start, up, down,
     * left, right) for headless checks. */
    long press_frames[32];
    uint16_t press_keys[32];
    int press_count = 0;
    const char *screenshot_path = NULL;
    frontend_argc = argc;
    frontend_argv = argv;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--title-bmp") && i + 1 < argc) image_path = argv[++i];
        else if (!strcmp(argv[i], "--rom") && i + 1 < argc) rom_path = argv[++i];
        else if (!strcmp(argv[i], "--runtime") && i + 1 < argc) runtime_path = argv[++i];
        else if (!strcmp(argv[i], "--save-dir") && i + 1 < argc) save_directory_arg = argv[++i];
        else if (!strcmp(argv[i], "--game-over")) start_game_over = true;
        else if (!strcmp(argv[i], "--mute")) mute = true;
        else if (!strcmp(argv[i], "--main-menu")) start_main_menu = true;
        else if (!strcmp(argv[i], "--press") && i + 1 < argc) {
            static const struct { const char *name; uint16_t key; } names[] = {
                {"a", AOS_KEY_A}, {"b", AOS_KEY_B}, {"start", AOS_KEY_START},
                {"up", AOS_KEY_UP}, {"down", AOS_KEY_DOWN}, {"left", AOS_KEY_LEFT},
                {"right", AOS_KEY_RIGHT}};
            for (char *p = argv[++i]; *p && press_count < 32;) {
                char *at = strchr(p, '@');
                if (!at) break;
                uint16_t key = 0;
                for (size_t k = 0; k < sizeof names / sizeof names[0]; ++k)
                    if ((size_t)(at - p) == strlen(names[k].name) &&
                        !strncmp(p, names[k].name, (size_t)(at - p)))
                        key = names[k].key;
                press_keys[press_count] = key;
                press_frames[press_count++] = strtol(at + 1, &p, 10);
                if (*p == ',') ++p;
            }
        }
        else if (!strcmp(argv[i], "--screenshot") && i + 2 < argc) {
            /* --screenshot FRAME FILE: that frame as a BMP, then quit. */
            screenshot_frame = strtol(argv[++i], NULL, 10);
            screenshot_path = argv[++i];
        }
        else {
            fprintf(stderr,
                    "Usage: fusion_aria_title_preview [--title-bmp FILE] [--rom FILE] "
                    "[--runtime FILE] [--save-dir DIR] [--game-over] [--main-menu] [--mute] [--screenshot FRAME FILE] [--press KEY@FRAME,...]\n");
            return 2;
        }
    }
    if (!fusion_host_init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | (mute ? 0 : SDL_INIT_AUDIO))) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    /* PlaySong through the exported song table and MP2K bank (the title's
     * MUS_27), mixed as SoundMainRAM's signed 8-bit mono at 10512 Hz. */
    char audio_error[256];
    aos_audio_reset();
    static AosMp2kEngine mp2k;
    aos_mp2k_engine_init(&mp2k);
    bool have_audio = !mute &&
                      aos_audio_load_song_table(AUDIO_TABLE, audio_error, sizeof audio_error) &&
                      aos_mp2k_engine_load_bank(&mp2k, AUDIO_BANK, audio_error,
                                                sizeof audio_error);
    SDL_AudioStream *audio_stream = NULL;
    if (have_audio) {
        aos_audio_set_start_sink(start_mp2k_song, &mp2k);
        SDL_AudioSpec spec = {SDL_AUDIO_S8, 1, AOS_MP2K_HOST_RATE};
        audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL,
                                                 NULL);
        if (audio_stream) SDL_ResumeAudioStreamDevice(audio_stream);
        else fprintf(stderr, "No audio output: %s\n", SDL_GetError());
    } else if (!mute) {
        fprintf(stderr, "No Aria audio: ./build/fusion_rebuild --task aria_sound (%s)\n",
                audio_error);
    }
    char runtime_buffer[1024];
    if (!runtime_path) {
        const char *base_path = SDL_GetBasePath();
        if (!base_path) {
            fprintf(stderr, "Cannot resolve runtime directory: %s\n", SDL_GetError());
            fusion_host_quit();
            return 2;
        }
        int used = snprintf(runtime_buffer, sizeof runtime_buffer, "%sfusion_aria_runtime",
                            base_path);
        if (used < 0 || (size_t)used >= sizeof runtime_buffer) {
            fprintf(stderr, "Runtime path is too long\n");
            fusion_host_quit();
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
        fusion_host_quit();
        return 2;
    }
    SlotView slots[AOS_SAVE_SLOT_COUNT] = {0};
    SlotView quicks[AOS_SAVE_SLOT_COUNT] = {0};
    if (!save_directory || !*save_directory ||
        !slot_paths(slots, quicks, save_directory)) {
        fprintf(stderr, "Cannot prepare Aria save slots\n");
        SDL_free(preference_path);
        fusion_host_quit();
        return 2;
    }
    SDL_Window *window = fusion_host_create_window("Aria title - reconstruction preview",
                                           GBA_W * SCALE, GBA_H * SCALE, 0);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
    if (!renderer) {
        fprintf(stderr, "Unable to create SDL renderer: %s\n", SDL_GetError());
        if (window) fusion_host_destroy_window(window);
        SDL_free(preference_path);
        fusion_host_quit();
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
            fusion_host_destroy_window(window);
            SDL_free(preference_path);
            fusion_host_quit();
            return 2;
        }
        art = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);
        if (!art) {
            fprintf(stderr, "Cannot create title texture: %s\n", SDL_GetError());
            SDL_DestroyRenderer(renderer);
            fusion_host_destroy_window(window);
            SDL_free(preference_path);
            fusion_host_quit();
            return 2;
        }
    }
    FusionRom intro_rom = {0};
    AosIntroResources intro_resources = {0};
    AosIntroFont intro_font = {0};
    AosIntroTextTiles intro_tiles = {0};
    AosIntroTextTiles menu_prompt_tiles = {0};
    static AosMenuBg0 menu_bg0;
    bool have_menu_bg0 = false;
    /* Entity 0's objects (the labels, cursors, file icons and names). */
    static AosMainMenuObjects menu_objects;
    static AosMainMenuSpriteState menu_sprite_state;
    bool have_menu_objects = false;
    SDL_Texture *menu_objects_texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GBA_W, GBA_H);
    if (menu_objects_texture) {
        SDL_SetTextureBlendMode(menu_objects_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(menu_objects_texture, SDL_SCALEMODE_NEAREST);
    }
    SDL_Texture *intro_actor_texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
        GBA_W, GBA_H);
    if (intro_actor_texture) {
        SDL_SetTextureBlendMode(intro_actor_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(intro_actor_texture, SDL_SCALEMODE_NEAREST);
    }
    SDL_Texture *menu_bg0_texture = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GBA_W, GBA_H);
    if (menu_bg0_texture) {
        SDL_SetTextureBlendMode(menu_bg0_texture, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(menu_bg0_texture, SDL_SCALEMODE_NEAREST);
    }
    AosMainMenuText menu_texts[AOS_MAIN_MENU_TEXT_MAX + 2];
    static AosMainMenuFilePanel file_panel;
    size_t menu_text_count = 0;
    uint8_t menu_prompt_language = 0;
    bool have_menu_prompt = false;
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
            fusion_host_destroy_window(window);
            SDL_free(preference_path);
            fusion_host_quit();
            return 2;
        }
        aos_intro_text_tiles_clear(&intro_tiles);
        have_intro_resources = true;
    }
    AosGameLifecycle lifecycle;
    AosMenuSaveBridge save_bridge;
    aos_game_lifecycle_init(&lifecycle, false);
    /* The native title screen: its layers and its palette rows (0x08209308
     * for the objects, 0x080E622C for the cycle of BG row 0). */
    static AosTitleGraphics title_graphics;
    SDL_Texture *title_texture = NULL;
    if (intro_rom.data) {
        char title_error[256];
        if (aos_title_graphics_init(&title_graphics, &intro_rom, title_error,
                                    sizeof title_error) &&
            intro_rom.size > 0x20A000) {
            lifecycle.title.graphics = &title_graphics;
            lifecycle.title.obj_palette_rows = intro_rom.data + 0x209308;
            lifecycle.title.cycle_rows = intro_rom.data + 0x0E622C;
            title_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                              SDL_TEXTUREACCESS_STREAMING, GBA_W, GBA_H);
            if (title_texture) SDL_SetTextureScaleMode(title_texture, SDL_SCALEMODE_NEAREST);
        } else {
            fprintf(stderr, "No native Aria title: %s\n", title_error);
        }
    }
    /* The two startup logos (Licensed by Nintendo, Konami) from the ROM. */
    static AosLogoGraphics logo_graphics[2];
    bool have_logos = false;
    SDL_Texture *logo_texture = NULL;
    if (intro_rom.data) {
        char logo_error[256];
        have_logos = aos_logo_graphics_init(&logo_graphics[AOS_LOGO_NINTENDO], &intro_rom,
                                            AOS_LOGO_NINTENDO, logo_error, sizeof logo_error) &&
                     aos_logo_graphics_init(&logo_graphics[AOS_LOGO_KONAMI], &intro_rom,
                                            AOS_LOGO_KONAMI, logo_error, sizeof logo_error);
        if (have_logos) {
            logo_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING, GBA_W, GBA_H);
            if (logo_texture) SDL_SetTextureScaleMode(logo_texture, SDL_SCALEMODE_NEAREST);
            else have_logos = false;
        } else {
            fprintf(stderr, "No native Aria logos: %s\n", logo_error);
        }
    }
    /* --main-menu: as after START on the title (the Metroid Vania
     * launcher's entry point), past the logos and the title. */
    if (start_main_menu) aos_game_mode_set(&lifecycle.game_mode, AOS_MODE_MAIN_MENU);
    /* --game-over: the runtime's Soma died; GAME_OVER then the Konami logo. */
    if (start_game_over) aos_game_lifecycle_start_game_over(&lifecycle);
    static AosGameOverGraphics game_over_graphics;
    bool have_game_over_graphics = false;
    SDL_Texture *game_over_background = NULL, *game_over_sprite = NULL;
    aos_menu_save_bridge_init(&save_bridge);
    for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i) {
        save_bridge.present[i] = slots[i].present;
        save_bridge.slots[i] = slots[i].data;
        save_bridge.quick_present[i] = quicks[i].present;
        save_bridge.quicks[i] = quicks[i].data;
    }
    aos_menu_save_bridge_sync(&save_bridge, &lifecycle.main_menu);
    /* The keyboard and the first gamepad (AOS_KEY_* are the GBA key bits). */
    GbaPadMap pad_map;
    gba_pad_map_default(&pad_map);
    GbaInput gamepad = {0};
    gba_input_init(&gamepad, &pad_map);
    bool running = true;
    bool interactive = true;
    uint64_t next_frame = SDL_GetTicksNS();
    char launch_error[128] = "";
    int pending_launch_slot = -1;
    bool pending_launch_quick = false;
    AosPortableStart pending_start = AOS_PORTABLE_START_NONE;
    long preview_frame = 0;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
                running = false;
            gba_input_handle_event(&gamepad, &event);
        }
        if (!running) break;
        uint16_t held = (uint16_t)(keymask(SDL_GetKeyboardState(NULL)) | gba_input_buttons(&gamepad));
        if ((held & (AOS_KEY_LEFT | AOS_KEY_RIGHT)) == (AOS_KEY_LEFT | AOS_KEY_RIGHT))
            held &= (uint16_t)~(AOS_KEY_LEFT | AOS_KEY_RIGHT);
        if ((held & (AOS_KEY_UP | AOS_KEY_DOWN)) == (AOS_KEY_UP | AOS_KEY_DOWN))
            held &= (uint16_t)~(AOS_KEY_UP | AOS_KEY_DOWN);
        for (int k = 0; k < press_count; ++k)
            if (press_frames[k] == preview_frame) held |= press_keys[k];
        aos_input_update(&lifecycle.input, held);
        aos_game_lifecycle_update(&lifecycle);
        /* Inside the Metroid Vania launcher, the menu's way back to the
         * title (B, a game over's Konami logo) leads to its shared title. */
        if (fusion_host_embedded() &&
            (lifecycle.game_mode.mode == AOS_MODE_TITLE_SCREEN ||
             lifecycle.game_mode.mode == AOS_MODE_KONAMI_LOGO ||
             lifecycle.game_mode.mode == AOS_MODE_LICENSED_BY_NINTENDO ||
             lifecycle.game_mode.mode == AOS_MODE_RESET)) {
            const char *launcher[] = {"fusion_metroidvania", NULL};
            fusion_host_launch(launcher);
            running = false;
            break;
        }
        /* The main menu's PlaySong requests (MUS_14, its sounds). */
        for (unsigned k = 0; k < lifecycle.main_menu.sound_count; ++k)
            aos_audio_play_song(lifecycle.main_menu.sounds[k]);
        lifecycle.main_menu.sound_count = 0;
        /* The title's PlaySong requests (MUS_27, then SONG_MUTE). */
        if (lifecycle.title.song >= 0) {
            aos_audio_play_song((uint16_t)lifecycle.title.song);
            lifecycle.title.song = -1;
        }
        /* GameModeKonamiLogoUpdate's PlaySong(SE_428) after the code. */
        if (lifecycle.startup_logo.konami_code_sound_requested) {
            aos_audio_play_song(428);
            lifecycle.startup_logo.konami_code_sound_requested = false;
        }
        /* GameModeIntroCutsceneUpdate and sub_08009178's PlaySong calls. */
        uint16_t intro_audio[AOS_INTRO_AUDIO_CAPACITY];
        size_t intro_audio_count = aos_intro_cutscene_audio_requests(
            &lifecycle.intro_cutscene, intro_audio);
        for (size_t k = 0; k < intro_audio_count; ++k)
            aos_audio_play_song(intro_audio[k]);
        lifecycle.intro_cutscene.music_start_requested = false;
        lifecycle.intro_cutscene.music_mute_requested = false;
        lifecycle.intro_cutscene.entities.affine_sound_requested = false;
        aos_audio_volume_update();
        if (have_audio) {
            const AosAudioState *audio_state = aos_audio_state();
            for (unsigned player = 0; player < AOS_AUDIO_PLAYER_COUNT; ++player) {
                aos_mp2k_player_pause(&mp2k, (uint8_t)player,
                                      audio_state->players[player].paused);
                aos_mp2k_player_volume(
                    &mp2k, (uint8_t)player,
                    (uint8_t)(audio_state->players[player].volume_current / 0x10));
            }
            aos_mp2k_engine_frame(&mp2k);
            int8_t pcm[AOS_MP2K_SAMPLES_PER_FRAME];
            aos_mp2k_mix(&mp2k, pcm, sizeof pcm);
            if (audio_stream &&
                SDL_GetAudioStreamQueued(audio_stream) < (int)sizeof pcm * AOS_MP2K_HOST_QUEUE_FRAMES)
                SDL_PutAudioStreamData(audio_stream, pcm, sizeof pcm);
        }
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
            have_menu_objects = have_main_menu_graphics && menu_objects_texture &&
                aos_main_menu_objects_init(&menu_objects, &intro_rom,
                                           lifecycle.main_menu.language, launch_error,
                                           sizeof launch_error);
            have_menu_bg0 = menu_bg0_texture &&
                aos_menu_bg0_init(&menu_bg0, &intro_rom, lifecycle.main_menu.language,
                                  launch_error, sizeof launch_error);
            if (have_menu_bg0) aos_menu_bg0_map_text(&menu_bg0);
            have_menu_prompt = false;
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
            } else {
                aos_intro_entities_bind_actor_sheet(
                    &lifecycle.intro_cutscene.entities,
                    &intro_graphics.actors[1]);
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
        if (have_intro_resources &&
            lifecycle.game_mode.mode == AOS_MODE_MAIN_MENU) {
            /* The file panels (BG0 statistics, area names, BG1 split). */
            AosMenuFileSummary files[AOS_MENU_FILES];
            for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i) {
                files[2 * i] = file_summary(&slots[i]);
                files[2 * i + 1] = file_summary(&quicks[i]);
            }
            bool panel_name_entry = lifecycle.main_menu.name_entry_active ||
                                    lifecycle.game_mode.update_stage == 9;
            if (have_menu_bg0)
                aos_main_menu_file_panels(&menu_bg0, &lifecycle.main_menu, panel_name_entry,
                                          files, &file_panel);
            else
                memset(&file_panel, 0, sizeof file_panel);
            AosMainMenuText texts[AOS_MAIN_MENU_TEXT_MAX + 2];
            size_t count = aos_main_menu_text_layout(&lifecycle.main_menu, texts);
            for (int k = 0; k < file_panel.area_count; ++k)
                texts[count++] = (AosMainMenuText){file_panel.area_text[k], 0,
                                                   file_panel.area_row[k]};
            if (!have_menu_prompt || count != menu_text_count ||
                memcmp(texts, menu_texts, count * sizeof texts[0]) ||
                lifecycle.main_menu.language != menu_prompt_language) {
                have_menu_prompt = render_menu_texts(
                    &intro_resources, &intro_font, &menu_prompt_tiles,
                    lifecycle.main_menu.language, texts, count, launch_error,
                    sizeof launch_error);
                memcpy(menu_texts, texts, count * sizeof texts[0]);
                if (have_menu_bg0 && have_menu_prompt)
                    aos_menu_bg0_set_text(&menu_bg0, &menu_prompt_tiles);
                menu_text_count = count;
                menu_prompt_language = lifecycle.main_menu.language;
            }
            if (have_menu_bg0 && have_menu_prompt) upload_menu_bg0(menu_bg0_texture, &menu_bg0);
        }
        if (!aos_menu_save_bridge_consume(&save_bridge, &lifecycle.main_menu)) {
            snprintf(launch_error, sizeof launch_error, "INVALID SLOT OPERATION");
        } else {
            if ((save_bridge.changed_mask || save_bridge.removed_mask ||
                 save_bridge.removed_quick_mask) &&
                !persist_bridge(&save_bridge, slots, quicks, launch_error,
                                sizeof launch_error)) {
                pending_launch_slot = -1;
                pending_launch_quick = false;
                pending_start = AOS_PORTABLE_START_NONE;
            }
            if (save_bridge.start != AOS_PORTABLE_START_NONE) {
                pending_launch_slot = save_bridge.launch_slot;
                pending_launch_quick = save_bridge.launch_quick;
                pending_start = save_bridge.start;
            }
        }
        /* Loads reach IN_GAME directly. New files remain in this process for
         * the native introduction controller and launch only at its handoff. */
        bool destination_ready = pending_start != AOS_PORTABLE_START_NONE &&
                                 lifecycle.game_mode.mode == AOS_MODE_IN_GAME;
        if (destination_ready && pending_launch_slot >= 0) {
            const char *load_path = pending_launch_quick
                                        ? quicks[pending_launch_slot].path
                                        : slots[pending_launch_slot].path;
            if (launch_runtime(runtime_path, rom_path, load_path,
                               slots[pending_launch_slot].path,
                               pending_launch_quick, launch_error,
                               sizeof launch_error)) {
                running = false;
            } else if (pending_launch_quick) {
                save_bridge.quick_present[pending_launch_slot] = true;
                quicks[pending_launch_slot].present = true;
                aos_menu_save_bridge_sync(&save_bridge, &lifecycle.main_menu);
                pending_launch_slot = -1;
                pending_launch_quick = false;
                pending_start = AOS_PORTABLE_START_NONE;
            }
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
            render_intro_actors(
                renderer, intro_actor_texture, &intro_graphics,
                &lifecycle.intro_cutscene.entities,
                lifecycle.intro_cutscene.alpha_blend);
        } else if (in_main_menu && have_main_menu_graphics) {
            render_main_menu_backgrounds(
                renderer, &main_menu_graphics, main_menu_textures,
                &lifecycle.main_menu, lifecycle.game_mode.update_stage, &file_panel);
        } else if (lifecycle.game_mode.mode == AOS_MODE_GAME_OVER && intro_rom.data) {
            if (!have_game_over_graphics) {
                have_game_over_graphics = aos_game_over_graphics_init(
                    &game_over_graphics, &intro_rom, lifecycle.reset.language, launch_error,
                    sizeof launch_error);
                if (have_game_over_graphics) {
                    const FusionAosRenderedBackground *bg = &game_over_graphics.background;
                    game_over_background = SDL_CreateTexture(
                        renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                        (int)bg->width_pixels, (int)bg->height_pixels);
                    if (game_over_background)
                        SDL_UpdateTexture(game_over_background, NULL, bg->rgba,
                                          (int)bg->width_pixels * 4);
                    SDL_Surface *surface = SDL_LoadBMP_IO(
                        SDL_IOFromConstMem(game_over_graphics.sprite_bmp,
                                           game_over_graphics.sprite_bmp_size), true);
                    if (surface) {
                        game_over_sprite = SDL_CreateTextureFromSurface(renderer, surface);
                        SDL_DestroySurface(surface);
                    }
                    if (game_over_sprite)
                        SDL_SetTextureBlendMode(game_over_sprite, SDL_BLENDMODE_BLEND);
                }
            }
            const AosGameOver *over = &lifecycle.game_over;
            if (game_over_background) {
                SDL_FRect source = {0, 0, GBA_W, GBA_H}, whole = {0, 0, GBA_W, GBA_H};
                SDL_RenderTexture(renderer, game_over_background, &source, &whole);
            }
            /* BLDALPHA: the sprite (OBJ, first target) over BG1 at EVA / 16,
             * the BG behind it at EVB / 16 (EVA + EVB = 16). */
            if (game_over_sprite && over->sprite_shown) {
                float w, h;
                SDL_GetTextureSize(game_over_sprite, &w, &h);
                SDL_FRect rect = {(float)(AOS_GAME_OVER_SPRITE_X + game_over_graphics.sprite_left),
                                  (float)(AOS_GAME_OVER_SPRITE_Y + game_over_graphics.sprite_top),
                                  w, h};
                SDL_SetTextureAlphaMod(game_over_sprite, (uint8_t)(over->eva * 255u / 16u));
                SDL_RenderTexture(renderer, game_over_sprite, NULL, &rect);
            }
            /* The palette fades: each colour mixed with the target colour by
             * the weight; BG and OBJ share their weight on this screen. */
            if (over->bg.weight < 0x10000) {
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, (uint8_t)(over->bg.r * 255 / 31),
                                       (uint8_t)(over->bg.g * 255 / 31),
                                       (uint8_t)(over->bg.b * 255 / 31),
                                       (uint8_t)((0x10000 - over->bg.weight) * 255 / 0x10000));
                SDL_FRect whole = {0, 0, GBA_W, GBA_H};
                SDL_RenderFillRect(renderer, &whole);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
        } else if (have_logos && (lifecycle.game_mode.mode == AOS_MODE_LICENSED_BY_NINTENDO ||
                                  lifecycle.game_mode.mode == AOS_MODE_KONAMI_LOGO)) {
            static uint8_t logo_pixels[GBA_W * GBA_H * 4];
            AosLogo logo = lifecycle.game_mode.mode == AOS_MODE_KONAMI_LOGO ? AOS_LOGO_KONAMI
                                                                            : AOS_LOGO_NINTENDO;
            aos_logo_render(&logo_graphics[logo], lifecycle.startup_logo.blend_control,
                            lifecycle.startup_logo.brightness, logo_pixels);
            SDL_UpdateTexture(logo_texture, NULL, logo_pixels, GBA_W * 4);
            SDL_FRect dst = {0, 0, GBA_W, GBA_H};
            SDL_RenderTexture(renderer, logo_texture, NULL, &dst);
        } else if (title_texture && lifecycle.game_mode.mode == AOS_MODE_TITLE_SCREEN) {
            static uint8_t title_pixels[GBA_W * GBA_H * 4];
            aos_title_screen_render(&lifecycle.title, title_pixels);
            SDL_UpdateTexture(title_texture, NULL, title_pixels, GBA_W * 4);
            SDL_FRect dst = {0, 0, GBA_W, GBA_H};
            SDL_RenderTexture(renderer, title_texture, NULL, &dst);
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
        if (in_main_menu && !have_menu_objects) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 10, 12, 24,
                                   have_main_menu_graphics ? 125 : 235);
            SDL_FRect sheet = {12, 15, 216, 130};
            SDL_RenderFillRect(renderer, &sheet);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
        if (in_main_menu) {
            SDL_SetRenderDrawColor(renderer, 245, 235, 210, 255);
            AosMainMenu *native_menu = &lifecycle.main_menu;
            bool native_prompt = have_menu_bg0 && have_menu_prompt && menu_text_count > 0;
            if (have_menu_bg0) {
                SDL_FRect whole_screen = {0, 0, GBA_W, GBA_H};
                SDL_RenderTexture(renderer, menu_bg0_texture, NULL, &whole_screen);
            }
            bool name_entry = native_menu->name_entry_active ||
                              lifecycle.game_mode.update_stage == 9;
            if (have_menu_objects) {
                AosMenuFileSummary files[AOS_MENU_FILES];
                for (unsigned i = 0; i < AOS_SAVE_SLOT_COUNT; ++i) {
                    files[2 * i] = file_summary(&slots[i]);
                    files[2 * i + 1] = file_summary(&quicks[i]);
                }
                static AosObjSprite sprites[AOS_MENU_MAX_SPRITES];
                int count = aos_main_menu_sprites(native_menu, name_entry, files,
                                                  (uint32_t)preview_frame,
                                                  &menu_objects.sheets[AOS_MENU_SHEET_ICONS],
                                                  &menu_sprite_state, sprites);
                static AosObjLayer layer;
                static uint8_t object_pixels[GBA_W * GBA_H * 4];
                aos_obj_layer_clear(&layer);
                for (int k = 0; k < count; ++k)
                    aos_obj_layer_draw(&layer, menu_objects.sheets, &sprites[k]);
                for (int k = 0; k < GBA_W * GBA_H; ++k) {
                    uint16_t c = menu_objects.palette[layer.index[k]];
                    object_pixels[k * 4] = (uint8_t)((c & 31) * 255 / 31);
                    object_pixels[k * 4 + 1] = (uint8_t)(((c >> 5) & 31) * 255 / 31);
                    object_pixels[k * 4 + 2] = (uint8_t)(((c >> 10) & 31) * 255 / 31);
                    object_pixels[k * 4 + 3] = layer.index[k] ? 255 : 0;
                }
                SDL_UpdateTexture(menu_objects_texture, NULL, object_pixels, GBA_W * 4);
                SDL_FRect whole_screen = {0, 0, GBA_W, GBA_H};
                SDL_RenderTexture(renderer, menu_objects_texture, NULL, &whole_screen);
            }
            /* Without the ROM's objects: the host's diagnostic labels. */
            SDL_SetRenderDrawColor(renderer, 245, 235, 210, 255);
            if (have_menu_objects) {
            } else if (name_entry) {
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
                if (!native_prompt)
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
                        snprintf(line, sizeof line, "FILE %u %.8s A%02u R%03u HP%d%s", i + 1,
                                 name, slots[i].data.area, slots[i].data.room,
                                 slots[i].data.hp, quicks[i].present ? " QUICK" : "");
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
                if (confirming && native_prompt && !native_menu->confirm_active) {
                    /* YES/NO are native text; the selection marker is not. */
                    unsigned choice = native_menu->file_cursor;
                    float marker = native_menu->selected_slot == 1 ?
                        (choice == 0 ? 9 : 0x14) : (choice == 0 ? 0xA : 0x15);
                    SDL_RenderDebugText(renderer, 16 + marker * 6 - 8, 136, ">");
                } else if (confirming) {
                    unsigned choice = native_menu->confirm_active ?
                        native_menu->confirm_choice : native_menu->file_cursor;
                    SDL_RenderDebugText(renderer, 55, native_prompt ? 100 : 119,
                                        choice == 0 ? "> YES      NO" : "  YES    > NO");
                } else if (!native_prompt) {
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
            } else if (lifecycle.game_mode.mode == AOS_MODE_TITLE_SCREEN && !title_texture) {
                SDL_RenderDebugText(renderer, 72, 122, "PRESS START");
            } else if (!have_logos && lifecycle.game_mode.mode == AOS_MODE_LICENSED_BY_NINTENDO) {
                SDL_RenderDebugText(renderer, 48, 76, "LICENSED BY NINTENDO");
            } else if (!have_logos && lifecycle.game_mode.mode == AOS_MODE_KONAMI_LOGO) {
                SDL_RenderDebugText(renderer, 86, 76, "KONAMI");
            }
        }
        uint16_t brightness = lifecycle.game_mode.mode == AOS_MODE_GAME_OVER ? 0 :
            in_main_menu ? lifecycle.main_menu.brightness :
            lifecycle.game_mode.mode == AOS_MODE_INTRO_CUTSCENE ?
                lifecycle.intro_cutscene.brightness :
            lifecycle.game_mode.mode == AOS_MODE_TITLE_SCREEN ?
                (title_texture ? 0 : lifecycle.title.bldy) :
            have_logos && (lifecycle.game_mode.mode == AOS_MODE_LICENSED_BY_NINTENDO ||
                           lifecycle.game_mode.mode == AOS_MODE_KONAMI_LOGO)
                ? 0 : lifecycle.startup_logo.brightness;
        uint8_t darkness = brightness > 16 ? 16 : (uint8_t)brightness;
        if (darkness > 0) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0,
                                   (uint8_t)(darkness * 255u / 16u));
            SDL_FRect whole = {0, 0, GBA_W, GBA_H};
            SDL_RenderFillRect(renderer, &whole);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
        /* Diagnostic 'ready' light; not part of native GBA imagery, so not
         * drawn over the screens shown natively (logos, title, main menu). */
        bool native_frame =
            (in_main_menu && have_menu_objects) ||
            (lifecycle.game_mode.mode == AOS_MODE_TITLE_SCREEN && title_texture) ||
            (have_logos && (lifecycle.game_mode.mode == AOS_MODE_LICENSED_BY_NINTENDO ||
                            lifecycle.game_mode.mode == AOS_MODE_KONAMI_LOGO));
        if (interactive && !native_frame) {
            SDL_SetRenderDrawColor(renderer, 120, 200, 145, 255);
            SDL_FRect marker = {227, 147, 5, 5};
            SDL_RenderFillRect(renderer, &marker);
        }
        if (screenshot_path && preview_frame == screenshot_frame) {
            SDL_Surface *shot = SDL_RenderReadPixels(renderer, NULL);
            if (!shot || !SDL_SaveBMP(shot, screenshot_path))
                fprintf(stderr, "Cannot save %s: %s\n", screenshot_path, SDL_GetError());
            SDL_DestroySurface(shot);
            running = false;
        }
        ++preview_frame;
        SDL_RenderPresent(renderer);
        next_frame += 1000000000ull / 60ull;
        uint64_t now = SDL_GetTicksNS();
        if (now < next_frame) SDL_DelayNS(next_frame - now);
        else if (now - next_frame > 100000000ull) next_frame = now;
    }
    destroy_main_menu_textures(main_menu_textures);
    aos_main_menu_graphics_free(&main_menu_graphics);
    if (menu_bg0_texture) SDL_DestroyTexture(menu_bg0_texture);
    if (title_texture) SDL_DestroyTexture(title_texture);
    if (menu_objects_texture) SDL_DestroyTexture(menu_objects_texture);
    if (intro_actor_texture) SDL_DestroyTexture(intro_actor_texture);
    if (logo_texture) SDL_DestroyTexture(logo_texture);
    destroy_intro_textures(intro_background_textures);
    aos_intro_graphics_free(&intro_graphics);
    fusion_rom_close(&intro_rom);
    gba_input_close(&gamepad);
    if (audio_stream) SDL_DestroyAudioStream(audio_stream);
    aos_audio_set_start_sink(NULL, NULL);
    aos_mp2k_engine_destroy(&mp2k);
    if (art) SDL_DestroyTexture(art);
    SDL_DestroyRenderer(renderer);
    fusion_host_destroy_window(window);
    SDL_free(preference_path);
    fusion_host_quit();
    return 0;
}

#ifndef FUSION_EMBEDDED
int main(int argc, char **argv) {
    return aria_title_preview_main(argc, argv);
}
#endif
