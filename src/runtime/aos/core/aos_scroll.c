/* SPDX-License-Identifier: GPL-3.0-only */
/* Ports of the BG scroll routines; comments name the cvaos routine
 * and the native offsets they reproduce. See aos_scroll.h. */
#include "aos_scroll.h"
#include <string.h>

/* The native velocity cap: 0x80 << 0xC = 0x80000 (8.0 px per call). */
#define AOS_SCROLL_VEL_CAP 0x80000
/* Tile-row rebuild granularity: positions are 16.16 px, an 8-px
 * tile is 0x80000, and the boundary probes shift by 0x13. */

void aos_scroll_bind(AosScroll *s, int bg, uint8_t meta_0, uint8_t meta_1,
                     uint8_t mode, uint16_t unk_14, uint16_t unk_16,
                     int32_t x_pos, int32_t y_pos) {
    AosBgLayer *b = &s->bg[bg];
    memset(b, 0, sizeof *b);
    b->meta_0 = meta_0;
    b->meta_1 = meta_1;
    b->unk_18 = mode;
    b->unk_14 = unk_14;
    b->unk_16 = unk_16;
    b->x_pos = x_pos;
    b->y_pos = y_pos;
    /* sub_0803F8A8 also lands the integer halves in the bgOfs
     * shadow through the request write-back; seed it the same way. */
    s->disp_h[bg] = (uint16_t)((uint32_t)x_pos >> 16);
    s->disp_v[bg] = (uint16_t)((uint32_t)y_pos >> 16);
}

/* The interpolation half shared by sub_0800ED5C/sub_0800EE54:
 * (arg * (range - base)) / (ref_range - base) with ARM SDIV's
 * zero-denominator result of 0, truncated to u16 like the native
 * var_2 assignment. */
static uint16_t scroll_origin_interp(uint16_t arg, uint16_t range,
                                     uint16_t ref_range,
                                     uint16_t base) {
    int32_t den = (int32_t)ref_range - base;
    if (den == 0) return 0;
    return (uint16_t)(((int32_t)arg * ((int32_t)range - base)) / den);
}

uint16_t aos_scroll_origin_x(uint8_t mode, uint8_t meta_0,
                             uint16_t range, uint16_t ref_range,
                             uint16_t arg) {
    /* sub_0800ED5C: the x half. */
    uint16_t v = arg;
    switch (mode) {
    case 0: case 8: case 9: case 10: case 14: case 17: case 18:
    case 20: case 21: case 22: case 27: case 28: case 29: case 30:
    case 32: case 33: case 35:
        v = 0;
        break;
    case 5: case 7:
        v = (uint16_t)(v * 2);
        break;
    case 2: case 4:
        v = meta_0 > 1 ? (uint16_t)(v / 2) : 0;
        break;
    case 11: case 13: case 15: case 16: case 23: case 31:
        v = meta_0 > 1
            ? scroll_origin_interp(v, range, ref_range, 0xF0) : 0;
        break;
    case 36:
        break;
    default:
        break;
    }
    return v;
}

uint16_t aos_scroll_origin_y(uint8_t mode, uint8_t meta_1,
                             uint16_t range, uint16_t ref_range,
                             uint16_t arg) {
    /* sub_0800EE54: the y half — same skeleton with the 0xD0
     * base, the +0x30 bias and mode 10's 0xFFD0 (which wraps the
     * final +0x30 back to 0). Modes 15/16 interpolate without
     * the meta_1 gate the 12/13 pair carries. */
    uint16_t v = arg;
    switch (mode) {
    case 0: case 8: case 9: case 14: case 17: case 18:
    case 20: case 21: case 22: case 27: case 28: case 29: case 30:
    case 32: case 33: case 35:
        v = 0;
        break;
    case 10:
        v = 0xFFD0;
        break;
    case 6: case 7:
        v = (uint16_t)(v * 2);
        break;
    case 3: case 4:
        v = meta_1 > 1 ? (uint16_t)(v / 2) : 0;
        break;
    case 12: case 13:
        v = meta_1 > 1
            ? scroll_origin_interp(v, range, ref_range, 0xD0) : 0;
        break;
    case 15: case 16:
        v = scroll_origin_interp(v, range, ref_range, 0xD0);
        break;
    case 36:
        break;
    default:
        break;
    }
    return (uint16_t)(v + 0x30);
}

static void scroll_out_reset(AosScrollOut *out) {
    if (out) memset(out, 0, sizeof *out);
}

/* Tile origin the native rebuild block derives from the layer's
 * integer scroll position (bgInfo +6/+0xA, the high halves of
 * xPos/yPos — read before the position integrates, so the old
 * scroll offset names the tile row). x_pos/x_neg differ: the
 * positive helper adds 0xF7 before the >>3, the negative one
 * subtracts 8 tiles' worth through the (v<<13)>>16 wrap. */
static uint16_t scroll_tile_x_pos(int32_t x_pos) {
    return (uint16_t)((((uint32_t)(uint16_t)((uint32_t)x_pos >> 16)) +
                       0xF7) >> 3);
}
static uint16_t scroll_tile_x_neg(int32_t x_pos) {
    int32_t t = (int32_t)(uint16_t)((uint32_t)x_pos >> 16) - 8;
    return (uint16_t)(((uint32_t)t << 13) >> 16);
}
static uint16_t scroll_tile_y_pos(int32_t y_pos) {
    return (uint16_t)((((uint32_t)(uint16_t)((uint32_t)y_pos >> 16)) +
                       0xA7) >> 3);
}
static uint16_t scroll_tile_y_neg(int32_t y_pos) {
    int32_t t = (int32_t)(uint16_t)((uint32_t)y_pos >> 16) - 8;
    return (uint16_t)(((uint32_t)t << 13) >> 16);
}

/* The VRAM destination the native computes for the rebuild DMA:
 * 0x0600E000 + (bg << 11) + (tile_x & 0x1F) * 2
 * + (tile_y & 0x1F) * 64 — the BG tilemap word address. */
static uint32_t scroll_rebuild_vram(int bg, uint16_t tile_x,
                                    uint16_t tile_y) {
    return 0x0600E000u + ((uint32_t)bg << 11) +
           (((uint32_t)tile_x & 0x1F) << 1) +
           (((uint32_t)tile_y & 0x1F) << 6);
}

/* Fills the descriptor's staging-buffer words through the bound
 * tile source — the sub_08001994 lookups the native runs before
 * BgCmdBuffer_WriteData. `column` selects the span orientation:
 * true for the x-axis helpers (fixed tile_x, tile_y stepping
 * upward), false for the y-axis ones (fixed tile_y, tile_x
 * stepping upward). With no source bound word_count stays 0 and
 * the descriptor remains report-only. */
static void scroll_rebuild_words(const AosScroll *s,
                                 AosScrollRebuild *r, bool column) {
    r->word_count = 0;
    if (!s->tile_source.fetch) return;
    size_t n = s->tile_source.fetch(s->tile_source.context, r->bg,
                                    column, r->tile_x, r->tile_y,
                                    r->count, r->words);
    r->word_count = (uint16_t)(n > 0x1F ? 0x1F : n);
}

void aos_scroll_tile_source(AosScroll *s, AosScrollTileFetch fetch,
                            void *context) {
    s->tile_source.fetch = fetch;
    s->tile_source.context = context;
}

/* sub_0803F3F8: positive x velocity. Clamps the request against the
 * map edge ((meta_0 << 5) tile blocks of 8 px plus the 30-tile
 * screen), caps at +8 px, stores xVel, integrates xPos and queues
 * the tile-row rebuild when the (pos - 0x10000) >> 19 tile index
 * changes — the asm probes (x - 0x10000) >> 19 against
 * (x + v - 0x10000) >> 19 on both sides. The rebuild streams 21
 * tile lookups (BgCmdBuffer_WriteData(0x80, 0x15, ...)) down the
 * screen's tile rows; the descriptor is reported through `out`. */
static void scroll_vel_x_pos(AosScroll *s, int bgnum, int32_t vx,
                             AosScrollOut *out) {
    AosBgLayer *bg = &s->bg[bgnum];
    int32_t x = bg->x_pos;
    int32_t v = vx;
    if (!((((uint32_t)(x + vx)) >> 19) + 0x1E <
          ((uint32_t)bg->meta_0 << 5)))
        v = ((int32_t)bg->meta_0 << 24) - (x + 0xF00000);
    if (v > AOS_SCROLL_VEL_CAP) v = AOS_SCROLL_VEL_CAP;
    bg->x_vel = v;
    if ((((uint32_t)(x - 0x10000)) >> 19) !=
        (((uint32_t)(x + v - 0x10000)) >> 19)) {
        if (out) {
            out->map_rebuild_x = true;
            AosScrollRebuild *r = &out->rebuild_x;
            r->queued = true;
            r->bg = bgnum;
            r->tile_x = scroll_tile_x_pos(x);
            r->tile_y = (uint16_t)(((uint32_t)(uint16_t)
                                    ((uint32_t)bg->y_pos >> 16)) >> 3);
            r->count = 0x15;
            r->bg_cmd = 0x80;
            r->length = 0x15;
            r->vram_addr = scroll_rebuild_vram(bgnum, r->tile_x,
                                               r->tile_y);
            scroll_rebuild_words(s, r, true);
        }
    }
    bg->x_pos = x + v;
}

/* sub_0803F4C8: negative x velocity. Stops at x = 0, caps at
 * -8 px, stores xVel, integrates xPos, rebuilds on a tile-index
 * change (the unshifted (x >> 19) vs ((x + v) >> 19) probe). */
static void scroll_vel_x_neg(AosScroll *s, int bgnum, int32_t vx,
                             AosScrollOut *out) {
    AosBgLayer *bg = &s->bg[bgnum];
    int32_t x = bg->x_pos;
    int32_t v = vx;
    if (x + vx < 0) v = -x;
    if (v < -AOS_SCROLL_VEL_CAP) v = -AOS_SCROLL_VEL_CAP;
    bg->x_vel = v;
    if ((((uint32_t)x) >> 19) != (((uint32_t)(x + v)) >> 19)) {
        if (out) {
            out->map_rebuild_x = true;
            AosScrollRebuild *r = &out->rebuild_x;
            r->queued = true;
            r->bg = bgnum;
            r->tile_x = scroll_tile_x_neg(x);
            r->tile_y = (uint16_t)(((uint32_t)(uint16_t)
                                    ((uint32_t)bg->y_pos >> 16)) >> 3);
            r->count = 0x15;
            r->bg_cmd = 0x80;
            r->length = 0x15;
            r->vram_addr = scroll_rebuild_vram(bgnum, r->tile_x,
                                               r->tile_y);
            scroll_rebuild_words(s, r, true);
        }
    }
    bg->x_pos = x + v;
}

/* sub_0803F580 / sub_0803FA10: positive y velocity — the
 * x_pos mirror on y_pos with meta_1, the +edge_add/edge_off
 * constants and the same (pos - 0x10000) >> 19 rebuild probe on
 * both sides. sub_0803FBBC's variant uses +0x1A/+0xD00000
 * (0xD0 << 16); sub_0803FC6C's room-transition sibling
 * (sub_0803FA10) uses +0x14/+0xA00000 (0xA0 << 16). The rebuild
 * streams 31 tile lookups across the screen's tile columns
 * (BgCmdBuffer_WriteData(0, 0x1F, ...)). */
static void scroll_vel_y_pos(AosScroll *s, int bgnum, int32_t vy,
                             AosScrollOut *out, uint32_t edge_add,
                             int32_t edge_off) {
    AosBgLayer *bg = &s->bg[bgnum];
    int32_t y = bg->y_pos;
    int32_t v = vy;
    if (!((((uint32_t)(y + vy)) >> 19) + edge_add <
          ((uint32_t)bg->meta_1 << 5)))
        v = ((int32_t)bg->meta_1 << 24) - (y + edge_off);
    if (v > AOS_SCROLL_VEL_CAP) v = AOS_SCROLL_VEL_CAP;
    bg->y_vel = v;
    if ((((uint32_t)(y - 0x10000)) >> 19) !=
        (((uint32_t)(y + v - 0x10000)) >> 19)) {
        if (out) {
            out->map_rebuild_y = true;
            AosScrollRebuild *r = &out->rebuild_y;
            r->queued = true;
            r->bg = bgnum;
            r->tile_x = (uint16_t)(((uint32_t)(uint16_t)
                                    ((uint32_t)bg->x_pos >> 16)) >> 3);
            r->tile_y = scroll_tile_y_pos(y);
            r->count = 0x1F;
            r->bg_cmd = 0x00;
            r->length = 0x1F;
            r->vram_addr = scroll_rebuild_vram(bgnum, r->tile_x,
                                               r->tile_y);
            scroll_rebuild_words(s, r, false);
        }
    }
    bg->y_pos = y + v;
}

/* sub_0803F650 / sub_0803FAE0: negative y velocity — stops
 * yPos at `pin` whole (a request that would cross it is replaced
 * by the distance back to it), caps at -8 px, integrates and
 * rebuilds on a tile-index change. sub_0803FBBC's variant
 * (sub_0803F650) pins at 0x300000 (0x30 << 16); sub_0803FC6C's
 * room-transition sibling (sub_0803FAE0) pins at 0 — the native
 * test is `yPos + vy < pin`, so the 0x300000 case's
 * `<= 0x2FFFFF` folds into the same comparison. */
static void scroll_vel_y_neg(AosScroll *s, int bgnum, int32_t vy,
                             AosScrollOut *out, int32_t pin) {
    AosBgLayer *bg = &s->bg[bgnum];
    int32_t y = bg->y_pos;
    int32_t v = vy;
    if (y + vy < pin) v = pin - y;
    if (v < -AOS_SCROLL_VEL_CAP) v = -AOS_SCROLL_VEL_CAP;
    bg->y_vel = v;
    if ((((uint32_t)y) >> 19) != (((uint32_t)(y + v)) >> 19)) {
        if (out) {
            out->map_rebuild_y = true;
            AosScrollRebuild *r = &out->rebuild_y;
            r->queued = true;
            r->bg = bgnum;
            r->tile_x = (uint16_t)(((uint32_t)(uint16_t)
                                    ((uint32_t)bg->x_pos >> 16)) >> 3);
            r->tile_y = scroll_tile_y_neg(y);
            r->count = 0x1F;
            r->bg_cmd = 0x00;
            r->length = 0x1F;
            r->vram_addr = scroll_rebuild_vram(bgnum, r->tile_x,
                                               r->tile_y);
            scroll_rebuild_words(s, r, false);
        }
    }
    bg->y_pos = y + v;
}

void aos_scroll_request(AosScroll *s, int bg, int32_t vx, int32_t vy,
                        AosScrollOut *out) {
    AosBgLayer *b = &s->bg[bg];
    if (out) ++out->requests;
    /* pBgMetadata->unk_0 > 1 gates the x axis; <= 1 pins xPos to 0
     * and clears xVel (the one-screen pin). */
    if (b->meta_0 > 1) {
        if (vx > 0)      scroll_vel_x_pos(s, bg, vx, out);
        else if (vx < 0) scroll_vel_x_neg(s, bg, vx, out);
        else             b->x_vel = 0;
    } else {
        b->x_vel = 0;
        b->x_pos = 0;
    }
    /* pBgMetadata->unk_1 > 1 gates the y axis; <= 1 pins yPos to
     * 0x30 << 16 and clears yVel. */
    if (b->meta_1 > 1) {
        if (vy > 0)      scroll_vel_y_pos(s, bg, vy, out, 0x1A, 0xD00000);
        else if (vy < 0) scroll_vel_y_neg(s, bg, vy, out, 0x300000);
        else             b->y_vel = 0;
    } else {
        b->y_pos = 0x300000;
        b->y_vel = 0;
    }
    /* Write-back: the integer halves into bgOfs hOfs/vOfs. */
    s->disp_h[bg] = (uint16_t)((uint32_t)b->x_pos >> 16);
    s->disp_v[bg] = (uint16_t)((uint32_t)b->y_pos >> 16);
}

/* sub_0803FC6C(bg, vx, vy): the room-transition sibling of
 * sub_0803FBBC — byte-identical skeleton (same bgInfo indexing,
 * same meta_0/meta_1 gates, same x helpers sub_0803F3F8/
 * sub_0803F4C8, same bgOfs write-back) but the y axis runs
 * sub_0803FA10/sub_0803FAE0 instead of sub_0803F580/sub_0803F650:
 * the upper-edge test is (y+vy)>>19 + 0x14 against meta_1<<5 and
 * the clamp lands at (meta_1<<24) - (y + 0xA00000) (0xA0 px
 * screen vs 0xD0), negative y stops at 0 rather than the 0x30
 * pin, and a gated axis pins yPos to 0 instead of 0x300000.
 * Called by GameModeIntroCutsceneUpdate stage 3 and
 * GameModeCreditsUpdate stage 3 (the door transition sub_080104EC
 * uses plain sub_0803FBBC on a blanked display instead) with vy =
 * +/-0x8400-class fixed-point steps. */
void aos_scroll_transition_request(AosScroll *s, int bg, int32_t vx,
                                   int32_t vy, AosScrollOut *out) {
    AosBgLayer *b = &s->bg[bg];
    if (out) ++out->requests;
    if (b->meta_0 > 1) {
        if (vx > 0)      scroll_vel_x_pos(s, bg, vx, out);
        else if (vx < 0) scroll_vel_x_neg(s, bg, vx, out);
        else             b->x_vel = 0;
    } else {
        b->x_vel = 0;
        b->x_pos = 0;
    }
    if (b->meta_1 > 1) {
        if (vy > 0)      scroll_vel_y_pos(s, bg, vy, out, 0x14, 0xA00000);
        else if (vy < 0) scroll_vel_y_neg(s, bg, vy, out, 0);
        else             b->y_vel = 0;
    } else {
        b->y_pos = 0;
        b->y_vel = 0;
    }
    s->disp_h[bg] = (uint16_t)((uint32_t)b->x_pos >> 16);
    s->disp_v[bg] = (uint16_t)((uint32_t)b->y_pos >> 16);
}

size_t aos_scroll_shift_entities(const AosScroll *s, int bg,
                                 AosScrollEntityRef *ents, size_t count) {
    /* sub_08000F60: var_0 = -xVel.whole, var_1 = -yVel.whole added
     * to unk_524/unk_528 of every entity with unk_53D_1 set. */
    int32_t dx = -s->bg[bg].x_vel;
    int32_t dy = -s->bg[bg].y_vel;
    size_t shifted = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!ents[i].flagged || !ents[i].fine_x || !ents[i].fine_y)
            continue;
        *ents[i].fine_x += dx;
        *ents[i].fine_y += dy;
        ++shifted;
    }
    return shifted;
}

/* sub_0800D288_inline_0: (delta * (layer_range - base)) /
 * (ref_range - base). ARM SDIV yields 0 on a zero divisor. */
static int32_t scroll_interp(uint16_t layer_range, uint16_t ref_range,
                             int32_t delta, uint16_t base) {
    int32_t den = (int32_t)ref_range - base;
    if (den == 0) return 0;
    return (delta * ((int32_t)layer_range - base)) / den;
}

void aos_scroll_apply(AosScroll *s, const AosScrollWorld *world,
                      AosScrollOut *out) {
    AosBgLayer *b1 = &s->bg[1];
    int32_t sx = b1->x_vel;
    int32_t sy = b1->y_vel;
    uint32_t flags = world ? world->unk_33c3 : 0;
    bool keep4cc = world && world->unk_4cc_0;
    uint32_t p4c8 = world ? world->unk_4c8 : 0;
    if (b1->unk_18 == 0x1A && out) out->fx |= AOS_SCROLL_FX_CDAC;
    for (int bg = 2; bg < 4; ++bg) {
        AosBgLayer *b = &s->bg[bg];
        switch (b->unk_18) {
        case 1:  aos_scroll_request(s, bg, sx, sy, out); break;
        case 5:  aos_scroll_request(s, bg, sx * 2, sy, out); break;
        case 6:  aos_scroll_request(s, bg, sx, sy * 2, out); break;
        case 7:  aos_scroll_request(s, bg, sx * 2, sy * 2, out); break;
        case 2:  aos_scroll_request(s, bg, sx >> 1, sy, out); break;
        case 3:  aos_scroll_request(s, bg, sx, sy >> 1, out); break;
        case 4:  aos_scroll_request(s, bg, sx >> 1, sy >> 1, out); break;
        case 8:  s->disp_h[bg] = (uint16_t)(s->disp_h[bg] + 1); break;
        case 9:  s->disp_h[bg] = (uint16_t)(s->disp_h[bg] + ~sx); break;
        case 10:
            if (flags & 0x02000000)
                s->disp_v[bg] = (uint16_t)(s->disp_v[bg] - 2);
            break;
        case 11:
            aos_scroll_request(s, bg,
                scroll_interp(b->unk_14, b1->unk_14, sx, 0xF0), sy, out);
            break;
        case 12:
            aos_scroll_request(s, bg, sx,
                scroll_interp(b->unk_16, b1->unk_16, sy, 0xD0), out);
            break;
        case 13:
            aos_scroll_request(s, bg,
                scroll_interp(b->unk_14, b1->unk_14, sx, 0xF0),
                scroll_interp(b->unk_16, b1->unk_16, sy, 0xD0), out);
            break;
        case 14: if (out) out->fx |= AOS_SCROLL_FX_CB00; break;
        case 15:
            aos_scroll_request(s, bg,
                scroll_interp(b->unk_14, b1->unk_14, sx, 0xF0),
                scroll_interp(b->unk_16, b1->unk_16, sy, 0xD0), out);
            if (out) out->fx |= AOS_SCROLL_FX_SCANLINE;
            break;
        case 16:
            aos_scroll_request(s, bg,
                scroll_interp(b->unk_14, b1->unk_14, sx, 0xF0),
                scroll_interp(b->unk_16, b1->unk_16, sy, 0xD0), out);
            if (out) out->fx |= AOS_SCROLL_FX_CC90;
            break;
        case 18: if (out) out->fx |= AOS_SCROLL_FX_D000; break;
        case 19:
            if (!keep4cc && (flags & 0x40000) && out)
                out->fx |= AOS_SCROLL_FX_BG2_DISABLE;
            aos_scroll_request(s, bg, sx, sy, out);
            break;
        case 20:
            /* Fills the hBlankEffect buffer with the unk_4C8 wave
             * (0x80 zeros, 8x v, 8x 3v/2, 0x14x 2v) and arms the
             * hblank on REG_BG3HOFS; the buffer write stays
             * unported, the consumed parameter is reported. */
            if (out) { out->fx |= AOS_SCROLL_FX_HBLANK;
                       out->unk_4c8_seen = p4c8; }
            break;
        case 21:
            /* Flag-gated like the native: the affine setup runs
             * only while unk_33C[3] bit 0x02000000 is set, else
             * BG2 is disabled outright. The affine parameters
             * (texX/Y 0x8000, scr 0x70/0x38, sx/sy 0x80, alpha =
             * -unk_4C8 << 9) need BgAffineSet state the host does
             * not model; only the consumed parameter is reported. */
            if (flags & 0x02000000) {
                if (out) { out->fx |= AOS_SCROLL_FX_AFFINE;
                           out->unk_4c8_seen = p4c8; }
            } else if (out) {
                out->fx |= AOS_SCROLL_FX_BG2_DISABLE;
            }
            break;
        case 23:
            aos_scroll_request(s, bg,
                scroll_interp(b->unk_14, b1->unk_14, sx, 0xF0),
                scroll_interp(b->unk_16, b1->unk_16, sy, 0xD0), out);
            if (out) out->fx |= AOS_SCROLL_FX_SCANLINE;
            break;
        case 24:
            aos_scroll_request(s, bg, sx, sy, out);
            if (out) out->fx |= AOS_SCROLL_FX_CC90;
            break;
        case 25: if (out) out->fx |= AOS_SCROLL_FX_CC90; break;
        case 26: aos_scroll_request(s, bg, sx, sy, out); break;
        case 27:
            /* BgAffineSet on BG2 with scrX/scrY = 0x100 - BG1's
             * integer position and sx/sy/alpha derived from
             * unk_4C8 through sub_080009E4's sine table; neither
             * the affine destination nor the sine table is
             * modelled, so the parameter is only reported. */
            if (out) { out->fx |= AOS_SCROLL_FX_AFFINE;
                       out->unk_4c8_seen = p4c8; }
            break;
        case 28:
        case 29: if (out) out->fx |= AOS_SCROLL_FX_SCANLINE; break;
        case 30:
            /* The scroll state lands (xPos += BG1 vel, hOfs write);
             * the sine hblank wave over vOfs
             * ((sub_080009E4((v<<0xB) + (unk_4C8<<0xA)) >> 0xD) +
             * vOfs per scanline) stays unported — reported. */
            if (out) { out->fx |= AOS_SCROLL_FX_HBLANK;
                       out->unk_4c8_seen = p4c8; }
            b->x_pos += sx;
            s->disp_h[bg] = (uint16_t)((uint32_t)b->x_pos >> 16);
            break;
        case 31:
            aos_scroll_request(s, bg, sx, sy, out);
            if (out) out->fx |= AOS_SCROLL_FX_HBLANK;
            break;
        case 32: if (out) out->fx |= AOS_SCROLL_FX_SCANLINE; break;
        case 36:
            /* Same hblank wave shape as case 30 over hOfs with a
             * different phase step; only the parameter is
             * reported. */
            aos_scroll_request(s, bg, sx, sy, out);
            if (out) { out->fx |= AOS_SCROLL_FX_HBLANK;
                       out->unk_4c8_seen = p4c8; }
            break;
        default: break;
        }
    }
}

int aos_scroll_transaction(AosScroll *s, uint8_t gate_byte, int param0,
                           int32_t vx, int32_t vy,
                           const AosScrollWorld *world, AosScrollOut *out) {
    /* sub_0800ED24: gEwramData +0x427 > param0 suppresses the whole
     * transaction; the callers discard the return value. */
    if (gate_byte > param0) return 0;
    scroll_out_reset(out);
    aos_scroll_request(s, 1, vx, vy, out);
    if (world && world->entities && out)
        out->entities_shifted =
            aos_scroll_shift_entities(s, 1, world->entities,
                                      world->entity_count);
    else if (out)
        out->entities_shifted = 0;
    aos_scroll_apply(s, world, out);
    return 1;
}
