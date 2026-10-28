/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/runtime/aos_skill_flea.h"

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};
static const AosProbes probes = {2, -8, 0, 4, {-4, -2}};
static unsigned draws;
static uint32_t rolls[] = {1, 0, 0x8000};
static uint32_t scripted(void) { return rolls[draws++ % 3]; }

int main(void) {
    memset(cells, 0, sizeof cells);
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;     /* floor top 160 */
    AosFlea flea = aos_flea_create(100 << 16, 186 << 16, false);
    assert(flea.body.x == 108 << 16 && flea.body.y == 160 << 16 && flea.body.mirrored);
    assert(flea.state == 1 && flea.timer == 160 && flea.body.ay == 0x2000);
    AosFlea left = aos_flea_create(100 << 16, 186 << 16, true);
    assert(left.body.x == 92 << 16 && !left.body.mirrored);
    /* It walks ahead at 1.5 and hops on landing. */
    bool end;
    int32_t x0 = flea.body.x;
    bool hopped = false, rose = false;
    for (int i = 0; i < 40; ++i) {
        assert(aos_flea_update(&flea, &layer, &probes, scripted, 0, 0, 16, 16, &end));
        if (flea.body.vy < 0) hopped = true;
        if (flea.frame == 0) rose = true;
    }
    assert(flea.body.x > x0 && hopped && rose);
    /* At the end of its 160 frames: the 16-frame ending, then deletion. */
    flea.timer = 0;
    assert(aos_flea_update(&flea, &layer, &probes, scripted, 0, 0, 16, 16, &end));
    assert(end && flea.state == 2 && flea.blinking && flea.timer == 16);
    int frames = 0;
    while (aos_flea_update(&flea, &layer, &probes, scripted, 0, 0, 16, 16, &end)) frames++;
    assert(frames == 16);
    /* Off screen it goes at once. */
    AosFlea far = aos_flea_create(100 << 16, 186 << 16, false);
    assert(!aos_flea_update(&far, &layer, &probes, scripted, 1000, 0, 16, 16, &end));
    /* A wall turns it around. */
    for (int y = 0; y < H; ++y) cells[y * W + 16] = 0x03;     /* x 128..135 */
    AosFlea walled = aos_flea_create(100 << 16, 186 << 16, false);
    bool turned = false;
    for (int i = 0; i < 40 && !turned; ++i) {
        aos_flea_update(&walled, &layer, &probes, scripted, 0, 0, 16, 16, &end);
        turned = !walled.body.mirrored;
    }
    assert(turned && (walled.body.x >> 16) < 128);
    puts("aos skill flea tests passed");
    return 0;
}
