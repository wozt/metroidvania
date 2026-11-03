/* SPDX-License-Identifier: GPL-3.0-only */
/* The Metroid Vania launcher: the native title screens of Aria of Sorrow
 * (aos_title_screen) and of Zero Mission (mzm_title_screen) at once, split
 * by the rotating line of metroidvania_split. The music is the one of the
 * game covering most of the top half; START launches the game covering most
 * of the bottom half (fusion_aria_title_preview --main-menu or
 * fusion_room_runtime --file-select). The ROMs' data stay outside Git. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aos_audio.h"
#include "aos_game_mode.h"
#include "aos_mp2k.h"
#include "aos_title_graphics.h"
#include "aos_title_screen.h"
#include "gba_ppu.h"
#include "metroidvania_split.h"
#include "mzm_audio.h"
#include "mzm_rom.h"
#include "mzm_screen.h"
#include "mzm_sfx.h"
#include "mzm_title_screen.h"

#define DEFAULT_ARIA_ROM "roms/Castlevania - Aria of Sorrow (USA).gba"
#define ARIA_SONG_TABLE "assets/extracted/aria/sound/runtime/song_table.tsv"
#define ARIA_SOUND_BANK "assets/extracted/aria/sound/runtime/sound_bank.bin"
#define MZM_ROM_BANK "assets/extracted/metroid/rom/rom_bank.bin"
#define MZM_ROM_SYMBOLS "assets/extracted/metroid/rom/rom_symbols.tsv"
#define MZM_SOUND_BANK "assets/extracted/metroid/sound/runtime/sound_bank.bin"
#define MZM_SOUND_TABLES "assets/extracted/metroid/sound/runtime/sound_tables.tsv"

enum { MZM_AUDIO_RATE = 65536, PIXELS = MV_SCREEN_W * MV_SCREEN_H };

/* Zero Mission's sound is pulled by the device, one GBA frame at a time, as
 * in fusion_room_runtime; the title only changes the engine's state, under
 * the stream's lock. */
typedef struct {
    int16_t samples[2 * 2048];
    int count, offset;
} MzmPull;

static SDL_AudioStream *mzm_stream;

static void SDLCALL mzm_pull(void *userdata, SDL_AudioStream *stream, int additional, int total) {
    MzmPull *pull = userdata;
    (void)total;
    int frames = additional / 4;
    while (frames > 0) {
        if (pull->offset >= pull->count) {
            mzm_audio_update();
            pull->count = mzm_audio_render(pull->samples, 2048, MZM_AUDIO_RATE);
            pull->offset = 0;
            if (pull->count <= 0) {
                pull->count = frames < 2048 ? frames : 2048;
                memset(pull->samples, 0, (size_t)pull->count * 4);
            }
        }
        int n = pull->count - pull->offset;
        if (n > frames) n = frames;
        SDL_PutAudioStreamData(stream, &pull->samples[pull->offset * 2], n * 4);
        pull->offset += n;
        frames -= n;
    }
}

static void mzm_sfx_sink(void *context, MzmSfxAction action, uint16_t sound, uint16_t timer) {
    (void)context;
    if (!mzm_stream) return;
    SDL_LockAudioStream(mzm_stream);
    switch (action) {
    case MZM_SFX_PLAY: mzm_sound_play(sound); break;
    case MZM_SFX_PLAY_NOT_PLAYING: mzm_sound_play_not_already_playing(sound); break;
    case MZM_SFX_STOP: mzm_sound_stop(sound); break;
    case MZM_SFX_FADE: mzm_sound_fade(sound, timer); break;
    case MZM_SFX_STOP_ALL: mzm_audio_stop_all(); break;
    case MZM_SFX_FADE_ALL: mzm_sound_fade_all(timer); break;
    case MZM_SFX_MUSIC_PLAY: mzm_music_play(sound, (uint8_t)timer); break;
    case MZM_SFX_MUSIC_INSERT: mzm_music_insert(sound, (uint8_t)timer); break;
    case MZM_SFX_BACKUP_TRACK2: mzm_audio_backup_track2(); break;
    case MZM_SFX_RETRIEVE_TRACK2: mzm_audio_retrieve_track2(); break;
    case MZM_SFX_MUSIC_FADE: mzm_music_fade(timer); break;
    case MZM_SFX_MUSIC_FADE_AND_QUEUE: mzm_music_fade_and_queue(timer, sound, 0); break;
    case MZM_SFX_MUSIC_PRIORITY: mzm_music_update_priority((uint8_t)timer); break;
    case MZM_SFX_MUSIC_AFTER_ALARM: mzm_music_after_alarm_disable(); break;
    }
    SDL_UnlockAudioStream(mzm_stream);
}

static void aria_start_song(void *context, uint16_t song, const AosAudioSongInfo *info) {
    (void)song;
    (void)aos_mp2k_player_start(context, info->player, info->header_pointer);
}

static bool write_bmp(const char *path, const uint8_t *rgba) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    uint32_t size = 54 + PIXELS * 3;
    uint8_t header[54] = {'B', 'M'};
    for (int k = 0; k < 4; ++k) header[2 + k] = (uint8_t)(size >> (8 * k));
    header[10] = 54;
    header[14] = 40;
    header[18] = MV_SCREEN_W;
    header[22] = MV_SCREEN_H;
    header[26] = 1;
    header[28] = 24;
    fwrite(header, 1, sizeof header, f);
    for (int y = MV_SCREEN_H - 1; y >= 0; --y)
        for (int x = 0; x < MV_SCREEN_W; ++x) {
            const uint8_t *p = rgba + ((size_t)y * MV_SCREEN_W + (size_t)x) * 4;
            uint8_t bgr[3] = {p[2], p[1], p[0]};
            fwrite(bgr, 1, 3, f);
        }
    return fclose(f) == 0;
}

/* The game at the bottom: its frontend, started from this executable's
 * folder in the current working directory. */
static bool launch(MvGame game, const char *aria_rom) {
    const char *base = SDL_GetBasePath();
    char program[1024];
    snprintf(program, sizeof program, "%s%s", base ? base : "./",
             game == MV_GAME_ARIA ? "fusion_aria_title_preview" : "fusion_room_runtime");
    const char *aria_args[] = {program, "--rom", aria_rom, "--main-menu", NULL};
    const char *mzm_args[] = {program, "--file-select", NULL};
    SDL_Process *process = SDL_CreateProcess(game == MV_GAME_ARIA ? aria_args : mzm_args, false);
    if (!process) {
        fprintf(stderr, "Cannot launch %s: %s\n", program, SDL_GetError());
        return false;
    }
    SDL_DestroyProcess(process);
    return true;
}

int main(int argc, char **argv) {
    const char *aria_rom_path = DEFAULT_ARIA_ROM, *capture_dir = NULL;
    bool mute = false;
    long capture_frames[32];
    int capture_count = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--aria-rom") && i + 1 < argc) aria_rom_path = argv[++i];
        else if (!strcmp(argv[i], "--mute")) mute = true;
        else if (!strcmp(argv[i], "--capture") && i + 2 < argc) {
            /* --capture DIR F1,F2,...: headless, BMPs of those frames. */
            capture_dir = argv[++i];
            for (char *p = argv[++i]; *p && capture_count < 32;) {
                capture_frames[capture_count++] = strtol(p, &p, 10);
                if (*p == ',') ++p;
            }
            mute = true;
        } else {
            fprintf(stderr, "Usage: fusion_metroidvania [--aria-rom FILE] [--mute] "
                            "[--capture DIR FRAME,...]\n");
            return 2;
        }
    }

    /* Aria: the title's layers, sheets and palette rows from the ROM. */
    FusionRom aria_rom = {0};
    static AosTitleGraphics aria_graphics;
    char error[256];
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, aria_rom_path, &aria_rom, error, sizeof error) ||
        !aos_title_graphics_init(&aria_graphics, &aria_rom, error, sizeof error) ||
        aria_rom.size < 0x20A000) {
        fprintf(stderr, "No Aria title (%s): %s\n", aria_rom_path, error);
        return 1;
    }
    static AosTitleScreen aria;
    aos_title_screen_init(&aria, &aria_graphics, aria_rom.data + 0x209308,
                          aria_rom.data + 0x0E622C);
    /* Zero Mission: the title on its own GBA display, from the ROM bank. */
    static GbaPpu mzm_ppu;
    static MzmScreen mzm_screen;
    static MzmTitleScreen mzm;
    if (!mzm_rom_open(MZM_ROM_BANK, MZM_ROM_SYMBOLS) || !mzm_screen_init(&mzm_screen, &mzm_ppu)) {
        fprintf(stderr, "No Zero Mission ROM bank: ./build/fusion_rebuild (%s)\n", MZM_ROM_BANK);
        fusion_rom_close(&aria_rom);
        return 1;
    }
    mzm_title_enter(&mzm, false);

    if (!SDL_Init((capture_dir ? 0 : SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) |
                  (mute ? 0 : SDL_INIT_AUDIO))) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    /* Both games' sound run all the time; the stream of the game at the
     * bottom is silent (gain 0). */
    static AosMp2kEngine mp2k;
    aos_mp2k_engine_init(&mp2k);
    aos_audio_reset();
    SDL_AudioStream *aria_stream = NULL;
    static MzmPull pull;
    if (!mute) {
        SDL_AudioSpec mzm_spec = {SDL_AUDIO_S16, 2, MZM_AUDIO_RATE};
        if (mzm_audio_open(MZM_SOUND_BANK, MZM_SOUND_TABLES)) {
            mzm_audio_initialize();
            mzm_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &mzm_spec,
                                                   mzm_pull, &pull);
            if (mzm_stream) {
                mzm_sfx_set_sink(mzm_sfx_sink, NULL);
                SDL_ResumeAudioStreamDevice(mzm_stream);
            }
        } else {
            fprintf(stderr, "No Zero Mission sound: %s\n", MZM_SOUND_BANK);
        }
        if (aos_audio_load_song_table(ARIA_SONG_TABLE, error, sizeof error) &&
            aos_mp2k_engine_load_bank(&mp2k, ARIA_SOUND_BANK, error, sizeof error)) {
            aos_audio_set_start_sink(aria_start_song, &mp2k);
            SDL_AudioSpec aria_spec = {SDL_AUDIO_S8, 1, AOS_MP2K_MIX_RATE};
            aria_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &aria_spec,
                                                    NULL, NULL);
            if (aria_stream) SDL_ResumeAudioStreamDevice(aria_stream);
        } else {
            fprintf(stderr, "No Aria sound: %s\n", error);
        }
    }

    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    if (!capture_dir) {
        window = SDL_CreateWindow("Metroid Vania", MV_SCREEN_W * 4, MV_SCREEN_H * 4,
                                  SDL_WINDOW_RESIZABLE);
        renderer = window ? SDL_CreateRenderer(window, NULL) : NULL;
        texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                               SDL_TEXTUREACCESS_STREAMING, MV_SCREEN_W,
                                               MV_SCREEN_H)
                           : NULL;
        if (!texture) {
            fprintf(stderr, "No window: %s\n", SDL_GetError());
            return 1;
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderLogicalPresentation(renderer, MV_SCREEN_W, MV_SCREEN_H,
                                         SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }

    static uint8_t aria_pixels[PIXELS * 4], mzm_pixels[PIXELS * 4], pixels[PIXELS * 4];
    static uint32_t mzm_argb[PIXELS];
    bool running = true, start = false;
    long last_capture = 0;
    for (int k = 0; k < capture_count; ++k)
        if (capture_frames[k] > last_capture) last_capture = capture_frames[k];
    uint64_t next_frame = SDL_GetTicksNS();
    for (uint32_t frame = 0; running; ++frame) {
        SDL_Event event;
        while (!capture_dir && SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
                running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                     (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER))
                start = true;
            else if (event.type == SDL_EVENT_GAMEPAD_ADDED)
                SDL_OpenGamepad(event.gdevice.which);
            else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                     event.gbutton.button == SDL_GAMEPAD_BUTTON_START)
                start = true;
        }
        uint16_t angle = mv_split_angle(frame);
        if (start) {
            /* START: the game at the bottom, its title already played. */
            if (launch(mv_split_bottom(angle), aria_rom_path)) break;
            start = false;
        }
        /* Both titles run without input; each comes back to itself when it
         * would leave (Aria's idle introduction, Zero Mission's demo). */
        if (aos_title_screen_update(&aria, 0, 0) != AOS_MODE_SAME) aria.stage = 0;
        if (aria.song >= 0) aos_audio_play_song((uint16_t)aria.song);
        mzm_screen_input(&mzm_screen, 0);
        if (mzm_title_update(&mzm, &mzm_screen)) mzm_title_enter(&mzm, true);

        aos_audio_volume_update();
        if (aria_stream) {
            aos_mp2k_engine_frame(&mp2k);
            int8_t pcm[AOS_MP2K_SAMPLES_PER_FRAME];
            aos_mp2k_mix(&mp2k, pcm, sizeof pcm);
            SDL_PutAudioStreamData(aria_stream, pcm, sizeof pcm);
        }
        MvGame top = mv_split_top(angle);
        if (aria_stream) SDL_SetAudioStreamGain(aria_stream, top == MV_GAME_ARIA ? 1.0f : 0.0f);
        if (mzm_stream) SDL_SetAudioStreamGain(mzm_stream, top == MV_GAME_MZM ? 1.0f : 0.0f);

        aos_title_screen_render(&aria, aria_pixels);
        gba_ppu_render(mzm_screen.ppu, MV_SCREEN_W, MV_SCREEN_H, false, mzm_argb,
                       MV_SCREEN_W * 4);
        for (int k = 0; k < PIXELS; ++k) {
            uint32_t c = mzm_argb[k];
            mzm_pixels[k * 4] = (uint8_t)(c >> 16);
            mzm_pixels[k * 4 + 1] = (uint8_t)(c >> 8);
            mzm_pixels[k * 4 + 2] = (uint8_t)c;
            mzm_pixels[k * 4 + 3] = 255;
        }
        mv_split_compose(angle, aria_pixels, mzm_pixels, pixels);

        if (capture_dir) {
            for (int k = 0; k < capture_count; ++k)
                if (capture_frames[k] == (long)frame) {
                    char path[1024];
                    snprintf(path, sizeof path, "%s/mv_%04u.bmp", capture_dir, frame);
                    if (!write_bmp(path, pixels)) fprintf(stderr, "Cannot write %s\n", path);
                }
            if ((long)frame >= last_capture) running = false;
            continue;
        }
        SDL_UpdateTexture(texture, NULL, pixels, MV_SCREEN_W * 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        next_frame += 1000000000ull / 60ull;
        uint64_t now = SDL_GetTicksNS();
        if (now < next_frame) SDL_DelayNS(next_frame - now);
        else if (now - next_frame > 100000000ull) next_frame = now;
    }
    if (mzm_stream) SDL_DestroyAudioStream(mzm_stream);
    mzm_stream = NULL;
    if (aria_stream) SDL_DestroyAudioStream(aria_stream);
    aos_audio_set_start_sink(NULL, NULL);
    aos_mp2k_engine_destroy(&mp2k);
    mzm_audio_close();
    mzm_screen_free(&mzm_screen);
    mzm_rom_close();
    fusion_rom_close(&aria_rom);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
