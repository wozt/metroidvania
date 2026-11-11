/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillWerewolfUse. */
#include "aos/red_souls/aos_soul_red_werewolf.h"

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
    AosWerewolf right = aos_werewolf_create(100 << 16, 150 << 16, false);
    assert(right.body.x == 120 << 16 && right.body.y == 140 << 16);
    assert(!right.body.mirrored && right.state == 1);
    assert(aos_werewolf_ground_ok(&right, &layer));
    AosWerewolf left = aos_werewolf_create(100 << 16, 150 << 16, true);
    assert(left.body.x == 80 << 16 && left.body.mirrored);
    /* No floor anywhere: the cast's sub_0802393C fails. */
    memset(cells, 0, sizeof cells);
    assert(!aos_werewolf_ground_ok(&right, &layer));
}

static void walk_direction(void) {
    floor_row(20);
    AosWerewolf right = aos_werewolf_create(100 << 16, 150 << 16, false);
    assert(aos_werewolf_ground_ok(&right, &layer));
    aos_werewolf_land(&right, &layer, &probes);
    assert(right.body.ground & 1);
    int32_t x0 = right.body.x;
    aos_werewolf_update(&right, &layer, &probes);
    assert(right.body.x > x0);
    AosWerewolf left = aos_werewolf_create(100 << 16, 150 << 16, true);
    aos_werewolf_land(&left, &layer, &probes);
    x0 = left.body.x;
    aos_werewolf_update(&left, &layer, &probes);
    assert(left.body.x < x0);
    /* A wall ahead stops the step (mode 4 keeps the entity in place). */
    for (int y = 0; y < H; ++y) cells[y * W + 20] = 0x03;
    AosWerewolf walled = aos_werewolf_create(100 << 16, 150 << 16, false);
    aos_werewolf_land(&walled, &layer, &probes);
    for (int i = 0; i < 400; ++i)
        aos_werewolf_update(&walled, &layer, &probes);
    assert((walled.body.x >> 16) < 160);
}

int main(void) {
    create_and_ground();
    walk_direction();
    puts("aos_soul_red_werewolf: ok");
    return 0;
}
