/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_INTRO_GRAPHICS_H
#define AOS_INTRO_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/aos.h"
#include "../../aos/game/aos_obj.h"

enum { AOS_INTRO_BACKGROUND_COUNT = 3, AOS_INTRO_ACTOR_SHEETS = 2 };

typedef struct {
    FusionAosRenderedBackground backgrounds[AOS_INTRO_BACKGROUND_COUNT];
    AosObjSheet actors[AOS_INTRO_ACTOR_SHEETS];
    uint16_t object_palette[256];
    uint8_t backdrop_rgb[3];
    uint8_t text_rgb[3];
} AosIntroGraphics;

bool aos_intro_graphics_init(AosIntroGraphics *graphics,
                             const FusionRom *rom,
                             char *error, size_t error_size);
void aos_intro_graphics_free(AosIntroGraphics *graphics);

#endif
