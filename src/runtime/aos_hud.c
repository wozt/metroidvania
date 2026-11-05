/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_hud.h"

#include <string.h>

enum { HUD_PALETTE_ROW = 0xD };

static void store(AosHud *hud, int x, int y, uint16_t entry) {
    if (x >= 0 && y >= 0 && x < AOS_HUD_MAP_SIZE && y < AOS_HUD_MAP_SIZE)
        hud->map[y * AOS_HUD_MAP_SIZE + x] = entry;
}

static void clear_notice_map(AosHud *hud) {
    for (int y = 18; y <= 20; ++y)
        for (int x = 0; x < AOS_HUD_MAP_SIZE; ++x)
            store(hud, x, y, 0);
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
    memset(hud, 0, sizeof *hud);
    frame(hud);
    hud->hp = hp;
    hud->mp = mp;
    hud->max_hp = max_hp;
    hud->max_mp = max_mp;
    aos_hud_bar(hud, 4, 1, hud->hp, max_hp, false);
    aos_hud_bar(hud, 4, 2, hud->mp, max_mp, true);
}

static void notice_pixel(uint8_t *tiles, unsigned x, unsigned y, uint8_t color) {
    if (x >= 32 * 8 || y >= 8) return;
    size_t offset = (x / 8) * 32 + y * 4 + (x % 8) / 2;
    unsigned shift = (x & 1u) * 4u;
    tiles[offset] = (uint8_t)((tiles[offset] & ~(0xFu << shift)) |
                              ((color & 0xFu) << shift));
}

static unsigned render_notice(uint8_t *tiles, const AosHudFont *font, const char *name) {
    memset(tiles, 0, AOS_HUD_NOTICE_TILE_BYTES);
    unsigned cursor = 0;
    for (const unsigned char *text = (const unsigned char *)name; *text; ++text) {
        if (*text < 0x20) continue;
        unsigned glyph = *text - 0x20;
        if (glyph >= AOS_HUD_FONT_GLYPHS) continue;
        const uint8_t *metric = font->metrics + glyph * 4;
        const uint8_t *bitmap = font->glyphs + glyph * 10;
        unsigned left = metric[0], width = metric[1];
        if (left > 7 || width > 8 || left + width > 8) continue;
        if (cursor + width > 20 * 6) break;
        for (unsigned y = 0; y < 8; ++y) {
            uint8_t bits = (uint8_t)(bitmap[y] << left);
            for (unsigned x = 0; x < width; ++x)
                notice_pixel(tiles, cursor + x, y,
                             bits & (0x80u >> x) ? 0xFu : 1u);
        }
        cursor += width;
    }
    return cursor;
}

static bool show_notice(AosHud *hud, const AosHudFont *font, const char *name,
                        AosHudNoticeKind kind) {
    if (!hud || !font || !font->metrics || !font->glyphs || !name || !*name) return false;
    uint8_t page = hud->notice_page;
    unsigned pixels = render_notice(hud->notice_tiles[page], font, name);
    unsigned width = (pixels + 7) >> 3;
    if (width < 2) width = 2;
    if (width > 15) width = 15;
    /* sub_08040898 clears the whole context-B window to colour 1 before
     * the glyphs are written, including the padding of the final tile. */
    for (unsigned y = 0; y < 8; ++y)
        for (unsigned x = pixels; x < width * 8; ++x)
            notice_pixel(hud->notice_tiles[page], x, y, 1);

    clear_notice_map(hud);
    int x = kind == AOS_HUD_NOTICE_ENEMY ? 26 - (int)width : 0;
    store(hud, x++, 18, 0xE220);
    store(hud, x++, 18, 0xE221);
    store(hud, x++, 18, 0xE222);
    for (unsigned i = 2; i < width; ++i) store(hud, x++, 18, 0xE223);
    store(hud, x++, 18, 0xE622);
    store(hud, x++, 18, 0xE621);
    store(hud, x, 18, 0xE620);

    x = kind == AOS_HUD_NOTICE_ENEMY ? 28 - (int)width : 0;
    store(hud, x++, 19, 0xE230);
    store(hud, x++, 19, 0xE231);
    for (unsigned i = 0; i < width; ++i)
        store(hud, x++, 19, (uint16_t)(0xE280 + page * 0x20 + i));
    store(hud, x++, 19, 0xE631);
    store(hud, x, 19, 0xE630);

    x = kind == AOS_HUD_NOTICE_ENEMY ? 26 - (int)width : 0;
    store(hud, x++, 20, 0xE240);
    store(hud, x++, 20, 0xE241);
    store(hud, x++, 20, 0xE242);
    for (unsigned i = 2; i < width; ++i) store(hud, x++, 20, 0xE243);
    store(hud, x++, 20, 0xE642);
    store(hud, x++, 20, 0xE641);
    store(hud, x, 20, 0xE640);

    hud->notice_timer = 0xF0;
    hud->notice_active = true;
    hud->notice_kind = (uint8_t)kind;
    hud->notice_page ^= 1;
    hud->revision++;
    return true;
}

bool aos_hud_show_enemy(AosHud *hud, const AosHudFont *font, const char *name) {
    return show_notice(hud, font, name, AOS_HUD_NOTICE_ENEMY);
}

bool aos_hud_show_item(AosHud *hud, const AosHudFont *font, const char *name) {
    return show_notice(hud, font, name, AOS_HUD_NOTICE_ITEM);
}

void aos_hud_tick(AosHud *hud) {
    if (!hud || !hud->notice_active || !hud->notice_timer) return;
    if (--hud->notice_timer) return;
    clear_notice_map(hud);
    hud->notice_active = false;
    hud->revision++;
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
    const uint8_t *source = tiles + (tile - 0x200) * 32;
    if (hud->notice_active && tile >= 0x280 && tile < 0x2C0) {
        unsigned page = (tile - 0x280) / 0x20;
        source = hud->notice_tiles[page] + (tile & 0x1F) * 32;
    }
    uint8_t byte = source[py * 4 + px / 2];
    unsigned index = px & 1 ? byte >> 4 : byte & 0xF;
    if (!index) return false;
    unsigned row = entry >> 12;
    size_t palette_offset = row == 0xE && hud->notice_active
                                ? 64 + (size_t)hud->notice_kind * 32
                                : row == 0xE ? 32 : 0;
    *color = (uint16_t)(palette[palette_offset + index * 2] |
                        palette[palette_offset + index * 2 + 1] << 8);
    return true;
}
