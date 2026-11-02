/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_lightning.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* 16 ahead of Soma and 28 above his feet, reach 16. */
    AosLightning l = aos_lightning_create(100 << 16, 200 << 16, false, true);
    assert(l.x == 116 << 16 && l.y == 172 << 16 && l.reach == 0x10 && l.vflip);
    AosBox box = aos_lightning_box(&l);
    assert(box.x == 0 && box.y == -16 && box.w == 0x10 && box.h == 32);
    /* Up to frame 3 it follows Soma while he casts (flag 0x100). */
    aos_lightning_update(&l, 2, 0x100, 120 << 16, 200 << 16, true);
    assert(l.x == 104 << 16 && l.reach == 0x10 && !l.facing_left);
    aos_lightning_update(&l, 3, 0, 160 << 16, 200 << 16, false);
    assert(l.x == 104 << 16);
    /* Past frame 3 it no longer follows and reaches 8 further a frame,
     * up to 0x80. */
    for (int k = 0; k < 30; ++k) aos_lightning_update(&l, 5, 0x100, 0, 0, false);
    assert(l.x == 104 << 16 && l.reach == 0x80 && aos_lightning_box(&l).w == 0x80);
    aos_lightning_update(&l, 8, 0, 0, 0, false);
    assert(l.palette_reloaded);
    puts("aos_skill_lightning: ok");
    return 0;
}
