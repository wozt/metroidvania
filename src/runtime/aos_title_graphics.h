/* SPDX-License-Identifier: GPL-3.0-only */
/* The backgrounds of Aria's title screen (GameModeTitleScreenUpdate stage
 * 1): the palette resource 0x080E6430, the character resources 0x080E5BE0
 * (block 0) and 0x080E5BE8 (block 2), and the three layers of
 * sub_0803F8A8: BG0 (0x080E9024) and BG1 (0x080E90B4), 4bpp text layers on
 * character block 2, and BG2 (0x080E8F94), the 256-colour affine layer of
 * mode 1 on block 0 (sub_0803F7C8: an 8 x 8 map of 4 x 4-tile blocks, one
 * byte a tile); and the three object sheets of aos_title_layers.h. No SDL. */
#ifndef AOS_TITLE_GRAPHICS_H
#define AOS_TITLE_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/aos.h"

#include "aos_title_layers.h"

/* sub_0803F710 for a 4bpp text layer (metadata as sub_0803F8A8 reads it)
 * whose tiles sit on `character_block` of `vram`: 256 x 256 indices, each
 * row * 16 + colour, 0 where transparent (pixels not written). */
bool aos_title_decode_text_layer(uint8_t *out, const FusionRom *rom, uint32_t metadata,
                                 unsigned character_block, const uint8_t *vram, char *error,
                                 size_t error_size);
bool aos_title_graphics_init(AosTitleGraphics *graphics, const FusionRom *rom, char *error,
                             size_t error_size);

#endif
