/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_notice.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static AosSoulNotice notice;

int main(int argc, char **argv) {
    AosIntroTextTiles rendered = {0};
    rendered.data[0] = 0x0F;
    aos_soul_notice_build(&notice, &rendered, 5, true);
    int x = 15 - (5 + 4) / 2;
    assert(notice.map[3 * 32 + x] == 0xE220);
    assert(notice.map[4 * 32 + x + 2] == 0xE280);
    assert(notice.map[5 * 32 + x] == 0xE240);
    assert(notice.map[6 * 32] == 0xE2B0);
    assert(notice.map[7 * 32] == 0xE2E0);
    assert(notice.map[10 * 32] == 0xE2B1);

    memset(notice.static_tiles, 0, sizeof notice.static_tiles);
    notice.static_tiles[(0x220 - 0x200) * 32] = 1;
    notice.palette[(14 * 16 + 1) * 2] = 0x1F;
    uint16_t color = 0;
    assert(aos_soul_notice_pixel(&notice, 0, (unsigned)x * 8, 18, &color));
    assert(color == 0x001F);

    if (argc == 2) {
        FusionRom rom = {0};
        char error[128];
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &rom,
                               error, sizeof error));
        assert(aos_soul_notice_init(&notice, &rom, error, sizeof error));
        aos_soul_notice_build(&notice, &rendered, 5, true);
        bool frame_visible = false;
        for (unsigned y = 0; y < 160; ++y)
            for (unsigned px = 0; px < 240; ++px)
                frame_visible |= aos_soul_notice_pixel(&notice, 0, px, y, &color);
        assert(frame_visible);
        fusion_rom_close(&rom);
    }
    puts("Aria soul notice tests passed");
    return 0;
}
