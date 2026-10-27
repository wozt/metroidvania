/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_HUD_H
#define AOS_HUD_H
#include <stdbool.h>
#include <stdint.h>

/* The in-game HUD on BG0 (src/code_0800CB00.c): map base 28, tiles 0x200-
 * 0x2FF from 0x0827B208, palette 13 from 0x0820C428, BG0 scrolled 6 pixels
 * up. Shown values (gEwramData->unk_60.unk_404-0x40A) chase the real ones. */
enum {
    AOS_HUD_MAP_SIZE = 32,
    AOS_HUD_TILE_BYTES = 0x2000,
    AOS_HUD_SCROLL_Y = 6,
};

typedef struct {
    uint16_t map[AOS_HUD_MAP_SIZE * AOS_HUD_MAP_SIZE];
    int16_t hp, mp, max_hp, max_mp;     /* unk_404, unk_406, unk_408, unk_40A */
} AosHud;

/* sub_0800DA50: a cleared map, the frame of sub_0800DB78, the shown values
 * set to the real ones and both bars drawn (sub_0800F138). */
void aos_hud_init(AosHud *hud, int16_t hp, int16_t max_hp, int16_t mp, int16_t max_mp);
/* sub_0800F138: an 8-tile bar at map cell (x, y) filled value * 64 / max
 * pixels (at least 1 when the value is positive), from tile 0x200 (HP) or
 * 0x210 (MP). */
void aos_hud_bar(AosHud *hud, int x, int y, int value, int maximum, bool mp);
/* sub_0800DC70: each shown value moves up to two steps toward the real one;
 * a bar is redrawn when its values moved. */
void aos_hud_update(AosHud *hud, int16_t hp, int16_t max_hp, int16_t mp, int16_t max_mp);
/* The BGR555 colour of screen pixel (x, y), false when transparent; tiles
 * holds 0x2000 bytes for tiles 0x200-0x2FF and palette 16 colours. */
bool aos_hud_pixel(const AosHud *hud, const uint8_t *tiles, const uint8_t *palette,
                   unsigned x, unsigned y, uint16_t *color);
#endif
