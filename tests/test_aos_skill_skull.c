/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "../src/runtime/aos_audio.h"
#include "../src/runtime/aos_skill_skull.h"

int main(void) {
    aos_audio_reset();
    aos_audio_clear();
    AosSkull skull = aos_skull_create(false);
    assert(skull.dx == 20 && skull.dy == -16 && !skull.facing_left);
    assert(aos_skull_create(true).dx == -20);
    /* It follows Soma's whole position. */
    assert(aos_skull_update(&skull, (100 << 16) + 0x8000, 150 << 16, 0, 0));
    assert(skull.x == 120 << 16 && skull.y == 134 << 16);
    /* Frame 10, tick 0: the beam sound is held, once. */
    aos_skull_update(&skull, 100 << 16, 150 << 16, 10, 0);
    aos_skull_update(&skull, 100 << 16, 150 << 16, 10, 1);
    assert(aos_audio_requested(AOS_AUDIO_START_SONG, 0xCE) == 1 && aos_audio_held(3) == 0xCE);
    /* Holding it again keeps one slot. */
    assert(aos_audio_hold(0xCE) && aos_audio_held(2) == 0);
    /* After the animation: released on the next update, then gone. */
    aos_skull_end(&skull);
    assert(!aos_skull_update(&skull, 100 << 16, 150 << 16, 0, 0));
    assert(aos_audio_requested(AOS_AUDIO_STOP_SONG, 0xCE) == 1 && aos_audio_held(3) == 0);
    puts("aos skill skull tests passed");
    return 0;
}
