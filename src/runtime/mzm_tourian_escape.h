/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's post-Tourian escape cutscene (tourian_escape.c), rendered
 * by the shared non-gameplay GBA PPU from the private ROM bank. */
#ifndef MZM_TOURIAN_ESCAPE_H
#define MZM_TOURIAN_ESCAPE_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_room_gfx.h"
#include "mzm_screen.h"

typedef struct {
    bool ready, planet, flash;
    uint8_t stage, flash_timer, flash_level;
    uint16_t timer, explosion_timer;
    int16_t ship_x, explosion_x;
    MzmRoomGfx room;
} MzmTourianEscape;

/* TourianEscapeInit and the start of TourianEscapeUpdateStage. */
bool mzm_tourian_escape_enter(MzmTourianEscape *escape, MzmScreen *screen);
/* Advances the current native scene. The first scene stops at stage 1 until
 * the following Samus-in-her-ship scene is ported. */
void mzm_tourian_escape_update(MzmTourianEscape *escape, MzmScreen *screen);

#endif /* MZM_TOURIAN_ESCAPE_H */
