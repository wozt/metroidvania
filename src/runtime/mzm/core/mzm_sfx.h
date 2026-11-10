/* SPDX-License-Identifier: GPL-3.0-only */
/* The SoundPlay, SoundPlayNotAlreadyPlaying, SoundStop and SoundFade calls
 * of the gameplay modules. The modules stay independent of the sound engine:
 * the runtime installs a sink that forwards the requests to mzm_audio, and
 * every request is also recorded (the last MZM_SFX_LOG of them) so tests can
 * check which sounds a frame asked for. */
#ifndef MZM_SFX_H
#define MZM_SFX_H

#include <stdint.h>

#include "mzm_sound_ids.h"

typedef enum {
    MZM_SFX_PLAY, MZM_SFX_PLAY_NOT_PLAYING, MZM_SFX_STOP, MZM_SFX_FADE, MZM_SFX_STOP_ALL,
    MZM_SFX_MUSIC_PLAY, MZM_SFX_MUSIC_INSERT, MZM_SFX_BACKUP_TRACK2, MZM_SFX_RETRIEVE_TRACK2,
    MZM_SFX_MUSIC_FADE_AND_QUEUE, MZM_SFX_MUSIC_FADE, MZM_SFX_MUSIC_PRIORITY, MZM_SFX_FADE_ALL,
    MZM_SFX_MUSIC_AFTER_ALARM
} MzmSfxAction;
typedef struct { MzmSfxAction action; uint16_t sound, timer; } MzmSfxRequest;
typedef void (*MzmSfxSink)(void *context, MzmSfxAction action, uint16_t sound, uint16_t timer);

enum { MZM_SFX_LOG = 1024 };

void mzm_sfx_set_sink(MzmSfxSink sink, void *context);
/* SoundPlay */
void mzm_sfx_play(uint16_t sound);
/* SoundPlayNotAlreadyPlaying */
void mzm_sfx_play_not_playing(uint16_t sound);
/* SoundStop */
void mzm_sfx_stop(uint16_t sound);
/* SoundFade */
void mzm_sfx_fade(uint16_t sound, uint16_t timer);
/* StopAllMusicAndSounds */
void mzm_sfx_stop_all(void);
/* FadeAllSounds */
void mzm_sfx_fade_all(uint16_t timer);
/* PlayMusic (the priority travels in the timer field) */
void mzm_sfx_music_play(uint16_t music, uint16_t priority);
/* InsertMusicAndQueueCurrent (isNotInterrupting in the timer field) */
void mzm_sfx_music_insert(uint16_t music, uint16_t not_interrupting);
/* UpdateMusicPriority (the priority in the timer field) */
void mzm_sfx_music_priority(uint8_t priority);
/* FadeMusic */
void mzm_sfx_music_fade(uint16_t timer);
/* FadeCurrentMusicAndQueueNextMusic with priority 0 */
void mzm_sfx_music_fade_and_queue(uint16_t timer, uint16_t music);
/* UpdateMusicAfterAlarmDisable */
void mzm_sfx_music_after_alarm_disable(void);
/* BackupTrackData2SoundChannels */
void mzm_sfx_backup_track2(void);
/* RetrieveTrackData2SoundChannels */
void mzm_sfx_retrieve_track2(void);

/* The recorded requests since the last clear (oldest first). */
void mzm_sfx_clear(void);
int mzm_sfx_count(void);
MzmSfxRequest mzm_sfx_get(int index);
/* Whether `sound` was requested with `action` since the last clear. */
int mzm_sfx_requested(MzmSfxAction action, uint16_t sound);

#endif /* MZM_SFX_H */
