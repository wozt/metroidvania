/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free Zero Mission sound engine regression: a synthetic sound bank (a
 * DirectSound song and a PSG sound effect written here) drives the ported
 * sequencer, mixer and the PSG emulation. */
#include "gba_psg.h"
#include "mzm_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

enum { BASE = 0x08100000u, BANK_SIZE = 0x400 };
enum { SONG = 0x40, VOICES = 0x60, SAMPLE = 0x100, TRACK = 0x200, SFX = 0x280, SFX_TRACK = 0x2A0,
       SFX_VOICES = 0x2C0, LOOP_SONG = 0x300, LOOP_TRACK = 0x320, JINGLE = 0x340,
       JINGLE_TRACK = 0x360, TRACK2_SONG = 0x380 };

static uint8_t bank[BANK_SIZE];

static void put32(uint32_t offset, uint32_t value) {
    for (int i = 0; i < 4; ++i) bank[offset + (uint32_t)i] = (uint8_t)(value >> 8 * i);
}

static void put16(uint32_t offset, uint16_t value) {
    bank[offset] = (uint8_t)value;
    bank[offset + 1] = (uint8_t)(value >> 8);
}

static void put_bytes(uint32_t offset, const uint8_t *bytes, size_t count) {
    memcpy(bank + offset, bytes, count);
}

/* Entry 1: a one-track song on player 0 playing middle C on a looped
 * 64-sample square at the native rate for 24 ticks. Entry 2: a sound effect
 * on player 3 playing a square 1 note. Entry 3: the song's note repeated
 * forever (GOTO). Entry 4: a jingle ending with 0xB6, which replays the
 * queued music. Entry 5: the looping track on player 2. */
static void build_bank(void) {
    memset(bank, 0, sizeof bank);
    put32(8, BASE + SONG);
    put16(12, 0);
    put32(16, BASE + SFX);
    put16(20, 3);
    put32(24, BASE + LOOP_SONG);
    put16(28, 0);
    /* Song header: one track, priority 0, voice group, track. */
    bank[SONG] = 1;
    put32(SONG + 4, BASE + VOICES);
    put32(SONG + 8, BASE + TRACK);
    /* Voice 0: DirectSound, attack 255, decay 0, sustain 255, release 0. */
    put32(VOICES + 4, BASE + SAMPLE);
    bank[VOICES + 8] = 255;
    bank[VOICES + 10] = 255;
    /* Wave: looped, 13379 Hz * 1024 at key 60, 64 samples. */
    put16(SAMPLE + 2, 0x4000);
    put32(SAMPLE + 4, 13379u * 1024);
    put32(SAMPLE + 8, 0);
    put32(SAMPLE + 0xC, 64);
    for (int i = 0; i < 65; ++i) bank[SAMPLE + 0x10 + (uint32_t)i] = (uint8_t)(i % 64 < 32 ? 100 : -100);
    /* VOICE 0, VOL 100, TEMPO 75 (150 bpm: one tick a frame), N24 Cn3 v127,
     * W24, FINE. */
    static const uint8_t track[] = {0xBD, 0, 0xBE, 100, 0xBB, 75, 0xE7, 60, 127, 0x98, 0xB1};
    put_bytes(TRACK, track, sizeof track);
    /* Sound effect: one square 1 track. */
    bank[SFX] = 1;
    bank[SFX + 2] = 10;
    put32(SFX + 4, BASE + SFX_VOICES);
    put32(SFX + 8, BASE + SFX_TRACK);
    /* Voice 0: square 1, duty 2, no sweep; attack 0, decay 0, sustain 15. */
    bank[SFX_VOICES] = 1;
    put32(SFX_VOICES + 4, 2);
    bank[SFX_VOICES + 10] = 15;
    static const uint8_t sfx[] = {0xBD, 0, 0xBE, 127, 0xBB, 75, 0xD7, 72, 127, 0x90, 0xB1};
    put_bytes(SFX_TRACK, sfx, sizeof sfx);
    bank[LOOP_SONG] = 1;
    put32(LOOP_SONG + 4, BASE + VOICES);
    put32(LOOP_SONG + 8, BASE + LOOP_TRACK);
    static const uint8_t loop[] = {0xBD, 0, 0xBE, 100, 0xBB, 75, 0xE7, 60, 127, 0x98, 0xB2,
                                   (uint8_t)(BASE + LOOP_TRACK + 6), (uint8_t)((BASE + LOOP_TRACK) >> 8),
                                   (uint8_t)((BASE + LOOP_TRACK) >> 16), (uint8_t)((BASE + LOOP_TRACK) >> 24)};
    put_bytes(LOOP_TRACK, loop, sizeof loop);
    put32(32, BASE + JINGLE);
    put16(36, 0);
    bank[JINGLE] = 1;
    put32(JINGLE + 4, BASE + VOICES);
    put32(JINGLE + 8, BASE + JINGLE_TRACK);
    static const uint8_t jingle[] = {0xBD, 0, 0xBE, 100, 0xBB, 75, 0xE7, 67, 127, 0x98, 0xB6};
    put_bytes(JINGLE_TRACK, jingle, sizeof jingle);
    put32(40, BASE + TRACK2_SONG);
    put16(44, 2);
    /* Entry 0x63 (SOUND_63): the sound effect on player 3; entry 0x3F (63,
     * MUSIC_IMAGO_COCOON_BATTLE in the game): the player 2 song, which the
     * pause once played by mistake. */
    put32(0x63 * 8, BASE + SFX);
    put16(0x63 * 8 + 4, 3);
    put32(0x3F * 8, BASE + TRACK2_SONG);
    put16(0x3F * 8 + 4, 2);
    bank[TRACK2_SONG] = 1;
    put32(TRACK2_SONG + 4, BASE + VOICES);
    put32(TRACK2_SONG + 8, BASE + LOOP_TRACK);
}

static void write_list(FILE *f, const char *kind, const char *name, int count, int (*value)(int)) {
    fprintf(f, "%s\t%s\t", kind, name);
    for (int i = 0; i < count; ++i) fprintf(f, i ? ",%d" : "%d", value(i));
    fputc('\n', f);
}

static int note_delay(int i) { return 1000 + i * 7; }
static int clock(int i) {
    static const int tail[] = {28, 30, 32, 36, 40, 42, 44, 48, 52, 54, 56, 60, 64, 66, 68, 72,
                               76, 78, 80, 84, 88, 90, 92, 96};
    return i <= 24 ? i : tail[i - 25];
}
static int noise(int i) { return 0xD7 - i; }
static int cgb3(int i) { return i * 16 & 0xE0; }
static int divisor(int i) { return i + 1; }
static int rate(int i) { return i == 4 ? 13379 : 10000; }
static int pitch(int i) { return i == 4 ? 321023 : 0; }
static int per_frame(int i) { return i == 4 ? 224 : 96; }
static int clamp(int i) {
    int v = i < 256 ? i : i - 512;
    return (v > 127 ? 127 : v < -128 ? -128 : v) & 0xFF;
}
static int one(int i) { return i != 3 && i != 6; }
static int window(int i) { return i + 1; }

static bool write_files(const char *bank_path, const char *tables_path) {
    FILE *f = fopen(bank_path, "wb");
    if (!f) return false;
    uint8_t header[16] = {'M', 'Z', 'M', 'S', 'N', 'D', '0', '1'};
    for (int i = 0; i < 4; ++i) {
        header[8 + i] = (uint8_t)(BASE >> 8 * i);
        header[12 + i] = (uint8_t)(BANK_SIZE >> 8 * i);
    }
    fwrite(header, 1, 16, f);
    fwrite(bank, 1, sizeof bank, f);
    fclose(f);
    f = fopen(tables_path, "w");
    if (!f) return false;
    fprintf(f, "schema\tmetroidvania-mzm-sound-v1\nentries\t%u\t100\n", BASE);
    write_list(f, "table", "sUnk_808cad0", 129, note_delay);
    write_list(f, "table", "sClockTable", 49, clock);
    write_list(f, "table", "sNoiseTable", 60, noise);
    write_list(f, "table", "sCgb3Vol", 16, cgb3);
    write_list(f, "table", "sUnk_808cc4d", 19, divisor);
    write_list(f, "table", "sNativeSampleRate", 13, rate);
    write_list(f, "table", "sMusicPitchData", 13, pitch);
    write_list(f, "table", "sPcmSamplesPerVBlankTable", 13, per_frame);
    write_list(f, "table", "sArray_808cce2", 512, clamp);
    write_list(f, "table", "sArray_808cee2", 10, one);
    write_list(f, "window", "sUnk_808cc4d", 129, window);
    fprintf(f, "alias\t5\t1\ndefault\t1\n");
    fclose(f);
    return true;
}

/* Render `frames` GBA frames at 48 kHz; the peak amplitude and the number
 * of sign changes of the left channel (and its silent samples). */
static int silent_samples;
static void render(int frames, int *peak, int *crossings) {
    int16_t buffer[2048];
    int previous = 0;
    *peak = 0;
    *crossings = 0;
    silent_samples = 0;
    for (int frame = 0; frame < frames; ++frame) {
        mzm_audio_update();
        int count = mzm_audio_render(buffer, 1024, 48000);
        for (int i = 0; i < count; ++i) {
            int v = buffer[i * 2];
            if (v == 0) silent_samples++;
            if (abs(v) > *peak) *peak = abs(v);
            int sign = v > 0 ? 1 : v < 0 ? -1 : 0;
            if (sign && previous && sign != previous) (*crossings)++;
            if (sign) previous = sign;
        }
    }
}

static void test_psg_square(void) {
    GbaPsg psg;
    gba_psg_reset(&psg);
    gba_psg_write8(&psg, 0x04000084, 0x80);
    gba_psg_write16(&psg, 0x04000080, 0x2277);     /* square 2 on both sides */
    gba_psg_write16(&psg, 0x04000082, 0x0002);
    gba_psg_write16(&psg, 0x04000068, 0xF080);     /* duty 50%, volume 15 */
    /* 131072 / (2048 - 1792) = 512 Hz. */
    gba_psg_write16(&psg, 0x0400006C, 0x8000 | 1792);
    int changes = 0, previous = 0, peak = 0;
    for (int i = 0; i < 4194; ++i) {               /* 1000 steps of 1 ms / 1000 */
        gba_psg_step(&psg, 1000);
        int left, right;
        gba_psg_mix(&psg, 0, 0, &left, &right);
        CHECK(left == right);
        if (abs(left) > peak) peak = abs(left);
        int sign = left > 0 ? 1 : left < 0 ? -1 : 0;
        if (sign && previous && sign != previous) changes++;
        if (sign) previous = sign;
    }
    CHECK(peak > 0);
    /* About one second: 512 periods, two sign changes each. */
    CHECK(changes > 1000 && changes < 1050);
    /* SOUNDCNT_X off silences everything. */
    gba_psg_write8(&psg, 0x04000084, 0);
    int left, right;
    gba_psg_mix(&psg, 100, 100, &left, &right);
    CHECK(left == 0 && right == 0);
}

static void test_engine(void) {
    const char *bank_path = "mzm_audio_test_bank.bin", *tables_path = "mzm_audio_test_tables.tsv";
    build_bank();
    CHECK(write_files(bank_path, tables_path));
    CHECK(mzm_audio_open(bank_path, tables_path));
    CHECK(mzm_audio_ready());
    mzm_audio_initialize();

    int peak, crossings;
    render(4, &peak, &crossings);
    CHECK(peak == 0);

    /* The song: one DirectSound channel at the native rate. */
    mzm_music_play(1, 0);
    CHECK(mzm_music_current() == 1);
    mzm_audio_update();
    CHECK(mzm_audio_player_playing(0));
    CHECK(mzm_audio_active_channels() == 1);
    /* The FIFO lags the mixing by two frames of buffers. */
    render(3, &peak, &crossings);
    render(18, &peak, &crossings);
    CHECK(peak > 3000);
    /* The DAC holds the last FIFO sample across frames: no silent samples
     * at each frame's start (they buzzed at 60 Hz). */
    CHECK(silent_samples == 0);
    /* 18 frames of 4.48 ms... a 13379 / 64 Hz square: ~62.9 periods. */
    double seconds = 18 * 280896.0 / 16777216.0;
    int expected = (int)(seconds * 13379.0 / 64.0 * 2.0);
    CHECK(abs(crossings - expected) <= 3);

    /* The gate ends the note at tick 24, FINE stops the player at tick 25. */
    render(10, &peak, &crossings);
    CHECK(!mzm_audio_player_playing(0));
    CHECK(mzm_audio_active_channels() == 0);
    render(4, &peak, &crossings);
    CHECK(peak == 0);

    /* A sound effect on a PSG channel. */
    mzm_sound_play(2);
    render(1, &peak, &crossings);
    CHECK(mzm_audio_player_playing(3));
    render(6, &peak, &crossings);
    CHECK(peak > 0);
    mzm_sound_stop(2);
    render(1, &peak, &crossings);
    CHECK(!mzm_audio_player_playing(3));
    render(2, &peak, &crossings);
    CHECK(peak == 0);

    /* DecreaseMusicVolume (the pause) plays SOUND_63 (0x63) and keeps the
     * room's music; ResetMusicVolume leaves it playing. */
    mzm_music_play(3, 0);
    render(2, &peak, &crossings);
    mzm_music_decrease_volume();
    render(1, &peak, &crossings);
    CHECK(mzm_audio_player_playing(3));
    CHECK(!mzm_audio_player_playing(2));
    CHECK(mzm_audio_player_playing(0));
    mzm_music_reset_volume();
    render(4, &peak, &crossings);
    CHECK(mzm_audio_player_playing(0));
    mzm_sound_stop(0x63);
    mzm_audio_stop_all();
    render(2, &peak, &crossings);

    /* CheckSetNewMusicTrack: the alias maps 5 to song 1, which differs
     * from the looping song 3: a 30 frame fade, then
     * CheckPlayTransitionMusicTrack starts song 1. */
    mzm_music_play(3, 0);
    render(40, &peak, &crossings);
    CHECK(mzm_audio_player_playing(0));
    mzm_music_check_set_new_track(3);
    render(2, &peak, &crossings);
    CHECK(mzm_audio_player_playing(0));
    mzm_music_check_set_new_track(5);
    render(28, &peak, &crossings);
    CHECK(mzm_audio_player_playing(0));
    CHECK(peak > 0);
    render(4, &peak, &crossings);
    CHECK(!mzm_audio_player_playing(0));
    CHECK(mzm_music_current() == 3);
    mzm_music_check_play_transition_track();
    CHECK(mzm_music_current() == 1);
    mzm_audio_update();
    CHECK(mzm_audio_player_playing(0));
    /* InsertMusicAndQueueCurrent: the jingle stops player 3 and takes
     * player 0; its 0xB6 end replays the looping song (ReplayQueuedMusic),
     * which keeps player 0 playing past the jingle's 25 ticks. */
    mzm_audio_stop_all();
    mzm_music_play(3, 0);
    render(4, &peak, &crossings);
    mzm_sound_play(2);
    render(1, &peak, &crossings);
    CHECK(mzm_audio_player_playing(3));
    mzm_music_insert(4, 0);
    CHECK(!mzm_audio_player_playing(3));
    CHECK(mzm_audio_player_playing(0));
    CHECK(mzm_music_current() == 3);
    render(40, &peak, &crossings);
    CHECK(mzm_audio_player_playing(0));
    CHECK(peak > 0);
    /* Without a queued music, the jingle alone ends. */
    mzm_audio_stop_all();
    mzm_music_play(4, 0);
    render(40, &peak, &crossings);
    CHECK(!mzm_audio_player_playing(0));

    /* BackupTrackData2SoundChannels releases player 2's channel and
     * RetrieveTrackData2SoundChannels gives it back. */
    mzm_audio_stop_all();
    mzm_sound_play(5);
    render(2, &peak, &crossings);
    CHECK(mzm_audio_player_playing(2));
    CHECK(mzm_audio_active_channels() == 1);
    mzm_audio_backup_track2();
    CHECK(mzm_audio_active_channels() == 0);
    mzm_audio_backup_track2();
    mzm_audio_retrieve_track2();
    CHECK(mzm_audio_active_channels() == 1);
    mzm_audio_retrieve_track2();
    CHECK(mzm_audio_active_channels() == 1);

    /* A room without music falls back to the default track. */
    mzm_audio_stop_all();
    mzm_music_check_set_new_track(0);
    mzm_music_check_play_transition_track();
    CHECK(mzm_music_current() == 1);
    /* Under the alarm priority (0x40) a room's track neither fades nor
     * starts on the transition (unk_378c, unk_38d8). */
    mzm_audio_stop_all();
    mzm_music_play(3, 0x40);
    mzm_music_check_set_new_track(5);
    render(40, &peak, &crossings);
    CHECK(mzm_audio_player_playing(0));
    mzm_music_check_play_transition_track();
    CHECK(mzm_music_current() == 3);

    mzm_audio_close();
    CHECK(!mzm_audio_ready());
    remove(bank_path);
    remove(tables_path);
}

int main(void) {
    test_psg_square();
    test_engine();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    puts("mzm_audio tests passed");
    return 0;
}
