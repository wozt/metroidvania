/* SPDX-License-Identifier: GPL-3.0-only */
/* The launcher's rotating split screen. */
#include "metroidvania_split.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* The share of the top half each game covers, in pixels. */
static int aria_in_top(uint16_t angle) {
    int n = 0;
    for (int y = 0; y < MV_SCREEN_H / 2; ++y)
        for (int x = 0; x < MV_SCREEN_W; ++x) n += mv_split_pixel(angle, x, y) == MV_GAME_ARIA;
    return n;
}

int main(void) {
    /* A third of a degree a frame: a quarter turn in 4.5 seconds. */
    assert(mv_split_angle(0) == 0 && mv_split_angle(270) == 0x4000 &&
           mv_split_angle(540) == 0x8000 && mv_split_angle(1080) == 0);
    /* Frame 0: Aria above the horizontal line, Zero Mission below. */
    assert(mv_split_pixel(0, 120, 10) == MV_GAME_ARIA && mv_split_pixel(0, 120, 150) == MV_GAME_MZM);
    assert(mv_split_top(0) == MV_GAME_ARIA && mv_split_bottom(0) == MV_GAME_MZM);
    /* Clockwise: a quarter turn later Aria is on the right. */
    assert(mv_split_pixel(0x4000, 230, 80) == MV_GAME_ARIA &&
           mv_split_pixel(0x4000, 10, 80) == MV_GAME_MZM);
    /* Half a turn: the games have swapped. */
    assert(mv_split_pixel(0x8000, 120, 10) == MV_GAME_MZM && mv_split_top(0x8000) == MV_GAME_MZM &&
           mv_split_bottom(0x8000) == MV_GAME_ARIA);
    /* The top game is the one covering most of the top half, every frame
     * of the turn; where the line is vertical it is a tie. */
    const int half = MV_SCREEN_W * MV_SCREEN_H / 2;
    for (uint32_t frame = 0; frame < MV_FRAMES_PER_TURN; ++frame) {
        uint16_t a = mv_split_angle(frame);
        int aria = aria_in_top(a);
        if (frame == 270 || frame == 810) {
            assert(aria == half / 2);
            continue;
        }
        /* Near the vertical the pixel count may tie. */
        if (mv_split_top(a) == MV_GAME_ARIA) assert(aria >= half / 2);
        else assert(aria <= half / 2);
    }
    /* Composition: each pixel from its game's screen. */
    static uint8_t aria[MV_SCREEN_W * MV_SCREEN_H * 4], mzm[MV_SCREEN_W * MV_SCREEN_H * 4],
        out[MV_SCREEN_W * MV_SCREEN_H * 4];
    memset(aria, 0xAA, sizeof aria);
    memset(mzm, 0x55, sizeof mzm);
    mv_split_compose(0x2000, aria, mzm, out);
    for (int y = 0; y < MV_SCREEN_H; y += 7)
        for (int x = 0; x < MV_SCREEN_W; x += 5)
            assert(out[(y * MV_SCREEN_W + x) * 4] ==
                   (mv_split_pixel(0x2000, x, y) == MV_GAME_ARIA ? 0xAA : 0x55));
    printf("metroidvania_split: ok\n");
    return 0;
}
