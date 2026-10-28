/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-neutral controller for Aria's PlaySong and audio commands. */
#ifndef AOS_AUDIO_H
#define AOS_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
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

enum {
    AOS_AUDIO_LOG_CAPACITY = 64,
    AOS_AUDIO_PLAYER_COUNT = 8,
    AOS_AUDIO_MAX_SONGS = 0x400,
    AOS_AUDIO_MUSIC_COUNT = 46
};

enum {
    AOS_AUDIO_STOP = 0x1000,
    AOS_AUDIO_PAUSE = 0x1002,
    AOS_AUDIO_RESUME = 0x1003,
    AOS_AUDIO_6BIT = 0x1006,
    AOS_AUDIO_7BIT = 0x1007,
    AOS_AUDIO_8BIT = 0x1008,
    AOS_AUDIO_9BIT = 0x1009,
    AOS_AUDIO_SONG_MUTE = 0x1010,
    AOS_AUDIO_SONG_LOUD = 0x1011,
    AOS_AUDIO_SONG_QUIET = 0x1012,
    AOS_AUDIO_FREQ_BASE = 0x1020,
    AOS_AUDIO_PLAYER_STOP_BASE = 0x1030,
    AOS_AUDIO_PLAYER_RESUME_BASE = 0x1038,
    AOS_AUDIO_PLAYER_MUTE_BASE = 0x1040
};

typedef struct {
    uint8_t player;
    uint8_t priority;
    uint32_t header_pointer;
} AosAudioSongInfo;

typedef void (*AosAudioStartSink)(void *context, uint16_t song,
                                  const AosAudioSongInfo *info);

typedef struct {
    bool active;
    bool paused;
    uint16_t song;
    uint8_t priority;
    uint16_t volume_step;
    uint16_t volume_updated;
    uint16_t volume_coarse;
    uint16_t volume_target;
    uint16_t volume_current;
    bool volume_up;
} AosAudioPlayerState;

typedef struct {
    bool paused;
    int16_t current_song;
    uint32_t sound_mode;
    AosAudioPlayerState players[AOS_AUDIO_PLAYER_COUNT];
} AosAudioState;

void aos_audio_set_sink(AosAudioSink sink, void *context);
/* Observe songs only after StartSong accepts the player-priority replacement. */
void aos_audio_set_start_sink(AosAudioStartSink sink, void *context);
/* Install the gSongTable fields needed by StartSong. The data is copied. */
bool aos_audio_set_song_table(const AosAudioSongInfo *songs, size_t count);
/* Load the private song-table manifest exported from the verified Aria ROM. */
bool aos_audio_load_song_table(const char *path, char *error, size_t error_size);
size_t aos_audio_song_count(void);
bool aos_audio_song_info(size_t song, AosAudioSongInfo *info);
/* Reset the host controller and remove its song table, retaining the sink. */
void aos_audio_reset(void);
/* PlaySong: route a song ID, STOP_SONG request or AudioCommand value. */
void aos_audio_play_song(uint16_t song);
/* MPlayVolumeUpdate, normally called once per VBlank by AudioVSync. */
void aos_audio_volume_update(void);
const AosAudioState *aos_audio_state(void);

void aos_audio_clear(void);
int aos_audio_count(void);
AosAudioRequest aos_audio_get(int index);
/* How many logged requests match (0 when none). */
int aos_audio_requested(AosAudioAction action, uint16_t value);

#endif /* AOS_AUDIO_H */
