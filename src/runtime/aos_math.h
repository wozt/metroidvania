/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow fixed-point trigonometry shared by the enemies, the souls
 * and the familiars: the ROM sine sub_080009E4 (sSineTable) and the BIOS
 * ArcTan2. No SDL. */
#ifndef AOS_MATH_H
#define AOS_MATH_H

#include <stdint.h>

/* sub_080009E4: sine of a 16-bit angle (0x10000 per turn), 16.16. */
int32_t aos_sine(uint32_t angle);
/* ArcTan2 (BIOS call 0x0A): the angle of (x, y), 0x10000 per turn. */
uint16_t aos_arctan2(int16_t x, int16_t y);

#endif
