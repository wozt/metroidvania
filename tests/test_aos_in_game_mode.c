/* SPDX-License-Identifier: GPL-3.0-only */
/* GameModeInGameUpdate (sub_0800B700) unk_64/unk_65 dispatch channel:
 * case latching, the per-frame SetPlayerInput(1, 0) predicate and the
 * stage numbering the runtime counterparts report. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/runtime/aos/game/aos_in_game_mode.h"

int main(void) {
    AosInGameMode mode;

    /* Fresh state is normal play: no input lock. */
    aos_in_game_mode_init(&mode);
    assert(mode.unk_64 == AOS_INGAME_PLAY && mode.unk_65 == 0);
    assert(!aos_in_game_mode_locks_input(&mode));

    /* Case 1 (in-game) and case 2 (death, sub_0800C8C8) never park the
     * override in the dispatch itself. */
    aos_in_game_mode_enter(&mode, AOS_INGAME_DEATH, false);
    assert(mode.unk_64 == 2 && mode.unk_65 == 0 && !mode.unk_66);
    assert(!aos_in_game_mode_locks_input(&mode));

    /* Case 3 (sub_080104EC): locked only while unk_4CC_6 holds — the
     * runtime keeps the gate clear, so the transition stages leave
     * player input alone (the suspended update does the masking). */
    aos_in_game_mode_enter(&mode, AOS_INGAME_ROOM_TRANSITION, false);
    assert(mode.unk_64 == 3 && mode.unk_65 == 0);
    assert(!aos_in_game_mode_locks_input(&mode));
    mode.unk_65 = 2;
    assert(!aos_in_game_mode_locks_input(&mode));
    mode.unk_4cc_6 = true;
    assert(aos_in_game_mode_locks_input(&mode));
    mode.unk_4cc_6 = false;

    /* Case 0 boot fade: stages 1 and 3 lock, 0/2/4 do not. */
    aos_in_game_mode_enter(&mode, AOS_INGAME_BOOT_FADE, false);
    for (int stage = 0; stage <= 4; ++stage) {
        mode.unk_65 = (uint8_t)stage;
        assert(aos_in_game_mode_locks_input(&mode) ==
               (stage == 1 || stage == 3));
    }

    /* Cases 4 (sub_080110E4 quick map), 5 (sub_080106DC) and 6
     * (sub_08047764 soul menu) lock every frame regardless of stage. */
    static const uint8_t locked[] = {4, 5, 6};
    for (unsigned i = 0; i < sizeof locked; ++i) {
        aos_in_game_mode_enter(&mode, locked[i], i == 1);
        assert(mode.unk_64 == locked[i] && mode.unk_65 == 0);
        for (int stage = 0; stage <= 8; ++stage) {
            mode.unk_65 = (uint8_t)stage;
            assert(aos_in_game_mode_locks_input(&mode));
        }
    }
    /* Only sub_08010978 (case 5) clears unk_66 natively. */
    mode.unk_66 = 7;
    aos_in_game_mode_enter(&mode, AOS_INGAME_QUICK_MAP, false);
    assert(mode.unk_66 == 7);
    aos_in_game_mode_enter(&mode, AOS_INGAME_TRANSITION_FADE, true);
    assert(mode.unk_66 == 0);

    /* Cases 7/8 (map warp chain) issue no SetPlayerInput. */
    aos_in_game_mode_enter(&mode, AOS_INGAME_MAP_SECOND, false);
    assert(!aos_in_game_mode_locks_input(&mode));
    aos_in_game_mode_enter(&mode, AOS_INGAME_ROOM_WARP, false);
    assert(!aos_in_game_mode_locks_input(&mode));

    /* The case tails write only unk_64 = 1; unk_65 keeps its stage. */
    mode.unk_65 = 3;
    aos_in_game_mode_resume(&mode);
    assert(mode.unk_64 == AOS_INGAME_PLAY && mode.unk_65 == 3);

    /* Case labels cover the documented range. */
    assert(strcmp(aos_in_game_mode_case_name(3), "room_transition") == 0);
    assert(strcmp(aos_in_game_mode_case_name(4), "quick_map") == 0);
    assert(strcmp(aos_in_game_mode_case_name(6), "soul_menu") == 0);
    assert(strcmp(aos_in_game_mode_case_name(14), "cutscene") == 0);
    assert(strcmp(aos_in_game_mode_case_name(200), "unknown") == 0);

    puts("aos_in_game_mode: ok");
    return 0;
}
