/* SPDX-License-Identifier: GPL-3.0-only */
/* Local GTK4 browser for ROM-derived Aria and Zero Mission sound entries. */
#include "aos_audio.h"
#include "aos_mp2k.h"
#include "mzm_audio.h"
#include "mzm_sound_ids.h"
#include "metroidvania_split_audio.h"

#include <SDL3/SDL.h>
#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ARIA_TABLE FUSION_SOURCE_DIR "/assets/extracted/aria/sound/runtime/song_table.tsv"
#define ARIA_BANK FUSION_SOURCE_DIR "/assets/extracted/aria/sound/runtime/sound_bank.bin"
#define MZM_BANK FUSION_SOURCE_DIR "/assets/extracted/metroid/sound/runtime/sound_bank.bin"
#define MZM_TABLES FUSION_SOURCE_DIR "/assets/extracted/metroid/sound/runtime/sound_tables.tsv"

typedef enum { BROWSER_ARIA, BROWSER_MZM } BrowserGame;

typedef struct {
    int16_t samples[2 * 2048];
    int count, offset;
} MzmPull;

typedef struct {
    GtkWidget *game, *id, *identity, *status, *pause, *repeat;
    AosMp2kEngine aria;
    SDL_AudioStream *aria_stream, *mzm_stream;
    MzmPull mzm_pull;
    BrowserGame selected_game;
    int selected_id, player, repeat_delay;
    bool ready[2], playing, paused;
    gint64 last_tick;
    double aria_accumulator;
    guint timer;
} Browser;

static void aria_start_song(void *context, uint16_t song,
                            const AosAudioSongInfo *info) {
    (void)song;
    (void)aos_mp2k_player_start(context, info->player, info->header_pointer);
}

static void SDLCALL mzm_pull(void *userdata, SDL_AudioStream *stream,
                             int additional, int total) {
    MzmPull *pull = userdata;
    (void)total;
    int frames = additional / 4;
    while (frames > 0) {
        if (pull->offset >= pull->count) {
            mzm_audio_update();
            pull->count = mzm_audio_render(pull->samples, 2048, 65536);
            pull->offset = 0;
            if (pull->count <= 0) {
                pull->count = frames < 2048 ? frames : 2048;
                memset(pull->samples, 0, (size_t)pull->count * 4);
            }
        }
        int count = pull->count - pull->offset;
        if (count > frames) count = frames;
        SDL_PutAudioStreamData(stream, pull->samples + pull->offset * 2,
                               count * 4);
        pull->offset += count;
        frames -= count;
    }
}

static void update_identity(Browser *browser) {
    char text[160];
    if (browser->selected_game == BROWSER_ARIA) {
        AosAudioSongInfo info;
        if (aos_audio_song_info((size_t)browser->selected_id, &info))
            snprintf(text, sizeof text, "Aria ID %d (0x%03X)  -  native player %u",
                     browser->selected_id, browser->selected_id, info.player);
        else
            snprintf(text, sizeof text, "Aria ID %d (0x%03X)  -  unavailable",
                     browser->selected_id, browser->selected_id);
    } else {
        int player = mzm_audio_sound_player((uint16_t)browser->selected_id);
        snprintf(text, sizeof text, "Zero Mission ID %d (0x%03X)  -  native player %d",
                 browser->selected_id, browser->selected_id, player);
    }
    gtk_label_set_text(GTK_LABEL(browser->identity), text);
}

static void select_id(Browser *browser, int id) {
    int maximum = browser->selected_game == BROWSER_ARIA
                    ? (int)aos_audio_song_count() - 1 : MZM_SOUND_COUNT - 1;
    if (maximum < 0) maximum = 0; /* Both banks can be missing. */
    if (id < 0) id = maximum;
    if (id > maximum) id = 0;
    gtk_spin_button_set_range(GTK_SPIN_BUTTON(browser->id), 0, maximum);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(browser->id), id);
    browser->selected_id = id;
    update_identity(browser);
}

static void play_current(Browser *browser) {
    BrowserGame game = browser->selected_game;
    if (!browser->ready[game]) {
        gtk_label_set_text(GTK_LABEL(browser->status),
                           "The extracted sound bank is unavailable. Run fusion_rebuild first.");
        return;
    }
    browser->selected_id = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(browser->id));
    browser->player = -1;
    if (game == BROWSER_ARIA) {
        AosAudioSongInfo info;
        if (!aos_audio_song_info((size_t)browser->selected_id, &info)) return;
        browser->player = info.player;
        SDL_ClearAudioStream(browser->aria_stream);
        aos_audio_play_song((uint16_t)browser->selected_id);
        if (browser->mzm_stream) SDL_PauseAudioStreamDevice(browser->mzm_stream);
        SDL_ResumeAudioStreamDevice(browser->aria_stream);
    } else {
        browser->player = mzm_audio_sound_player((uint16_t)browser->selected_id);
        SDL_LockAudioStream(browser->mzm_stream);
        mzm_audio_stop_all();
        memset(&browser->mzm_pull, 0, sizeof browser->mzm_pull);
        SDL_ClearAudioStream(browser->mzm_stream);
        mzm_sound_play((uint16_t)browser->selected_id);
        SDL_UnlockAudioStream(browser->mzm_stream);
        if (browser->aria_stream) SDL_PauseAudioStreamDevice(browser->aria_stream);
        SDL_ResumeAudioStreamDevice(browser->mzm_stream);
    }
    browser->playing = true;
    browser->paused = false;
    browser->repeat_delay = 4;
    gtk_button_set_label(GTK_BUTTON(browser->pause), "Pause");
    gtk_label_set_text(GTK_LABEL(browser->status), "Playing native ROM sequence");
    update_identity(browser);
}

static void play_clicked(GtkButton *button, gpointer userdata) {
    (void)button;
    play_current(userdata);
}

static void pause_clicked(GtkButton *button, gpointer userdata) {
    Browser *browser = userdata;
    if (!browser->playing) return;
    browser->paused = !browser->paused;
    SDL_AudioStream *stream = browser->selected_game == BROWSER_ARIA
                                ? browser->aria_stream : browser->mzm_stream;
    if (browser->paused) SDL_PauseAudioStreamDevice(stream);
    else SDL_ResumeAudioStreamDevice(stream);
    gtk_button_set_label(button, browser->paused ? "Resume" : "Pause");
    gtk_label_set_text(GTK_LABEL(browser->status),
                       browser->paused ? "Paused" : "Playing native ROM sequence");
}

static void previous_clicked(GtkButton *button, gpointer userdata) {
    (void)button;
    Browser *browser = userdata;
    select_id(browser, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(browser->id)) - 1);
}

static void next_clicked(GtkButton *button, gpointer userdata) {
    (void)button;
    Browser *browser = userdata;
    select_id(browser, gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(browser->id)) + 1);
}

static void id_changed(GtkSpinButton *spin, gpointer userdata) {
    Browser *browser = userdata;
    browser->selected_id = gtk_spin_button_get_value_as_int(spin);
    update_identity(browser);
}

static void game_changed(GObject *object, GParamSpec *spec, gpointer userdata) {
    (void)spec;
    Browser *browser = userdata;
    browser->selected_game = (BrowserGame)gtk_drop_down_get_selected(GTK_DROP_DOWN(object));
    select_id(browser, browser->selected_game == BROWSER_ARIA ? 0x11C : 0x1DC);
    gtk_label_set_text(GTK_LABEL(browser->status), "Select an ID and press Play");
}

static gboolean audio_tick(gpointer userdata) {
    Browser *browser = userdata;
    gint64 now = g_get_monotonic_time();
    if (!browser->last_tick) browser->last_tick = now;
    browser->aria_accumulator += (double)(now - browser->last_tick) / 1000000.0;
    browser->last_tick = now;
    if (browser->selected_game == BROWSER_ARIA && browser->playing && !browser->paused) {
        while (browser->aria_accumulator >= 1.0 / 60.0) {
            browser->aria_accumulator -= 1.0 / 60.0;
            aos_audio_volume_update();
            aos_mp2k_engine_frame(&browser->aria);
            int8_t pcm[AOS_MP2K_SAMPLES_PER_FRAME];
            aos_mp2k_mix(&browser->aria, pcm, sizeof pcm);
            if (SDL_GetAudioStreamQueued(browser->aria_stream) <
                (int)sizeof pcm * AOS_MP2K_HOST_QUEUE_FRAMES)
                SDL_PutAudioStreamData(browser->aria_stream, pcm, sizeof pcm);
        }
    } else if (browser->aria_accumulator > 1.0 / 60.0) {
        browser->aria_accumulator = 0.0;
    }
    if (browser->repeat_delay > 0) --browser->repeat_delay;
    if (browser->playing && !browser->paused && browser->repeat_delay == 0 &&
        gtk_check_button_get_active(GTK_CHECK_BUTTON(browser->repeat))) {
        bool ended;
        if (browser->selected_game == BROWSER_ARIA) {
            const AosMp2kPlayer *player = aos_mp2k_player(&browser->aria,
                                                          (uint8_t)browser->player);
            ended = !player || player->ended;
        } else {
            SDL_LockAudioStream(browser->mzm_stream);
            ended = browser->player < 0 ||
                    !mzm_audio_player_playing(browser->player);
            SDL_UnlockAudioStream(browser->mzm_stream);
        }
        if (ended) play_current(browser);
    }
    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication *application, gpointer userdata) {
    Browser *browser = userdata;
    if (g_getenv("FUSION_SFX_DEBUG"))
        fprintf(stderr, "[sfx-browser] GTK activate: opening window\n");
    GtkWidget *window = gtk_application_window_new(application);
    gtk_window_set_title(GTK_WINDOW(window), "Metroid Vania - Native SFX Browser");
    gtk_window_set_default_size(GTK_WINDOW(window), 560, 220);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);
    gtk_window_set_child(GTK_WINDOW(window), box);

    const char *games[] = {"Castlevania: Aria of Sorrow", "Metroid: Zero Mission", NULL};
    browser->game = gtk_drop_down_new_from_strings(games);
    gtk_box_append(GTK_BOX(box), browser->game);

    browser->identity = gtk_label_new("");
    gtk_widget_add_css_class(browser->identity, "title-3");
    gtk_box_append(GTK_BOX(box), browser->identity);

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(controls, GTK_ALIGN_CENTER);
    GtkWidget *previous = gtk_button_new_with_label("Previous");
    browser->id = gtk_spin_button_new_with_range(0, 0x3FF, 1);
    gtk_widget_set_size_request(browser->id, 110, -1);
    GtkWidget *next = gtk_button_new_with_label("Next");
    GtkWidget *play = gtk_button_new_with_label("Play");
    browser->pause = gtk_button_new_with_label("Pause");
    browser->repeat = gtk_check_button_new_with_label("Repeat");
    gtk_box_append(GTK_BOX(controls), previous);
    gtk_box_append(GTK_BOX(controls), browser->id);
    gtk_box_append(GTK_BOX(controls), next);
    gtk_box_append(GTK_BOX(controls), play);
    gtk_box_append(GTK_BOX(controls), browser->pause);
    gtk_box_append(GTK_BOX(controls), browser->repeat);
    gtk_box_append(GTK_BOX(box), controls);

    browser->status = gtk_label_new("Select an ID and press Play");
    gtk_box_append(GTK_BOX(box), browser->status);

    g_signal_connect(browser->game, "notify::selected", G_CALLBACK(game_changed), browser);
    g_signal_connect(browser->id, "value-changed", G_CALLBACK(id_changed), browser);
    g_signal_connect(previous, "clicked", G_CALLBACK(previous_clicked), browser);
    g_signal_connect(next, "clicked", G_CALLBACK(next_clicked), browser);
    g_signal_connect(play, "clicked", G_CALLBACK(play_clicked), browser);
    g_signal_connect(browser->pause, "clicked", G_CALLBACK(pause_clicked), browser);
    select_id(browser, 0x11C);
    gtk_window_present(GTK_WINDOW(window));
}

static bool audio_init(Browser *browser) {
    char error[256];
    aos_mp2k_engine_init(&browser->aria);
    aos_audio_reset();
    browser->ready[BROWSER_ARIA] =
        aos_audio_load_song_table(ARIA_TABLE, error, sizeof error) &&
        aos_mp2k_engine_load_bank(&browser->aria, ARIA_BANK, error, sizeof error);
    if (browser->ready[BROWSER_ARIA]) {
        aos_audio_set_start_sink(aria_start_song, &browser->aria);
        SDL_AudioSpec spec = {SDL_AUDIO_S8, 1, AOS_MP2K_HOST_RATE};
        browser->aria_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        browser->ready[BROWSER_ARIA] = browser->aria_stream != NULL;
    }
    browser->ready[BROWSER_MZM] = mzm_audio_open(MZM_BANK, MZM_TABLES);
    if (browser->ready[BROWSER_MZM]) {
        mzm_audio_initialize();
        SDL_AudioSpec spec = {SDL_AUDIO_S16LE, 2, 65536};
        browser->mzm_stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, mzm_pull, &browser->mzm_pull);
        browser->ready[BROWSER_MZM] = browser->mzm_stream != NULL;
    }
    if (browser->aria_stream) SDL_PauseAudioStreamDevice(browser->aria_stream);
    if (browser->mzm_stream) SDL_PauseAudioStreamDevice(browser->mzm_stream);
    return browser->ready[BROWSER_ARIA] || browser->ready[BROWSER_MZM];
}

int main(int argc, char **argv) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL audio initialization failed: %s\n", SDL_GetError());
        return 1;
    }
    Browser browser = {0};
    if (!audio_init(&browser))
        fprintf(stderr, "No extracted native sound bank is available.\n");
    if (argc == 2 && !strcmp(argv[1], "--check")) {
        AosAudioSongInfo aria_default = {0};
        (void)aos_audio_song_info(MV_SPLIT_ARIA_HEARTBEAT_SOUND,
                                  &aria_default);
        printf("Aria entries: %zu; Zero Mission entries: %d; defaults: "
               "Aria 0x%03X player %u, Zero Mission 0x%03X player %d\n",
               aos_audio_song_count(), MZM_SOUND_COUNT,
               MV_SPLIT_ARIA_HEARTBEAT_SOUND,
               aria_default.player,
               MV_SPLIT_MZM_LIGHTNING_SOUND,
               mzm_audio_sound_player(MV_SPLIT_MZM_LIGHTNING_SOUND));
        if (browser.aria_stream) SDL_DestroyAudioStream(browser.aria_stream);
        if (browser.mzm_stream) SDL_DestroyAudioStream(browser.mzm_stream);
        aos_audio_set_start_sink(NULL, NULL);
        aos_mp2k_engine_destroy(&browser.aria);
        mzm_audio_close();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return browser.ready[BROWSER_ARIA] && browser.ready[BROWSER_MZM] ? 0 : 1;
    }
    browser.timer = g_timeout_add(8, audio_tick, &browser);
    GtkApplication *application = gtk_application_new(
        "com.wozt.metroidvania.sfx-browser", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application, "activate", G_CALLBACK(activate), &browser);
    if (g_getenv("FUSION_SFX_DEBUG"))
        fprintf(stderr, "[sfx-browser] starting GTK main loop\n");
    int result = g_application_run(G_APPLICATION(application), argc, argv);
    if (g_getenv("FUSION_SFX_DEBUG"))
        fprintf(stderr, "[sfx-browser] GTK loop stopped, status=%d\n", result);
    g_source_remove(browser.timer);
    if (browser.aria_stream) SDL_DestroyAudioStream(browser.aria_stream);
    if (browser.mzm_stream) SDL_DestroyAudioStream(browser.mzm_stream);
    aos_audio_set_start_sink(NULL, NULL);
    aos_mp2k_engine_destroy(&browser.aria);
    mzm_audio_close();
    g_object_unref(application);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return result;
}
