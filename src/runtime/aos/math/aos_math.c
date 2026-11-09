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
