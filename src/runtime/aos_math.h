/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow fixed-point trigonometry shared by the enemies, the souls
 * and the familiars: the ROM sine sub_080009E4 (sSineTable), the BIOS
 * ArcTan2 and Sqrt, and the 16.16 product sub_0803E86C. No SDL. */
#ifndef AOS_MATH_H
#define AOS_MATH_H

#include <stdint.h>

/* sub_080009E4: sine of a 16-bit angle (0x10000 per turn), 16.16. */
int32_t aos_sine(uint32_t angle);
/* ArcTan2 (BIOS call 0x0A): the angle of (x, y), 0x10000 per turn. */
uint16_t aos_arctan2(int16_t x, int16_t y);

/* Sqrt (BIOS call 0x08): the integer square root, rounded down. */
uint32_t aos_isqrt(uint32_t x);
/* sub_0803E86C: a 16.16 product from the operands' top 24 bits, rounded
 * toward zero to a multiple of 0x100. */
int32_t aos_fixed_mul(int32_t a, int32_t b);

#endif
