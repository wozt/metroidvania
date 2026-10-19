/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's GM_CHOZODIA_ESCAPE (chozodia_escape.c), on the
 * non-gameplay screen from the ROM bank: the blue ship leaving the mother
 * ship, the mother ship heating up inside its growing explosion (the
 * H-blank window becomes a per-line WIN0H), blowing up, the ship leaving
 * Zebes and the "MISSION ACCOMPLISHED" letters. The runtime surrounding
 * this screen saves the completion and gallery/difficulty unlock state,
 * then starts the credits. */
#ifndef MZM_CHOZODIA_ESCAPE_H
#define MZM_CHOZODIA_ESCAPE_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_screen.h"

enum { MZM_CHOZODIA_ESCAPE_OBJECTS = 5, MZM_CHOZODIA_ESCAPE_LINES = 160 };

/* struct ChozodiaEscapeData, gSubGameMode1 and gBg0-2X/YPosition */
typedef struct {
    uint8_t sub_mode;
    uint8_t stage, unk_1, unk_2;
    uint16_t timer;
    uint8_t oam_types[MZM_CHOZODIA_ESCAPE_OBJECTS];
    uint32_t oam_pointers[MZM_CHOZODIA_ESCAPE_OBJECTS];     /* ROM addresses */
    uint8_t oam_frames[MZM_CHOZODIA_ESCAPE_OBJECTS];
    uint8_t oam_timers[MZM_CHOZODIA_ESCAPE_OBJECTS];
    uint16_t oam_x[MZM_CHOZODIA_ESCAPE_OBJECTS], oam_y[MZM_CHOZODIA_ESCAPE_OBJECTS];
    uint8_t unk_3e[MZM_CHOZODIA_ESCAPE_OBJECTS];
    int16_t oam_x_offset, oam_y_offset;
    uint16_t scaling;
    uint16_t haze[2][MZM_CHOZODIA_ESCAPE_LINES];
    uint16_t semi_minor_axis;
    uint8_t haze_writing, haze_reading;
    bool hblank;
    uint16_t dispcnt, bldcnt;
    uint16_t monochrome[256];
    uint16_t bg_x[3], bg_y[3];
} MzmChozodiaEscape;

/* gMainGameMode = GM_CHOZODIA_ESCAPE, gSubGameMode1 = 0; false without
 * the ROM bank. */
bool mzm_chozodia_escape_enter(MzmChozodiaEscape *e, MzmScreen *s);
/* ChozodiaEscapeHandler, then its V-blank, for one frame; true once it
 * ended (GM_CREDITS natively). */
bool mzm_chozodia_escape_update(MzmChozodiaEscape *e, MzmScreen *s);

#endif /* MZM_CHOZODIA_ESCAPE_H */
