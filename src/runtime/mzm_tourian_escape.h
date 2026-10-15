/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's post-Tourian escape cutscene (tourian_escape.c), rendered
 * by the shared non-gameplay GBA PPU from the private ROM bank. */
#ifndef MZM_TOURIAN_ESCAPE_H
#define MZM_TOURIAN_ESCAPE_H

#include <stdbool.h>
#include <stdint.h>

#include "mzm_room_gfx.h"
#include "mzm_screen.h"

enum { MZM_TOURIAN_ESCAPE_OBJECTS = 10 };

typedef struct {
    bool ready, planet, flash, varia;
    uint8_t stage, flash_timer, flash_level, effect_phase, effect_timer;
    uint16_t timer, explosion_timer, bg2_scale, object_scaling, secondary_scaling;
    int16_t ship_x, explosion_x, bg0_x, bg0_y;
    int16_t firing_vertical_velocity, firing_vertical_acceleration;
    int16_t win0_left, win0_right, win0_top, win0_bottom;
    uint8_t object_active[MZM_TOURIAN_ESCAPE_OBJECTS];
    uint8_t object_frame[MZM_TOURIAN_ESCAPE_OBJECTS];
    uint8_t object_timer[MZM_TOURIAN_ESCAPE_OBJECTS];
    uint32_t object_oam_address[MZM_TOURIAN_ESCAPE_OBJECTS];
    uint16_t object_x[MZM_TOURIAN_ESCAPE_OBJECTS];
    uint16_t object_y[MZM_TOURIAN_ESCAPE_OBJECTS];
    int16_t object_velocity[MZM_TOURIAN_ESCAPE_OBJECTS];
    MzmRoomGfx room;
} MzmTourianEscape;

/* TourianEscapeInit and the start of TourianEscapeUpdateStage. */
bool mzm_tourian_escape_enter(MzmTourianEscape *escape, MzmScreen *screen, bool varia);
/* Advances the current native scene. The first nine scenes stop at stage 9
 * until the following crashing scene is ported. */
void mzm_tourian_escape_update(MzmTourianEscape *escape, MzmScreen *screen);

#endif /* MZM_TOURIAN_ESCAPE_H */
