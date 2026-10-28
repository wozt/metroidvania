/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-neutral request boundary for Aria's PlaySong routine. */
#ifndef AOS_AUDIO_H
#define AOS_AUDIO_H

#include <stdint.h>

typedef enum {
    AOS_AUDIO_START_SONG,
    AOS_AUDIO_STOP_SONG,
    AOS_AUDIO_COMMAND
} AosAudioAction;

typedef struct {
    AosAudioAction action;
    uint16_t value;
} AosAudioRequest;

typedef void (*AosAudioSink)(void *context, AosAudioAction action, uint16_t value);

enum { AOS_AUDIO_LOG_CAPACITY = 64 };

void aos_audio_set_sink(AosAudioSink sink, void *context);
/* PlaySong: route a song ID, STOP_SONG request or AudioCommand value. */
void aos_audio_play_song(uint16_t song);

void aos_audio_clear(void);
int aos_audio_count(void);
AosAudioRequest aos_audio_get(int index);
int aos_audio_requested(AosAudioAction action, uint16_t value);

#endif /* AOS_AUDIO_H */
