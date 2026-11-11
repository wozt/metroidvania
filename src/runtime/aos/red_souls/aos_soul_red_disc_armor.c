/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_disc_armor.h"

#include "../../aos/math/aos_math.h"

AosDiscArmor aos_disc_armor_create(int32_t source_x, int32_t source_y,
                                   bool mirrored) {
    AosDiscArmor disc = {.state = 0, .timer = 0x1A,
                         .angle = mirrored ? 0x6000 : 0x2000,
                         .step = mirrored ? 0x276 : -0x276,
                         .anchor_x = source_x,
                         .anchor_y = source_y - 0x18000,
                         .x = source_x, .y = source_y - 0x18000};
    return disc;
}

bool aos_disc_armor_update(AosDiscArmor *disc) {
    if (disc->state == 2) return false;
    if (disc->state == 0) disc->state = 1;
    /* State 1 (also fallen through from state 0): the orbit radius is
     * |sin(timer * 1260)| * 60 pixels and the + 0x2C angle steps by the
     * facing-signed + 0x2E (-+0x276). The native multiplies the two
     * 16.16 sines with a plain muls; the >> 16 form used here is the
     * mathematically equivalent fixed-point product. */
    int32_t radius = aos_sine((uint32_t)disc->timer * 1260u);
    if (radius < 0) radius = -radius;
    radius = (radius * 60) >> 8; /* sin16.16 * 60 -> 16.16 px */
    int32_t dx = (int32_t)(((int64_t)aos_sine((uint32_t)(disc->angle + 0x4000)) *
                            radius) >> 16);
    int32_t dy = (int32_t)(((int64_t)aos_sine((uint32_t)disc->angle) *
                            radius) >> 16);
    disc->x = disc->anchor_x + dx;
    disc->y = disc->anchor_y + dy;
    disc->angle += disc->step;
    /* + 0x30 += 0x1B00 is a visual spin timer that is not ported. */
    if (disc->timer == 0) {
        disc->state = 2;
    } else {
        --disc->timer;
    }
    return true;
}
