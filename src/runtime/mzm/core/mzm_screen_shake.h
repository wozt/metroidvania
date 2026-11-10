/* SPDX-License-Identifier: GPL-3.0-only */
/* Zero Mission's screen shakes (screen_shake.c): one shared state, as the
 * game's gScreenShakeX / gScreenShakeY. Samus, the projectiles and the
 * sprites start them; RoomUpdateBackgroundsPosition runs them once a frame
 * and offsets BG0 to BG2 by the result (BG3 by half), never the sprites. */
#ifndef MZM_SCREEN_SHAKE_H
#define MZM_SCREEN_SHAKE_H

#include <stdint.h>

/* struct ScreenShake */
typedef struct {
    uint8_t timer, delay, intensity, direction;
} MzmShakeAxis;

typedef struct {
    MzmShakeAxis x, y;
} MzmScreenShake;

extern MzmScreenShake mzm_screen_shake;

/* ScreenShakeStartVertical / ScreenShakeStartHorizontal: a longer shake
 * replaces the current one; the timer left. Intensity bit 0 shakes both
 * ways, without it only up (left) every other step. */
uint8_t mzm_screen_shake_start_vertical(uint8_t duration, uint8_t intensity);
uint8_t mzm_screen_shake_start_horizontal(uint8_t duration, uint8_t intensity);
/* ScreenShakeUpdateVertical / ScreenShakeUpdateHorizontal: the pixel
 * offset this frame (+-2, +-1 in the last quarter second). */
int mzm_screen_shake_update_vertical(void);
int mzm_screen_shake_update_horizontal(void);
/* The room load's sScreenShake_Empty. */
void mzm_screen_shake_clear(void);

#endif
