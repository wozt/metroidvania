/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos/menus/aos_pause_graphics.h"

#include "aos/game/aos_recent_items.h"

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

    /* sub_08048C74's hand cursor for language 1: the raw (uncompressed)
     * 16 x 16-tile sheet at 0x082052D8, frame descriptor 0x08256EF8 and
     * the OBJ palette block at 0x0820C2C0 + 36 (sixteen colours; the real
     * row's upper half is zero). The real cursor
     * animation cycles 29 poses, so the fixture uses 10 steps — past the
     * old 8-step cap — to prove the decode accepts it. The sheet is all
     * colour index 15 and the palette all white, so every cursor pixel is
     * (255,255,255), a colour nothing else in this fixture produces. */
    {
        const size_t cursor_graphics = 0x2052D8, cursor_tiles = 0x2052DC;
        bytes[cursor_graphics] = 0;      /* raw resource */
        bytes[cursor_graphics + 1] = 4;
        bytes[cursor_graphics + 2] = 0x10;
        bytes[cursor_graphics + 3] = 0x10;
        memset(bytes + cursor_tiles, 0xFF, 0x2000);
        memset(bytes + 0x20C2E4, 0xFF, 32);  /* all sixteen palette colours */
        const size_t descriptor = 0x256EF8;
        write_u16(bytes, descriptor, 1);      /* one frame record */
        write_u16(bytes, descriptor + 2, 1);  /* one animation */
        write_u32(bytes, descriptor + 4, 0x0850C000);  /* records */
        write_u32(bytes, descriptor + 12, 0x0850C200); /* animation table */
        bytes[0x50C005] = 1;                  /* one piece in record 0 */
        write_u32(bytes, 0x50C00C, 0x0850C100); /* components */
        bytes[0x50C106] = 16;                 /* width */
        bytes[0x50C107] = 16;                 /* height */
        write_u32(bytes, 0x50C200, 0x0850C300); /* anim data */
        write_u16(bytes, 0x50C300, 10);       /* ten 4-tick steps */
        write_u16(bytes, 0x50C302, 1);
        for (unsigned step = 0; step < 10; ++step) {
            bytes[0x50C304 + step * 4] = 0;   /* frame 0 */
            bytes[0x50C305 + step * 4] = 4;   /* duration */
        }
    }

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
    /* The hand cursor decoded: one record, a ten-step animation (past the
     * former 8-step cap) and a painted 16 x 16 piece. sub_08048AD8(1, 12)
     * anchors it at pixel (14 + wiggle, 99); tick 1 gives wiggle -3 and a
     * 16 x 16 white piece at x 11..26, y 99..114. */
    assert(graphics.cursor_ready);
    assert(graphics.cursor_sheet.frame_count == 1);
    assert(graphics.cursor_sheet.anim_count == 1);
    assert(graphics.cursor_sheet.anim_length[0] == 10);
    {
        unsigned white = 0;
        for (unsigned y = 95; y <= 118; ++y)
            for (unsigned x = 8; x <= 30; ++x) {
                const uint8_t *p = rgba + ((size_t)y * 240 + x) * 4;
                if (p[0] == 255 && p[1] == 255 && p[2] == 255 && p[3] == 255)
                    ++white;
            }
        assert(white == 256);
    }

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

    /* Patch 1171: entering Soul Set armed sub_08048B0C's press flourish
     * (entity +0x24 state machine) and the first render already consumed
     * the invisible scale-0 frame. The next 15 renders walk the native
     * +0x0D timer 1..15 — sub_0803E058(0, timer<<12, timer<<12, 0, 0,
     * slot) — growing the hand anchored at pixel (0x35,9) before the
     * affine pass ends and fx_timer returns to 0. */
    assert(graphics.fx_timer == 2);
    {
        unsigned white[15];
        for (unsigned k = 0; k < 15; ++k) {
            assert(aos_pause_graphics_render_soul_set(
                &graphics, &soul, rgba, 240 * 160 * 4));
            unsigned count = 0;
            for (unsigned y = 0; y <= 24; ++y)
                for (unsigned x = 40; x <= 70; ++x) {
                    const uint8_t *p = rgba + ((size_t)y * 240 + x) * 4;
                    if (p[0] == 255 && p[1] == 255 && p[2] == 255 &&
                        p[3] == 255)
                        ++count;
                }
            white[k] = count;
        }
        assert(graphics.fx_timer == 0);
        /* The scale ramp grows the sprite: the last frame must paint
         * far more hand pixels than an early near-zero-scale frame. */
        assert(white[14] > white[2]);
        assert(white[14] > 100);
    }
    /* The Soul Set L/R swipe stand-in: a category delta re-arms the
     * flourish with the direction the native 0x40/0x80 input bits pass
     * to sub_080486E0, anchored toward the new category label. */
    soul.category = 1;
    assert(aos_pause_graphics_render_soul_set(&graphics, &soul,
                                              rgba, 240 * 160 * 4));
    assert(graphics.fx_timer == 2);
    assert(graphics.fx_dir == 1);
    for (unsigned k = 0; k < 15; ++k)
        assert(aos_pause_graphics_render_soul_set(
            &graphics, &soul, rgba, 240 * 160 * 4));
    assert(graphics.fx_timer == 0);
    {
        /* Category 1's label strip sits at tile row 5 -> pixel y 40;
         * the swipe anchor ends at (44, 44) on the final frame. */
        unsigned count = 0;
        for (unsigned y = 34; y <= 56; ++y)
            for (unsigned x = 34; x <= 56; ++x) {
                const uint8_t *p = rgba + ((size_t)y * 240 + x) * 4;
                if (p[0] == 255 && p[1] == 255 && p[2] == 255 &&
                    p[3] == 255)
                    ++count;
            }
        assert(count > 100);
    }
    soul.category = 0;
    assert(aos_pause_graphics_render_soul_set(&graphics, &soul,
                                              rgba, 240 * 160 * 4));
    assert(graphics.fx_dir == -1);
    for (unsigned k = 0; k < 15; ++k)
        assert(aos_pause_graphics_render_soul_set(
            &graphics, &soul, rgba, 240 * 160 * 4));
    assert(graphics.fx_timer == 0);

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
        assert(aos_pause_graphics_render_config(&graphics, config, 0,
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
        /* Real-ROM cursor regression: the English descriptor at
         * 0x0827B1F0 decodes to 38 frames and one 29-step animation, the
         * OBJ palette row keeps its native zero upper half (indices 8-15
         * render black, not the next ROM block's colours), and the hub
         * render actually paints the hand next to the first entry. */
        assert(graphics.cursor_ready);
        assert(graphics.cursor_sheet.frame_count == 38);
        assert(graphics.cursor_sheet.anim_count == 1);
        assert(graphics.cursor_sheet.anim_length[0] == 29);
        for (unsigned k = 8; k < 16; ++k)
            assert(graphics.cursor_palette[k] == 0);
        {
            uint8_t *without_cursor = malloc(240 * 160 * 4);
            assert(without_cursor);
            memcpy(without_cursor, rgba, 240 * 160 * 4);
            graphics.cursor_ready = false;
            assert(aos_pause_graphics_render_hub(&graphics, &values, 0,
                                                 rgba, 240 * 160 * 4));
            graphics.cursor_ready = true;
            unsigned cursor_diff = 0;
            int min_x = 240, max_x = -1, min_y = 160, max_y = -1;
            for (unsigned y = 0; y < 160; ++y)
                for (unsigned x = 0; x < 240; ++x) {
                    const uint8_t *a = rgba + ((size_t)y * 240 + x) * 4;
                    const uint8_t *b =
                        without_cursor + ((size_t)y * 240 + x) * 4;
                    if (memcmp(a, b, 4)) {
                        ++cursor_diff;
                        if ((int)x < min_x) min_x = (int)x;
                        if ((int)x > max_x) max_x = (int)x;
                        if ((int)y < min_y) min_y = (int)y;
                        if ((int)y > max_y) max_y = (int)y;
                    }
                }
            /* The two renders only differ by the hand's pose/wiggle, so
             * every changed pixel belongs to the cursor sprite near the
             * first hub entry (sub_08048AD8(1, 12) anchor). */
            assert(cursor_diff > 0);
            assert(min_x >= 0 && min_x < 40);
            assert(max_x < 64 && min_y > 80 && max_y < 140);
            free(without_cursor);
        }
        static const unsigned soul_records[] = {55, 24, 35};
        memset(soul.counts, 1, sizeof soul.counts);
        /* sub_080494F8's recent-first ordering: mix valid, duplicate,
         * empty and out-of-range cache entries across the three lists. */
        static const AosRecentSouls recent_souls = {
            .soul = {{5, 5, 0xFF, 200}, {7, 0xFF, 0xFF, 0xFF},
                     {3, 9, 0xFF, 0xFF}}};
        soul.recent_souls = &recent_souls;
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
        assert(aos_pause_graphics_render_sleep(
            &graphics, false, AOS_PAUSE_SLEEP_RESULT_NONE,
            rgba, 240 * 160 * 4));
        /* sub_0804BD38 renders three bands, not one shared bottom window:
         * the 0x361 refusal must land in the narrow upper-left question
         * box (tile rows ~8-10), not only the bottom description band. */
        {
            unsigned upper = 0;
            for (unsigned row = 8; row <= 10; ++row)
                for (unsigned col = 8; col <= 19; ++col)
                    if (graphics.bg0_map[row * 32 + col] != 0x330D)
                        ++upper;
            assert(upper > 0);
        }
        /* Band 3 stays empty on entry: sub_0804BD38 prints no result
         * into the shared bottom window (tile rows 15-19). */
        for (unsigned row = 15; row <= 19; ++row)
            for (unsigned col = 0; col <= 25; ++col)
                assert(graphics.bg0_map[row * 32 + col] == 0x330D);
        assert(aos_pause_graphics_render_sleep(
            &graphics, true, AOS_PAUSE_SLEEP_RESULT_NONE,
            rgba, 240 * 160 * 4));
        /* The available path draws the 0x36A question in the same upper
         * band plus vertically stacked Yes/No labels in the middle
         * parallelogram frame (place_text_id at pixels y96/y104). */
        {
            unsigned upper = 0;
            for (unsigned row = 8; row <= 10; ++row)
                for (unsigned col = 8; col <= 19; ++col)
                    if (graphics.bg0_map[row * 32 + col] != 0x330D)
                        ++upper;
            assert(upper > 0);
        }
        /* sub_0804BE0C: a successful save prints 0x36B "Game was saved/
         * successfully." and a failed one 0x386 "Save failed." into the
         * shared bottom window (tile rows 15-19, cols 0-25) as
         * palette-3 text tiles; the result persists until re-entry. */
        for (unsigned result = AOS_PAUSE_SLEEP_RESULT_SAVED;
             result <= AOS_PAUSE_SLEEP_RESULT_FAILED; ++result) {
            assert(aos_pause_graphics_render_sleep(
                &graphics, true, result, rgba, 240 * 160 * 4));
            unsigned result_cells = 0;
            for (unsigned row = 15; row <= 19; ++row)
                for (unsigned col = 0; col <= 25; ++col)
                    if ((graphics.bg0_map[row * 32 + col] & 0x3FF) < 0x200)
                        ++result_cells;
            assert(result_cells > 0);
        }
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
            /* Window fill at tilemap (6,13), 23x6: cell (14,6) is
             * i=1,j=0 -> 0x3200+1*32+0 = 0x3220; (18,26) -> 0x32B4. The
             * left column stays clear of the description canvas that
             * sub_0804BCA0 commits at tile origin (6,15). */
            assert(graphics.bg0_map[14 * 32 + 6] == 0x3220);
            assert(graphics.bg0_map[18 * 32 + 26] == 0x32B4);
            /* Strips: byte14>>1 = 3 iterations give rows 4,6,8 only —
             * i=0 row 4 -> 0x3380/0x3390, i=2 row 8 -> 0x33C0/0x33D0.
             * Row 10 stays at the 0x330D fill (no fourth strip row). */
            assert(graphics.bg0_map[4 * 32 + 3] == 0x3380);
            assert(graphics.bg0_map[4 * 32 + 12] == 0x3389);
            assert(graphics.bg0_map[4 * 32 + 17] == 0x3390);
            assert(graphics.bg0_map[8 * 32 + 3] == 0x33C0);
            assert(graphics.bg0_map[8 * 32 + 17] == 0x33D0);
            assert(graphics.bg0_map[10 * 32 + 3] == 0x330D);
            /* ON/OFF pairs: ability 0 even -> x 0xD row 4 tile 0xB31E;
             * ability 1 odd -> x 0x1B row 4 tile 0xB30E; ability 3 odd ->
             * x 0x1B row 6 tile 0xB31E. */
            assert(graphics.bg0_map[4 * 32 + 0xD] == 0xB31E);
            assert(graphics.bg0_map[4 * 32 + 0xE] == 0xB31F);
            assert(graphics.bg0_map[4 * 32 + 0x1B] == 0xB30E);
            assert(graphics.bg0_map[6 * 32 + 0x1B] == 0xB31E);
            /* Unowned ability 2 writes no marker. */
            assert(graphics.bg0_map[4 * 32 + 0xD] != 0xB30E);
            /* sub_0804BCA0/sub_0804BB40: cursor 0 is owned, so the
             * bottom window shows text 0x257+0 = 0x257 "Backdash by
             * pressing B." through the context-A cell path committed at
             * tile origin (6,15) — cells land on tile rows 15-18,
             * columns 6+, as palette-3 text tiles (index < 0x200),
             * below the name rows 12-14 and inside the frame. */
            unsigned description_cells = 0;
            for (unsigned row = 15; row <= 18; ++row)
                for (unsigned col = 6; col <= 28; ++col)
                    if ((graphics.bg0_map[row * 32 + col] & 0x3FF) < 0x200)
                        ++description_cells;
            assert(description_cells > 0);
            /* Selecting an unowned ability prints 0x34F "---" instead —
             * still text cells, but far fewer than a full description. */
            ability.cursor = 2;
            assert(aos_pause_graphics_render_ability(&graphics, &ability,
                                                     rgba, 240 * 160 * 4));
            unsigned unowned_cells = 0;
            for (unsigned row = 15; row <= 18; ++row)
                for (unsigned col = 6; col <= 28; ++col)
                    if ((graphics.bg0_map[row * 32 + col] & 0x3FF) < 0x200)
                        ++unowned_cells;
            assert(unowned_cells > 0 && unowned_cells < description_cells);
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
            /* Description of selected item 0 = text 0x15C on the bars,
             * and sub_08048804's icon entity draws icon 1 (item_use.tsv)
             * at anchor (0x1C,0x94) + frame offset (-8,-16) -> the
             * 16x16 icon spans pixels (20,132)-(35,147). */
            items.icons[0] = 1;
            items.selected = 0;
            assert(aos_pause_graphics_render_item_use(&graphics, &items,
                                                      rgba, 240 * 160 * 4));
            if (graphics.icons_ready) {
                /* Center of the icon block must carry icon colours, not
                 * the backdrop. */
                size_t center = ((size_t)140 * 240 + 28) * 4;
                bool icon_pixel = rgba[center + 3] == 255 &&
                    (rgba[center] != graphics.backdrop_rgb[0] ||
                     rgba[center + 1] != graphics.backdrop_rgb[1] ||
                     rgba[center + 2] != graphics.backdrop_rgb[2]);
                assert(icon_pixel);
            }
        }
        /* sub_0804D2C8 intro phases: +0xC 0 draws only the row
         * strips, 1 adds names/numbers, 2 adds the footer. Enemies 0
         * and 11 defeated on page 0/10. */
        {
            AosPauseEnemyValues enemy = {.top = 0, .cursor = 0,
                                         .intro_phase = 2};
            enemy.defeated[0] = 1u | (1u << 11); /* enemies 0 and 11 */
            /* Phase 0: strips present, names and footer absent. */
            AosPauseEnemyValues intro0 = enemy;
            intro0.intro_phase = 0;
            assert(aos_pause_graphics_render_enemy(&graphics, &intro0,
                                                   rgba, 240 * 160 * 4));
            assert(graphics.bg0_map[5 * 32 + 25] == 0xF335);
            assert(graphics.bg0_map[0x11 * 32 + 0xE] != 0x139B);
            /* Phase 1: names/numbers present, footer still absent. */
            AosPauseEnemyValues intro1 = enemy;
            intro1.intro_phase = 1;
            assert(aos_pause_graphics_render_enemy(&graphics, &intro1,
                                                   rgba, 240 * 160 * 4));
            assert(graphics.bg0_map[0x11 * 32 + 0xE] != 0x139B);
            assert(aos_pause_graphics_render_enemy(&graphics, &enemy,
                                                   rgba, 240 * 160 * 4));
            /* Slot strips at columns 6/0x14 rows 5+2*(slot/2); the big
             * enemy numbers overlap the strip's left cells, so assert on
             * cells past the number reach (col 25). */
            assert(graphics.bg0_map[5 * 32 + 25] == 0xF335);
            assert(graphics.bg0_map[13 * 32 + 25] == 0xF3B5);
            /* sub_0804C668 header/footer strips. */
            assert(graphics.bg0_map[3 * 32 + 0xB] == 0xF3D0);
            assert(graphics.bg0_map[3 * 32 + 0x18] == 0xF3DD);
            assert(graphics.bg0_map[0xF * 32 + 0x11] == 0xF3E0);
            assert(graphics.bg0_map[0xF * 32 + 0x15] == 0x13E4);
            assert(graphics.bg0_map[0xF * 32 + 0x19] == 0x13E8);
            assert(graphics.bg0_map[0xF * 32 + 0x1B] == 0xF3EA);
            /* sub_0804D2C8 footer: page "01" ends col 0xD row 0x11,
             * separator 0x139B, total "12" ends col 0x10. */
            assert(graphics.bg0_map[0x11 * 32 + 0xD] == 0x1301);
            assert(graphics.bg0_map[0x11 * 32 + 0xC] == 0x1300);
            assert(graphics.bg0_map[0x11 * 32 + 0xE] == 0x139B);
            assert(graphics.bg0_map[0x11 * 32 + 0x10] == 0x1302);
            assert(graphics.bg0_map[0x11 * 32 + 0xF] == 0x1301);
            enemy.top = 10;
            enemy.cursor = 10;
            assert(aos_pause_graphics_render_enemy(&graphics, &enemy,
                                                   rgba, 240 * 160 * 4));
            /* Enemy 11 defeated -> name text written near (0,64);
             * undiscovered enemies render 0x34E instead. */
        }
        /* sub_0804CDF4 detail: header numbers, drop names, name
         * underline, element grids, soul bars and markers. */
        {
            AosPauseEnemyValues detail = {.cursor = 1, .detail = 1};
            detail.defeated[0] = 1u << 1;
            detail.dropped[0][0] = 1u << 1; /* common drop seen */
            detail.hp = 18;
            detail.experience = 12;
            detail.weak = 0x04;
            detail.resist = 0x01;
            detail.common_item = 1;
            detail.rare_item = 0;
            detail.soul_type = 1;
            detail.soul_index = 2;
            detail.soul_owned = 1;
            assert(aos_pause_graphics_render_enemy_detail(
                &graphics, &detail, rgba, 240 * 160 * 4));
            /* sub_0804C968 header strips rows 8-9. */
            assert(graphics.bg0_map[8 * 32 + 0x12] == 0x1320);
            assert(graphics.bg0_map[8 * 32 + 0x1B] == 0x1329);
            assert(graphics.bg0_map[9 * 32 + 0x12] == 0x1330);
            /* Enemy number "002" right-aligned ending col 8 row 1;
             * digits are palette-1 entries 0x1300|digit. */
            assert(graphics.bg0_map[1 * 32 + 8] == (0x1300 | 2));
            /* HP "0018" ends col 0x14 row 2; EXP "00000012" ends
             * col 0x1B row 0xB. */
            assert(graphics.bg0_map[2 * 32 + 0x14] == (0x1300 | 8));
            assert(graphics.bg0_map[0xB * 32 + 0x1B] == (0x1300 | 2));
            /* Name underline strip and element icon grids. */
            assert(graphics.bg0_map[0xC * 32 + 2] == 0x1310);
            assert(graphics.bg0_map[4 * 32 + 0x15] == 0xD36F);
            assert(graphics.bg0_map[6 * 32 + 0x15] == 0xD3AF);
            /* Owned soul, view off: 0x134E marker row 0x10 and the
             * 0xCC0/0xCD0 soul icon strips. */
            assert(graphics.bg0_map[0x10 * 32 + 0x1C] == 0x134E);
            assert(graphics.bg0_map[1 * 32 + 0x18] == 0xCC0);
            assert(graphics.bg0_map[2 * 32 + 0x18] == 0xCD0);
            /* Soul bars rows 0xE-0x10. */
            assert(graphics.bg0_map[0xE * 32 + 3] == 0x1200);
            assert(graphics.bg0_map[0x10 * 32 + 3] == 0x1280);
        }
        /* Page dumps for visual comparison: argv[5..10] receive the
         * Item Use, Ability, Config, Sleep, Enemy list and Enemy detail
         * renders with a moderately populated state. */
        if (argc >= 11) {
            {
                AosPauseItemUseValues items = {.selected = 0, .top = 0};
                static const unsigned owned[] = {0, 3, 5, 9, 12, 16, 20, 24};
                for (unsigned i = 0; i < sizeof owned / sizeof owned[0]; ++i) {
                    items.counts[owned[i]] = (uint8_t)(1 + i);
                    items.icons[owned[i]] = (uint8_t)(owned[i] + 1);
                }
                assert(aos_pause_graphics_render_item_use(
                    &graphics, &items, rgba, 240 * 160 * 4));
                write_ppm(argv[5], rgba);
            }
            {
                AosPauseAbilityValues ability = {.cursor = 0};
                for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i) {
                    ability.owned[i] = 1;
                    ability.enabled[i] = (i & 1) != 0;
                }
                assert(aos_pause_graphics_render_ability(
                    &graphics, &ability, rgba, 240 * 160 * 4));
                write_ppm(argv[6], rgba);
            }
            {
                uint8_t config[AOS_BUTTON_ACTIONS] = {1, 0, 3, 2};
                assert(aos_pause_graphics_render_config(
                    &graphics, config, 1, rgba, 240 * 160 * 4));
                write_ppm(argv[7], rgba);
            }
            assert(aos_pause_graphics_render_sleep(
                &graphics, true, AOS_PAUSE_SLEEP_RESULT_NONE,
                rgba, 240 * 160 * 4));
            write_ppm(argv[8], rgba);
            if (argc >= 14) {
                /* sub_0804BE0C's result lines for capture comparison. */
                assert(aos_pause_graphics_render_sleep(
                    &graphics, true, AOS_PAUSE_SLEEP_RESULT_SAVED,
                    rgba, 240 * 160 * 4));
                write_ppm(argv[11], rgba);
                assert(aos_pause_graphics_render_sleep(
                    &graphics, true, AOS_PAUSE_SLEEP_RESULT_FAILED,
                    rgba, 240 * 160 * 4));
                write_ppm(argv[12], rgba);
                /* Ability page with the cursor on an unowned entry so
                 * sub_0804BCA0 prints 0x34F "---". */
                AosPauseAbilityValues unowned = {.cursor = 2};
                assert(aos_pause_graphics_render_ability(
                    &graphics, &unowned, rgba, 240 * 160 * 4));
                write_ppm(argv[13], rgba);
            }
            if (argc >= 20) {
                /* One dump per ability cursor (all owned) so each
                 * 0x257+cursor description can be compared natively. */
                AosPauseAbilityValues sweep = {0};
                for (unsigned i = 0; i < AOS_PAUSE_ABILITY_COUNT; ++i)
                    sweep.owned[i] = 1;
                for (unsigned cursor = 0; cursor < AOS_PAUSE_ABILITY_COUNT;
                     ++cursor) {
                    sweep.cursor = (uint8_t)cursor;
                    assert(aos_pause_graphics_render_ability(
                        &graphics, &sweep, rgba, 240 * 160 * 4));
                    write_ppm(argv[14 + cursor], rgba);
                }
            }
            {
                AosPauseEnemyValues enemy = {.top = 0, .cursor = 0,
                                             .intro_phase = 2};
                for (unsigned e = 0; e < AOS_PAUSE_ENEMY_RECORDS; ++e)
                    if (e % 3) enemy.defeated[e >> 5] |= 1u << (e & 31);
                assert(aos_pause_graphics_render_enemy(
                    &graphics, &enemy, rgba, 240 * 160 * 4));
                write_ppm(argv[9], rgba);
                enemy.detail = 1;
                enemy.cursor = 1;
                enemy.defeated[0] |= 1u << 1;
                enemy.dropped[0][0] = 1u << 1;
                enemy.hp = 18;
                enemy.experience = 12;
                enemy.weak = 0x04;
                enemy.resist = 0x01;
                enemy.common_item = 1;
                enemy.rare_item = 0;
                enemy.soul_type = 1;
                enemy.soul_index = 2;
                enemy.soul_owned = 1;
                enemy.soul_view = 1;
                assert(aos_pause_graphics_render_enemy_detail(
                    &graphics, &enemy, rgba, 240 * 160 * 4));
                write_ppm(argv[10], rgba);
            }
        }
        free(rgba);
        aos_pause_graphics_free(&graphics);
        fusion_rom_close(&local);
    }
    puts("Aria pause graphics tests passed");
    return 0;
}
