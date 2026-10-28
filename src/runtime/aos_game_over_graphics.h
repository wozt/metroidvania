/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_GAME_OVER_GRAPHICS_H
#define AOS_GAME_OVER_GRAPHICS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/aos.h"

/* The game-over screen of GameModeGameOverUpdate from the verified ROM:
 * BG1 in 256 colours (tiles 0x080E5C08 at VRAM 0 and 0x080E5C10 at 0x4000,
 * palette 0x080E6A3C, map 0x080E9584) and the language's GAME OVER sprite
 * (English 0x0825B220 / 0x0825E06C, French 0x082799A0 / 0x0827A9FC, German
 * 0x0826E85C / 0x0826F894, palette 0x0825C368) drawn at (0x80, 0xE). */
typedef struct {
    FusionAosRenderedBackground background;
    uint8_t *sprite_bmp;
    size_t sprite_bmp_size;
    int sprite_left, sprite_top;
    uint8_t language;
} AosGameOverGraphics;

enum { AOS_GAME_OVER_SPRITE_X = 0x80, AOS_GAME_OVER_SPRITE_Y = 0xE };

bool aos_game_over_graphics_init(AosGameOverGraphics *graphics, const FusionRom *rom,
                                 uint8_t language, char *error, size_t error_size);
void aos_game_over_graphics_free(AosGameOverGraphics *graphics);
#endif
