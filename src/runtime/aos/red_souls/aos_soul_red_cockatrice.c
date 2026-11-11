/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_cockatrice.h"

#include "../../aos/math/aos_math.h"

AosCockatrice aos_cockatrice_create(int32_t x, int32_t y, bool mirrored) {
    AosCockatrice cockatrice = {.state = 0, .timer = 0x20,
                                .angle = mirrored ? (int32_t)0xFFFF8000 - 0x2800
                                                  : 0x2800,
                                .step = mirrored ? 0x40 : -0x40,
                                .x = x, .y = y, .vx = 0, .vy = 0};
    return cockatrice;
}

bool aos_cockatrice_update(AosCockatrice *cockatrice, bool outside) {
    if (cockatrice->state == 0) cockatrice->state = 1;
    /* State 1 (also fallen through from state 0): + 0x2C += + 0x2E,
     * + 0x24 = sine(+ 0x0D << 0xA), + 0x50/+ 0x54 = sine(angle +/-
     * 0x4000) * 32 and the dynamic box offsets; + 0x40/+ 0x44 drift by
     * + 0x48/+ 0x4C which stay 0 in this model. */
    cockatrice->angle += cockatrice->step;
    cockatrice->vx = (int32_t)((int64_t)aos_sine((uint32_t)(cockatrice->angle + 0x4000)) * 32);
    cockatrice->vy = (int32_t)((int64_t)aos_sine((uint32_t)cockatrice->angle) * 32);
    if (cockatrice->timer == 0) return false;   /* state-2 delete frame */
    --cockatrice->timer;
    /* The tail: margin probe or + 0x59 bit 4 deletes. */
    return !outside;
}
