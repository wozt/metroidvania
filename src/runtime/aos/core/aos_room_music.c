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

/* sub_08013960_inline_0 / sub_08013CF0: a new song stops the previous
 * one. The early-out trusts the +0x90 currentSong word only while it
 * still describes the player: entity PlaySong calls that bypass the
 * music helpers (an ambient sub_08013EEC song, a jingle) can leave the
 * word stale — native sub_08013EEC resets it through inline_0(0), the
 * port's entity reports do not reach AosRoomMusic. When the tracked
 * song no longer owns its player, drop the word and stop the foreign
 * occupant like the missing teardown would have, then change normally. */
bool aos_room_music_change(AosRoomMusic *music, uint16_t song) {
    if (!music) return false;
    if (music->current_song == song) {
        AosAudioSongInfo target;
        if (!aos_audio_song_info(song, &target)) return true;
        const AosAudioPlayerState *player =
            &aos_audio_state()->players[target.player];
        if (!player->active || player->song == song) return true;
        /* Stale tracker: the song's player ran something else. Stop
         * that occupant unless the transition teardown already did
         * (a paused player keeps its song id), then re-arm. */
        if (!player->paused)
            aos_audio_play_song((uint16_t)(player->song | STOP_SONG));
        music->current_song = 0;
    }
    if (music->current_song) aos_audio_play_song((uint16_t)(music->current_song | STOP_SONG));
    music->current_song = song;
    aos_audio_play_song(song);
    return false;
}

bool aos_room_music_enter(AosRoomMusic *music, int area, int room,
                          const AosRoomMusicFlags *flags) {
    /* sub_0806C354's transition teardown runs before sub_08013960 on
     * every room load: sub_08013D30 releases the held sounds and
     * sub_08013F38 stops the +0x254CC ambient song and clears the
     * slot. Both are idempotent, so re-entering a room is safe. */
    aos_audio_release_held();
    aos_audio_ambient_stop();
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
    /* The teardown above covers the +0x254CC ambient only when the
     * entity side registered it through aos_audio_ambient_set — the
     * ported Object20 reports still play it as a bare PlaySong, so an
     * untracked song can still occupy the target player's slot. The
     * occupancy scan stays as the complement: an occupant that is
     * neither the incoming song nor the tracked current song is the
     * ambient the teardown would have stopped. Issue its STOP_SONG
     * and, when the stale tracker claimed that same song was current,
     * re-arm the change so the area song actually restarts instead
     * of early-returning. */
    AosAudioSongInfo target;
    if (aos_audio_song_info(song, &target)) {
        const AosAudioPlayerState *player =
            &aos_audio_state()->players[target.player];
        if (player->active && !player->paused && player->song != song &&
            player->song != music->current_song) {
            aos_audio_play_song((uint16_t)(player->song | STOP_SONG));
            if (music->current_song == song) music->current_song = 0;
        }
    }
    bool playing = aos_room_music_change(music, song);
    if (!playing && music->stop_player_0) aos_audio_play_song(MUSIC_PLAYER_STOP_0);
    return playing;
}
