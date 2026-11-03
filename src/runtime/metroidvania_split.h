/* SPDX-License-Identifier: GPL-3.0-only */
/* The Metroid Vania launcher's split screen: the two title screens share
 * the 240 x 160 screen across an invisible line through its centre that
 * turns clockwise at 30 degrees a second (half a degree a frame). Aria is
 * on the side the line's normal points to, upward at frame 0; the game
 * covering most of the top half gives the music, START launches the game
 * covering most of the bottom half. No SDL. */
#ifndef METROIDVANIA_SPLIT_H
#define METROIDVANIA_SPLIT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { MV_GAME_ARIA = 0, MV_GAME_MZM = 1 } MvGame;

enum { MV_SCREEN_W = 240, MV_SCREEN_H = 160, MV_FRAMES_PER_TURN = 720 };

/* The line's angle after `frame` frames, in 1/65536 of a turn. */
uint16_t mv_split_angle(uint32_t frame);
/* The game drawn at pixel (x, y): Aria where the pixel centre lies on the
 * side of the normal (sin a, -cos a), the Zero Mission elsewhere. */
MvGame mv_split_pixel(uint16_t angle, int x, int y);
/* The game covering most of the top half: Aria while the normal points up
 * (angle in [0xC000, 0x4000), the half-open turn quarter on each side). */
MvGame mv_split_top(uint16_t angle);
static inline MvGame mv_split_bottom(uint16_t angle) {
    return mv_split_top(angle) == MV_GAME_ARIA ? MV_GAME_MZM : MV_GAME_ARIA;
}
/* Both screens (RGBA, 240 x 160) into one. */
void mv_split_compose(uint16_t angle, const uint8_t *aria, const uint8_t *mzm, uint8_t *out);

#endif
