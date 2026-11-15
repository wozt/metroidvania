/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_in_game_mode.h"

void aos_in_game_mode_init(AosInGameMode *mode) {
    mode->unk_64 = AOS_INGAME_PLAY;
    mode->unk_65 = 0;
    mode->unk_66 = 0;
    mode->unk_4cc_6 = false;
}

void aos_in_game_mode_enter(AosInGameMode *mode, uint8_t unk_64,
                            bool clear_counter) {
    mode->unk_64 = unk_64;
    mode->unk_65 = 0;
    if (clear_counter) mode->unk_66 = 0;
}

void aos_in_game_mode_resume(AosInGameMode *mode) {
    /* Native tails write only unk_64 = 1 (e.g. sub_080104EC's return
     * path); unk_65 keeps its last stage until the next enter(). */
    mode->unk_64 = AOS_INGAME_PLAY;
}

bool aos_in_game_mode_locks_input(const AosInGameMode *mode) {
    switch (mode->unk_64) {
    case AOS_INGAME_BOOT_FADE:
        /* Case 0 stages 1 and 3 call SetPlayerInput(1, 0); stages 0,
         * 2 and 4 do not (sub_0800B700 inline switch on unk_65). */
        return mode->unk_65 == 1 || mode->unk_65 == 3;
    case AOS_INGAME_ROOM_TRANSITION:
        /* Case 3 calls SetPlayerInput(1, 0) only while unk_4CC_6
         * holds (the cutscene/entity-update gate); the runtime has
         * no unk_4CC_6 writer, so this stays clear. */
        return mode->unk_4cc_6;
    case AOS_INGAME_QUICK_MAP:
    case AOS_INGAME_TRANSITION_FADE:
    case AOS_INGAME_SOUL_MENU:
        /* Cases 4 and 6 call SetPlayerInput(1, 0) every frame around
         * their routine; case 5's sub_080106DC calls it internally
         * on entry (code_0800F1FC.c). */
        return true;
    default:
        return false;
    }
}

const char *aos_in_game_mode_case_name(uint8_t unk_64) {
    switch (unk_64) {
    case 0: return "boot_fade";
    case 1: return "in_game";
    case 2: return "death";
    case 3: return "room_transition";
    case 4: return "quick_map";
    case 5: return "transition_fade";
    case 6: return "soul_menu";
    case 7: return "map_second";
    case 8: return "room_warp";
    case 9: return "cutscene_a";
    case 10: return "credits";
    case 11: case 12: case 13: case 14: return "cutscene";
    default: return "unknown";
    }
}
