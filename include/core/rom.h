#ifndef FUSION_CORE_ROM_H
#define FUSION_CORE_ROM_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ROM_ARIA_US,
    ROM_ZERO_MISSION_US
} RomKind;

typedef struct {
    RomKind kind;
    const char *label;
    const char *expected_sha1;
    const char *path;
} RomRequirement;

bool rom_validate(const RomRequirement *requirement, char *error, size_t error_size);

#endif
