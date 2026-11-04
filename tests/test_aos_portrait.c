/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_portrait.h"

#include <assert.h>
#include <string.h>

static void test_table(void) {
    static const uint32_t expected[AOS_PORTRAIT_COUNT] = {
        0x0815E03C, 0x0815E04C, 0x0815E054, 0x0815E05C,
        0x0815E074, 0x0815E07C, 0x0815E06C, 0x0815E044,
    };
    for (uint8_t id = 0; id < AOS_PORTRAIT_COUNT; ++id)
        assert(aos_portrait_graphics_pointer(id) == expected[id]);
    assert(aos_portrait_graphics_pointer(AOS_PORTRAIT_COUNT) == 0);
}

static void test_rejects_invalid_input(void) {
    AosPortraitImage image;
    char error[64] = {0};
    FusionRom rom = {.game = FUSION_ASSET_GAME_ARIA};
    assert(!aos_portrait_decode(&rom, 0, &image, error, sizeof error));
    assert(error[0] != '\0');
    memset(error, 0, sizeof error);
    assert(!aos_portrait_decode(&rom, AOS_PORTRAIT_COUNT, &image,
                                error, sizeof error));
    assert(error[0] != '\0');
}

int main(void) {
    test_table();
    test_rejects_invalid_input();
    return 0;
}
