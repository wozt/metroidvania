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
    AosScrollWorld w4 = { 0x02000000, false, 0x55, NULL, 0, -1 };
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
    AosScrollWorld w = { 0x02000000, false, 0, NULL, 0, -1 };
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

    /* ---- room-load sub_0800D288 (sub_0800ECA0's tail) ----
     * The native runs D288 once per room entry right after
     * sub_0800F9EC bound the layers, while bgInfo[1]'s velocities
     * are still 0. The scroll modes then call sub_0803FBBC with
     * (0, 0): a layer whose pBgMetadata dimension is <= 1 has the
     * axis pinned (x -> 0, y -> 0x30 << 16) regardless of the
     * ED5C/EE54 origin the bind stored; a multi-screen layer keeps
     * its origin. Non-request modes still run their direct
     * bgOfs writes once (mode 8: hOfs + 1). */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0x10 << 16, 0x40 << 16);
    /* BG2: mode 1 (1:1 request) but a 1x1 metadata — both axes
     * pin at load even though its bound origin was nonzero. */
    aos_scroll_bind(&s, 2, 1, 1, 1, 0, 0, 0x33 << 16, 0x44 << 16);
    /* BG3: mode 1 on a 2x2 metadata — keeps its origin, gains
     * zero velocity. */
    aos_scroll_bind(&s, 3, 2, 2, 1, 0, 0, 0x55 << 16, 0x66 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(s.bg[2].x_pos == 0 && s.bg[2].y_pos == (int32_t)0x30 << 16);
    assert(s.bg[2].x_vel == 0 && s.bg[2].y_vel == 0);
    assert(s.disp_h[2] == 0 && s.disp_v[2] == 0x30);
    assert(s.bg[3].x_pos == (int32_t)0x55 << 16);
    assert(s.bg[3].y_pos == (int32_t)0x66 << 16);
    assert(s.disp_h[3] == 0x55 && s.disp_v[3] == 0x66);
    /* The two mode-1 requests ran (one per layer). */
    assert(out.requests == 2);

    /* Mode 8 at load: hOfs += 1 on top of the seeded shadow. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0);
    aos_scroll_bind(&s, 2, 2, 2, 8, 0, 0, 0x20 << 16, 0x30 << 16);
    aos_scroll_bind(&s, 3, 2, 2, 0, 0, 0, 0, 0);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(s.disp_h[2] == 0x21);
    assert(out.requests == 0);

    /* ---- aos_scroll_bgofs_blits: the layered draw path's wrap ----
     * A 512x256 layer (2x1 screens) under a 240x160 view. */
    {
        AosScrollBlit blits[4];
        /* No wrap: one full-view blit at the offset. */
        assert(aos_scroll_bgofs_blits(100, 50, 512, 256, 240, 160,
                                      blits) == 1);
        assert(blits[0].src_x == 100 && blits[0].src_y == 50);
        assert(blits[0].dst_x == 0 && blits[0].dst_y == 0);
        assert(blits[0].w == 240 && blits[0].h == 160);
        /* Right-edge wrap: 400 + 240 crosses 512; the second segment
         * resumes at source 0 and fills the remaining 128 px. */
        assert(aos_scroll_bgofs_blits(400, 50, 512, 256, 240, 160,
                                      blits) == 2);
        assert(blits[0].src_x == 400 && blits[0].w == 112);
        assert(blits[1].src_x == 0 && blits[1].w == 128);
        assert(blits[1].dst_x == 112);
        /* Bottom-edge wrap on a 256-tall layer. */
        assert(aos_scroll_bgofs_blits(10, 200, 512, 256, 240, 160,
                                      blits) == 2);
        assert(blits[0].src_y == 200 && blits[0].h == 56);
        assert(blits[1].src_y == 0 && blits[1].h == 104);
        assert(blits[1].dst_y == 56);
        /* Both axes wrap: a 256x256 layer at (200,200) needs all
         * four segments. */
        assert(aos_scroll_bgofs_blits(200, 200, 256, 256, 240, 160,
                                      blits) == 4);
        assert(blits[0].src_x == 200 && blits[0].src_y == 200);
        assert(blits[0].w == 56 && blits[0].h == 56);
        assert(blits[1].src_x == 0 && blits[1].src_y == 200);
        assert(blits[1].w == 184 && blits[1].h == 56);
        assert(blits[1].dst_x == 56);
        assert(blits[2].src_x == 200 && blits[2].src_y == 0);
        assert(blits[2].w == 56 && blits[2].h == 104);
        assert(blits[2].dst_y == 56);
        assert(blits[3].src_x == 0 && blits[3].src_y == 0);
        assert(blits[3].w == 184 && blits[3].h == 104);
        assert(blits[3].dst_x == 56 && blits[3].dst_y == 56);
        /* Negative offsets wrap into range like the native
         * tilemap's unsigned scroll position: -10 -> 502 and
         * -20 -> 236, both past the wrap edge for this view so
         * all four segments come back with the wrapped origins. */
        assert(aos_scroll_bgofs_blits(-10, -20, 512, 256, 240, 160,
                                      blits) == 4);
        assert(blits[0].src_x == 502 && blits[0].src_y == 236);
        /* Degenerate map size: one unwrapped full-view blit at the
         * raw offset (same as the composite path's !wrap_ok arm). */
        assert(aos_scroll_bgofs_blits(-5, 7, 0, 0, 240, 160,
                                      blits) == 1);
        assert(blits[0].src_x == -5 && blits[0].src_y == 7);
        assert(blits[0].w == 240 && blits[0].h == 160);
    }

    /* ---- display-side feed (draw_room_backgrounds' per-frame
     * chain): request(1, frame delta) then apply propagates BG1's
     * velocity into BG2/BG3's bgOfs shadows — the offsets the
     * layered draw samples. Mode 2 halves x, mode 1 is 1:1. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0x40 << 16, 0x40 << 16);
    aos_scroll_bind(&s, 2, 2, 2, 2, 0, 0, 0x10 << 16, 0x10 << 16);
    aos_scroll_bind(&s, 3, 2, 2, 1, 0, 0, 0x20 << 16, 0x20 << 16);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);   /* room-load tail */
    assert(s.disp_h[2] == 0x10 && s.disp_h[3] == 0x20);
    /* BG1 moves +4 px this frame; BG2 (mode 2) gains +2, BG3
     * (mode 1) gains +4 in their own bgOfs shadows. */
    aos_scroll_request(&s, 1, 4 << 16, 0, NULL);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(s.disp_h[2] == 0x12);
    assert(s.disp_h[3] == 0x24);
    /* A zero-delta frame leaves every layer's offset alone. */
    aos_scroll_request(&s, 1, 0, 0, NULL);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(s.disp_h[2] == 0x12 && s.disp_h[3] == 0x24);
    /* Mode 21's else-branch (unk_33C[3] bit 0x02000000 clear, the
     * unmodelled world flags stay 0) is the unconditional
     * dispCnt &= ~DCNT_BG2 write; the fx bit is what the draw
     * path folds into the BG2 enable flag. */
    memset(&s, 0, sizeof s);
    aos_scroll_bind(&s, 1, 2, 2, 0, 0, 0, 0, 0);
    aos_scroll_bind(&s, 2, 2, 2, 21, 0, 0, 0, 0);
    aos_scroll_bind(&s, 3, 2, 2, 0, 0, 0, 0, 0);
    memset(&out, 0, sizeof out);
    aos_scroll_apply(&s, NULL, &out);
    assert(out.fx & AOS_SCROLL_FX_BG2_DISABLE);

    /* ---- depth priorities (sub_0803DC18/3DD14 -> unk_A/unk_E,
     * consumed by sub_0803FD60's BGxCNT write and sub_0803DE10's
     * OAM attribute-2 stamp) ---- */
    {
        AosDepthPriorities d;
        /* Room-load shape: BG0's slot is always layer 0; BG1-3
         * take the background records' field_0 bytes. Castle
         * Corridor room 0's {0,22,23,31} gives the hardware
         * priorities BG0=0,BG1=1,BG2=2,BG3=3 — every band at or
         * below the lowest layer threshold scores 0, bands past
         * each higher threshold step up, capped at 3. */
        const uint8_t room0[4] = {0, 22, 23, 31};
        aos_scroll_depth_priorities(room0, &d);
        assert(d.bg_priority[0] == 0 && d.bg_priority[1] == 1 &&
               d.bg_priority[2] == 2 && d.bg_priority[3] == 3);
        assert(d.band_priority[0x00] == 0);
        assert(d.band_priority[0x01] == 1);   /* 1 > layer 0 only */
        assert(d.band_priority[0x0E] == 1);   /* the player's band */
        assert(d.band_priority[0x15] == 1);   /* Object1F flanks */
        assert(d.band_priority[0x16] == 1);
        assert(d.band_priority[0x17] == 2);   /* past layer 22 */
        assert(d.band_priority[0x18] == 3);   /* past layer 23 */
        assert(d.band_priority[0x1C] == 3);   /* Object1F ring hi */
        assert(d.band_priority[0x1F] == 3);
        /* A foreground layer below the player's band pushes him
         * back: {0,6,22,31} keeps the same BG priorities but the
         * band-0xE threshold count rises to 2 — he paints behind
         * BG1 like the native walk-behind-foreground case. */
        const uint8_t shallow[4] = {0, 6, 22, 31};
        aos_scroll_depth_priorities(shallow, &d);
        assert(d.bg_priority[0] == 0 && d.bg_priority[1] == 1 &&
               d.bg_priority[2] == 2 && d.bg_priority[3] == 3);
        assert(d.band_priority[0x0E] == 2);
        assert(d.band_priority[0x05] == 1);
        assert(d.band_priority[0x06] == 1);   /* ties stay front */
        assert(d.band_priority[0x07] == 2);
        /* Equal layer bytes: every BG scores 3 (each >= the
         * other three) while every arg0 threshold lands on 0x1F,
         * so no band ever exceeds one — all bands score 0, the
         * all-0x1F title-style degenerate the native
         * sub_0803DC18(0x1F,0x1F,0x1F,0x1F) call produces. */
        const uint8_t flat[4] = {0x1F, 0x1F, 0x1F, 0x1F};
        aos_scroll_depth_priorities(flat, &d);
        for (int i = 0; i < 4; ++i) assert(d.bg_priority[i] == 3);
        for (int b = 0; b < 0x20; ++b) assert(d.band_priority[b] == 0);
        /* Tied mid layers keep the hardware BG-number tie-break
         * data: {0,5,5,31} -> BG1 and BG2 both priority 2, BG3 3;
         * band thresholds collect the last-matching BG's byte
         * per rank so bands 1-5 score 1 and 6+ score 3. */
        const uint8_t tied[4] = {0, 5, 5, 31};
        aos_scroll_depth_priorities(tied, &d);
        assert(d.bg_priority[0] == 0 && d.bg_priority[1] == 2 &&
               d.bg_priority[2] == 2 && d.bg_priority[3] == 3);
        assert(d.band_priority[0x00] == 0);
        assert(d.band_priority[0x01] == 1);
        assert(d.band_priority[0x05] == 1);
        assert(d.band_priority[0x06] == 3);
        assert(d.band_priority[0x1F] == 3);
    }

    {
        /* aos_scroll_blend_first_targets: the room record's
         * blend_control (EwramData_unk88.unk_1E -> bldCnt) decoded
         * for the draw path. Only the two alpha-effect values the
         * ROM ships produce a mask: 0x1A44 (Castle Corridor 1 and
         * 59 siblings) selects BG2 as first target, 0x1C42
         * (Clock Tower 8, Underground region 3/4/5/8/14/17-19/25/
         * 29-33/42/45/49 and the Chaotic Realm boss rooms)
         * selects BG1. Everything else — no blend, brightness
         * effects, OBJ/backdrop-only first targets — reports 0. */
        assert(aos_scroll_blend_first_targets(0x0000) == 0x0);
        assert(aos_scroll_blend_first_targets(0x1A44) == 0x4);
        assert(aos_scroll_blend_first_targets(0x1C42) == 0x2);
        /* Brightness increase (effect 2) and decrease (effect 3)
         * are not alpha blends even with BG first targets set. */
        assert(aos_scroll_blend_first_targets(0x0004 | (2u << 6)) == 0);
        assert(aos_scroll_blend_first_targets(0x0004 | (3u << 6)) == 0);
        /* First-target bits with no effect stay 0 (0x0044 would
         * be BG2 + the alpha bit — covered above). */
        assert(aos_scroll_blend_first_targets(0x0004) == 0);
        /* BG0's first-target bit is kept in the mask (bit 0) —
         * the draw only consumes bits 1-3. */
        assert(aos_scroll_blend_first_targets(0x0041) == 0x1);
        /* OBJ (bit 4) and backdrop (bit 5) first targets are not
         * BG slots: masked out. */
        assert(aos_scroll_blend_first_targets(0x017F) == 0xF);
    }

    {
        /* aos_scroll_enemy_band5c: the entity record +0x5C
         * create-time bands decoded from the native Enemy*Create
         * bodies — sub_0806B04C's unconditional 0x12 default plus
         * the per-create overrides that land after it (the
         * pre-helper stores are dead writes). Spot-checks cover
         * the default, each override band and the family thunks;
         * ids outside the native 0x71 records report 0xFF. */
        assert(aos_scroll_enemy_band5c(0x00) == 0x12);  /* Bat */
        assert(aos_scroll_enemy_band5c(0x02) == 0x12);  /* Skeleton:
            * its create's 0x11 precedes sub_0806B04C — dead write */
        assert(aos_scroll_enemy_band5c(0x06) == 0x03);  /* Peeping Eye */
        assert(aos_scroll_enemy_band5c(0x08) == 0x0A);  /* Bone Pillar:
            * only its spawned segments carry a band natively */
        assert(aos_scroll_enemy_band5c(0x0B) == 0x03);  /* White Dragon */
        assert(aos_scroll_enemy_band5c(0x10) == 0x04);  /* Tiny Devil */
        assert(aos_scroll_enemy_band5c(0x15) == 0x13);  /* Minotaur via
            * sub_080B0AD4 */
        assert(aos_scroll_enemy_band5c(0x1C) == 0x09);  /* Catoblepas via
            * the GorgonCreate thunk */
        assert(aos_scroll_enemy_band5c(0x27) == 0x1D);  /* Kyoma Demon
            * create band (its live +0x5C is dynamic) */
        assert(aos_scroll_enemy_band5c(0x2C) == 0x12);  /* Altair create
            * band (its create's 3 precedes the helper) */
        assert(aos_scroll_enemy_band5c(0x2E) == 0x11);  /* Cockatrice via
            * sub_080BBFF0 */
        assert(aos_scroll_enemy_band5c(0x36) == 0x11);  /* Manticore */
        assert(aos_scroll_enemy_band5c(0x3E) == 0x11);  /* Giant Worm via
            * sub_080AD7F0 */
        assert(aos_scroll_enemy_band5c(0x44) == 0x11);  /* Slime */
        assert(aos_scroll_enemy_band5c(0x49) == 0x04);  /* Cagnazzo via
            * sub_080B7E80 */
        assert(aos_scroll_enemy_band5c(0x56) == 0x12);  /* Mudman create
            * band (its +0x5C is the live child counter) */
        assert(aos_scroll_enemy_band5c(0x62) == 0x03);  /* Erinys via the
            * ValkyrieCreate thunk */
        assert(aos_scroll_enemy_band5c(0x6D) == 0x1E);  /* Balore */
        assert(aos_scroll_enemy_band5c(0x6F) == 0x14);  /* Graham */
        assert(aos_scroll_enemy_band5c(0x70) == 0x12);  /* Chaos */
        assert(aos_scroll_enemy_band5c(0x71) == 0xFF);
        assert(aos_scroll_enemy_band5c(0xFF) == 0xFF);
        /* Every in-range kind band sits inside the native 0x00-0x1F
         * band space, so aria_bg_band_priority can resolve it. */
        for (int id = 0; id < 0x71; ++id)
            assert(aos_scroll_enemy_band5c((uint8_t)id) < 0x20);
    }

    /* ---- aos_scroll_hblank: the sub_0800D288 hBlankEffect
     * programs (single buffer, last emitter in the bg 2,3 order
     * wins). Spot-checks pin the absolute register values the
     * routines write per scanline. */
    {
        AosScrollHBlank hb;
        AosScrollWorld world = { 0, false, 0, NULL, 0, -1 };

        /* No emitter: plain modes produce no program. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 1, 0, 0, 0, 0x30 << 16);
        aos_scroll_bind(&s, 3, 1, 1, 2, 0, 0, 0, 0x30 << 16);
        assert(!aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 0);

        /* Mode 36 (BG3): own vOfs wave
         * sine((y<<12) + (unk_4C8<<10)) >> 14 + bgOfs.vOfs.
         * unk_4C8 = 0 -> arg = y<<12: y=4 hits 0x4000 (sine peak
         * 0x10000 >> 14 = +4), y=8 hits 0x8000 (sine 0), y=12 hits
         * 0xC000 (-0x10000 >> 14 = -4). */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 3, 1, 1, 36, 0, 0, 0, 0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && hb.vertical && !hb.affine);
        assert(hb.ofs[0] == 0x30 && hb.ofs[4] == 0x34 &&
               hb.ofs[8] == 0x30 && hb.ofs[12] == 0x2C);
        /* unk_4C8 phase shifts the wave: <<10 of 0x400 gives
         * arg offset 0x100000 == 0x10000-turn wrap -> same table
         * as 0x400<<10 & 0xFFFF... verify a nonzero phase moves
         * values instead of pinning them. */
        world.unk_4c8 = 0x10;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.ofs[0] != 0x30);

        /* Mode 30 (BG2): same vOfs family with the (y<<11) step
         * and >>13 amplitude — y=8 hits 0x4000 (0x10000>>13 = +8),
         * y=16 hits 0x8000 (0). */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 30, 0, 0, 0, 0x20 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 2 && hb.vertical);
        assert(hb.ofs[0] == 0x20 && hb.ofs[8] == 0x28 &&
               hb.ofs[16] == 0x20);

        /* Mode 31 (BG2): uniform hOfs jitter
         * sine(frameCtr << 8) >> 15 + bgOfs.hOfs on every row.
         * frame 0x40 -> arg 0x4000 -> +2 px everywhere. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 31, 0, 0, 0x10 << 16,
                        0x30 << 16);
        assert(aos_scroll_hblank(&s, &world, 0x40, &hb));
        assert(hb.target_bg == 2 && !hb.vertical);
        for (int y = 0; y < AOS_SCROLL_SCANLINES; ++y)
            assert(hb.ofs[y] == 0x12);

        /* Mode 14: sub_0800CB00's absolute ramp on REG_BG3HOFS —
         * unk_4C8 = 0x10: y=0 -> 16*64/16 = 64, y=64 -> 32,
         * y=127 -> 1, y>=128 -> 0. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 3, 1, 1, 14, 0, 0, 0, 0x30 << 16);
        world.unk_4c8 = 0x10;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && !hb.vertical);
        assert(hb.ofs[0] == 64 && hb.ofs[64] == 32 &&
               hb.ofs[127] == 1 && hb.ofs[128] == 0 &&
               hb.ofs[159] == 0);

        /* Mode 20: BG3 hOfs step pattern — 0x80 zeros then
         * unk_4C8 / 1.5x / 2x bands. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 3, 1, 1, 20, 0, 0, 0, 0x30 << 16);
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && !hb.vertical);
        assert(hb.ofs[0] == 0 && hb.ofs[127] == 0 &&
               hb.ofs[128] == 0x10 && hb.ofs[135] == 0x10 &&
               hb.ofs[136] == 0x18 && hb.ofs[143] == 0x18 &&
               hb.ofs[144] == 0x20 && hb.ofs[159] == 0x20);

        /* Mode 16 (BG3): sub_0800CC90 band wave on own hOfs.
         * p1 = 0 - yPos.int clamps to 0, var_r2 = 160, so every
         * row is in-band: ofs[y] = base + (2*sine((y+yint)<<12
         * + frame<<9)) >> 16. yPos.int = 0x30 pushes p1 negative
         * -> still 0; frame 0 -> arg = y<<12: y=4 sine peak ->
         * (2*0x10000)>>16 = +2. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 3, 1, 1, 16, 0, 0, 0x10 << 16,
                        0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && !hb.vertical);
        assert(hb.ofs[0] == 0x10 && hb.ofs[4] == 0x12 &&
               hb.ofs[8] == 0x10);

        /* Mode 28 (BG2): sub_0800CED4's ramped band — rows
         * outside [p1', p2'] keep base; inside they deviate.
         * Just assert the program is active, hOfs-targeted and
         * not constant. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 28, 0, 0, 0x08 << 16,
                        0x30 << 16);
        world.unk_4c8 = 0x20;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 2 && !hb.vertical);
        {
            int same = 1;
            for (int y = 1; y < AOS_SCROLL_SCANLINES; ++y)
                if (hb.ofs[y] != hb.ofs[0]) same = 0;
            assert(!same);
        }

        /* Last emitter wins: BG2 mode 30 (vOfs wave) vs BG3 mode
         * 14 — the bg3 pass runs second, so the CB00 hOfs program
         * on BG3 is the active one and BG2's wave is lost, like
         * the single native buffer. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 30, 0, 0, 0, 0x20 << 16);
        aos_scroll_bind(&s, 3, 1, 1, 14, 0, 0, 0, 0x30 << 16);
        world.unk_4c8 = 0x10;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && !hb.vertical &&
               hb.ofs[0] == 64);

        /* Mode 18 alone reports the affine program: target BG2,
         * affine flag, no ofs table — the flat waves stay off. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 18, 0, 0, 0, 0x30 << 16);
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 2 && hb.affine);
        /* ...but a later BG3 wave emitter overwrites it. */
        aos_scroll_bind(&s, 3, 1, 1, 36, 0, 0, 0, 0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && hb.vertical && !hb.affine);

        /* Mode 15 (BG3): sub_0800CB8C(0,160,1) — the ramp term
         * temp_r0 = (1*y)/160 is 0 on every row, so the wave is
         * sine(((yPos.int + y) << 13) + (frameCtr << 10)) >> 15
         * + bgOfs.hOfs. yPos.int = 0x30 makes (48+y)*8192 hit
         * 0x4000 at y=2 (peak +2), 0x8000 at y=6 (zero crossing
         * of the folded quadrant -> 0 offset from sine 0... the
         * table returns -0x10000>>15 = -2 there; y=10 repeats
         * the peak). */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 3, 1, 1, 15, 0, 0, 0x10 << 16,
                        0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && !hb.vertical && !hb.affine);
        assert(hb.ofs[0] == 0x10 && hb.ofs[2] == 0x12 &&
               hb.ofs[10] == 0x12);
        /* A nonzero frame phase shifts the band rows (frame 1
         * adds 0x400 to the sine argument: y=2's peak folds to
         * the +1 neighbour). */
        assert(aos_scroll_hblank(&s, &world, 1, &hb));
        assert(hb.ofs[2] != 0x12);

        /* Mode 23 (BG2): the second CB8C call site
         * (112,48,3) — rows below the 0x70 band keep the flat
         * bgOfs.hOfs, rows inside it carry the steeper-phase wave
         * (temp_r0 ramps 0..2 across the band). */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 23, 0, 0, 0x20 << 16,
                        0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 2 && !hb.vertical);
        for (int y = 0; y < 112; ++y) assert(hb.ofs[y] == 0x20);
        {
            int flat = 1;
            for (int y = 112; y < AOS_SCROLL_SCANLINES; ++y)
                if (hb.ofs[y] != 0x20) flat = 0;
            assert(!flat);
        }

        /* Mode 24 (BG3): sub_0800CC90(0xB0,0x200,1) — the band
         * slides up by yPos.int: p1 = 0xB0 - 0x30 = 0x80, width
         * clamps to 160 - 0x80 = 0x20, so rows [0x80,0xA0) wave
         * and everything else stays flat. arg = (48+y)<<12:
         * y=0x84 hits 0x4000 (+1 px at p3=1), y=0x88 hits 0x8000
         * (sine 0), y=0x8C hits 0xC000 (-1). */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 3, 1, 1, 24, 0, 0, 0x10 << 16,
                        0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && !hb.vertical);
        assert(hb.ofs[0x7F] == 0x10 && hb.ofs[0x80] == 0x10 &&
               hb.ofs[0x84] == 0x11 && hb.ofs[0x88] == 0x10 &&
               hb.ofs[0x8C] == 0x0F && hb.ofs[0x9F] == 0x0F);

        /* Mode 25 (BG2): sub_0800CC90(0x70,0x100,1) — p1 =
         * 0x70 - 0x30 = 0x40, width 0x90 clamps to 0x78, band
         * [0x40,0xB8). arg = (48+y)<<12: y=0x44 -> 0x4000 (+1),
         * y=0x48 -> 0x8000 (0), y=0x4C -> 0xC000 (-1). */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 25, 0, 0, 0x18 << 16,
                        0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 2 && !hb.vertical);
        assert(hb.ofs[0x3F] == 0x18 && hb.ofs[0x40] == 0x18 &&
               hb.ofs[0x44] == 0x19 && hb.ofs[0x48] == 0x18 &&
               hb.ofs[0x4C] == 0x17);

        /* Mode 29 (BG2): the second sub_0800CED4 band (0x70,0xCC)
         * — p1 = 0x70 - 0x30 = 0x40, var_sb = 0x5C, inclusive end
         * p2c = 0x9C. Rows outside the band (and the inclusive
         * edge rows) keep bgOfs.hOfs; inside rows deviate. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 29, 0, 0, 0x08 << 16,
                        0x30 << 16);
        world.unk_4c8 = 0x20;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 2 && !hb.vertical);
        assert(hb.ofs[0x3F] == 0x08 && hb.ofs[0x40] == 0x08 &&
               hb.ofs[0x9D] == 0x08);
        {
            int same = 1;
            for (int y = 0x41; y < 0x9C; ++y)
                if (hb.ofs[y] != hb.ofs[0x40]) same = 0;
            assert(!same);
        }

        /* BG1 mode 0x1A: sub_0800CDAC(1, 0x450, 0x4A0) — the
         * pre-loop emitter. yPos.int = 0x400 slides param_1 to
         * 0x50 (band [0x50,0xA0), var_sb 0x50); frame 0 gives
         * sp8 = sine(0) = 0 and arg = (0x1000*y - 0x50000) >> 2.
         * Rows below the band keep bgOfs.hOfs = 0x10; inside it
         * the var_1 ramp shears hOfs smoothly upward (the room
         * 7/33 iris). target_bg = 1, hOfs axis. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 1, 2, 2, 0x1A, 0, 0, 0x10 << 16,
                        0x400 << 16);
        aos_scroll_bind(&s, 2, 1, 1, 0, 0, 0, 0, 0x30 << 16);
        aos_scroll_bind(&s, 3, 1, 1, 0, 0, 0, 0, 0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 1 && !hb.vertical && !hb.affine);
        assert(hb.ofs[0] == 0x10 && hb.ofs[79] == 0x10 &&
               hb.ofs[80] == 0x10 && hb.ofs[88] == 0x10);
        assert(hb.ofs[96] == 0x11 && hb.ofs[104] == 0x12 &&
               hb.ofs[112] == 0x13 && hb.ofs[120] == 0x14 &&
               hb.ofs[128] == 0x15 && hb.ofs[159] == 0x16);
        /* The frame phase feeds both sine terms: frame 1 shifts
         * the band rows. */
        assert(aos_scroll_hblank(&s, &world, 1, &hb));
        assert(hb.target_bg == 1);
        assert(hb.ofs[136] != 0x16);   /* frame 0 gave 0x16 */
        /* A BG2/BG3 hblank emitter runs after CDAC natively and
         * overwrites the single buffer: mode 14 on BG3 wins. */
        aos_scroll_bind(&s, 3, 1, 1, 14, 0, 0, 0, 0x30 << 16);
        world.unk_4c8 = 0x10;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 3 && !hb.vertical && hb.ofs[0] == 64);
        /* ...but modes that never touch hBlankEffect (27's
         * direct BgAffineSet) leave the CDAC program standing. */
        aos_scroll_bind(&s, 3, 1, 1, 27, 0, 0, 0, 0x30 << 16);
        world.unk_4c8 = 0;
        assert(aos_scroll_hblank(&s, &world, 0, &hb));
        assert(hb.target_bg == 1 && hb.ofs[128] == 0x15);
    }

    /* ---- aos_scroll_affine: sub_0800D288's BgAffineSet programs
     * (unk_18 modes 18/21/27/32). Mode 18 fills the 160-record
     * per-scanline buffer; 21/27/32 produce one frame transform.
     * Both are reported together: the mode-18 records only mask
     * the per-frame registers when they win the hBlankEffect
     * buffer (aos_scroll_hblank's last-emitter rule). */
    {
        AosScrollAffine af;
        AosScrollWorld aw = { 0, false, 0, NULL, 0, -1 };

        /* No affine emitter: nothing reported. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 1, 0, 0, 0, 0x30 << 16);
        aos_scroll_bind(&s, 3, 1, 1, 2, 0, 0, 0, 0x30 << 16);
        memset(&af, 0xAA, sizeof af);
        assert(!aos_scroll_affine(&s, &aw, &af));
        assert(af.target_bg == 0 && !af.per_scanline &&
               !af.has_frame);

        /* Mode 18 on BG2, unk_4C8 = 0: scan[0] comes from
         * texX 0x8000, texY -102 (var_r6 = -sx), scr (0x78,0),
         * sx = 0x80000 / ((0x80<<5)+0x400) = 102, sy unit, alpha 0
         * -> pa = 102 (8.8), pb = pc = 0, pd = 0x100,
         * ph = (128 - (102/256)*120) * 256 = 20528,
         * pg = -102. No per-frame emitter -> has_frame clear. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 18, 0, 0, 0, 0x30 << 16);
        aos_scroll_bind(&s, 3, 1, 1, 0, 0, 0, 0, 0x30 << 16);
        memset(&af, 0, sizeof af);
        assert(aos_scroll_affine(&s, &aw, &af));
        assert(af.target_bg == 2 && af.per_scanline &&
               !af.has_frame);
        assert(af.scan[0].pa == 102 && af.scan[0].pb == 0 &&
               af.scan[0].pc == 0 && af.scan[0].pd == 0x100);
        assert(af.scan[0].ph == 20528 && af.scan[0].pg == -102);
        /* Later rows keep the pulse shape: record 0x7F is the
         * top-half ramp end, record 0x80 the bottom-half start —
         * denominators differ so pa differs. */
        assert(af.scan[0x7F].pa != af.scan[0x80].pa ||
               af.scan[0x7F].pg != af.scan[0x80].pg);

        /* Mode 21 with the event flag set (unk_33C[3] bit
         * 0x02000000): texel transform scr (0x70,0x38), half scale
         * sx=sy=0x80, alpha 0 -> pa = pd = 0x80, pb = pc = 0,
         * ph = (128 - 0.5*112) * 256 = 0x4800,
         * pg = (128 - 0.5*56) * 256 = 0x6400. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 21, 0, 0, 0, 0x30 << 16);
        aos_scroll_bind(&s, 3, 1, 1, 0, 0, 0, 0, 0x30 << 16);
        aw.unk_33c3 = 0x02000000;
        memset(&af, 0, sizeof af);
        assert(aos_scroll_affine(&s, &aw, &af));
        assert(af.target_bg == 2 && !af.per_scanline && af.has_frame);
        assert(af.frame.pa == 0x80 && af.frame.pd == 0x80 &&
               af.frame.pb == 0 && af.frame.pc == 0);
        assert(af.frame.ph == 0x4800 && af.frame.pg == 0x6400);
        /* Flag clear: mode 21's else arm disables BG2 — no affine
         * program at all. */
        aw.unk_33c3 = 0;
        memset(&af, 0, sizeof af);
        assert(!aos_scroll_affine(&s, &aw, &af));
        aw.unk_33c3 = 0x02000000;

        /* Mode 27 on BG2 + mode 18 on BG3: the per-scanline
         * program AND the per-frame write are both reported —
         * the draw picks per hblank.affine. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 27, 0, 0, 0, 0x30 << 16);
        aos_scroll_bind(&s, 3, 1, 1, 18, 0, 0, 0, 0x30 << 16);
        memset(&af, 0, sizeof af);
        assert(aos_scroll_affine(&s, &aw, &af));
        assert(af.target_bg == 2 && af.per_scanline && af.has_frame);

        /* Mode 32's room-keyed arm: room_index 33 selects the
         * dir = -1 record (scr 0x20/0x68). unk_4C8 = 0 -> sine 0,
         * sx = 0x100 unit x, alpha 0 -> pa = pd = 0x100. */
        memset(&s, 0, sizeof s);
        aos_scroll_bind(&s, 2, 1, 1, 32, 0, 0, 0, 0x30 << 16);
        aos_scroll_bind(&s, 3, 1, 1, 0, 0, 0, 0, 0x30 << 16);
        aw.room_index = 33;
        aw.unk_33c3 = 0;
        memset(&af, 0, sizeof af);
        assert(aos_scroll_affine(&s, &aw, &af));
        assert(af.target_bg == 2 && !af.per_scanline && af.has_frame);
        assert(af.frame.pa == 0x100 && af.frame.pd == 0x100);
        /* An unlisted room index takes the default arm (scr
         * 0x78/0x50, dir +1) — same unit transform at phase 0. */
        aw.room_index = -1;
        memset(&af, 0, sizeof af);
        assert(aos_scroll_affine(&s, &aw, &af));
        assert(af.frame.pa == 0x100);

        /* ---- aos_scroll_affine_texel: the BGxPA..PG fetch ----
         * ph = 8<<8, pg = 48<<8, unit matrix -> texel (x+8, y+48);
         * a half-scale pa adds x/2; pb/pd gradients add their
         * y/x terms like the hardware. */
        {
            AosScrollAffineSample id = { 0x100, 0, 0, 0x100,
                                         8 << 8, 48 << 8 };
            int32_t tx, ty;
            aos_scroll_affine_texel(&id, 10, 20, &tx, &ty);
            assert(tx == 18 && ty == 68);
            AosScrollAffineSample half = { 0x80, 0x40, 0x20, 0x100,
                                           0, 0 };
            aos_scroll_affine_texel(&half, 16, 8, &tx, &ty);
            /* tx = (0 + 0x80*16 + 0x40*8) >> 8 = (2048+512)>>8 = 10;
             * ty = (0 + 0x20*16 + 0x100*8) >> 8 = (512+2048)>>8 = 10. */
            assert(tx == 10 && ty == 10);
            /* Negative coordinates shift arithmetically like the
             * native s32 >> 8 (-1 >> 8 = -1, not 0). */
            aos_scroll_affine_texel(&id, -9, -49, &tx, &ty);
            assert(tx == -1 && ty == -1);
        }
    }

    /* ---- aos_scroll_camera_follow: the per-frame camera request
     * the player update tails issue through sub_0800ED24(0, vx, vy)
     * — vx/vy are the scroll-relative fine position integer halves
     * minus the +0x13214/+0x13216 anchors (0x78/0x6F), so BG1
     * glides at the native 8 px/frame cap toward the settle
     * position instead of snapping. */
    {
        memset(&s, 0, sizeof s);
        /* A 4x4-screen room; BG1 seeded at x = 0, y = 0x30 (the
         * native y pin). Player stands 20 px right of and 20 px
         * below the anchor on screen. */
        aos_scroll_bind(&s, 1, 4, 4, 0, 0, 0, 0, 0x30 << 16);
        memset(&out, 0, sizeof out);
        /* Frame 1: screen pos (140, 131) -> request (+20, +20) px,
         * capped to +8 px each axis. */
        int32_t px = (int32_t)140 << 16, py = (int32_t)179 << 16;
        assert(aos_scroll_camera_follow(&s, 0, px, py, NULL, &out) == 1);
        assert(s.bg[1].x_pos == (int32_t)8 << 16);
        assert(s.bg[1].y_pos == ((int32_t)0x30 << 16) + (8 << 16));
        /* Frames 2-3 keep gliding at the cap while the residual
         * request stays above 8 px; frame 4 lands the 4 px
         * remainder and frame 5 requests zero. */
        assert(aos_scroll_camera_follow(&s, 0, px, py, NULL, &out) == 1);
        assert(s.bg[1].x_pos == (int32_t)16 << 16);
        assert(s.bg[1].y_pos == ((int32_t)0x30 << 16) + (16 << 16));
        assert(aos_scroll_camera_follow(&s, 0, px, py, NULL, &out) == 1);
        assert(s.bg[1].x_pos == (int32_t)20 << 16);
        assert(s.bg[1].y_pos == ((int32_t)0x30 << 16) + (20 << 16));
        assert(aos_scroll_camera_follow(&s, 0, px, py, NULL, &out) == 1);
        assert(s.bg[1].x_pos == (int32_t)20 << 16);
        assert(s.bg[1].x_vel == 0 && s.bg[1].y_vel == 0);
        assert(s.disp_h[1] == 20 && s.disp_v[1] == 0x30 + 20);
        /* A +5 px player step requests +5 px — under the cap the
         * glide is proportional, not stepped. */
        px = (int32_t)145 << 16;
        assert(aos_scroll_camera_follow(&s, 0, px, py, NULL, &out) == 1);
        assert(s.bg[1].x_pos == (int32_t)25 << 16);
        assert(aos_scroll_camera_follow(&s, 0, px, py, NULL, &out) == 1);
        assert(s.bg[1].x_pos == (int32_t)25 << 16);
        /* Moving the player 30 px left of the anchor requests
         * -30 px, capped at -8 px per frame. */
        px = (int32_t)(25 + 120 - 30) << 16;   /* screen x = 90 */
        assert(aos_scroll_camera_follow(&s, 0, px, py, NULL, &out) == 1);
        assert(s.bg[1].x_pos == (int32_t)17 << 16);
        assert(s.bg[1].x_vel == -0x80000);
        /* The ED24 gate suppresses the whole transaction while
         * +0x427 exceeds param0 0 — position and velocity hold. */
        int32_t held_x = s.bg[1].x_pos, held_y = s.bg[1].y_pos;
        assert(aos_scroll_camera_follow(&s, 1, px, py, NULL, &out) == 0);
        assert(s.bg[1].x_pos == held_x && s.bg[1].y_pos == held_y);
    }

    puts("aos_scroll: ok");
    return 0;
}
