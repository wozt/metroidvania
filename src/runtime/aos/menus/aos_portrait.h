/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_PORTRAIT_H
#define AOS_PORTRAIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/rom.h"

/* Aria draws dialogue portraits on BG0 (240x160), not as a 128x128 sprite.
 * The native tile map places Soma's fragmented graphics on screen. */
enum { AOS_PORTRAIT_COUNT = 8, AOS_PORTRAIT_WIDTH = 240, AOS_PORTRAIT_HEIGHT = 160 };

typedef struct {
    uint8_t rgba[AOS_PORTRAIT_WIDTH * AOS_PORTRAIT_HEIGHT * 4];
} AosPortraitImage;

/* The eight resource pointers originally referenced by 0x08526C78.
 * Retained for the existing inventory tests. */
uint32_t aos_portrait_graphics_pointer(uint8_t portrait);

/* Draw the actual BG0 portrait using three ROM pointers from its 12-byte
 * descriptor: graphics, palette, and metatile-backed BG metadata.
 * Index-zero pixels and tiles outside this portrait's graphics bank are
 * transparent; this image can be composited over the live game scene. */
bool aos_portrait_decode(const FusionRom *rom, uint8_t portrait,
                         AosPortraitImage *image, char *error, size_t error_size);

#endif
