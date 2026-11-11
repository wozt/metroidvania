/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_altair.h"

AosAltair aos_altair_create(void) {
    return (AosAltair){.state = 0, .timer = 0, .phase = 0};
}

bool aos_altair_update(AosAltair *altair, bool hit, bool outside,
                       bool *song_ee) {
    *song_ee = false;
    if (altair->state == 0) {
        altair->state = 1;
    } else if (altair->state == 1) {
        /* + 0x6C bit 4 switches to the flicker state and anim sequence 2. */
        if (hit) altair->state = 2;
    } else if (altair->state == 2) {
        /* + 0x0D counts up; odd/even picks the + 0x5A variant (0x10/0x20),
         * a visual detail that is not ported. */
        ++altair->timer;
        if (hit) {
            /* variant 0x10, anim sequence 0, + 0x72 &= ~2, state 3. */
            altair->state = 3;
        }
    } else {
        /* sub_0806D430(e, 0x80000) physics is not ported; + 0x6E counts
         * and reaching 2 requests song 0xEE. */
        ++altair->phase;
        if (altair->phase == 2) *song_ee = true;
    }
    return !outside;
}
