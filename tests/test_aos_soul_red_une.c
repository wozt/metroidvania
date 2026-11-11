/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM-free deterministic tests for SkillUneUse. */
#include "aos/red_souls/aos_soul_red_une.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};
/* 0x084F13AA-like probes: one wall probe, ceiling -12, floor 0,
 * half width 5, wall offset -7. */
static const AosProbes probes = {1, -12, 0, 5, {-7}};

static void floor_row(int y) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[y * W + x] = 0x03;
}

int main(void) {
    floor_row(20);
    /* sub_08031EA4(entity, source, 8, -0x1A): spawn 8px to the facing
     * side and 0x1A above the source. */
    AosUne u = aos_une_create(100 << 16, 150 << 16, false);
    assert(u.state == 0 && u.speed == 0xA0000);
    assert(u.body.x == 108 << 16 && u.body.y == 124 << 16);
    assert(!u.body.mirrored && !u.shot1 && !u.shot2);
    assert(aos_une_update(&u, &layer, &probes, false, false));
    assert(u.state == 1);
    /* Walk until the floor contact (result & 8) requests song 0xE0. */
    int frames = 0;
    while (!u.shot1 && frames < 0x400) {
        assert(aos_une_update(&u, &layer, &probes, false, false));
        ++frames;
    }
    assert(u.shot1 && u.state == 2 && u.timer == 0x80);
    /* The 0x80-frame linger ends in state 3 with the song-0xE8 shot. */
    for (int i = 0; i < 0x80; ++i)
        assert(aos_une_update(&u, &layer, &probes, false, false));
    assert(u.state == 3 && u.shot2);
    /* State 3 only leaves through the + 0x6C bit-4 hit flag. */
    for (int i = 0; i < 4; ++i)
        assert(aos_une_update(&u, &layer, &probes, false, false));
    assert(u.state == 3);
    assert(aos_une_update(&u, &layer, &probes, true, false));
    /* The margin probe deletes from any state. */
    AosUne m = aos_une_create(100 << 16, 150 << 16, false);
    aos_une_update(&m, &layer, &probes, false, false);
    /* Entering state 4 survives the frame; the next update deletes. */
    assert(aos_une_update(&m, &layer, &probes, false, true));
    assert(!aos_une_update(&m, &layer, &probes, false, false));
    puts("aos_soul_red_une: ok");
    return 0;
}
