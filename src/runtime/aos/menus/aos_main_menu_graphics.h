/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_MAIN_MENU_GRAPHICS_H
#define AOS_MAIN_MENU_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/aos.h"

typedef enum {
    AOS_MAIN_GRAPHICS_BASE,
    AOS_MAIN_GRAPHICS_PLAY,
    AOS_MAIN_GRAPHICS_COPY,
    AOS_MAIN_GRAPHICS_DELETE,
    AOS_MAIN_GRAPHICS_NAME,
    AOS_MAIN_GRAPHICS_SCREEN_COUNT,
} AosMainGraphicsScreen;

enum { AOS_MAIN_GRAPHICS_LAYER_COUNT = 3 };

typedef struct {
    FusionAosRenderedBackground
        backgrounds[AOS_MAIN_GRAPHICS_SCREEN_COUNT][AOS_MAIN_GRAPHICS_LAYER_COUNT];
    uint8_t backdrop_rgb[3];
    uint8_t language;
} AosMainMenuGraphics;

bool aos_main_menu_graphics_init(AosMainMenuGraphics *graphics,
                                 const FusionRom *rom, uint8_t language,
                                 char *error, size_t error_size);
void aos_main_menu_graphics_free(AosMainMenuGraphics *graphics);

#endif
