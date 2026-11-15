/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_IN_GAME_MODE_H
#define AOS_IN_GAME_MODE_H

#include <stdbool.h>
#include <stdint.h>

/* gEwramData unk_60.unk_64/+0x65/+0x66: the dispatch state of
 * GameModeInGameUpdate (sub_0800B700, third_party/cvaos
 * src/code_0800B700.c). While the game mode is IN_GAME (or the
 * intro-cutscene return, mode 4) the update switches on unk_64;
 * each case owns a stage counter unk_65 (and the shared unk_66
 * timer of the cutscene cases 11-14). Several cases park a
 * priority-1 SetPlayerInput(1, 0) override in inputData every
 * frame, so playerHeldInput/playerNewInput read 0 regardless of
 * the pad while the global heldInput/newInput the entity pass
 * reads stay untouched.
 *
 * This module is the host-side state channel for that dispatch:
 * the room runtime latches the case its ported counterpart owns
 * (the sub_080104EC transition machine -> 3, the SELECT quick map
 * -> 4, the START soul menu -> 6) and queries the lock predicate
 * once per frame, mirroring the native per-frame SetPlayerInput
 * call through aos_input_set_player. The stage machines
 * themselves stay with their owners (room_transition,
 * quick_map_bldY, soul_menu); unk_65 here records the native
 * stage numbering for traceability, not a second implementation. */
typedef struct {
    uint8_t unk_64;     /* gEwramData + 0x64: dispatch case */
    uint8_t unk_65;     /* gEwramData + 0x65: stage within the case */
    uint8_t unk_66;     /* gEwramData + 0x66: shared counter */
    bool unk_4cc_6;     /* unk_60.unk_4CC_6: gates cases 1/3/11 entity updates */
} AosInGameMode;

/* Native unk_64 case numbers (sub_0800B700 switch). */
enum {
    AOS_INGAME_BOOT_FADE = 0,       /* case 0: power-on fade stages */
    AOS_INGAME_PLAY = 1,            /* case 1: normal in-game update */
    AOS_INGAME_DEATH = 2,           /* case 2: sub_0800C8C8 death/game over */
    AOS_INGAME_ROOM_TRANSITION = 3, /* case 3: sub_080104EC (unk_65 0-3) */
    AOS_INGAME_QUICK_MAP = 4,       /* case 4: sub_080110E4 (SELECT map) */
    AOS_INGAME_TRANSITION_FADE = 5, /* case 5: sub_080106DC fades+camera */
    AOS_INGAME_SOUL_MENU = 6,       /* case 6: sub_08047764 (START menu) */
    AOS_INGAME_MAP_SECOND = 7,      /* case 7: sub_080116A8/sub_08011AD0 */
    AOS_INGAME_ROOM_WARP = 8,       /* case 8: sub_08011B2C map warp */
    AOS_INGAME_CUTSCENE_A = 9,      /* case 9: sub_0805AFC0 */
    AOS_INGAME_CREDITS = 10,        /* case 10: sub_0800CA98 -> CREDITS */
    AOS_INGAME_CUTSCENE_B = 11      /* cases 11-14: unk_66 camera modes */
};

/* Fresh in-game state: unk_64 = 1 (normal play). Native room init
 * clears unk_64 = unk_65 = 0 and the boot fade (case 0) walks it to
 * 1; a room loaded mid-game starts directly in play. */
void aos_in_game_mode_init(AosInGameMode *mode);

/* Latch a new case like the native writers do: sub_08010244 ->
 * (3, 0), sub_08011338 -> (4, 0), sub_08010978 -> (5, 0),
 * sub_080115D4 -> (7, 0), sub_08011AD0 -> (8, 0); every writer
 * pairs the case with unk_65 = 0. unk_66 is only cleared by
 * sub_08010978 natively — pass clear_counter for that case. */
void aos_in_game_mode_enter(AosInGameMode *mode, uint8_t unk_64,
                            bool clear_counter);

/* The `unk_64 = 1` tail every case runs when its routine returns
 * nonzero (sub_080104EC, sub_080110E4, sub_080106DC, sub_08047764,
 * sub_08011B2C, ...). Native leaves unk_65 untouched; the next
 * enter() rewrites it. */
void aos_in_game_mode_resume(AosInGameMode *mode);

/* True on frames where GameModeInGameUpdate issues
 * SetPlayerInput(1, 0): the boot-fade stages unk_65 1 and 3 of
 * case 0, case 3 only while unk_4CC_6 holds (the entity-update
 * gate; its writer is unported so the runtime keeps it clear),
 * and every frame of cases 4 (sub_080110E4), 5 (sub_080106DC
 * calls it internally) and 6 (sub_08047764). Cases 7/8 issue no
 * SetPlayerInput in the decompiled source; cases 2 and 9-14
 * suspend the player update through the mode switch itself. */
bool aos_in_game_mode_locks_input(const AosInGameMode *mode);

/* Diagnostic case label for logs; "unknown" outside 0-14. */
const char *aos_in_game_mode_case_name(uint8_t unk_64);

#endif /* AOS_IN_GAME_MODE_H */
