/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_INTRO_GRAPHICS_H
#define AOS_INTRO_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/aos.h"

enum { AOS_INTRO_BACKGROUND_COUNT = 3 };

typedef struct {
    FusionAosRenderedBackground backgrounds[AOS_INTRO_BACKGROUND_COUNT];
    uint8_t backdrop_rgb[3];
    uint8_t text_rgb[3];
} AosIntroGraphics;

bool aos_intro_graphics_init(AosIntroGraphics *graphics,
                             const FusionRom *rom,
                             char *error, size_t error_size);
void aos_intro_graphics_free(AosIntroGraphics *graphics);

#endif
