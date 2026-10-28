/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_AXE_H
#define AOS_SKILL_AXE_H
#include <stdbool.h>
#include <stdint.h>

enum { AOS_AXE_TRAILS = 2, AOS_AXE_HISTORY = 10 };

/* SkillAxeArmorUse's axe. */
typedef struct {
    int32_t x, y;       /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, ax;     /* + 0x48, + 0x50 */
    int16_t spin;       /* + 0x24: +-1 */
    int16_t turns;      /* + 0x2A */
    uint16_t angle;     /* the angle drawn this frame, + 0x2A << 11 */
    uint8_t state;      /* + 0x0A: 0 created, 1 flying, 2 deleted next */
} AosAxe;

/* One of its two afterimages (sub_080240A0). */
typedef struct {
    int16_t history[AOS_AXE_HISTORY];   /* + 0x20: the axe's whole room X */
    int32_t x, y;
    int32_t angle, step;                /* + 0x48, + 0x50 */
    uint8_t lag;                        /* + 0x0B: 3 or 6 frames */
    uint8_t state;
} AosAxeTrail;

/* State 0 of SkillAxeArmorUse: at Soma's whole X and 26 pixels above his
 * whole Y, 3.25 pixels a frame toward his facing, pulled back by 0x1000 a
 * frame and turning by one step of 0x800 a frame (negative facing left).
 * The entity's own facing bit stays clear: it is drawn unmirrored. */
AosAxe aos_axe_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* One update: state 0 and 1 move by the speed, then pull the speed back
 * while it is within 8 pixels a frame, take the drawn angle and advance
 * the turn; outside the screen margins (mx, my) of entry 4 it goes to
 * state 2, deleted on the next update (false). */
bool aos_axe_update(AosAxe *axe, int cam_x, int cam_y, int mx, int my);
/* Afterimage `index` (0 or 1): 3 or 6 frames behind, its angle that many
 * turn steps back. */
AosAxeTrail aos_axe_trail_create(int index, bool soma_facing_left);
/* One update of sub_080240A0 after the axe's: deleted (false) once the axe
 * is in state 2; otherwise the history shifts in the axe's whole X, the
 * afterimage takes the X `lag` entries back and the axe's Y, and turns. */
bool aos_axe_trail_update(AosAxeTrail *trail, const AosAxe *axe);
#endif
