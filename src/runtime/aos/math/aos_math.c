/* SPDX-License-Identifier: GPL-3.0-only */
/* The game's sine and the BIOS ArcTan2: see aos_math.h. */
#include "aos_math.h"

#include <math.h>

/* sSineTable: every entry is floor(sin(i * pi / 2048) * 65536), which was
 * checked against all 1024 entries of the original table. */
static uint16_t sine_table[0x400];

static void build_sine(void) {
    if (sine_table[1]) return;
    for (int i = 0; i < 0x400; ++i)
        sine_table[i] = (uint16_t)floor(sin(i * 3.14159265358979323846 / 2048.0) * 65536.0);
}

int32_t aos_sine(uint32_t angle) {
    build_sine();
    uint32_t a = (angle + (0x80 >> 4)) & 0xFFFF;
    int32_t sign = 1;
    if (a & 0x8000) {
        a -= 0x8000;
        sign = -1;
    }
    if (a & 0x4000) {
        if (a & 0xFFFFBFF0u) return sign * (int32_t)sine_table[(0x800F - a) >> 4];
        return sign * 0x10000;
    }
    return sign * (int32_t)sine_table[a >> 4];
}

/* ArcTan2 (BIOS call 0x0A). The polynomial is the BIOS one as reimplemented
 * by mGBA's high-level BIOS; it is not checked against the BIOS itself. */
static int32_t arctan(int32_t i) {
    int32_t a = -((i * i) >> 14);
    int32_t b = ((0xA9 * a) >> 14) + 0x390;
    b = ((b * a) >> 14) + 0x91C;
    b = ((b * a) >> 14) + 0xFB6;
    b = ((b * a) >> 14) + 0x16AA;
    b = ((b * a) >> 14) + 0x2081;
    b = ((b * a) >> 14) + 0x3651;
    b = ((b * a) >> 14) + 0xA2F9;
    return (int16_t)((i * b) >> 16);
}

uint16_t aos_arctan2(int16_t x16, int16_t y16) {
    int32_t x = x16, y = y16, r;
    if (!y) return x >= 0 ? 0 : 0x8000;
    if (!x) return y >= 0 ? 0x4000 : 0xC000;
    if (y >= 0) {
        if (x >= 0 && x >= y) r = arctan(y * 0x4000 / x);
        else if (x < 0 && -x >= y) r = arctan(y * 0x4000 / x) + 0x8000;
        else r = 0x4000 - arctan(x * 0x4000 / y);
    } else {
        if (x <= 0 && -x > -y) r = arctan(y * 0x4000 / x) + 0x8000;
        else if (x > 0 && x >= -y) r = arctan(y * 0x4000 / x) + 0x10000;
        else r = 0xC000 - arctan(x * 0x4000 / y);
    }
    return (uint16_t)r;
}

/* Sqrt (BIOS call 0x08): the integer square root, rounded down. */
uint32_t aos_isqrt(uint32_t x) {
    uint32_t r = 0;
    for (uint32_t bit = 1u << 30; bit; bit >>= 2) {
        if (x >= r + bit) {
            x -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
    }
    return r;
}

/* sub_0803E86C: a 16.16 product from the operands' top 24 bits, rounded
 * toward zero to a multiple of 0x100. */
int32_t aos_fixed_mul(int32_t a, int32_t b) {
    if (!a || !b) return 0;
    if (a == 0x10000) return b;
    if (b == 0x10000) return a;
    int32_t a8 = a < 0 ? -((-a) >> 8) : a >> 8;
    int32_t b8 = b < 0 ? -((-b) >> 8) : b >> 8;
    int32_t p = a8 * b8;
    if (p < 0) p += 0xFF;
    return (int32_t)((uint32_t)(p >> 8) << 8);
}

bool aos_probe_box(int16_t ex, int16_t ey, int16_t x, int16_t y,
                   int16_t w, int16_t h) {
    /* (ent - origin) must fit the box; the ROM's shifted
     * compares make negative deltas fail. */
    int32_t dx = (int16_t)(ex - x);
    int32_t dy = (int16_t)(ey - y);
    return dx >= 0 && dx <= w && dy >= 0 && dy <= h;
}

uint16_t aos_entity_angle(int16_t ax, int16_t ay,
                          int16_t bx, int16_t by) {
    return aos_arctan2((int16_t)(bx - ax), (int16_t)(by - ay));
}

int32_t aos_phase_opposite(int32_t v) {
    return (int16_t)(0x8000 - (int16_t)v);
}

int32_t aos_phase_negate(int32_t v) {
    return -(int16_t)v;
}

int32_t aos_phase_comp_f0(int32_t v) {
    return 0xF00000 - v;
}

int32_t aos_phase_comp_a0(int32_t v) {
    return 0xA00000 - v;
}

int aos_sign_delta(int32_t v, int32_t ref) {
    if (v < ref) return 1;
    if (v > ref) return -1;
    return 0;
}

bool aos_within_range(int32_t ex, int32_t ey,
                      int32_t rx, int32_t ry, int32_t range) {
    int32_t dx = (ex - rx) >> 16;
    int32_t dy = (ey - ry) >> 16;
    uint32_t d2 = (uint32_t)(dx * dx + dy * dy);
    return ((uint32_t)aos_isqrt(d2) << 16) <= (uint32_t)range;
}

int aos_s16_sign(int16_t a, int16_t b) {
    /* ROM: +1 when (a - b) as u16 exceeds (b - a) as u16,
     * i.e. when b is "ahead" of a on the s16 circle. */
    uint16_t d1 = (uint16_t)(a - b);
    uint16_t d2 = (uint16_t)(b - a);
    if (d1 == d2) return 0;
    return d1 > d2 ? 1 : -1;
}

void aos_steer_toward(int32_t *px, int32_t *py,
                      int32_t tx, int32_t ty, uint32_t angle) {
    /* Faithful port of sub_08068BF0: the ROM rotates the
     * target delta by the entity angle through two sine
     * lookups (angle and angle + 0x4000) and writes the
     * result back into the fine position. */
    uint16_t a = (uint16_t)(int16_t)angle;
    uint16_t a90 = (uint16_t)(a + 0x4000);
    int32_t dx = (*px - tx) >> 16;
    int32_t dy = (*py - ty) >> 16;
    int32_t s90 = (int32_t)aos_sine(a90) >> 8;
    int32_t s0 = (int32_t)aos_sine(a) >> 8;
    int32_t nx = dx * s90 - dy * s0 + tx;
    int32_t ny = dx * s0 + dy * s90 + ty;
    *px = nx;
    *py = ny;
}

static int32_t clamp_approach(int32_t cur, int32_t bound, int32_t next) {
    /* The ROM's branch pair: approaching from below clamps at
     * bound from above, approaching from above clamps at bound
     * from below; an exact bound match keeps next. */
    if (bound > cur) {
        if (next > bound) return bound;
    } else if (bound < cur && next < bound) {
        return bound;
    }
    return next;
}

int32_t aos_approach_step(int32_t cur, int32_t bound,
                          int32_t step, int32_t scale) {
    return clamp_approach(cur, bound, cur + step * scale);
}

int32_t aos_advance_along(int32_t *px, int32_t *py, uint16_t angle,
                          int32_t cur, int32_t bound,
                          int32_t step, int32_t scale) {
    int32_t dist = clamp_approach(cur, bound, cur + step * scale);
    int32_t d = dist >> 8;
    int32_t sx = (int32_t)aos_sine((uint16_t)(angle + 0x4000)) >> 8;
    int32_t sy = (int32_t)aos_sine(angle) >> 8;
    *px += sx * d;
    *py += sy * d;
    return dist;
}
