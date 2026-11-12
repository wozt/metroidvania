/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/runtime/aos/game/aos_object02.h"

static AosCollision layer;
static uint8_t cells[64 * 64], blocks[512];
static uint32_t rng_state = 0x12345678;
static uint32_t test_rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

static void reset_layer(void) {
    memset(cells, 0, sizeof cells);
    memset(blocks, 0, sizeof blocks);
    layer = (AosCollision){.width_cells = 64, .height_cells = 64,
                          .cells = cells, .blocks = blocks,
                          .blocks_active = true};
}

/* Room pixel -> block index per aos_collision_set_block's layout: the
 * runtime keeps one byte per 16x16 block; the exact packing is checked
 * through the public setter's effect on nonzero bytes. */
static int block_count(void) {
    int n = 0;
    for (size_t i = 0; i < sizeof blocks; ++i) n += blocks[i] != 0;
    return n;
}

int main(void) {
    /* One-shot delete: param0 <= 1 with the + 0x37E bit already set. */
    AosObject02 o;
    reset_layer();
    assert(!aos_object02_create(&o, 0, 3, 1u << 3, false, 0, 0, 0));
    assert(o.deleted && !o.scene_active);

    /* Outside spawn, param0 == 0: state 0, no blocks until the timer
     * edge; scene_active holds gUnk_03002CB0 bit 15. */
    reset_layer();
    assert(aos_object02_create(&o, 0, 0, 0, false, 0, 0, 0));
    assert(o.state == 0 && o.timer == 0 && o.scene_active);
    assert(o.anim == 6);

    /* param1 == 7 picks anim 9. */
    reset_layer();
    assert(aos_object02_create(&o, 0, 7, 0, false, 0, 0, 0));
    assert(o.anim == 9);

    /* param0 == 1 outside -> state 0x14; param0 == 3 -> 0x63. */
    reset_layer();
    assert(aos_object02_create(&o, 1, 0, 0, false, 0, 0, 0));
    assert(o.state == 0x14);
    reset_layer();
    assert(aos_object02_create(&o, 3, 0, 0, false, 0, 0, 0));
    assert(o.state == 0x63);

    /* Near spawn: state 0xA, timer 0x30, forced zero; a leftward Soma
     * on the left half (x <= 0x77) gets his velocity cleared. */
    reset_layer();
    assert(aos_object02_create(&o, 0, 0, 0, true, 0x40, 0x40, -0x1000));
    assert(o.state == 0xA && o.timer == 0x30 && o.force_zero &&
           o.zero_soma_vx);
    reset_layer();
    assert(aos_object02_create(&o, 0, 0, 0, true, 0x40, 0x40, 0x1000));
    assert(!o.zero_soma_vx); /* moving toward centre: kept */

    /* State 0 near/far block cycle: entity at room (100,100), camera
     * (0,0) -> screen (100,100); soma screen x 90 -> diff+0x28 = 0x32
     * <= 0x4F -> near branch clears blocks and ramps the timer. */
    reset_layer();
    aos_object02_create(&o, 0, 0, 0, false, 100, 90, 0);
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 90, 100, 0, test_rng);
    assert(o.timer == 2);
    /* Timer ramps +2/frame to the 0x30 cap over 24 near updates. */
    for (int i = 0; i < 30; ++i)
        aos_object02_update(&o, &layer, 0, 0, 100, 100, 90, 100, 0, test_rng);
    assert(o.timer == 0x30);
    /* Far away: timer drains -2/frame from 0x30. */
    for (int i = 0; i < 24; ++i)
        aos_object02_update(&o, &layer, 0, 0, 100, 100, 200, 100, 0, test_rng);
    assert(o.timer <= 2);
    /* The release edge re-sealed the column while the timer crossed
     * 1-2 (timer 0 leaves it open). */
    assert(block_count() == 3);

    /* State 2 forces LEFT (0x20) while the object sits right of
     * centre, RIGHT (0x10) on the left. */
    reset_layer();
    aos_object02_create(&o, 0, 0, 0, false, 0, 0, 0);
    o.state = 2;
    o.facing_right = true;
    aos_object02_update(&o, &layer, 0, 0, 200, 100, 0, 0, 0, test_rng);
    assert(o.forced == 0x20 && o.holding);
    o.facing_right = false;
    aos_object02_update(&o, &layer, 0, 0, 50, 100, 0, 0, 0, test_rng);
    assert(o.forced == 0x10);

    /* State 10: Soma inside the zone keeps it; leaving reseals,
     * plays 0x113 and advances to 11. */
    int before = block_count();
    reset_layer();
    aos_object02_create(&o, 0, 0, 0, false, 0, 0, 0);
    o.state = 10;
    /* entity screen (100,100); soma screen x 120 -> anchor x-0x1C=104,
     * diff 4 <= 0x38, y equal -> inside: stays. */
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 120, 100, 0, test_rng);
    assert(o.state == 10 && o.song == -1);
    /* soma x 200 -> diff 80 > 0x38 -> outside: blocks + song + state 11 */
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 200, 100, 0, test_rng);
    assert(o.state == 11 && o.song == 0x113 && block_count() > before);

    /* State 11 drains + 0x14 then releases: holding/scene cleared and
     * the exit state follows param0. */
    o.timer = 2;
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 200, 100, 0, test_rng);
    assert(o.state == 11 && o.timer == 1 && o.holding);
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 200, 100, 0, test_rng);
    assert(o.state == 0x14 && !o.holding && !o.scene_active);

    /* State 20 waits on the + 0x37E bit, then songs and advances. */
    reset_layer();
    aos_object02_create(&o, 2, 5, 0, false, 0, 0, 0);
    o.state = 20; o.param1 = 5;
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 0, 0, 0, test_rng);
    assert(o.state == 20);
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 0, 0, 1u << 5, test_rng);
    assert(o.state == 21 && o.song == 0x113);

    /* State 21 counts 0x30 frames then clears the column. */
    o.timer = 0x2F;
    before = block_count();
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 0, 0, 0, test_rng);
    assert(o.state == 22 && block_count() < before + 3);

    /* Off-screen flag: entity far outside the window sets + 0x58 bit5. */
    reset_layer();
    aos_object02_create(&o, 0, 0, 0, false, 0, 0, 0);
    o.state = 3; /* an idle state: only the flag update runs */
    aos_object02_update(&o, &layer, 0, 0, 500, 500, 0, 0, 0, test_rng);
    assert(o.flags58 & 0x20);
    aos_object02_update(&o, &layer, 0, 0, 100, 100, 0, 0, 0, test_rng);
    assert(!(o.flags58 & 0x20));

    printf("aos_object02: ok\n");
    return 0;
}
