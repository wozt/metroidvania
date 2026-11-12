/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "aos/game/aos_object07.h"

static uint32_t rng_state = 0xABCDEF;
static uint32_t rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

int main(void) {
    AosObject07 o;

    /* param0 == 0: ambient + anchored children spawn. */
    aos_object07_create(&o, 0, 0, 0x100, 0x80, rng);
    assert(o.ambient_count == 12);
    assert(o.anchor_count == 4);
    for (int i = 0; i < o.ambient_count; ++i) {
        assert(o.motes[i].alive);
        assert(o.motes[i].layer <= 3);
        assert(o.motes[i].fx == ((int64_t)o.motes[i].sx << 16));
    }
    /* Anchored positions: rec + 0x50 - cam + pos[param0], 16.16. */
    assert((o.anchors[0].fx >> 16) == 16 + 0x50 - 0x100 + (int16_t)0xFFB4);
    assert((o.anchors[0].fy >> 16) == 80 - 0x80 + (int16_t)0xFFC8);
    assert(o.anchors[0].vx == 0x80);

    /* param1 selects the ambient count. */
    aos_object07_create(&o, 2, 1, 0, 0, rng);
    assert(o.ambient_count == 15 && o.anchor_count == 0);
    aos_object07_create(&o, 2, 5, 0, 0, rng);
    assert(o.ambient_count == 2);

    /* param0 == 1: no children, the anim cycler runs. */
    aos_object07_create(&o, 1, 0, 0, 0, rng);
    assert(o.ambient_count == 0 && o.anchor_count == 0);
    uint8_t fi = o.frame_index; uint16_t si = o.sub_index;
    aos_object07_update(&o, 0, 0, 0, 0, rng);
    assert(o.frame_index != fi || o.sub_index != si);

    /* param0 == 3: the global writes. */
    aos_object07_create(&o, 3, 0, 0, 0, rng);
    assert(o.clear_4c4 && o.set_42c_800);

    /* param0 == 4: gated on param1. */
    aos_object07_create(&o, 4, 0, 0, 0, rng);
    assert(!o.call_13eec);
    aos_object07_create(&o, 4, 2, 0, 0, rng);
    assert(o.call_13eec);

    /* Mote drift: state 0 -> 1 then pos += vel, parallax applied. */
    aos_object07_create(&o, 0, 0, 0, 0, rng);
    int32_t fx0 = o.motes[0].fx, fy0 = o.motes[0].fy;
    aos_object07_update(&o, 0, 0, 0, 0, rng);   /* state 0 -> 1 */
    aos_object07_update(&o, 0, 0, 0x10000, 0x8000, rng);
    assert(o.motes[0].state == 1);
    assert(o.motes[0].fx != fx0 || o.motes[0].fy != fy0);

    /* Anchored drift: fx += 0x80<<16, scroll/0xC subtracted. */
    int64_t ax = o.anchors[0].fx;
    aos_object07_update(&o, 0, 0, 0xC00 << 16, 0, rng);
    aos_object07_update(&o, 0, 0, 0xC00 << 16, 0, rng);
    /* two drift frames: +0x80 px - (0xC00<<16)/0xC each */
    assert(o.anchors[0].fx == ax + 2 * ((0x80 << 16) - (0xC00 << 16) / 0xC));

    printf("aos_object07: ok\n");
    return 0;
}
