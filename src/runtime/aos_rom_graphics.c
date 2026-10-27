/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_rom_graphics.h"

#include "fusion/assets/gba.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { AOS_ROM_BASE = 0x08000000, AOS_PALETTE_ROWS = 16 };

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static const uint8_t *slice(const FusionRom *rom, uint32_t pointer,
                            size_t size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_ARIA ||
        pointer < AOS_ROM_BASE)
        return NULL;
    size_t offset = (size_t)(pointer - AOS_ROM_BASE);
    if (offset > rom->size || size > rom->size - offset) return NULL;
    return rom->data + offset;
}

bool aos_rom_load_graphics_resource(const FusionRom *rom, uint32_t pointer,
                                    uint8_t *destination,
                                    size_t destination_size,
                                    char *error, size_t error_size) {
    const uint8_t *resource = slice(rom, pointer, 8);
    if (!resource || !destination) {
        set_error(error, error_size, "invalid Aria graphics resource");
        return false;
    }
    size_t size = resource[1] == 0 || resource[1] == 4
        ? (size_t)resource[3] << 9 : 0x4000;
    if (size == 0 || size > destination_size) {
        set_error(error, error_size, "Aria graphics resource exceeds VRAM");
        return false;
    }
    if (resource[0] == 0) {
        const uint8_t *source = slice(rom, pointer + 4, size);
        if (!source) {
            set_error(error, error_size, "truncated Aria graphics resource");
            return false;
        }
        memcpy(destination, source, size);
        if (error && error_size) error[0] = '\0';
        return true;
    }
    if (resource[0] != 1) {
        set_error(error, error_size, "unsupported Aria graphics encoding");
        return false;
    }
    uint32_t compressed_pointer = read_u32(resource + 4);
    const uint8_t *compressed = slice(rom, compressed_pointer, 4);
    if (!compressed) {
        set_error(error, error_size, "invalid Aria graphics pointer");
        return false;
    }
    size_t available = rom->size - (size_t)(compressed_pointer - AOS_ROM_BASE);
    size_t expanded_size = fusion_gba_uncompressed_size(compressed, available);
    uint8_t *expanded = expanded_size == size + 4 ? malloc(expanded_size) : NULL;
    size_t written = 0;
    if (!expanded ||
        !fusion_gba_lz77_uncompress(compressed, available, expanded,
                                    expanded_size, &written) ||
        written != expanded_size || expanded[0] != 0) {
        free(expanded);
        set_error(error, error_size, "invalid compressed Aria graphics");
        return false;
    }
    memcpy(destination, expanded + 4, size);
    free(expanded);
    if (error && error_size) error[0] = '\0';
    return true;
}

bool aos_rom_load_palette_resource(const FusionRom *rom, uint32_t pointer,
                                   uint8_t palette[0x200],
                                   char *error, size_t error_size) {
    const uint8_t *resource = slice(rom, pointer, 4);
    if (!resource || !palette || resource[0] != 0) {
        set_error(error, error_size, "invalid Aria palette resource");
        return false;
    }
    size_t count = resource[2] > AOS_PALETTE_ROWS
        ? AOS_PALETTE_ROWS : resource[2];
    size_t first = resource[3];
    if (count == 0 || first > AOS_PALETTE_ROWS ||
        count > AOS_PALETTE_ROWS - first) {
        set_error(error, error_size, "Aria palette resource exceeds palette memory");
        return false;
    }
    const uint8_t *source = slice(rom, pointer + 4, count * 0x20);
    if (!source) {
        set_error(error, error_size, "truncated Aria palette resource");
        return false;
    }
    memcpy(palette + first * 0x20, source, count * 0x20);
    if (error && error_size) error[0] = '\0';
    return true;
}
