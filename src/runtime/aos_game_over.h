/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_GAME_OVER_H
#define AOS_GAME_OVER_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_input.h"

/* A palette fade of sub_0803D18C (BG set, unk_1261C) or sub_0803CDF0 (OBJ
 * set, unk_11DDC): every colour mixes with (r, g, b) by the weight, 0x10000
 * keeping the colour and 0 giving the target, which moves from `from` to `to`
 * by sub_0803E8BC's step. */
typedef struct {
    uint8_t r, g, b;       /* 5-bit target colour */
    int32_t weight, to, step;
    bool active;           /* unk_4C, read by sub_0803D408 / sub_0803D06C */
} AosPaletteFade;

/* sub_0803D18C / sub_0803CDF0. */
void aos_fade_start(AosPaletteFade *fade, uint8_t r, uint8_t g, uint8_t b, int32_t from,
                    int32_t to, int32_t frames);
/* One frame of the fade processor: the weight moves by its step and the
 * fade ends on its target (the processor itself is not decompiled; this is
 * its traced effect). */
void aos_fade_tick(AosPaletteFade *fade);
/* sub_0803E8BC: 16.16 division keeping 8 fractional bits of each side. */
int32_t aos_fixed_divide(int32_t numerator, int32_t denominator);

/* GameModeGameOverUpdate (src/code_08013960.c) and the in-game death
 * sub-state sub_0800C8C8 it follows. */
typedef struct {
    uint8_t stage;          /* gameModeUpdateStage */
    uint16_t timer;         /* unk_12 */
    uint8_t alpha;          /* unk_4 */
    uint8_t eva, evb;       /* BLDALPHA: OBJ and BG1 weights (0-16) */
    bool sprite_shown;      /* the language's GAME OVER sprite exists */
    AosPaletteFade bg, obj;
} AosGameOver;

/* sub_0800C8C8 case 0 after sub_0801AF20 (Soma's death): music muted, BG
 * fading to black over 0x40 frames and OBJ over 0x80. */
void aos_death_start(AosGameOver *death);
/* sub_0800C8C8 case 1: true once the OBJ fade ended (the game then enters
 * GAME_OVER; the 0x42C & 0x200000 restart is not modelled). */
bool aos_death_update(AosGameOver *death);
/* The mode's stages: 0 sets the screen (BG fading in from black over 0x20,
 * EVA 0 / EVB 16), 1 waits for the fade, 2 waits 0x80 frames (creating the
 * sprite on the last) then blends it in over 0x20, 3 waits for any new
 * press and fades BG and OBJ to white over 0x20, 4 waits for the BG fade and
 * returns AOS_MODE_KONAMI_LOGO. Fades advance after each update. */
int32_t aos_game_over_update(AosGameOver *game_over, const AosInput *input);
#endif
