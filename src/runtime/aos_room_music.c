/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_room_music.h"

#include "aos_audio.h"

enum {
    SONG_LOUD = 0x1011,
    SONG_QUIET = 0x1012,
    MUSIC_PLAYER_MUTE_0 = 0x1040,   /* sub_08013F6C(0x1040) */
    STOP_SONG = 0x8000,
    MUSIC_PLAYER_STOP_0 = 0x1030,
    AREA_COUNT = 13,
};

/* sUnk_084F106C and sUnk_084F1086: each area's song for Soma and Julius. */
static const uint16_t soma_songs[AREA_COUNT] = {2, 8, 7, 5, 6, 9, 1, 3, 10, 4, 11, 12, 12};
static const uint16_t julius_songs[AREA_COUNT] = {20, 8, 7, 5, 6, 9, 2, 3, 10, 4, 11, 12, 12};

/* sub_08013960_inline_0: a new song stops the previous one. */
static bool change(AosRoomMusic *music, uint16_t song) {
    if (music->current_song == song) return true;
    if (music->current_song) aos_audio_play_song((uint16_t)(music->current_song | STOP_SONG));
    music->current_song = song;
    aos_audio_play_song(song);
    return false;
}

bool aos_room_music_enter(AosRoomMusic *music, int area, int room,
                          const AosRoomMusicFlags *flags) {
    if (music->boss_mode) return change(music, 0x14);
    if (flags && (flags->save || flags->warp)) {
        aos_audio_play_song(SONG_QUIET);
        return false;
    }
    if (flags && flags->transition) {
        if (!music->fade_requested) {
            music->fade_requested = true;
            aos_audio_play_song(MUSIC_PLAYER_MUTE_0);
        }
        music->current_song = 0;
        return false;
    }
    aos_audio_play_song(SONG_LOUD);
    if (area < 0 || area >= AREA_COUNT) return false;
    uint16_t song = music->julius ? julius_songs[area] : soma_songs[area];
    music->special_song = 0;
    if (area == 4 && room == 0xA) song = music->special_song = 0x2A;
    if (!music->special_song && area == 7 && room == 0x21) song = music->special_song = 0x27;
    if (!music->special_song && !(music->events_37e & 0x20) && area == 7 &&
        (room == 0x22 || room == 0x23 || room == 0x24 || room == 0x29 || room == 0x10))
        song = music->special_song = 0x29;
    if (!music->special_song && (music->events_37e & 4) && area == 5 &&
        (room == 6 || room == 0xE))
        song = music->special_song = 0x25;
    bool playing = change(music, song);
    if (!playing && music->stop_player_0) aos_audio_play_song(MUSIC_PLAYER_STOP_0);
    return playing;
}
