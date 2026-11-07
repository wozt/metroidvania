/* SPDX-License-Identifier: GPL-3.0-only */
/* The Metroid Vania launcher: the native title screens of Aria of Sorrow
 * (aos_title_screen) and of Zero Mission (mzm_title_screen) at once, split
 * by the animated patch-0046 geometry of metroidvania_split. The music is
 * the one of the game covering most of the bottom half (the one START
 * launches); independent host gains keep both games' SFX audible. A first
 * START skips both titles' openings; once both wait for START, START goes
 * to the game covering most of the bottom half: its title plays its own
 * exit, then that game's frontend (fusion_aria_title_preview --main-menu or
 * fusion_room_runtime --file-select) runs in this process and window
 * (fusion_host), and so do the frontends it launches. The ROMs' data stay
 * outside Git. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aos_audio.h"
#include "aos_game_mode.h"
#include "aos_input.h"
#include "aos_mp2k.h"
#include "aos_title_graphics.h"
#include "aos_title_screen.h"
#include "fusion_host.h"
#include "gba_input.h"
#include "gba_ppu.h"
#include "metroidvania_split.h"
#include "mzm_audio.h"
#include "mzm_rom.h"
#include "mzm_screen.h"
#include "mzm_sfx.h"
#include "mzm_title_screen.h"

/* The embedded frontends (FUSION_EMBEDDED). */
int room_runtime_main(int argc, char **argv);
int aria_title_preview_main(int argc, char **argv);
int aria_runtime_main(int argc, char **argv);

#define DEFAULT_ARIA_ROM "roms/Castlevania - Aria of Sorrow (USA).gba"
#define ARIA_SONG_TABLE "assets/extracted/aria/sound/runtime/song_table.tsv"
#define ARIA_SOUND_BANK "assets/extracted/aria/sound/runtime/sound_bank.bin"
#define MZM_ROM_BANK "assets/extracted/metroid/rom/rom_bank.bin"
#define MZM_ROM_SYMBOLS "assets/extracted/metroid/rom/rom_symbols.tsv"
#define MZM_SOUND_BANK "assets/extracted/metroid/sound/runtime/sound_bank.bin"
#define MZM_SOUND_TABLES "assets/extracted/metroid/sound/runtime/sound_tables.tsv"

enum {
    MZM_AUDIO_RATE = 65536,
    PIXELS = MV_SCREEN_W * MV_SCREEN_H,
    CROSSFADE_FRAMES = 40,      /* the music's cross-fade, about 0.7 s */
    MAX_PRESSES = 1024
};

/* Zero Mission's sound is pulled by the device, one GBA frame at a time, as
 * in fusion_room_runtime; the title only changes the engine's state, under
 * the stream's lock. */
typedef struct {
    int16_t samples[2 * 2048];
    int count, offset;
} MzmPull;

static SDL_AudioStream *mzm_stream;
static AosMp2kEngine mp2k;

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

static bool aria_ready(const AosTitleScreen *t) {
    return t->stage == 8 && !t->sliding;
}

static int parse_frames(char *text, long frames[MAX_PRESSES]) {
    int count = 0;
    for (char *p = text; *p && count < MAX_PRESSES;) {
        frames[count++] = strtol(p, &p, 10);
        if (*p == ',') ++p;
        else if (*p) break;
    }
    return count;
}

static bool listed(const long *frames, int count, long frame) {
    for (int k = 0; k < count; ++k)
        if (frames[k] == frame) return true;
    return false;
}

/* The frontends this process can run in its window. */
static int run_frontend(const char *program, int argc, char **argv) {
    if (!strcmp(program, "fusion_room_runtime")) return room_runtime_main(argc, argv);
    if (!strcmp(program, "fusion_aria_title_preview")) return aria_title_preview_main(argc, argv);
    if (!strcmp(program, "fusion_aria_runtime")) return aria_runtime_main(argc, argv);
    fprintf(stderr, "Unknown frontend %s\n", program);
    return 1;
}

static void trigger_split_pulse(MvSplit *split, bool aria, MvSplitHold *hold) {
    uint16_t sound = mv_split_native_sound(aria);
    /* Each beat climbs one semitone from six below the source pitch. */
    int semitones = mv_split_pulse_semitones(hold ? hold->pulse_index : 0);
    if (hold) ++hold->pulse_index;
    if (aria) {
        aos_audio_play_song(sound);
        AosAudioSongInfo info;
        if (aos_audio_song_info(sound, &info))
            aos_mp2k_player_pitch(&mp2k, info.player, (int8_t)semitones);
    } else {
        mzm_sfx_play(sound);
        int player = mzm_audio_sound_player(sound);
        if (player >= 0) mzm_audio_set_player_pitch(player, (int8_t)semitones);
    }
    /* Both native effects complete inside the prototype's visual wave. */
    mv_split_trigger_pulse(split, aria, 0.22f);
}

typedef struct {
    const char *aria_rom_path, *capture_dir, *split_config_path;
    bool mute;
    long capture_frames[MAX_PRESSES], presses[MAX_PRESSES];
    int capture_count, press_count;
    const char *frontend_args[16];   /* --frontend-arg: appended (scripted runs) */
    int frontend_arg_count;
} Options;

enum { TITLE_QUIT = 0, TITLE_CHOSEN = 1, TITLE_ERROR = -1,
       CHOOSING_FRAMES = 180 };    /* the chosen title's exit, at most 3 s */

/* The shared title screen until a game is chosen (*chosen) or the window
 * closes; `window` is NULL for headless captures. */
static int shared_title(const Options *o, SDL_Window *window, MvGame *chosen_game) {
    static MvSplit split;
    char error[256] = "";
    if (!mv_split_init(&split, o->split_config_path, error, sizeof error)) {
        fprintf(stderr, "Invalid rotating divider: %s\n", error);
        return TITLE_ERROR;
    }
    /* Aria: the title's layers, sheets and palette rows from the ROM. */
    FusionRom aria_rom = {0};
    static AosTitleGraphics aria_graphics;
    if (!fusion_rom_open(FUSION_ASSET_GAME_ARIA, o->aria_rom_path, &aria_rom, error, sizeof error) ||
        !aos_title_graphics_init(&aria_graphics, &aria_rom, error, sizeof error) ||
        aria_rom.size < 0x20A000) {
        fprintf(stderr, "No Aria title (%s): %s\n", o->aria_rom_path, error);
        fusion_rom_close(&aria_rom);
        return TITLE_ERROR;
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
        return TITLE_ERROR;
    }
    mzm_title_enter(&mzm, false);

    /* Both games' sound runs all the time; the gains cross-fade toward the
     * game at the top. */
    aos_mp2k_engine_init(&mp2k);
    aos_audio_reset();
    SDL_AudioStream *aria_stream = NULL;
    static MzmPull pull;
    memset(&pull, 0, sizeof pull);
    if (!o->mute) {
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
            SDL_AudioSpec aria_spec = {SDL_AUDIO_S8, 1, AOS_MP2K_HOST_RATE};
            aria_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &aria_spec,
                                                    NULL, NULL);
            if (aria_stream) SDL_ResumeAudioStreamDevice(aria_stream);
        } else {
            fprintf(stderr, "No Aria sound: %s\n", error);
        }
    }

    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;
    SDL_Gamepad *pads[8] = {0};
    if (window) {
        SDL_Renderer *previous = SDL_GetRenderer(window);
        if (previous) SDL_DestroyRenderer(previous);
        SDL_SetWindowTitle(window, "Metroid Vania");
        renderer = SDL_CreateRenderer(window, NULL);
        texture = renderer ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                               SDL_TEXTUREACCESS_STREAMING, MV_SCREEN_W,
                                               MV_SCREEN_H)
                           : NULL;
        if (!texture) {
            fprintf(stderr, "No window: %s\n", SDL_GetError());
            return TITLE_ERROR;
        }
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
        SDL_SetRenderLogicalPresentation(renderer, MV_SCREEN_W, MV_SCREEN_H,
                                         SDL_LOGICAL_PRESENTATION_LETTERBOX);
        /* The gamepads already connected (a frontend may have closed them). */
        int count = 0;
        SDL_JoystickID *ids = SDL_GetGamepads(&count);
        for (int k = 0; ids && k < count && k < 8; ++k) pads[k] = SDL_OpenGamepad(ids[k]);
        SDL_free(ids);
    }

    static uint8_t aria_pixels[PIXELS * 4], mzm_pixels[PIXELS * 4], pixels[PIXELS * 4];
    static uint32_t mzm_argb[PIXELS];
    bool running = true;
    long last_frame = 0;
    for (int k = 0; k < o->capture_count; ++k)
        if (o->capture_frames[k] > last_frame) last_frame = o->capture_frames[k];
    /* START's state: the openings being skipped, then the chosen game's
     * title playing its exit (the line stops turning). */
    bool skipping = false, choosing = false, chosen_done = false, mzm_pending = false;
    int choosing_frames = 0;
    MvGame chosen = MV_GAME_ARIA;
    uint16_t mzm_held = 0;
    MvSplitHold aria_hold = {0}, metroid_hold = {0};
    float gain[2] = {0.0f, 1.0f};   /* Zero Mission is at the bottom first */
    uint64_t next_frame = SDL_GetTicksNS();
    for (long frame = 0; running; ++frame) {
        bool start = listed(o->presses, o->press_count, frame);
        SDL_Event event;
        while (window && SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE))
                running = false;
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                     (event.key.key == SDLK_RETURN || event.key.key == SDLK_KP_ENTER))
                start = true;
            else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                     event.key.key == SDLK_X) {
                if (mv_split_hold_press(&metroid_hold))
                    trigger_split_pulse(&split, false, &metroid_hold);
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       event.key.key == SDLK_Y) {
                if (mv_split_hold_press(&aria_hold))
                    trigger_split_pulse(&split, true, &aria_hold);
            } else if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_X) {
                mv_split_hold_release(&metroid_hold);
            } else if (event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_Y) {
                mv_split_hold_release(&aria_hold);
            } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
                for (int k = 0; k < 8; ++k)
                    if (!pads[k]) {
                        pads[k] = SDL_OpenGamepad(event.gdevice.which);
                        break;
                    }
            } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
                for (int k = 0; k < 8; ++k)
                    if (pads[k] && SDL_GetGamepadID(pads[k]) == event.gdevice.which) {
                        SDL_CloseGamepad(pads[k]);
                        pads[k] = NULL;
                    }
                mv_split_hold_release(&metroid_hold);
                mv_split_hold_release(&aria_hold);
            } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                       event.gbutton.button == SDL_GAMEPAD_BUTTON_START)
                start = true;
            else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                     event.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH) {
                if (mv_split_hold_press(&metroid_hold))
                    trigger_split_pulse(&split, false, &metroid_hold);
            } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
                       event.gbutton.button == SDL_GAMEPAD_BUTTON_WEST) {
                if (mv_split_hold_press(&aria_hold))
                    trigger_split_pulse(&split, true, &aria_hold);
            } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
                       event.gbutton.button == SDL_GAMEPAD_BUTTON_NORTH) {
                mv_split_hold_release(&metroid_hold);
            } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
                       event.gbutton.button == SDL_GAMEPAD_BUTTON_WEST) {
                mv_split_hold_release(&aria_hold);
            }
        }
        if (!running) break;
        unsigned metroid_pulses = 0, aria_pulses = 0;
        bool launch_metroid = mv_split_hold_tick(
            &metroid_hold, PROTO_FIXED_TIMESTEP, &metroid_pulses);
        bool launch_aria = mv_split_hold_tick(
            &aria_hold, PROTO_FIXED_TIMESTEP, &aria_pulses);
        while (metroid_pulses--)
            trigger_split_pulse(&split, false, &metroid_hold);
        while (aria_pulses--)
            trigger_split_pulse(&split, true, &aria_hold);
        if (!chosen_done && (launch_metroid || launch_aria)) {
            chosen = launch_metroid ? MV_GAME_MZM : MV_GAME_ARIA;
            chosen_done = true;
            choosing = true;
            skipping = false;
        }
        uint16_t angle = mv_split_rotation_angle(&split);
        bool both_ready = aria_ready(&aria) && mzm_title_ready(&mzm);
        uint16_t aria_input = 0, mzm_press = 0;
        if (start && !choosing) {
            if (both_ready) {
                /* START to the game at the bottom, through its own title. */
                choosing = true;
                choosing_frames = 0;
                chosen = mv_split_bottom(angle);
                if (chosen == MV_GAME_ARIA) aria_input = AOS_KEY_START;
                else mzm_pending = true;
                skipping = false;
            } else {
                skipping = true;
            }
        }
        /* Skipping: one START to each opening that can be skipped. */
        if (skipping) {
            if (aria.stage >= 2 && aria.stage <= 7) aria_input = AOS_KEY_START;
            if (mzm_title_skippable(&mzm) && !mzm_held) mzm_press = GBA_KEY_START;
            if (both_ready) skipping = false;
        }
        /* A press to Zero Mission's title needs a released key before it. */
        if (mzm_pending && !mzm_held) {
            mzm_press = GBA_KEY_START;
            mzm_pending = false;
        }
        mzm_held = mzm_press;
        /* A chosen title that does not leave (it may have missed the
         * press) gives the line back. */
        if (choosing && ++choosing_frames > CHOOSING_FRAMES && !chosen_done) choosing = false;

        /* No idle introduction or demo here: their timers are held. */
        if (aria.stage == 8 && !aria.chosen) aria.counter = 10 * 60;
        mzm.demo_timer = 0;
        int32_t aria_mode = aos_title_screen_update(&aria, aria_input, 0);
        if (aria.song >= 0) aos_audio_play_song((uint16_t)aria.song);
        if (aria_mode != AOS_MODE_SAME) {
            if (choosing && chosen == MV_GAME_ARIA) chosen_done = true;
            else aria.stage = 0;     /* its idle introduction: start again */
        }
        mzm_screen_input(&mzm_screen, mzm_press);
        if (mzm_title_update(&mzm, &mzm_screen)) {
            if (choosing && chosen == MV_GAME_MZM && mzm.sub_mode2 == 1) chosen_done = true;
            else mzm_title_enter(&mzm, true);   /* its demo: back past the comets */
        }

        /* The music of the game covering most of the bottom half, the one
         * START launches. The other fades out, then pauses (its sequencer
         * stops) until its turn comes back. */
        MvGame heard = choosing ? chosen : mv_split_bottom(angle);
        for (int g = 0; g < 2; ++g) {
            const float step = 1.0f / CROSSFADE_FRAMES;
            float target = g == (int)heard ? 1.0f : 0.0f;
            if (gain[g] < target) gain[g] = gain[g] + step > target ? target : gain[g] + step;
            else gain[g] = gain[g] - step < target ? target : gain[g] - step;
        }
        if (mzm_stream) {
            SDL_LockAudioStream(mzm_stream);
            mzm_audio_set_player_gain(0, (uint8_t)(gain[MV_GAME_MZM] * 255.0f));
            SDL_UnlockAudioStream(mzm_stream);
        }
        /* MUS_27 plays once (the game leaves for its introduction before it
         * ends); here the title stays, so it starts again. */
        AosAudioSongInfo title_song;
        if (aria_stream && aria.stage >= 2 && aria.stage <= 8 && !aria.chosen &&
            aos_audio_song_info(27, &title_song) && mp2k.players[title_song.player].ended)
            aos_audio_play_song(27);
        if (aria_stream) {
            aos_mp2k_player_host_gain(&mp2k, 0,
                (uint8_t)(gain[MV_GAME_ARIA] * 255.0f));
            aos_audio_volume_update();
            aos_mp2k_engine_frame(&mp2k);
            int8_t pcm[AOS_MP2K_SAMPLES_PER_FRAME];
            aos_mp2k_mix(&mp2k, pcm, sizeof pcm);
            /* A slow frame never builds up a delay. */
            if (SDL_GetAudioStreamQueued(aria_stream) < (int)sizeof pcm * AOS_MP2K_HOST_QUEUE_FRAMES)
                SDL_PutAudioStreamData(aria_stream, pcm, sizeof pcm);
        }
        mv_split_tick(&split, PROTO_FIXED_TIMESTEP, !choosing);

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
        mv_split_compose(&split, aria_pixels, mzm_pixels, pixels);

        if (!window) {
            if (listed(o->capture_frames, o->capture_count, frame)) {
                char path[1024];
                snprintf(path, sizeof path, "%s/mv_%04ld.bmp", o->capture_dir, frame);
                if (!write_bmp(path, pixels)) fprintf(stderr, "Cannot write %s\n", path);
                printf("frame %ld: aria stage %u, zero mission %s, %s\n", frame, aria.stage,
                       mzm_title_ready(&mzm) ? "ready" : "opening",
                       choosing ? "starting" : skipping ? "skipping" : "idle");
            }
            if (chosen_done) {
                printf("frame %ld: launch %s\n", frame,
                       chosen == MV_GAME_ARIA ? "Aria" : "Zero Mission");
                running = false;
            }
            if (frame >= last_frame) running = false;
            continue;
        }
        if (chosen_done) break;
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
    /* The titles' resources go before the game's frontend takes them. */
    if (mzm_stream) SDL_DestroyAudioStream(mzm_stream);
    mzm_stream = NULL;
    mzm_sfx_set_sink(NULL, NULL);
    if (aria_stream) SDL_DestroyAudioStream(aria_stream);
    aos_audio_set_start_sink(NULL, NULL);
    aos_mp2k_engine_destroy(&mp2k);
    mzm_audio_close();
    mzm_screen_free(&mzm_screen);
    mzm_rom_close();
    fusion_rom_close(&aria_rom);
    for (int k = 0; k < 8; ++k)
        if (pads[k]) SDL_CloseGamepad(pads[k]);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    *chosen_game = chosen;
    return chosen_done ? TITLE_CHOSEN : TITLE_QUIT;
}

int main(int argc, char **argv) {
    static Options o = {.aria_rom_path = DEFAULT_ARIA_ROM};
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--aria-rom") && i + 1 < argc) o.aria_rom_path = argv[++i];
        else if (!strcmp(argv[i], "--split-config") && i + 1 < argc)
            o.split_config_path = argv[++i];
        else if (!strcmp(argv[i], "--mute")) o.mute = true;
        else if (!strcmp(argv[i], "--capture") && i + 2 < argc) {
            /* --capture DIR F1,F2,...: headless, BMPs of those frames. */
            o.capture_dir = argv[++i];
            o.capture_count = parse_frames(argv[++i], o.capture_frames);
            o.mute = true;
        } else if (!strcmp(argv[i], "--press") && i + 1 < argc) {
            /* --press F1,F2,...: START at those frames (scripted runs). */
            o.press_count = parse_frames(argv[++i], o.presses);
        } else if (!strcmp(argv[i], "--frontend-arg") && i + 1 < argc && o.frontend_arg_count < 16) {
            o.frontend_args[o.frontend_arg_count++] = argv[++i];
        } else {
            fprintf(stderr, "Usage: fusion_metroidvania [--aria-rom FILE] "
                            "[--split-config FILE.json] [--mute] "
                            "[--capture DIR FRAME,...] [--press FRAME,...] [--frontend-arg ARG]\n");
            return 2;
        }
    }
    if (!SDL_Init((o.capture_dir ? 0 : SDL_INIT_VIDEO | SDL_INIT_GAMEPAD) |
                  (o.mute ? 0 : SDL_INIT_AUDIO))) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = NULL;
    if (!o.capture_dir) {
        window = SDL_CreateWindow("Metroid Vania", MV_SCREEN_W * 4, MV_SCREEN_H * 4,
                                  SDL_WINDOW_RESIZABLE);
        if (!window) {
            fprintf(stderr, "No window: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
    }
    int rc = 0;
    for (;;) {
        MvGame chosen;
        int title = shared_title(&o, window, &chosen);
        if (title != TITLE_CHOSEN || !window) {
            rc = title == TITLE_ERROR ? 1 : 0;
            break;
        }
        /* The chosen game in this window, then whatever it launches, until
         * one of them comes back to the shared title (or the window closes). */
        fusion_host_embed(window);
        const char *args[24] = {NULL};
        int count = 0;
        if (chosen == MV_GAME_ARIA) {
            args[count++] = "fusion_aria_title_preview";
            args[count++] = "--rom";
            args[count++] = o.aria_rom_path;
            args[count++] = "--main-menu";
        } else {
            args[count++] = "fusion_room_runtime";
            args[count++] = "--file-select";
        }
        for (int k = 0; k < o.frontend_arg_count; ++k) args[count++] = o.frontend_args[k];
        args[count] = NULL;
        fusion_host_launch(args);
        int frontend_argc;
        char **frontend_argv;
        const char *program;
        bool back = false;
        while (fusion_host_take_launch(&frontend_argc, &frontend_argv, &program)) {
            if (!strcmp(program, "fusion_metroidvania")) {
                back = true;
                break;
            }
            fprintf(stderr, "Metroid Vania: %s\n", program);
            rc = run_frontend(program, frontend_argc, frontend_argv);
        }
        fusion_host_embed(NULL);
        if (!back) break;
        fprintf(stderr, "Metroid Vania: back to the shared title\n");
    }
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return rc;
}
