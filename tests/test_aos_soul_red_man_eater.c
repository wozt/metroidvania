/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../src/runtime/aos/red_souls/aos_soul_red_man_eater.h"

static uint32_t seq[8];
static unsigned seq_i;
static uint32_t test_rng(void) { return seq[seq_i++ % 8]; }

int main(void) {
    /* soma at pixel (100, 80), facing right. The variant roll reads
     * the SECOND rng draw (the first is discarded): 7 % 3 = 1. */
    seq[0] = 0xAAAA; seq[1] = 7;
    seq_i = 0;
    AosManEater m = aos_man_eater_create(100 << 16, 80 << 16, false, test_rng);
    assert(m.variant == 1);
    assert(m.base == 5);
    assert(m.x == (100 + 8) << 16);   /* +8 px along the facing */
    assert(m.y == (80 - 0x1A) << 16); /* 0x1A px up */
    assert(m.vx == 0x12000 && m.vy == 0);
    assert(m.state == 1 && m.frame == 5);

    /* Facing left: -8 px and -0x12000. */
    seq_i = 0;
    AosManEater l = aos_man_eater_create(100 << 16, 80 << 16, true, test_rng);
    assert(l.x == (100 - 8) << 16 && l.vx == -0x12000);

    /* State 1: 30 updates of flight. frame = base + tick/10. */
    uint32_t steps = 0;
    while (m.state == 1 && steps < 40) {
        assert(aos_man_eater_update(&m, test_rng));
        steps++;
        if (steps <= 10) assert(m.frame == 5);
        else if (steps <= 20) assert(m.frame == 6);
        else assert(m.frame == 7);
    }
    assert(m.state == 2 && m.timer == 0x30);
    assert(m.x == (108 << 16) + 30 * 0x12000);

    /* State 2: 48 updates of + 0xD=0x30 countdown. */
    steps = 0;
    while (m.state == 2 && steps < 60) {
        assert(aos_man_eater_update(&m, test_rng));
        steps++;
    }
    assert(m.state == 3 && m.timer == 0x10 && m.flag_f == 1);
    assert(steps == 49); /* 48 countdowns + the transition update */

    /* State 3: timer drains to 0 over 16 updates, then the next
     * update deletes through the state-5 bookkeeping. */
    steps = 0;
    while (aos_man_eater_update(&m, test_rng)) steps++;
    assert(m.state == 3 || m.state == 5);
    assert(m.released);

    /* The bite: a contact while the state is <= 2 sticks it — vx=0,
     * frame base+4, state 4 for 16 frames, then state 3 for 16 more. */
    seq_i = 0;
    AosManEater b = aos_man_eater_create(0, 0, false, test_rng);
    b.hit = true;
    assert(aos_man_eater_update(&b, test_rng));
    assert(b.state == 4 && b.vx == 0 && b.frame == 9 && b.timer == 0x10);
    steps = 0;
    while (b.state == 4 && steps < 40) {
        assert(aos_man_eater_update(&b, test_rng));
        steps++;
    }
    assert(b.state == 3 && b.timer == 0x10);
    steps = 0;
    while (aos_man_eater_update(&b, test_rng)) steps++;
    assert(b.released);

    /* The margin probe deletes with the same bookkeeping. */
    seq_i = 0;
    AosManEater o = aos_man_eater_create(0, 0, false, test_rng);
    o.outside = true;
    assert(!aos_man_eater_update(&o, test_rng));
    assert(o.released);

    /* A hit in state 3 does not convert (the tail gates state <= 2). */
    seq_i = 0;
    AosManEater s = aos_man_eater_create(0, 0, false, test_rng);
    while (s.state == 1) assert(aos_man_eater_update(&s, test_rng));
    while (s.state == 2) assert(aos_man_eater_update(&s, test_rng));
    assert(s.state == 3);
    s.hit = true;
    assert(aos_man_eater_update(&s, test_rng));
    assert(s.state == 3);
    printf("aos_soul_red_man_eater: ok\n");
    return 0;
}
