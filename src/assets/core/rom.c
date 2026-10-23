/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/rom.h"

#include "sha1.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FUSION_ROM_MAX_SIZE = 32 * 1024 * 1024 };

static const FusionAssetGameSpec specs[FUSION_ASSET_GAME_COUNT] = {
    {FUSION_ASSET_GAME_METROID, "metroid", "Metroid: Zero Mission USA",
     "Metroid - Zero Mission (USA).gba", "5de8536afe1f0078ee6fe1089f890e8c7aa0a6e8", 8 * 1024 * 1024},
    {FUSION_ASSET_GAME_ARIA, "aria", "Castlevania: Aria of Sorrow USA",
     "Castlevania - Aria of Sorrow (USA).gba", "abd71fe01ebb201bcc133074db1dd8c5253776c7", 8 * 1024 * 1024},
};

static void set_error(char *error, size_t error_size, const char *message) {
    if (error && error_size > 0) {
        snprintf(error, error_size, "%s", message);
        error[error_size - 1] = '\0';
    }
}

const FusionAssetGameSpec *fusion_asset_game_spec(FusionAssetGame game) {
    return game < FUSION_ASSET_GAME_COUNT ? &specs[game] : NULL;
}

const FusionAssetGameSpec *fusion_asset_game_from_id(const char *id) {
    if (!id) return NULL;
    for (int i = 0; i < FUSION_ASSET_GAME_COUNT; ++i)
        if (!strcmp(id, specs[i].id)) return &specs[i];
    return NULL;
}

bool fusion_rom_sha1_hex(const uint8_t *data, size_t size, char out[41]) {
    static const char hex[] = "0123456789abcdef";
    if ((!data && size != 0) || !out) return false;
    uint8_t digest[20];
    fusion_sha1_digest(data, size, digest);
    for (size_t i = 0; i < sizeof digest; ++i) {
        out[i * 2] = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    out[40] = '\0';
    return true;
}

static void clear_rom(FusionRom *rom) {
    if (!rom) return;
    if (rom->data) {
        volatile uint8_t *bytes = rom->data;
        for (size_t i = 0; i < rom->size; ++i) bytes[i] = 0;
        free(rom->data);
    }
    *rom = (FusionRom){0};
}

bool fusion_rom_open(FusionAssetGame game, const char *path, FusionRom *rom,
                     char *error, size_t error_size) {
    const FusionAssetGameSpec *spec = fusion_asset_game_spec(game);
    if (!spec || !path || !rom) {
        set_error(error, error_size, "invalid ROM arguments");
        return false;
    }
    *rom = (FusionRom){0};
    FILE *file = fopen(path, "rb");
    if (!file) {
        char message[256];
        snprintf(message, sizeof message, "cannot open %s: %s", path, strerror(errno));
        set_error(error, error_size, message);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error(error, error_size, "cannot seek ROM");
        return false;
    }
    long length = ftell(file);
    if (length < 0 || (unsigned long)length > FUSION_ROM_MAX_SIZE) {
        fclose(file);
        set_error(error, error_size, "ROM size is outside the supported safety bound");
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_error(error, error_size, "cannot rewind ROM");
        return false;
    }
    rom->size = (size_t)length;
    if (rom->size != spec->expected_size) {
        size_t actual_size = rom->size;
        fclose(file);
        *rom = (FusionRom){0};
        char message[256];
        snprintf(message, sizeof message, "%s has size %zu, expected %zu", path, actual_size,
                 spec->expected_size);
        set_error(error, error_size, message);
        return false;
    }
    rom->data = malloc(rom->size);
    if (!rom->data) {
        fclose(file);
        *rom = (FusionRom){0};
        set_error(error, error_size, "cannot allocate ROM buffer");
        return false;
    }
    size_t read = fread(rom->data, 1, rom->size, file);
    bool complete = read == rom->size && ferror(file) == 0;
    fclose(file);
    if (!complete) {
        clear_rom(rom);
        set_error(error, error_size, "ROM read was truncated or failed");
        return false;
    }
    char actual[41];
    fusion_rom_sha1_hex(rom->data, rom->size, actual);
    if (strcmp(actual, spec->sha1) != 0) {
        clear_rom(rom);
        char message[256];
        snprintf(message, sizeof message, "%s has SHA-1 %s, expected %s", path, actual,
                 spec->sha1);
        set_error(error, error_size, message);
        return false;
    }
    rom->game = game;
    if (error && error_size) error[0] = '\0';
    return true;
}

void fusion_rom_close(FusionRom *rom) { clear_rom(rom); }

bool fusion_rom_read_gba_header(const FusionRom *rom, FusionGbaHeader *header) {
    return rom && fusion_gba_read_header(rom->data, rom->size, header);
}
