/* Tests for the ported BG scroll primitives in aos/core/aos_scroll.c:
 * sub_0803FBBC (aos_scroll_request), sub_08000F60
 * (aos_scroll_shift_entities), sub_0800D288 (aos_scroll_apply) and
 * the sub_0800ED24 gate (aos_scroll_transaction). */
#include "../src/runtime/aos/core/aos_scroll.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Stub tile-word source for the rebuild DMA tests: records the
 * span arguments and fills `count` deterministic words
 * (0x2000 base + bg<<8 + 0x8000 when column + index). `ret`,
 * when nonzero, shortens the reported write count. */
struct scroll_test_fetch_log {
    int calls;
    int bg;
    int column;
    uint16_t tile_x, tile_y;
    size_t count;
    size_t ret;
};

static size_t scroll_test_fetch(void *context, int bg, bool column,
                                uint16_t tile_x, uint16_t tile_y,
                                size_t count, uint16_t *out) {
    struct scroll_test_fetch_log *log = context;
    ++log->calls;
    log->bg = bg;
    log->column = column;
    log->tile_x = tile_x;
    log->tile_y = tile_y;
    log->count = count;
    for (size_t i = 0; i < count; ++i)
        out[i] = (uint16_t)(0x2000u + ((uint32_t)bg << 8) +
                            (column ? 0x8000u : 0u) + i);
    return log->ret ? log->ret : count;
}

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

    /* ---- tile-row rebuild DMA descriptor (sub_0803F3F8 block) ----
     * A +8 px x request from 64 px crosses the (pos-0x10000)>>19
     * probe; the native then runs 20 sub_08001994 lookups and one
     * BgCmdBuffer_WriteData(0x80, 0x15) to the computed VRAM
     * address. Tile coords come from the OLD integer position:
     * tile_x = (xInt + 0xF7) >> 3 = (64+247)>>3 = 38,
     * tile_y = yInt >> 3 = 48>>3 = 6. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0x40 << 16, 0x30 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, 0x80000, 0, &out);
    assert(out.map_rebuild_x && !out.map_rebuild_y);
    assert(out.rebuild_x.queued && out.rebuild_x.bg == 1);
    assert(out.rebuild_x.tile_x == 38 && out.rebuild_x.tile_y == 6);
    assert(out.rebuild_x.count == 0x15);
    assert(out.rebuild_x.bg_cmd == 0x80 && out.rebuild_x.length == 0x15);
    /* 0x0600E000 + (1<<11) + (38&0x1F)<<1 + (6&0x1F)<<6
     * = 0x0600E000 + 0x800 + 0xC + 0x180 = 0x0600E98C */
    assert(out.rebuild_x.vram_addr == 0x0600E98Cu);
    assert(!out.rebuild_y.queued);

    /* No boundary crossing: from 65 px a +1 px request stays in
     * the same (pos-0x10000)>>19 slot — no rebuild is queued. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0x41 << 16, 0x30 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, 0x10000, 0, &out);
    assert(!out.map_rebuild_x && !out.rebuild_x.queued);

    /* Negative x (sub_0803F4C8): from 8 px a -8 px request lands
     * on 0 and crosses x>>19 1 -> 0. tile_x = ((8-8)<<13)>>16 = 0;
     * tile_y = 32>>3 = 4 on BG2: 0x0600E000 + (2<<11) + 0 + 0x100
     * = 0x0600F100. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 2, 4, 4, 0, 0, 0, 0x8 << 16, 0x20 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 2, (int32_t)0xFFF80000, 0, &out);
    assert(out.map_rebuild_x);
    assert(out.rebuild_x.bg == 2);
    assert(out.rebuild_x.tile_x == 0 && out.rebuild_x.tile_y == 4);
    assert(out.rebuild_x.vram_addr == 0x0600F100u);
    assert(s.bg[2].x_pos == 0);

    /* Negative x near the left edge wraps the tile coordinate the
     * way the asm does: xInt = 4 -> t = -4 -> ((-4)<<13)>>16
     * = 0xFFFF (the u16 lookup arg sub_08001994 receives). */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0x4 << 16, 0x10 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, (int32_t)0xFFF80000, 0, &out);
    /* x + vx = 4px - 8px < 0 -> v = -x, lands on 0; x>>19 was 0
     * already (4px < 8px tile), no crossing, no rebuild. */
    assert(!out.rebuild_x.queued);
    assert(s.bg[1].x_pos == 0);

    /* Positive y (sub_0803F580): from 64 px a +8 px request
     * crosses the y probe; 30 lookups, WriteData(0, 0x1F).
     * tile_x = xInt>>3 = 0, tile_y = (64+0xA7)>>3 = 231>>3 = 28:
     * 0x0600E000 + 0x800 + 0 + (28&0x1F)<<6 = 0x0600EF00. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0, 0x40 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, 0, 0x80000, &out);
    assert(out.map_rebuild_y && !out.map_rebuild_x);
    assert(out.rebuild_y.queued && out.rebuild_y.bg == 1);
    assert(out.rebuild_y.tile_x == 0 && out.rebuild_y.tile_y == 28);
    assert(out.rebuild_y.count == 0x1F);
    assert(out.rebuild_y.bg_cmd == 0x00 && out.rebuild_y.length == 0x1F);
    assert(out.rebuild_y.vram_addr == 0x0600EF00u);

    /* Negative y (sub_0803F650): from 72 px a -8 px request lands
     * on 64 px and crosses y>>19 1 -> 0 (72px>>19... 0x480000>>19
     * = 9, 0x400000>>19 = 8 — both slots differ? recompute: the
     * unshifted probe compares y>>19 before/after: 9 vs 8). */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0x8 << 16, 0x48 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, 0, (int32_t)0xFFF80000, &out);
    assert(out.map_rebuild_y);
    /* tile_x = xInt>>3 = 8>>3 = 1 (iterated start); tile_y uses
     * the neg helper's (yInt - 8) wrap: t = 72-8 = 64 ->
     * (64<<13)>>16 = 8. vram = 0x0600E000 + (1<<11) + (1&0x1F)<<1
     * + (8&0x1F)<<6 = 0x0600E000 + 0x800 + 2 + 0x200 = 0x0600EA02. */
    assert(out.rebuild_y.tile_x == 1);
    assert(out.rebuild_y.tile_y == 8);
    assert(out.rebuild_y.vram_addr == 0x0600EA02u);

    /* ---- unk_60.unk_4C8 reporting (modes 20/21/27/30/36) ---- */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0);
    aos_scroll_request(&s, 1, 0x40000, 0, &out);
    aos_scroll_bind(&s, 2, 2, 2, 20, 0, 0, 0, 0);
    aos_scroll_bind(&s, 3, 2, 2, 27, 0, 0, 0, 0);
    AosScrollWorld w4 = { 0x02000000, false, 0x55, NULL, 0 };
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, &w4, &out);
    /* mode 20: hblank only; mode 21's flag is set but BG3 runs
     * mode 27 — both consume unk_4C8, the last one wins. */
    assert(out.fx & AOS_SCROLL_FX_HBLANK);
    assert(out.fx & AOS_SCROLL_FX_AFFINE);
    assert(out.unk_4c8_seen == 0x55);
    /* With the flag clear mode 21 (if it ran) would disable BG2;
     * here mode 20/27 ignore flags entirely. */

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
    AosScrollWorld w = { 0x02000000, false, 0, NULL, 0 };
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

    /* ---- sub_0803FC6C (aos_scroll_transition_request) ----
     * Same skeleton as sub_0803FBBC; the y axis runs
     * sub_0803FA10/sub_0803FAE0: edge constants +0x14/+0xA00000
     * and a 0 pin instead of 0x30 << 16. */

    /* Positive y, no edge clamp: y = 64 px, vy = +2 px on a
     * 4-screen-tall map. (y+vy)>>19 = 8, +0x14 = 28 < 4<<5 = 128,
     * so v = vy. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 4, 0, 0, 0, 0x10 << 16, 0x40 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_transition_request(&s, 1, 0x40000, 0x20000, &out);
    assert(s.bg[1].x_vel == 0x40000);          /* x axis identical */
    assert(s.bg[1].y_vel == 0x20000);
    assert(s.bg[1].y_pos == ((int32_t)0x40 << 16) + 0x20000);
    assert(s.disp_v[1] == 0x42);
    assert(out.requests == 1);

    /* Upper edge: meta_1 = 2 -> map 512 px, screen 0xA0 = 160 px,
     * so y clamps so y + 160 <= 512, i.e. y <= 352 px = 0x1600000.
     * From y = 0x1580000 (344 px) a +8 px request:
     * (y+vy)>>19 = 0x1600000>>19 = 44, +0x14 = 64 >= 2<<5 = 64 ->
     * v = (2<<24) - (0x1580000 + 0xA00000) = 0x2000000 - 0x1F80000
     *   = 0x80000 (exactly the cap); y lands on 0x1600000. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0x158 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_transition_request(&s, 1, 0, 0x80000, &out);
    assert(s.bg[1].y_vel == 0x80000);
    assert(s.bg[1].y_pos == (int32_t)0x160 << 16);
    assert(s.disp_v[1] == 0x160);

    /* The same request through sub_0803FBBC would clamp against
     * the 0xD0 edge instead — divergence proof: at y = 0x1580000
     * the FBBC formula gives (2<<24) - (0x1580000 + 0xD00000)
     * = -0x280000, so yVel would be negative. Skip: just assert
     * the transition helper did NOT use the 0xD0 constant. */

    /* Negative y pins at 0, not 0x300000: from 32 px, -8 px steps
     * land on 0; a further request stores v = -y = 0. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 4, 0, 0, 0, 0, 0x20 << 16);
    memset(&out, 0, sizeof out);
    for (int i = 0; i < 4; ++i)
        aos_scroll_transition_request(&s, 1, 0, (int32_t)0xFFF80000,
                                      &out);
    assert(s.bg[1].y_pos == 0);
    assert(s.bg[1].y_vel == -0x80000);   /* last step: -8 px -> 0 */
    aos_scroll_transition_request(&s, 1, 0, (int32_t)0xFFF80000, &out);
    assert(s.bg[1].y_pos == 0 && s.bg[1].y_vel == 0);
    /* Contrast: sub_0803FBBC on the same state would pin at
     * 0x300000 — drive it there and confirm the difference. */
    aos_scroll_bind(&s, 1, 2, 4, 0, 0, 0, 0, 0x20 << 16);
    for (int i = 0; i < 6; ++i)
        aos_scroll_request(&s, 1, 0, (int32_t)0xFFF80000, NULL);
    assert(s.bg[1].y_pos == (int32_t)0x30 << 16);

    /* Gated axis: meta_1 <= 1 pins yPos to 0 (not 0x300000) and
     * clears yVel; meta_0 <= 1 still pins x to 0. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 1, 1, 0, 0, 0, 0x55 << 16, 0x40 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_transition_request(&s, 1, 0x40000, 0x40000, &out);
    assert(s.bg[1].x_pos == 0 && s.bg[1].x_vel == 0);
    assert(s.bg[1].y_pos == 0 && s.bg[1].y_vel == 0);
    assert(s.disp_h[1] == 0 && s.disp_v[1] == 0);

    /* Tile-row rebuild: the transition y helpers queue the same
     * 30-lookup / WriteData(0, 0x1F) descriptor as sub_0803F580.
     * From y = 64 px a +8 px request crosses the probe; on BG1:
     * tile_y = (64 + 0xA7) >> 3 = 28, vram 0x0600EF00. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0, 0x40 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_transition_request(&s, 1, 0, 0x80000, &out);
    assert(out.map_rebuild_y && !out.map_rebuild_x);
    assert(out.rebuild_y.queued && out.rebuild_y.bg == 1);
    assert(out.rebuild_y.tile_x == 0 && out.rebuild_y.tile_y == 28);
    assert(out.rebuild_y.count == 0x1F);
    assert(out.rebuild_y.bg_cmd == 0x00 && out.rebuild_y.length == 0x1F);
    assert(out.rebuild_y.vram_addr == 0x0600EF00u);

    /* Negative-y rebuild uses the same (yInt - 8) wrap tile row:
     * from y = 72 px a -8 px request on BG2 lands on 64 px;
     * y>>19 goes 9 -> 8, so a rebuild queues with
     * tile_y = ((72-8)<<13)>>16 = 8 and
     * vram = 0x0600E000 + (2<<11) + (1&0x1F)<<1 + (8&0x1F)<<6
     *      = 0x0600E000 + 0x1000 + 2 + 0x200 = 0x0600F202
     * (tile_x = 8>>3 = 1). */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 2, 4, 4, 0, 0, 0, 0x8 << 16, 0x48 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_transition_request(&s, 2, 0, (int32_t)0xFFF80000, &out);
    assert(out.map_rebuild_y);
    assert(out.rebuild_y.bg == 2);
    assert(out.rebuild_y.tile_x == 1 && out.rebuild_y.tile_y == 8);
    assert(out.rebuild_y.vram_addr == 0x0600F202u);
    assert(s.bg[2].y_pos == ((int32_t)0x48 << 16) - 0x80000);

    /* ---- rebuild staging-buffer words (aos_scroll_tile_source) ----
     * With a source bound the descriptor's words[] carry the
     * sub_08001994 lookups in native order: the x rebuild is a
     * column (fixed tile_x, tile_y stepping, count 0x15), the y
     * rebuild a row (fixed tile_y, tile_x stepping, 0x1F). */
    struct scroll_test_fetch_log log;
    memset(&log, 0, sizeof log);
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0x40 << 16, 0x30 << 16);
    aos_scroll_tile_source(&s, scroll_test_fetch, &log);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, 0x80000, 0, &out);
    assert(out.rebuild_x.queued);
    assert(log.calls == 1);
    assert(log.bg == 1 && log.column);
    assert(log.tile_x == 38 && log.tile_y == 6 && log.count == 0x15);
    assert(out.rebuild_x.word_count == 0x15);
    for (size_t i = 0; i < 0x15; ++i)
        assert(out.rebuild_x.words[i] == (uint16_t)(0xA100u + i));
    assert(!out.rebuild_y.queued && out.rebuild_y.word_count == 0);

    /* The y rebuild asks for the row orientation. */
    memset(&log, 0, sizeof log);
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0, 0x40 << 16);
    aos_scroll_tile_source(&s, scroll_test_fetch, &log);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, 0, 0x80000, &out);
    assert(out.rebuild_y.queued);
    assert(log.calls == 1 && !log.column);
    assert(log.tile_x == 0 && log.tile_y == 28 && log.count == 0x1F);
    assert(out.rebuild_y.word_count == 0x1F);
    assert(out.rebuild_y.words[0] == 0x2100);
    assert(out.rebuild_y.words[0x1E] == (uint16_t)(0x2100u + 0x1E));

    /* A short-writing source clamps word_count to what it
     * reported; clearing the source returns to report-only. */
    memset(&log, 0, sizeof log);
    log.ret = 3;
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0x40 << 16, 0x30 << 16);
    aos_scroll_tile_source(&s, scroll_test_fetch, &log);
    memset(&out, 0, sizeof out);
    aos_scroll_request(&s, 1, 0x80000, 0, &out);
    assert(out.rebuild_x.word_count == 3);
    aos_scroll_tile_source(&s, NULL, NULL);
    memset(&out, 0, sizeof out);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0x40 << 16, 0x30 << 16);
    aos_scroll_request(&s, 1, 0x80000, 0, &out);
    assert(out.rebuild_x.queued && out.rebuild_x.word_count == 0);
    /* The transition sibling resolves words the same way. */
    memset(&log, 0, sizeof log);
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0, 0x40 << 16);
    aos_scroll_tile_source(&s, scroll_test_fetch, &log);
    memset(&out, 0, sizeof out);
    aos_scroll_transition_request(&s, 1, 0, 0x80000, &out);
    assert(out.rebuild_y.queued && out.rebuild_y.word_count == 0x1F);
    assert(log.calls == 1 && !log.column && log.count == 0x1F);

    /* ---- sub_0800ED5C / sub_0800EE54 load origins ---- */
    /* Mode 1 is absent from both switches: the arg passes
     * through (the y twin still adds its +0x30 bias). */
    assert(aos_scroll_origin_x(1, 2, 0x100, 0x140, 0x123) == 0x123);
    assert(aos_scroll_origin_y(1, 2, 0x100, 0x110, 0x123) == 0x153);
    /* The zero lists differ per axis: mode 10 zeroes x but sets
     * y to 0xFFD0 + 0x30 = 0 (u16 wrap); mode 6 doubles y but
     * keeps x's arg. */
    assert(aos_scroll_origin_x(10, 2, 0, 0, 0x50) == 0);
    assert(aos_scroll_origin_y(10, 2, 0, 0, 0x50) == 0);
    assert(aos_scroll_origin_y(6, 2, 0, 0, 0x50) == 0xD0);
    assert(aos_scroll_origin_x(6, 2, 0, 0, 0x50) == 0x50);
    /* Halving gates on the layer's own meta byte. */
    assert(aos_scroll_origin_x(2, 2, 0, 0, 0x50) == 0x28);
    assert(aos_scroll_origin_x(2, 1, 0, 0, 0x50) == 0);
    assert(aos_scroll_origin_y(3, 1, 0, 0, 0x50) == 0x30);
    assert(aos_scroll_origin_y(4, 2, 0, 0, 0x51) == 0x58);
    /* Interpolation: x uses base 0xF0 gated on meta_0, y base
     * 0xD0; modes 15/16 skip the y meta gate. */
    /* x: arg 0x400 * (range 0x118 - 0xF0) / (ref 0x140 - 0xF0)
     *    = 0x400 * 0x28 / 0x50 = 0x200; no bias on x. */
    assert(aos_scroll_origin_x(11, 2, 0x118, 0x140, 0x400) == 0x200);
    assert(aos_scroll_origin_x(11, 1, 0x118, 0x140, 0x400) == 0);
    /* y: arg 0x300 * (0xE8 - 0xD0) / (0x110 - 0xD0)
     *    = 0x300 * 0x18 / 0x40 = 0x120; + 0x30 bias = 0x150. */
    assert(aos_scroll_origin_y(12, 2, 0xE8, 0x110, 0x300) == 0x150);
    assert(aos_scroll_origin_y(12, 1, 0xE8, 0x110, 0x300) == 0x30);
    /* Mode 15 interpolates y unconditionally (no meta gate). */
    assert(aos_scroll_origin_y(15, 1, 0xE8, 0x110, 0x300) == 0x150);
    /* A zero denominator yields 0 like ARM SDIV. */
    assert(aos_scroll_origin_x(13, 2, 0x118, 0xF0, 0x400) == 0);
    assert(aos_scroll_origin_y(13, 2, 0xE8, 0xD0, 0x300) == 0x30);
    /* Mode 36 keeps the arg on both axes. */
    assert(aos_scroll_origin_x(36, 2, 0, 0, 0x77) == 0x77);
    assert(aos_scroll_origin_y(36, 2, 0, 0, 0x77) == 0xA7);

    puts("aos_scroll: ok");
    return 0;
}
