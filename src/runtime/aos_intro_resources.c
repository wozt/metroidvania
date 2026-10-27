/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_intro_resources.h"

#include <stdio.h>

enum {
    AOS_ROM_BASE = 0x08000000,
    AOS_INTRO_TEXT_TABLE = 0x08506B38,
    AOS_INTRO_MAIN_TEXT_ID = 0x0B,
    AOS_TEXT_LANGUAGE_STRIDE = 0x3C4,
    AOS_TEXT_LANGUAGE_COUNT = 3,
    AOS_INTRO_SCRIPT_MAX_SIZE = 0x10000,
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static bool pointer_offset(const AosIntroResources *resources,
                           uint32_t pointer, size_t size, size_t *offset) {
    if (!resources || !resources->rom || !resources->rom->data ||
        pointer < AOS_ROM_BASE)
        return false;
    size_t value = (size_t)(pointer - AOS_ROM_BASE);
    if (value > resources->rom->size || size > resources->rom->size - value)
        return false;
    if (offset) *offset = value;
    return true;
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static bool table_pointer(const AosIntroResources *resources, size_t index,
                          uint32_t *pointer) {
    const size_t entry_count =
        AOS_TEXT_LANGUAGE_STRIDE * AOS_TEXT_LANGUAGE_COUNT;
    size_t table;
    if (index >= entry_count ||
        !pointer_offset(resources, AOS_INTRO_TEXT_TABLE,
                        entry_count * 4, &table))
        return false;
    *pointer = read_u32(resources->rom->data + table + index * 4);
    return true;
}

static bool script_at(AosIntroResources *resources, uint16_t id,
                      const uint8_t **script, size_t *script_size) {
    if (!resources || !script || !script_size || resources->language < 1 ||
        resources->language > AOS_TEXT_LANGUAGE_COUNT ||
        id >= AOS_TEXT_LANGUAGE_STRIDE)
        return false;
    size_t language_base =
        (size_t)(resources->language - 1) * AOS_TEXT_LANGUAGE_STRIDE;
    uint32_t pointer;
    size_t offset;
    if (!table_pointer(resources, language_base + id, &pointer) ||
        !pointer_offset(resources, pointer, 3, &offset))
        return false;

    size_t limit = resources->rom->size;
    for (size_t index = 0; index < AOS_TEXT_LANGUAGE_STRIDE; ++index) {
        uint32_t candidate;
        size_t candidate_offset;
        if (!table_pointer(resources, language_base + index, &candidate) ||
            candidate <= pointer ||
            !pointer_offset(resources, candidate, 1, &candidate_offset))
            continue;
        if (candidate_offset < limit) limit = candidate_offset;
    }
    if (limit <= offset + 2) return false;
    size_t size = limit - offset;
    if (size > AOS_INTRO_SCRIPT_MAX_SIZE) size = AOS_INTRO_SCRIPT_MAX_SIZE;
    *script = resources->rom->data + offset;
    *script_size = size;
    return true;
}

static bool resolve_insert(void *user, uint8_t id, const uint8_t **script,
                           size_t *script_size) {
    return script_at(user, id, script, script_size);
}

static bool valid_main_script(const uint8_t *script, size_t size) {
    if (!script || size < 3) return false;
    size_t position = 2;
    while (position < size) {
        uint8_t command = script[position];
        if (command & 0xE0u) {
            ++position;
        } else if (command == 1) {
            if (position + 1 >= size) return false;
            position += 2;
        } else if (command == 5 || command == 6 || command == 9) {
            ++position;
        } else if (command == 10) {
            return true;
        } else {
            return false;
        }
    }
    return false;
}

bool aos_intro_resources_init(AosIntroResources *resources,
                              const FusionRom *rom, char *error,
                              size_t error_size) {
    if (!resources || !rom || rom->game != FUSION_ASSET_GAME_ARIA ||
        !rom->data || rom->size == 0) {
        set_error(error, error_size, "invalid verified Aria ROM resource view");
        return false;
    }
    *resources = (AosIntroResources){.rom = rom, .language = 1};
    if (!pointer_offset(resources, AOS_INTRO_TEXT_TABLE,
                        AOS_TEXT_LANGUAGE_STRIDE * AOS_TEXT_LANGUAGE_COUNT * 4,
                        NULL)) {
        *resources = (AosIntroResources){0};
        set_error(error, error_size, "Aria text pointer table is outside the ROM");
        return false;
    }
    if (error && error_size) error[0] = '\0';
    return true;
}

bool aos_intro_resources_bind(AosIntroResources *resources,
                              AosIntroEntities *entities, uint8_t language,
                              char *error, size_t error_size) {
    if (!resources || !resources->rom || !entities || language < 1 ||
        language > AOS_TEXT_LANGUAGE_COUNT) {
        set_error(error, error_size, "invalid Aria introduction resource binding");
        return false;
    }
    resources->language = language;
    const uint8_t *script;
    size_t script_size;
    if (!script_at(resources, AOS_INTRO_MAIN_TEXT_ID, &script, &script_size) ||
        !valid_main_script(script, script_size)) {
        set_error(error, error_size,
                  "Aria introduction text script is missing or malformed");
        return false;
    }
    aos_intro_entities_set_text_script(entities, script, script_size,
                                       resolve_insert, resources);
    if (error && error_size) error[0] = '\0';
    return true;
}

bool aos_intro_resources_read_text(AosIntroResources *resources,
                                   uint8_t language, uint16_t id,
                                   AosNativeText *text, char *error,
                                   size_t error_size) {
    if (!resources || !resources->rom || !text || language < 1 ||
        language > AOS_TEXT_LANGUAGE_COUNT) {
        set_error(error, error_size, "invalid Aria localized text request");
        return false;
    }
    uint8_t previous_language = resources->language;
    resources->language = language;
    const uint8_t *script = NULL;
    size_t script_size = 0;
    bool found = script_at(resources, id, &script, &script_size);
    resources->language = previous_language;
    if (!found || script_size < 3) {
        set_error(error, error_size, "Aria localized text is outside the ROM");
        return false;
    }

    *text = (AosNativeText){.line_count = 1};
    for (size_t position = 2; position < script_size; ++position) {
        uint8_t value = script[position];
        if (value & 0xE0u) {
            size_t line = text->line_count - 1;
            size_t count = text->glyph_counts[line];
            if (count >= AOS_NATIVE_TEXT_MAX_GLYPHS) {
                set_error(error, error_size, "Aria localized text line is too long");
                return false;
            }
            text->glyphs[line][count] = value;
            text->glyph_counts[line] = count + 1;
        } else if (value == 6) {
            if (text->line_count >= AOS_NATIVE_TEXT_MAX_LINES) {
                set_error(error, error_size, "Aria localized text has too many lines");
                return false;
            }
            ++text->line_count;
        } else if (value == 10) {
            if (error && error_size) error[0] = '\0';
            return true;
        } else {
            set_error(error, error_size,
                      "unsupported control in Aria localized text");
            return false;
        }
    }
    set_error(error, error_size, "unterminated Aria localized text");
    return false;
}
