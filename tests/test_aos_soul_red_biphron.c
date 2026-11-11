/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillBiphronUse and sub_080286BC. */
#include "aos/red_souls/aos_soul_red_biphron.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};
/* 0x084F13B4: one wall probe, ceiling -12, floor 0, half width 5. */
static const AosProbes probes = {1, -12, 0, 5, {-7}};

static void floor_row(int y) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
}

static void create_and_ground(void) {
    floor_row(20);
    AosBiphron right = aos_biphron_create(100 << 16, 150 << 16, false);
    assert(right.body.x == 120 << 16 && right.body.y == 140 << 16);
    assert(!right.body.mirrored);
    assert(right.speed == 0x8000 && right.timer == 0x40);
    assert(aos_biphron_ground_ok(&right, &layer));
    AosBiphron left = aos_biphron_create(100 << 16, 150 << 16, true);
    assert(left.body.x == 80 << 16 && left.body.mirrored);
    memset(cells, 0, sizeof cells);
    assert(!aos_biphron_ground_ok(&right, &layer));
}

static void ramp_and_walk(void) {
    floor_row(20);
    AosBiphron b = aos_biphron_create(100 << 16, 150 << 16, false);
    assert(aos_biphron_ground_ok(&b, &layer));
    aos_biphron_land(&b, &layer, &probes);
    assert(b.body.ground & 1);
    bool totem = false;
    int totems = 0;
    int32_t x0 = b.body.x;
    /* + 0x24 starts at 0x8000 and gains 0x1000 per frame toward 0x1FFFF. */
    for (int i = 0; i < 8; ++i) {
        assert(aos_biphron_update(&b, &layer, &probes, &totem));
        totems += totem;
    }
    assert(b.speed == 0x8000 + 8 * 0x1000);
    assert(b.body.x != x0);
    /* The 0x40-tick lifetime ends it; totems fire at + 0x0D & 7 == 0,
     * nine times from 0x40 down to 0. */
    int frames = 0;
    totem = false;
    while (aos_biphron_update(&b, &layer, &probes, &totem)) {
        totems += totem;
        ++frames;
    }
    totems += totem;    /* the delete frame's totem request */
    assert(frames <= 0x40 && totems == 9);
}

static void wall_flip(void) {
    floor_row(20);
    /* A wall column just ahead of the spawn point (it walks right). */
    for (int y = 0; y < H; ++y) cells[y * W + 17] = 0x03;
    AosBiphron b = aos_biphron_create(100 << 16, 150 << 16, false);
    aos_biphron_land(&b, &layer, &probes);
    bool totem = false;
    bool flipped = false;
    for (int i = 0; i < 0x40 && aos_biphron_update(&b, &layer, &probes, &totem); ++i)
        flipped |= b.body.mirrored;
    assert(flipped);
}

int main(void) {
    create_and_ground();
    ramp_and_walk();
    wall_flip();
    puts("aos_soul_red_biphron: ok");
    return 0;
}
