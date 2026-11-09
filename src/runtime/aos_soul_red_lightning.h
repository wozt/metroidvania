/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_LIGHTNING_H
#define AOS_SOUL_RED_LIGHTNING_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_combat.h"

/* SkillLightningDollUse (0x080277B0): a bolt ahead of Soma whose attack
 * box reaches further every frame. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (whole parts written) */
    bool facing_left;       /* + 0x0C: Soma's facing when it was cast */
    bool vflip;             /* + 0x58 bit 7: a random bit */
    uint8_t reach;          /* + 0x0D: the box width */
    bool palette_reloaded;  /* + 0x0B: frame 8's palette reload (not drawn) */
} AosLightning;

/* State 0: placed by sub_08031EA4 at Soma's facing and (16, -28), reach
 * 16, a random vertical flip (sub_08042A54(1, 0xA) is not run). */
AosLightning aos_lightning_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                                  bool vflip);
/* State 1, before the animation step, with the current sequence frame:
 * past frame 3 the reach grows by 8 while below 0x80; up to frame 3 it
 * follows Soma while his flag 0x100 (a cast) is set. At frame 8 the
 * palette reload of 0x0820B188 is marked (sub_0803C7B4 is not drawn). */
void aos_lightning_update(AosLightning *lightning, uint8_t frame, uint32_t soma_flags,
                          int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* sub_08042884(e, 0x2000F000 + reach << 16): (0, -16, reach x 32). */
AosBox aos_lightning_box(const AosLightning *lightning);
#endif
