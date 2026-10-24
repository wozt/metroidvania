/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded Metroid: Zero Mission asset metadata readers. */
#ifndef FUSION_ASSETS_MZM_H
#define FUSION_ASSETS_MZM_H

#include "fusion/assets/rom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { FUSION_MZM_ROM_SYMBOL_CAPACITY = 65536 };

typedef struct {
    const char *name;
    uint32_t address;
    uint32_t size;
    size_t elf_order;
} FusionMzmRomSymbol;

typedef struct {
    FusionMzmRomSymbol *entries;
    size_t count;
    size_t duplicate_name_count;
} FusionMzmRomSymbolTable;

/* Symbol names point into elf_data and remain valid only while that buffer is
 * alive. The returned entry array is released with fusion_mzm_free_symbols. */
bool fusion_mzm_read_rom_symbols(const FusionRom *rom,
                                 const uint8_t *elf_data, size_t elf_size,
                                 FusionMzmRomSymbolTable *table,
                                 char *error, size_t error_size);
void fusion_mzm_free_symbols(FusionMzmRomSymbolTable *table);

#endif /* FUSION_ASSETS_MZM_H */
