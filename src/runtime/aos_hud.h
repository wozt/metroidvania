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
    AOS_HUD_PALETTE_BYTES = 0x60,
    AOS_HUD_FONT_GLYPHS = 0xE0,
    AOS_HUD_FONT_METRIC_BYTES = AOS_HUD_FONT_GLYPHS * 4,
    AOS_HUD_FONT_BITMAP_BYTES = AOS_HUD_FONT_GLYPHS * 10,
    AOS_HUD_NOTICE_TILE_BYTES = 0x400,
    AOS_HUD_TILE_OFFSET = AOS_HUD_PALETTE_BYTES,
    AOS_HUD_FONT_METRIC_OFFSET = AOS_HUD_TILE_OFFSET + AOS_HUD_TILE_BYTES,
    AOS_HUD_FONT_BITMAP_OFFSET = AOS_HUD_FONT_METRIC_OFFSET + AOS_HUD_FONT_METRIC_BYTES,
    AOS_HUD_ASSET_BYTES = AOS_HUD_PALETTE_BYTES + AOS_HUD_TILE_BYTES +
                          AOS_HUD_FONT_METRIC_BYTES + AOS_HUD_FONT_BITMAP_BYTES,
    AOS_HUD_SCROLL_Y = 6,
};

typedef struct {
    const uint8_t *metrics; /* sUnk_080E2EC0: left offset, width, two unused bytes */
    const uint8_t *glyphs;  /* sUnk_08509876: ten bytes per glyph, first eight drawn */
} AosHudFont;

typedef struct {
    uint16_t map[AOS_HUD_MAP_SIZE * AOS_HUD_MAP_SIZE];
    int16_t hp, mp, max_hp, max_mp;     /* unk_404, unk_406, unk_408, unk_40A */
    uint8_t notice_tiles[2][AOS_HUD_NOTICE_TILE_BYTES];
    uint16_t notice_timer;              /* unk_422: 0xF0 frames */
    uint8_t notice_page;                /* unk_424: alternating 0x280/0x2A0 tiles */
    bool notice_active;
    uint32_t revision;                  /* dynamic tiles can change without map changes */
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
/* sub_0800EA98 queues the defeated enemy's native text id; sub_0800DE4C
 * renders it into the alternating context-B font page and right-aligns its
 * three-row frame at the bottom of BG0. The caller supplies the resolved
 * language string so the state stays usable by the runtime and editor. */
bool aos_hud_show_enemy(AosHud *hud, const AosHudFont *font, const char *name);
/* sub_0800EFD4: expire the current enemy/item notice after 0xF0 frames. */
void aos_hud_tick(AosHud *hud);
/* The BGR555 colour of screen pixel (x, y), false when transparent; tiles
 * holds 0x2000 bytes for tiles 0x200-0x2FF. Palette holds the initial rows
 * 13-14 followed by the row-14 enemy-notice palette. */
bool aos_hud_pixel(const AosHud *hud, const uint8_t *tiles, const uint8_t *palette,
                   unsigned x, unsigned y, uint16_t *color);
#endif
