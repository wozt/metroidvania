/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_resources.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ROM_SIZE = 0x510000,
    TEXT_TABLE_OFFSET = 0x506B38,
    MAIN_OFFSET = 0x1000,
    INSERT_OFFSET = 0x1100,
    PROMPT_OFFSET = 0x1200,
    PROMPT_ID = 0x354,
    BAD_PROMPT_ID = 0x355,
};

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

static void next_text_row(AosIntroEntities *entities) {
    for (unsigned frame = 0; frame < 60; ++frame) {
        aos_intro_entities_update(entities, 3, 0x1600000, 0, 0x0C90000);
        if (entities->text_step_requested) return;
    }
    assert(!"text row timeout");
}

int main(int argc, char **argv) {
    uint8_t *data = calloc(1, ROM_SIZE);
    assert(data);
    static const uint8_t main_script[] = {
        1, 0, 'A', 6, 1, 7, 'B', 10,
    };
    static const uint8_t insert_script[] = {1, 0, 'X', 6};
    memcpy(data + MAIN_OFFSET, main_script, sizeof main_script);
    memcpy(data + INSERT_OFFSET, insert_script, sizeof insert_script);
    write_u32(data, TEXT_TABLE_OFFSET + 7 * 4, 0x08000000 + INSERT_OFFSET);
    write_u32(data, TEXT_TABLE_OFFSET + 8 * 4,
              0x08000000 + INSERT_OFFSET + sizeof insert_script);
    write_u32(data, TEXT_TABLE_OFFSET + 11 * 4, 0x08000000 + MAIN_OFFSET);
    write_u32(data, TEXT_TABLE_OFFSET + 12 * 4,
              0x08000000 + MAIN_OFFSET + sizeof main_script);

    /* A two-line localized prompt and an entry with an unknown control. */
    static const uint8_t prompt_script[] = {0, 0, 'P', 'L', 6, 'Y', 10, 'Z'};
    static const uint8_t bad_script[] = {0, 0, 'Q', 3, 10};
    memcpy(data + PROMPT_OFFSET, prompt_script, sizeof prompt_script);
    memcpy(data + PROMPT_OFFSET + 0x20, bad_script, sizeof bad_script);
    write_u32(data, TEXT_TABLE_OFFSET + PROMPT_ID * 4, 0x08000000 + PROMPT_OFFSET);
    write_u32(data, TEXT_TABLE_OFFSET + (PROMPT_ID + 1) * 4,
              0x08000000 + PROMPT_OFFSET + 0x20);
    write_u32(data, TEXT_TABLE_OFFSET + (BAD_PROMPT_ID + 1) * 4,
              0x08000000 + PROMPT_OFFSET + 0x20 + sizeof bad_script);

    FusionRom rom = {
        .data = data,
        .size = ROM_SIZE,
        .game = FUSION_ASSET_GAME_ARIA,
    };
    AosIntroResources resources;
    char error[128];
    assert(aos_intro_resources_init(&resources, &rom, error, sizeof error));

    AosIntroEntities entities;
    aos_intro_entities_init(&entities);
    assert(aos_intro_resources_bind(&resources, &entities, 1, error,
                                    sizeof error));
    next_text_row(&entities);
    assert(entities.text_glyph_count == 1 && entities.text_glyphs[0] == 'A' &&
           entities.text_script_position == 2);
    next_text_row(&entities);
    assert(entities.text_glyph_count == 2 &&
           memcmp(entities.text_glyphs, "XB", 2) == 0);
    assert(entities.text_script_finished && entities.text_script_position == -1);

    AosNativeText text;
    assert(aos_intro_resources_read_text(&resources, 1, PROMPT_ID, &text, error,
                                         sizeof error));
    assert(text.line_count == 2 && text.glyph_counts[0] == 2 &&
           memcmp(text.glyphs[0], "PL", 2) == 0 && text.glyph_counts[1] == 1 &&
           text.glyphs[1][0] == 'Y');
    assert(resources.language == 1);
    assert(!aos_intro_resources_read_text(&resources, 1, BAD_PROMPT_ID, &text,
                                          error, sizeof error));
    assert(strstr(error, "unsupported control") != NULL);
    assert(!aos_intro_resources_read_text(&resources, 4, PROMPT_ID, &text,
                                          error, sizeof error));

    assert(!aos_intro_resources_bind(&resources, &entities, 0, error,
                                     sizeof error));
    assert(strstr(error, "invalid Aria introduction") != NULL);

    rom.game = FUSION_ASSET_GAME_METROID;
    assert(!aos_intro_resources_init(&resources, &rom, error, sizeof error));
    free(data);

    /* Optional local smoke check. CTest uses only synthetic data; developers
     * may pass their verified ROM without printing or exporting its text. */
    if (argc == 2) {
        FusionRom local = {0};
        assert(fusion_rom_open(FUSION_ASSET_GAME_ARIA, argv[1], &local,
                               error, sizeof error));
        assert(aos_intro_resources_init(&resources, &local, error,
                                        sizeof error));
        for (uint8_t language = 1; language <= 3; ++language) {
            aos_intro_entities_init(&entities);
            assert(aos_intro_resources_bind(&resources, &entities, language,
                                            error, sizeof error));
            next_text_row(&entities);
            assert(entities.text_glyph_count > 0 &&
                   !entities.text_script_missing &&
                   !entities.text_script_malformed);
            /* Every prompt the main-menu controller selects. */
            static const uint16_t prompts[] = {0x354, 0x355, 0x356, 0x358, 0x359,
                                               0x35A, 0x35B, 0x35C, 0x377, 0x379};
            for (size_t index = 0; index < sizeof prompts / sizeof prompts[0]; ++index) {
                if (!aos_intro_resources_read_text(&resources, language, prompts[index],
                                                   &text, error, sizeof error)) {
                    fprintf(stderr, "prompt 0x%X language %u: %s\n", prompts[index],
                            language, error);
                    assert(0);
                }
                assert(text.glyph_counts[0] > 0);
            }
        }
        fusion_rom_close(&local);
    }
    puts("Aria introduction resource tests passed");
    return 0;
}
