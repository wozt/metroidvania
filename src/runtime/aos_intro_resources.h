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

bool aos_intro_resources_init(AosIntroResources *resources,
                              const FusionRom *rom, char *error,
                              size_t error_size);
bool aos_intro_resources_bind(AosIntroResources *resources,
                              AosIntroEntities *entities, uint8_t language,
                              char *error, size_t error_size);

#endif
