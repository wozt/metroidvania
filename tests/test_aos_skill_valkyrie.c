/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_skill_valkyrie.h"

static uint32_t mid(void) { return 0x10; }

int main(void) {
    AosValkyrie v = aos_valkyrie_create((100 << 16) + 0x8000, 150 << 16, false);
    assert(v.x == 120 << 16 && v.y == 134 << 16 && v.state == 1);
    assert(aos_valkyrie_create(100 << 16, 150 << 16, true).x == 80 << 16);
    AosValkyrieSpark spark;
    /* No sparkle before its 20th frame, then one every fourth frame. */
    int sparks = 0, first = 0;
    for (int frame = 1; frame <= 40; ++frame) {
        assert(aos_valkyrie_update(&v, 0, 0, mid, &spark));
        if (spark.spawn) {
            if (!sparks) first = frame;
            sparks++;
        }
    }
    assert(first == 20 && sparks == 6);
    /* At counter 40 (random 0x10: no offset), the diagonal point. */
    assert(spark.x == v.x + ((40 * 3) << 14) + 0x200000);
    assert(spark.y == v.y + 0x380000 - ((40 * 31) << 12));
    assert(spark.ay == (0x10 << 4) - 0x2000 && spark.offset_x == 0);
    /* Off screen it is hidden. */
    aos_valkyrie_update(&v, 1000, 0, mid, &spark);
    assert(v.hidden);
    /* The animation's end: 10 blinking frames, then deleted. */
    aos_valkyrie_animation_ended(&v);
    assert(v.state == 2 && v.timer == 10 && v.blinking);
    int frames = 0;
    while (aos_valkyrie_update(&v, 0, 0, mid, &spark)) frames++;
    assert(frames == 10);
    puts("aos skill valkyrie tests passed");
    return 0;
}
