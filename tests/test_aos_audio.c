/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_audio.h"

typedef struct {
    int calls;
    AosAudioAction action;
    uint16_t value;
} SinkState;

typedef struct {
    int calls;
    uint16_t song;
    AosAudioSongInfo info;
} StartSinkState;

static void sink(void *context, AosAudioAction action, uint16_t value) {
    SinkState *state = context;
    state->calls++;
    state->action = action;
    state->value = value;
}

static void start_sink(void *context, uint16_t song, const AosAudioSongInfo *info) {
    StartSinkState *state = context;
    state->calls++;
    state->song = song;
    state->info = *info;
}

static void routing_tests(void) {
    SinkState state = {0};
    aos_audio_reset();
    aos_audio_set_sink(sink, &state);

    aos_audio_play_song(0xE3);
    aos_audio_play_song(0x806E);
    aos_audio_play_song(0x1005);

    assert(aos_audio_count() == 3);
    assert(aos_audio_get(0).action == AOS_AUDIO_START_SONG && aos_audio_get(0).value == 0xE3);
    assert(aos_audio_get(1).action == AOS_AUDIO_STOP_SONG && aos_audio_get(1).value == 0x6E);
    assert(aos_audio_get(2).action == AOS_AUDIO_COMMAND && aos_audio_get(2).value == 0x1005);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xE3));
    assert(state.calls == 3 && state.action == AOS_AUDIO_COMMAND && state.value == 0x1005);

    aos_audio_set_sink(NULL, NULL);
}

static void controller_tests(void) {
    AosAudioSongInfo songs[0xE4] = {0};
    songs[0] = (AosAudioSongInfo){3, 1, 0};
    songs[1] = (AosAudioSongInfo){0, 10, 0};
    songs[2] = (AosAudioSongInfo){0, 5, 0};
    songs[3] = (AosAudioSongInfo){0, 12, 0};
    songs[0x6E] = (AosAudioSongInfo){1, 20, 0};
    songs[0xE3] = (AosAudioSongInfo){2, 30, 0};

    aos_audio_reset();
    assert(!aos_audio_set_song_table(NULL, 1));
    assert(aos_audio_set_song_table(songs, sizeof songs / sizeof songs[0]));

    /* StartSong records the current music before its priority check. */
    aos_audio_play_song(1);
    const AosAudioState *state = aos_audio_state();
    assert(state->current_song == 1 && state->players[0].active);
    assert(!state->players[0].paused && state->players[0].song == 1);
    assert(state->players[0].priority == 10 && state->players[0].volume_current == 0xFF0);
    aos_audio_play_song(2);
    assert(state->current_song == 2 && state->players[0].song == 1);
    aos_audio_play_song(3);
    assert(state->current_song == 3 && state->players[0].song == 3);

    /* A paused player accepts a lower-priority replacement. */
    aos_audio_play_song(AOS_AUDIO_PAUSE);
    assert(state->paused && state->players[0].paused);
    aos_audio_play_song(2);
    assert(state->players[0].song == 2 && !state->players[0].paused);
    aos_audio_play_song(0x8000 | 2);
    assert(state->current_song == -1 && state->players[0].paused);
    aos_audio_play_song(AOS_AUDIO_RESUME);
    assert(!state->paused && !state->players[0].paused);

    /* AudioCommand controls resolution, frequency and the eight players. */
    aos_audio_play_song(AOS_AUDIO_6BIT);
    assert((state->sound_mode & 0x00B00000) == 0x00B00000);
    aos_audio_play_song(AOS_AUDIO_9BIT);
    assert((state->sound_mode & 0x00B00000) == 0x00800000);
    aos_audio_play_song(AOS_AUDIO_FREQ_BASE + 11);
    assert((state->sound_mode & 0x000F0000) == 0x000C0000);
    aos_audio_play_song(0x6E);
    assert(state->players[1].song == 0x6E && !state->players[1].paused);
    aos_audio_play_song(AOS_AUDIO_PLAYER_STOP_BASE + 1);
    assert(state->players[1].paused);
    aos_audio_play_song(AOS_AUDIO_PLAYER_RESUME_BASE + 1);
    assert(!state->players[1].paused);
    aos_audio_play_song(AOS_AUDIO_PLAYER_MUTE_BASE + 1);
    assert(state->players[1].volume_step == 0x19 && state->players[1].volume_target == 0);

    /* The native ramp stops a player after reaching a mute target. */
    for (int i = 0; i < 200 && state->players[1].volume_step; ++i)
        aos_audio_volume_update();
    assert(state->players[1].volume_current == 0 && state->players[1].paused);

    aos_audio_play_song(1);
    aos_audio_play_song(AOS_AUDIO_SONG_QUIET);
    assert(state->players[0].volume_step == 0x3C && state->players[0].volume_target == 0x900);
    for (int i = 0; i < 100 && state->players[0].volume_step; ++i)
        aos_audio_volume_update();
    assert(state->players[0].volume_current == 0x900 && !state->players[0].paused);

    aos_audio_play_song(AOS_AUDIO_STOP);
    assert(state->current_song == -1 && !state->paused);
    for (int i = 0; i < AOS_AUDIO_PLAYER_COUNT; ++i) {
        assert(state->players[i].paused);
        assert(state->players[i].volume_step == 0);
        assert(state->players[i].volume_current == 0xFF0);
    }
}

static void accepted_start_sink_test(void) {
    AosAudioSongInfo songs[4] = {
        {0, 0, 0}, {0, 10, 0x08300010},
        {0, 5, 0x08300020}, {0, 12, 0x08300030},
    };
    StartSinkState observed = {0};
    aos_audio_reset();
    aos_audio_set_start_sink(start_sink, &observed);
    assert(aos_audio_set_song_table(songs, 4));
    aos_audio_play_song(1);
    assert(observed.calls == 1 && observed.song == 1);
    assert(observed.info.player == 0 && observed.info.header_pointer == 0x08300010);
    aos_audio_play_song(2);
    assert(observed.calls == 1);
    aos_audio_play_song(3);
    assert(observed.calls == 2 && observed.song == 3);
    assert(observed.info.priority == 12 && observed.info.header_pointer == 0x08300030);
    aos_audio_set_start_sink(NULL, NULL);
}

static void loader_tests(void) {
    char error[128];
    aos_audio_reset();
    assert(aos_audio_load_song_table(FUSION_SOURCE_DIR "/tests/fixtures/aos_song_table.tsv",
                                     error, sizeof error));
    assert(aos_audio_song_count() == 2);
    AosAudioSongInfo info;
    assert(aos_audio_song_info(1, &info));
    assert(info.player == 0 && info.priority == 10 && info.header_pointer == 0x08000200);
    assert(!aos_audio_song_info(2, &info));
    aos_audio_play_song(1);
    const AosAudioState *state = aos_audio_state();
    assert(state->players[0].active && state->players[0].song == 1);
    assert(state->players[0].priority == 10);
    assert(!aos_audio_load_song_table(FUSION_SOURCE_DIR "/tests/fixtures/missing.tsv",
                                      error, sizeof error));
    assert(aos_audio_song_count() == 2);
}

int main(void) {
    routing_tests();
    controller_tests();
    accepted_start_sink_test();
    loader_tests();
    puts("aos audio tests passed");
    return 0;
}
