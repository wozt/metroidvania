/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_NOTICE_H
#define AOS_SOUL_NOTICE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_intro_font.h"
#include "fusion/assets/rom.h"

enum { AOS_SOUL_NOTICE_MAP_SIZE = 32, AOS_SOUL_NOTICE_STATIC_BYTES = 0x2000 };

typedef struct {
    uint16_t map[AOS_SOUL_NOTICE_MAP_SIZE * AOS_SOUL_NOTICE_MAP_SIZE];
    uint8_t static_tiles[AOS_SOUL_NOTICE_STATIC_BYTES];
    AosIntroTextTiles dynamic_tiles;
    uint8_t palette[0x200];
} AosSoulNotice;

bool aos_soul_notice_init(AosSoulNotice *notice, const FusionRom *rom,
                          char *error, size_t error_size);
/* Build sub_0800E708(0, 0, arg2) and, when `description` is set (the
 * native arg2 != 0 path), sub_0800E540(0, 1). `name_tiles` is
 * ceil(the soul-name pixel width / 8); player_low selects the upper
 * layout. */
void aos_soul_notice_build(AosSoulNotice *notice,
                           const AosIntroTextTiles *rendered,
                           unsigned name_tiles, bool player_low,
                           bool description);
bool aos_soul_notice_pixel(const AosSoulNotice *notice, uint32_t frame,
                           unsigned x, unsigned y, uint16_t *color);
#endif
