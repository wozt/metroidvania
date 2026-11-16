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

    /* sub_080106DC (case 5): the stage machine walks unk_65 0->3,
     * ramps the modeled bldY 0->16->0 on the unk_66 cadence and
     * returns 1 exactly on the frame the native routine does
     * (~95 calls: 61 ramp-up + 3 commit/settle + 31 ramp-down). */
    aos_in_game_mode_init(&mode);
    aos_in_game_mode_enter(&mode, AOS_INGAME_TRANSITION_FADE, true);
    aos_in_game_mode_transition_fade_begin(&mode);
    assert(mode.fade_bldY == 0 && mode.unk_65 == 0 && mode.unk_66 == 0);
    int fade_frames = 0;
    while (aos_in_game_mode_transition_fade_step(&mode) == 0) {
        ++fade_frames;
        assert(mode.unk_64 == AOS_INGAME_TRANSITION_FADE);
        assert(fade_frames < 1000);
    }
    assert(mode.fade_bldY == 0 && mode.unk_65 == 0);
    assert(fade_frames > 90 && fade_frames < 100);
    /* The native tail's unk_64 = 1. */
    aos_in_game_mode_resume(&mode);
    assert(mode.unk_64 == AOS_INGAME_PLAY);

    /* sub_0800FFCC area banner: armed at sub_080104EC's stage-2
     * settle end, it suspends the entity pass through unk_A074_0
     * for 189 frames (stages 1-4: 31 + 61 + 96 + 1) after one
     * stage-0 frame that still updates. */
    aos_in_game_mode_init(&mode);
    assert(mode.unk_A074_1 && !mode.unk_A074_0 && !mode.unk_42C_80);
    /* Unarmed: the step is the native early-out. */
    aos_in_game_mode_area_banner_step(&mode, false);
    assert(!mode.unk_A074_0 && mode.unk_4CD == 0);
    /* Arm for area 3 (castle cell (3<<6)|room). */
    aos_in_game_mode_arm_area_banner(&mode, 3);
    assert(mode.unk_42C_80 && mode.banner_area == 3);
    /* Out-of-range results never arm (sub_080104EC's <= 0xC check). */
    aos_in_game_mode_arm_area_banner(&mode, 13);
    assert(mode.banner_area == 3);   /* unchanged: still armed for 3 */
    aos_in_game_mode_init(&mode);
    aos_in_game_mode_arm_area_banner(&mode, 13);
    assert(!mode.unk_42C_80);
    /* Stage 0: A074_0/A074_1 flip, unk_66 = 0x3E, unk_37C bit set,
     * but this frame's entity pass already ran (the caller reads
     * unk_A074_0 before stepping). */
    aos_in_game_mode_arm_area_banner(&mode, 3);
    assert(!mode.unk_A074_0);
    aos_in_game_mode_area_banner_step(&mode, false);
    assert(mode.unk_A074_0 && !mode.unk_A074_1 && mode.unk_4CD == 1);
    assert(mode.unk_66 == 0x3E && (mode.unk_37C & (1u << 3)));
    /* Stage 1: 31 calls of unk_66 -= 2 ending in the 0x3C reload.
     * The stage-0 step already ran above; count every step whose
     * PRE-call unk_A074_0 froze a frame. */
    int frozen = 0, total = 1;   /* stage 0 already ran, unfrozen */
    while (mode.unk_4CD != 0) {
        bool was_frozen = mode.unk_A074_0;
        aos_in_game_mode_area_banner_step(&mode, false);
        ++total;
        if (was_frozen) ++frozen;
        assert(total < 1000);
    }
    /* unk_A074_0 was set by stage 0, so every step after the first
     * froze a frame: stages 1(31) + 2(61) + 3(96) + 4(1) = 189. */
    assert(total == 190 && frozen == 189);
    assert(!mode.unk_42C_80 && !mode.unk_A074_0 && mode.unk_A074_1);
    assert(mode.unk_4CD == 0 && mode.unk_66 == 0x60);
    /* The shown bit survives: a second banner for area 3 is refused
     * by sub_0800FCB8, so the runtime never re-arms it. */
    assert(aos_area_banner_area((3u << 6) | 5, false, false, false,
                                mode.unk_37C) == -1);
    /* The door-bit abort ((unk_42C & 0x101) == 1) cancels mid-run. */
    aos_in_game_mode_init(&mode);
    aos_in_game_mode_arm_area_banner(&mode, 1);
    aos_in_game_mode_area_banner_step(&mode, false);
    aos_in_game_mode_area_banner_step(&mode, true);
    assert(!mode.unk_42C_80 && mode.unk_4CD == 0 && !mode.unk_A074_0 &&
           mode.unk_A074_1);
    /* sub_08010244's prologue cancels an armed banner the same way. */
    aos_in_game_mode_arm_area_banner(&mode, 2);
    aos_in_game_mode_area_banner_cancel(&mode);
    assert(!mode.unk_42C_80 && !mode.unk_A074_0 && mode.unk_A074_1);
    /* The step only advances in case 1 like the case-1 dispatch. */
    aos_in_game_mode_enter(&mode, AOS_INGAME_QUICK_MAP, false);
    aos_in_game_mode_arm_area_banner(&mode, 4);
    aos_in_game_mode_area_banner_step(&mode, false);
    assert(mode.unk_4CD == 0 && !mode.unk_A074_0);

    /* sub_0800FCB8 picks: plain cell banners, exclusions and the
     * room-pair remaps. */
    assert(aos_area_banner_area((5u << 6) | 10, false, false, false, 0) == 5);
    assert(aos_area_banner_area(0xFFFF, false, false, false, 0) == -1);
    assert(aos_area_banner_area((2u << 6) | 4, false, false, false, 0) == -1);
    assert(aos_area_banner_area((0u << 6) | 26, false, false, false, 0) == -1);
    assert(aos_area_banner_area((5u << 6) | 10, true, false, false, 0) == -1);
    assert(aos_area_banner_area((5u << 6) | 10, false, true, false, 0) == -1);
    assert(aos_area_banner_area((5u << 6) | 10, false, false, true, 0) == -1);
    /* (13,7) remaps area 7 -> banner 12; (9,8) -> 13 fails the arm
     * range but FCB8 still returns it. */
    assert(aos_area_banner_area((7u << 6) | 13, false, false, false, 0) == 12);
    assert(aos_area_banner_area((8u << 6) | 9, false, false, false, 0) == 13);
    assert(aos_area_banner_area((8u << 6) | 10, false, false, false, 0) == 14);
    assert(aos_area_banner_area((8u << 6) | 11, false, false, false, 0) == 15);

    /* Case labels cover the documented range. */
    assert(strcmp(aos_in_game_mode_case_name(3), "room_transition") == 0);
    assert(strcmp(aos_in_game_mode_case_name(4), "quick_map") == 0);
    assert(strcmp(aos_in_game_mode_case_name(6), "soul_menu") == 0);
    /* Cases 9-14 are the sub_0805AFC0 window coroutine, the
     * sub_0800CA98 credits entry and the unk_4CC_6-gated SIO
     * link-sync chain — not cutscenes (see the enum docs). */
    assert(strcmp(aos_in_game_mode_case_name(9), "window_coroutine") == 0);
    assert(strcmp(aos_in_game_mode_case_name(10), "credits") == 0);
    assert(strcmp(aos_in_game_mode_case_name(11), "link_sync_wait") == 0);
    assert(strcmp(aos_in_game_mode_case_name(12), "link_sync_poll") == 0);
    assert(strcmp(aos_in_game_mode_case_name(13), "link_sync_poll2") == 0);
    assert(strcmp(aos_in_game_mode_case_name(14), "link_sync_tail") == 0);
    assert(strcmp(aos_in_game_mode_case_name(200), "unknown") == 0);

    puts("aos_in_game_mode: ok");
    return 0;
}
