/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/mzm.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum {
    ELF_SIZE = 0x240,
    SECTION_OFFSET = 0x40,
    STRING_OFFSET = 0x100,
    SYMBOL_OFFSET = 0x160,
    ROM_SIZE = 0x100
};

static void write_u16(uint8_t *data, size_t offset, uint16_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
}

static void write_u32(uint8_t *data, size_t offset, uint32_t value) {
    data[offset] = (uint8_t)value;
    data[offset + 1] = (uint8_t)(value >> 8);
    data[offset + 2] = (uint8_t)(value >> 16);
    data[offset + 3] = (uint8_t)(value >> 24);
}

static void write_symbol(uint8_t *elf, size_t index, uint32_t name,
                         uint32_t address, uint32_t size, uint8_t kind) {
    size_t offset = SYMBOL_OFFSET + index * 16;
    write_u32(elf, offset, name);
    write_u32(elf, offset + 4, address);
    write_u32(elf, offset + 8, size);
    elf[offset + 12] = kind;
}

static void make_elf(uint8_t elf[ELF_SIZE]) {
    memset(elf, 0, ELF_SIZE);
    memcpy(elf, "\x7f" "ELF", 4);
    elf[4] = 1;
    elf[5] = 1;
    write_u32(elf, 0x20, SECTION_OFFSET);
    write_u16(elf, 0x2E, 40);
    write_u16(elf, 0x30, 3);

    const char names[] = "\0first\0dup\0outside\0zero\0func\0";
    memcpy(elf + STRING_OFFSET, names, sizeof names);
    size_t strings = SECTION_OFFSET + 40;
    write_u32(elf, strings + 4, 3);
    write_u32(elf, strings + 16, STRING_OFFSET);
    write_u32(elf, strings + 20, sizeof names);

    size_t symbols = SECTION_OFFSET + 80;
    write_u32(elf, symbols + 4, 2);
    write_u32(elf, symbols + 16, SYMBOL_OFFSET);
    write_u32(elf, symbols + 20, 6 * 16);
    write_u32(elf, symbols + 24, 1);
    write_u32(elf, symbols + 36, 16);
    write_symbol(elf, 1, 7, 0x08000030, 4, 1);
    write_symbol(elf, 2, 1, 0x08000010, 8, 1);
    write_symbol(elf, 3, 7, 0x08000020, 4, 1);
    write_symbol(elf, 4, 11, 0x09000000, 4, 1);
    write_symbol(elf, 5, 24, 0x08000040, 4, 2);
}

static size_t read_file(const char *path, uint8_t *data, size_t capacity) {
    FILE *file = fopen(path, "rb");
    assert(file);
    size_t size = fread(data, 1, capacity, file);
    assert(ferror(file) == 0);
    assert(fgetc(file) == EOF);
    assert(fclose(file) == 0);
    return size;
}

int main(void) {
    uint8_t elf[ELF_SIZE];
    uint8_t rom_bytes[ROM_SIZE] = {0};
    make_elf(elf);
    FusionRom rom = {rom_bytes, sizeof rom_bytes, FUSION_ASSET_GAME_METROID};
    FusionMzmRomSymbolTable table;
    char error[160];
    assert(fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                       error, sizeof error));
    assert(table.count == 2);
    assert(table.duplicate_name_count == 1);
    assert(!strcmp(table.entries[0].name, "first"));
    assert(table.entries[0].address == 0x08000010 && table.entries[0].size == 8);
    assert(!strcmp(table.entries[1].name, "dup"));
    assert(table.entries[1].address == 0x08000020 && table.entries[1].size == 4);
    const char *bank_path = "fusion-mzm-rom-bank-test.bin";
    const char *manifest_path = "fusion-mzm-rom-symbols-test.tsv";
    remove(bank_path);
    remove(manifest_path);
    assert(fusion_mzm_write_rom_bank(&rom, &table, bank_path, manifest_path,
                                     error, sizeof error));
    uint8_t bank[16 + ROM_SIZE];
    assert(read_file(bank_path, bank, sizeof bank) == sizeof bank);
    assert(!memcmp(bank, "MZMROM01", 8));
    assert(bank[8] == 0 && bank[9] == 0 && bank[10] == 0 && bank[11] == 8);
    assert(bank[12] == 0 && bank[13] == 1 &&
           bank[14] == 0 && bank[15] == 0);
    assert(!memcmp(bank + 16, rom_bytes, ROM_SIZE));
    uint8_t manifest[256];
    size_t manifest_size = read_file(manifest_path, manifest, sizeof manifest - 1);
    manifest[manifest_size] = '\0';
    assert(!strcmp((char *)manifest,
                   "schema\tmetroidvania-mzm-rom-v1\n"
                   "symbol\tfirst\t134217744\t8\n"
                   "symbol\tdup\t134217760\t4\n"));
    assert(remove(bank_path) == 0);
    assert(remove(manifest_path) == 0);
    fusion_mzm_free_symbols(&table);
    assert(table.entries == NULL && table.count == 0);

    elf[0] = 0;
    assert(!fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                        error, sizeof error));
    make_elf(elf);
    write_u32(elf, SECTION_OFFSET + 80 + 20, 15);
    assert(!fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                        error, sizeof error));
    make_elf(elf);
    write_symbol(elf, 1, 7, 0x080000F0, 0x20, 1);
    assert(!fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                        error, sizeof error));
    rom.game = FUSION_ASSET_GAME_ARIA;
    assert(!fusion_mzm_read_rom_symbols(&rom, elf, sizeof elf, &table,
                                        error, sizeof error));
    puts("Native Zero Mission ROM-symbol tests passed");
    return 0;
}
