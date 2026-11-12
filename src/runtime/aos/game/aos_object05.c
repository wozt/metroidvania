/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_object05.h"

/* sub_08068AD4: (u16)(ex - ax) <= w && (u16)(ey - ay) <= h on screen
 * pixels; the caller passes room pixels minus the camera. */
static bool probe(int32_t ex, int32_t ey, int32_t ax, int32_t ay,
                  uint16_t w, uint16_t h) {
    return (uint16_t)(ex - ax) <= w && (uint16_t)(ey - ay) <= h;
}

bool aos_object05_create(AosObject05 *o, int32_t screen_x,
                         int32_t screen_y, int32_t soma_screen_x,
                         int32_t soma_screen_y) {
    (void)screen_y;
    *o = (AosObject05){
        .state = 0, .timer = 0,
        /* + 0x1A: the shared sub_0804DF3C init sets it from screen x. */
        .facing_right = screen_x > 0x78, .variant = 0x7D,
        .flags5a = 0x38, .flags58 = 0, .song = -1,
    };
    /* sub_0804DF3C sets + 0x1B = 6, + 0x5C = 0x16, + 0x65 = 0x3F —
     * overwritten to 0x7D above — and the winOut 0x3F/0x2F blend lives
     * in the renderer, not modelled here. The probe is a 0x38 x 0x22
     * window anchored at (soma_x - 0x1C, soma_y). */
    if (probe(screen_x, screen_y, soma_screen_x - 0x1C, soma_screen_y,
              0x38, 0x22))
        o->timer = 0x30;
    return true;
}

void aos_object05_update(AosObject05 *o, int32_t sx, int32_t sy,
                         int32_t soma_screen_x, int32_t soma_screen_y) {
    (void)soma_screen_y;
    o->song = -1;
    /* The + 0x58 off-screen bit: in-window flag clear. */
    o->flags58 = (o->flags58 & ~0x20) |
                 (probe(sx, sy, -0x20, 0, 0x130, 0xD0) ? 0 : 0x20);
    if ((uint16_t)(sx - soma_screen_x + 0x28) <= 0x4F) {
        if (o->timer == 0 &&
            probe(sx, sy, -0x10, 0, 0x110, 0xC0))
            o->song = 0x113;
        o->timer += 2;
        if (o->timer > 0x2F) o->timer = 0x30;
    } else {
        if (o->timer == 0x30 &&
            probe(sx, sy, -0x10, 0, 0x110, 0xC0))
            o->song = 0x113;
        o->timer = o->timer <= 2 ? 0 : o->timer - 2;
    }
}
