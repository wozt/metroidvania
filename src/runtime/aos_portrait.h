/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_PORTRAIT_H
#define AOS_PORTRAIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/rom.h"

enum { AOS_PORTRAIT_COUNT = 8, AOS_PORTRAIT_WIDTH = 128, AOS_PORTRAIT_HEIGHT = 128 };

typedef struct {
    uint8_t rgba[AOS_PORTRAIT_WIDTH * AOS_PORTRAIT_HEIGHT * 4];
} AosPortraitImage;

/* The dialogue resource table at 0x08526C78 indexes these native graphics
 * descriptors.  The global dialogue palette is shared by all eight entries. */
uint32_t aos_portrait_graphics_pointer(uint8_t portrait);

/* Decode one native 4bpp dialogue portrait to an RGBA image.  Colour index
 * zero is transparent, matching the portrait OBJ tiles. */
bool aos_portrait_decode(const FusionRom *rom, uint8_t portrait,
                         AosPortraitImage *image, char *error, size_t error_size);

#endif
