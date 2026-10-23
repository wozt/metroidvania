/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/rom.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char digest[41];
    const uint8_t abc[] = {'a', 'b', 'c'};
    assert(fusion_rom_sha1_hex(abc, sizeof abc, digest));
    assert(strcmp(digest, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
    assert(!fusion_rom_sha1_hex(NULL, 1, digest));

    const FusionAssetGameSpec *aria = fusion_asset_game_from_id("aria");
    assert(aria && aria->expected_size == 8u * 1024u * 1024u);
    assert(strcmp(aria->sha1, "abd71fe01ebb201bcc133074db1dd8c5253776c7") == 0);
    assert(fusion_asset_game_from_id("unknown") == NULL);

    FusionRom rom = {0};
    char error[256];
    assert(!fusion_rom_open(FUSION_ASSET_GAME_ARIA, "does-not-exist.gba", &rom,
                            error, sizeof error));
    assert(strstr(error, "cannot open") != NULL);
    FILE *bad = fopen("fusion-assets-short.gba", "wb");
    assert(bad);
    assert(fputc(0, bad) != EOF);
    assert(fclose(bad) == 0);
    assert(!fusion_rom_open(FUSION_ASSET_GAME_ARIA, "fusion-assets-short.gba", &rom,
                            error, sizeof error));
    assert(strstr(error, "size 1") != NULL);
    assert(remove("fusion-assets-short.gba") == 0);
    puts("Native asset ROM tests passed");
    return 0;
}
