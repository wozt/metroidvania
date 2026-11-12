/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/game/aos_object0f.h"

static uint32_t rng_state = 0x12345678;
static uint32_t test_rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return rng_state >> 16;
}

static void create_splits_palette_row(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0x0705);
    assert(o.palette_row == 7);
    assert(o.param0 == 5);
    assert(o.state == 0 && !o.finished);
    AosObject0f p;
    aos_object0f_create(&p, 0x0009);
    assert(p.palette_row == 7);    /* high byte 0 defaults to 7 */
    assert(p.param0 == 9);
    AosObject0f q;
    aos_object0f_create(&q, 0x0203);
    assert(q.palette_row == 2);
    assert(q.param0 == 3);
}

static void probe_miss_sets_offscreen_and_skips(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0);
    aos_object0f_update(&o, false, true, 0x10000, 0x20000, test_rng);
    assert(o.flags58 & 0x20);
    assert(o.state == 0);          /* the hit never ran */
    assert(o.song == -1);
}

static void hit_starts_break_sequence(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0);
    aos_object0f_update(&o, true, true, 0x10000, 0x20000, test_rng);
    assert(o.song == 0xA2);
    assert(o.state == 1);
    assert(o.timer == 0x28);
    assert(o.flags58 & 0x20);
    /* Next frame with no hit: state 1 counts down, no song. */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng);
    assert(o.song == -1);
    assert(o.state == 1);
    assert(o.timer == 0x27);
}

static void particles_only_late_and_on_four(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0);
    o.state = 1;
    o.timer = 0x25;                /* above 0x1E, not multiple of 4: none */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng);
    assert(!o.particle);
    o.timer = 0x24;                /* > 0x1E and %4==0: one drift */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng);
    assert(o.particle);
    /* Jitter bounded by sub_0802D5EC(0x80000): |d| <= 0x3FFFF. */
    assert(o.particle_x >= 0x10000 - 0x40000 &&
           o.particle_x <= 0x10000 + 0x3FFFF);
    assert(o.particle_y >= 0x20000 - 0x40000 &&
           o.particle_y <= 0x20000 + 0x3FFFF);
    o.unk5a = 0xFF;                /* bits 3-6 -> palette 0x1F & 0xF = 0xF */
    o.timer = 0x24;
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng);
    assert(o.particle_palette == 0xF);
    o.timer = 0x10;                /* at/below 0x1E: no more particles */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng);
    assert(!o.particle);
}

static void timer_wrap_marks_finished(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0);
    o.state = 1;
    o.timer = 1;
    aos_object0f_update(&o, true, false, 0, 0, test_rng);
    assert(o.timer == 0);
    assert(!o.finished);
    aos_object0f_update(&o, true, false, 0, 0, test_rng);
    assert(o.timer == 0xFF);
    assert(o.finished);
}

int main(void) {
    create_splits_palette_row();
    probe_miss_sets_offscreen_and_skips();
    hit_starts_break_sequence();
    particles_only_late_and_on_four();
    timer_wrap_marks_finished();
    puts("aos_object0f: ok");
    return 0;
}
