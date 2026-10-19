/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's GM_CREDITS (ending_and_gallery.c), on the non-gameplay
 * screen from the ROM bank: the credits scrolling over the dark Chozo
 * wall, the wall scrolling down and zooming into the Chozo drawing, the
 * end screen with Samus posing, then the ending image with the clear time
 * and collection rate, followed by the unlocked-options pop-up. */
#ifndef MZM_CREDITS_H
#define MZM_CREDITS_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_screen.h"
#include "mzm_text.h"

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

enum {
    MZM_ENDING_OAM = 30,
    MZM_ENDING_FLAG_NEW_TIME_ATTACK_RECORD = 1 << 0,
    MZM_ENDING_FLAG_UNKNOWN = 1 << 4,
    MZM_ENDING_FLAG_FIRST_TIME_ATTACK_CLEAR = 1 << 5,
    MZM_ENDING_FLAG_FIRST_HARD_MODE_CLEAR = 1 << 6,
    MZM_ENDING_FLAG_FIRST_CLEAR = 1 << 7,
};

/* struct EndingData, the parts the ported credits and ending image use. */
typedef struct {
    uint8_t stage, unk_1, end_screen_timer;
    uint16_t timer;
    uint8_t unk_6;
    uint16_t unk_8, unk_a, current_line, unk_e;
    uint16_t line_tilemap_1[32], line_tilemap_2[32];
    uint32_t line_offset_1, line_offset_2;
    uint8_t ending_number, completion_percentage, language, oam_length;
    uint8_t oam_types[MZM_ENDING_OAM];
    uint8_t ending_letters_spawn_delay[MZM_ENDING_OAM];
    uint8_t letter_palette_timer[MZM_ENDING_OAM];
    uint16_t oam_x[MZM_ENDING_OAM], oam_y[MZM_ENDING_OAM];
    uint32_t oam_frame[MZM_ENDING_OAM];
    uint8_t pose_palette_timer, has_new_record, new_record_palette_timer;
    uint16_t dispcnt, bldcnt;
} MzmEndingData;

typedef struct {
    uint8_t sub_mode;               /* gSubGameMode1 */
    uint32_t pen;                   /* the item count and ending number */
    uint8_t igt_hours, igt_minutes, igt_seconds;
    uint8_t ending_flags;           /* gEndingFlags */
    MzmEndingData data;             /* ENDING_DATA */
    MzmMessage popup_message;       /* gCurrentMessage */
    uint32_t popup_text;            /* English file-screen text in ROM */
    uint16_t bg_x[4], bg_y[4];      /* gBg0-3X/YPosition */
} MzmCredits;

/* gMainGameMode = GM_CREDITS with the ending's item count and number;
 * false without the ROM bank. */
bool mzm_credits_enter(MzmCredits *c, MzmScreen *s, uint32_t pen);
/* gInGameTimer shown by EndingImageLoadIGTAndPercentageGraphics. */
void mzm_credits_set_time(MzmCredits *c, uint8_t hours, uint8_t minutes, uint8_t seconds);
/* Transient flags selected while ChozodiaEscapeInit updates the persistent
 * unlock state. They select the new-record badge and unlocked pop-ups. */
void mzm_credits_set_ending_flags(MzmCredits *c, uint8_t flags);
/* CreditsHandler, then the current V-blank, for one frame; true once the
 * ending image and any unlocked-options pop-ups ended. */
bool mzm_credits_update(MzmCredits *c, MzmScreen *s);

#endif /* MZM_CREDITS_H */
