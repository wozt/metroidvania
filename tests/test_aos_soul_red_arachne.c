/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/red_souls/aos_soul_red_arachne.h"

static void flight_tests(void) {
    /* Arachne record 6: vx 0x30000, vy -0x10000, gravity 0x2000. */
    AosArachneSoul a = aos_arachne_create(100 << 16, 200 << 16, false, 6,
                                          false, 0x30000, -0x10000, 0x2000);
    assert(a.x == 108 << 16 && a.y == 174 << 16);
    assert(a.vx == 0x30000 && a.vy == -0x10000 && a.ay == 0x2000);
    /* State 0 falls into state 1's movement on the same frame:
     * vy -0x10000 + 0x2000 = -0xE000, x += 0x30000. */
    assert(aos_arachne_update(&a, false, false));
    assert(a.state == 1);
    assert(a.vy == -0xE000 && a.y == (174 << 16) - 0xE000);
    assert(a.x == (108 << 16) + 0x30000);
    /* Second frame: vy -0xC000. */
    assert(aos_arachne_update(&a, false, false));
    assert(a.vy == -0xC000 && a.y == (174 << 16) - 0xE000 - 0xC000);
    assert(a.x == (108 << 16) + 0x60000);
    /* Mirrored facing negates the record vx. */
    AosArachneSoul m = aos_arachne_create(100 << 16, 200 << 16, true, 6,
                                          false, 0x30000, -0x10000, 0x2000);
    assert(m.x == 92 << 16 && m.vx == -0x30000);
    /* Mudman record 7 keeps its own speeds and the release flag. */
    AosArachneSoul d = aos_arachne_create(100 << 16, 200 << 16, false, 7,
                                          true, 0x28000, -0x20000, 0x2000);
    assert(d.vx == 0x28000 && d.vy == -0x20000 && d.releases);
}

static void arachne_linger_tests(void) {
    AosArachneSoul a = aos_arachne_create(100 << 16, 200 << 16, false, 6,
                                          false, 0x30000, -0x10000, 0x2000);
    aos_arachne_update(&a, false, false);
    /* The strike planted the child: state 2 with the 0x10 linger. */
    a.planted = true;
    assert(aos_arachne_update(&a, false, false) && a.state == 2);
    assert(a.timer == 0x10 && a.blink && a.frame5);
    /* States 2/3 do not move. */
    int32_t x = a.x, y = a.y;
    int frames = 0;
    while (aos_arachne_update(&a, false, false)) {
        assert(a.x == x && a.y == y);
        frames++;
    }
    /* timer 0x10 -> 0x10 updates in state 2 (the ==0 check precedes
     * the decrement). */
    assert(frames == 0x10);
    /* The host running out deletes early. */
    AosArachneSoul b = aos_arachne_create(100 << 16, 200 << 16, false, 6,
                                          false, 0x30000, -0x10000, 0x2000);
    aos_arachne_update(&b, false, false);
    b.planted = true;
    aos_arachne_update(&b, false, false);
    b.host_gone = true;
    assert(!aos_arachne_update(&b, false, false));
}

static void mudman_release_tests(void) {
    AosArachneSoul d = aos_arachne_create(100 << 16, 200 << 16, false, 7,
                                          true, 0x28000, -0x20000, 0x2000);
    aos_arachne_update(&d, false, false);
    d.planted = true;
    assert(aos_arachne_update(&d, false, false));
    assert(d.state == 3 && d.released);
    /* State 3 waits on the sequence-2 animation end or the host. */
    assert(aos_arachne_update(&d, false, false));
    assert(!aos_arachne_update(&d, false, true));
    AosArachneSoul e = aos_arachne_create(100 << 16, 200 << 16, false, 7,
                                          true, 0x28000, -0x20000, 0x2000);
    aos_arachne_update(&e, false, false);
    e.planted = true;
    aos_arachne_update(&e, false, false);
    e.host_gone = true;
    assert(!aos_arachne_update(&e, false, false));
    /* The margin probe deletes from any state. */
    AosArachneSoul o = aos_arachne_create(100 << 16, 200 << 16, false, 6,
                                          false, 0x30000, -0x10000, 0x2000);
    assert(!aos_arachne_update(&o, true, false));
}

static void shard_tests(void) {
    /* parent vx 0x28000 -> base -0x14000; vx draw 0xC0 -> +0x8000;
     * vy draw 0 -> -0x8000; frame draw 0 -> 10. */
    AosArachneShard s = aos_arachne_shard_create(50 << 16, 60 << 16,
                                                 0x28000, 0, 0xC0, 0);
    assert(s.x == 50 << 16 && s.y == 60 << 16);
    assert(s.vx == -0xC000 && s.vy == -0x8000);
    assert(s.frame == 10 && s.alive);
    assert(aos_arachne_shard_update(&s, false));
    assert(s.x == (50 << 16) - 0xC000);
    assert(s.y == (60 << 16) - 0x8000);
    assert(s.vy == -0x8000 + 0x4000);
    /* Odd frame draw picks still 11. */
    AosArachneShard t = aos_arachne_shard_create(0, 0, 0, 1, 0x80, 0);
    assert(t.frame == 11 && t.vx == 0);
    /* The window probe deletes. */
    assert(!aos_arachne_shard_update(&t, true) && !t.alive);
    assert(!aos_arachne_shard_update(&t, false));
}

int main(void) {
    flight_tests();
    arachne_linger_tests();
    mudman_release_tests();
    shard_tests();
    puts("aos soul red arachne tests passed");
    return 0;
}
