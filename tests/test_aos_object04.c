/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/runtime/aos/game/aos_object04.h"

static AosCollision layer;
static uint8_t cells[64 * 64], blocks[512];

static void reset_layer(void) {
    memset(cells, 0, sizeof cells);
    memset(blocks, 0, sizeof blocks);
    layer = (AosCollision){.width_cells = 64, .height_cells = 64,
                          .cells = cells, .blocks = blocks,
                          .blocks_active = true};
}

static int block_count(void) {
    int n = 0;
    for (size_t i = 0; i < sizeof blocks; ++i) n += blocks[i] != 0;
    return n;
}

int main(void) {
    AosObject04 o;

    /* One-shot delete: param0 <= 1 with the + 0x37E bit already set. */
    reset_layer();
    assert(!aos_object04_create(&o, 0, 3, 1u << 3, 0, 0, 0));
    assert(o.deleted && !o.scene_active);

    /* Directional probe: object right of centre (+0x1A set) wants
     * soma_x >= ex - 0x18. screen_x 200, soma 190 -> near (locked). */
    reset_layer();
    assert(aos_object04_create(&o, 0, 0, 0, 200, 190, 0));
    assert(o.state == 0xA && o.timer == 0x30 && o.force_zero);
    /* soma 150 < 182 -> far: state 0, no blocks placed by create. */
    reset_layer();
    assert(aos_object04_create(&o, 0, 0, 0, 200, 150, 0));
    assert(o.state == 0 && !o.force_zero);
    /* Left-of-centre object (+0x1A clear) wants soma_x <= ex + 0x18:
     * screen_x 50, soma 60 <= 68 -> near; soma 100 -> far. */
    reset_layer();
    assert(aos_object04_create(&o, 0, 0, 0, 50, 60, 0));
    assert(o.state == 0xA);
    reset_layer();
    assert(aos_object04_create(&o, 0, 0, 0, 50, 100, 0));
    assert(o.state == 0);

    /* param0 entry states and the vx/friction clear direction. */
    reset_layer();
    assert(aos_object04_create(&o, 1, 0, 0, 200, 150, 0));
    assert(o.state == 0x14);
    reset_layer();
    assert(aos_object04_create(&o, 3, 0, 0, 200, 150, 0));
    assert(o.state == 0x63);
    /* Near with Soma left of centre moving left -> zero_soma_vx. */
    reset_layer();
    assert(aos_object04_create(&o, 0, 0, 0, 50, 60, -0x1000));
    /* screen_x 50 <= 0x78 -> not facing_right; soma_screen_x 60 <= 0x77
     * and vx < 0 -> cleared. */
    assert(o.zero_soma_vx);
    reset_layer();
    assert(aos_object04_create(&o, 0, 0, 0, 50, 60, 0x1000));
    assert(!o.zero_soma_vx);

    /* State 0 block cycle: same shape as Object02 — entity room
     * (100,100), camera 0; soma screen x 90 -> near window holds. */
    reset_layer();
    aos_object04_create(&o, 0, 0, 0, 100, 90, 0);
    for (int i = 0; i < 30; ++i)
        aos_object04_update(&o, &layer, 0, 0, 100, 100, 90, 100, 0, NULL);
    assert(o.timer == 0x30);
    /* Far drains -2/frame; the 1-2 edge re-seals the column. */
    for (int i = 0; i < 30; ++i)
        aos_object04_update(&o, &layer, 0, 0, 100, 100, 200, 100, 0, NULL);
    assert(block_count() == 3);

    /* Case 2 forces +0x1A ? RIGHT : LEFT with no holding flag. */
    reset_layer();
    aos_object04_create(&o, 0, 0, 0, 200, 150, 0);
    o.state = 2;
    o.facing_right = true;
    aos_object04_update(&o, &layer, 0, 0, 100, 100, 0, 0, 0, NULL);
    assert(o.forced == 0x10 && !o.holding);
    o.facing_right = false;
    aos_object04_update(&o, &layer, 0, 0, 50, 100, 0, 0, 0, NULL);
    assert(o.forced == 0x20);

    /* Case 10 forces the opposite and waits on the directional probe:
     * entity screen (100,100) facing_right false -> wants soma_x <=
     * 118; soma 200 leaves -> reseal + song + state 11. */
    reset_layer();
    aos_object04_create(&o, 0, 0, 0, 50, 100, 0);
    o.state = 10;
    aos_object04_update(&o, &layer, 0, 0, 50, 100, 200, 100, 0, NULL);
    assert(o.state == 11 && o.song == 0x113);
    /* Inside the approach side it holds (soma 100 <= 68? no: ex=50,
     * +0x18 -> soma_x <= 68; use 60). */
    reset_layer();
    aos_object04_create(&o, 0, 0, 0, 50, 60, 0);
    o.state = 10;
    aos_object04_update(&o, &layer, 0, 0, 50, 100, 60, 100, 0, NULL);
    assert(o.state == 10 && o.forced == 0x10);

    /* Case 11 countdown releases without a holding flag. */
    o.state = 11;
    o.timer = 2;
    aos_object04_update(&o, &layer, 0, 0, 50, 100, 60, 100, 0, NULL);
    assert(o.state == 11 && o.timer == 1);
    aos_object04_update(&o, &layer, 0, 0, 50, 100, 60, 100, 0, NULL);
    assert(o.state == 0x14 && !o.scene_active && !o.holding);

    /* Case 20 waits on the + 0x37E bit then songs and advances. */
    reset_layer();
    aos_object04_create(&o, 2, 5, 0, 200, 150, 0);
    o.state = 20; o.param1 = 5;
    aos_object04_update(&o, &layer, 0, 0, 100, 100, 0, 0, 0, NULL);
    assert(o.state == 20);
    aos_object04_update(&o, &layer, 0, 0, 100, 100, 0, 0, 1u << 5, NULL);
    assert(o.state == 21 && o.song == 0x113);

    /* Case 21 clears the column after 0x30 frames, no song. */
    o.timer = 0x2F;
    aos_object04_update(&o, &layer, 0, 0, 100, 100, 0, 0, 0, NULL);
    assert(o.state == 22 && o.song == -1);

    /* Off-screen flag tracks the same window probe. */
    reset_layer();
    aos_object04_create(&o, 0, 0, 0, 200, 150, 0);
    o.state = 3;
    aos_object04_update(&o, &layer, 0, 0, 500, 500, 0, 0, 0, NULL);
    assert(o.flags58 & 0x20);
    aos_object04_update(&o, &layer, 0, 0, 100, 100, 0, 0, 0, NULL);
    assert(!(o.flags58 & 0x20));

    printf("aos_object04: ok\n");
    return 0;
}
