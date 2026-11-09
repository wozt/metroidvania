/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_killer_doll.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { W = 64, H = 32 };
static uint8_t cells[W * H];
static const AosCollision layer = {2, 1, W, H, cells, NULL, false};

int main(void) {
    for (int x = 0; x < W; ++x) cells[20 * W + x] = 0x03;       /* floor top 160 */
    /* Thrown from 8 ahead and 16 up at 1.5 forward, 2.5 up. */
    AosDecoy d = aos_decoy_create(100 << 16, 159 << 16, false);
    assert(d.x == 108 << 16 && d.y == 143 << 16 && d.vx == 0x18000 &&
           d.vy == (int32_t)0xFFFD8000 && d.timer == 0xB4 && d.state == 1);
    assert(aos_decoy_update(&d, &layer));
    assert(d.x == (108 << 16) + 0x18000 - 0 && d.vy == (int32_t)0xFFFD8000 + 0x2000 &&
           d.vx == 0x18000 - 0x100 && d.timer == 0xB3);
    /* It rises, falls back, lands and slides to a stop. */
    int frames = 0;
    while (d.vx != 0 && frames < 200) {
        assert(aos_decoy_update(&d, &layer));
        ++frames;
    }
    assert(d.vy == 0 && (d.y >> 16) >= 159 && (d.y >> 16) <= 161 && frames < 60);
    /* After 0xB4 frames it blinks in place for 0x1E more, then vanishes. */
    frames = 0;
    while (d.state == 1 && frames < 300) {
        aos_decoy_update(&d, &layer);
        ++frames;
    }
    assert(d.state == 2 && d.blinking);
    int32_t x = d.x, y = d.y;
    frames = 0;
    while (aos_decoy_update(&d, &layer)) ++frames;
    assert(frames == 0x1D && d.x == x && d.y == y);
    /* Thrown left into a wall at x 96..103 (8-pixel cells) it bounces back
     * at half speed. */
    memset(cells, 0, sizeof cells);
    for (int y = 0; y < H; ++y) cells[y * W + 12] = 0x03;
    d = aos_decoy_create(124 << 16, 159 << 16, true);
    bool bounced = false;
    for (int f = 0; f < 20 && !bounced; ++f) {
        aos_decoy_update(&d, &layer);
        bounced = d.vx > 0;
    }
    assert(bounced && (d.x >> 16) >= 112 && (d.x >> 16) <= 113);
    puts("aos_soul_red_killer_doll: ok");
    return 0;
}
