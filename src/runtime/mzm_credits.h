/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's GM_CREDITS (ending_and_gallery.c), on the non-gameplay
 * screen from the ROM bank: the credits scrolling over the dark Chozo
 * wall, the wall scrolling down and zooming into the Chozo drawing, then
 * the end screen (Samus posing, her last pose chosen by the ending
 * number). The ending image with the clear time and rate, and the
 * unlocked options pop-up that follow, are not ported. */
#ifndef MZM_CREDITS_H
#define MZM_CREDITS_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_screen.h"

/* What ChozodiaEscapeGetItemCountAndEndingNumber reads: gEquipment's
 * maximums and abilities (suitMisc, beams and bombs: 14 at most),
 * gDifficulty (0 easy, 1 normal, 2 hard) and gInGameTimer.hours. */
typedef struct {
    int max_energy, max_missiles, max_super_missiles, max_power_bombs;
    int abilities;
    uint8_t difficulty, igt_hours;
} MzmEndingStats;

/* ChozodiaEscapeGetItemCountAndEndingNumber: the ending image (bits 0-3),
 * the ability count (4-7), the power bomb (8-11), super missile (12-15),
 * missile (16-23) and energy (24-31) tank counts. 0 without the ROM bank. */
uint32_t mzm_ending_item_count_and_number(const MzmEndingStats *stats);

/* struct EndingData, the parts the ported credits use */
typedef struct {
    uint8_t stage, unk_1, end_screen_timer;
    uint16_t timer;
    uint8_t unk_6;
    uint16_t unk_8, unk_a, current_line, unk_e;
    uint16_t line_tilemap_1[32], line_tilemap_2[32];
    uint32_t line_offset_1, line_offset_2;
    uint8_t ending_number;
    uint8_t oam_types[2];
    uint8_t unk_160;
    uint16_t dispcnt, bldcnt;
} MzmEndingData;

typedef struct {
    uint8_t sub_mode;               /* gSubGameMode1 */
    uint32_t pen;                   /* the item count and ending number */
    MzmEndingData data;             /* ENDING_DATA */
    uint16_t bg_x[4], bg_y[4];      /* gBg0-3X/YPosition */
} MzmCredits;

/* gMainGameMode = GM_CREDITS with the ending's item count and number;
 * false without the ROM bank. */
bool mzm_credits_enter(MzmCredits *c, MzmScreen *s, uint32_t pen);
/* CreditsHandler, then the current V-blank, for one frame; true once the
 * ported part ended (the ending image follows natively). */
bool mzm_credits_update(MzmCredits *c, MzmScreen *s);

#endif /* MZM_CREDITS_H */
