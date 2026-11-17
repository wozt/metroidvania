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

    /* Anchored drift: + 0x40 += + 0x48 verbatim (vel 0x80 is already
     * 16.16 — no << 16), scroll/0xC subtracted. */
    int64_t ax = o.anchors[0].fx;
    aos_object07_update(&o, 0, 0, 0xC00 << 16, 0, rng);
    aos_object07_update(&o, 0, 0, 0xC00 << 16, 0, rng);
    /* two drift frames: +0x80 - (0xC00<<16)/0xC each */
    assert(o.anchors[0].fx == ax + 2 * (0x80 - (int32_t)((0xC00 << 16) / 0xC)));

    /* --- sub_0804F160 state 1: sine velocities and the anchor wrap
     * on a hand-built single mote (speeds zeroed so only the wrap
     * and the phase advance move the state). --- */
    memset(&o, 0, sizeof(o));
    o.param0 = 2;
    o.ambient_count = 1;
    AosObject07Mote *m = &o.motes[0];
    m->alive = true;
    m->state = 1;
    m->layer = 1;

    /* Non-layer-3: +0x48 = +0x50 verbatim, +0x4C sinusoidal in
     * +0x14 (+= 0x800 per frame) — the y velocity must vary and
     * change sign over the 32-frame phase period. */
    m->kind3 = 0;
    m->speed_x = 0x4000;
    m->speed_y = 0x2000;
    m->timer = 0;
    m->fx = 0x40 * 0x10000;
    m->fy = 0x80 * 0x10000;
    int32_t vy_min = 0x7FFFFFFF, vy_max = (int32_t)0x80000000;
    int distinct_vy = 0;
    int32_t prev_vy = 0x7FFFFFFF;
    for (int f = 0; f < 40; ++f) {
        aos_object07_update(&o, 0, 0, 0, 0, rng);
        assert(m->vx == m->speed_x);           /* x stays constant */
        if (m->vy < vy_min) vy_min = m->vy;
        if (m->vy > vy_max) vy_max = m->vy;
        if (m->vy != prev_vy) ++distinct_vy;
        prev_vy = m->vy;
    }
    assert(m->timer == 40 * 0x800);            /* +0x14 advance */
    assert(vy_min < 0 && vy_max > 0);         /* sine swings both ways */
    assert(distinct_vy > 4);                   /* not a constant drift */

    /* Layer 3: +0x18 += 0xA00 and BOTH axes sinusoidal — vx must
     * not stay pinned to speed_x. */
    m->kind3 = 1;
    m->layer = 3;
    m->speed_x = 0x800;
    m->speed_y = 0x800;
    m->phase2 = 0;
    m->timer = 0;
    int vx_varies = 0;
    int32_t last_vx = 0x7FFFFFFF;
    for (int f = 0; f < 40; ++f) {
        aos_object07_update(&o, 0, 0, 0, 0, rng);
        if (m->vx != last_vx) ++vx_varies;
        last_vx = m->vx;
    }
    assert(m->phase2 == 40 * 0xA00);           /* +0x18 advance */
    assert(vx_varies > 4);

    /* Wrap edges with zeroed speeds (velocities all 0): */
    m->kind3 = 0;
    m->layer = 0;
    m->speed_x = m->speed_y = 0;
    m->timer = m->phase2 = 0;

    /* px > 0x110 -> +0x42 = -0x20 and +0x46 = (rng & 0x7F) + 1;
     * the 16.16 fraction survives the strh pair. */
    m->fx = 0x111 * 0x10000 | 0x1234;
    m->fy = 0x40 * 0x10000 | 0x5678;
    uint32_t rs = rng_state;
    uint32_t draw = (rs * 1103515245u + 12345u) >> 16;
    int expect_py = (int)(draw & 0x7F) + 1;
    aos_object07_update(&o, 0, 0, 0, 0, rng);
    assert((int16_t)(m->fx >> 16) == -0x20);
    assert((m->fx & 0xFFFF) == 0x1234);
    assert((int16_t)(m->fy >> 16) == expect_py);
    assert((m->fy & 0xFFFF) == 0x5678);
    assert(m->sx == -0x20 && m->sy == expect_py);

    /* px < -0x20 -> +0x42 = 0x110 with the same +0x46 reseed. */
    m->fx = -0x21 * 0x10000 | 0x1111;
    m->fy = 0x40 * 0x10000;
    rs = rng_state;
    draw = (rs * 1103515245u + 12345u) >> 16;
    expect_py = (int)(draw & 0x7F) + 1;
    aos_object07_update(&o, 0, 0, 0, 0, rng);
    assert((int16_t)(m->fx >> 16) == 0x110);
    assert((m->fx & 0xFFFF) == 0x1111);
    assert((int16_t)(m->fy >> 16) == expect_py);

    /* py > 0xC0 (x in range) -> +0x46 = -0x10 and +0x42 = rng & 0xBF. */
    m->fx = 0x40 * 0x10000 | 0x2222;
    m->fy = 0xC1 * 0x10000 | 0x3333;
    rs = rng_state;
    draw = (rs * 1103515245u + 12345u) >> 16;
    int expect_px = (int)(draw & 0xBF);
    aos_object07_update(&o, 0, 0, 0, 0, rng);
    assert((int16_t)(m->fy >> 16) == -0x10);
    assert((m->fy & 0xFFFF) == 0x3333);
    assert((int16_t)(m->fx >> 16) == expect_px);
    assert((m->fx & 0xFFFF) == 0x2222);

    /* py < -0x20 -> +0x46 = 0xB0 and +0x42 = rng & 0xBF. */
    m->fx = 0x40 * 0x10000;
    m->fy = -0x21 * 0x10000;
    rs = rng_state;
    draw = (rs * 1103515245u + 12345u) >> 16;
    expect_px = (int)(draw & 0xBF);
    aos_object07_update(&o, 0, 0, 0, 0, rng);
    assert((int16_t)(m->fy >> 16) == 0xB0);
    assert((int16_t)(m->fx >> 16) == expect_px);

    /* Inclusive boundaries: px = 0x110 / -0x20 and py = 0xC0 /
     * -0x20 do NOT wrap — no rng is consumed. */
    m->fx = 0x110 * 0x10000 | 0x4444;
    m->fy = 0xC0 * 0x10000 | 0x5555;
    rs = rng_state;
    aos_object07_update(&o, 0, 0, 0, 0, rng);
    assert(rng_state == rs);   /* boundary values: no wrap, no rng */
    assert((int16_t)(m->fx >> 16) == 0x110);
    assert((int16_t)(m->fy >> 16) == 0xC0);
    m->fx = -0x20 * 0x10000;
    m->fy = -0x20 * 0x10000;
    rs = rng_state;
    aos_object07_update(&o, 0, 0, 0, 0, rng);
    assert(rng_state == rs);   /* boundary values: no wrap, no rng */
    assert((int16_t)(m->fx >> 16) == -0x20);
    assert((int16_t)(m->fy >> 16) == -0x20);

    /* x-wrap takes precedence over y: px > 0x110 with py > 0xC0
     * still reseeds +0x46 (not +0x42) and teleports x to -0x20. */
    m->fx = 0x111 * 0x10000;
    m->fy = 0xC1 * 0x10000;
    rs = rng_state;
    draw = (rs * 1103515245u + 12345u) >> 16;
    expect_py = (int)(draw & 0x7F) + 1;
    aos_object07_update(&o, 0, 0, 0, 0, rng);
    assert((int16_t)(m->fx >> 16) == -0x20);
    assert((int16_t)(m->fy >> 16) == expect_py);

    printf("aos_object07: ok\n");
    return 0;
}
