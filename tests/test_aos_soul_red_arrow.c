/* SPDX-License-Identifier: GPL-3.0-only */
/* The Skull Archer soul's arrow: flight, stop on a hit, 16-frame blink. */
#include "aos/red_souls/aos_soul_red_arrow.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosArrow a = aos_arrow_start(0x100000, false);
    assert(a.state == 1 && a.vx == 0x60000);
    assert(aos_arrow_update(&a, false) && a.x == 0x160000 && a.state == 1);
    AosArrow left = aos_arrow_start(0x100000, true);
    assert(aos_arrow_update(&left, false) && left.x == 0xA0000);
    /* The frame of the hit still moves it, then it waits 16 frames. */
    assert(aos_arrow_update(&a, true) && a.x == 0x1C0000 && a.state == 2 && a.timer == 0x10);
    for (int k = 0; k < 16; ++k) assert(aos_arrow_update(&a, false) && a.x == 0x1C0000);
    assert(a.timer == 0 && !aos_arrow_update(&a, false));
    puts("aos/red_souls/aos_soul_red_arrow: ok");
    return 0;
}
