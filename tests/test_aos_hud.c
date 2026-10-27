/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_hud.h"

#include <assert.h>
#include <stdio.h>

static AosHud hud;

static uint16_t at(int x, int y) { return hud.map[y * AOS_HUD_MAP_SIZE + x]; }

int main(void) {
    /* sub_0800DA50 with full HP and MP: the frame of sub_0800DB78 and two
     * full bars (tiles 0x208 and 0x218) from cell (4, 1). */
    aos_hud_init(&hud, 320, 320, 80, 80);
    assert(at(0, 0) == (0xD000 | 0x209) && at(3, 0) == (0xD000 | 0x20C) &&
           at(0, 4) == (0xD000 | 0x249) && at(12, 0) == (0xD000 | 0x20D) &&
           at(13, 3) == (0xD000 | 0x23E) && at(14, 0) == 0);
    for (int x = 4; x < 12; ++x) {
        assert(at(x, 1) == (0xD000 | 0x208));
        assert(at(x, 2) == (0xD000 | 0x218));
    }
    /* sub_0800F138: 100 of 320 fills (100 * 100 / 320 << 6) / 100 = 19
     * pixels: two full tiles, a 3-pixel tile, then empty ones. */
    aos_hud_bar(&hud, 4, 1, 100, 320, false);
    assert(at(4, 1) == (0xD000 | 0x208) && at(5, 1) == (0xD000 | 0x208) &&
           at(6, 1) == (0xD000 | 0x203) && at(7, 1) == (0xD000 | 0x200) &&
           at(11, 1) == (0xD000 | 0x200));
    /* A tiny positive value still shows one pixel; zero shows none. */
    aos_hud_bar(&hud, 4, 2, 1, 999, true);
    assert(at(4, 2) == (0xD000 | 0x211) && at(5, 2) == (0xD000 | 0x210));
    aos_hud_bar(&hud, 4, 2, 0, 80, true);
    assert(at(4, 2) == (0xD000 | 0x210));

    /* sub_0800DC70: two steps a frame toward the real values. */
    aos_hud_init(&hud, 320, 320, 80, 80);
    aos_hud_update(&hud, 300, 320, 75, 80);
    assert(hud.hp == 318 && hud.mp == 78);
    for (int i = 0; i < 9; ++i) aos_hud_update(&hud, 300, 320, 75, 80);
    assert(hud.hp == 300 && hud.mp == 75);
    aos_hud_update(&hud, 300, 332, 75, 80);
    assert(hud.max_hp == 322);

    /* Pixels: BG0 scrolled 6 pixels up, palette 13, index 0 transparent. */
    static uint8_t tiles[AOS_HUD_TILE_BYTES];
    uint8_t palette[32] = {0};
    palette[2 * 3] = 0x1F;                      /* colour 3: red */
    tiles[(0x209 - 0x200) * 32 + 6 * 4] = 0x03; /* tile 0x209, row 6, pixel 0 */
    aos_hud_init(&hud, 1, 1, 1, 1);
    uint16_t color;
    assert(aos_hud_pixel(&hud, tiles, palette, 0, 0, &color) && color == 0x001F);
    assert(!aos_hud_pixel(&hud, tiles, palette, 1, 0, &color));
    assert(!aos_hud_pixel(&hud, tiles, palette, 200, 100, &color));
    puts("Aria HUD tests passed");
    return 0;
}
