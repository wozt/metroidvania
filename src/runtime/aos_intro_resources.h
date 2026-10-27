/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_INTRO_RESOURCES_H
#define AOS_INTRO_RESOURCES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_intro_entities.h"
#include "fusion/assets/rom.h"

/* Non-owning view of the verified Aria ROM. No script bytes are copied to a
 * tracked or exported file. */
typedef struct {
    const FusionRom *rom;
    uint8_t language;
} AosIntroResources;

enum {
    AOS_NATIVE_TEXT_MAX_LINES = 4,
    AOS_NATIVE_TEXT_MAX_GLYPHS = 80,
};

typedef struct {
    uint8_t glyphs[AOS_NATIVE_TEXT_MAX_LINES][AOS_NATIVE_TEXT_MAX_GLYPHS];
    size_t glyph_counts[AOS_NATIVE_TEXT_MAX_LINES];
    size_t line_count;
} AosNativeText;

bool aos_intro_resources_init(AosIntroResources *resources,
                              const FusionRom *rom, char *error,
                              size_t error_size);
bool aos_intro_resources_bind(AosIntroResources *resources,
                              AosIntroEntities *entities, uint8_t language,
                              char *error, size_t error_size);
/* Reads one localized entry from the native text table. Control 6 starts a
 * new line and control 10 terminates the entry, matching sub_08046BC8. */
bool aos_intro_resources_read_text(AosIntroResources *resources,
                                   uint8_t language, uint16_t id,
                                   AosNativeText *text, char *error,
                                   size_t error_size);

#endif
