/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/gba.h"

#include <string.h>

enum {
    GBA_HEADER_TITLE = 0xA0,
    GBA_HEADER_GAME_CODE = 0xAC,
    GBA_HEADER_MAKER_CODE = 0xB0,
    GBA_HEADER_FIXED = 0xB2,
    GBA_HEADER_MAIN_UNIT = 0xB3,
    GBA_HEADER_DEVICE = 0xB4,
    GBA_HEADER_VERSION = 0xBC,
    GBA_HEADER_CHECKSUM = 0xBD,
    GBA_HEADER_SIZE = 0xC0
};

bool fusion_gba_header_checksum_valid(const uint8_t *rom, size_t rom_size) {
    if (!rom || rom_size <= GBA_HEADER_CHECKSUM) return false;
    unsigned int sum = 0;
    for (size_t offset = GBA_HEADER_TITLE; offset <= GBA_HEADER_VERSION; ++offset) {
        sum += rom[offset];
    }
    uint8_t expected = (uint8_t)(-(sum + 0x19u));
    return expected == rom[GBA_HEADER_CHECKSUM];
}

bool fusion_gba_read_header(const uint8_t *rom, size_t rom_size,
                            FusionGbaHeader *header) {
    if (!rom || !header || rom_size < GBA_HEADER_SIZE || rom[GBA_HEADER_FIXED] != 0x96) {
        return false;
    }
    memset(header, 0, sizeof *header);
    memcpy(header->title, rom + GBA_HEADER_TITLE, 12);
    memcpy(header->game_code, rom + GBA_HEADER_GAME_CODE, 4);
    memcpy(header->maker_code, rom + GBA_HEADER_MAKER_CODE, 2);
    header->main_unit_code = rom[GBA_HEADER_MAIN_UNIT];
    header->device_type = rom[GBA_HEADER_DEVICE];
    header->software_version = rom[GBA_HEADER_VERSION];
    header->complement_checksum = rom[GBA_HEADER_CHECKSUM];
    header->checksum_valid = fusion_gba_header_checksum_valid(rom, rom_size);
    return true;
}
