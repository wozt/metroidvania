/* SPDX-License-Identifier: GPL-3.0-only */
/* Minimal affine sprite path: see aos_affine_sprite.h for the native
 * routine decode (sub_0803E058/sub_0803E968/sub_0803E86C/sub_0803E8FC). */
#include "aos_affine_sprite.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

/* sub_080009E4: sine lookup returning Q16.16 for a Q16.16 fraction of
 * a turn. The native sSineTable holds 2048 quarter-wave entries in
 * Q16.16 approaching 0xFFFF, and the routine returns explicit
 * Q_16_16(±1) = ±0x10000 at the quadrant peaks, so the host port
 * evaluates the same function with libm (the menu flourishes only ever
 * pass angle 0, so table-vs-libm precision is moot there). */
static int32_t sine_q16(int32_t angle) {
    double turns = (double)((uint32_t)angle & 0xFFFFu) / 65536.0;
    double value = sin(turns * 6.28318530717958647692) * 65536.0;
    if (value >= 65536.0) return 0x10000;
    if (value <= -65536.0) return -0x10000;
    return (int32_t)(value < 0.0 ? value - 0.5 : value + 0.5);
}

/* sub_0803E86C: Q16.16 multiply with the native truncation-toward-zero
 * shifts (the callers divide the result by 256 the same way). */
static int32_t mul_q16(int32_t a, int32_t b) {
    if (a == 0 || b == 0) return 0;
    if (a == 0x10000) return b;
    if (b == 0x10000) return a;
    int32_t ar = a < 0 ? -(-a >> 8) : a >> 8;
    int32_t br = b < 0 ? -(-b >> 8) : b >> 8;
    return (ar * br / 0x100) << 8;
}

/* sub_0803E8FC: Q16.16 reciprocal (0x10000 / (s >> 8)) << 8. */
static int32_t reciprocal_q16(int32_t scale) {
    if (scale == 0x10000) return 0x10000;
    int32_t magnitude = scale < 0 ? -scale : scale;
    int32_t shifted = scale < 0 ? -(magnitude >> 8) : magnitude >> 8;
    if (!shifted) return 0x10000;
    return (0x10000 / shifted) << 8;
}

/* The native stores truncate the Q16.16 product toward zero before the
 * final >>8 into the s16 8.8 parameter. */
static int16_t param_q8(int32_t trig, int32_t reciprocal) {
    int32_t product = mul_q16(trig, reciprocal);
    int32_t value = product < 0 ? -((-product) >> 8) : product >> 8;
    if (value > 0x7FFF) value = 0x7FFF;
    if (value < -0x8000) value = -0x8000;
    return (int16_t)value;
}

void aos_affine_matrix(int32_t angle, int32_t scale_x, int32_t scale_y,
                       bool flip_h, bool flip_v, int16_t params[4]) {
    int32_t sine = sine_q16(angle);
    int32_t cosine = sine_q16(angle + 0x4000);
    /* sub_0803E058: scales at or below 0x400 take the 0x7FFFFF sentinel
     * instead of the reciprocal, which collapses the sprite. */
    int32_t recip_x = scale_x > 0x400 ? reciprocal_q16(scale_x) : 0x7FFFFF;
    int32_t recip_y = scale_y > 0x400 ? reciprocal_q16(scale_y) : 0x7FFFFF;
    int16_t pa = param_q8(cosine, recip_x);
    int16_t pb = param_q8(sine, recip_x);
    int16_t pc = param_q8(-sine, recip_y);
    int16_t pd = param_q8(cosine, recip_y);
    /* unk_C/unk_E flips negate the matching row pair. */
    if (flip_h) { pa = (int16_t)-pa; pb = (int16_t)-pb; }
    if (flip_v) { pc = (int16_t)-pc; pd = (int16_t)-pd; }
    params[0] = pa;
    params[1] = pb;
    params[2] = pc;
    params[3] = pd;
}

static int frame_pixel(const AosObjSheet *sheet, const AosObjFrame *frame,
                       int fx, int fy) {
    for (unsigned k = 0; k < frame->count; ++k) {
        const AosObjPiece *p = &frame->pieces[k];
        int px = fx - p->x, py = fy - p->y;
        if (px < 0 || py < 0 || px >= p->width || py >= p->height) continue;
        int sx = p->source_x + (p->hflip ? p->width - 1 - px : px);
        int sy = p->source_y + (p->vflip ? p->height - 1 - py : py);
        int c = sheet->pixels[sy * AOS_OBJ_SHEET_SIZE + sx];
        if (c) return c;
    }
    return 0;
}

void aos_affine_sprite_draw(const AosObjSheet *sheet, const AosObjFrame *frame,
                            const uint16_t palette[16], int anchor_x,
                            int anchor_y, const int16_t params[4],
                            uint8_t *rgba, int width, int height) {
    if (!sheet || !frame || !palette || !rgba || width <= 0 || height <= 0)
        return;
    int reach = 0;
    for (unsigned k = 0; k < frame->count; ++k) {
        const AosObjPiece *p = &frame->pieces[k];
        int e[4] = {abs(p->x), abs(p->y), abs(p->x + p->width),
                    abs(p->y + p->height)};
        for (int j = 0; j < 4; ++j)
            if (e[j] > reach) reach = e[j];
    }
    /* Affine sprites cover the double-size box; the same *3 bound as
     * aos_obj_layer_draw keeps every reachable frame texel inside. */
    reach *= 3;
    int pa = params[0], pb = params[1], pc = params[2], pd = params[3];
    for (int dy = -reach; dy <= reach; ++dy) {
        int y = anchor_y + dy;
        if (y < 0 || y >= height) continue;
        for (int dx = -reach; dx <= reach; ++dx) {
            int x = anchor_x + dx;
            if (x < 0 || x >= width) continue;
            int fx = (pa * dx + pb * dy) >> 8;
            int fy = (pc * dx + pd * dy) >> 8;
            int c = frame_pixel(sheet, frame, fx, fy);
            if (!c || c > 15) continue;
            uint16_t bgr = palette[c];
            uint8_t *out = rgba + ((size_t)y * width + x) * 4;
            out[0] = (uint8_t)(((bgr & 31) * 255) / 31);
            out[1] = (uint8_t)((((bgr >> 5) & 31) * 255) / 31);
            out[2] = (uint8_t)((((bgr >> 10) & 31) * 255) / 31);
            out[3] = 255;
        }
    }
}
