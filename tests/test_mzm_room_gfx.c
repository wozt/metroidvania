/* SPDX-License-Identifier: GPL-3.0-only */
/* The native room backgrounds (mzm_room_gfx) on the exported ROM bank:
 * the room entries, the BG control registers, the animated tiles and
 * palettes, the block layers and the liquid BG0. Skipped without the bank. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "mzm_rom.h"
#include "mzm_room_gfx.h"

static MzmRoomGfx g;

static uint16_t entry_at(int bg, int tx, int ty) {
    /* 512x256 map: the right half in the next screen block. */
    uint32_t at = (uint32_t)bg * 0x1000 + (tx & 32 ? 0x800u : 0) + (uint32_t)((tx & 31) + (ty & 31) * 32) * 2;
    return (uint16_t)(g.ppu.vram[at] | g.ppu.vram[at + 1] << 8);
}

int main(void) {
    if (!mzm_rom_open("assets/extracted/metroid/rom/rom_bank.bin",
                      "assets/extracted/metroid/rom/rom_symbols.tsv")) {
        printf("mzm_room_gfx: no ROM bank, skipped\n");
        return 0;
    }
    assert(!mzm_room_gfx_load(&g, 9, 0, false, false, false));
    /* Norfair room 22: RLE BG0/BG1/BG2, LZ77 BG3, lava at block 21,
     * transparency 0x10 (additive BG0 at 13/16). */
    assert(mzm_room_gfx_load(&g, 2, 22, false, false, false));
    assert(g.tileset == 16 && g.bg_prop[0] == 0x10 && g.bg_prop[3] == 0x40 && g.visual_effect == 2);
    assert(g.effect_y == 21 * 64 && g.transparency == 0x10 && g.eva == 13 && g.evb == 16);
    /* CREATE_BGCNT(1, 0/2/4, prio, 512x256) and BG3 at char 2, screen 6. */
    assert(g.ppu.io[4] == (0x4000 | 0 << 8 | 1 << 2 | 0) && g.ppu.io[5] == (0x4000 | 2 << 8 | 1 << 2 | 1));
    assert(g.ppu.io[6] == (0x4000 | 4 << 8 | 1 << 2 | 2) && (g.ppu.io[7] & 0x1FFF) == (6 << 8 | 2 << 2 | 3));
    assert(mzm_room_gfx_priority(&g, 0) == 0 && mzm_room_gfx_priority(&g, 3) == 3);
    /* The backdrop is black and the tileset palette fills rows 3 to 15. */
    assert(g.ppu.palette[0] == 0 && g.ppu.palette[3 * 16 + 1] != 0);
    /* The animated tiles and the animated palette change over time. */
    static uint8_t tiles[0x800];
    memcpy(tiles, g.ppu.vram + 0x4000, sizeof tiles);
    uint16_t palette[16];
    memcpy(palette, &g.ppu.palette[0xF0], sizeof palette);
    for (int i = 0; i < 120; ++i) mzm_room_gfx_animate(&g, false, false, false);
    assert(memcmp(tiles, g.ppu.vram + 0x4000, sizeof tiles) != 0);
    bool palette_changed = memcmp(palette, &g.ppu.palette[0xF0], sizeof palette) != 0;
    assert(palette_changed || g.animated_palette == 0);
    /* Nothing moves while movement is prevented. */
    memcpy(tiles, g.ppu.vram + 0x4000, sizeof tiles);
    for (int i = 0; i < 60; ++i) mzm_room_gfx_animate(&g, true, false, false);
    assert(!memcmp(tiles, g.ppu.vram + 0x4000, sizeof tiles));
    /* A block layer: block 0 is the empty 0x40 entries; a tileset block
     * takes its four entries from the tileset's tilemap. */
    uint16_t blocks[4 * 3] = {0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1024};
    mzm_room_gfx_blocks(&g, 1, blocks, 4, 3, 0, 0, 320, 224);
    assert(entry_at(1, 0, 0) == 0x40 && entry_at(1, 1, 1) == 0x40);
    assert(entry_at(1, 2, 0) != 0x40 || entry_at(1, 3, 0) != 0x40 || entry_at(1, 2, 1) != 0x40);
    /* Common block 0 (1024) is the empty 0x40 too; out of the layer, 0x40. */
    assert(entry_at(1, 6, 4) == 0x40 && entry_at(1, 10, 10) == 0x40);
    /* Rendering one layer: transparent where nothing is drawn. */
    static uint32_t pixels[224][320];
    mzm_room_gfx_render(&g, 1, 320, 224, &pixels[0][0], 320 * 4);
    assert(pixels[0][0] >> 24 == 0);
    /* An RLE BG0 follows BG1 (not a liquid layer). */
    int x, y;
    assert(!mzm_room_gfx_bg0_position(&g, 100, 50, false, &x, &y));
    /* Crateria room 7: an LZ77 water BG0 at block 22 with its wave. */
    assert(mzm_room_gfx_load(&g, 5, 7, false, false, false));
    assert(g.bg_prop[0] == 0x40 && g.visual_effect == 1 && g.water_moving && g.effect_y == 22 * 64);
    assert(mzm_room_gfx_bg0_position(&g, 100, 200, false, &x, &y) && x == 100);
    /* BG0's top row sits at the surface: (effectY - BG1 Y) / 4 plus the
     * wave, upward. */
    assert(y <= -(22 * 64 - 200 * 4) / 4 + 16 && y >= -(22 * 64 - 200 * 4) / 4 - 16);
    int first_y = y;
    bool moved = false;
    for (int i = 0; i < 200 && !moved; ++i) {
        mzm_room_gfx_bg0_position(&g, 100, 200, false, &x, &y);
        moved = y != first_y;
    }
    assert(moved && mzm_room_gfx_effect_y(&g) > 0);
    mzm_rom_close();
    printf("mzm_room_gfx: ok\n");
    return 0;
}
