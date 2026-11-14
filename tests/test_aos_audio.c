/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/core/aos_audio.h"

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
    /* The portable MP2K player reaching FINE clears the native active
     * status, so a later lower-priority sound can use the same player. */
    aos_audio_player_finished(0);
    assert(!aos_audio_state()->players[0].active);
    aos_audio_play_song(2);
    assert(observed.calls == 2 && observed.song == 2);
    aos_audio_play_song(3);
    assert(observed.calls == 3 && observed.song == 3);
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

static void ambient_tests(void) {
    /* +0x254CC lifecycle: sub_08013EEC plants the ambient song after
     * resetting the tracked current song to MUS_DUMMY, sub_08013F38
     * stops it on transitions, and sub_08013D30 releases held sounds. */
    AosAudioSongInfo songs[64] = {0};
    songs[0] = (AosAudioSongInfo){0, 0, 0};
    songs[2] = (AosAudioSongInfo){0, 50, 0};
    songs[38] = (AosAudioSongInfo){0, 100, 0};
    songs[0x9C] = (AosAudioSongInfo){4, 50, 0};

    aos_audio_reset();
    assert(aos_audio_set_song_table(songs, 64));
    assert(aos_audio_ambient() == 0);

    /* sub_08013EEC(38): stop the tracked song, MUS_DUMMY takes its
     * player, then the ambient song plays and the slot records it. */
    aos_audio_play_song(2);
    assert(aos_audio_state()->current_song == 2);
    aos_audio_clear();
    aos_audio_ambient_set(38);
    assert(aos_audio_ambient() == 38);
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 2) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 38) == 1);
    assert(aos_audio_state()->players[0].song == 38 &&
           !aos_audio_state()->players[0].paused);
    /* The dummy reset leaves the tracker on song 0's slot only until
     * the ambient start rewrites it to the ambient song. */
    assert(aos_audio_state()->current_song == 38);

    /* With nothing tracked, sub_08013EEC skips the dummy reset. */
    aos_audio_reset();
    assert(aos_audio_set_song_table(songs, 64));
    aos_audio_clear();
    aos_audio_ambient_set(38);
    assert(!aos_audio_requested(AOS_AUDIO_START_SONG, 0));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 38) == 1);

    /* sub_08013F38: the transition teardown stops the ambient song
     * and clears the slot; a second call is a no-op. */
    aos_audio_clear();
    aos_audio_ambient_stop();
    assert(aos_audio_ambient() == 0);
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 38) == 1);
    assert(aos_audio_state()->players[0].paused);
    aos_audio_clear();
    aos_audio_ambient_stop();
    assert(aos_audio_count() == 0);

    /* The raw +0x254CC word store records without playing. */
    aos_audio_clear();
    aos_audio_ambient_store(7);
    assert(aos_audio_ambient() == 7 && aos_audio_count() == 0);
    aos_audio_ambient_stop();
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 7) == 1);

    /* sub_08013D30 releases every held sound, index 3 down to 0. */
    aos_audio_reset();
    assert(aos_audio_set_song_table(songs, 64));
    assert(!aos_audio_hold(0x9C));
    assert(!aos_audio_hold(0x75));
    assert(aos_audio_hold(0x9C));   /* already listed */
    assert(aos_audio_held(3) == 0x9C && aos_audio_held(2) == 0x75);
    aos_audio_clear();
    aos_audio_release_held();
    assert(aos_audio_held(0) == 0 && aos_audio_held(2) == 0 &&
           aos_audio_held(3) == 0);
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 0x9C) == 1);
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 0x75) == 1);
}

int main(void) {
    routing_tests();
    controller_tests();
    accepted_start_sink_test();
    loader_tests();
    ambient_tests();
    puts("aos audio tests passed");
    return 0;
}
