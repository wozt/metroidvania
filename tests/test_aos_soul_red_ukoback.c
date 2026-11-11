/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillUkobackUse. */
#include "aos/red_souls/aos_soul_red_ukoback.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    AosUkoback u = aos_ukoback_create();
    assert(u.state == 0 && u.timer == 0xF0 && u.ramp == 0);
    /* State 0 enters state 1; + 0x24 ramps 0x1000/frame while <= 0xFFFF. */
    assert(aos_ukoback_update(&u, false, false));
    assert(u.state == 1);
    for (int i = 0; i < 8; ++i)
        assert(aos_ukoback_update(&u, false, false));
    assert(u.ramp == 9 * 0x1000);
    /* + 0x0D counts 0xF0 down to 0, then state 2 decays -0x2000/frame. */
    int frames = 0;
    while (u.state == 1 && frames < 0x200) {
        assert(aos_ukoback_update(&u, false, false));
        ++frames;
    }
    assert(u.state == 2);
    frames = 0;
    while (aos_ukoback_update(&u, false, false)) ++frames;
    assert(frames <= 0x10);
    /* The margin probe deletes from any state. */
    AosUkoback m = aos_ukoback_create();
    aos_ukoback_update(&m, false, false);
    assert(!aos_ukoback_update(&m, false, true));
    /* The + 0x18 force-end zeroes + 0x0D and reaches state 2 early. */
    AosUkoback f = aos_ukoback_create();
    aos_ukoback_update(&f, false, false);
    assert(aos_ukoback_update(&f, true, false));
    assert(f.state == 2);
    puts("aos_soul_red_ukoback: ok");
    return 0;
}
