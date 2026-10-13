/* SPDX-License-Identifier: GPL-3.0-only */
/* The Zero Mission intro (intro.c, English): "Emergency order" typed
 * letter by letter, the ship flying towards the camera (an affine sprite),
 * Samus in her ship, "Exterminate all...", the view of Zebes, "Defeat the
 * Mother Brain", Mother Brain in her jar and the fuzz; A, B or START skips
 * it. The intro's story text of the other languages (TextStartStory) is not
 * ported. */
#ifndef MZM_INTRO_H
#define MZM_INTRO_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_screen.h"

/* struct IntroData and the game mode counters it runs under. */
typedef struct {
    uint8_t stage, current_character, character_palette, final_character;
    int16_t char_drawer_x;
    uint16_t char_drawer_y;
    uint8_t char_drawer_palette, char_drawer_stage, unk_a;
    uint16_t timer;
    uint32_t text_oam;              /* pTextOamFramePointer (ROM address) */
    uint16_t fuzz_palette[16];
    uint16_t rotation, scaling;
    uint8_t unk_3d;
    uint16_t ship_x, ship_y;        /* shipFlyingTowardsCameraX / Y */
    uint16_t dispcnt, bldcnt;
    uint16_t bg0_x, bg0_y;          /* gBg0XPosition / gBg0YPosition (pixels) */
    bool fuzz_vblank;               /* IntroFuzzVBlank is the V-blank */
    uint8_t sub_mode1, sub_mode2;   /* gSubGameMode1, gSubGameMode2 (1: skipped) */
} MzmIntro;

/* gSubGameMode1 = 0 */
void mzm_intro_enter(MzmIntro *intro);
/* IntroHandler then its V-blank for one frame; true when it ended (or was
 * skipped). */
bool mzm_intro_update(MzmIntro *intro, MzmScreen *s);

#endif
