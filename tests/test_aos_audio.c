/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_audio.h"

typedef struct {
    int calls;
    AosAudioAction action;
    uint16_t value;
} SinkState;

static void sink(void *context, AosAudioAction action, uint16_t value) {
    SinkState *state = context;
    state->calls++;
    state->action = action;
    state->value = value;
}

int main(void) {
    SinkState state = {0};
    aos_audio_clear();
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
    puts("aos audio tests passed");
    return 0;
}
