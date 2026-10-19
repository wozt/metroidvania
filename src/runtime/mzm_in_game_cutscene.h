/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission suit upgrade in-game cutscene (in_game_cutscene.c:
 * InGameCutsceneStart, InGameCutsceneProcess, InGameCutsceneInit and
 * InGameCutsceneUpgradingSuit with InGameCutsceneCalculateSuitFlashOffset):
 * the room's BG0 faded out and hidden, window 1 opened on Samus and grown
 * to the whole screen with BG0 filled by a bright tile blended over it, the
 * suit activated, the window shrunk back, then BG0 restored and control
 * given back. The IGC_GETTING_VARIA and IGC_GETTING_FULLY_POWERED scenes
 * share it; the renderer draws the window from the state kept here. */
#ifndef MZM_IN_GAME_CUTSCENE_H
#define MZM_IN_GAME_CUTSCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_samus.h"

/* ItemAcquisition (gCurrentItemBeingAcquired) */
enum { MZM_ACQUISITION_VARIA = 14, MZM_ACQUISITION_GRAVITY = 15 };
/* RuinsTestFightStage values the cutscene reads and writes */
enum { MZM_RUINS_TEST_STARTING_SUIT_ANIM = 7, MZM_RUINS_TEST_SUIT_ANIM_ENDED = 8 };

typedef struct {
    /* gInGameCutscene */
    bool queued, started;           /* cutsceneNumber set / IGC_STARTED_FLAG */
    uint8_t stage;
    uint16_t timer;
    uint8_t item;                   /* gCurrentItemBeingAcquired */
    /* gSuitFlashEffect: window 1 (pixels) */
    uint8_t left, right, top, bottom, flash_timer;
    /* gIoRegistersBackup's BLDALPHA and gWrittenToBldalpha_L / _H */
    uint8_t eva_backup, evb_backup, bldalpha_l, bldalpha_h;
    /* DISPCNT without BG0 (stages 3-6 and 15-16) */
    bool bg0_hidden;
    /* BG0 filled with tile 0xC0 of palette 9 and blended at 13/16 over the
     * screen inside window 1, nothing of BG0 outside it (stages 7-14) */
    bool flash;
    /* gDefaultTransparency.unk_0: the room transparency held */
    bool transparency_held;
} MzmUpgradingSuit;

typedef struct {
    int samus_x, samus_y;           /* gSamusData position (native subpixels) */
    int bg1_x, bg1_y;               /* gBg1XPosition / gBg1YPosition (subpixels) */
    bool power_bomb;                /* a power bomb exploding or placed */
    uint8_t eva, evb;               /* the room's BLDALPHA at the start */
} MzmUpgradingSuitInput;

/* InGameCutsceneStart with gCurrentItemBeingAcquired = item. */
void mzm_upgrading_suit_start(MzmUpgradingSuit *c, uint8_t item);
/* InGameCutsceneProcess for one frame. The suit is activated in
 * `equipment` (stage 11), `ruins_stage` (gSubSpriteData1.work3) is read
 * and set to SUIT_ANIM_ENDED. Returns true on the frame control is given
 * back (lastWallTouchedMidAir and gDisablePause cleared). */
bool mzm_upgrading_suit_process(MzmUpgradingSuit *c, const MzmUpgradingSuitInput *in,
                                MzmEquipment *equipment, uint8_t *ruins_stage);
/* InGameCutsceneCalculateSuitFlashOffset: newEnd << 8 | newStart. */
uint16_t mzm_suit_flash_offset(uint8_t intensity, uint8_t start, uint8_t end);

#endif
