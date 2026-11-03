/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria's object sheets from the ROM (aos_obj.h). No SDL. */
#ifndef AOS_OBJ_ROM_H
#define AOS_OBJ_ROM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_obj.h"
#include "fusion/assets/rom.h"

/* sub_0803B800 then sub_0803B924: the 4bpp graphics resource `graphics` (a
 * 16 x 16-tile sheet) and the frame descriptor `frames` (+ 0: records,
 * + 2: animations, + 4: the 16-byte records, + 12: the animation table). */
bool aos_obj_sheet_decode(AosObjSheet *sheet, const FusionRom *rom, uint32_t graphics,
                          uint32_t frames, char *error, size_t error_size);

#endif
