/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos/core/aos_audio.h"

#include "../src/runtime/aos/red_souls/aos_soul_red_dryad.h"

static void flight_tests(void) {
    AosDryad d = aos_dryad_create(100 << 16, 200 << 16, false);
    assert(d.x == 120 << 16 && d.y == 174 << 16);
    assert(d.vx == 0x10000 && d.vy == -0x20000 && d.ay == 0x2000);
    assert(aos_dryad_update(&d));
    assert(d.state == 1);
    /* vy -0x20000 + 0x2000 = -0x1E000; x advances by vx. */
    assert(aos_dryad_update(&d));
    assert(d.vy == -0x1E000 && d.y == (174 << 16) - 0x1E000);
    assert(d.x == (120 << 16) + 0x10000);
    /* Mirrored facing flips vx and the spawn offset. */
    AosDryad m = aos_dryad_create(100 << 16, 200 << 16, true);
    assert(m.x == 80 << 16 && m.vx == -0x10000);
    /* The fall caps at 8 px a frame. */
    for (int i = 0; i < 400; ++i) aos_dryad_update(&d);
    assert(d.vy == 0x40000);
}

static void planted_tests(void) {
    aos_audio_clear();
    AosDryad d = aos_dryad_create(100 << 16, 200 << 16, false);
    aos_dryad_update(&d);
    /* The strike callback planted it: state 2 next update, +0x26 = 0. */
    d.planted = true;
    d.timer = 0x78;
    assert(aos_dryad_update(&d) && d.state == 2);
    /* First planted frame: song 0xD5 and the +8 bonus once. */
    assert(aos_dryad_update(&d) && d.state == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xD5) == 1);
    assert(d.bonus && d.counter == 1);
    /* It counts to 0x14 then deletes through state 4. */
    int frames = 0;
    while (aos_dryad_update(&d)) frames++;
    assert(d.state == 4 && d.counter == 0x15 && frames == 0x14);
}

static void host_tests(void) {
    AosDryad d = aos_dryad_create(100 << 16, 200 << 16, false);
    aos_dryad_update(&d);
    d.planted = true;
    aos_dryad_update(&d);
    /* The host's + 0x34 ran out (child +0x24): straight to delete. */
    d.host_gone = true;
    assert(aos_dryad_update(&d) && d.state == 4);
    assert(!aos_dryad_update(&d));
    /* The host's + 0x72 bit 2 ends it after the counter frame. */
    AosDryad f = aos_dryad_create(100 << 16, 200 << 16, false);
    aos_dryad_update(&f);
    f.planted = true;
    aos_dryad_update(&f);
    f.host_flag = true;
    assert(aos_dryad_update(&f) && f.state == 4);
    /* Animation end (sub_0803F17C == 3) lingers 0x10 frames blinking. */
    AosDryad a = aos_dryad_create(100 << 16, 200 << 16, false);
    aos_dryad_update(&a);
    a.planted = true;
    aos_dryad_update(&a);
    a.anim_done = true;
    assert(aos_dryad_update(&a) && a.state == 3 && a.timer == 0x10);
    int linger = 0;
    while (aos_dryad_update(&a)) {
        assert(a.blink);
        linger++;
    }
    assert(linger == 0x11 && a.state == 4);
    /* The margin probe enters state 4 from any state. */
    AosDryad o = aos_dryad_create(100 << 16, 200 << 16, false);
    aos_dryad_update(&o);
    o.outside = true;
    assert(aos_dryad_update(&o) && o.state == 4);
    assert(!aos_dryad_update(&o));
}

int main(void) {
    flight_tests();
    planted_tests();
    host_tests();
    puts("aos soul red dryad tests passed");
    return 0;
}
