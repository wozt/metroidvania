/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_ROM_GRAPHICS_H
#define AOS_ROM_GRAPHICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "fusion/assets/rom.h"

bool aos_rom_load_graphics_resource(const FusionRom *rom, uint32_t pointer,
                                    uint8_t *destination,
                                    size_t destination_size,
                                    char *error, size_t error_size);
bool aos_rom_load_palette_resource(const FusionRom *rom, uint32_t pointer,
                                   uint8_t palette[0x200],
                                   char *error, size_t error_size);

#endif
