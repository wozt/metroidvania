/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_audio.h"

#include <stddef.h>

enum { AOS_STOP_SONG_FLAG = 0x8000 };

static AosAudioSink g_sink;
static void *g_context;
static AosAudioRequest g_log[AOS_AUDIO_LOG_CAPACITY];
static int g_count;

void aos_audio_set_sink(AosAudioSink sink, void *context) {
    g_sink = sink;
    g_context = context;
}

static void request(AosAudioAction action, uint16_t value) {
    if (g_count < AOS_AUDIO_LOG_CAPACITY)
        g_log[g_count++] = (AosAudioRequest){action, value};
    if (g_sink) g_sink(g_context, action, value);
}

void aos_audio_play_song(uint16_t song) {
    /* PlaySong starts table entries below 0x400. Higher values either stop
     * one masked song or pass through to AudioCommand unchanged. */
    if (song < 0x400)
        request(AOS_AUDIO_START_SONG, song);
    else if ((song & AOS_STOP_SONG_FLAG) != 0)
        request(AOS_AUDIO_STOP_SONG, song & 0x3FF);
    else
        request(AOS_AUDIO_COMMAND, song);
}

void aos_audio_clear(void) { g_count = 0; }
int aos_audio_count(void) { return g_count; }

AosAudioRequest aos_audio_get(int index) {
    return index >= 0 && index < g_count
               ? g_log[index]
               : (AosAudioRequest){AOS_AUDIO_START_SONG, 0};
}

int aos_audio_requested(AosAudioAction action, uint16_t value) {
    for (int i = 0; i < g_count; ++i)
        if (g_log[i].action == action && g_log[i].value == value) return 1;
    return 0;
}
