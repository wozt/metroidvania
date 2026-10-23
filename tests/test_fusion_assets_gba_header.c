/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/gba.h"
#include "fusion/assets/rom.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void set_complement_checksum(uint8_t *rom) {
    unsigned int sum = 0;
    for (size_t offset = 0xA0; offset <= 0xBC; ++offset) sum += rom[offset];
    rom[0xBD] = (uint8_t)(-(sum + 0x19u));
}

int main(void) {
    uint8_t rom[0xC0] = {0};
    memcpy(rom + 0xA0, "SYNTHETIC TEST", 12);
    memcpy(rom + 0xAC, "ABCD", 4);
    memcpy(rom + 0xB0, "01", 2);
    rom[0xB2] = 0x96;
    rom[0xB3] = 0x01;
    rom[0xB4] = 0x02;
    rom[0xBC] = 0x03;
    set_complement_checksum(rom);

    FusionGbaHeader header;
    assert(fusion_gba_read_header(rom, sizeof rom, &header));
    assert(!strcmp(header.title, "SYNTHETIC TE"));
    assert(!strcmp(header.game_code, "ABCD"));
    assert(!strcmp(header.maker_code, "01"));
    assert(header.main_unit_code == 1 && header.device_type == 2);
    assert(header.software_version == 3 && header.checksum_valid);
    assert(fusion_gba_header_checksum_valid(rom, sizeof rom));
    FusionRom verified_buffer = {rom, sizeof rom, FUSION_ASSET_GAME_ARIA};
    FusionGbaHeader wrapped;
    assert(fusion_rom_read_gba_header(&verified_buffer, &wrapped));
    assert(!strcmp(wrapped.game_code, "ABCD"));

    rom[0xBD] ^= 1;
    assert(fusion_gba_read_header(rom, sizeof rom, &header));
    assert(!header.checksum_valid);
    rom[0xB2] = 0;
    assert(!fusion_gba_read_header(rom, sizeof rom, &header));
    assert(!fusion_gba_header_checksum_valid(rom, 0xBD));
    puts("Native GBA header tests passed");
    return 0;
}
