/* SPDX-License-Identifier: GPL-3.0-only */
/* The two startup logos of Aria (stage 1 of GameModeLicensedByNintendoUpdate
 * and GameModeKonamiLogoUpdate): the character resource on block 0, the
 * palette and the BG0 layer of each, and their screen under the BLDCNT /
 * BLDY of the native controllers (aos_game_mode). No SDL. */
#ifndef AOS_LOGO_GRAPHICS_H
#define AOS_LOGO_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/rom.h"

typedef enum { AOS_LOGO_NINTENDO, AOS_LOGO_KONAMI } AosLogo;

enum { AOS_LOGO_W = 240, AOS_LOGO_H = 160, AOS_LOGO_LAYER = 256 };

typedef struct {
    uint8_t bg0[AOS_LOGO_LAYER * AOS_LOGO_LAYER];   /* row * 16 + colour, 0: transparent */
    uint16_t palette[256];                           /* BG palette, 15-bit colours */
} AosLogoGraphics;

/* Nintendo: sub_0803FD9C(0x080E5C00), sub_0803C8B0(0x080E6838), BG0
 * 0x080E94F4. Konami: sub_0803FD9C(0x080E5C18), row 0 of 0x080E6C40
 * (sub_0803C918), BG0 0x080E9614. */
bool aos_logo_graphics_init(AosLogoGraphics *graphics, const FusionRom *rom, AosLogo logo,
                            char *error, size_t error_size);

/* BG0 (or the backdrop, colour 0) through BLDCNT's brightness effect (bits
 * 6-7: 2 increase toward white, 3 decrease toward black, on the first
 * targets of bits 0 and 5) with BLDY, as 240 x 160 RGBA pixels. */
void aos_logo_render(const AosLogoGraphics *graphics, uint16_t blend_control, uint16_t brightness,
                     uint8_t rgba[AOS_LOGO_W * AOS_LOGO_H * 4]);

#endif
