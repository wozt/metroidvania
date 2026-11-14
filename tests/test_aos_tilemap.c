/* SPDX-License-Identifier: GPL-3.0-only */
/* aos_tilemap tests: a synthetic Aria ROM fixture exercises the
 * sub_08001994-equivalent lookup — block-map resolution, in-block
 * flips, the top-two-bits palette XOR, empty blocks, absent and
 * affine layers, compressed (unk_2 & 2) tile payloads, map bounds and
 * the two rebuild span helpers. */
#include "aos/core/aos_tilemap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    GBA_ROM_BASE = 0x08000000,
    AREA_DIRECTORY_OFFSET = 0x50EF08,
    ROOM_DESC_OFFSET = 0x1000,
    BG_DESC_OFFSET = 0x1100,
    META1_OFFSET = 0x1200,
    META3_OFFSET = 0x1280,
    TILES1_OFFSET = 0x1400,
    MAP1_OFFSET = 0x1600,
    LZ77_BG3_OFFSET = 0x1800,
    MAP3_OFFSET = 0x1900,
    ROM_SIZE = AREA_DIRECTORY_OFFSET + FUSION_AOS_AREA_COUNT * 4
};

static void write_u16(uint8_t *data, size_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

static void write_background(uint8_t *data, size_t offset,
                             uint32_t metadata_pointer) {
    write_u32(data, offset + 8, metadata_pointer);
}

static void write_metadata(uint8_t *data, size_t offset,
                           uint8_t width_screens, uint8_t height_screens,
                           uint16_t flags, uint32_t blocks_pointer,
                           uint32_t map_pointer) {
    data[offset] = width_screens;
    data[offset + 1] = height_screens;
    write_u16(data, offset + 2, flags);
    write_u32(data, offset + 4, blocks_pointer);
    write_u32(data, offset + 8, 0);
    write_u32(data, offset + 12, map_pointer);
}

/* Same literal-only LZ77 writer shape as test_fusion_assets_aos.c. */
static size_t write_lz77_literals(uint8_t *data, size_t offset,
                                  const uint8_t *expanded, size_t size) {
    data[offset] = 0x10;
    data[offset + 1] = (uint8_t)size;
    data[offset + 2] = (uint8_t)(size >> 8);
    data[offset + 3] = (uint8_t)(size >> 16);
    size_t input = 0, output = offset + 4;
    while (input < size) {
        data[output++] = 0;
        size_t count = size - input < 8 ? size - input : 8;
        memcpy(data + output, expanded + input, count);
        input += count;
        output += count;
    }
    return output - offset;
}

int main(void) {
    uint8_t *bytes = calloc(1, ROM_SIZE);
    assert(bytes);

    /* Area directory: twelve one-room tables packed against the
     * directory so read_area_directory derives room_count == 1. */
    for (size_t area = 0; area < FUSION_AOS_AREA_COUNT; ++area) {
        size_t table = AREA_DIRECTORY_OFFSET -
                       FUSION_AOS_AREA_COUNT * 4 + area * 4;
        write_u32(bytes, AREA_DIRECTORY_OFFSET + area * 4,
                  GBA_ROM_BASE + (uint32_t)table);
        write_u32(bytes, table, GBA_ROM_BASE + ROOM_DESC_OFFSET);
    }
    /* Room descriptor (0x24 bytes): selector 0xFFFF terminates the
     * variant chain; transitions_pointer == resolved pointer gives an
     * empty transition table. */
    write_u16(bytes, ROOM_DESC_OFFSET + 2, 0xFFFF);
    write_u32(bytes, ROOM_DESC_OFFSET + 8,
              GBA_ROM_BASE + BG_DESC_OFFSET);
    write_u32(bytes, ROOM_DESC_OFFSET + 24,
              GBA_ROM_BASE + ROOM_DESC_OFFSET);
    /* BG1 decoded, BG2 absent (metadata_pointer 0), BG3 LZ77. */
    write_background(bytes, BG_DESC_OFFSET + 0 * 12,
                     GBA_ROM_BASE + META1_OFFSET);
    write_background(bytes, BG_DESC_OFFSET + 1 * 12, 0);
    write_background(bytes, BG_DESC_OFFSET + 2 * 12,
                     GBA_ROM_BASE + META3_OFFSET);
    write_metadata(bytes, META1_OFFSET, 1, 1, 0,
                   GBA_ROM_BASE + TILES1_OFFSET,
                   GBA_ROM_BASE + MAP1_OFFSET);
    write_metadata(bytes, META3_OFFSET, 1, 1, 2,
                   GBA_ROM_BASE + LZ77_BG3_OFFSET,
                   GBA_ROM_BASE + MAP3_OFFSET);

    /* BG1 tile words: cell index i holds 0x2000 + i. */
    for (size_t index = 0; index < 64; ++index)
        write_u16(bytes, TILES1_OFFSET + index * 2,
                  (uint16_t)(0x2000 + index));
    /* BG1 block map (8x8 for a 1x1-screen map): block ids pick
     * (block-1)*16 + lx + ly*4 into the tile array; the top bits flip
     * and carry the palette XOR. */
    write_u16(bytes, MAP1_OFFSET + 0 * 2, 1);        /* block (0,0) */
    write_u16(bytes, MAP1_OFFSET + 1 * 2, 2);        /* block (1,0) */
    write_u16(bytes, MAP1_OFFSET + 8 * 2, 0x4001);   /* (0,1) xflip */
    write_u16(bytes, MAP1_OFFSET + 9 * 2, 0x8002);   /* (1,1) yflip */
    write_u16(bytes, MAP1_OFFSET + 10 * 2, 0xC003);  /* (2,1) both+pal */
    /* entry 11 stays 0: an empty block. */

    /* BG3 compressed payload: 4-byte expanded prefix (first byte 0)
     * then 32 tile words 0x3000 + i. */
    uint8_t expanded3[4 + 64];
    memset(expanded3, 0, sizeof expanded3);
    for (size_t index = 0; index < 32; ++index)
        write_u16(expanded3, 4 + index * 2, (uint16_t)(0x3000 + index));
    write_lz77_literals(bytes, LZ77_BG3_OFFSET, expanded3,
                        sizeof expanded3);
    write_u16(bytes, MAP3_OFFSET + 0 * 2, 1);
    write_u16(bytes, MAP3_OFFSET + 1 * 2, 2);

    FusionRom rom = {bytes, ROM_SIZE, FUSION_ASSET_GAME_ARIA};
    FusionAosAreaDirectory directory;
    char error[160];
    assert(fusion_aos_read_area_directory(&rom, &directory,
                                          error, sizeof error));
    assert(directory.total_rooms == FUSION_AOS_AREA_COUNT);

    AosRoomTilemap tilemap;
    assert(aos_room_tilemap_load(&tilemap, &rom, &directory, 0, 0,
                                 error, sizeof error));
    assert(aos_tilemap_status(&tilemap, 1) == FUSION_AOS_TILEMAP_DECODED);
    assert(aos_tilemap_status(&tilemap, 2) == FUSION_AOS_TILEMAP_ABSENT);
    assert(aos_tilemap_status(&tilemap, 3) == FUSION_AOS_TILEMAP_DECODED);
    assert(aos_tilemap_status(&tilemap, 0) == FUSION_AOS_TILEMAP_ABSENT);
    assert(aos_tilemap_status(&tilemap, 4) == FUSION_AOS_TILEMAP_ABSENT);
    assert(aos_tilemap_status(NULL, 1) == FUSION_AOS_TILEMAP_ABSENT);

    /* Plain cells: block (0,0) is tile-array cells 0..15 row-major. */
    assert(aos_tilemap_lookup(&tilemap, 1, 0, 0) == 0x2000);
    assert(aos_tilemap_lookup(&tilemap, 1, 1, 0) == 0x2001);
    assert(aos_tilemap_lookup(&tilemap, 1, 3, 0) == 0x2003);
    /* Block (1,0) starts at cell (2-1)*16 = 16. */
    assert(aos_tilemap_lookup(&tilemap, 1, 4, 0) == 0x2010);
    assert(aos_tilemap_lookup(&tilemap, 1, 7, 3) == 0x201F);
    /* xflip on block (0,1): tile_x 0 maps to local x 3. */
    assert(aos_tilemap_lookup(&tilemap, 1, 0, 4) == (0x2003 ^ 0x0400));
    assert(aos_tilemap_lookup(&tilemap, 1, 3, 7) == (0x200C ^ 0x0400));
    /* yflip on block (1,1): tile (4,4) -> local (0,3) -> cell 28. */
    assert(aos_tilemap_lookup(&tilemap, 1, 4, 4) == (0x201C ^ 0x0800));
    /* both flips on block (2,1): the 0xC000 bits XOR 0x0C00 into
     * every cell of the block; base cell 32, local (3,3) -> 47. */
    assert(aos_tilemap_lookup(&tilemap, 1, 8, 4) == (0x202F ^ 0x0C00));
    assert(aos_tilemap_lookup(&tilemap, 1, 11, 4) == (0x202C ^ 0x0C00));
    /* Empty block and out-of-map coordinates read as 0. */
    assert(aos_tilemap_lookup(&tilemap, 1, 12, 4) == 0);
    assert(aos_tilemap_lookup(&tilemap, 1, 32, 0) == 0);
    assert(aos_tilemap_lookup(&tilemap, 1, 0, 32) == 0);
    /* Absent and invalid layers. */
    assert(aos_tilemap_lookup(&tilemap, 2, 0, 0) == 0);
    assert(aos_tilemap_lookup(&tilemap, 0, 0, 0) == 0);
    assert(aos_tilemap_lookup(NULL, 1, 0, 0) == 0);
    /* BG3's LZ77 payload decodes like the native unk_2&2 path. */
    assert(aos_tilemap_lookup(&tilemap, 3, 0, 0) == 0x3000);
    assert(aos_tilemap_lookup(&tilemap, 3, 1, 0) == 0x3001);
    assert(aos_tilemap_lookup(&tilemap, 3, 4, 0) == 0x3010);
    assert(aos_tilemap_lookup(&tilemap, 3, 12, 0) == 0);

    /* Rebuild spans: the x-scroll column fixes tile_x and steps
     * tile_y (sub_0803F3F8/3F4C8, native count 0x15); the y-scroll
     * row fixes tile_y and steps tile_x (sub_0803F580/3F650, 0x1F). */
    uint16_t span[8];
    assert(aos_tilemap_column(&tilemap, 1, 0, 0, 4, span, 8) == 4);
    assert(span[0] == 0x2000 && span[1] == 0x2004 &&
           span[2] == 0x2008 && span[3] == 0x200C);
    assert(aos_tilemap_row(&tilemap, 1, 0, 0, 4, span, 8) == 4);
    assert(span[0] == 0x2000 && span[1] == 0x2001 &&
           span[2] == 0x2002 && span[3] == 0x2003);
    /* The cap truncates, a NULL out probes the count, and the native
     * counts fit the staging buffer. */
    assert(aos_tilemap_column(&tilemap, 1, 0, 0, 10, span, 3) == 3);
    assert(aos_tilemap_column(&tilemap, 1, 0, 0, 0x15, NULL, 0) == 0x15);
    assert(aos_tilemap_row(&tilemap, 1, 0, 0, 0x1F, NULL, 0) == 0x1F);
    /* A span crossing the map edge pads with empty-block zeros:
     * tile_x 30..31 sit in block (7,0), which has no entry. */
    assert(aos_tilemap_row(&tilemap, 1, 30, 0, 4, span, 8) == 4);
    assert(span[0] == 0 && span[1] == 0 && span[2] == 0 && span[3] == 0);
    /* Block (1,0)'s row segment: cells 16..19. */
    assert(aos_tilemap_row(&tilemap, 1, 4, 0, 4, span, 8) == 4);
    assert(span[0] == 0x2010 && span[1] == 0x2011 &&
           span[2] == 0x2012 && span[3] == 0x2013);

    /* Load failures propagate; a missing room index is rejected. */
    aos_room_tilemap_free(&tilemap);
    assert(!aos_room_tilemap_load(&tilemap, &rom, &directory, 0, 1,
                                  error, sizeof error));
    assert(!aos_room_tilemap_load(&tilemap, &rom, &directory, 99, 0,
                                  error, sizeof error));

    aos_room_tilemap_free(&tilemap);
    assert(tilemap.layers[0].cells == NULL);
    aos_room_tilemap_free(&tilemap); /* double free is safe */
    free(bytes);
    puts("aos_tilemap tests passed");
    return 0;
}
