/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_arrow.h"

AosArrow aos_arrow_start(int32_t x, bool facing_left) {
    return (AosArrow){.x = x, .vx = facing_left ? -0x60000 : 0x60000, .state = 1};
}

bool aos_arrow_update(AosArrow *arrow, bool struck) {
    if (arrow->state == 1) {
        arrow->x += arrow->vx;
        if (struck) {
            arrow->state = 2;
            arrow->timer = 0x10;
        }
    } else if (arrow->state == 2) {
        if (arrow->timer == 0) return false;
        arrow->timer--;
    }
    return true;
}
