/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/runtime/aos_audio.h"

#include "../src/runtime/aos_skill_throw.h"

/* A record shaped like the Skeleton's (parameter 3): half a pixel forward,
 * four up, gravity 0x2000, spin 0x0B00 (values chosen for the test). */
static const AosThrowRecord bone = {0x8000, -0x40000, 0x2000, 7, 0, 0, 0, 0xE6, 0x0080,
                                    0x0B00, 0};

static void flight_tests(void) {
    assert(aos_throw_supported(&bone));
    AosThrow e = aos_throw_create(100 << 16, 200 << 16, false);
    assert(e.x == 108 << 16 && e.y == 174 << 16 && e.state == 0);
    /* The first update only sets the speeds, then the tail turns it. */
    assert(aos_throw_update(&e, &bone, 0, 0, 16, 16));
    assert(e.state == 1 && e.vx == 0x8000 && e.vy == -0x40000 && e.x == 108 << 16);
    assert(e.angle == 0x0B00 && e.spin == 0x0B00);
    /* Flying: the gravity joins the speed before the move. */
    assert(aos_throw_update(&e, &bone, 0, 0, 16, 16));
    assert(e.vy == -0x3E000 && e.y == (174 << 16) - 0x3E000 && e.x == (108 << 16) + 0x8000);
    assert(e.angle == 0x1600);
    /* The fall is capped at 8 pixels a frame (camera following it). */
    for (int i = 0; i < 400; ++i) aos_throw_update(&e, &bone, (e.x >> 16) - 120, (e.y >> 16) - 80, 16, 16);
    assert(e.vy == 0x80000 && e.state == 1);
    /* The tail tests the moved position: y 176 is still on the margin
     * (160 + 16), y 184 is past it and the next update deletes it. */
    AosThrow edge = aos_throw_create(100 << 16, 200 << 16, false);
    aos_throw_update(&edge, &bone, 0, 0, 16, 16);
    edge.vy = 0x80000 - 0x2000;
    edge.y = (176 << 16) - 0x80000;
    assert(aos_throw_update(&edge, &bone, 0, 0, 16, 16) && edge.state == 1 && edge.y >> 16 == 176);
    assert(aos_throw_update(&edge, &bone, 0, 0, 16, 16) && edge.state == 3 && edge.y >> 16 == 184);
    assert(!aos_throw_update(&edge, &bone, 0, 0, 16, 16));
}

static void facing_left_tests(void) {
    AosThrow e = aos_throw_create(100 << 16, 200 << 16, true);
    assert(e.x == 92 << 16);
    aos_throw_update(&e, &bone, 0, 0, 16, 16);
    assert(e.vx == -0x8000 && e.spin == -0x0B00 && e.angle == (uint16_t)-0x0B00);
    aos_throw_update(&e, &bone, 0, 0, 16, 16);
    assert(e.x == (92 << 16) - 0x8000);
}

static void refused_tests(void) {
    AosThrowRecord other = bone;
    other.flags = 0x0044;
    assert(aos_throw_supported(&other));
    other.flags = AOS_THROW_ANIMATED;
    assert(!aos_throw_supported(&other));
    other.flags = 0x0060;          /* the flash of 0x20 is not ported */
    assert(!aos_throw_supported(&other));
}

/* Parameter 5's shape: a fused grenade rolling at 1.125, thrown up at 1.625. */
enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};
static const AosProbes probes = {1, -16, 0, 6, {-8}};
static const AosThrowRecord grenade = {0x12000, -0x1A000, 0x2000, 7, 3, 0, 0, 0xE6, 0x0044, 0, 0};

static void grenade_tests(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;     /* floor top 160 */
    aos_audio_clear();
    AosThrow g = aos_throw_create(100 << 16, 186 << 16, false);
    assert(aos_throw_update_world(&g, &grenade, &layer, &probes, 0, 0, 16, 16));
    assert(g.state == 1 && g.timer == 0x50 && !g.attacking && g.roll_speed == 0x12000);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xE6) == 1);
    /* It rolls ahead, lands, bounces once it falls fast, then stops. */
    int bounces = 0;
    int32_t x0 = g.x;
    aos_audio_clear();
    for (int i = 0; i < 0x4F; ++i) {
        assert(aos_throw_update_world(&g, &grenade, &layer, &probes, 0, 0, 16, 16));
        assert(g.state == 1);
    }
    bounces = aos_audio_requested(AOS_AUDIO_START_SONG, 0x11B);
    assert(g.x > x0 && bounces == 1 && g.roll_speed == 0 && (g.y >> 16) == 159);
    /* The 0x50th frame of its fuse explodes it: attack on, hidden, 0xD7. */
    aos_audio_clear();
    assert(aos_throw_update_world(&g, &grenade, &layer, &probes, 0, 0, 16, 16));
    assert(g.exploded && g.state == 2 && g.attacking && g.hidden && g.timer == 2);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xD7) == 1);
    /* 2 + 1 frames in state 2, 16 + 1 in state 4, then gone. */
    int frames = 0;
    while (aos_throw_update_world(&g, &grenade, &layer, &probes, 0, 0, 16, 16)) frames++;
    assert(frames == 3 + 17 && g.state == 3);
    /* A wall turns it around. */
    for (int y = 0; y < H; ++y) cells[y * W + 16] = 0x03;     /* x 128..135 */
    AosThrow w = aos_throw_create(100 << 16, 186 << 16, false);
    bool turned = false;
    for (int i = 0; i < 40 && !turned; ++i) {
        aos_throw_update_world(&w, &grenade, &layer, &probes, 0, 0, 16, 16);
        turned = w.facing_left;
    }
    assert(turned && (w.x >> 16) < 128);
}

int main(void) {
    flight_tests();
    facing_left_tests();
    refused_tests();
    grenade_tests();
    puts("aos skill throw tests passed");
    return 0;
}
