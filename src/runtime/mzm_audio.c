/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission sound engine; comments name the pinned decompilation's
 * routines (see mzm_audio.h). ROM data stays as 32-bit GBA addresses in the
 * engine structures, as the native code relies on their bits; RAM
 * structures are host structures laid out in the native RAM order where the
 * code compares their addresses. */
#include "mzm_audio.h"
#include "gba_psg.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Native structures (include/structs/audio.h) ---- */

typedef struct { uint8_t attack, decay, sustain, release; } Envelope;

struct TrackVariables;

typedef struct SoundChannel {
    uint8_t unk_0, unk_1, unk_2, unk_3, unk_4, unk_5, unk_6, unk_7;
    Envelope envelope;
    uint8_t unk_C, unk_D, unk_E, unk_F, unk_10, unk_11, unk_12, unk_13;
    uint32_t pSize, unk_18, unk_1C, pSample, pData;     /* pSize, pSample, pData: ROM */
    struct TrackVariables *pVariables;
    struct SoundChannel *pChannel1, *pChannel2;
} SoundChannel;

typedef struct PsgSoundData {
    uint8_t unk_0, unk_1, unk_2, unk_3;
    Envelope envelope;
    uint16_t maybe_noteDelay;
    uint8_t unk_A, unk_B, unk_C, unk_D, unk_E, unk_F, unk_10, unk_11, unk_12, undefined_13;
    uint16_t unk_14;
    uint8_t unk_16, unk_17, unk_18;
    int8_t unk_19;
    uint8_t unk_1A, unk_1B, unk_1C, unk_1D, unk_1E, unk_1F;
    uint32_t pSample;               /* a ROM address or a duty / noise value */
    struct TrackVariables *pVariables;
} PsgSoundData;

typedef struct TrackVariables {
    uint8_t unk_0, unk_1, delay, unk_3, volume, unk_5, unk_6, unk_7, unk_8, unk_9;
    uint8_t repeatCount, priority, unk_C, unk_D, unk_E, unk_F;
    uint8_t lfoSpeed, modulationDepth, modulationType;
    int8_t unk_13;
    uint8_t unk_14, unk_15;
    int8_t unk_16, unk_17;
    uint8_t unk_18;
    int8_t pitchBend;
    uint8_t bendRange, unk_1B, keyShift, unk_1D, tune, unk_1F;
    uint32_t unk_20;
    uint32_t pRawData;              /* ROM */
    uint32_t patternStartPointers[3];
    uint8_t channel, unk_35, unk_36, unk_37;
    uint32_t pSample1;              /* a ROM address or a duty / noise value */
    Envelope envelope1;
    uint32_t pSample2;              /* ROM: drum kit or key split voices */
    uint32_t envelope2;             /* the key split table address (ROM) */
    SoundChannel *pChannel;
    PsgSoundData *pSoundPsg;
} TrackVariables;

typedef struct TrackData {
    uint8_t flags, amountOfTracks, undefined_2, unk_3, currentTrack, maxAmountOfTracks;
    uint16_t maybe_volume, fadingTimer, unk_A, unk_C, unk_E;
    uint32_t pHeader, pVoice;       /* ROM */
    TrackVariables *pVariables;
    uint8_t occupied, unk_1D, unk_1E, priority;
    uint16_t musicTrack;
    uint8_t maxSoundChannels, unk_23;
    uint16_t unk_24, unk_26;
    uint8_t queueFlags;
} TrackData;

typedef struct { TrackData *pTrack; TrackVariables *pVariables; uint16_t maxAmountOfTracks, unk_A; }
    TrackGroupRomData;

typedef struct { uint8_t exists, priority; uint16_t fadingTimer; uint32_t pHeader; } SoundQueue;

enum { PCM_DMA_BUF_SIZE = 1536, NUM_MUSIC_PLAYERS = 9, MAX_SOUND_CHANNELS = 11 };

typedef struct {
    uint8_t unk_0, occupied, unk_2, unk_3, reverb, maxSoundChannels, volume, freqIndex;
    uint8_t unk_8, unk_9, currentSoundChannel, volumeDownFlag, unk_C, unk_D, unk_E, unk_F;
    uint8_t unk_10, unk_11;
    uint16_t sampleRate;
    uint32_t unk_14, pitch;
    uint16_t musicTrack, unk_1E;
    uint8_t unk_20, priority;
    uint16_t musicTrackOnTransition;
    uint32_t musicRawData[768];
    uint8_t soundRawData[PCM_DMA_BUF_SIZE * 2];
    SoundChannel soundChannels[MAX_SOUND_CHANNELS];
} MusicInfo;

/* ---- Engine state (the native globals) ---- */

static MusicInfo gMusicInfo;
static TrackData gTrackData[NUM_MUSIC_PLAYERS];
/* gTrack0Variables..gTrack8Variables are contiguous in RAM, in this order. */
static TrackVariables gTrackVariables[12 + 10 + 2 + 2 + 2 + 2 + 3 + 1 + 6];
/* gUnk_300376C directly precedes gPsgSounds: index 1 is gPsgSounds[0]. */
static PsgSoundData gPsgSoundsArea[5];
#define gPsgSounds (gPsgSoundsArea + 1)
#define gUnk_300376C gPsgSoundsArea
static SoundQueue gSoundQueue[NUM_MUSIC_PLAYERS];
static TrackGroupRomData sMusicTrackDataRom[NUM_MUSIC_PLAYERS];

/* gNumMusicPlayers, gUnk_Audio0x64 and gUnk_Audio0x194F700 (link-time
 * constants of the decompilation). */
enum { UNK_AUDIO_0X64 = 0x64 };
#define SOUND_ACTION_STARTUP 0x0194F700u

/* ---- ROM data and tables ---- */

static uint8_t *g_bank;
static uint32_t g_bank_base, g_bank_size;
static uint32_t g_entries, g_entry_count;
static struct {
    /* With the ROM halfword past the table (GetNoteDelay reads it). */
    uint16_t sUnk_808cad0[129];
    uint8_t sClockTable[49], sNoiseTable[60], sCgb3Vol[16], sUnk_808cc4d[19];
    uint32_t sNativeSampleRate[13], sMusicPitchData[13];
    uint16_t sPcmSamplesPerVBlankTable[13];
    uint8_t sArray_808cce2[512], sArray_808cee2[10];
    /* The ROM bytes of sUnk_808cc4d from index -64 to 64 (unk_1ccc). */
    uint8_t sUnk_808cc4d_window[129];
} T;

static uint8_t rd8(uint32_t address) {
    uint32_t offset = address - g_bank_base;
    return g_bank && offset < g_bank_size ? g_bank[offset] : 0;
}

static uint16_t rd16(uint32_t address) {
    return (uint16_t)(rd8(address) | rd8(address + 1) << 8);
}

static uint32_t rd32(uint32_t address) {
    return (uint32_t)rd16(address) | (uint32_t)rd16(address + 2) << 16;
}

static int8_t rds8(uint32_t address) {
    return (int8_t)rd8(address);
}

static Envelope envelope_at(uint32_t address) {
    return (Envelope){rd8(address), rd8(address + 1), rd8(address + 2), rd8(address + 3)};
}

/* struct SoundEntry */
static uint32_t entry_header(uint16_t sound) {
    return sound < g_entry_count ? rd32(g_entries + sound * 8u) : 0;
}

static uint16_t entry_group(uint16_t sound) {
    uint16_t group = sound < g_entry_count ? rd16(g_entries + sound * 8u + 4) : 0;
    return group < NUM_MUSIC_PLAYERS ? group : 0;
}

/* ---- Sound hardware ---- */

static GbaPsg g_psg;
/* The FIFO block being played and its position (16 samples per DMA). */
static int g_fifo_sample;
static int g_fifo_block = -1;
static double g_output_phase;
/* The FIFO samples the DACs hold between output samples, kept across
 * frames, and the PSG cycles not yet stepped. */
static int8_t g_fifo_a, g_fifo_b;
static double g_psg_phase;

enum { REG_SOUNDBIAS = 0x04000088 };

static void reg_write8(uint32_t address, uint8_t value) { gba_psg_write8(&g_psg, address, value); }
static void reg_write16(uint32_t address, uint16_t value) { gba_psg_write16(&g_psg, address, value); }
static uint8_t reg_read8(uint32_t address) { return gba_psg_read8(&g_psg, address); }
static uint16_t reg_read16(uint32_t address) { return gba_psg_read16(&g_psg, address); }

/* REG_VCOUNT while UpdateAudio runs (in VBlank). */
static uint8_t read_vcount(void) {
    return 160;
}

/* MidiKey2Freq (BIOS): the sample rate of a key and fine adjustment. */
static uint32_t MidiKey2Freq(uint32_t wave, uint8_t key, uint8_t fine) {
    uint32_t frequency = rd32(wave + 4);
    return (uint32_t)(frequency / exp2f((180.f - key - fine / 256.f) / 12.f));
}

/* GetNoteFrequency (asm/soundcode.s) */
static int32_t CallGetNoteFrequency(int32_t frequency, uint32_t pitch) {
    return (int32_t)(((uint64_t)(uint32_t)frequency * pitch) >> 18);
}

/* ---- Forward declarations ---- */

static void DoSoundAction(uint32_t action);
static void SetupSoundTransfer(void);
static void UpdateTrack(TrackData *pTrack);
static void UpdatePsgSounds(void);
static void UpdateMusic(void);
static void ResetTrack(TrackData *pTrack);
static void ClearRegistersForPsg(PsgSoundData *pSound, uint8_t channel);
static void PlayMusic(uint16_t musicTrack, uint8_t priority);
static void QueueSound(uint16_t sound, uint16_t timer);
static void ReplayQueuedMusic(uint8_t queueFlags);
static void unk_2d2c(TrackData *pTrack);
static void unk_2e6c(TrackData *pTrack);
static void unk_2a38(TrackData *pTrack);
static void unk_20a4(SoundChannel *pChannel);

/* ---- asm/audio_internal.s ---- */

/* InitTrack: 1 when a higher priority sound keeps the player. */
static uint8_t InitTrack(TrackData *pTrack, uint32_t pHeader) {
    if (pTrack->occupied) return 0;
    pTrack->occupied = 1;
    if (!(pTrack->unk_1E & 1)) {
        uint32_t word = rd32(pHeader);
        uint8_t tracks = (uint8_t)word;
        if (tracks == 0) {
            ResetTrack(pTrack);
            pTrack->flags = 0;
        } else {
            if (pTrack->flags & 2) {
                if (pTrack->unk_1D != 0 && pTrack->unk_3 > (uint8_t)(word >> 16)) {
                    pTrack->occupied = 0;
                    return 1;
                }
                ResetTrack(pTrack);
            }
            /* str (word << 8) | 2 at flags. */
            pTrack->flags = 2;
            pTrack->amountOfTracks = tracks;
            pTrack->undefined_2 = (uint8_t)(word >> 8);
            pTrack->unk_3 = (uint8_t)(word >> 16);
            pTrack->pVoice = rd32(pHeader + 4);
            pTrack->pHeader = pHeader;
            pTrack->unk_C = 0x100;
            if (word >> 31) DoSoundAction(word >> 24);
            TrackVariables *v = pTrack->pVariables;
            memset(v, 0, sizeof *v * tracks);
            for (int i = 0; i < tracks; ++i) {
                v[i].unk_0 = 1;
                v[i].bendRange = 2;
                v[i].unk_1B = 0xC;
                v[i].volume = 0;
                v[i].unk_5 = 0x40;
                v[i].unk_6 = 0x40;
                v[i].unk_7 = 0;
                v[i].pRawData = rd32(pHeader + 8 + 4u * i);
            }
        }
    }
    pTrack->occupied = 0;
    return 0;
}

/* The channel teardown shared by StopMusicOrSound and ResetTrack. */
static void release_tracks(TrackData *pTrack) {
    pTrack->flags = 1;
    pTrack->currentTrack = 0;
    TrackVariables *v = pTrack->pVariables;
    for (int n = pTrack->amountOfTracks; n > 0; --n, ++v) {
        PsgSoundData *psg = v->pSoundPsg;
        if (psg) {
            ClearRegistersForPsg(psg, (uint8_t)((v->channel & 7) - 1));
            psg->unk_0 = 0;
            psg->pVariables = NULL;
        }
        for (SoundChannel *c = v->pChannel; c;) {
            SoundChannel *next = c->pChannel2;
            c->unk_0 = 0;
            c->pVariables = NULL;
            c->pChannel2 = NULL;
            c->pChannel1 = NULL;
            c = next;
        }
        v->pSoundPsg = NULL;
        v->pChannel = NULL;
    }
}

/* StopMusicOrSound */
static void StopMusicOrSound(TrackData *pTrack) {
    if (pTrack->occupied) return;
    pTrack->occupied = 1;
    if (!(pTrack->unk_1E & 1) && pTrack->flags & 2) release_tracks(pTrack);
    pTrack->occupied = 0;
}

/* ResetTrack */
static void ResetTrack(TrackData *pTrack) {
    if (!(pTrack->unk_1E & 1) && pTrack->flags & 2) release_tracks(pTrack);
}

/* unk_4cfc: the sequencer ticks of this frame (tempo accumulator). */
static uint32_t unk_4cfc(TrackData *pTrack) {
    uint32_t word = pTrack->unk_C | (uint32_t)pTrack->unk_E << 16;
    uint32_t sum = (word + (word << 16)) >> 16;
    uint32_t ticks = 0;
    if (sum & 0xFF00) {
        ticks = sum >> 8;
        sum &= 0xFF;
    }
    pTrack->unk_E = (uint16_t)sum;
    return ticks;
}

/* unk_4d1c: TEMPO */
static void unk_4d1c(TrackData *pTrack, TrackVariables *pVariables) {
    uint8_t tempo = rd8(pVariables->pRawData + 1);
    pVariables->pRawData += 2;
    uint32_t bpm = (uint32_t)tempo << 1;
    pTrack->unk_A = (uint16_t)bpm;
    pTrack->unk_C = (uint16_t)(bpm == 150 ? 0x100 : ((bpm << 8) * 0x1B4Eu) >> 20);
    pTrack->unk_E = 0;
}

/* UploadSampleToWaveRam */
static void UploadSampleToWaveRam(uint32_t sample) {
    reg_write8(0x04000070, 0x40);
    for (int i = 0; i < 16; ++i) reg_write8(0x04000090 + (uint32_t)i, rd8(sample + (uint32_t)i));
    reg_write8(0x04000070, 0);
}

/* unk_4e10: the voice of a drum kit key or of a key split. */
static void unk_4e10(TrackVariables *pVariables) {
    uint32_t voice = pVariables->pSample2;
    uint8_t key = pVariables->unk_1;
    if ((pVariables->unk_0 & 0xF0) == 0x80) {
        voice += 12u * key;
        pVariables->unk_35 = rd8(voice + 1);
        uint8_t pan = rd8(voice + 3);
        if (pan == 0) pVariables->unk_6 = 0x40;
        else if ((int)pan - 0x80 >= 0) pVariables->unk_6 = (uint8_t)(pan - 0x80);
    } else {
        voice += 12u * rd8(pVariables->envelope2 + key);
    }
    uint32_t word = rd32(voice);
    uint8_t type = (uint8_t)word;
    pVariables->channel = type;
    pVariables->unk_36 = (uint8_t)(word >> 16);
    uint8_t channel = type & 7;
    if (channel == 0) {
        pVariables->pSample1 = rd32(voice + 4);
    } else if (channel <= 2 || channel == 4) {
        int shift = 6;
        if (channel <= 2) {
            uint8_t sweep = (uint8_t)(word >> 24);
            pVariables->unk_37 = !(sweep & 0x80) && sweep & 0x70 ? sweep : 8;
        } else {
            shift = 3;
        }
        /* strb: only the low byte of pSample1 changes. */
        pVariables->pSample1 = (pVariables->pSample1 & ~0xFFu) |
                               (uint8_t)(rd32(voice + 4) << shift);
    } else if (channel == 3) {
        UploadSampleToWaveRam(rd32(voice + 4));
    }
    pVariables->envelope1 = envelope_at(voice + 8);
}

/* unk_4eb4: the key offset (unk_17) and fine adjustment (unk_18). */
static void unk_4eb4(TrackVariables *pVariables) {
    int32_t bend = pVariables->pitchBend;
    if (bend > 0) bend++;
    uint32_t word = pVariables->keyShift | (uint32_t)pVariables->unk_1D << 8 |
                    (uint32_t)pVariables->tune << 16 | (uint32_t)pVariables->unk_1F << 24;
    int32_t value = bend * pVariables->bendRange * 4;
    value += (int32_t)(int8_t)word << 8;
    value += (int32_t)(int8_t)(word >> 8) << 8;
    value += ((int32_t)(word << 8) >> 22) << 2;
    value += (int32_t)(word >> 24);
    if (pVariables->lfoSpeed && pVariables->modulationDepth && pVariables->modulationType == 0)
        value += pVariables->unk_13 * 4 * 12;
    pVariables->unk_17 = (int8_t)(value >> 8);
    pVariables->unk_18 = (uint8_t)value;
}

/* unk_4f10: the volume and pan pair (unk_8, unk_9). */
static void unk_4f10(TrackVariables *pVariables) {
    int32_t volume = (int32_t)(pVariables->volume * pVariables->unk_5) >> 5;
    if (pVariables->modulationType == 1) {
        volume = (volume * (pVariables->unk_13 + 0x41)) >> 6;
        if (volume >= 0xFF) volume = 0xFF;
    }
    int32_t pan = (int8_t)pVariables->unk_7 + pVariables->unk_6 - 0x40;
    /* The native clamp turns a pan below -64 into +64. */
    if (pan >= 0x3F) pan = 0x3F;
    else if (pan < -0x40) pan = 0x40;
    if (pVariables->modulationType == 2) {
        pan += pVariables->unk_13;
        if (pan >= 0x3F) pan = 0x3F;
        else if (pan + 0x40 < 0) pan = 0x40;
    }
    uint32_t right = (uint32_t)((pan + 0x40) * volume) >> 7;
    uint32_t left = (uint32_t)((0x40 - pan) * volume) >> 7;
    uint16_t pair = (uint16_t)(right | left << 8);
    pVariables->unk_8 = (uint8_t)pair;
    pVariables->unk_9 = (uint8_t)(pair >> 8);
}

/* unk_4f8c: start a DirectSound note on a channel. */
static void unk_4f8c(SoundChannel *pChannel, TrackVariables *pVariables, int32_t priority) {
    pChannel->unk_13 |= 2;
    unk_20a4(pChannel);
    SoundChannel *head = pVariables->pChannel;
    if (head) head->pChannel1 = pChannel;
    pChannel->pChannel2 = head;
    pChannel->pChannel1 = NULL;
    pChannel->pVariables = pVariables;
    pVariables->pChannel = pChannel;
    pChannel->unk_0 = 1;
    pChannel->unk_1 = pVariables->channel;
    pChannel->unk_2 = (uint8_t)priority;
    pChannel->unk_3 = pVariables->unk_6;
    pChannel->envelope = pVariables->envelope1;
    pChannel->unk_C = pVariables->unk_C;
    pChannel->unk_D = pVariables->unk_D;
    pChannel->unk_E = pVariables->unk_E;
    pChannel->unk_F = pVariables->unk_F;
    uint32_t sample = pVariables->pSample1;
    pChannel->pSample = sample;
    pChannel->pData = sample + 0x10;
    pChannel->pSize = sample + 0xC;
    uint8_t key = pVariables->unk_1;
    pChannel->unk_6 = key;
    if ((pVariables->unk_0 & 0xF0) == 0x80) key = pVariables->unk_35;
    pChannel->unk_7 = key;
    int32_t midi = pVariables->unk_17 + key;
    if (midi < 0) midi = 0;
    else if (midi > 0x7F) midi = 0x7F;
    int32_t frequency = (int32_t)MidiKey2Freq(sample, (uint8_t)midi, pVariables->unk_18);
    if (frequency == gMusicInfo.sampleRate) frequency = 0x4000;
    else frequency = CallGetNoteFrequency(frequency, gMusicInfo.pitch);
    pChannel->unk_1C = (uint32_t)frequency;
}

/* The pointer operand of GOTO and PATT. */
static uint32_t read_pointer(uint32_t address) {
    return rd32(address);
}

/* AudioCommand_Goto */
static void AudioCommand_Goto(TrackVariables *pVariables) {
    pVariables->pRawData = read_pointer(pVariables->pRawData + 1);
}

/* AudioCommand_PatternPlay: the return address in the first free slot. */
static void AudioCommand_PatternPlay(TrackVariables *pVariables) {
    uint32_t back = pVariables->pRawData + 5;
    pVariables->pRawData = read_pointer(pVariables->pRawData + 1);
    uint32_t *slot = pVariables->patternStartPointers;
    if (slot[0] && slot[1] && slot[2]) return;
    if (slot[0] == 0) slot[0] = back;
    else if (slot[1] == 0) slot[1] = back;
    else slot[2] = back;
}

/* unk_5104: write a PSG channel's registers. */
static void unk_5104(PsgSoundData *pSound) {
    uint16_t control = pSound->unk_14;
    uint8_t channel = pSound->pVariables ? pSound->pVariables->channel : 0;
    if (channel > 8) {
        uint8_t resolution = (uint8_t)(reg_read8(REG_SOUNDBIAS + 1) >> 6 << 6);
        if (resolution < 0x40) {
            control = (uint16_t)((control + 2) & 0xC7FE);
        } else if (resolution < 0x80) {
            control = (uint16_t)((control + 1) & 0xC7FE);
        }
    }
    uint16_t low = (uint16_t)(pSound->unk_11 | pSound->unk_12 << 8);
    switch (channel & 7) {
    case 1:
        reg_write8(0x04000060, pSound->unk_10);
        reg_write16(0x04000062, low);
        reg_write16(0x04000064, control);
        reg_write16(0x04000064, control);
        break;
    case 2:
        reg_write16(0x04000068, low);
        reg_write16(0x0400006C, control);
        break;
    case 3:
        reg_write8(0x04000070, pSound->unk_10);
        reg_write16(0x04000072, low);
        reg_write16(0x04000074, control);
        break;
    case 4:
        reg_write16(0x04000078, low);
        reg_write16(0x0400007C, control);
        break;
    }
    pSound->unk_14 = (uint16_t)(control & 0x7FFF);
}

/* ---- asm/soundcode.s ---- */

/* SoundCodeB: the reverb of the previous output into the accumulators. */
static uint32_t *SoundCodeB(uint8_t *a, uint8_t *b, uint32_t *out, int count) {
    int reverb = gMusicInfo.reverb;
    for (; count > 0; --count) {
        int32_t r6 = (int8_t)a[0x600] + (int8_t)a[0] + (int8_t)b[0] + (int8_t)b[0x600];
        int32_t r8 = (int8_t)a[0x601] + (int8_t)a[1] + (int8_t)b[1] + (int8_t)b[0x601];
        a += 2;
        b += 2;
        r6 *= reverb;
        r8 *= reverb;
        r6 = r6 >> 9;
        if (r6 < 0) r6++;
        r8 = r8 >> 9;
        if (r8 < 0) r8++;
        out[0] = out[1] = (uint32_t)r6 << 7;
        out[2] = out[3] = (uint32_t)r8 << 7;
        out += 4;
    }
    return out;
}

/* SoundCodeC: the accumulators through the clamp table into the FIFO
 * halves (first words to FIFO A, second to FIFO B). */
static uint32_t *SoundCodeC(uint8_t *out, uint32_t *in, int count) {
    uint8_t *second = out + 0x600;
    for (; count > 0; --count) {
        for (int i = 0; i < 4; ++i) {
            out[i] = T.sArray_808cce2[in[i * 2] >> 7 & 0x1FF];
            second[i] = T.sArray_808cce2[in[i * 2 + 1] >> 7 & 0x1FF];
        }
        out += 4;
        second += 4;
        in += 8;
    }
    return in;
}

/* One interpolated sample of SoundCodeA, with the native unsigned shift. */
static void mix_interpolated(uint32_t *acc, uint32_t data, uint32_t position, uint32_t left,
                             uint32_t right) {
    uint32_t index = position >> 14;
    int32_t s = rds8(data + index), n = rds8(data + index + 1);
    uint32_t value = (uint32_t)s + ((uint32_t)((n - s) * (int32_t)(position & 0x3FFF)) >> 14);
    acc[0] += value * left;
    acc[1] += value * right;
}

/* SoundCodeA: one channel into `count` groups of four samples. Each group
 * first checks whether four steps stay inside the sample (the fast path
 * without checks); otherwise every step is checked until the end or, for a
 * looped sample, until the first loop back, after which the group's
 * remaining samples go unchecked as in the native code. */
static void SoundCodeA(SoundChannel *pChannel, uint32_t *acc, int count) {
    uint32_t position = pChannel->unk_18;
    uint32_t left = pChannel->unk_11, right = pChannel->unk_12;
    uint32_t data = pChannel->pData, header = pChannel->pSample;
    uint32_t length = rd32(header + 0xC);
    uint16_t loop = rd16(header + 2);
    if (pChannel->unk_1 == 0x8) {
        /* Fixed frequency: one sample byte per output sample. */
        uint32_t loop_start = rd32(header + 8);
        for (; count > 0; --count) {
            if (position + 4 < length) {
                for (int k = 0; k < 4; ++k, acc += 2, position++) {
                    int32_t s = rds8(data + position);
                    acc[0] += (uint32_t)s * left;
                    acc[1] += (uint32_t)s * right;
                }
                continue;
            }
            for (int k = 0; k < 4; ++k, acc += 2) {
                int32_t s = rds8(data + position);
                acc[0] += (uint32_t)s * left;
                acc[1] += (uint32_t)s * right;
                if (++position == length) {
                    if (!loop) goto end;
                    position = loop_start;
                }
            }
        }
    } else if (pChannel->unk_1 == 0x20) {
        /* Reversed playback, without loop. */
        uint32_t step = ~pChannel->unk_1C + 1;
        for (; count > 0; --count) {
            bool fast = (int32_t)(position + (step << 2)) >> 14 > 0;
            for (int k = 0; k < 4; ++k, acc += 2) {
                mix_interpolated(acc, data, position, left, right);
                if (!fast && k == 3) goto end;
                position += step;
                if (!fast && (position >> 14) == 0) goto end;
            }
        }
    } else {
        uint32_t step = pChannel->unk_1C;
        uint32_t loop_back = rd32(header + 8) - length;
        for (; count > 0; --count) {
            bool checked = (int32_t)(length - ((position + (step << 2)) >> 14)) <= 0;
            for (int k = 0; k < 4; ++k, acc += 2) {
                mix_interpolated(acc, data, position, left, right);
                position += step;
                if (!checked) continue;
                if (!loop) {
                    if (k == 3 || (position >> 14) >= length) goto end;
                } else if (k == 3 || (position >> 14) >= length) {
                    position = (position & 0x3FFF) + (((position >> 14) + loop_back) << 14);
                    checked = false;
                }
            }
        }
    }
    pChannel->unk_18 = position;
    return;
end:
    pChannel->unk_0 = 0;
}

/* ---- src/audio.c ---- */

/* UpdateMusic: envelopes and the DirectSound mixing of the next blocks. */
static void UpdateMusic(void) {
    uint32_t var_0 = gMusicInfo.unk_9;
    if (var_0 != 0) {
        uint32_t vcount = read_vcount();
        if (vcount < 160) vcount += 228;
        var_0 = vcount + gMusicInfo.unk_9;
    }
    uint8_t var_8 = gMusicInfo.unk_10;
    uint16_t var_4 = (uint16_t)(gMusicInfo.unk_11 * 16);
    uint8_t var_7 = (uint8_t)(((gMusicInfo.unk_C << 1) + var_8) - 1);
    if (var_7 >= gMusicInfo.unk_E) var_7 = (uint8_t)(var_7 - gMusicInfo.unk_E);
    uint8_t var_3 = 0, var_6 = 0;
    if (var_8 <= var_7 && var_7 <= gMusicInfo.unk_11) {
        var_4 = (uint16_t)((var_7 - gMusicInfo.unk_C + 1) * 16);
        var_6 = gMusicInfo.unk_C;
        gMusicInfo.unk_11 = var_7 + 1 == gMusicInfo.unk_E ? 0 : (uint8_t)(var_7 + 1);
    } else if (gMusicInfo.unk_11 <= var_8 && var_8 <= var_7) {
        var_4 = (uint16_t)((var_7 - gMusicInfo.unk_C + 1) * 16);
        var_6 = gMusicInfo.unk_C;
        gMusicInfo.unk_11 = var_7 + 1 == gMusicInfo.unk_E ? 0 : (uint8_t)(var_7 + 1);
    } else if (var_7 <= gMusicInfo.unk_11 && gMusicInfo.unk_11 <= var_8) {
        var_4 = 0;
        var_6 = (uint8_t)(var_7 + 1);
        gMusicInfo.unk_11 = var_6;
    } else if (gMusicInfo.unk_11 < var_7) {
        var_6 = (uint8_t)(var_7 - gMusicInfo.unk_11 + 1);
        gMusicInfo.unk_11 = var_7 + 1 == gMusicInfo.unk_E ? 0 : (uint8_t)(var_7 + 1);
    } else if (gMusicInfo.unk_11 > var_7) {
        var_6 = (uint8_t)(gMusicInfo.unk_E - gMusicInfo.unk_11);
        var_3 = (uint8_t)(var_7 + 1);
        gMusicInfo.unk_11 = var_3;
    } else {
        var_4 = 0;
        var_6 = (uint8_t)(var_7 + 1);
        gMusicInfo.unk_11 = var_6;
    }
    if (var_6 == 0) return;

    uint8_t *buffer2 = &gMusicInfo.soundRawData[var_4];
    uint32_t *buffer = SoundCodeB(buffer2, buffer2, gMusicInfo.musicRawData, var_6 * 8);
    if (var_3 != 0)
        SoundCodeB(gMusicInfo.soundRawData, gMusicInfo.soundRawData, buffer, var_3 * 8);

    gMusicInfo.currentSoundChannel = 0;
    for (uint16_t i = 0; i < gMusicInfo.maxSoundChannels && i < MAX_SOUND_CHANNELS; i++) {
        if (var_0 != 0) {
            uint32_t vcount = read_vcount();
            if (vcount < 160) vcount += 228;
            if (vcount >= var_0) break;
        }
        SoundChannel *pChannel = &gMusicInfo.soundChannels[i];
        if (pChannel->unk_0 == 0) continue;
        gMusicInfo.currentSoundChannel++;
        TrackVariables *pVariables = pChannel->pVariables;
        while (pChannel->unk_13 != 0) {
            if (pChannel->unk_13 & 0x2) {
                pChannel->unk_18 = pChannel->unk_1 == 0x20 ? rd32(pChannel->pSample + 12) << 14 : 0;
                pChannel->unk_10 = 0;
                pChannel->unk_13 &= (uint8_t)~0x2;
                pChannel->unk_13 |= 0x10;
            } else if (pChannel->unk_13 & 0x4) {
                pChannel->unk_13 &= (uint8_t)~0x4;
            } else if (pChannel->unk_13 & 0x10) {
                if (pVariables && pVariables->unk_0 & 0x80) {
                    pVariables->unk_6 = pChannel->unk_3;
                    unk_4f10(pVariables);
                }
                if (pVariables) {
                    pChannel->unk_4 = (uint8_t)((pVariables->unk_8 * (pChannel->unk_F + 1)) >> 7);
                    pChannel->unk_5 = (uint8_t)((pVariables->unk_9 * (pChannel->unk_F + 1)) >> 7);
                }
                pChannel->unk_13 &= (uint8_t)~0x10;
            } else {
                pChannel->unk_13 = 0;
            }
        }
        int16_t var_1 = pChannel->unk_10;
        uint32_t var_5 = pChannel->unk_0 & 0xF;
        uint32_t tmp2;
        uint8_t sustain;
        if (var_5 == 0xA) {
            pChannel->unk_0 = 0;
            continue;
        }
        if (var_5 == 1) {
            if (pChannel->envelope.attack != 0xFF) goto lbl_1;
            goto lbl_3;
        }
        switch (var_5) {
        case 2:
            goto lbl_2;
        lbl_1:
            var_1 = 0;
            pChannel->unk_0 = 2;
        lbl_2:
            var_1 = (int16_t)(var_1 + pChannel->envelope.attack);
            if (var_1 <= 0xFE) break;
        lbl_3:
            tmp2 = pChannel->envelope.decay;
            sustain = pChannel->envelope.sustain;
            if (tmp2 != 0) {
                var_1 = 0xFF;
                pChannel->unk_0 = 3;
            } else {
                var_1 = sustain;
                pChannel->unk_0 = 4;
                break;
            }
            /* fallthrough */
        case 3:
            if ((var_1 = (int16_t)((var_1 * pChannel->envelope.decay) >> 8)) >
                (sustain = pChannel->envelope.sustain))
                break;
            if (sustain != 0) {
                var_1 = sustain;
                pChannel->unk_0 = 4;
                break;
            }
            /* fallthrough */
        case 5:
            pChannel->unk_0 = 6;
            /* fallthrough */
        case 6:
            if ((var_1 = (int16_t)((var_1 * pChannel->envelope.release) >> 8)) > 0) break;
            pChannel->unk_0 = 0;
            if (pChannel->unk_C != 0 && pChannel->unk_D != 0) pChannel->unk_0 = 8;
            else unk_20a4(pChannel);
            continue;
        case 8:
            pChannel->unk_D--;
            if (pChannel->unk_D == 0) {
                pChannel->unk_0 = 0;
                unk_20a4(pChannel);
            }
            break;
        }
        pChannel->unk_10 = (uint8_t)var_1;
        int16_t var_2 = (int16_t)((var_1 * (gMusicInfo.volume + 1)) >> 4);
        pChannel->unk_11 = (uint8_t)((var_2 * pChannel->unk_4) >> 8);
        pChannel->unk_12 = (uint8_t)((var_2 * pChannel->unk_5) >> 8);
        SoundCodeA(pChannel, gMusicInfo.musicRawData, (var_6 + var_3) * 4);
    }

    buffer2 = &gMusicInfo.soundRawData[var_4];
    buffer = SoundCodeC(buffer2, gMusicInfo.musicRawData, var_6 * 4);
    if (var_3 != 0) SoundCodeC(gMusicInfo.soundRawData, buffer, var_3 * 4);
}

/* UpdatePsgSounds */
static void UpdatePsgSounds(void) {
    int16_t var_0 = 0;
    for (uint8_t i = 0; i < 4; i++) {
        PsgSoundData *pSound = &gPsgSounds[i];
        TrackVariables *pVariables;
        uint8_t control;
        if (pSound->unk_0 == 0) continue;
        while (pSound->unk_F) {
            if (pSound->unk_F & 0x2) {
                pVariables = pSound->pVariables;
                pSound->unk_A = pVariables->unk_F;
                pSound->unk_B = pVariables->volume;
                pSound->unk_D = pVariables->unk_C;
                pSound->unk_E = pVariables->unk_D;
                pSound->unk_1D = pVariables->unk_36;
                pSound->unk_1E = pVariables->unk_37;
                pSound->pSample = pVariables->pSample1;
                pSound->envelope = pVariables->envelope1;
                pSound->unk_1F = pVariables->modulationType;
                pSound->unk_F &= (uint8_t)~0x2;
                pSound->unk_F |= 0x10;
                continue;
            }
            if (pSound->unk_F & 0x4) {
                pSound->unk_14 = pSound->maybe_noteDelay;
                unk_5104(pSound);
                pSound->unk_F &= (uint8_t)~0x4;
                continue;
            }
            if (pSound->unk_F & 0x10) {
                if (pSound->pVariables->unk_6 != pSound->unk_C) {
                    pSound->pVariables->unk_6 = pSound->unk_C;
                    unk_4f10(pSound->pVariables);
                }
                pSound->unk_2 = (uint8_t)((pSound->pVariables->unk_8 * (pSound->unk_A + 1)) >> 7);
                pSound->unk_3 = (uint8_t)((pSound->pVariables->unk_9 * (pSound->unk_A + 1)) >> 7);
                pSound->unk_1A = (uint8_t)((pSound->unk_2 + pSound->unk_3) >> 4);
                pSound->unk_1B = (uint8_t)((pSound->unk_1A * pSound->envelope.sustain + 0xF) >> 4);
                control = (uint8_t)(reg_read8(0x04000081) & ~(0x11 << i));
                if (pSound->unk_2 >= pSound->unk_3) {
                    if ((pSound->unk_2 >> 1) > pSound->unk_3) reg_write8(0x04000081, (uint8_t)(control | (1 << i)));
                    else reg_write8(0x04000081, (uint8_t)(control | (17 << i)));
                } else {
                    if ((pSound->unk_3 >> 1) > pSound->unk_2) reg_write8(0x04000081, (uint8_t)(control | (16 << i)));
                    else reg_write8(0x04000081, (uint8_t)(control | (17 << i)));
                }
                pSound->unk_12 = 0;
                pSound->unk_14 |= 0x8000;
                var_0 = 1;
                pSound->unk_F &= (uint8_t)~0x10;
                continue;
            }
            if (pSound->unk_F & 0x20) {
                pSound->unk_14 = pSound->maybe_noteDelay | 0x8000;
                unk_5104(pSound);
                pSound->unk_F &= (uint8_t)~0x20;
            }
            pSound->unk_F = (uint8_t)(pSound->unk_F & 0x36);
            if (!(pSound->unk_F & 0x36)) pSound->unk_F = 0;
        }

        control = pSound->unk_0 & 0xF;
        if (control == 0xA) {
            ClearRegistersForPsg(pSound, i);
            continue;
        }
        if (control == 0x1) {
            if (i == 0) pSound->unk_10 = pSound->unk_1E;
            else if (i == 2) pSound->unk_10 = 0x80;
            pSound->unk_11 = i < 2 ? (uint8_t)pSound->pSample : 0;
            pSound->unk_14 = pSound->maybe_noteDelay | 0x8000;
            if (pSound->unk_1D != 0) goto lbl_pre_case_9;
            if (pSound->envelope.attack != 0) goto lbl_pre_case_2;
            if (pSound->envelope.decay == 0) goto lbl_1;
            goto lbl_2;
        }
        if (pSound->unk_18 == 0) {
            switch (pSound->unk_0 & 0xF) {
            case 2:
                goto lbl_case_2;
            lbl_pre_case_2:
                pSound->unk_19 = 0;
                pSound->unk_12 = i == 2 ? 0 : (uint8_t)(pSound->envelope.attack + 8);
                pSound->unk_0 = 2;
                unk_5104(pSound);
            lbl_case_2:
                if (++pSound->unk_19 >= pSound->unk_1A) {
                    if (pSound->envelope.decay == 0) goto lbl_1;
                    goto lbl_2;
                }
                if (i == 2) {
                    pSound->unk_12 = 0;
                    var_0 = 1;
                }
                pSound->unk_18 = pSound->envelope.attack;
                break;
            case 3:
                goto lbl_case_3;
            lbl_2:
                pSound->unk_19 = (int8_t)pSound->unk_1A;
                if (i == 2) {
                    pSound->unk_12 = 0;
                } else {
                    pSound->unk_12 = pSound->envelope.decay;
                    pSound->unk_14 = pSound->maybe_noteDelay | 0x8000;
                }
                pSound->unk_0 = 3;
                var_0 = 1;
            lbl_case_3:
                if (--pSound->unk_19 > pSound->unk_1B) {
                    if (i == 2) {
                        pSound->unk_12 = 0;
                        var_0 = 1;
                    }
                    pSound->unk_18 = pSound->envelope.decay;
                    break;
                }
            lbl_1:
                pSound->unk_19 = (int8_t)pSound->unk_1B;
                pSound->unk_12 = 0;
                if (i != 2) pSound->unk_14 = pSound->maybe_noteDelay | 0x8000;
                pSound->unk_0 = 4;
                var_0 = 1;
                /* fallthrough */
            case 4:
                pSound->unk_19 = (int8_t)pSound->unk_1B;
                if ((pSound->unk_0 & 0xF) == 4) pSound->unk_18 = 1;
                break;
            case 5:
                if (pSound->envelope.release != 0 && pSound->unk_1D == 0) {
                    if (i != 2) {
                        pSound->unk_12 = pSound->envelope.release;
                        pSound->unk_14 = pSound->maybe_noteDelay | 0x8000;
                    }
                    pSound->unk_0 = 6;
                    var_0 = 1;
                } else {
                    goto lbl_0;
                }
                /* fallthrough */
            case 6:
                pSound->unk_19--;
                if (pSound->unk_19 > 0) {
                    if (i == 2) {
                        pSound->unk_12 = 0;
                        var_0 = 1;
                    }
                    pSound->unk_18 = pSound->envelope.release;
                    break;
                }
            lbl_0:
                pSound->unk_0 = 0;
                if (pSound->unk_D != 0 && pSound->unk_E != 0) {
                    pSound->unk_19 = (int8_t)((pSound->unk_1A * pSound->unk_D + 0xFF) >> 8);
                    pSound->unk_18 = pSound->unk_E;
                    if (i == 2) {
                        pSound->unk_12 = T.sCgb3Vol[pSound->unk_19 & 15];
                    } else {
                        pSound->unk_12 = 0;
                        pSound->unk_14 = pSound->maybe_noteDelay | 0x8000;
                    }
                    pSound->unk_0 = 8;
                    var_0 = 1;
                } else {
                    ClearRegistersForPsg(pSound, i);
                    continue;
                }
                break;
            case 8:
                if (pSound->unk_18 == 0) {
                    ClearRegistersForPsg(pSound, i);
                    pSound->unk_0 = 0;
                }
                continue;
            case 9:
                goto lbl_case_9;
            lbl_pre_case_9:
                pSound->unk_11 |= pSound->unk_1D;
                pSound->unk_12 = 0;
                pSound->unk_19 = (int8_t)pSound->unk_1B;
                pSound->unk_14 |= 0x4000;
                pSound->unk_0 = 9;
                var_0 = 1;
            lbl_case_9:
                pSound->unk_18 = 0xFF;
                if (var_0) pSound->unk_14 |= 0x4000;
                break;
            }
        }
        if (var_0) {
            var_0 = 0;
            control = (uint8_t)pSound->unk_19;
            if (i == 2) pSound->unk_12 = T.sCgb3Vol[control & 15];
            else pSound->unk_12 |= (uint8_t)(control << 4);
            unk_5104(pSound);
        }
        pSound->unk_18--;
    }
}

static uint16_t GetNoteDelay(TrackVariables *pVariables, uint8_t key, uint8_t fine);

/* unk_1bf0: the gate times of the DirectSound notes. */
static void unk_1bf0(TrackVariables *pVariables) {
    for (SoundChannel *c = pVariables->pChannel; c; c = c->pChannel2)
        if (c->unk_E != 0 && --c->unk_E == 0) c->unk_0 = 5;
}

/* unk_1c18: the gate time of the PSG note. */
static void unk_1c18(TrackVariables *pVariables) {
    PsgSoundData *pSound = pVariables->pSoundPsg;
    if (pVariables->unk_E != 0 && --pVariables->unk_E == 0) {
        pSound->unk_0 = 5;
        pSound->unk_18 = 0;
    }
}

/* unk_1c3c: the LFO on DirectSound notes. */
static void unk_1c3c(TrackVariables *pVariables) {
    SoundChannel *pChannel = pVariables->pChannel;
    switch (pVariables->modulationType) {
    case 1:
    case 2:
        unk_4f10(pVariables);
        for (; pChannel; pChannel = pChannel->pChannel2) pChannel->unk_13 |= 0x10;
        break;
    case 0:
        unk_4eb4(pVariables);
        if (pChannel->unk_1 != 0 && pChannel->unk_1 != 0x20) break;
        for (; pChannel; pChannel = pChannel->pChannel2) {
            int16_t midi = (int16_t)(pVariables->unk_17 + pChannel->unk_7);
            if (midi >= 0x80) midi = 0x7F;
            else if (midi < 0) midi = 0;
            int32_t frequency = (int32_t)MidiKey2Freq(pVariables->pSample1, (uint8_t)midi,
                                                       pVariables->unk_18);
            if (frequency == gMusicInfo.sampleRate) frequency = 0x4000;
            else frequency = CallGetNoteFrequency(frequency, gMusicInfo.pitch);
            pChannel->unk_1C = (uint32_t)frequency;
        }
        break;
    }
}

/* unk_1ccc: the LFO on PSG notes. */
static void unk_1ccc(TrackVariables *pVariables, int16_t param) {
    PsgSoundData *pSound = pVariables->pSoundPsg;
    if (pVariables->modulationType == 1) {
        unk_4f10(pVariables);
        /* The native index runs outside the table; a zero divisor (the
         * table's own first entry) is taken as a zero quotient. */
        int divisor = param >= -64 && param <= 64 ? T.sUnk_808cc4d_window[param + 64] : 0;
        pSound->unk_12 = (uint8_t)(((uint8_t)pSound->unk_19 +
                                    (divisor ? (int32_t)pVariables->unk_13 / divisor : 0)) * 16);
        pSound->unk_F |= 0x20;
    } else if (pVariables->modulationType == 0) {
        unk_4eb4(pVariables);
        int16_t key = (int16_t)(pVariables->unk_17 + pSound->unk_1C);
        if (key >= 0x80) key = 0x7F;
        else if (key < 0) key = 0;
        pSound->maybe_noteDelay = GetNoteDelay(pVariables, (uint8_t)key, pVariables->unk_18);
        pSound->unk_F |= 0x4;
    } else if (pVariables->modulationType == 2) {
        unk_4f10(pVariables);
        pSound->unk_F |= 0x10;
    }
}

static void unk_1d5c(TrackVariables *pVariables) {
    for (SoundChannel *c = pVariables->pChannel; c; c = c->pChannel2) c->unk_13 |= 0x10;
}

static void unk_1d78(TrackVariables *pVariables) {
    for (SoundChannel *c = pVariables->pChannel; c; c = c->pChannel2) {
        int16_t midi = (int16_t)(pVariables->unk_17 + c->unk_7);
        if (midi >= 0x80) midi = 0x7F;
        else if (midi < 0) midi = 0;
        int32_t frequency = (int32_t)MidiKey2Freq(c->pSample, (uint8_t)midi, pVariables->unk_18);
        if (frequency == gMusicInfo.sampleRate) frequency = 0x4000;
        else frequency = CallGetNoteFrequency(frequency, gMusicInfo.pitch);
        c->unk_1C = (uint32_t)frequency;
        c->unk_13 |= 4;
    }
}

static void unk_1ddc(TrackVariables *pVariables) {
    pVariables->pSoundPsg->unk_F |= 0x10;
}

static void unk_1de8(TrackVariables *pVariables) {
    int16_t key = (int16_t)(pVariables->unk_17 + pVariables->pSoundPsg->unk_1C);
    if (key >= 0x80) key = 0x7F;
    else if (key < 0) key = 0;
    pVariables->pSoundPsg->maybe_noteDelay = GetNoteDelay(pVariables, (uint8_t)key,
                                                          pVariables->unk_18);
    pVariables->pSoundPsg->unk_F |= 0x4;
}

/* unk_1e2c: a free (or the weakest) DirectSound channel for a note. */
static void unk_1e2c(TrackData *pTrack, TrackVariables *pVariables) {
    uint8_t var_0 = pTrack->unk_3 + pVariables->priority > 0xFF ? 0xFF
                    : (uint8_t)(pTrack->unk_3 + pVariables->priority);
    uint32_t var_2 = var_0;
    TrackVariables *pVariables2 = pVariables;
    uint8_t var_1 = 0;
    SoundChannel *pChannel = NULL;
    int32_t var_3, var_4;
    for (uint8_t i = 0; i < gMusicInfo.maxSoundChannels && i < MAX_SOUND_CHANNELS; i++) {
        SoundChannel *c = &gMusicInfo.soundChannels[i];
        if (c->unk_0 == 0) {
            pChannel = c;
            goto end;
        }
        if (c->unk_0 == 5 || c->unk_0 == 6 || c->unk_0 == 8) {
            if (!var_1) {
                var_1 = 1;
                var_4 = 0;
                var_3 = 1;
            } else {
                var_4 = 1;
                var_3 = 0;
            }
        } else {
            var_4 = 0;
            var_3 = 0;
        }
        if (!var_3) {
            if (var_4 || !var_1) {
                if (c->unk_2 >= var_2 &&
                    (c->unk_2 > var_2 || (c->pVariables <= pVariables2 && c->pVariables < pVariables2)))
                    continue;
                var_3 = 1;
            } else {
                var_3 = 0;
            }
        }
        if (var_3) {
            var_2 = c->unk_2;
            pVariables2 = c->pVariables;
            pChannel = c;
        }
    }
    if (pChannel != NULL) {
    end:
        unk_4f8c(pChannel, pVariables, var_0);
    }
}

/* unk_1f3c: during a fade, only as many notes as the fade allows. */
static void unk_1f3c(TrackData *pTrack, TrackVariables *pVariables) {
    if (pTrack->flags & 0xC0 && ++pTrack->unk_23 <= pTrack->maxSoundChannels)
        unk_1e2c(pTrack, pVariables);
}

/* unk_2030: start a PSG note. */
static void unk_2030(PsgSoundData *pSound, TrackVariables *pVariables, uint32_t priority) {
    pSound->unk_16 = (uint8_t)priority;
    pSound->unk_1 = pVariables->channel;
    pSound->unk_C = pVariables->unk_6;
    pSound->unk_17 = pVariables->unk_1;
    pSound->unk_1C = (pVariables->unk_0 & 0xF0) == 0x80 ? pVariables->unk_35 : pVariables->unk_1;
    int16_t key = (int16_t)(pVariables->unk_17 + pSound->unk_1C);
    if (key >= 0x80) key = 0x7F;
    else if (key < 0) key = 0;
    pSound->maybe_noteDelay = GetNoteDelay(pVariables, (uint8_t)key, pVariables->unk_18);
    if (pSound->pVariables) pSound->pVariables->pSoundPsg = NULL;
    pSound->pVariables = pVariables;
    pSound->unk_F |= 2;
    pSound->unk_0 = 1;
    pVariables->pSoundPsg = pSound;
}

/* unk_1f90 and unk_1fe0: a PSG note when its channel is free or weaker. */
static void unk_1f90(TrackData *pTrack, TrackVariables *pVariables) {
    uint8_t var_0 = pTrack->unk_3 + pVariables->priority > 0xFF ? 0xFF
                    : (uint8_t)(pTrack->unk_3 + pVariables->priority);
    PsgSoundData *pSound = &gUnk_300376C[pVariables->channel & 7];
    if ((pVariables->channel & 7) > 4) return;
    if (pSound->unk_0 == 0 ||
        (var_0 >= pSound->unk_16 && (var_0 != pSound->unk_16 || pVariables <= pSound->pVariables)))
        unk_2030(pSound, pVariables, var_0);
}

static void unk_1fe0(TrackData *pTrack, TrackVariables *pVariables) {
    unk_1f90(pTrack, pVariables);
}

/* unk_20a4: unlink a channel from its track. */
static void unk_20a4(SoundChannel *pChannel) {
    if (pChannel->pVariables == NULL) return;
    if (pChannel->pChannel2) pChannel->pChannel2->pChannel1 = pChannel->pChannel1;
    if (pChannel->pChannel1) pChannel->pChannel1->pChannel2 = pChannel->pChannel2;
    else pChannel->pVariables->pChannel = pChannel->pChannel2;
    pChannel->pVariables = NULL;
}

/* GetNoteDelay: the PSG frequency register value of a key. */
static uint16_t GetNoteDelay(TrackVariables *pVariables, uint8_t key, uint8_t fine) {
    uint16_t delay;
    if ((pVariables->channel & 7) < 4) {
        delay = T.sUnk_808cad0[key & 0x7F];
        if (fine != 0) {
            if (key + 1 > 0x7F) key = 0x7F;
            uint16_t temp = (uint16_t)(((T.sUnk_808cad0[key + 1] - delay) * (fine + 1)) >> 8);
            delay = (uint16_t)(delay + temp);
        }
    } else {
        int index = key - 0x15;
        delay = (uint16_t)((index >= 0 && index < 60 ? T.sNoiseTable[index] : 0) |
                           (uint8_t)pVariables->pSample1);
    }
    return delay;
}

/* The end of a track (FINE, and 0xB6 which also requests the queued
 * music). */
static void end_track(TrackData *pTrack, TrackVariables *pVariables, bool queue) {
    if (pVariables->pSoundPsg) {
        ClearRegistersForPsg(pVariables->pSoundPsg, (uint8_t)((pVariables->channel & 7) - 1));
        pVariables->pSoundPsg->unk_0 = 0;
        pVariables->pSoundPsg->pVariables = NULL;
    }
    for (SoundChannel *c = pVariables->pChannel; c;) {
        SoundChannel *next = c->pChannel2;
        c->unk_0 = 10;
        c->pVariables = NULL;
        c->pChannel2 = NULL;
        c->pChannel1 = NULL;
        c = next;
    }
    pVariables->pSoundPsg = NULL;
    pVariables->pChannel = NULL;
    if (++pTrack->currentTrack == pTrack->amountOfTracks) {
        unk_2a38(pTrack);
        pTrack->flags = 0;
        pTrack->currentTrack = 0;
        if (queue) pTrack->queueFlags |= 0x2;
    }
    pVariables->unk_0 = 0;
}

static void AudioCommand_PatternEnd(TrackVariables *pVariables) {
    pVariables->pRawData++;
    for (int i = 2; i >= 0; i--) {
        if (pVariables->patternStartPointers[i]) {
            pVariables->pRawData = pVariables->patternStartPointers[i];
            pVariables->patternStartPointers[i] = 0;
            break;
        }
    }
}

static void AudioCommand_Repeat(TrackVariables *pVariables) {
    if (pVariables->repeatCount == 0) {
        pVariables->pRawData++;
        pVariables->repeatCount = rd8(pVariables->pRawData);
        AudioCommand_PatternPlay(pVariables);
        return;
    }
    if (--pVariables->repeatCount == 0) {
        AudioCommand_PatternEnd(pVariables);
        return;
    }
    pVariables->pRawData++;
    AudioCommand_PatternPlay(pVariables);
}

static void AudioCommand_Voice(TrackData *pTrack, TrackVariables *pVariables) {
    uint32_t voice = pTrack->pVoice + 12u * rd8(pVariables->pRawData);
    pVariables->channel = rd8(voice);
    if (pVariables->channel > 0x3F) {
        if (pVariables->channel == 0x80) {
            pVariables->unk_0 |= 0x80;
            pVariables->pSample2 = rd32(voice + 4);
        } else if (pVariables->channel == 0x40) {
            pVariables->unk_0 |= 0x40;
            pVariables->pSample2 = rd32(voice + 4);
            pVariables->envelope2 = rd32(voice + 8);
        }
    } else {
        pVariables->unk_0 &= 0xF;
        pVariables->unk_36 = rd8(voice + 2);
        uint32_t channel = pVariables->channel & 7;
        uint32_t sample = rd32(voice + 4);
        if (channel == 0) {
            pVariables->pSample1 = sample;
        } else if (channel < 3) {
            uint8_t sweep = rd8(voice + 3);
            pVariables->unk_37 = !(sweep & 0x80) && sweep & 0x70 ? sweep : 8;
            pVariables->pSample1 = sample << 0x1E >> 0x18;
        } else if (channel == 3) {
            UploadSampleToWaveRam(sample);
        } else if (channel == 4) {
            pVariables->pSample1 = sample << 0x1B >> 0x18;
        }
        pVariables->envelope1 = envelope_at(voice + 8);
    }
    pVariables->pRawData++;
}

static void AudioCommand_EndOfTie(TrackVariables *pVariables) {
    uint32_t key;
    if (rds8(pVariables->pRawData) >= 0) {
        key = rd8(pVariables->pRawData);
        pVariables->unk_1 = (uint8_t)key;
        pVariables->pRawData++;
    } else {
        key = pVariables->unk_1;
    }
    for (SoundChannel *c = pVariables->pChannel; c; c = c->pChannel2) {
        if ((uint8_t)(c->unk_0 - 1) < 4 && c->unk_6 == key) {
            c->unk_0 = 5;
            break;
        }
    }
    PsgSoundData *pSound = pVariables->pSoundPsg;
    if (pSound && pSound->unk_17 == key) {
        pSound->unk_0 = 5;
        pSound->unk_18 = 0;
    }
}

/* sMusicCommandFunctionPointers (the commands from FINE that only read
 * their track's variables). */
static void track_command(TrackVariables *v, uint8_t command) {
    switch (command) {
    case 0xB2: AudioCommand_Goto(v); break;
    case 0xB3: AudioCommand_PatternPlay(v); break;
    case 0xB4: AudioCommand_PatternEnd(v); break;
    case 0xB5: AudioCommand_Repeat(v); break;
    case 0xBA:          /* PRIO */
        v->pRawData++;
        v->priority = rd8(v->pRawData++);
        break;
    case 0xBC:          /* KEYSH */
        v->pRawData++;
        v->keyShift = rd8(v->pRawData);
        v->unk_0 |= 0x8;
        v->pRawData++;
        break;
    case 0xBE:          /* VOL */
        v->volume = rd8(v->pRawData++);
        v->unk_0 |= 0x4;
        break;
    case 0xBF:          /* PAN */
        v->unk_6 = rd8(v->pRawData++);
        v->unk_0 |= 0x4;
        break;
    case 0xC0:          /* BEND */
        v->pitchBend = (int8_t)(rd8(v->pRawData++) - 0x40);
        v->unk_0 |= 0x8;
        break;
    case 0xC1:          /* BENDR */
        v->bendRange = rd8(v->pRawData++);
        v->unk_0 |= 0x8;
        break;
    case 0xC2:          /* LFOS */
        v->lfoSpeed = (uint8_t)((rd8(v->pRawData++) + 1) / 2);
        break;
    case 0xC3: {        /* LFODL */
        uint8_t delay = rd8(v->pRawData++);
        v->unk_14 = delay;
        v->unk_15 = delay;
        break;
    }
    case 0xC4: v->modulationDepth = rd8(v->pRawData++); break;     /* MOD */
    case 0xC5: v->modulationType = rd8(v->pRawData++); break;      /* MODT */
    case 0xC8:          /* TUNE */
        v->tune = (uint8_t)(rd8(v->pRawData++) - 0x40);
        v->unk_0 |= 0x8;
        break;
    case 0xCD:          /* XCMD: xIECV, xIECL */
        if (rd8(v->pRawData) == 0x08) {
            v->pRawData++;
            v->unk_C = rd8(v->pRawData++);
        }
        if (rd8(v->pRawData) == 0x09) {
            v->pRawData++;
            v->unk_D = rd8(v->pRawData++);
        }
        break;
    case 0xCE: AudioCommand_EndOfTie(v); break;
    default: break;     /* Music_EmptyCommand */
    }
}

/* UpdateTrack: the sequencer ticks of a music player. */
static void UpdateTrack(TrackData *pTrack) {
    if (pTrack->occupied) return;
    pTrack->occupied = 1;
    if (!(pTrack->unk_1E & 1)) {
        if (pTrack->flags & 0xF8) {
            if (pTrack->flags & 0x98) unk_2d2c(pTrack);
            else unk_2e6c(pTrack);
        }
        if (pTrack->flags & 0x2) {
            uint32_t ticks = unk_4cfc(pTrack);
            /* A broken song (a loop without waits) cannot hang the engine. */
            int guard = 0;
            while (ticks != 0) {
                TrackVariables *v = pTrack->pVariables;
                for (uint8_t i = 0; i < pTrack->amountOfTracks; i++, v++) {
                    if (v->unk_0 == 0) continue;
                    if (v->pChannel) unk_1bf0(v);
                    if (v->pSoundPsg) unk_1c18(v);
                    if (v->delay != 0) {
                        v->delay--;
                        if (v->unk_15 != 0) {
                            v->unk_15--;
                        } else if (v->lfoSpeed != 0 && v->modulationDepth != 0) {
                            int16_t var_2;
                            if (v->pChannel) {
                                if (v->pChannel->unk_0 != 0) {
                                    v->unk_16 = (int8_t)(v->unk_16 + v->lfoSpeed);
                                    if ((int8_t)(v->unk_16 - 0x40) < 0) var_2 = v->unk_16;
                                    else var_2 = (int16_t)(0x80 - (uint8_t)v->unk_16);
                                    if (var_2 != (int8_t)v->unk_13) {
                                        v->unk_13 = (int8_t)((var_2 * (v->modulationDepth + 1)) >> 7);
                                        unk_1c3c(v);
                                    }
                                }
                            } else if (v->pSoundPsg && v->pSoundPsg->unk_0 != 0) {
                                v->unk_16 = (int8_t)(v->unk_16 + v->lfoSpeed);
                                if ((int8_t)(v->unk_16 - 0x40) < 0) var_2 = v->unk_16;
                                else var_2 = (int16_t)(0x80 - (uint8_t)v->unk_16);
                                if (var_2 != (int8_t)v->unk_13) {
                                    v->unk_13 = (int8_t)((var_2 * (v->modulationDepth + 1)) >> 7);
                                    unk_1ccc(v, var_2);
                                }
                            }
                        }
                    }
                    while (v->delay == 0 && v->unk_0 != 0) {
                        if (++guard > 100000) {
                            v->unk_0 = 0;
                            break;
                        }
                        uint8_t command = rd8(v->pRawData);
                        if ((int8_t)command >= 0) {
                            command = v->unk_3;
                        } else if (command > 0xBC) {
                            v->unk_3 = command;
                            v->pRawData++;
                        }
                        if (command > 0xCE) {
                            v->unk_0 |= 0x2;
                            v->unk_E = T.sClockTable[command - 0xCF];
                            uint8_t value = rd8(v->pRawData);
                            if (v->unk_14 != 0) v->unk_15 = v->unk_14;
                            if ((int8_t)value >= 0) {
                                v->unk_1 = value;
                                v->pRawData++;
                                value = rd8(v->pRawData);
                                if ((int8_t)value >= 0) {
                                    v->unk_F = value;
                                    v->pRawData++;
                                    value = rd8(v->pRawData);
                                    if ((int8_t)value >= 0) {
                                        v->unk_E = (uint8_t)(v->unk_E + value);
                                        v->pRawData++;
                                    }
                                }
                            }
                            if ((v->unk_0 & 0xF0) > 0x3F) unk_4e10(v);
                            v->unk_13 = 0;
                            unk_4eb4(v);
                            unk_4f10(v);
                            if (pTrack->flags & 0xC0) {
                                if (v->channel & 7) unk_1fe0(pTrack, v);
                                else unk_1f3c(pTrack, v);
                            } else {
                                if (v->channel & 7) unk_1f90(pTrack, v);
                                else unk_1e2c(pTrack, v);
                            }
                        } else if (command > 0xB0) {
                            if (command == 0xBB) {
                                unk_4d1c(pTrack, v);
                            } else if (command == 0xBD) {
                                AudioCommand_Voice(pTrack, v);
                            } else if (command == 0xB1) {
                                end_track(pTrack, v, false);
                                break;
                            } else if (command == 0xB6) {
                                end_track(pTrack, v, true);
                                break;
                            } else {
                                track_command(v, command);
                            }
                        } else {
                            v->delay = T.sClockTable[command - 0x80];
                            v->pRawData++;
                            break;
                        }
                    }
                    if (v->unk_0 & 4) {
                        unk_4f10(v);
                        if (v->pChannel) unk_1d5c(v);
                        if (v->pSoundPsg) unk_1ddc(v);
                        v->unk_0 &= (uint8_t)~4;
                    }
                    if (v->unk_0 & 8) {
                        unk_4eb4(v);
                        if (v->pChannel) unk_1d78(v);
                        if (v->pSoundPsg) unk_1de8(v);
                        v->unk_0 &= (uint8_t)~8;
                    }
                }
                ticks--;
            }
        }
    }
    pTrack->occupied = 0;
    if (pTrack->queueFlags == 0x1) {
        PlayMusic(pTrack->musicTrack, pTrack->priority);
        pTrack->musicTrack = 0;
        pTrack->priority = 0;
        pTrack->queueFlags = 0;
    } else if (pTrack->queueFlags & 0x4) {
        QueueSound(pTrack->musicTrack & 0x7FFF, 0);
        pTrack->musicTrack = 0;
        pTrack->priority = 0;
        pTrack->queueFlags = 0;
    } else if (pTrack->queueFlags & 0x2) {
        if (pTrack->queueFlags & 0x80) ReplayQueuedMusic(pTrack->queueFlags);
        pTrack->queueFlags = 0;
    }
}

/* ClearRegistersForPsg */
static void ClearRegistersForPsg(PsgSoundData *pSound, uint8_t channel) {
    (void)pSound;
    switch (channel) {
    case 0:
        reg_write8(0x04000063, 8);
        reg_write16(0x04000064, 0x8000);
        break;
    case 1:
        reg_write8(0x04000069, 8);
        reg_write16(0x0400006C, 0x8000);
        break;
    case 2:
        reg_write8(0x04000070, 0);
        break;
    case 3:
        reg_write8(0x04000079, 8);
        reg_write16(0x0400007C, 0x8000);
        break;
    }
}

/* ---- src/audio_wrappers.c ---- */

/* unk_2a38 */
static void unk_2a38(TrackData *pTrack) {
    if (!(pTrack->unk_1E & 1) && pTrack->flags & 2) pTrack->flags = 1;
}

/* ApplyMusicSoundFading */
static void ApplyMusicSoundFading(TrackData *pTrack, uint16_t timer) {
    if (timer == 0) {
        StopMusicOrSound(pTrack);
        return;
    }
    if (pTrack->occupied) return;
    pTrack->occupied = 1;
    if (pTrack->flags & 2) {
        if (pTrack->flags & 0xF8) {
            ResetTrack(pTrack);
        } else {
            pTrack->flags |= 8;
            pTrack->maybe_volume = 0xFFFF;
            pTrack->fadingTimer = (uint16_t)(0xFFFF / timer);
        }
    }
    pTrack->occupied = 0;
}

/* unk_2d2c: a fade out step. */
static void unk_2d2c(TrackData *pTrack) {
    TrackVariables *v = pTrack->pVariables;
    if ((int32_t)pTrack->maybe_volume - pTrack->fadingTimer >= 0) {
        pTrack->maybe_volume = (uint16_t)(pTrack->maybe_volume - pTrack->fadingTimer);
        if (pTrack->flags & 0x80) {
            pTrack->unk_24 = (uint16_t)(pTrack->unk_24 + pTrack->unk_26);
            if ((uint16_t)(pTrack->unk_24 >> 0xC) != 0) {
                pTrack->maxSoundChannels--;
                pTrack->unk_24 &= 0xFFF;
            }
            pTrack->unk_23 = 0;
        }
        for (int i = pTrack->amountOfTracks; i > 0; --i, ++v) {
            if (v->unk_0 != 0) {
                v->unk_5 = (uint8_t)(pTrack->maybe_volume / 1024);
                v->unk_0 |= 4;
            }
        }
    } else {
        ResetTrack(pTrack);
        if (pTrack->flags & 8) pTrack->flags = 0;
        else if (pTrack->flags & 0x10) pTrack->flags = 1;
        if (pTrack->musicTrack != 0) {
            if (pTrack->musicTrack & 0x8000) pTrack->queueFlags |= 4;
            else pTrack->queueFlags |= 1;
        }
    }
}

/* unk_2e6c: a fade in step. */
static void unk_2e6c(TrackData *pTrack) {
    TrackVariables *v = pTrack->pVariables;
    if ((int32_t)pTrack->maybe_volume + pTrack->fadingTimer <= 0xFFFF) {
        pTrack->maybe_volume = (uint16_t)(pTrack->maybe_volume + pTrack->fadingTimer);
        if (pTrack->flags & 0x40) {
            pTrack->unk_24 = (uint16_t)(pTrack->unk_24 + pTrack->unk_26);
            if ((uint16_t)(pTrack->unk_24 >> 0xC) != 0) {
                pTrack->maxSoundChannels++;
                pTrack->unk_24 &= 0xFFF;
            }
            pTrack->unk_23 = 0;
        }
        for (int i = pTrack->amountOfTracks; i > 0; --i, ++v) {
            v->unk_5 = (uint8_t)(pTrack->maybe_volume / 1024);
            v->unk_0 |= 4;
        }
    } else {
        for (int i = pTrack->amountOfTracks; i > 0; --i, ++v) {
            v->unk_5 = 0x40;
            v->unk_0 |= 4;
        }
        pTrack->flags = 0x2;
    }
}

/* DoSoundAction */
static void DoSoundAction(uint32_t action) {
    if (gMusicInfo.occupied) return;
    gMusicInfo.occupied = 1;
    if (action & 0x80) gMusicInfo.reverb = (uint8_t)(action + 0x80);
    if (action & 0xF00) {
        gMusicInfo.maxSoundChannels = (uint8_t)((action & 0xF00) >> 8);
        /* The native loop also clears the entry past the array. */
        for (int i = MAX_SOUND_CHANNELS - 1; i >= 0; i--) gMusicInfo.soundChannels[i].unk_0 = 0;
    }
    if (action & 0xF000) gMusicInfo.volume = (uint8_t)((action & 0xF000) >> 12);
    if (action & 0xF0000) {
        gMusicInfo.freqIndex = (uint8_t)((action & 0xF0000) >> 16);
        if (gMusicInfo.freqIndex != 0) SetupSoundTransfer();
    }
    if (action & 0xF00000)
        reg_write8(REG_SOUNDBIAS + 1, (uint8_t)((reg_read8(REG_SOUNDBIAS + 1) & 0x3F) |
                                                (action & 0xF00000) >> 14));
    uint32_t control = action & 0xF000000;
    if (control == 0x2000000) {
        reg_write16(0x04000082, reg_read16(0x04000082) & 0xE10D);
        reg_write16(0x04000082, reg_read16(0x04000082) | 0x1);
    } else if (control == 0x1000000) {
        reg_write16(0x04000082, reg_read16(0x04000082) & ~0x1);
        reg_write16(0x04000082, reg_read16(0x04000082) | 0x3302);
    }
    gMusicInfo.occupied = 0;
}

/* SetupSoundTransfer (the DMA and timer setup is the hardware model of
 * mzm_audio_render). */
static void SetupSoundTransfer(void) {
    memset(gMusicInfo.soundRawData, 0, sizeof gMusicInfo.soundRawData);
    uint8_t index = gMusicInfo.freqIndex < 13 ? gMusicInfo.freqIndex : 4;
    gMusicInfo.sampleRate = (uint16_t)T.sNativeSampleRate[index];
    gMusicInfo.pitch = T.sMusicPitchData[index];
    uint16_t samplesPerFrame = T.sPcmSamplesPerVBlankTable[index];
    gMusicInfo.unk_14 = samplesPerFrame;
    uint16_t blocks = (uint16_t)(samplesPerFrame / 16);
    gMusicInfo.unk_C = (uint8_t)blocks;
    uint32_t frames = blocks ? 96u / gMusicInfo.unk_C : 0;
    gMusicInfo.unk_D = (uint8_t)frames;
    gMusicInfo.unk_E = (uint8_t)(frames * blocks);
    gMusicInfo.unk_10 = (uint8_t)(gMusicInfo.unk_E - 1);
    gMusicInfo.unk_11 = (uint8_t)(blocks * 2);
    g_fifo_sample = 0;
    g_fifo_block = -1;
    g_fifo_a = g_fifo_b = 0;
}

/* InitializeAudio */
void mzm_audio_initialize(void) {
    if (gMusicInfo.occupied) return;
    gba_psg_reset(&g_psg);
    gMusicInfo.occupied = 1;
    reg_write8(0x04000084, 0x80);
    reg_write16(0x04000082, 0xA90E);
    reg_write8(REG_SOUNDBIAS + 1, (uint8_t)((reg_read8(REG_SOUNDBIAS + 1) & 0x3F) | 0x40));
    reg_write8(0x04000063, 0x08);
    reg_write8(0x04000065, 0x80);
    reg_write8(0x04000069, 0x08);
    reg_write8(0x0400006D, 0x80);
    reg_write8(0x04000079, 0x08);
    reg_write8(0x0400007D, 0x80);
    reg_write8(0x04000070, 0x0);
    reg_write8(0x04000080, 0x77);
    memset(&gMusicInfo, 0, 28);
    gMusicInfo.occupied = 1;
    gMusicInfo.unk_9 = UNK_AUDIO_0X64;
    memset(gPsgSoundsArea, 0, sizeof gPsgSoundsArea);
    static const uint16_t counts[NUM_MUSIC_PLAYERS] = {12, 10, 2, 2, 2, 2, 3, 1, 6};
    static const uint16_t unk_a[NUM_MUSIC_PLAYERS] = {1, 1, 1, 0, 1, 1, 0, 1, 1};
    TrackVariables *v = gTrackVariables;
    for (int i = 0; i < NUM_MUSIC_PLAYERS; i++) {
        sMusicTrackDataRom[i] = (TrackGroupRomData){&gTrackData[i], v, counts[i], unk_a[i]};
        v += counts[i];
        memset(&gTrackData[i], 0, sizeof gTrackData[i]);
        gTrackData[i].pVariables = sMusicTrackDataRom[i].pVariables;
        gTrackData[i].maxAmountOfTracks = (uint8_t)counts[i];
        gTrackData[i].unk_1D = (uint8_t)unk_a[i];
    }
    gMusicInfo.occupied = 0;
    DoSoundAction(SOUND_ACTION_STARTUP);
    gMusicInfo.occupied = 1;
    memset(gMusicInfo.soundChannels, 0, sizeof gMusicInfo.soundChannels);
    memset(gSoundQueue, 0, sizeof gSoundQueue);
    gMusicInfo.occupied = 0;
}

/* UpdateAudio */
void mzm_audio_update(void) {
    if (!g_bank) return;
    for (int i = 0; i < NUM_MUSIC_PLAYERS; i++) {
        if (!gSoundQueue[i].exists) continue;
        InitTrack(sMusicTrackDataRom[i].pTrack, gSoundQueue[i].pHeader);
        /* str 0: exists, priority and fadingTimer. */
        gSoundQueue[i].exists = 0;
        gSoundQueue[i].priority = 0;
        gSoundQueue[i].fadingTimer = 0;
        gSoundQueue[i].pHeader = 0;
    }
    int start = 0, count = NUM_MUSIC_PLAYERS;
    uint8_t priority = gMusicInfo.priority;
    if (priority & 0x80) {
        if ((priority & 0x7F) != 4) {
            start = 1;
            count = NUM_MUSIC_PLAYERS - 1;
        } else {
            count = 0;
        }
    } else if (priority == 4) {
        TrackData *pTrack = sMusicTrackDataRom[gMusicInfo.unk_20 ? 1 : 0].pTrack;
        if (pTrack->flags & 2) UpdateTrack(pTrack);
        count = 0;
    }
    for (int i = start; i < start + count; i++) {
        TrackData *pTrack = sMusicTrackDataRom[i].pTrack;
        if (pTrack && pTrack->flags & 2) UpdateTrack(pTrack);
    }
    if (!gMusicInfo.occupied) {
        gMusicInfo.occupied = 1;
        UpdatePsgSounds();
        UpdateMusic();
    }
    gMusicInfo.occupied = 0;
}

/* DMA2IntrCode */
static void DMA2IntrCode(void) {
    gMusicInfo.unk_10++;
    if (gMusicInfo.unk_10 == gMusicInfo.unk_E) gMusicInfo.unk_10 = 0;
}

int mzm_audio_render(int16_t *out, int max_frames, int rate) {
    if (!g_bank || gMusicInfo.unk_E == 0 || rate <= 0) return 0;
    /* One GBA frame: 280896 cycles of 16.78 MHz, `unk_14` FIFO samples. */
    const double frame_cycles = 280896.0;
    int samples = (int)gMusicInfo.unk_14;
    double output = g_output_phase + frame_cycles * rate / 16777216.0;
    int frames = (int)output;
    g_output_phase = output - frames;
    if (frames > max_frames) frames = max_frames;
    int written = 0;
    int consumed = 0;
    int8_t a = g_fifo_a, b = g_fifo_b;
    for (int i = 0; i < frames; ++i) {
        /* The FIFO sample under this output sample (zero-order hold). */
        int target = (int)((double)(i + 1) * samples / frames);
        while (consumed < target) {
            if (g_fifo_block < 0 || g_fifo_sample == 16) {
                /* DMA 1 and 2 fetch the next block, then DMA2IntrCode. */
                g_fifo_block = gMusicInfo.unk_10;
                g_fifo_sample = 0;
                DMA2IntrCode();
            }
            int offset = g_fifo_block * 16 + g_fifo_sample;
            a = (int8_t)gMusicInfo.soundRawData[offset];
            b = (int8_t)gMusicInfo.soundRawData[0x600 + offset];
            g_fifo_sample++;
            consumed++;
        }
        g_psg_phase += 4194304.0 / rate;
        int cycles = (int)g_psg_phase;
        g_psg_phase -= cycles;
        gba_psg_step(&g_psg, cycles);
        int left, right;
        gba_psg_mix(&g_psg, a, b, &left, &right);
        int l = left * 12, r = right * 12;
        out[written * 2] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        out[written * 2 + 1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
        written++;
    }
    g_fifo_a = a;
    g_fifo_b = b;
    return written;
}

/* ---- src/music_wrappers.c ---- */

/* QueueSound */
static void QueueSound(uint16_t sound, uint16_t timer) {
    uint8_t group = (uint8_t)entry_group(sound);
    uint32_t header = entry_header(sound);
    if (!header) return;
    uint8_t priority = rd8(header + 2);
    if (T.sArray_808cee2[group] == 0 || !(gSoundQueue[group].exists & 3) ||
        gSoundQueue[group].priority <= priority) {
        gSoundQueue[group].exists = timer == 0 ? 1 : 2;
        gSoundQueue[group].fadingTimer = timer;
        gSoundQueue[group].priority = priority;
        gSoundQueue[group].pHeader = header;
    }
}

/* StopOrFadeSound */
static void StopOrFadeSound(uint16_t sound, uint16_t timer) {
    uint8_t group = (uint8_t)entry_group(sound);
    uint32_t header = entry_header(sound);
    if (gSoundQueue[group].exists & 3 && header == gSoundQueue[group].pHeader) {
        gSoundQueue[group].exists = 0;
        return;
    }
    TrackData *pTrack = sMusicTrackDataRom[group].pTrack;
    if (header == pTrack->pHeader) {
        if (timer == 0) StopMusicOrSound(pTrack);
        else ApplyMusicSoundFading(pTrack, timer);
    }
}

/* PlayMusic */
static void PlayMusic(uint16_t musicTrack, uint8_t priority) {
    if (gMusicInfo.occupied) return;
    gMusicInfo.occupied = 1;
    if (!(gMusicInfo.priority & 0x84)) {
        gMusicInfo.priority = (uint8_t)((gMusicInfo.priority & 0x80) | priority);
        uint32_t header = entry_header(musicTrack);
        TrackData *pTrack = sMusicTrackDataRom[0].pTrack;
        if (header != pTrack->pHeader || !(pTrack->flags & 2)) {
            gMusicInfo.unk_20 = 0;
            gMusicInfo.musicTrack = musicTrack;
            gMusicInfo.occupied = 0;
            InitTrack(pTrack, header);
        }
    }
    gMusicInfo.occupied = 0;
}

/* DetermineNewMusicTrack (the alias pairs of include/constants/audio.h). */
static uint16_t DetermineNewMusicTrack(uint16_t musicTrack);

/* gSoundChannelBackup and gSoundChannelTrack2Backup */
typedef struct { SoundChannel *pChannel; SoundChannel channel; } SoundChannelBackup;
enum { SOUND_CHANNEL_BACKUPS = 7 };
static SoundChannelBackup gSoundChannelBackup[SOUND_CHANNEL_BACKUPS];
static SoundChannelBackup gSoundChannelTrack2Backup[SOUND_CHANNEL_BACKUPS];

/* The channel walk shared by unk_34ac and BackupTrackData2SoundChannels:
 * every sample channel of the track is saved and released. */
static uint8_t backup_track_channels(TrackData *pTrack, SoundChannelBackup *backup,
                                     uint8_t currChannel) {
    pTrack->unk_1E = 1;
    TrackVariables *pVariables = pTrack->pVariables;
    for (uint8_t j = 0; j < pTrack->amountOfTracks; j++, pVariables++) {
        if (pVariables->channel & 0xC0) continue;
        if (pVariables->pSoundPsg)
            ClearRegistersForPsg(pVariables->pSoundPsg, (uint8_t)((pVariables->channel & 7) - 1));
        SoundChannel *pChannelNext;
        for (SoundChannel *pChannel = pVariables->pChannel; pChannel; pChannel = pChannelNext) {
            /* The native arrays hold 7 entries and nothing bounds the
             * index; the port drops what would overflow them. */
            if (currChannel < SOUND_CHANNEL_BACKUPS) {
                backup[currChannel].pChannel = pChannel;
                backup[currChannel].channel = *pChannel;
            }
            currChannel++;
            pChannel->unk_0 = 0;
            pChannel->pVariables = NULL;
            pChannelNext = pChannel->pChannel2;
            pChannel->pChannel2 = NULL;
            pChannel->pChannel1 = NULL;
        }
    }
    return currChannel;
}

/* The walk shared by unk_35d0 and RetrieveTrackData2SoundChannels. */
static uint8_t retrieve_track_channels(TrackData *pTrack, const SoundChannelBackup *backup,
                                       uint8_t currChannel) {
    pTrack->unk_1E &= (uint8_t)~1;
    TrackVariables *pVariables = pTrack->pVariables;
    for (uint8_t j = 0; j < pTrack->amountOfTracks; j++, pVariables++) {
        if (pVariables->channel & 0xC0) continue;
        for (SoundChannel *pChannel = pVariables->pChannel; pChannel; pChannel = pChannel->pChannel2) {
            unk_20a4(pChannel);
            if (currChannel < SOUND_CHANNEL_BACKUPS) *pChannel = backup[currChannel].channel;
            currChannel++;
        }
    }
    return currChannel;
}

/* unk_34ac: saves the channels of the players under an inserted music
 * (2 alone, or 1, 3, 6 and 8 when interrupting: mask 0x14A). */
static void unk_34ac(uint8_t isInterrupting) {
    uint8_t currChannel = 0;
    for (uint8_t track = isInterrupting ? 1 : 2; track < NUM_MUSIC_PLAYERS; track++) {
        if (!((track == 2 && !isInterrupting) || (0x14A >> track) & 1)) continue;
        TrackData *pTrack = sMusicTrackDataRom[track].pTrack;
        if (pTrack->occupied) continue;
        pTrack->occupied = 1;
        if (!(pTrack->unk_1E & 1) && pTrack->flags & 2)
            currChannel = backup_track_channels(pTrack, gSoundChannelBackup, currChannel);
        pTrack->occupied = 0;
    }
}

/* unk_35d0: restores what unk_34ac saved. */
static void unk_35d0(uint8_t param_1) {
    uint8_t currChannel = 0;
    for (uint8_t i = param_1 ? 1 : 2; i < NUM_MUSIC_PLAYERS; i++) {
        if (!((i == 2 && !param_1) || (0x14A >> i) & 1)) continue;
        TrackData *pTrack = sMusicTrackDataRom[i].pTrack;
        if (pTrack->occupied) continue;
        pTrack->occupied = 1;
        if (pTrack->unk_1E & 1)
            currChannel = retrieve_track_channels(pTrack, gSoundChannelBackup, currChannel);
        pTrack->occupied = 0;
    }
}

/* unk_2a8c: stops every player whose channels are not saved. */
static void unk_2a8c(void) {
    for (int i = NUM_MUSIC_PLAYERS - 1; i >= 0; i--)
        if (!(sMusicTrackDataRom[i].pTrack->unk_1E & 1)) StopMusicOrSound(sMusicTrackDataRom[i].pTrack);
}

/* DelayMusicStart */
static void DelayMusicStart(TrackData *pTrack, uint16_t delay) {
    if (!pTrack->occupied) {
        pTrack->occupied = 1;
        TrackVariables *pVariables = pTrack->pVariables;
        for (uint8_t trackId = 0; trackId < pTrack->amountOfTracks; trackId++, pVariables++)
            if (pVariables->unk_0 != 0) pVariables->delay = (uint8_t)(pVariables->delay + delay);
    }
    pTrack->occupied = 0;
}

/* InsertMusicAndQueueCurrent (isNotInterrupting is 0 for the item jingles,
 * 1 for the loading jingle). */
static void InsertMusicAndQueueCurrent(uint16_t musicTrack, uint8_t isNotInterrupting) {
    if (gMusicInfo.occupied) return;
    gMusicInfo.occupied = 1;
    if (!(gMusicInfo.priority & 0x84)) {
        uint32_t pHeader = entry_header(musicTrack);
        TrackData *pTrack = sMusicTrackDataRom[0].pTrack;
        if ((pTrack->flags & (0x2 | 0x8 | 0x10)) != 0x2 || pHeader != pTrack->pHeader) {
            if (isNotInterrupting == 0) {
                StopMusicOrSound(sMusicTrackDataRom[3].pTrack);
                unk_34ac(1);
                unk_2a8c();
                pTrack->queueFlags |= 0x80;
            } else if (isNotInterrupting == 1) {
                unk_2a8c();
                pTrack->queueFlags |= 0x40;
                pTrack->queueFlags |= 0x80;
            }
            gMusicInfo.unk_20 = 1;
            gMusicInfo.occupied = 0;
            InitTrack(pTrack, pHeader);
        }
    }
    gMusicInfo.occupied = 0;
}

/* unk_3058: the track volumes of a player's masked tracks ((value & 0x3FC)
 * / 4; 0x40 is full), unless the player is stopping or fading. */
static void unk_3058(TrackData *pTrack, uint16_t variablesMask, uint16_t value) {
    if (pTrack->occupied) return;
    pTrack->occupied = 1;
    if (!(pTrack->flags & (0x80 | 0x40 | 0x20 | 0x10 | 0x8))) {
        TrackVariables *pVariables = pTrack->pVariables;
        value = (uint16_t)((value & 0x3FC) / 4);
        for (int i = pTrack->amountOfTracks - 1; i >= 0; i--, pVariables++) {
            if (variablesMask >> i & 1) {
                pVariables->unk_5 = (uint8_t)value;
                pVariables->unk_0 |= 4;
            }
        }
    }
    pTrack->occupied = 0;
}

/* gUnk_Audio0x50: a linker symbol at 0x50, read as its address. */
enum { AUDIO_VOLUME_DOWN = 0x50 };

/* ReplayQueuedMusic */
static void ReplayQueuedMusic(uint8_t queueFlags) {
    if (gMusicInfo.occupied) return;
    gMusicInfo.occupied = 1;
    if (!(queueFlags & 0x40)) unk_35d0(1);
    TrackData *pTrack = sMusicTrackDataRom[0].pTrack;
    gMusicInfo.unk_20 = 0;
    uint16_t music = (uint16_t)(gMusicInfo.musicTrack - 0x5A) < 0xA
                         ? DetermineNewMusicTrack(gMusicInfo.musicTrack)
                         : gMusicInfo.musicTrack;
    uint32_t pHeader = entry_header(music);
    gMusicInfo.occupied = 0;
    InitTrack(pTrack, pHeader);
    if (queueFlags & 0x40) DelayMusicStart(pTrack, 60);
    if (gMusicInfo.volumeDownFlag & 0x80) {
        unk_3058(sMusicTrackDataRom[0].pTrack, 0xFFFF, AUDIO_VOLUME_DOWN);
        unk_3058(sMusicTrackDataRom[1].pTrack, 0xFFFF, AUDIO_VOLUME_DOWN);
        unk_3058(sMusicTrackDataRom[7].pTrack, 0xFFFF, AUDIO_VOLUME_DOWN);
    }
    gMusicInfo.occupied = 0;
}

/* DecreaseMusicVolume (UpdateMusicWhenPausing): the pause sound, the
 * channels of the other players saved, the music a third as loud. */
void mzm_music_decrease_volume(void) {
    if (!g_bank) return;
    QueueSound(0x63, 0);            /* SOUND_63 (0x63, not 63: MUSIC_IMAGO_COCOON_BATTLE) */
    unk_34ac(0);
    gMusicInfo.volumeDownFlag |= 0x80;
    unk_3058(sMusicTrackDataRom[0].pTrack, 0xFFFF, AUDIO_VOLUME_DOWN);
    unk_3058(sMusicTrackDataRom[1].pTrack, 0xFFFF, AUDIO_VOLUME_DOWN);
}

/* ResetMusicVolume (UpdateMusicAfterPause) */
void mzm_music_reset_volume(void) {
    if (!g_bank) return;
    unk_3058(sMusicTrackDataRom[0].pTrack, 0xFFFF, 0x100);
    unk_3058(sMusicTrackDataRom[1].pTrack, 0xFFFF, 0x100);
    gMusicInfo.volumeDownFlag &= (uint8_t)~0x80;
    unk_35d0(0);
}

/* BackupTrackData2SoundChannels */
static void BackupTrackData2SoundChannels(void) {
    TrackData *pTrack = sMusicTrackDataRom[2].pTrack;
    if (!pTrack->occupied) {
        pTrack->occupied = 1;
        if (!(pTrack->unk_1E & 1) && pTrack->flags & 2)
            backup_track_channels(pTrack, gSoundChannelTrack2Backup, 0);
    }
    pTrack->occupied = 0;
}

/* RetrieveTrackData2SoundChannels */
static void RetrieveTrackData2SoundChannels(void) {
    TrackData *pTrack = sMusicTrackDataRom[2].pTrack;
    if (!pTrack->occupied) {
        pTrack->occupied = 1;
        if (pTrack->unk_1E & 1) retrieve_track_channels(pTrack, gSoundChannelTrack2Backup, 0);
    }
    pTrack->occupied = 0;
}

void mzm_music_insert(uint16_t music, uint8_t not_interrupting) {
    InsertMusicAndQueueCurrent(music, not_interrupting);
}

/* UpdateMusicPriority */
void mzm_music_update_priority(uint8_t priority) {
    if (gMusicInfo.occupied) return;
    gMusicInfo.occupied = 1;
    if (priority == 0x80) gMusicInfo.priority |= priority;
    else if (priority == 0xFF) gMusicInfo.priority &= (uint8_t)~0x80;
    else gMusicInfo.priority = (uint8_t)((gMusicInfo.priority & 0x80) | priority);
    gMusicInfo.occupied = 0;
}

void mzm_audio_backup_track2(void) { BackupTrackData2SoundChannels(); }
void mzm_audio_retrieve_track2(void) { RetrieveTrackData2SoundChannels(); }

void mzm_sound_play(uint16_t sound) {
    QueueSound(sound, 0);
}

void mzm_sound_play_not_already_playing(uint16_t sound) {
    TrackData *pTrack = sMusicTrackDataRom[entry_group(sound)].pTrack;
    if (!(pTrack->flags & 2) || entry_header(sound) != pTrack->pHeader) QueueSound(sound, 0);
}

void mzm_sound_stop(uint16_t sound) {
    StopOrFadeSound(sound, 0);
}

void mzm_sound_fade(uint16_t sound, uint16_t timer) {
    StopOrFadeSound(sound, timer);
}

void mzm_music_play(uint16_t music, uint8_t priority) {
    PlayMusic(music, priority);
}

/* FadeCurrentMusicAndQueueNextMusic */
void mzm_music_fade_and_queue(uint16_t timer, uint16_t music, uint8_t priority) {
    if (gMusicInfo.priority & 0x84) return;
    if (timer == 0) {
        PlayMusic(music, priority);
        return;
    }
    TrackData *pTrack = sMusicTrackDataRom[0].pTrack;
    pTrack->musicTrack = music;
    pTrack->priority = priority;
    ApplyMusicSoundFading(pTrack, timer);
}

void mzm_music_fade(uint16_t timer) {
    if (gMusicInfo.occupied) return;
    gMusicInfo.occupied = 1;
    if (!(gMusicInfo.priority & 0x84)) {
        TrackData *pTrack = sMusicTrackDataRom[0].pTrack;
        if (timer != 0) ApplyMusicSoundFading(pTrack, timer);
        else StopMusicOrSound(pTrack);
    }
    gMusicInfo.occupied = 0;
}

void mzm_sound_fade_all(uint16_t timer) {
    /* FadeAllSounds: players 1 to 8 */
    for (int i = 1; i <= 8 && i < NUM_MUSIC_PLAYERS; i++) ApplyMusicSoundFading(sMusicTrackDataRom[i].pTrack, timer);
}

void mzm_audio_stop_all(void) {
    for (int i = NUM_MUSIC_PLAYERS - 1; i >= 0; i--) StopMusicOrSound(sMusicTrackDataRom[i].pTrack);
}

/* The MUSIC_* pairs of DetermineNewMusicTrack, set by mzm_audio_open from
 * sound_tables.tsv (`alias` lines). */
static uint16_t g_alias[8][2];
static int g_alias_count;

static uint16_t DetermineNewMusicTrack(uint16_t musicTrack) {
    for (int i = 0; i < g_alias_count; ++i)
        if (g_alias[i][0] == musicTrack) return g_alias[i][1];
    return musicTrack;
}

/* SOUND_A9, the default track of CheckSetNewMusicTrack. */
static uint16_t g_default_track = 0xA9;

enum { MUSIC_STEALTH = 0x46, MUSIC_CHOZODIA_DETECTED = 0x49, MUSIC_STEALTH_FIRST = 0x5A };

/* unk_378c: CheckSetNewMusicTrack under the stealth and alarm priorities
 * (0x10 stealth, 0x20 detected, 0x40 alarm). The stealth area tracks
 * (0x5A to 0x63) keep the stealth or detected music; under the alarm a
 * stealth track is only remembered for after it. */
static void unk_378c(uint16_t musicTrack) {
    if (gMusicInfo.occupied) return;
    gMusicInfo.occupied = 1;
    ApplyMusicSoundFading(sMusicTrackDataRom[6].pTrack, 10);
    ApplyMusicSoundFading(sMusicTrackDataRom[8].pTrack, 10);
    if (gSoundQueue[6].exists & 3) gSoundQueue[6].exists = 0;
    if (gSoundQueue[8].exists & 3) gSoundQueue[8].exists = 0;
    bool change;
    if (gMusicInfo.priority & 0x30) {
        if ((uint16_t)(gMusicInfo.musicTrack - MUSIC_STEALTH_FIRST) < 10) {
            if ((uint16_t)(musicTrack - MUSIC_STEALTH_FIRST) >= 10)
                musicTrack = gMusicInfo.priority & 0x10 ? MUSIC_STEALTH : MUSIC_CHOZODIA_DETECTED;
            change = true;
        } else {
            change = (uint16_t)(musicTrack - MUSIC_STEALTH_FIRST) < 10;
        }
    } else if (gMusicInfo.priority & 0x40) {
        if ((uint16_t)(musicTrack - MUSIC_STEALTH_FIRST) < 10) gMusicInfo.musicTrackOnTransition = musicTrack;
        change = false;
    } else {
        change = true;
    }
    if (change) {
        TrackData *pTrack = sMusicTrackDataRom[0].pTrack;
        if (entry_header(DetermineNewMusicTrack(musicTrack)) != pTrack->pHeader) {
            ApplyMusicSoundFading(pTrack, 30);
            gMusicInfo.musicTrackOnTransition = musicTrack;
        }
    }
    gMusicInfo.occupied = 0;
}

/* CheckSetNewMusicTrack */
void mzm_music_check_set_new_track(uint16_t musicTrack) {
    if (gMusicInfo.priority & 4) return;
    if (gMusicInfo.priority & 0x70) {
        unk_378c(musicTrack);
        return;
    }
    if (gMusicInfo.priority != 0 || gMusicInfo.occupied) return;
    gMusicInfo.occupied = 1;
    ApplyMusicSoundFading(sMusicTrackDataRom[6].pTrack, 10);
    ApplyMusicSoundFading(sMusicTrackDataRom[8].pTrack, 10);
    if (gSoundQueue[6].exists & 3) gSoundQueue[6].exists = 0;
    if (gSoundQueue[8].exists & 3) gSoundQueue[8].exists = 0;
    TrackData *pTrack = sMusicTrackDataRom[0].pTrack;
    if (musicTrack == 0) musicTrack = g_default_track;
    uint16_t next = DetermineNewMusicTrack(musicTrack);
    if (entry_header(next) != pTrack->pHeader) {
        ApplyMusicSoundFading(pTrack, 30);
        gMusicInfo.musicTrackOnTransition = next;
    }
    gMusicInfo.occupied = 0;
}

/* unk_38d8: the transition track under the stealth priorities (none under
 * the alarm). */
static void unk_38d8(void) {
    if (!(gMusicInfo.priority & 0x40) && gMusicInfo.musicTrackOnTransition != 0) {
        InitTrack(sMusicTrackDataRom[0].pTrack, entry_header(DetermineNewMusicTrack(gMusicInfo.musicTrackOnTransition)));
        gMusicInfo.unk_20 = 0;
        gMusicInfo.musicTrack = gMusicInfo.musicTrackOnTransition;
    }
    gMusicInfo.musicTrackOnTransition = 0;
}

/* CheckPlayTransitionMusicTrack */
void mzm_music_check_play_transition_track(void) {
    if (gMusicInfo.priority & 4) return;
    if (gMusicInfo.priority & 0x70) {
        unk_38d8();
        return;
    }
    if (gMusicInfo.priority == 0 && gMusicInfo.musicTrackOnTransition != 0) {
        InitTrack(sMusicTrackDataRom[0].pTrack, entry_header(gMusicInfo.musicTrackOnTransition));
        gMusicInfo.unk_20 = 0;
        gMusicInfo.musicTrack = gMusicInfo.musicTrackOnTransition;
        gMusicInfo.musicTrackOnTransition = 0;
    }
}

/* UpdateMusicAfterAlarmDisable: back to the stealth track remembered under
 * the alarm, else to the detected music. */
void mzm_music_after_alarm_disable(void) {
    gMusicInfo.priority = 0x20;
    if (gMusicInfo.musicTrackOnTransition == 0 ||
        (uint16_t)(gMusicInfo.musicTrackOnTransition - MUSIC_STEALTH_FIRST) >= 10) {
        mzm_music_fade_and_queue(5, MUSIC_CHOZODIA_DETECTED, 0x20);
        return;
    }
    ApplyMusicSoundFading(sMusicTrackDataRom[0].pTrack, 5);
}

uint16_t mzm_music_current(void) {
    return gMusicInfo.musicTrack;
}

int mzm_audio_active_channels(void) {
    int count = 0;
    for (int i = 0; i < MAX_SOUND_CHANNELS; ++i) count += gMusicInfo.soundChannels[i].unk_0 != 0;
    return count;
}

bool mzm_audio_player_playing(int player) {
    return player >= 0 && player < NUM_MUSIC_PLAYERS && gTrackData[player].flags & 2;
}

/* ---- Loading ---- */

static bool parse_table(const char *list, void *destination, int width, int count) {
    const char *cursor = list;
    for (int i = 0; i < count; ++i) {
        char *end = NULL;
        unsigned long value = strtoul(cursor, &end, 10);
        if (end == cursor) return false;
        if (width == 1) ((uint8_t *)destination)[i] = (uint8_t)value;
        else if (width == 2) ((uint16_t *)destination)[i] = (uint16_t)value;
        else ((uint32_t *)destination)[i] = (uint32_t)value;
        cursor = *end == ',' ? end + 1 : end;
    }
    return true;
}

bool mzm_audio_open(const char *bank_path, const char *tables_path) {
    mzm_audio_close();
    FILE *f = fopen(bank_path, "rb");
    if (!f) return false;
    uint8_t header[16];
    bool ok = fread(header, 1, 16, f) == 16 && !memcmp(header, "MZMSND01", 8);
    if (ok) {
        g_bank_base = (uint32_t)(header[8] | header[9] << 8 | header[10] << 16 |
                                 (uint32_t)header[11] << 24);
        g_bank_size = (uint32_t)(header[12] | header[13] << 8 | header[14] << 16 |
                                 (uint32_t)header[15] << 24);
        g_bank = g_bank_size && g_bank_size < (32u << 20) ? malloc(g_bank_size) : NULL;
        ok = g_bank && fread(g_bank, 1, g_bank_size, f) == g_bank_size;
    }
    fclose(f);
    f = ok ? fopen(tables_path, "rb") : NULL;
    if (!f) ok = false;
    static char line[8192];
    int tables = 0;
    while (ok && fgets(line, sizeof line, f)) {
        char name[64];
        unsigned long address, count;
        if (sscanf(line, "entries\t%lu\t%lu", &address, &count) == 2) {
            g_entries = (uint32_t)address;
            g_entry_count = (uint32_t)count;
            continue;
        }
        unsigned long from, to;
        if (sscanf(line, "alias\t%lu\t%lu", &from, &to) == 2 && g_alias_count < 8) {
            g_alias[g_alias_count][0] = (uint16_t)from;
            g_alias[g_alias_count][1] = (uint16_t)to;
            g_alias_count++;
            continue;
        }
        if (sscanf(line, "default\t%lu", &from) == 1) {
            g_default_track = (uint16_t)from;
            continue;
        }
        if (sscanf(line, "table\t%63s\t", name) != 1 && sscanf(line, "window\t%63s\t", name) != 1)
            continue;
        const char *list = strchr(strchr(line, '\t') + 1, '\t');
        if (!list) continue;
        list++;
        struct { const char *name; void *destination; int width, count; } known[] = {
            {"sUnk_808cad0", T.sUnk_808cad0, 2, 129}, {"sClockTable", T.sClockTable, 1, 49},
            {"sNoiseTable", T.sNoiseTable, 1, 60}, {"sCgb3Vol", T.sCgb3Vol, 1, 16},
            {"sUnk_808cc4d", T.sUnk_808cc4d, 1, 19},
            {"sNativeSampleRate", T.sNativeSampleRate, 4, 13},
            {"sMusicPitchData", T.sMusicPitchData, 4, 13},
            {"sPcmSamplesPerVBlankTable", T.sPcmSamplesPerVBlankTable, 2, 13},
            {"sArray_808cce2", T.sArray_808cce2, 1, 512},
            {"sArray_808cee2", T.sArray_808cee2, 1, 10},
        };
        if (sscanf(line, "window\t%63s\t", name) == 1 && !strcmp(name, "sUnk_808cc4d")) {
            ok = parse_table(list, T.sUnk_808cc4d_window, 1, 129);
            tables++;
            continue;
        }
        for (size_t i = 0; i < sizeof known / sizeof known[0]; ++i)
            if (!strcmp(name, known[i].name)) {
                ok = parse_table(list, known[i].destination, known[i].width, known[i].count);
                tables++;
            }
    }
    if (f) fclose(f);
    if (!ok || tables != 11 || !g_entry_count) {
        fprintf(stderr, "Zero Mission sound data missing or invalid (python3 -m scripts.mzm_audio)\n");
        mzm_audio_close();
        return false;
    }
    return true;
}

void mzm_audio_close(void) {
    free(g_bank);
    g_bank = NULL;
    g_bank_size = 0;
    g_entry_count = 0;
    g_alias_count = 0;
}

bool mzm_audio_ready(void) {
    return g_bank != NULL;
}
