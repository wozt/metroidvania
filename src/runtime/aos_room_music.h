/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_ROOM_MUSIC_H
#define AOS_ROOM_MUSIC_H
#include <stdbool.h>
#include <stdint.h>

/* A room's inputs to sub_08013960 (metadata/room_music.tsv). */
typedef struct {
    bool save, warp, transition;
} AosRoomMusicFlags;

/* gEwramData->unk_60's music fields. */
typedef struct {
    uint16_t current_song;      /* currentSong */
    bool fade_requested;        /* unk_42C bit 0x40000 (sub_08013F6C) */
    bool boss_mode;             /* unk_4CC_0: song 0x14 */
    bool julius;                /* currentCharacter != 0 with unk_60 bit 1 */
    uint8_t events_37e;         /* unk_37E: bits 2 and 5 pick special songs */
    bool stop_player_0;         /* unk_4BE != 0 */
    uint16_t special_song;      /* unk_254CE */
} AosRoomMusic;

/* sub_08013960 for the room at (area, room), sending its PlaySong requests
 * through aos_audio_play_song: save and warp rooms only quiet the music,
 * transition rooms mute player 0 once (0x1040) and forget the song, others
 * set it loud and play the area's song (sUnk_084F106C, or sUnk_084F1086 for
 * Julius) or a room's special song, stopping the previous one. Returns
 * whether the song was already playing. */
bool aos_room_music_enter(AosRoomMusic *music, int area, int room,
                          const AosRoomMusicFlags *flags);
#endif
