/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission sound engine (no SDL): src/audio.c, src/audio_wrappers.c,
 * the sound parts of src/music_wrappers.c and src/room_music.c, and the
 * assembly of asm/audio_internal.s (InitTrack, StopMusicOrSound, ResetTrack,
 * UpdateAudio, the note, pitch and volume helpers, Goto and PatternPlay,
 * UploadSampleToWaveRam, the PSG register writes) and asm/soundcode.s (the
 * DirectSound mixer SoundCodeA/B/C, GetNoteFrequency, the DMA 2 interrupt).
 * ROM data (songs, voice groups, samples) is read through GBA addresses from
 * the sound bank of scripts/mzm_audio.py; the engine tables come from its
 * sound_tables.tsv. The GBA sound hardware (DMA to the FIFOs at the timer 0
 * rate, the PSG channels, the output mixer) is emulated by mzm_audio_render
 * and gba_psg. MidiKey2Freq (a BIOS call) uses the usual high-level
 * formula. */
#ifndef MZM_AUDIO_H
#define MZM_AUDIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Load the sound bank and tables; false (with a message) when missing. */
bool mzm_audio_open(const char *bank_path, const char *tables_path);
void mzm_audio_close(void);
bool mzm_audio_ready(void);
/* InitializeAudio */
void mzm_audio_initialize(void);
/* UpdateAudio, once per frame (the VBlank callback). */
void mzm_audio_update(void);
/* One frame of the sound hardware (224 samples at 13379 Hz): stereo
 * int16 frames at `rate` Hz written to `out` (at most `max_frames`);
 * returns the number written. */
int mzm_audio_render(int16_t *out, int max_frames, int rate);

/* SoundPlay, SoundPlayNotAlreadyPlaying, SoundStop, SoundFade */
void mzm_sound_play(uint16_t sound);
void mzm_sound_play_not_already_playing(uint16_t sound);
void mzm_sound_stop(uint16_t sound);
void mzm_sound_fade(uint16_t sound, uint16_t timer);
/* PlayMusic, FadeMusic, StopAllMusicAndSounds */
void mzm_music_play(uint16_t music, uint8_t priority);
void mzm_music_fade(uint16_t timer);
void mzm_audio_stop_all(void);
/* UpdateMusicPriority */
void mzm_music_update_priority(uint8_t priority);
/* InsertMusicAndQueueCurrent: the current music is replayed when `music`
 * ends (0 for the item jingles, 1 for the loading jingle). */
void mzm_music_insert(uint16_t music, uint8_t not_interrupting);
/* BackupTrackData2SoundChannels and RetrieveTrackData2SoundChannels */
void mzm_audio_backup_track2(void);
void mzm_audio_retrieve_track2(void);
/* CheckSetNewMusicTrack (a door transition starts: the room's track) and
 * CheckPlayTransitionMusicTrack (the new room is shown). */
void mzm_music_check_set_new_track(uint16_t music);
void mzm_music_check_play_transition_track(void);
/* gMusicInfo.musicTrack */
uint16_t mzm_music_current(void);

/* For tests: the number of active DirectSound channels and whether a
 * music player (sMusicTrackDataRom index) is playing. */
int mzm_audio_active_channels(void);
bool mzm_audio_player_playing(int player);

#endif /* MZM_AUDIO_H */
