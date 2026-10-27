/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_hud.h"

#include <string.h>

enum { HUD_PALETTE_ROW = 0xD };

static void store(AosHud *hud, int x, int y, uint16_t entry) {
    if (x >= 0 && y >= 0 && x < AOS_HUD_MAP_SIZE && y < AOS_HUD_MAP_SIZE)
        hud->map[y * AOS_HUD_MAP_SIZE + x] = entry;
}

/* sUnk_080E0F74: two frame blocks (map x, y, first tile column and row,
 * width, height) laid from tile 0x200 + column + row * 16. */
static void frame(AosHud *hud) {
    static const int blocks[2][6] = {{0, 0, 9, 0, 4, 5}, {12, 0, 13, 0, 2, 4}};
    for (int i = 0; i < 2; ++i)
        for (int row = 0; row < blocks[i][5]; ++row)
            for (int column = 0; column < blocks[i][4]; ++column)
                store(hud, blocks[i][0] + column, blocks[i][1] + row,
                      (uint16_t)((0x200 + column + blocks[i][2] + (blocks[i][3] + row) * 0x10) |
                                 HUD_PALETTE_ROW << 12));
}

void aos_hud_bar(AosHud *hud, int x, int y, int value, int maximum, bool mp) {
    int base = mp ? 0x210 : 0x200, fill = 0;
    if (value > 0 && maximum > 0) {
        fill = (value * 100 / maximum << 6) / 100;
        if (fill == 0) fill = 1;
    }
    for (int i = 0; i < 8; ++i) {
        int tile = i == fill / 8 ? base + fill - i * 8 : fill >= (i + 1) * 8 ? base + 8 : base;
        store(hud, x + i, y, (uint16_t)(tile | HUD_PALETTE_ROW << 12));
    }
}

void aos_hud_init(AosHud *hud, int16_t hp, int16_t max_hp, int16_t mp, int16_t max_mp) {
    memset(hud->map, 0, sizeof hud->map);
    frame(hud);
    hud->hp = hp;
    hud->mp = mp;
    hud->max_hp = max_hp;
    hud->max_mp = max_mp;
    aos_hud_bar(hud, 4, 1, hud->hp, max_hp, false);
    aos_hud_bar(hud, 4, 2, hud->mp, max_mp, true);
}

/* One step of a shown value toward its target. */
static bool chase(int16_t *shown, int16_t target) {
    if (*shown == target) return false;
    *shown = (int16_t)(*shown < target ? *shown + 1 : *shown - 1);
    return true;
}

void aos_hud_update(AosHud *hud, int16_t hp, int16_t max_hp, int16_t mp, int16_t max_mp) {
    bool moved = false;
    for (int i = 0; i < 2; ++i) {
        moved |= chase(&hud->hp, hp);
        moved |= chase(&hud->max_hp, max_hp);
    }
    if (moved) aos_hud_bar(hud, 4, 1, hud->hp, hud->max_hp, false);
    moved = false;
    for (int i = 0; i < 2; ++i) {
        moved |= chase(&hud->mp, mp);
        moved |= chase(&hud->max_mp, max_mp);
    }
    if (moved) aos_hud_bar(hud, 4, 2, hud->mp, hud->max_mp, true);
}

bool aos_hud_pixel(const AosHud *hud, const uint8_t *tiles, const uint8_t *palette,
                   unsigned x, unsigned y, uint16_t *color) {
    unsigned map_y = (y + AOS_HUD_SCROLL_Y) % (AOS_HUD_MAP_SIZE * 8);
    uint16_t entry = hud->map[(map_y / 8) * AOS_HUD_MAP_SIZE + (x / 8) % AOS_HUD_MAP_SIZE];
    unsigned tile = entry & 0x3FF;
    if (tile < 0x200 || tile > 0x2FF) return false;
    unsigned px = x % 8, py = map_y % 8;
    if (entry & 0x400) px = 7 - px;
    if (entry & 0x800) py = 7 - py;
    uint8_t byte = tiles[(tile - 0x200) * 32 + py * 4 + px / 2];
    unsigned index = px & 1 ? byte >> 4 : byte & 0xF;
    if (!index) return false;
    *color = (uint16_t)(palette[index * 2] | palette[index * 2 + 1] << 8);
    return true;
}
