/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_quick_map.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_SIZE = 0x119000,
    GRAPHICS_OFFSET = 0x0E3664,
    CASTLE_OFFSET = 0x116650,
    CONNECTIONS_OFFSET = 0x117DD0,
    PALETTE_OFFSET = 0x1183CC,
};

static void write_u16(uint8_t *bytes, size_t offset, uint16_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
}

static void write_ppm(const char *path, const uint8_t *rgba) {
    FILE *out = fopen(path, "wb");
    assert(out);
    fputs("P6\n240 160\n255\n", out);
    for (size_t pixel = 0; pixel < 240 * 160; ++pixel)
        assert(fwrite(rgba + pixel * 4, 1, 3, out) == 3);
    assert(fclose(out) == 0);
}

int main(int argc, char **argv) {
    uint8_t *bytes = calloc(1, ROM_SIZE);
    uint8_t *rgba = malloc(240 * 160 * 4);
    assert(bytes && rgba);
    memset(bytes + CASTLE_OFFSET, 0xFF, AOS_QUICK_MAP_CELLS * 2);
    write_u16(bytes, CASTLE_OFFSET + (10 * AOS_QUICK_MAP_WIDTH + 10) * 2, 0);
    bytes[PALETTE_OFFSET] = 0;
    bytes[PALETTE_OFFSET + 2] = 16;
    bytes[PALETTE_OFFSET + 3] = 0;
    write_u16(bytes, PALETTE_OFFSET + 4, 0x0000);
    write_u16(bytes, PALETTE_OFFSET + 4 + 4 * 2, 0x001F);
    write_u16(bytes, PALETTE_OFFSET + 4 + 5 * 2, 0x03E0);
    bytes[GRAPHICS_OFFSET] = 0x55;
    bytes[CONNECTIONS_OFFSET] = 0;
    FusionRom rom = {bytes, ROM_SIZE, FUSION_ASSET_GAME_ARIA};
    AosQuickMap map;
    char error[160];
    assert(aos_quick_map_init(&map, &rom, error, sizeof error));
    assert(map.vram[0x6000] == 0x55 && map.castle[10 * 64 + 10] == 0);
    assert(aos_quick_map_render(&map, true, false, false, 0,
                                rgba, 240 * 160 * 4));
    bool colored = false;
    for (size_t pixel = 0; pixel < 240 * 160; ++pixel)
        colored |= rgba[pixel * 4] || rgba[pixel * 4 + 1] || rgba[pixel * 4 + 2];
    assert(colored);
    free(rgba);
    free(bytes);

    /* Schema-v13 persistence: pack the explored cells and restore them. */
    memset(map.explored, 0, sizeof map.explored);
    map.explored[0] = 1;
    map.explored[7] = 1;
    map.explored[AOS_QUICK_MAP_CELLS - 1] = 1;
    uint8_t packed[AOS_QUICK_MAP_PACKED_BYTES];
    aos_quick_map_pack_explored(&map, packed);
    assert(packed[0] == 0x81 && packed[AOS_QUICK_MAP_PACKED_BYTES - 1] == 0x80);
    AosQuickMap restored = {0};
    aos_quick_map_unpack_explored(&restored, packed);
    assert(restored.explored[0] == 1 && restored.explored[7] == 1 &&
           restored.explored[AOS_QUICK_MAP_CELLS - 1] == 1 &&
           restored.explored[1] == 0 && restored.explored[100] == 0);
    memset(packed, 0, sizeof packed);
    aos_quick_map_unpack_explored(&restored, packed);
    assert(!restored.explored[0] && !restored.explored[AOS_QUICK_MAP_CELLS - 1]);

    /* sUnk_081183B0: cell (24,10) is entry 1, (47,20) entry 7; the
     * negative-coordinate entries never match a map cell. */
    assert(aos_quick_map_secret_index(24, 10) == 1);
    assert(aos_quick_map_secret_index(47, 20) == 7);
    assert(aos_quick_map_secret_index(13, 14) == 4);
    assert(aos_quick_map_secret_index(0, 0) < 0);
    assert(aos_quick_map_secret_index(24, -10) < 0);
    AosQuickMap secrets = {0};
    assert(!aos_quick_map_secret_discovered(&secrets, 24, 10));
    secrets.secret_rooms = (uint16_t)(1u << 1);
    assert(aos_quick_map_secret_discovered(&secrets, 24, 10));
    assert(!aos_quick_map_secret_discovered(&secrets, 47, 20));

    if (argc >= 2) {
        FusionRom local = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &local,
                               error, sizeof error));
        assert(aos_quick_map_init(&map, &local, error, sizeof error));
        assert(aos_quick_map_visit(&map, &local, 0, 3, 640, 191,
                                   error, sizeof error));
        rgba = malloc(240 * 160 * 4);
        assert(rgba);
        assert(aos_quick_map_render(&map, true, true, true, 24,
                                    rgba, 240 * 160 * 4));
        if (argc >= 3) write_ppm(argv[2], rgba);
        free(rgba);
        fusion_rom_close(&local);
    }
    puts("Aria quick-map tests passed");
    return 0;
}
