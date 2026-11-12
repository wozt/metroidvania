/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>

#include "aos/game/aos_object05.h"

int main(void) {
    AosObject05 o;

    /* Create: Soma inside the 0x38 x 0x22 probe window arms the
     * timer; outside it stays 0. */
    assert(aos_object05_create(&o, 0x80, 0x40, 0x90, 0x40));
    assert(o.timer == 0x30);
    assert(o.facing_right == true);    /* screen x 0x80 > 0x78 */
    assert(o.variant == 0x7D && o.flags5a == 0x38);

    assert(aos_object05_create(&o, 0x40, 0x40, 0x90, 0x40));
    assert(o.timer == 0);              /* 0x40 - (0x90-0x1C)= -0x34 -> out */
    assert(o.facing_right == false);

    /* Update: the + 0x28 window decides the timer direction.
     * ex_sx - soma_sx + 0x28 <= 0x4F  <=>  soma within reach. */
    aos_object05_create(&o, 0x40, 0x40, 0x90, 0x40);   /* timer 0 */
    /* soma far right: sx - 0x90 + 0x28 with sx=0x40 -> negative u16 huge */
    o.timer = 0;
    aos_object05_update(&o, 0x40, 0x40, 0x200, 0x40);
    assert(o.timer == 0);              /* out of window, timer <= 2 -> 0 */
    /* soma close: sx - soma + 0x28 in [0, 0x4F] */
    aos_object05_update(&o, 0x100, 0x40, 0xE0, 0x40);  /* 0x100-0xE0+0x28=0x48 */
    assert(o.timer == 2);
    /* probe for the song fires only when timer was 0 entering window —
     * place the object inside the 0x110 x 0xC0 zone too (ax=-0x10). */
    o.timer = 0;
    aos_object05_update(&o, 0x100, 0x40, 0xE0, 0x40);
    assert(o.song == 0x113 && o.timer == 2);

    /* Saturate at 0x30 while in the window. */
    o.timer = 0x2E;
    aos_object05_update(&o, 0x100, 0x40, 0xE0, 0x40);
    assert(o.timer == 0x30);
    aos_object05_update(&o, 0x100, 0x40, 0xE0, 0x40);
    assert(o.timer == 0x30 && o.song == -1);

    /* Leaving the window: timer == 0x30 plays the song once, then
     * drains by 2; <= 2 clears. */
    o.timer = 0x30;
    aos_object05_update(&o, 0x100, 0x40, 0x400, 0x40); /* out of window */
    assert(o.song == 0x113 && o.timer == 0x2E);
    o.timer = 2;
    aos_object05_update(&o, 0x100, 0x40, 0x400, 0x40);
    assert(o.timer == 0 && o.song == -1);

    /* Off-screen flag: probe (sx,sy) vs (-0x20,0) 0x130 x 0xD0. */
    aos_object05_update(&o, 0x100, 0x40, 0x400, 0x40);
    assert((o.flags58 & 0x20) == 0);
    aos_object05_update(&o, -0x100, 0x40, 0x400, 0x40);
    assert((o.flags58 & 0x20) == 0x20);

    printf("aos_object05: ok\n");
    return 0;
}
