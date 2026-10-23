/* SPDX-License-Identifier: GPL-3.0-only */
/* ROM verification primitives shared by native asset tools. */
#ifndef FUSION_ASSETS_ROM_H
#define FUSION_ASSETS_ROM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/gba.h"

typedef enum {
    FUSION_ASSET_GAME_METROID = 0,
    FUSION_ASSET_GAME_ARIA = 1,
    FUSION_ASSET_GAME_COUNT
} FusionAssetGame;

typedef struct {
    FusionAssetGame game;
    const char *id;
    const char *title;
    const char *filename;
    const char *sha1;
    size_t expected_size;
} FusionAssetGameSpec;

typedef struct {
    uint8_t *data;
    size_t size;
    FusionAssetGame game;
} FusionRom;

const FusionAssetGameSpec *fusion_asset_game_spec(FusionAssetGame game);
const FusionAssetGameSpec *fusion_asset_game_from_id(const char *id);

/* Returns the lower-case SHA-1 digest of a memory range as 40 hexadecimal
 * characters plus a terminating NUL. */
bool fusion_rom_sha1_hex(const uint8_t *data, size_t size, char out[41]);

/* Opens, bounds-checks and verifies one supported ROM. The input file is
 * never modified and its private bytes are released by fusion_rom_close. */
bool fusion_rom_open(FusionAssetGame game, const char *path, FusionRom *rom,
                     char *error, size_t error_size);
void fusion_rom_close(FusionRom *rom);

/* Reads the standard header from an already verified private ROM buffer. */
bool fusion_rom_read_gba_header(const FusionRom *rom, FusionGbaHeader *header);

#endif /* FUSION_ASSETS_ROM_H */
