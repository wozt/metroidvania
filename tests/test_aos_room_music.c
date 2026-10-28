/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_audio.h"
#include "../src/runtime/aos_room_music.h"

static const AosRoomMusicFlags plain = {false, false, false};

int main(void) {
    aos_audio_reset();
    aos_audio_clear();
    AosRoomMusic music = {0};
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
    /* Julius plays the other table; the boss mode plays song 0x14. */
    music.julius = true;
    aos_room_music_enter(&music, 0, 3, &plain);
    assert(music.current_song == 20);
    music.boss_mode = true;
    aos_room_music_enter(&music, 5, 0, &save);
    assert(music.current_song == 0x14);
    puts("aos room music tests passed");
    return 0;
}
