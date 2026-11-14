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
    /* unk_1325C.currentCharacter != 0 (Julius mode). The alternate
     * sUnk_084F1086 table additionally requires the gEwramData + 0x60
     * word's bit 1, which sub_08013960 AND-s in from the progress
     * word published through aos_room_music_set_progress — the port
     * has no Julius mode, so this stays false today. */
    bool julius;
    uint8_t events_37e;         /* unk_37E: bits 2 and 5 pick special songs */
    bool stop_player_0;         /* unk_4BE != 0 */
    uint16_t special_song;      /* unk_254CE */
} AosRoomMusic;

/* Publish the gEwramData + 0x60 progress-flags word (the portable
 * save's AosSaveData.progress_flags / object20_ewram_word60 channel)
 * for sub_08013960's cleared-game conjunct: bit 1 selects
 * sUnk_084F1086 over sUnk_084F106C when music->julius is set. The
 * native word lives in SRAM slot 6; the runtime mirrors it through
 * the entity-update fill path, so the setter is called once per
 * object20_fill_in with the synced value. */
void aos_room_music_set_progress(uint32_t progress_flags);

/* sub_08013CF0: replace the tracked music while preserving PlaySong's
 * stop-before-start ordering. Event controllers use this before room music
 * resumes through aos_room_music_enter. */
bool aos_room_music_change(AosRoomMusic *music, uint16_t song);

/* sub_08013960 for the room at (area, room), sending its PlaySong requests
 * through aos_audio_play_song: save and warp rooms only quiet the music,
 * transition rooms mute player 0 once (0x1040) and forget the song, others
 * set it loud and play the area's song (sUnk_084F106C, or sUnk_084F1086
 * when julius is set and the published progress word has bit 1) or a
 * room's special song, stopping the previous one. Returns
 * whether the song was already playing. */
bool aos_room_music_enter(AosRoomMusic *music, int area, int room,
                          const AosRoomMusicFlags *flags);
#endif
