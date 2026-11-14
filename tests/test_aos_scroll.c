/* Tests for the ported BG scroll primitives in aos/core/aos_scroll.c:
 * sub_0803FBBC (aos_scroll_request), sub_08000F60
 * (aos_scroll_shift_entities), sub_0800D288 (aos_scroll_apply) and
 * the sub_0800ED24 gate (aos_scroll_transaction). */
#include "../src/runtime/aos/core/aos_scroll.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    AosScroll s;
    AosScrollOut out;

    /* ---- bind (sub_0803F8A8's state half) ---- */
    memset(&s, 0, sizeof s);
    /* A two-screen-wide, one-screen-tall room: meta_0 = 2,
     * meta_1 = 1 pins the y axis like a native one-screen room. */
    aos_scroll_bind(&s, 1, 2, 1, 0, 0, 0, 0x40 << 16, 0x30 << 16);
    assert(s.bg[1].x_pos == (int32_t)0x40 << 16);
    assert(s.bg[1].y_pos == (int32_t)0x30 << 16);
    assert(s.disp_h[1] == 0x40 && s.disp_v[1] == 0x30);

    /* ---- sub_0803FBBC positive x (sub_0803F3F8) ---- */
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, 0x40000, 0, &out);
    /* vx = 4 px, under the cap: xVel = vx and xPos += vx. */
    assert(s.bg[1].x_vel == 0x40000);
    assert(s.bg[1].x_pos == ((int32_t)0x40 << 16) + 0x40000);
    /* meta_1 <= 1 pins yPos to 0x30 and clears yVel. */
    assert(s.bg[1].y_pos == (int32_t)0x30 << 16);
    assert(s.bg[1].y_vel == 0);
    assert(s.disp_h[1] == 0x44);
    assert(out.requests == 1);

    /* A request past the map edge is clamped to the remaining
     * scroll: meta_0 = 2 -> x edge = 2*256 - 240 = 272 px. */
    aos_scroll_bind(&s, 1, 2, 1, 0, 0, 0, 0, 0x30 << 16);
    aos_scroll_request(&s, 1, 0x80000, 0, &out);   /* +8 px cap */
    assert(s.bg[1].x_vel == 0x80000);
    /* Drive it to the edge: each call adds up to 8 px. */
    for (int i = 0; i < 40; ++i)
        aos_scroll_request(&s, 1, 0x80000, 0, NULL);
    /* The clamp formula (meta_0<<24) - (x + 0xF00000) stops the
     * scroll so the right edge (x + 240) stays inside 512 px. */
    assert(s.bg[1].x_pos <= ((int32_t)2 << 24) - 0xF00000);
    assert(s.bg[1].x_vel <= 0x80000);

    /* ---- sub_0803FBBC negative x (sub_0803F4C8) ---- */
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0x10 << 16, 0x80 << 16);
    aos_scroll_request(&s, 1, (int32_t)0xFFFC0000, 0, &out); /* -4 px */
    assert(s.bg[1].x_vel == (int32_t)0xFFFC0000);
    assert(s.bg[1].x_pos == ((int32_t)0x10 << 16) - 0x40000);
    /* A request past the left edge stops at x = 0 (-x wins over
     * vx), then the -8 px cap limits the stored velocity. */
    aos_scroll_request(&s, 1, (int32_t)0xFFF00000, 0, &out); /* -16 px */
    assert(s.bg[1].x_vel == -0x80000);
    assert(s.bg[1].x_pos == ((int32_t)0x10 << 16) - 0x40000 - 0x80000);
    aos_scroll_request(&s, 1, (int32_t)0xFFF00000, 0, &out);
    assert(s.bg[1].x_pos == 0);
    assert(s.bg[1].x_vel == -0x40000);
    /* The -8 px cap applies to large negative requests. */
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0x100 << 16, 0x80 << 16);
    aos_scroll_request(&s, 1, (int32_t)0xFF000000, 0, &out); /* -256 px */
    assert(s.bg[1].x_vel == -0x80000);
    assert(s.bg[1].x_pos == ((int32_t)0x100 << 16) - 0x80000);

    /* ---- sub_0803FBBC y (sub_0803F580 / sub_0803F650) ---- */
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0x40 << 16);
    aos_scroll_request(&s, 1, 0, 0x20000, &out);   /* +2 px */
    assert(s.bg[1].y_vel == 0x20000);
    assert(s.bg[1].y_pos == ((int32_t)0x40 << 16) + 0x20000);
    /* Negative y requests integrate until yPos + vy would cross
     * 0x2FFFFF; then the request becomes the distance back to the
     * 0x30 pin (0x300000 whole). */
    aos_scroll_request(&s, 1, 0, (int32_t)0xFFF00000, &out); /* -16 px */
    assert(s.bg[1].y_pos == ((int32_t)0x40 << 16) + 0x20000 - 0x80000);
    aos_scroll_request(&s, 1, 0, (int32_t)0xFFF00000, &out);
    aos_scroll_request(&s, 1, 0, (int32_t)0xFFF00000, &out);
    assert(s.bg[1].y_pos == (int32_t)0x30 << 16);
    assert(s.bg[1].y_vel == -0x20000);
    /* A huge negative request is capped at -8 px first. */
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0x100 << 16);
    aos_scroll_request(&s, 1, 0, (int32_t)0xFF000000, &out);
    assert(s.bg[1].y_vel == -0x80000);

    /* meta_0 <= 1 pins x to 0 and clears xVel (one-screen pin). */
    aos_scroll_bind(&s, 1, 1, 1, 0, 0, 0, 0x55 << 16, 0x30 << 16);
    aos_scroll_request(&s, 1, 0x40000, 0x40000, &out);
    assert(s.bg[1].x_pos == 0 && s.bg[1].x_vel == 0);
    assert(s.bg[1].y_pos == (int32_t)0x30 << 16 && s.bg[1].y_vel == 0);

    /* A zero velocity clears the stored vel but keeps the pos. */
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0x10 << 16, 0x40 << 16);
    aos_scroll_request(&s, 1, 0x40000, 0, &out);
    aos_scroll_request(&s, 1, 0, 0, &out);
    assert(s.bg[1].x_vel == 0 && s.bg[1].y_vel == 0);
    assert(s.bg[1].x_pos == ((int32_t)0x10 << 16) + 0x40000);

    /* ---- sub_08000F60 entity compensation ---- */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0);
    aos_scroll_request(&s, 1, 0x40000, 0x10000, &out);
    int32_t ex = 0x5000, ey = 0x6000, fx = 0x7000, fy = 0x8000;
    AosScrollEntityRef ents[2] = {
        { true, &ex, &ey },    /* flagged: shifts */
        { false, &fx, &fy },   /* unflagged: stays */
    };
    size_t n = aos_scroll_shift_entities(&s, 1, ents, 2);
    assert(n == 1);
    assert(ex == 0x5000 - 0x40000 && ey == 0x6000 - 0x10000);
    assert(fx == 0x7000 && fy == 0x8000);

    /* ---- sub_0800D288 propagation (modes 1-7) ---- */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0);
    aos_scroll_request(&s, 1, 0x40000, 0x20000, &out);  /* BG1 vel */
    /* BG2 mode 1 (1:1), BG3 mode 5 (x2 on x). */
    aos_scroll_bind(&s, 2, 2, 2, 1, 0, 0, 0, 0);
    aos_scroll_bind(&s, 3, 2, 2, 5, 0, 0, 0, 0);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(s.bg[2].x_vel == 0x40000 && s.bg[2].y_vel == 0x20000);
    assert(s.bg[3].x_vel == 0x80000 && s.bg[3].y_vel == 0x20000);
    /* The requests integrated the positions too. */
    assert(s.bg[2].x_pos == 0x40000 && s.bg[2].y_pos == 0x20000);
    assert(s.bg[3].x_pos == 0x80000);
    assert(out.requests == 2);

    /* Modes 2-4 halve the matching axis. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0);
    aos_scroll_request(&s, 1, 0x40000, 0x20000, &out);
    aos_scroll_bind(&s, 2, 2, 2, 4, 0, 0, 0, 0);
    aos_scroll_bind(&s, 3, 2, 2, 0, 0, 0, 0, 0);   /* mode 0: none */
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(s.bg[2].x_vel == 0x20000 && s.bg[2].y_vel == 0x10000);
    assert(s.bg[3].x_vel == 0 && s.bg[3].y_vel == 0);
    assert(out.requests == 1);

    /* Mode 8/9 write bgOfs directly; mode 10 is flag-gated. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0);
    aos_scroll_request(&s, 1, 0x40000, 0, &out);
    aos_scroll_bind(&s, 2, 2, 2, 8, 0, 0, 0, 0);
    aos_scroll_bind(&s, 3, 2, 2, 10, 0, 0, 0, 0);
    s.disp_h[2] = 0x100; s.disp_v[3] = 0x200;
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(s.disp_h[2] == 0x101);
    assert(s.disp_v[3] == 0x200);          /* flag clear: no -2 */
    AosScrollWorld w = { 0x02000000, false, NULL, 0 };
    memset(&out, 0, sizeof out);
    s.disp_v[3] = 0x200;
    aos_scroll_apply(&s, &w, &out);
    assert(s.disp_v[3] == 0x1FE);

    /* Interpolation modes 11-13 scale by unk_14/unk_16. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0xF0, 0xD0, 0, 0);
    aos_scroll_request(&s, 1, 0x40000, 0x20000, &out);
    aos_scroll_bind(&s, 2, 2, 2, 13, 0x178, 0x168, 0, 0); /* halfway */
    aos_scroll_bind(&s, 3, 2, 2, 0, 0, 0, 0, 0);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    /* (0x40000 * (0x178-0xF0)) / (0xF0-0xF0) -> den 0 -> 0 */
    assert(s.bg[2].x_vel == 0);
    /* Give BG1 a real range: unk_14 = 0x140 -> den = 0x50. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0x140, 0x110, 0, 0);
    aos_scroll_request(&s, 1, 0x40000, 0x20000, &out);
    aos_scroll_bind(&s, 2, 2, 2, 13, 0x118, 0xE8, 0, 0);
    aos_scroll_bind(&s, 3, 2, 2, 0, 0, 0, 0, 0);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    /* x: 0x40000*(0x118-0xF0)/(0x140-0xF0) = 0x40000*0x28/0x50
     *   = 0x40000*40/80 = 0x20000 */
    assert(s.bg[2].x_vel == 0x20000);
    /* y: 0x20000*(0xE8-0xD0)/(0x110-0xD0) = 0x20000*0x18/0x40
     *   = 0x20000*24/64 = 0xC000 */
    assert(s.bg[2].y_vel == 0xC000);

    /* Unported modes report fx but still run their velocity
     * request where the native does (case 31). */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0x1A, 0, 0, 0, 0);
    aos_scroll_request(&s, 1, 0x40000, 0, &out);
    aos_scroll_bind(&s, 2, 2, 2, 31, 0, 0, 0, 0);
    aos_scroll_bind(&s, 3, 2, 2, 27, 0, 0, 0, 0);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(out.fx & AOS_SCROLL_FX_CDAC);
    assert(out.fx & AOS_SCROLL_FX_HBLANK);
    assert(out.fx & AOS_SCROLL_FX_AFFINE);
    assert(s.bg[2].x_vel == 0x40000);       /* case 31 requests */
    assert(s.bg[3].x_vel == 0);             /* case 27: affine only */

    /* ---- sub_0800ED24 gate (aos_scroll_transaction) ---- */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0x10 << 16, 0x40 << 16);
    memset(&out, 0, sizeof out);
    /* gate byte 2 > param0 1: suppressed, no state change. */
    int ran = aos_scroll_transaction(&s, 2, 1, 0x40000, 0, NULL, &out);
    assert(ran == 0);
    assert(s.bg[1].x_vel == 0 && s.bg[1].x_pos == (int32_t)0x10 << 16);
    /* gate byte 1 <= param0 1: runs request + shift + apply. */
    ran = aos_scroll_transaction(&s, 1, 1, 0x40000, 0x20000, NULL, &out);
    assert(ran == 1);
    assert(s.bg[1].x_vel == 0x40000 && s.bg[1].y_vel == 0x20000);
    assert(s.bg[1].x_pos == ((int32_t)0x10 << 16) + 0x40000);
    assert(s.bg[1].y_pos == ((int32_t)0x40 << 16) + 0x20000);
    assert(out.entities_shifted == 0);
    /* param0 0 with gate byte 1: suppressed (the room-transition
     * settle call uses param0 0). */
    ran = aos_scroll_transaction(&s, 1, 0, 0x40000, 0, NULL, &out);
    assert(ran == 0);

    puts("aos_scroll: ok");
    return 0;
}
