/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/menus/aos_quick_map.h"

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

static void write_u32(uint8_t *bytes, size_t offset, uint32_t value) {
    bytes[offset] = (uint8_t)value;
    bytes[offset + 1] = (uint8_t)(value >> 8);
    bytes[offset + 2] = (uint8_t)(value >> 16);
    bytes[offset + 3] = (uint8_t)(value >> 24);
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

    /* sub_08012154 + sub_08012048: a wall break marks the secret bit.
     * Direction 0: cell = map origin + room pixel >> 8. */
    AosQuickMap marked = {0};
    aos_quick_map_secret_mark(&marked, 20, 8, 4 * 256, 2 * 256, 0);
    assert(marked.secret_rooms == (uint16_t)(1u << 1));
    /* Direction 1 mirrors the cell Y, reaching the negative entries. */
    aos_quick_map_secret_mark(&marked, 25, 7, 2 * 256, 2 * 256, 1);
    assert(marked.secret_rooms == (uint16_t)((1u << 1) | (1u << 0)));
    /* Direction 2 mirrors Y and steps X. */
    aos_quick_map_secret_mark(&marked, 13, 20, 2 * 256, 2 * 256, 2);
    assert(marked.secret_rooms ==
           (uint16_t)((1u << 1) | (1u << 0) | (1u << 8)));
    /* Direction 4 steps Y; a non-table cell marks nothing. */
    aos_quick_map_secret_mark(&marked, 0, 0, 0, 0, 4);
    assert(marked.secret_rooms ==
           (uint16_t)((1u << 1) | (1u << 0) | (1u << 8)));

    /* sub_08011430's per-frame discovery needs the area directory and
     * a room descriptor, so it gets a purpose-built ROM: thirteen
     * one-room area tables at 0x40000 and one shared descriptor at
     * 0x50000 whose coordinates word puts the room's map origin at
     * cell (10, 10). */
    enum {
        DIR_ROM_SIZE = 0x520000,
        DIR_TABLE_OFFSET = 0x40000,
        DIR_DESCRIPTOR_OFFSET = 0x50000,
        GBA_BASE = 0x08000000,
    };
    uint8_t *dir_bytes = calloc(1, DIR_ROM_SIZE);
    assert(dir_bytes);
    dir_bytes[PALETTE_OFFSET + 2] = 1; /* palette resource: one row */
    for (int area = 0; area < FUSION_AOS_AREA_COUNT; ++area)
        write_u32(dir_bytes, 0x50EF08 + (size_t)area * 4,
                  GBA_BASE + DIR_TABLE_OFFSET + (uint32_t)area * 4);
    for (int area = 0; area < FUSION_AOS_AREA_COUNT; ++area)
        write_u32(dir_bytes, DIR_TABLE_OFFSET + (size_t)area * 4,
                  GBA_BASE + DIR_DESCRIPTOR_OFFSET);
    /* Descriptor: selector 0xFFFF terminates the variant chain,
     * transitions_pointer == self gives an empty transition table,
     * coordinates word = map_x 10 | map_y 10 << 7. */
    write_u16(dir_bytes, DIR_DESCRIPTOR_OFFSET + 2, 0xFFFF);
    write_u32(dir_bytes, DIR_DESCRIPTOR_OFFSET + 24,
              GBA_BASE + DIR_DESCRIPTOR_OFFSET);
    write_u16(dir_bytes, DIR_DESCRIPTOR_OFFSET + 34, 10 | (10 << 7));
    FusionRom dir_rom = {dir_bytes, DIR_ROM_SIZE, FUSION_ASSET_GAME_ARIA};
    AosQuickMap discovered = {0};
    assert(aos_quick_map_init(&discovered, &dir_rom, error, sizeof error));
    /* A normal PLAY frame marks the cell under Soma and recenters
     * the cursor like sub_08010FF4. */
    assert(aos_quick_map_player_visit(&discovered, &dir_rom, 0, 0,
                                      0, 0, 100, false,
                                      error, sizeof error));
    assert(discovered.explored[10 * AOS_QUICK_MAP_WIDTH + 10]);
    assert(discovered.current_x == 10 && discovered.current_y == 10 &&
           discovered.has_current);
    /* Player 300 px into the room crosses into cell x 11. */
    memset(discovered.explored, 0, sizeof discovered.explored);
    discovered.has_current = false;
    assert(aos_quick_map_player_visit(&discovered, &dir_rom, 0, 0,
                                      300, 0, 100, false,
                                      error, sizeof error));
    assert(discovered.explored[10 * AOS_QUICK_MAP_WIDTH + 11]);
    /* The native guards: screen x >= 240, unk_4CC_0 boss mode and an
     * area-11 cell all skip the mark while reporting success. */
    memset(discovered.explored, 0, sizeof discovered.explored);
    discovered.has_current = false;
    discovered.castle[10 * AOS_QUICK_MAP_WIDTH + 10] = (uint16_t)(11 << 6);
    assert(aos_quick_map_player_visit(&discovered, &dir_rom, 0, 0,
                                      0, 0, 100, false,
                                      error, sizeof error));
    assert(!discovered.explored[10 * AOS_QUICK_MAP_WIDTH + 10]);
    discovered.castle[10 * AOS_QUICK_MAP_WIDTH + 10] = 0;
    assert(aos_quick_map_player_visit(&discovered, &dir_rom, 0, 0,
                                      0, 0, 240, false,
                                      error, sizeof error));
    assert(!discovered.explored[10 * AOS_QUICK_MAP_WIDTH + 10]);
    assert(aos_quick_map_player_visit(&discovered, &dir_rom, 0, 0,
                                      0, 0, 100, true,
                                      error, sizeof error));
    assert(!discovered.explored[10 * AOS_QUICK_MAP_WIDTH + 10]);
    /* An unresolvable room still reports failure. */
    assert(!aos_quick_map_player_visit(&discovered, &dir_rom, 0, 1,
                                       0, 0, 100, false,
                                       error, sizeof error));
    free(dir_bytes);

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
