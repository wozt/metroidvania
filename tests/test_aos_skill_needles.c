/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_audio.h"
#include "../src/runtime/aos_skill_needles.h"

static uint32_t zero(void) { return 0; }

int main(void) {
    AosNeedles n = aos_needles_create(100 << 16, 186 << 16, false);
    assert(n.x == 120 << 16 && n.y == 160 << 16 && n.timer == 0x80 && n.state == 1);
    bool burst;
    /* 128 frames of waiting, the last one bursting. */
    aos_audio_clear();
    int waited = 0;
    while (n.state == 1) {
        assert(aos_needles_update(&n, zero, 0, 0, 16, 16, &burst));
        waited++;
    }
    assert(waited == 0x80 && burst && n.state == 2 && n.timer == 0x20);
    assert(n.vy == -0x10000 && n.gravity == 0x2000);
    assert(n.angle[0] == 0xF000 && n.angle[1] == (uint16_t)(0xF000 + 0x3333));
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xD7) == 1);
    /* A hit bursts it at once. */
    AosNeedles hit = aos_needles_create(100 << 16, 186 << 16, false);
    hit.struck = true;
    aos_needles_update(&hit, zero, 0, 0, 16, 16, &burst);
    assert(burst && hit.state == 2);
    /* The pieces fly out; each frame only one parity is drawn. */
    aos_needles_update(&n, zero, 0, 0, 16, 16, &burst);
    assert(n.radius[0] == 0x20 << 5 && n.timer == 0x1F);
    int32_t x, y;
    assert(aos_needles_piece(&n, 0, &x, &y) && !aos_needles_piece(&n, 1, &x, &y));
    int frames = 1;
    while (aos_needles_update(&n, zero, -1000, -1000, 4000, 4000, &burst)) frames++;
    assert(frames == 0x20);     /* 32 burst updates, the 33rd deletes it */
    puts("aos skill needles tests passed");
    return 0;
}
