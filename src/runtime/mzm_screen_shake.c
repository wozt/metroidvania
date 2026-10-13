/* SPDX-License-Identifier: GPL-3.0-only */
/* The screen shakes (see mzm_screen_shake.h); comments name the pinned
 * decompilation's routines. */
#include "mzm_screen_shake.h"

#include <string.h>

MzmScreenShake mzm_screen_shake;

static uint8_t start(MzmShakeAxis *axis, uint8_t duration, uint8_t intensity) {
    if (duration != 0 && duration > axis->timer) {
        axis->timer = duration;
        axis->delay = 0;
        axis->intensity = intensity;
        axis->direction = 0;
    }
    return axis->timer;
}

uint8_t mzm_screen_shake_start_vertical(uint8_t duration, uint8_t intensity) {
    return start(&mzm_screen_shake.y, duration, intensity);
}

uint8_t mzm_screen_shake_start_horizontal(uint8_t duration, uint8_t intensity) {
    return start(&mzm_screen_shake.x, duration, intensity);
}

/* SHAKE_UP / SHAKE_LEFT (-1) first, then SHAKE_DOWN / SHAKE_RIGHT (+1) or
 * nothing, every third frame; halved (DIV_SHIFT) in the last 15 frames. */
static int update(MzmShakeAxis *axis) {
    if (axis->timer == 0) return 0;
    axis->timer--;
    if (axis->delay < 2) {
        axis->delay++;
        return 0;
    }
    axis->delay = 0;
    int offset = -2;
    if (axis->direction) offset = axis->intensity & 0x7F ? 2 : 0;
    axis->direction ^= 1;
    if (axis->timer <= 15) offset /= 2;
    return offset;
}

int mzm_screen_shake_update_vertical(void) { return update(&mzm_screen_shake.y); }

int mzm_screen_shake_update_horizontal(void) { return update(&mzm_screen_shake.x); }

void mzm_screen_shake_clear(void) { memset(&mzm_screen_shake, 0, sizeof mzm_screen_shake); }
