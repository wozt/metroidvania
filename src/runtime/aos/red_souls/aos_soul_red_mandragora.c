/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_mandragora.h"

#include "../../aos/math/aos_math.h"

AosMandragora aos_mandragora_create(void) {
    return (AosMandragora){.state = 0, .timer = 0, .flag_f = 0,
                           .leaf = false, .leaf_x = 0, .leaf_y = 0};
}

bool aos_mandragora_update(AosMandragora *mandragora, int32_t rng,
                           int32_t entity_x, int32_t entity_y,
                           bool hit, bool outside) {
    mandragora->leaf = false;
    if (mandragora->state == 0) {
        mandragora->state = 1;
    } else if (mandragora->state == 1) {
        /* + 0x0D counts up; every fourth frame emits a leaf at a
         * sine-offset position: angle = (int16)rng + 0x4000, the two
         * sub_080009E4 reads offset the spawn by up to 24 pixels. */
        uint8_t t = (uint8_t)(mandragora->timer + 1);
        mandragora->timer = t;
        if ((t & 3) == 0) {
            int32_t angle = (int32_t)(int16_t)rng + 0x4000;
            /* sub_080009E4 returns 16.16; * 3 * 8 keeps it 16.16, so the
             * offsets reach +/- 24 pixels around + 0x40/+ 0x44. */
            int32_t dx = (int32_t)((int64_t)aos_sine((uint32_t)(angle + 0x4000)) * 24);
            int32_t dy = (int32_t)((int64_t)aos_sine((uint32_t)angle) * 24);
            mandragora->leaf = true;
            mandragora->leaf_x = entity_x + dx;
            mandragora->leaf_y = entity_y + dy;
        }
        /* The + 0x6C bit-4 hit flag: palette restore (not ported),
         * sequence 1, + 0x0D = 0x20, state 2, + 0x72 |= 2. */
        if (hit) {
            mandragora->state = 2;
            mandragora->timer = 0x20;
        }
    } else if (mandragora->state == 2) {
        /* + 0x0D counts 0x20 down; at 0 it sets + 0x0F and re-arms 8. */
        if (mandragora->timer == 0) {
            mandragora->flag_f = 1;
            mandragora->timer = 8;
            mandragora->state = 3;
        } else {
            --mandragora->timer;
        }
    } else if (mandragora->state == 3) {
        /* + 0x0D counts the last 8 frames down to the delete. */
        if (mandragora->timer == 0) return false;
        --mandragora->timer;
    }
    /* The tail: margin probe or + 0x59 bit 4 deletes with the palette
     * restore; otherwise sub_0803F17C keeps the animation. */
    if (outside) return false;
    return true;
}
