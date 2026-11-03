/* SPDX-License-Identifier: GPL-3.0-only */
/* The decoded layers of Aria's title screen and the BG2 affine transform
 * (filled by aos_title_graphics; no ROM access here). No SDL. */
#ifndef AOS_TITLE_LAYERS_H
#define AOS_TITLE_LAYERS_H

#include <stdbool.h>
#include <stdint.h>

enum { AOS_TITLE_TEXT_LAYERS = 2, AOS_TITLE_LAYER_SIZE = 256 };

/* Each layer as 256 x 256 palette indices (0: transparent): BG0 and BG1
 * hold row * 16 + colour, BG2 a 256-colour index. */
typedef struct {
    uint8_t text[AOS_TITLE_TEXT_LAYERS][AOS_TITLE_LAYER_SIZE * AOS_TITLE_LAYER_SIZE];
    uint8_t affine[AOS_TITLE_LAYER_SIZE * AOS_TITLE_LAYER_SIZE];
    uint16_t palette[256];  /* the BG palette resource (sub_0803C8B0) */
} AosTitleGraphics;


/* BgAffineSet for TitleScreenSetAffine_inline: the texture point (128, 128)
 * at the screen point (120, 80), scaled by 0x100 / `scale` and turned by
 * `angle` (frameCounter * 16); the 8.8 matrix (pa, pb, pc, pd) and the
 * 20.8 start point (x, y) as the BIOS computes them. */
typedef struct {
    int16_t pa, pb, pc, pd;
    int32_t x, y;
} AosTitleAffine;

AosTitleAffine aos_title_affine(int16_t scale, uint16_t angle);

/* The BG2 index at screen (sx, sy): 0 outside the 256 x 256 layer (no
 * wrap) or on a transparent pixel. */
uint8_t aos_title_affine_index(const AosTitleGraphics *graphics, const AosTitleAffine *affine,
                               int sx, int sy);

#endif
