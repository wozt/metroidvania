/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "aos/game/aos_object06.h"
#include "aos/core/aos_collision.h"

static uint32_t rng_state = 0x12345678;
static uint32_t rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

static AosCollision layer;

int main(void) {
    AosObject06 o;

    /* Create far (no probe hit, + 0x37E bit 2 clear): seals the column,
     * timer 0, state by param0. */
    memset(&layer, 0, sizeof(layer));
    assert(aos_object06_create(&o, &layer, 0, 0, 0,
                               0x40, 0x40, 0x400, 0x40, 0x100, 0x100));
    assert(o.state == 0 && o.timer == 0);
    assert(o.flags58 == 0);         /* screen x 0x40 <= 0x78 */

    /* Near via the probe: state 0xA, timer 0x30. */
    aos_object06_create(&o, &layer, 0, 0, 0,
                        0x80, 0x40, 0x90, 0x40, 0x100, 0x100);
    assert(o.state == 0xA && o.timer == 0x30);
    assert(o.flags58 == 0x40);      /* right half of the screen */

    /* Near via + 0x37E bit 2 alone. */
    aos_object06_create(&o, &layer, 0, 0, 4,
                        0x40, 0x40, 0x400, 0x40, 0x100, 0x100);
    assert(o.state == 0xA && o.timer == 0x30);

    /* param0 1/3 pick the far entry states. */
    aos_object06_create(&o, &layer, 1, 0, 0,
                        0x40, 0x40, 0x400, 0x40, 0x100, 0x100);
    assert(o.state == 0x14);
    aos_object06_create(&o, &layer, 3, 0, 0,
                        0x40, 0x40, 0x400, 0x40, 0x100, 0x100);
    assert(o.state == 0x63);

    /* Update state 0: + 0x37E bit 0 set -> state 2 immediately. */
    aos_object06_create(&o, &layer, 0, 0, 0,
                        0x40, 0x40, 0x400, 0x40, 0x100, 0x100);
    o.state = 0;
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 1, false, rng);
    assert(o.state == 2);

    /* State 0 particles: counter ticks, burst every frame, drift on
     * odd counters; no trigger while the probe misses. */
    o.state = 0; o.counter = 0;
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, false, rng);
    assert(o.fx_burst && o.fx_drift && o.counter == 1);
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, false, rng);
    assert(o.fx_burst && !o.fx_drift && o.counter == 2);

    /* soma + 0x10 bit 0x80 suppresses the trigger probe. */
    o.counter = 0;
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x40,
                        0xF0, 0x40, 0x80, 0, false, rng);
    assert(!o.triggered && o.song == -1);

    /* Probe hit: song 0x111 + triggered. */
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x40,
                        0xF0, 0x40, 0, 0, false, rng);
    assert(o.triggered && o.song == 0x111);

    /* State 1: forces the direction, counts + 0xD down to state 2. */
    o.state = 1; o.counter = 2; o.flags58 = 0x40;
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, false, rng);
    assert(o.forced == 0x20 && o.counter == 1 && o.state == 1);
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, false, rng);
    assert(o.counter == 0 && o.state == 1);
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, false, rng);
    assert(o.state == 2);

    /* State 2 without the cinematic flag: no forced input, timer
     * counts but the release waits for + 0x4CC bit 6. */
    o.state = 2; o.timer = 0; o.scene_active = true;
    memset(&layer, 0, sizeof(layer));
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, false, rng);
    assert(o.forced == 0 && o.timer == 1);
    o.timer = 0x2F;
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, false, rng);
    assert(o.timer == 0x30 && !o.scene_active && o.state == 2);

    /* With the cinematic flag: forced input and state 3 at the edge. */
    o.state = 2; o.timer = 0x2F; o.scene_active = true;
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, true, rng);
    assert(o.forced == 0x20 && o.state == 3);

    /* State 3 forces unconditionally. */
    o.flags58 = 0;
    aos_object06_update(&o, &layer, 0, 0, 0x100, 0x100,
                        0x400, 0x40, 0, 0, false, rng);
    assert(o.forced == 0x10);

    /* Off-screen flag. */
    o.state = 0;
    aos_object06_update(&o, &layer, 0, 0, -0x100, 0x100,
                        0x400, 0x40, 0, 1, false, rng);
    assert((o.flags58 & 0x20) == 0x20);

    printf("aos_object06: ok\n");
    return 0;
}
