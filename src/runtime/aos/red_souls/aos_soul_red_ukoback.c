/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_ukoback.h"

AosUkoback aos_ukoback_create(void) {
    return (AosUkoback){.state = 0, .timer = 0xF0};
}

bool aos_ukoback_update(AosUkoback *ukoback, bool force_end, bool outside) {
    if (ukoback->state == 0) ukoback->state = 1;
    if (ukoback->state == 1) {
        /* + 0x24 gains 0x1000 a frame while it stays <= 0xFFFF. */
        if (ukoback->ramp <= 0xFFFF) ukoback->ramp += 0x1000;
        if (force_end) ukoback->timer = 0;
        if (ukoback->timer == 0) {
            ukoback->state = 2;
        } else {
            --ukoback->timer;
        }
    } else if (ukoback->state == 2) {
        if (ukoback->ramp <= 0) return false;
        ukoback->ramp -= 0x2000;
    }
    /* The margin probe deletes from any state. */
    if (outside) return false;
    return true;
}
