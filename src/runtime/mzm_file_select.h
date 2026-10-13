/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission file select screen (menus/file_select.c, US version):
 * the Chozo background and the menu, the three files with their energy,
 * timer, difficulty and area, the cursor and file markers, the file's
 * "Start game" panel, the continue / new game choice of a completed file,
 * the difficulty panel, the start animation, the panels' texts (drawn with
 * the native font), the fades, and the copy and erase menus (the files
 * themselves are copied and deleted through callbacks). The corrupted
 * files, the options and the time attack are not ported: the options entry
 * stays but does nothing. */
#ifndef MZM_FILE_SELECT_H
#define MZM_FILE_SELECT_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_screen.h"
#include "mzm_text.h"

/* struct SaveFileInfo: what the screen shows of a file. */
typedef struct {
    bool exists, corrupted;
    uint8_t area;                   /* currentArea */
    uint16_t energy, max_energy, missiles, max_missiles;
    uint8_t suit;                   /* SuitType */
    uint8_t igt_hours, igt_minutes, igt_seconds;
    uint8_t completed_game;         /* completedGame flags */
    bool intro_played;
    uint8_t difficulty;             /* Difficulty: 0 easy, 1 normal, 2 hard */
    bool time_attack;
} MzmSaveFileInfo;

/* What the screen leaves for. */
enum {
    MZM_FILE_SELECT_STAY = 0,
    MZM_FILE_SELECT_CONTINUE,       /* gSubGameMode2 1: the file is loaded */
    MZM_FILE_SELECT_NEW_GAME,       /* gSubGameMode2 2: a new game on the file */
    MZM_FILE_SELECT_TITLE,          /* gSubGameMode2 3: out to the intro */
};

typedef struct {
    uint16_t dispcnt, bldcnt, bg0cnt, bg1cnt, bg2cnt, bg3cnt;
    uint16_t unk_14, unk_16, unk_18, unk_1a, unk_1c, unk_1e, unk_20;
    MzmMenuOam oam[15];             /* fileScreenOam */
    MzmSaveFileInfo files[3];       /* gSaveFilesInfo */
    struct {
        uint8_t confirm_copy, confirm_erase, start_game, completed_file_options;
        uint8_t confirm_overwriting_completed, japanese_text, difficulty;
    } cursors;                      /* fileSelectCursors */
    uint8_t process_text_stage;
    uint8_t message_queue[3];       /* messageInfoIdQueue */
    uint8_t difficulty_message, continue_new_message, unk_3a, enabled_menus;
    uint16_t timer;
    uint8_t current_sub_menu, cursor_position, sub_menu_stage;
    uint16_t sub_menu_timer;
    uint8_t unk_24;
    struct {
        uint8_t stage, color, intensity, frequency;
        uint16_t timer;
        bool palette_updated;
    } fading;
    struct { uint8_t delay, set; } maintained;   /* gMaintainedInputData */
    MzmMessage message;             /* gCurrentMessage */
    uint8_t most_recent;            /* gMostRecentSaveFile */
    uint8_t sub_mode1, sub_mode2;   /* gSubGameMode1 / gSubGameMode2 */
    uint8_t copy_source, current_file, erase_file;   /* copySourceFile, currentFile, eraseFile */
    /* SramCopyFile / SramDeleteFile on the game's files (NULL: only the
     * screen's info changes); they return false on failure. */
    bool (*copy_file)(void *context, int source, int destination);
    bool (*erase_file_callback)(void *context, int file);
    void *context;
} MzmFileSelect;

/* gSubGameMode1 = 0 with the files' info and gMostRecentSaveFile; the
 * callbacks are set afterwards and kept. */
void mzm_file_select_enter(MzmFileSelect *f, const MzmSaveFileInfo files[3], uint8_t most_recent);
/* FileSelectMenuHandler then FileSelectVBlank for one frame; returns
 * MZM_FILE_SELECT_*. `most_recent` then names the chosen file and
 * `cursors.difficulty` the new game's difficulty. */
int mzm_file_select_update(MzmFileSelect *f, MzmScreen *s);

#endif
