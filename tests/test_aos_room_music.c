/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/core/aos_audio.h"
#include "../src/runtime/aos/core/aos_room_music.h"

static const AosRoomMusicFlags plain = {false, false, false};

int main(void) {
    aos_audio_reset();
    aos_audio_clear();
    AosRoomMusic music = {0};
    assert(!aos_room_music_change(&music, 28));
    assert(music.current_song == 28);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 28) == 1);
    assert(!aos_room_music_change(&music, 16));
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 28) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 16) == 1);
    assert(aos_room_music_change(&music, 16));
    aos_audio_clear();
    music = (AosRoomMusic){0};
    /* Castle Corridor (area 0): loud, then its song 2. */
    assert(!aos_room_music_enter(&music, 0, 3, &plain));
    assert(aos_audio_count() == 2 && aos_audio_requested(AOS_AUDIO_COMMAND, 0x1011));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 2) && music.current_song == 2);
    /* Another room of the area keeps it playing. */
    aos_audio_clear();
    assert(aos_room_music_enter(&music, 0, 4, &plain));
    assert(aos_audio_count() == 1 && !aos_audio_requested(AOS_AUDIO_START_SONG, 2));
    /* A save room only quiets it. */
    aos_audio_clear();
    const AosRoomMusicFlags save = {true, false, false};
    assert(!aos_room_music_enter(&music, 0, 5, &save));
    assert(aos_audio_count() == 1 && aos_audio_requested(AOS_AUDIO_COMMAND, 0x1012));
    assert(music.current_song == 2);
    /* Chapel (area 1): the corridor's song stops, song 8 starts. */
    aos_audio_clear();
    assert(!aos_room_music_enter(&music, 1, 0, &plain));
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 2) &&
           aos_audio_requested(AOS_AUDIO_START_SONG, 8));
    /* A transition room mutes player 0 once and forgets the song. */
    aos_audio_clear();
    const AosRoomMusicFlags corridor = {false, false, true};
    assert(!aos_room_music_enter(&music, 1, 9, &corridor));
    assert(aos_audio_requested(AOS_AUDIO_COMMAND, 0x1040) && music.current_song == 0);
    aos_audio_clear();
    aos_room_music_enter(&music, 1, 9, &corridor);
    assert(aos_audio_count() == 0);
    /* Special rooms: area 4 room 0xA, area 7's rooms before event bit 5. */
    aos_audio_clear();
    aos_room_music_enter(&music, 4, 0xA, &plain);
    assert(music.current_song == 0x2A && music.special_song == 0x2A);
    aos_room_music_enter(&music, 7, 0x23, &plain);
    assert(music.current_song == 0x29);
    music.events_37e = 0x20;
    aos_room_music_enter(&music, 7, 0x23, &plain);
    assert(music.current_song == 3);
    /* Julius plays the other table only with the cleared-game
     * progress bit (sub_08013960's currentCharacter != 0 && unk_60
     * & 2 conjunct); the boss mode plays song 0x14. */
    music.julius = true;
    aos_room_music_enter(&music, 0, 3, &plain);
    assert(music.current_song == 2);   /* bit 1 clear: Soma table */
    aos_room_music_set_progress(2);
    aos_room_music_enter(&music, 0, 3, &plain);
    assert(music.current_song == 20);
    aos_room_music_set_progress(0);
    music.julius = false;
    music.boss_mode = true;
    aos_room_music_enter(&music, 5, 0, &save);
    assert(music.current_song == 0x14);
    /* A room enter whose tracked song is not the one playing re-issues
     * it: the +0x254CC ambient (here song 38 on the same player, the
     * opening guard's sub_08013EEC(0x26)) occupied player 0 after the
     * tracker claimed song 2 was current. The enter stops the foreign
     * song like the native transition teardown (sub_08013F38) and
     * restarts the area song; a genuinely playing song is left alone. */
    aos_audio_reset();
    AosAudioSongInfo ambient_table[39] = {0};
    ambient_table[2] = (AosAudioSongInfo){0, 50, 0};
    ambient_table[38] = (AosAudioSongInfo){0, 100, 0};
    assert(aos_audio_set_song_table(ambient_table, 39));
    AosRoomMusic stale = {0};
    assert(!aos_room_music_change(&stale, 2));
    aos_audio_play_song(38);
    assert(aos_audio_state()->players[0].song == 38);
    aos_audio_clear();
    assert(!aos_room_music_enter(&stale, 0, 3, &plain));
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 38) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 2) == 1);
    assert(stale.current_song == 2);
    assert(aos_audio_state()->players[0].song == 2);
    aos_audio_clear();
    assert(aos_room_music_enter(&stale, 0, 4, &plain));
    assert(aos_audio_count() == 1 &&
           aos_audio_requested(AOS_AUDIO_COMMAND, 0x1011));

    /* sub_08013EEC's tracker reset, ported as aos_audio_ambient_set:
     * the ambient song plants +0x254CC and the room enter's
     * sub_08013F38 teardown stops it before the area song resumes. */
    aos_audio_reset();
    assert(aos_audio_set_song_table(ambient_table, 39));
    AosRoomMusic ambient_music = {0};
    assert(!aos_room_music_change(&ambient_music, 2));
    aos_audio_ambient_set(38);
    assert(aos_audio_ambient() == 38);
    assert(aos_audio_state()->players[0].song == 38);
    aos_audio_clear();
    assert(!aos_room_music_enter(&ambient_music, 0, 3, &plain));
    assert(aos_audio_ambient() == 0);
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 38) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 2) == 1);
    assert(ambient_music.current_song == 2);
    assert(aos_audio_state()->players[0].song == 2);

    /* A stale tracker word is re-armed by aos_room_music_change itself:
     * current_song claims the incoming song while its player runs a
     * foreign song — the change stops the occupant and restarts. */
    aos_audio_reset();
    assert(aos_audio_set_song_table(ambient_table, 39));
    AosRoomMusic tracked = {0};
    assert(!aos_room_music_change(&tracked, 2));
    aos_audio_play_song(38);           /* foreign PlaySong bypasses the tracker */
    aos_audio_clear();
    assert(!aos_room_music_change(&tracked, 2));
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 38) == 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 2) == 1);
    assert(tracked.current_song == 2);
    assert(aos_audio_state()->players[0].song == 2);

    /* A genuinely playing tracked song still early-outs. */
    aos_audio_clear();
    assert(aos_room_music_change(&tracked, 2));
    assert(aos_audio_count() == 0);
    puts("aos room music tests passed");
    return 0;
}
