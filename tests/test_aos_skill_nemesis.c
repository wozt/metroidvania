/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_nemesis.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* 300 frames shown, rising one pixel a frame from 9 to 38 above Soma. */
    AosNemesisSoul s = aos_nemesis_soul_create();
    assert(s.state == 1 && s.timer == 0x12C && !s.blinking);
    assert(aos_nemesis_soul_update(&s));
    assert(s.timer == 0x12B && s.rise == 1 && aos_nemesis_soul_dy(&s) == -9);
    int frames = 1;
    while (!s.blinking) {
        assert(aos_nemesis_soul_update(&s));
        ++frames;
    }
    /* The 301st update starts the blink (state 2, 30 frames). */
    assert(frames == 301 && s.state == 2 && s.blink == 0x1D && s.rise == 30 &&
           aos_nemesis_soul_dy(&s) == -38);
    while (aos_nemesis_soul_update(&s)) ++frames;
    assert(frames == 301 + 29 && s.blink == 0);
    puts("aos_skill_nemesis: ok");
    return 0;
}
