/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_pause_graphics.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_SIZE = 0x510000,
    PALETTE_OFFSET = 0x0E5C20,
    GRAPHICS_OFFSET = 0x15E02C,
    METADATA_OFFSET = 0x15E940,
    LANGUAGE_OFFSET = 0x27B200,
    COMPRESSED_GRAPHICS = 0x1000,
    COMPRESSED_LANGUAGE = 0x4000,
    BLOCKS_OFFSET = 0x8000,
    MAP_OFFSET = 0x8100,
    GRAPHICS_SIZE = 0x2000,
    TEXT_SCRIPT_OFFSET = 0x500000,
    TEXT_TABLE_OFFSET = 0x506B38,
    FONT_METRICS_OFFSET = 0x0E2B4C,
    FONT_GLYPHS_OFFSET = 0x50A136,
    SMALL_FONT_METRICS_OFFSET = 0x0E2EC0,
    SMALL_FONT_GLYPHS_OFFSET = 0x509876,
    TEXT_ENTRIES = 0x3C4 * 3,
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

static void write_literal_lz77(uint8_t *destination,
                               const uint8_t *source, size_t size) {
    destination[0] = 0x10;
    destination[1] = (uint8_t)size;
    destination[2] = (uint8_t)(size >> 8);
    destination[3] = (uint8_t)(size >> 16);
    size_t input = 0, output = 4;
    while (input < size) {
        destination[output++] = 0;
        size_t count = size - input < 8 ? size - input : 8;
        memcpy(destination + output, source + input, count);
        input += count;
        output += count;
    }
}

static void graphics_descriptor(uint8_t *bytes, size_t offset,
                                size_t compressed) {
    bytes[offset] = 1;
    bytes[offset + 1] = 4;
    bytes[offset + 2] = 0x10;
    bytes[offset + 3] = 0x10;
    write_u32(bytes, offset + 4, 0x08000000u + (uint32_t)compressed);
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
    uint8_t *decoded = calloc(1, GRAPHICS_SIZE + 4);
    uint8_t *rgba = malloc(240 * 160 * 4);
    assert(bytes && decoded && rgba);
    bytes[PALETTE_OFFSET] = 0;
    bytes[PALETTE_OFFSET + 2] = 16;
    bytes[PALETTE_OFFSET + 3] = 0;
    write_u16(bytes, PALETTE_OFFSET + 4 + 1 * 2, 0x03E0);
    write_u16(bytes, PALETTE_OFFSET + 4 + (3 * 16 + 1) * 2, 0x001F);
    write_u16(bytes, PALETTE_OFFSET + 4 + (7 * 16 + 1) * 2, 0x7C00);

    decoded[1] = 4;
    decoded[2] = 0x20;
    memset(decoded + 4 + 32, 0x11, 32);
    write_literal_lz77(bytes + COMPRESSED_GRAPHICS, decoded,
                       GRAPHICS_SIZE + 4);
    memset(decoded + 4, 0, GRAPHICS_SIZE);
    memset(decoded + 4 + 0x40 * 32, 0x11, 7 * 0x10 * 32);
    write_literal_lz77(bytes + COMPRESSED_LANGUAGE, decoded,
                       GRAPHICS_SIZE + 4);
    graphics_descriptor(bytes, GRAPHICS_OFFSET, COMPRESSED_GRAPHICS);
    graphics_descriptor(bytes, LANGUAGE_OFFSET, COMPRESSED_LANGUAGE);

    bytes[TEXT_SCRIPT_OFFSET + 2] = 'A';
    bytes[TEXT_SCRIPT_OFFSET + 3] = 10;
    for (unsigned entry = 0; entry < TEXT_ENTRIES; ++entry)
        write_u32(bytes, TEXT_TABLE_OFFSET + entry * 4,
                  0x08000000u + TEXT_SCRIPT_OFFSET);
    unsigned glyph = 'A' - 0x20;
    bytes[FONT_METRICS_OFFSET + glyph * 4 + 1] = 1;
    bytes[SMALL_FONT_METRICS_OFFSET + glyph * 4 + 1] = 1;
    memset(bytes + FONT_GLYPHS_OFFSET + glyph * 14, 0x80, 12);
    memset(bytes + SMALL_FONT_GLYPHS_OFFSET + glyph * 10, 0x80, 8);

    bytes[METADATA_OFFSET] = 1;
    bytes[METADATA_OFFSET + 1] = 1;
    write_u32(bytes, METADATA_OFFSET + 4, 0x08000000u + BLOCKS_OFFSET);
    write_u32(bytes, METADATA_OFFSET + 12, 0x08000000u + MAP_OFFSET);
    for (unsigned cell = 0; cell < 16; ++cell)
        write_u16(bytes, BLOCKS_OFFSET + cell * 2, 1);
    for (unsigned block = 0; block < 64; ++block)
        write_u16(bytes, MAP_OFFSET + block * 2, 1);

    FusionRom rom = {bytes, ROM_SIZE, FUSION_ASSET_GAME_ARIA};
    AosPauseGraphics graphics;
    char error[160];
    assert(aos_pause_graphics_init(&graphics, &rom, 1, error, sizeof error));
    assert(graphics.background.width_pixels == 256 &&
           graphics.background.height_pixels == 256 &&
           graphics.background.unresolved_count == 0);
    AosPauseHubValues values = {
        .hp = 320, .mp = 80, .max_hp = 320, .max_mp = 80,
        .stats = {10, 12, 11, 9}, .attack = 15, .defence = 11,
        .level = 1, .experience = 84, .next_experience = 0,
        .play_frames = 60 * 3723,
    };
    assert(aos_pause_graphics_render_hub(&graphics, &values, 0,
                                         rgba, 240 * 160 * 4));
    assert((graphics.bg0_map[12 * 32 + 2] & 0xF000) == 0x3000);
    assert((graphics.bg0_map[13 * 32 + 2] & 0xF000) == 0x7000);
    assert(graphics.bg0_map[2 * 32 + 14] == 0x3300);

    AosPauseSoulSetValues soul = {
        .stats = {11, 12, 13, 14},
        .attack = 15,
        .defence = 16,
        .category = 0,
        .cursor = {1, 0, 0},
        .equipped = {1, 0, 0},
    };
    soul.counts[0][0] = 2;
    assert(aos_pause_graphics_render_soul_set(&graphics, &soul,
                                               rgba, 240 * 160 * 4));
    assert((graphics.bg0_map[3 * 32 + 6] & 0xF000) == 0x8000);
    assert((graphics.bg0_map[5 * 32 + 6] & 0xF000) == 0x3000);
    assert(graphics.bg0_map[2 * 32 + 22] == 0x3305);
    assert((graphics.bg0_map[10 * 32 + 15] & 0x3FF) < 0x200);
    assert((graphics.bg0_map[12 * 32] & 0x3FF) < 0x200);
    assert((graphics.bg0_map[15 * 32 + 7] & 0x3FF) < 0x200);

    AosPauseEquipValues equip = {
        .current = {15, 16, 11, 12, 13, 14},
        .preview = {18, 17, 12, 12, 13, 14},
        .equipped = {0, 0, 25},
        .items = {AOS_PAUSE_EQUIP_NONE, 0},
        .quantities = {0, 2},
        .slot = 0,
        .item_count = 2,
        .selected_item = 0,
        .preview_active = true,
    };
    assert(aos_pause_graphics_render_equip(&graphics, &equip,
                                            rgba, 240 * 160 * 4));
    assert(graphics.bg0_map[3 * 32 + 2] == 0x33D0);
    assert(graphics.bg0_map[2 * 32 + 22] == 0x3305);
    assert((graphics.bg0_map[2 * 32 + 24] & 0x3FF) == 0x34A);
    assert((graphics.bg0_map[8 * 32 + 15] & 0x3FF) < 0x200);
    assert((graphics.bg0_map[10 * 32] & 0x3FF) < 0x200);
    assert((graphics.bg0_map[16 * 32 + 5] & 0x3FF) < 0x200);
    assert(graphics.bg0_chars[0x340 * 32] == 0);
    /* sub_0804C264: rows 4,6,8,10 show actions {0,1,3,2}; the name strip
     * is 0x32A0+button_index*3 in column 0x12. */
    {
        uint8_t config[AOS_BUTTON_ACTIONS] = {1, 0, 3, 2};
        assert(aos_pause_graphics_render_config(&graphics, config,
                                                rgba, 240 * 160 * 4));
        assert(graphics.bg0_map[4 * 32 + 6] == 0x3359);
        assert(graphics.bg0_map[4 * 32 + 12] == 0x335F);
        assert(graphics.bg0_map[4 * 32 + 18] == 0x32A3);
        assert(graphics.bg0_map[4 * 32 + 20] == 0x32A5);
        assert(graphics.bg0_map[6 * 32 + 11] == 0x336E);
        assert(graphics.bg0_map[6 * 32 + 18] == 0x32A0);
        assert(graphics.bg0_map[8 * 32 + 18] == 0x32A6);
        assert(graphics.bg0_map[10 * 32 + 18] == 0x32A9);
    }
    assert(aos_pause_graphics_render_hub(&graphics, &values, 0,
                                         rgba, 240 * 160 * 4));
    assert(memcmp(graphics.bg0_chars + 0x340 * 32,
                  graphics.bg0_base_chars + 0x340 * 32, 32) == 0);
    aos_pause_graphics_free(&graphics);
    free(rgba);
    free(decoded);
    free(bytes);

    if (argc >= 2) {
        FusionRom local = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &local,
                               error, sizeof error));
        assert(aos_pause_graphics_init(&graphics, &local, 1,
                                       error, sizeof error));
        rgba = malloc(240 * 160 * 4);
        assert(rgba);
        assert(aos_pause_graphics_render_hub(&graphics, &values, 0,
                                             rgba, 240 * 160 * 4));
        if (argc >= 3) write_ppm(argv[2], rgba);
        static const unsigned soul_records[] = {55, 24, 35};
        memset(soul.counts, 1, sizeof soul.counts);
        for (unsigned category = 0; category < 3; ++category) {
            soul.category = (uint8_t)category;
            for (unsigned cursor = 0; cursor <= soul_records[category]; ++cursor) {
                soul.cursor[category] = (uint8_t)cursor;
                if (!aos_pause_graphics_render_soul_set(
                        &graphics, &soul, rgba, 240 * 160 * 4)) {
                    fprintf(stderr, "Soul Set render failed for category %u cursor %u\n",
                            category, cursor);
                    assert(false);
                }
            }
        }
        if (argc >= 4) {
            soul.category = 0;
            soul.cursor[0] = 1;
            assert(aos_pause_graphics_render_soul_set(&graphics, &soul,
                                                       rgba, 240 * 160 * 4));
            write_ppm(argv[3], rgba);
        }
        static const unsigned first_item[] = {0, 0, 25};
        static const unsigned item_count[] = {59, 25, 20};
        equip.item_count = 1;
        equip.preview_active = true;
        for (unsigned slot = 0; slot < 3; ++slot) {
            equip.slot = (uint8_t)slot;
            for (unsigned index = 0; index < item_count[slot]; ++index) {
                unsigned item = first_item[slot] + index;
                equip.items[0] = (uint8_t)item;
                equip.selected_item = (uint8_t)item;
                equip.equipped[slot] = (uint8_t)item;
                if (!aos_pause_graphics_render_equip(
                        &graphics, &equip, rgba, 240 * 160 * 4)) {
                    fprintf(stderr, "Equip render failed for slot %u item %u\n",
                            slot, item);
                    assert(false);
                }
            }
        }
        if (argc >= 5) {
            equip.slot = 0;
            equip.items[0] = 0;
            equip.selected_item = 0;
            assert(aos_pause_graphics_render_equip(&graphics, &equip,
                                                    rgba, 240 * 160 * 4));
            write_ppm(argv[4], rgba);
        }
        assert(aos_pause_graphics_render_sleep(&graphics, false,
                                               rgba, 240 * 160 * 4));
        assert(aos_pause_graphics_render_sleep(&graphics, true,
                                               rgba, 240 * 160 * 4));
        /* sub_0804BA18/sub_0804BB40: window fill, strips, ON/OFF pairs and
         * names. Ability 0 owned+enabled, 1 owned+disabled, 2 unowned. */
        {
            AosPauseAbilityValues ability = {.cursor = 0};
            ability.owned[0] = 1;
            ability.enabled[0] = 1;
            ability.owned[1] = 1;
            ability.owned[3] = 1;
            ability.enabled[3] = 1;
            assert(aos_pause_graphics_render_ability(&graphics, &ability,
                                                     rgba, 240 * 160 * 4));
            /* Window fill below the text band: cell (17*32+10) is
             * i=4,j=6 -> 0x3200+4*32+6 = 0x3286; (18*32+26) -> 0x32B6. */
            assert(graphics.bg0_map[17 * 32 + 10] == 0x3286);
            assert(graphics.bg0_map[18 * 32 + 26] == 0x32B6);
            /* Strips above the name rows (text occupies tile rows
             * 12-14): i=0 row 4 -> 0x3380/0x3390, i=3 row 10 ->
             * 0x33E0/0x33F0. */
            assert(graphics.bg0_map[4 * 32 + 3] == 0x3380);
            assert(graphics.bg0_map[4 * 32 + 12] == 0x3389);
            assert(graphics.bg0_map[4 * 32 + 17] == 0x3390);
            assert(graphics.bg0_map[10 * 32 + 3] == 0x33E0);
            assert(graphics.bg0_map[10 * 32 + 17] == 0x33F0);
            /* ON/OFF pairs: ability 0 even -> x 0xD row 4 tile 0xB31E;
             * ability 1 odd -> x 0x1B row 4 tile 0xB30E; ability 3 odd ->
             * x 0x1B row 6 tile 0xB31E. */
            assert(graphics.bg0_map[4 * 32 + 0xD] == 0xB31E);
            assert(graphics.bg0_map[4 * 32 + 0xE] == 0xB31F);
            assert(graphics.bg0_map[4 * 32 + 0x1B] == 0xB30E);
            assert(graphics.bg0_map[6 * 32 + 0x1B] == 0xB31E);
            /* Unowned ability 2 writes no marker. */
            assert(graphics.bg0_map[4 * 32 + 0xD] != 0xB30E);
        }
        /* sub_0804B6C4/sub_0804B494: slot strips rows 11-14, bottom bars
         * rows 16-18, dashes for empty slots, quantity digits. */
        {
            AosPauseItemUseValues items = {.selected = 0xFF, .top = 0};
            items.counts[0] = 3;   /* Potion x3 */
            items.counts[5] = 1;   /* Mind Up x1 */
            items.counts[9] = 12;  /* Cream Soda x12 */
            assert(aos_pause_graphics_render_item_use(&graphics, &items,
                                                      rgba, 240 * 160 * 4));
            /* Strips: left col x=2 row 11 entry 0x3380; right col x=0x11
             * row 14 entry 0x3390+3*0x20 = 0x33F0. */
            assert(graphics.bg0_map[11 * 32 + 2] == 0x3380);
            assert(graphics.bg0_map[14 * 32 + 0x11] == 0x33F0);
            /* Bottom bars: row 16 col 5 = 0x3200, row 18 col 28 = 0x3240+23
             * = 0x3257. */
            assert(graphics.bg0_map[16 * 32 + 5] == 0x3200);
            assert(graphics.bg0_map[18 * 32 + 28] == 0x3257);
            /* Slot 0 (item 0, qty 3): name text overwrote cells near
             * (0,96); quantity digit '3' = 0x3303 at col 0xD row 0xB. */
            assert(graphics.bg0_map[0xB * 32 + 0xD] == 0x3303);
            /* Slot 1 (item 5, qty 1): odd column, digit at col 0x1C row
             * 0xB = 0x3301. */
            assert(graphics.bg0_map[0xB * 32 + 0x1C] == 0x3301);
            /* Slot 2 (item 9, qty 12): two digits 1,2 ending col 0xD ->
             * col 0xC = 0x3301, col 0xD = 0x3302. */
            assert(graphics.bg0_map[0xC * 32 + 0xC] == 0x3301);
            assert(graphics.bg0_map[0xC * 32 + 0xD] == 0x3302);
            /* Slots 3-7 empty: slot 3 (odd) dashes at cols 0x1B/0x1C
             * row 0xC; slot 4 (even) at cols 0xC/0xD row 0xD. */
            assert(graphics.bg0_map[0xC * 32 + 0x1B] == 0x330D);
            assert(graphics.bg0_map[0xC * 32 + 0x1C] == 0x330D);
            assert(graphics.bg0_map[0xD * 32 + 0xC] == 0x330D);
            assert(graphics.bg0_map[0xD * 32 + 0xD] == 0x330D);
            /* Description of selected item 0 = text 0x15C on the bars. */
            items.selected = 0;
            assert(aos_pause_graphics_render_item_use(&graphics, &items,
                                                      rgba, 240 * 160 * 4));
        }
        /* sub_0804D2C8 list: strips, names gated by defeated bits, big
         * enemy numbers. Enemies 0 and 11 defeated on page 0/10. */
        {
            AosPauseEnemyValues enemy = {.top = 0, .cursor = 0};
            enemy.defeated[0] = 1u | (1u << 11); /* enemies 0 and 11 */
            assert(aos_pause_graphics_render_enemy(&graphics, &enemy,
                                                   rgba, 240 * 160 * 4));
            /* Slot strips at columns 6/0x14 rows 5+2*(slot/2); the big
             * enemy numbers overlap the strip's left cells, so assert on
             * cells past the number reach (col 25). */
            assert(graphics.bg0_map[5 * 32 + 25] == 0xF335);
            assert(graphics.bg0_map[13 * 32 + 25] == 0xF3B5);
            enemy.top = 10;
            enemy.cursor = 10;
            assert(aos_pause_graphics_render_enemy(&graphics, &enemy,
                                                   rgba, 240 * 160 * 4));
            /* Enemy 11 defeated -> name text written near (0,64);
             * undiscovered enemies render 0x34E instead. */
        }
        free(rgba);
        aos_pause_graphics_free(&graphics);
        fusion_rom_close(&local);
    }
    puts("Aria pause graphics tests passed");
    return 0;
}
