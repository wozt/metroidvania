/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_were_beast.h"

AosWereBeast aos_were_beast_create(void) {
    /* The native entry also sets + 0x59 bit 4 before the state dispatch;
     * its writer semantics were not traced (same divergence as the Killer
     * Mantle tail). */
    return (AosWereBeast){.state = 0};
}

bool aos_were_beast_update(AosWereBeast *beast, bool anim_finished,
                           bool source_gone) {
    /* State 1 ends through sub_0803F17C == 3 or the source flag check;
     * state 2 releases the native counters and deletes the entity. */
    if (beast->state == 2) return false;
    if (beast->state == 0) beast->state = 1;
    if (anim_finished || source_gone) beast->state = 2;
    return true;
}
