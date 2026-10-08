#include "core/rom.h"

#include "core/sha1.h"

#include <stdio.h>
#include <string.h>

bool rom_validate(const RomRequirement *requirement, char *error, size_t error_size)
{
    uint8_t digest[20];
    char actual[41];
    char detail[160];

    if (!sha1_file(requirement->path, digest, detail, sizeof(detail))) {
        if (error_size > 0)
            snprintf(error, error_size, "%s: %s", requirement->label, detail);
        return false;
    }
    sha1_hex(digest, actual);
    if (strcmp(actual, requirement->expected_sha1) != 0) {
        if (error_size > 0)
            snprintf(error, error_size, "%s: SHA-1 inconnu (%s), attendu %s",
                     requirement->label, actual, requirement->expected_sha1);
        return false;
    }
    return true;
}
