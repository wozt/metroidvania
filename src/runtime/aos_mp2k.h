/* SPDX-License-Identifier: GPL-3.0-only */
/* Host-neutral MP2K track event decoder used by Aria's native audio port. */
#ifndef AOS_MP2K_H
#define AOS_MP2K_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    AOS_MP2K_PATTERN_DEPTH = 3,
    AOS_MP2K_NOTE_LOG_CAPACITY = 32,
    AOS_MP2K_PLAYER_COUNT = 8,
    AOS_MP2K_TRACK_COUNT = 16,
    AOS_MP2K_BANK_BASE = 0x0827B2C0,
    AOS_MP2K_BANK_SIZE = 0x275638,
    /* DEFAULT_SOUND_MODE: 8 DirectSound channels (SOUND_MODE_MAXCHN). */
    AOS_MP2K_DIRECT_CHANNELS = 8,
    AOS_MP2K_CGB_CHANNELS = 4,
    /* Channel status bits (+ 0x00). */
    AOS_MP2K_CHANNEL_START = 0x80,
    AOS_MP2K_CHANNEL_STOP = 0x40,
    AOS_MP2K_CHANNEL_ACTIVE = 0xC7,
};

typedef struct {
    uint8_t key;
    uint8_t velocity;
    uint8_t gate;
    bool end_tie;   /* an EOT for `key`, logged in order with the notes */
    uint8_t voice;      /* the track's voice and priority when it was read */
    uint8_t priority;
} AosMp2kNote;

/* One ToneData record of a voice group (12 bytes). */
typedef struct {
    uint8_t type;       /* + 0x0: & 7 CGB channel, 0x40 key split, 0x80 rhythm */
    uint8_t key;        /* + 0x1: a rhythm entry's key */
    uint8_t length;     /* + 0x2 */
    uint8_t pan_sweep;  /* + 0x3: a rhythm entry's pan when bit 7 is set */
    uint32_t wav;       /* + 0x4: sample, or the sub voice group of 0x40 / 0x80 */
    uint8_t attack, decay, sustain, release;   /* + 0x8; 0x40's key table pointer */
} AosMp2kTone;

/* A sound channel as MP2K_event_nxx leaves it; envelopes, volume, pitch
 * and mixing are not ported, so a stopped channel stays stopped. */
typedef struct {
    uint8_t status;         /* + 0x00 */
    uint8_t type;           /* + 0x01: the tone's type */
    uint8_t key;            /* + 0x08: the key played (a rhythm entry's own) */
    uint8_t gate;           /* + 0x10 */
    uint8_t midi_key;       /* + 0x11: the note's key, which EOT matches */
    uint8_t velocity;       /* + 0x12 */
    uint8_t priority;       /* + 0x13 */
    int8_t rhythm_pan;      /* + 0x14 */
    uint8_t right_volume;   /* + 0x02: ChnVolSetAsm */
    uint8_t left_volume;    /* + 0x03 */
    uint32_t frequency;     /* + 0x20: MidiKeyToFreq (DirectSound only) */
    AosMp2kTone tone;
    bool linked;            /* in its track's chain (+ 0x2C, + 0x30, + 0x34) */
    uint8_t player, track;
    uint32_t serial;        /* chain order: newer channels are nearer the head */
} AosMp2kChannel;

typedef struct {
    const uint8_t *data;
    size_t size;
    uint32_t address_base;
    size_t cursor;
    size_t pattern_stack[AOS_MP2K_PATTERN_DEPTH];
    uint8_t pattern_level;
    uint8_t repeat_count;
    uint8_t wait;
    uint8_t running_status;
    uint8_t key;
    uint8_t velocity;
    uint8_t gate;
    uint8_t priority;
    uint8_t voice;
    uint8_t volume;
    int8_t pan;
    int8_t bend;
    uint8_t bend_range;
    int8_t key_shift;
    int8_t tune;
    uint8_t lfo_speed;
    uint8_t lfo_delay;
    uint8_t mod_depth;
    uint8_t mod_type;
    /* MPlayVolumeControl and the other public controls (+ 0x13, + 0x15,
     * + 0x0B, + 0x09 sources), and TrkVolPitSet's results. */
    uint8_t vol_public;         /* 0x40 at the track's start */
    int8_t pan_public;
    int8_t key_shift_public;
    uint8_t pitch_public;
    uint8_t vol_right, vol_left;    /* + 0x10, + 0x11 */
    int8_t key_shift_calculated;    /* + 0x08 */
    uint8_t pitch_calculated;       /* + 0x09 */
    uint16_t tempo_raw;
    bool tempo_changed;     /* a TEMPO event ran since the player read it */
    uint8_t end_tie_key;
    bool active;
    bool failed;
    AosMp2kNote notes[AOS_MP2K_NOTE_LOG_CAPACITY];
    size_t note_count;
    size_t end_tie_count;
} AosMp2kTrack;

typedef struct {
    bool active;
    uint32_t header_pointer;
    uint32_t voicegroup_pointer;
    uint8_t track_count;
    uint8_t block_count;
    uint8_t priority;
    uint8_t reverb;
    uint16_t tempo_raw;         /* + 0x1C */
    uint16_t tempo_interval;    /* + 0x20 */
    uint16_t tempo_scale;       /* + 0x1E */
    uint16_t tempo_counter;     /* + 0x22 */
    uint32_t clock;             /* + 0x0C */
    uint16_t track_bits;        /* + 0x04: the tracks that existed on the last clock */
    bool ended;                 /* + 0x04 = 0x80000000: no track left */
    bool failed;                /* a track met an unsupported or invalid stream */
    AosMp2kTrack tracks[AOS_MP2K_TRACK_COUNT];
} AosMp2kPlayer;

typedef struct {
    uint8_t *owned_bank;
    const uint8_t *bank;
    size_t bank_size;
    uint32_t bank_base;
    AosMp2kPlayer players[AOS_MP2K_PLAYER_COUNT];
    AosMp2kChannel direct[AOS_MP2K_DIRECT_CHANNELS];   /* SoundMixerState + 0x50 */
    AosMp2kChannel cgb[AOS_MP2K_CGB_CHANNELS];         /* cgbChans */
    uint32_t serial;
    size_t dropped_notes;   /* notes MP2K_event_nxx found no channel or tone for */
    size_t missing_samples; /* DirectSound mixes whose sample is outside the bank */
} AosMp2kEngine;

void aos_mp2k_track_start(AosMp2kTrack *track, const uint8_t *data,
                          size_t size, uint32_t address_base);
bool aos_mp2k_track_start_at(AosMp2kTrack *track, const uint8_t *data,
                             size_t size, uint32_t address_base,
                             uint32_t start_address);
/* Advance one MP2K sequencer clock. Voice allocation and mixing are separate. */
bool aos_mp2k_track_tick(AosMp2kTrack *track);
void aos_mp2k_track_clear_notes(AosMp2kTrack *track);

void aos_mp2k_engine_init(AosMp2kEngine *engine);
bool aos_mp2k_engine_set_bank(AosMp2kEngine *engine, const uint8_t *bank,
                              size_t size, uint32_t address_base);
bool aos_mp2k_engine_load_bank(AosMp2kEngine *engine, const char *path,
                               char *error, size_t error_size);
void aos_mp2k_engine_destroy(AosMp2kEngine *engine);
bool aos_mp2k_player_start(AosMp2kEngine *engine, uint8_t player,
                           uint32_t header_pointer);
const AosMp2kPlayer *aos_mp2k_player(const AosMp2kEngine *engine, uint8_t player);
/* MP2KPlayerMain's sequencer part for one frame: the tempo counter gains
 * the interval, and while it is at least 150 every existing track advances
 * one clock (a TEMPO event sets the raw tempo to twice its value and the
 * interval to raw * scale >> 8), the clock counts, and with no track left
 * the player ends; then the counter loses 150. Fades, channel gates, LFOs
 * and TrkVolPitSet are not part of this step. */
void aos_mp2k_player_frame(AosMp2kPlayer *player);
/* The same frame with the channel side of MP2K_event_nxx, MP2K_event_endtie
 * and the gate countdown: before each track's clock its channels' gates
 * count down (a gate reaching 0 stops the channel); each note resolves its
 * tone from the voice group (key split 0x40 through its key table, rhythm
 * 0x80 with the entry's key and pan; a nested split or rhythm is dropped),
 * takes priority min(255, player + track) and a channel: the tone's CGB
 * channel when free, stopping, lower in priority or equal from a track not
 * before it, else the first free DirectSound channel, else the best
 * stopping one, else the lowest active one below or equal to it; an EOT
 * stops the newest active channel of the track with its key. Players are
 * ordered as their indices for the track-address comparisons. */
void aos_mp2k_engine_player_frame(AosMp2kEngine *engine, uint8_t player);
/* TrkVolPitSet with no LFO (modCalculated 0): the volume split by pan and
 * the key shift and pitch from bend, range, tune and the public controls. */
void aos_mp2k_track_vol_pit_set(AosMp2kTrack *track);
/* MidiKeyToFreq: the sample's rate for `key` (clamped at 178) and the fine
 * adjustment `fine` (256ths of a semitone) through gScaleTable and
 * gFreqTable. */
uint32_t aos_mp2k_key_to_freq(uint32_t wav_freq, uint8_t key, uint8_t fine);
/* A voice group's entry `index` (VOICE copies it into the track). */
bool aos_mp2k_tone(const AosMp2kEngine *engine, uint32_t voicegroup, unsigned index,
                   AosMp2kTone *tone);
/* m4aSoundMain's walk over the players: one frame of each active one. */
void aos_mp2k_engine_frame(AosMp2kEngine *engine);

#endif /* AOS_MP2K_H */
