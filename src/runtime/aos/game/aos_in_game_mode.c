/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_in_game_mode.h"

#include <stddef.h>

void aos_in_game_mode_init(AosInGameMode *mode) {
    mode->unk_64 = AOS_INGAME_PLAY;
    mode->unk_65 = 0;
    mode->unk_66 = 0;
    mode->unk_4cc_6 = false;
    mode->unk_4CD = 0;
    mode->unk_42C_80 = false;
    mode->banner_area = -1;
    mode->unk_37C = 0;
    mode->unk_A074_0 = false;
    mode->unk_A074_1 = true;
    mode->fade_bldY = 0;
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
         * holds — the link-session flag; no writer exists in the
         * decompiled corpus (see the struct comment), so this
         * stays clear. */
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

void aos_in_game_mode_arm_area_banner(AosInGameMode *mode, int area) {
    /* sub_080104EC unk_65 == 2: `if ((sub_0800FCB8(...) >= 0) &&
     * (sub_0800FCB8(...) <= 0xC)) unk_42C |= 0x80`. The area index
     * is latched for sub_0800FE98's unk_37C bit and tile select;
     * the caller range-checks like the native arm does. */
    if (area < 0 || area > 0xC) return;
    mode->unk_42C_80 = true;
    mode->banner_area = (int8_t)area;
}

void aos_in_game_mode_area_banner_step(AosInGameMode *mode,
                                       bool door_bit0) {
    /* sub_0800FFCC is only reached from GameModeInGameUpdate's
     * case 1; its own `!(unk_42C & 0x80)` early-out covers the
     * unarmed case. */
    if (mode->unk_64 != AOS_INGAME_PLAY || !mode->unk_42C_80)
        return;

    /* `(unk_42C & 0x101) == 1` -> sub_0801093C + return: the abort
     * runs sub_08010144 (unk_A074_0 = 0, sub_0800C5A8(1) ->
     * unk_A074_1 = 1) and clears the 0x80 arm through
     * sub_08010244_inline_0's sibling path in sub_0801093C. */
    if (door_bit0) {
        mode->unk_42C_80 = false;
        mode->unk_4CD = 0;
        mode->unk_A074_0 = false;
        mode->unk_A074_1 = true;
        return;
    }

    switch (mode->unk_4CD) {
    case 0:
        /* unk_42C |= 0x80 (already armed), dispCnt restore,
         * sub_0800C5A8(0) -> unk_A074_1 = 0, unk_A074_0 = 1,
         * unk_66 = 0x3E, then sub_0800FE98(param_0,
         * sub_0800FCB8(param_0)): the windowed banner strip, the
         * hBlank fade setup and `unk_37C |= 1 << area`. The
         * display/tile work has no runtime counterpart; only the
         * shown-bit and the A074 bits are modelled. */
        mode->unk_A074_0 = true;
        mode->unk_A074_1 = false;
        mode->unk_66 = 0x3E;
        if (mode->banner_area >= 0 && mode->banner_area <= 0xC)
            mode->unk_37C |= (uint16_t)(1u << mode->banner_area);
        mode->unk_4CD = 1;
        break;
    case 1:
        /* unk_66 -= 2 with sub_0800FDC8's hBlank fade; at zero it
         * reloads 0x3C for the hold stage. */
        mode->unk_66 = (uint16_t)(mode->unk_66 - 2);
        if (mode->unk_66 == 0) {
            mode->unk_66 = 0x3C;
            mode->unk_4CD = 2;
        }
        break;
    case 2:
        /* Plain unk_66 countdown hold (0x3C frames). */
        if (mode->unk_66 != 0)
            mode->unk_66 = (uint16_t)(mode->unk_66 - 1);
        else
            mode->unk_4CD = 3;
        break;
    case 3:
        /* unk_66 += 1 with the fade and the bldY ramp; past 0x5F the
         * stage tail runs sub_08010144 (whose unk_A074_0 = 0 /
         * unk_4CD = 0 restores are then overridden) and latches
         * unk_A074_1 = 0, unk_A074_0 = 1, unk_4CD = 4. */
        mode->unk_66 = (uint16_t)(mode->unk_66 + 1);
        if (mode->unk_66 > 0x5F) {
            mode->unk_A074_1 = false;
            mode->unk_A074_0 = true;
            mode->unk_4CD = 4;
        }
        break;
    default: /* unk_4CD == 4 */
        /* unk_42C &= ~0x80, unk_A074_1 = 1, unk_A074_0 = 0,
         * unk_4CD = 0, return 1 — the entity pass resumes on the
         * NEXT frame (this frame's sub_08000B64 already ran with
         * the bit still set). */
        mode->unk_42C_80 = false;
        mode->unk_A074_1 = true;
        mode->unk_A074_0 = false;
        mode->unk_4CD = 0;
        break;
    }
}

void aos_in_game_mode_area_banner_cancel(AosInGameMode *mode) {
    mode->unk_42C_80 = false;
    mode->unk_4CD = 0;
    mode->unk_A074_0 = false;
    mode->unk_A074_1 = true;
}

int aos_area_banner_area(uint16_t cell, bool transition_room,
                         bool save_room, bool warp_room,
                         uint16_t unk_37C) {
    /* sUnk_080E0FA4: the (area, room) pairs sub_0800FCB8 refuses —
     * save-point approaches, warp rooms and scripted set pieces
     * whose names must not banner. */
    static const struct { uint8_t area, room; } excluded[] = {
        {0, 0}, {0, 26}, {2, 4}, {2, 8}, {2, 5}, {4, 6}, {4, 14},
        {5, 6}, {6, 30}, {7, 7}, {8, 12}, {11, 11}, {11, 32},
    };
    /* cell == 0xFFFF decodes to area 15/room 63 natively and falls
     * through GetRoomPointer out of table; no exported room maps
     * there, so it cannot banner. */
    if (cell == 0xFFFF) return -1;

    unsigned area_ = (cell >> 6) & 0xF;
    unsigned room = cell & 0x3F;

    /* CheckRoomTransition (the transition-room pointer list) then
     * GetSaveRoomFlagFromMapPosition (cell bit 15) /
     * GetWarpRoomFlagFromMapPosition (cell bit 14). */
    if (transition_room || save_room || warp_room) return -1;

    for (size_t i = 0; i < sizeof excluded / sizeof *excluded; ++i)
        if (area_ == excluded[i].area && room == excluded[i].room)
            return -1;

    /* The (room, area) remaps: (13,7)->12, (9,8)->13, (10,8)->14,
     * (11,8)->15 — the multi-part areas share one banner name. */
    unsigned area = area_;
    if (room == 13 && area_ == 7) area = 12;
    else if (room == 9 && area_ == 8) area = 13;
    else if (room == 10 && area_ == 8) area = 14;
    else if (room == 11 && area_ == 8) area = 15;

    if ((unk_37C >> area) & 1) return -1;
    return (int)area;
}

void aos_in_game_mode_transition_fade_begin(AosInGameMode *mode) {
    /* sub_08010978: unk_64 = 5, unk_65 = 0, unk_66 = 0 (the caller's
     * enter(..., true) already cleared unk_66) and the blend
     * registers start the fade from bldY = 0. */
    mode->fade_bldY = 0;
}

int aos_in_game_mode_transition_fade_step(AosInGameMode *mode) {
    switch (mode->unk_65) {
    case 0:
        /* unk_A074_2 = unk_A074_4 = 3 (unmodelled bitfield) then
         * the bldY ramp: +1 every fourth unk_66 count until
         * BLDY_MAX_VALUE (16); the frame bldY is already max runs
         * the cleanup tail and advances to stage 1. */
        if (mode->fade_bldY < 16) {
            if ((mode->unk_66 & 3) == 0) mode->fade_bldY++;
            mode->unk_66 = (uint16_t)(mode->unk_66 + 1);
        } else {
            mode->unk_65 = 1;
        }
        return 0;
    case 1:
        /* sub_08010350 + sub_0801083C: the intra-room screen-scroll
         * commit and map-position recompute. The runtime has no
         * unk_88->unk_18 screen-transition table; the follow camera
         * already holds the player centred, so the commit is a
         * documented no-op and the stage lasts this one frame. */
        mode->unk_65 = 2;
        return 0;
    case 2:
        /* sub_0803FBBC scroll request + sub_08000F60/sub_0800D288
         * until BG1's xVel/yVel reach zero. No scroll was issued,
         * so the velocities are already zero: one frame. */
        mode->unk_65 = 3;
        return 0;
    default: /* unk_65 == 3 */
        /* unk_66 += 1; every second count bldY -= 1; at zero the
         * routine restores unk_A074_2/4, unk_65 = 0 and returns 1. */
        mode->unk_66 = (uint16_t)(mode->unk_66 + 1);
        if ((mode->unk_66 & 1) == 0 && mode->fade_bldY > 0)
            mode->fade_bldY--;
        if (mode->fade_bldY == 0) {
            mode->unk_65 = 0;
            return 1;
        }
        return 0;
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
    case 9: return "window_coroutine";
    case 10: return "credits";
    case 11: return "link_sync_wait";
    case 12: return "link_sync_poll";
    case 13: return "link_sync_poll2";
    case 14: return "link_sync_tail";
    default: return "unknown";
    }
}
