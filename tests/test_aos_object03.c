/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aos/game/aos_object03.h"
#include "aos/core/aos_collision.h"

static AosCollision layer;

int main(void) {
    AosObject03 o;

    /* Create ALWAYS seals the column and picks the param0 state —
     * no probe, no one-shot. */
    memset(&layer, 0, sizeof(layer));
    assert(aos_object03_create(&o, &layer, 0, 0, 0x40, 0x100, 0x100));
    assert(o.state == 0 && o.timer == 0);
    assert(o.flags58 == 0);
    aos_object03_create(&o, &layer, 1, 0, 0x40, 0x100, 0x100);
    assert(o.state == 0x14);
    aos_object03_create(&o, &layer, 3, 0, 0x40, 0x100, 0x100);
    assert(o.state == 0x63);

    /* State 0 without the cinematic flag advances straight to 2. */
    o.state = 0;
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x100, 0x100, false, false,
                        0x4000, 0x4000);
    assert(o.state == 2);

    /* escort_busy (+ 0x13220) freezes state 0 even without cinematic. */
    o.state = 0;
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x100, 0x100, true, false,
                        0x4000, 0x4000);
    assert(o.state == 0);

    /* Cinematic + Soma outside the window: stays 0, counter reset. */
    o.state = 0; o.counter = 9;
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x300, 0x100, false, true,
                        0x4000, 0x4000);
    assert(o.state == 0 && o.counter == 0 && o.forced == 0);

    /* Cinematic + Soma in window but partners absent: forced walk
     * applies on the first window, then the state waits. */
    o.counter = 0;
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x110, 0x100, false, true,
                        0x4000, 0x4000);
    /* ex_sx - soma + 0x18 = 0x100-0x110+0x18 = 8 <= 0x2F -> forced;
     * +0x13 -> 3 <= 0x26 -> partner check fails (0x4000) -> wait */
    assert(o.forced == 0x10 && o.state == 0);

    /* Both partners abreast -> counter 4, state 1. */
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x110, 0x100, false, true,
                        0x100, 0x100);
    assert(o.state == 1 && o.counter == 4 && o.forced == 0x10);

    /* State 1 counts down then advances. */
    for (int i = 0; i < 4; i++)
        aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                            0x110, 0x100, false, true, 0, 0);
    assert(o.state == 1 && o.counter == 0);
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x110, 0x100, false, true, 0, 0);
    assert(o.state == 2);

    /* State 2 without cinematic: clears blocks + song once, counts,
     * releases, stays (no state 3 without the flag). */
    o.state = 2; o.timer = 0; o.scene_active = true;
    memset(&layer, 0, sizeof(layer));
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x100, false, false, 0, 0);
    assert(o.song == 0x113 && o.timer == 1 && o.forced == 0);
    o.timer = 0x2F;
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x100, false, false, 0, 0);
    assert(o.timer == 0x30 && !o.scene_active && o.state == 2);

    /* With cinematic: forced input and state 3 at the edge. */
    o.state = 2; o.timer = 0x2F; o.scene_active = true;
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x100, false, true, 0, 0);
    assert(o.forced == 0x10 && o.state == 3);

    /* State 3 forces unconditionally (dir from + 0x58 bit 0x40). */
    o.flags58 = 0x40;
    aos_object03_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x100, false, false, 0, 0);
    assert(o.forced == 0x20);

    printf("aos_object03: ok\n");
    return 0;
}
