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
    MZM_SFX_PLAY, MZM_SFX_PLAY_NOT_PLAYING, MZM_SFX_STOP, MZM_SFX_FADE, MZM_SFX_STOP_ALL
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

/* The recorded requests since the last clear (oldest first). */
void mzm_sfx_clear(void);
int mzm_sfx_count(void);
MzmSfxRequest mzm_sfx_get(int index);
/* Whether `sound` was requested with `action` since the last clear. */
int mzm_sfx_requested(MzmSfxAction action, uint16_t sound);

#endif /* MZM_SFX_H */
