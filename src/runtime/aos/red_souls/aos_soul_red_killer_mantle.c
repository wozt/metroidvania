/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_killer_mantle.h"

AosKillerMantle aos_killer_mantle_create(void) {
    /* The native state-0 block loads the 0x081D912C tiles / 0x0820B40C
     * palette / 0x08234CDC frames record (entry 0x138 of 0x080E13BC),
     * presets attack box 5, plays song 0xD1, bumps the + 0x131BE cast
     * counter and enters state 1; the runtime performs the audible and
     * bookkeeping halves. */
    return (AosKillerMantle){.state = 0};
}

bool aos_killer_mantle_update(AosKillerMantle *mantle, bool anim_finished,
                              bool outside) {
    /* State 1's tail: sub_0806D128(e, 4) outside the camera margins, or
     * the + 0x59 bit-4 bookkeeping already latched, ends the skill; the
     * bit-4 writer was not traced, so the port ends on the animation end
     * and the margin probe only (documented divergence). State 2 clears
     * the native counters and deletes the entity. */
    if (mantle->state == 2) return false;
    if (mantle->state == 0) mantle->state = 1;
    if (anim_finished || outside) mantle->state = 2;
    return true;
}
