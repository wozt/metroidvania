/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free regression for MakeBackgroundFlash's palette behavior tables. */
#include "mzm_background_effect.h"

#include <assert.h>
#include <stdio.h>

static void fill(uint16_t palette[256], uint16_t color) {
    for (int i = 0; i < 256; ++i) palette[i] = (uint16_t)(color + i % 7);
}

int main(void) {
    uint16_t palette[256];

    fill(palette, 0x4210);
    uint16_t original = palette[0], animated = palette[240];
    assert(mzm_background_effect_start(MZM_BACKGROUND_EFFECT_QUICK_FLASH));
    assert(!mzm_background_effect_start(MZM_BACKGROUND_EFFECT_SLIGHT_YELLOW));
    mzm_background_effect_update(palette, true);
    assert(palette[0] == original && mzm_background_effect.timer == 0);
    mzm_background_effect_update(palette, false);
    assert(palette[0] == 0x7FFF && palette[239] == 0x7FFF && palette[240] == animated &&
           mzm_background_effect.color_stage == 16);
    for (int i = 0; i < 3; ++i) mzm_background_effect_update(palette, false);
    assert(palette[0] == 0x7FFF);
    mzm_background_effect_update(palette, false);
    assert(palette[0] == (31 << 10));
    mzm_background_effect_update(palette, false);
    mzm_background_effect_update(palette, false);
    assert(mzm_background_effect.color_stage == 14 && palette[0] != 0x7FFF);
    for (int i = 0; i < 6; ++i) mzm_background_effect_update(palette, false);
    assert(palette[0] == original && mzm_background_effect.color_stage == 0);
    mzm_background_effect_update(palette, false);
    assert(mzm_background_effect.type == MZM_BACKGROUND_EFFECT_NONE);

    fill(palette, 0);
    assert(mzm_background_effect_start(MZM_BACKGROUND_EFFECT_SLIGHT_YELLOW));
    for (int i = 0; i < 12; ++i) mzm_background_effect_update(palette, false);
    assert(palette[0] == 0);
    mzm_background_effect_update(palette, false);
    assert(mzm_background_effect.color_stage == 3 && palette[0] == (4 | 4 << 5));
    mzm_background_effect_clear();

    fill(palette, 0);
    assert(mzm_background_effect_start(MZM_BACKGROUND_EFFECT_HEAVY_YELLOW));
    mzm_background_effect_update(palette, false);
    assert(mzm_background_effect.color_stage == 2 && palette[0] == (2 | 2 << 5));
    mzm_background_effect_clear();
    assert(mzm_background_effect.type == MZM_BACKGROUND_EFFECT_NONE);

    printf("mzm_background_effect: ok\n");
    return 0;
}
