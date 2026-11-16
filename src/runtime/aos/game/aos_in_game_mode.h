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
 * timer of the link-sync cases 11-14 and sub_0800FFCC's banner
 * fade). Several cases park a priority-1 SetPlayerInput(1, 0)
 * override in inputData every frame, so playerHeldInput/
 * playerNewInput read 0 regardless of the pad while the global
 * heldInput/newInput the entity pass reads stay untouched.
 *
 * This module is the host-side state channel for that dispatch:
 * the room runtime latches the case its ported counterpart owns
 * (the sub_080104EC transition machine -> 3, the SELECT quick map
 * -> 4, the START soul menu -> 6) and queries the lock predicate
 * once per frame, mirroring the native per-frame SetPlayerInput
 * call through aos_input_set_player. The stage machines
 * themselves stay with their owners (room_transition,
 * quick_map_bldY, soul_menu, the sub_0800FFCC banner fields
 * below, the aos_map_warp picker/commit machines for cases 7/8);
 * unk_65 here records the native stage numbering for
 * traceability. Case 5 is the exception: sub_080106DC's stage
 * machine is small enough that aos_in_game_mode_transition_fade_*
 * below models it directly on unk_65/unk_66/fade_bldY. */
typedef struct {
    uint8_t unk_64;     /* gEwramData + 0x64: dispatch case */
    uint8_t unk_65;     /* gEwramData + 0x65: stage within the case */
    uint16_t unk_66;    /* gEwramData + 0x66: shared counter (native u16) */
    /* unk_60.unk_4CC_6 (gEwramData + 0x4CC bit 6): the link-session
     * flag. Read by RandomNumberGenerator2 (src/code_080009A0.c —
     * selects randomNumber2), the player damage/weapon gates in
     * src/code/code_080211F0.c, sub_080344AC (player +0x4F4 |=
     * 0x80000), sub_0800C5D8's death-room branch and the case-0/3
     * tails that arm the cases 11-14 link-sync chain. Every
     * decompiled writer only CLEARS it (src/code_08002454.c:399,
     * src/code_0800B700.c:1378's DMA_FILL_32 of unk_60); an
     * exhaustive scan of every asm/non_matching site that loads
     * the 0x4CC offset finds only bit-0/1/2 tests and the
     * GameModeSoulTradeMenu bit-2 clear — no set-to-1 exists in
     * the decomp corpus, so the flag is either written by link
     * code outside the corpus or is dead in retail. The runtime
     * keeps it clear. */
    bool unk_4cc_6;
    /* sub_0800FFCC area-name banner (code_0800F1FC.c): unk_42C bit
     * 0x80 arms it at sub_080104EC's stage-2 settle end when
     * sub_0800FCB8 returns a banner area in [0, 0xC]; unk_4CD is
     * the routine's own stage (0 idle, 1-4 the timed sequence);
     * unk_37C is the per-area already-shown bitmask sub_0800FE98
     * sets and sub_0800FCB8 tests; banner_area latches the FCB8
     * result. unk_A074_0/unk_A074_1 are the gEwramData +0xA074
     * bitfield bits this routine owns: bit 0 suspends
     * sub_08000B64's entity pass (and sub_0800CA40/sub_08011CD0
     * and the case-1 exit check), bit 1 is the sub_0800C5A8
     * menu/cutscene gate the SELECT/START edges test. */
    uint8_t unk_4CD;
    bool unk_42C_80;
    int8_t banner_area;
    uint16_t unk_37C;
    bool unk_A074_0;
    bool unk_A074_1;
    /* sub_080106DC (case 5) stage model: the modeled
     * gDisplayRegisters.bldY (0..BLDY_MAX_VALUE = 16) the runtime
     * turns into a black veil. unk_65/unk_66 already carry the
     * native stage and shared counter. */
    uint8_t fade_bldY;
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
    /* case 9: sub_0805AFC0 — the shared window-manager coroutine
     * (control block gEwramData + 0xE2C, unk_65 stages 0-13:
     * window-frame open, language portrait pick 0x0815F120/
     * 0x0815F480/0x0815F2D0, sub_08067310/sub_08067488 text
     * paging, input poll, close). Armed ONLY by sub_0805AF68,
     * which the Object20 proximity trigger func_08065DB8
     * (asm/code/code_08060B98.s, the runtime's
     * AOS_OBJECT20_FAMILY_LOOP / aos_npc_talk_65db8) calls on
     * its state-2 text-done tail after sub_08067898. It latches
     * unk_64 = 9, unk_65 = 0, unk_A074 |= 0x3C, unk_A074_0 |= 1
     * and gDisplayRegisters + 0x48 = 0xFF; the case-1 entity
     * pass is suspended except for the sub_08000B64 calls inside
     * stages 0 and 4, and the routine's return-1 tail writes
     * unk_64 = 1. */
    AOS_INGAME_WINDOW_COROUTINE = 9,
    /* case 10: sub_0800CA98 — one frame: restores unk_42C around
     * sub_0800C7A4's state wipe, then returns GAME_MODE_CREDITS
     * to the main loop (GameModeCreditsUpdate,
     * src/code_08013960.c: MUS_26, tilesets 0x080E5BC8/5BD0/
     * 5BD8, scroll tables 0x080E8F04/0x080E7B54/0x080E81E4/
     * 0x080E8874, A|START skip, then GAME_MODE_KONAMI_LOGO).
     * Writers: the Object20 story endings — asm/code/
     * code_08050A3C.s sub_0805C08C (param0 29) and sub_0805C628
     * (param0 31), asm/code/code_08060B98.s sub_08062664
     * (param0 23) — each stores gEwramData + 0x64 = 0x0A on its
     * post-window-close tail; the runtime's write_64_0a reports
     * already cover all four param0 sites. */
    AOS_INGAME_CREDITS = 10,
    /* cases 11-14: the unk_4CC_6-gated SIO link-sync chain —
     * NOT cutscenes. Armed only from the case-0 stage-4 and
     * case-3 return tails when unk_4CC_6 holds (sub_080D7F78 +
     * sub_080D7FB8 init the multi-SIO sync, unk_66 = 0x10).
     * Case 11 counts unk_66 down then latches 12; case 12 polls
     * sub_080D7F28 (sub_080D80CC && sub_080D80EC link-ready) &&
     * sub_080D7F48 (partner id 0xA2C) while sub_0803407C/
     * sub_0803427C exchange the unk_4D4/unk_4DC player records
     * over sub_080D8020/sub_080D8088, latching 13 (or case 2
     * death on chain end / unk_13224 bounds); case 13 repeats
     * the poll and latches 14; case 14 adds sub_080341D0 +
     * sub_08034498(NULL) and runs the case-1 room/exit machinery
     * again. All exits land on unk_64 = 2 (sub_0800C8C8
     * death/game over). Unreachable without a link session. */
    AOS_INGAME_LINK_SYNC_WAIT = 11,
    AOS_INGAME_LINK_SYNC_POLL = 12,
    AOS_INGAME_LINK_SYNC_POLL2 = 13,
    AOS_INGAME_LINK_SYNC_TAIL = 14
};

/* Fresh in-game state: unk_64 = 1 (normal play), unk_A074_1 set
 * (the menu gate is open). Native room init clears unk_64 =
 * unk_65 = 0 and the boot fade (case 0) walks it to 1; a room
 * loaded mid-game starts directly in play. */
void aos_in_game_mode_init(AosInGameMode *mode);

/* Latch a new case like the native writers do: sub_08010244 ->
 * (3, 0), sub_08011338 -> (4, 0), sub_08010978 -> (5, 0),
 * sub_080115D4 -> (7, 0), sub_08011AD0 -> (8, 0), sub_0805AF68
 * -> (9, 0) plus the unk_A074/display side effects case 9 owns,
 * the Object20 story endings -> (10, untouched unk_65) through
 * the raw +0x64 byte store, and the case-0/3 tails -> (0xB, 0x10
 * unk_66) when unk_4CC_6 holds; every enter()-style writer pairs
 * the case with unk_65 = 0 except case 10 which leaves unk_65
 * alone. unk_66 is only cleared by sub_08010978 and preset to
 * 0x10 by the link-sync arms — pass clear_counter for case 5. */
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
 * suspend gameplay through the mode dispatch itself — the
 * case-1 entity pass simply does not run (case 9's routine calls
 * sub_08000B64 only inside its stage-0 and stage-4 windows, and
 * case 14 runs the room/exit machinery itself), so the predicate
 * stays false for them even though the player cannot act.
 * The sub_0800FFCC banner never touches SetPlayerInput — it
 * freezes the entity pass through unk_A074_0 instead. */
bool aos_in_game_mode_locks_input(const AosInGameMode *mode);

/* sub_080104EC's unk_65 == 2 tail: when BG1's scroll velocities
 * reach zero the native code runs sub_0800FCB8 and, for a result
 * in [0, 0xC], sets unk_42C bit 0x80 — arming sub_0800FFCC on the
 * first case-1 frame after the transition resumes. `area` is that
 * FCB8 result (see aos_area_banner_area); call only in range. */
void aos_in_game_mode_arm_area_banner(AosInGameMode *mode, int area);

/* One sub_0800FFCC call. GameModeInGameUpdate's case 1 runs it
 * every frame after sub_08011CD0; call this once per unk_64 == 1
 * frame and only then — other cases return immediately like the
 * native `!(unk_42C & 0x80)` early-out combined with the case
 * dispatch (the routine itself is only reached from case 1).
 * `door_bit0` reproduces the `(unk_42C & 0x101) == 1` abort check
 * at the top of the routine — pass true only when unk_42C bit 0
 * is set and bit 8 clear (the runtime's door-hold approximation
 * never produces that exact pattern; the door sets both bits).
 * Read mode->unk_A074_0 BEFORE calling to gate the frame's entity
 * pass: native runs sub_08000B64 before sub_0800FFCC, so the bit
 * this call leaves decides the NEXT frame's update. */
void aos_in_game_mode_area_banner_step(AosInGameMode *mode,
                                       bool door_bit0);

/* sub_08010244's prologue (and sub_0801093C's armed branch): a new
 * room transition requested while the banner holds runs
 * sub_08010144 — unk_A074_0 = 0, sub_0800C5A8(1) -> unk_A074_1 = 1,
 * unk_4CD = 0 — then clears unk_42C bit 0x80. Cancels a pending
 * banner without touching unk_37C (the area stays marked shown,
 * like the native abort). */
void aos_in_game_mode_area_banner_cancel(AosInGameMode *mode);

/* sub_0800FCB8: pick the area-name banner for a room. `cell` is
 * the castle map-table u16 under the room's map anchor (bit 15 =
 * save room, bit 14 = warp room, bits 6-9 = area, bits 0-5 =
 * room); `transition_room`/`save_room`/`warp_room` are the
 * CheckRoomTransition list hit and the GetSaveRoomFlagFromMap-
 * Position/GetWarpRoomFlagFromMapPosition cell bits resolved by
 * the caller for the cell's decoded room; `unk_37C` is the
 * already-shown bitmask. Returns the banner area index 0-12 or
 * -1. The native scroll-position chain (bgInfo[1].xPos.
 * part8.integer1 >> 8 inside GetAreaFromMapPosition) always
 * collapses to the room's map-anchor cell for in-game scroll
 * distances, so only (map_x, map_y) matters. The (7,13)->12,
 * (8,9)->13, (8,10)->14, (8,11)->15 room-pair remaps are kept;
 * results above 12 fail sub_080104EC's `<= 0xC` arm check. */
int aos_area_banner_area(uint16_t cell, bool transition_room,
                         bool save_room, bool warp_room,
                         uint16_t unk_37C);

/* sub_08010978's latch for case 5: unk_64 = 5, unk_65 = 0,
 * unk_66 = 0 plus the blend registers. Call right after
 * aos_in_game_mode_enter(mode, AOS_INGAME_TRANSITION_FADE,
 * true) — the enter already clears unk_66 like the native
 * writer; this only zeroes the modeled bldY. */
void aos_in_game_mode_transition_fade_begin(AosInGameMode *mode);

/* One sub_080106DC call while unk_64 == 5. Returns 1 on the frame
 * the native routine returns 1 (bldY reached 0 in unk_65 == 3) —
 * the caller then runs aos_in_game_mode_resume like the case-5
 * tail's `unk_64 = 1`. Stage map: unk_65 0 ramps bldY 0->16 at one
 * step per four unk_66 counts, 1 is the sub_08010350/sub_0801083C
 * screen-scroll commit — unported because the room header's
 * unk_88->unk_18 screen-transition table and sub_0800F9EC's room
 * reload have no runtime channel (see aos_in_game_mode.c), so the
 * stage is a documented one-frame no-op, 2 waits out BG1's
 * scroll velocities (zero without the commit, so one frame), 3
 * ramps bldY back to 0 at one step per two unk_66 counts. The
 * stage-0 cleanup (sub_0800FBD8 transient-entity sweep,
 * sub_0803CCBC/sub_0806C354/sub_080124A8 effect teardown,
 * hBlankEffect stop, player unk_53C_5 = 0) has no runtime channel
 * and is not modelled. */
int aos_in_game_mode_transition_fade_step(AosInGameMode *mode);

/* Diagnostic case label for logs; "unknown" outside 0-14. */
const char *aos_in_game_mode_case_name(uint8_t unk_64);

#endif /* AOS_IN_GAME_MODE_H */
