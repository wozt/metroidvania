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

/* gEwramData + 0x60 unk_60 word, mirrored from the save's
 * progress_flags by the entity-update fill path (same channel as
 * object20_ewram_word60). sub_08013960 picks sUnk_084F1086 only when
 * currentCharacter != 0 AND this word's bit 1 is set. */
static uint32_t room_music_progress_flags;

void aos_room_music_set_progress(uint32_t progress_flags) {
    room_music_progress_flags = progress_flags;
}

/* sub_08013960_inline_0: a new song stops the previous one. */
bool aos_room_music_change(AosRoomMusic *music, uint16_t song) {
    if (!music) return false;
    if (music->current_song == song) return true;
    if (music->current_song) aos_audio_play_song((uint16_t)(music->current_song | STOP_SONG));
    music->current_song = song;
    aos_audio_play_song(song);
    return false;
}

bool aos_room_music_enter(AosRoomMusic *music, int area, int room,
                          const AosRoomMusicFlags *flags) {
    if (music->boss_mode) return aos_room_music_change(music, 0x14);
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
    /* sub_08013960: (currentCharacter == 0) || !(unk_60 & 2) picks
     * sUnk_084F106C; otherwise sUnk_084F1086. music->julius carries
     * currentCharacter != 0; the bit-1 conjunct comes from the
     * published progress word. */
    uint16_t song = (music->julius && (room_music_progress_flags & 2u))
                        ? julius_songs[area]
                        : soma_songs[area];
    music->special_song = 0;
    if (area == 4 && room == 0xA) song = music->special_song = 0x2A;
    if (!music->special_song && area == 7 && room == 0x21) song = music->special_song = 0x27;
    if (!music->special_song && !(music->events_37e & 0x20) && area == 7 &&
        (room == 0x22 || room == 0x23 || room == 0x24 || room == 0x29 || room == 0x10))
        song = music->special_song = 0x29;
    if (!music->special_song && (music->events_37e & 4) && area == 5 &&
        (room == 6 || room == 0xE))
        song = music->special_song = 0x25;
    /* Native room loads run the transition teardown first:
     * sub_0806C354's sub_08013D30/sub_08013F38 release the held sounds
     * and stop the +0x254CC ambient song, so the area song owns its
     * player when sub_08013960_inline_0 runs. The port's room swap is
     * instant and has no transition stages; an untracked song still
     * occupying the target player's slot is the ambient the teardown
     * would have stopped (the opening's Object20 guard plants 0x26
     * through sub_08013EEC). Issue its STOP_SONG and, when the stale
     * tracker claimed that same song was current, re-arm the change so
     * the area song actually restarts instead of early-returning. */
    AosAudioSongInfo target;
    if (aos_audio_song_info(song, &target)) {
        const AosAudioPlayerState *player =
            &aos_audio_state()->players[target.player];
        if (player->active && player->song != song &&
            player->song != music->current_song) {
            aos_audio_play_song((uint16_t)(player->song | STOP_SONG));
            if (music->current_song == song) music->current_song = 0;
        }
    }
    bool playing = aos_room_music_change(music, song);
    if (!playing && music->stop_player_0) aos_audio_play_song(MUSIC_PLAYER_STOP_0);
    return playing;
}
