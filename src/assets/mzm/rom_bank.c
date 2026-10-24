/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/mzm.h"

#include "output.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    ELF32_HEADER_SIZE = 0x34,
    ELF32_SECTION_SIZE = 0x28,
    ELF32_SYMBOL_SIZE = 0x10,
    ELF_SECTION_SYMBOL_TABLE = 2,
    ELF_SYMBOL_OBJECT = 1,
    GBA_ROM_BASE = 0x08000000
};

typedef struct {
    const char *name;
    bool duplicate;
} NameSlot;

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0) return;
    snprintf(error, error_size, "%s", message);
    error[error_size - 1] = '\0';
}

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)data[1] << 8;
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t)data[0] | (uint32_t)data[1] << 8 |
           (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

static void write_u32(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static bool range_valid(size_t total, size_t offset, size_t length) {
    return offset <= total && length <= total - offset;
}

static int compare_symbols(const void *left_data, const void *right_data) {
    const FusionMzmRomSymbol *left = left_data;
    const FusionMzmRomSymbol *right = right_data;
    if (left->address < right->address) return -1;
    if (left->address > right->address) return 1;
    int names = strcmp(left->name, right->name);
    if (names != 0) return names;
    if (left->elf_order < right->elf_order) return -1;
    if (left->elf_order > right->elf_order) return 1;
    return 0;
}

static size_t name_hash(const char *name) {
    size_t hash = (size_t)1469598103934665603ULL;
    while (*name) {
        hash ^= (unsigned char)*name++;
        hash *= (size_t)1099511628211ULL;
    }
    return hash;
}

static bool append_symbol(FusionMzmRomSymbol **entries, size_t *count,
                          size_t *capacity, FusionMzmRomSymbol symbol) {
    if (*count == *capacity) {
        size_t next = *capacity == 0 ? 256 : *capacity * 2;
        if (next > FUSION_MZM_ROM_SYMBOL_CAPACITY)
            next = FUSION_MZM_ROM_SYMBOL_CAPACITY;
        if (next == *capacity) return false;
        FusionMzmRomSymbol *resized = realloc(*entries, next * sizeof **entries);
        if (!resized) return false;
        *entries = resized;
        *capacity = next;
    }
    (*entries)[(*count)++] = symbol;
    return true;
}

void fusion_mzm_free_symbols(FusionMzmRomSymbolTable *table) {
    if (!table) return;
    free(table->entries);
    *table = (FusionMzmRomSymbolTable){0};
}

typedef struct {
    const FusionRom *rom;
} BankWriteContext;

static bool write_bank_body(FILE *file, const void *raw_context) {
    const BankWriteContext *context = raw_context;
    uint8_t header[16] = {'M', 'Z', 'M', 'R', 'O', 'M', '0', '1'};
    write_u32(header + 8, GBA_ROM_BASE);
    write_u32(header + 12, (uint32_t)context->rom->size);
    return fwrite(header, 1, sizeof header, file) == sizeof header &&
           fwrite(context->rom->data, 1, context->rom->size, file) ==
               context->rom->size;
}

typedef struct {
    const FusionMzmRomSymbolTable *table;
} ManifestWriteContext;

static bool write_manifest_body(FILE *file, const void *raw_context) {
    const ManifestWriteContext *context = raw_context;
    if (fputs("schema\tmetroidvania-mzm-rom-v1\n", file) == EOF) return false;
    for (size_t index = 0; index < context->table->count; ++index) {
        const FusionMzmRomSymbol *symbol = &context->table->entries[index];
        if (fprintf(file, "symbol\t%s\t%u\t%u\n", symbol->name,
                    symbol->address, symbol->size) < 0) {
            return false;
        }
    }
    return true;
}

bool fusion_mzm_write_rom_bank(const FusionRom *rom,
                               const FusionMzmRomSymbolTable *table,
                               const char *bank_path, const char *manifest_path,
                               char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        rom->size > UINT32_MAX || !table || (table->count && !table->entries) ||
        !bank_path || !manifest_path || strcmp(bank_path, manifest_path) == 0) {
        set_error(error, error_size, "invalid Zero Mission ROM-bank write request");
        return false;
    }
    for (size_t index = 0; index < table->count; ++index) {
        const FusionMzmRomSymbol *symbol = &table->entries[index];
        if (!symbol->name || symbol->address < GBA_ROM_BASE) {
            set_error(error, error_size, "invalid Zero Mission ROM symbol");
            return false;
        }
        size_t offset = (size_t)(symbol->address - GBA_ROM_BASE);
        if (offset >= rom->size || symbol->size > rom->size - offset) {
            set_error(error, error_size, "Zero Mission ROM symbol is outside the bank");
            return false;
        }
    }
    BankWriteContext bank_context = {rom};
    ManifestWriteContext manifest_context = {table};
    if (!fusion_asset_write_atomic(bank_path, write_bank_body, &bank_context,
                                   error, error_size) ||
        !fusion_asset_write_atomic(manifest_path, write_manifest_body,
                                   &manifest_context, error, error_size)) {
        return false;
    }
    if (error && error_size > 0) error[0] = '\0';
    return true;
}

bool fusion_mzm_read_rom_symbols(const FusionRom *rom,
                                 const uint8_t *elf_data, size_t elf_size,
                                 FusionMzmRomSymbolTable *table,
                                 char *error, size_t error_size) {
    if (!rom || !rom->data || rom->game != FUSION_ASSET_GAME_METROID ||
        !elf_data || !table) {
        set_error(error, error_size, "invalid Zero Mission symbol-table request");
        return false;
    }
    *table = (FusionMzmRomSymbolTable){0};
    if (elf_size < ELF32_HEADER_SIZE || memcmp(elf_data, "\x7f" "ELF", 4) != 0 ||
        elf_data[4] != 1 || elf_data[5] != 1) {
        set_error(error, error_size, "expected a little-endian ELF32 file");
        return false;
    }
    size_t section_offset = read_u32(elf_data + 0x20);
    size_t section_size = read_u16(elf_data + 0x2E);
    size_t section_count = read_u16(elf_data + 0x30);
    if (section_size < ELF32_SECTION_SIZE || section_count == 0 ||
        section_count > (elf_size / section_size) ||
        !range_valid(elf_size, section_offset, section_count * section_size)) {
        set_error(error, error_size, "invalid ELF32 section table");
        return false;
    }

    FusionMzmRomSymbol *candidates = NULL;
    size_t candidate_count = 0;
    size_t candidate_capacity = 0;
    size_t object_count = 0;
    for (size_t section = 0; section < section_count; ++section) {
        const uint8_t *header = elf_data + section_offset + section * section_size;
        if (read_u32(header + 4) != ELF_SECTION_SYMBOL_TABLE) continue;
        size_t symbols_offset = read_u32(header + 16);
        size_t symbols_size = read_u32(header + 20);
        size_t string_section = read_u32(header + 24);
        size_t entry_size = read_u32(header + 36);
        if (entry_size < ELF32_SYMBOL_SIZE || symbols_size % entry_size != 0 ||
            string_section >= section_count ||
            !range_valid(elf_size, symbols_offset, symbols_size)) {
            free(candidates);
            set_error(error, error_size, "invalid ELF32 symbol table");
            return false;
        }
        const uint8_t *strings_header =
            elf_data + section_offset + string_section * section_size;
        size_t strings_offset = read_u32(strings_header + 16);
        size_t strings_size = read_u32(strings_header + 20);
        if (!range_valid(elf_size, strings_offset, strings_size)) {
            free(candidates);
            set_error(error, error_size, "invalid ELF32 symbol-name table");
            return false;
        }
        for (size_t offset = 0; offset < symbols_size; offset += entry_size) {
            const uint8_t *symbol = elf_data + symbols_offset + offset;
            uint32_t symbol_size = read_u32(symbol + 8);
            if ((symbol[12] & 0x0F) != ELF_SYMBOL_OBJECT || symbol_size == 0) continue;
            if (++object_count > FUSION_MZM_ROM_SYMBOL_CAPACITY) {
                free(candidates);
                set_error(error, error_size, "ELF32 object-symbol count exceeds the safety cap");
                return false;
            }
            size_t name_offset = read_u32(symbol);
            if (name_offset >= strings_size) {
                free(candidates);
                set_error(error, error_size, "ELF32 object symbol has an invalid name offset");
                return false;
            }
            const char *name = (const char *)elf_data + strings_offset + name_offset;
            size_t name_capacity = strings_size - name_offset;
            const char *terminator = memchr(name, '\0', name_capacity);
            if (!terminator) {
                free(candidates);
                set_error(error, error_size, "ELF32 object symbol name is unterminated");
                return false;
            }
            for (const char *cursor = name; cursor < terminator; ++cursor) {
                if ((unsigned char)*cursor > 0x7F) {
                    free(candidates);
                    set_error(error, error_size, "ELF32 object symbol name is not ASCII");
                    return false;
                }
            }
            uint32_t address = read_u32(symbol + 4);
            if (address < GBA_ROM_BASE) continue;
            size_t rom_offset = (size_t)(address - GBA_ROM_BASE);
            if (rom_offset >= rom->size) continue;
            if (symbol_size > rom->size - rom_offset) {
                free(candidates);
                set_error(error, error_size, "ELF32 object symbol ends past the ROM");
                return false;
            }
            if (!append_symbol(&candidates, &candidate_count, &candidate_capacity,
                               (FusionMzmRomSymbol){name, address, symbol_size,
                                                    object_count - 1})) {
                free(candidates);
                set_error(error, error_size, "cannot allocate Zero Mission symbol table");
                return false;
            }
        }
    }
    if (object_count == 0) {
        free(candidates);
        set_error(error, error_size, "no object symbols in the ELF symbol table");
        return false;
    }

    qsort(candidates, candidate_count, sizeof *candidates, compare_symbols);
    size_t slot_count = 1;
    while (slot_count < candidate_count * 2) slot_count *= 2;
    NameSlot *slots = candidate_count ? calloc(slot_count, sizeof *slots) : NULL;
    if (candidate_count && !slots) {
        free(candidates);
        set_error(error, error_size, "cannot allocate Zero Mission symbol-name index");
        return false;
    }
    size_t unique_count = 0;
    size_t duplicate_count = 0;
    for (size_t index = 0; index < candidate_count; ++index) {
        size_t slot = name_hash(candidates[index].name) & (slot_count - 1);
        while (slots[slot].name &&
               strcmp(slots[slot].name, candidates[index].name) != 0) {
            slot = (slot + 1) & (slot_count - 1);
        }
        if (!slots[slot].name) {
            slots[slot].name = candidates[index].name;
            candidates[unique_count++] = candidates[index];
        } else if (!slots[slot].duplicate) {
            slots[slot].duplicate = true;
            ++duplicate_count;
        }
    }
    free(slots);
    table->entries = candidates;
    table->count = unique_count;
    table->duplicate_name_count = duplicate_count;
    if (error && error_size > 0) error[0] = '\0';
    return true;
}
