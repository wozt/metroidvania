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
    aos_object0f_create(&o, 0x0705, 0);
    assert(o.palette_row == 7);
    assert(o.param0 == 5);
    assert(o.state == 0 && !o.finished);
    AosObject0f p;
    aos_object0f_create(&p, 0x0009, 0);
    assert(p.palette_row == 7);    /* high byte 0 defaults to 7 */
    assert(p.param0 == 9);
    AosObject0f q;
    aos_object0f_create(&q, 0x0203, 0);
    assert(q.palette_row == 2);
    assert(q.param0 == 3);
}

static void probe_miss_sets_offscreen_and_skips(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0, 0);
    aos_object0f_update(&o, false, true, 0x10000, 0x20000, test_rng, false);
    assert(o.flags58 & 0x20);
    assert(o.state == 0);          /* the hit never ran */
    assert(o.song == -1);
}

static void hit_starts_break_sequence(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0, 0);
    aos_object0f_update(&o, true, true, 0x10000, 0x20000, test_rng, false);
    assert(o.song == 0xA2);
    assert(o.state == 1);
    assert(o.timer == 0x28);
    assert(o.flags58 & 0x20);
    assert(o.burst);               /* the break flourish fires once */
    /* Next frame with no hit: state 1 counts down, no song. */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng, false);
    assert(o.song == -1);
    assert(o.state == 1);
    assert(o.timer == 0x27);
}

static void particles_only_late_and_on_four(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0, 0);
    o.state = 1;
    o.timer = 0x25;                /* above 0x1E, not multiple of 4: none */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng, false);
    assert(!o.particle);
    o.timer = 0x24;                /* > 0x1E and %4==0: one drift */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng, false);
    assert(o.particle);
    /* Jitter bounded by sub_0802D5EC(0x80000): |d| <= 0x3FFFF. */
    assert(o.particle_x >= 0x10000 - 0x40000 &&
           o.particle_x <= 0x10000 + 0x3FFFF);
    assert(o.particle_y >= 0x20000 - 0x40000 &&
           o.particle_y <= 0x20000 + 0x3FFFF);
    o.unk5a = 0xFF;                /* bits 3-6 -> palette 0x1F & 0xF = 0xF */
    o.timer = 0x24;
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng, false);
    assert(o.particle_palette == 0xF);
    o.timer = 0x10;                /* at/below 0x1E: no more particles */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng, false);
    assert(!o.particle);
}

static void timer_wrap_marks_finished(void) {
    AosObject0f o;
    aos_object0f_create(&o, 0, 0);
    o.state = 1;
    o.timer = 1;
    aos_object0f_update(&o, true, false, 0, 0, test_rng, false);
    assert(o.timer == 0);
    assert(!o.finished);
    aos_object0f_update(&o, true, false, 0, 0, test_rng, false);
    assert(o.timer == 0xFF);
    assert(o.finished);
}

static void drop_dispatch_on_hit(void) {
    /* Case 0, not full MP: a sub_08043C10 heart, big = (param1 != 0). */
    AosObject0f o;
    aos_object0f_create(&o, 0x0700, 1);
    aos_object0f_update(&o, true, true, 0x10000, 0x20000, test_rng, false);
    assert(o.drop_kind == AOS_OBJECT0F_DROP_HEART);
    assert(o.drop_big);
    /* Case 0, full MP on Soma: sub_08043F70 of (RNG & 1) + param1. */
    AosObject0f m;
    aos_object0f_create(&m, 0x0700, 2);
    rng_state = 0x12345678;
    aos_object0f_update(&m, true, true, 0x10000, 0x20000, test_rng, true);
    assert(m.drop_kind == AOS_OBJECT0F_DROP_MONEY);
    assert(m.drop_variant == 2 || m.drop_variant == 3);  /* (RNG&1) + 2 */
    /* Selector 1 (param0 low byte): money of variant param1, no roll. */
    AosObject0f s1;
    aos_object0f_create(&s1, 0x0701, 4);
    aos_object0f_update(&s1, true, true, 0x10000, 0x20000, test_rng, false);
    assert(s1.drop_kind == AOS_OBJECT0F_DROP_MONEY);
    assert(s1.drop_variant == 4);
    /* Selector 2: sub_08044054(x, y, 2, param1) item argument. */
    AosObject0f s2;
    aos_object0f_create(&s2, 0x0702, 9);
    aos_object0f_update(&s2, true, true, 0x10000, 0x20000, test_rng, false);
    assert(s2.drop_kind == AOS_OBJECT0F_DROP_ITEM);
    assert(s2.drop_item == 9);
    /* The drop latches once: a later frame reports NONE again. */
    aos_object0f_update(&o, true, false, 0x10000, 0x20000, test_rng, false);
    assert(o.drop_kind == AOS_OBJECT0F_DROP_NONE);
    /* burst is a one-shot on the hit frame. */
    assert(!o.burst);
}

int main(void) {
    create_splits_palette_row();
    probe_miss_sets_offscreen_and_skips();
    hit_starts_break_sequence();
    drop_dispatch_on_hit();
    particles_only_late_and_on_four();
    timer_wrap_marks_finished();
    puts("aos_object0f: ok");
    return 0;
}
