/* SPDX-License-Identifier: GPL-3.0-only */
/* The private Zero Mission ROM bank (scripts/mzm_rom_bank.py): the ROM bytes
 * by GBA address and the data symbols of the reference build by name, read
 * by the native screens (HUD, minimap, menus, cutscenes). Also the BIOS
 * decompression routines they use (LZ77UnCompVram/Wram, RLUnCompVram). */
#ifndef MZM_ROM_H
#define MZM_ROM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool mzm_rom_open(const char *bank_path, const char *symbols_path);
/* A bank in memory (tests): `bytes` are copied, at GBA address `base`. */
bool mzm_rom_open_memory(uint32_t base, const uint8_t *bytes, uint32_t size);
/* Adds a symbol (tests; the bank's file lists them otherwise). */
bool mzm_rom_add_symbol(const char *name, uint32_t address, uint32_t size);
void mzm_rom_close(void);
bool mzm_rom_ready(void);

/* `size` bytes at a GBA address, NULL when outside the bank. */
const uint8_t *mzm_rom_ptr(uint32_t address, uint32_t size);
/* The bytes from a GBA address to the end of the bank (NULL outside it). */
const uint8_t *mzm_rom_span(uint32_t address, uint32_t *available);
/* A symbol's address (0: unknown) and size. */
uint32_t mzm_rom_symbol(const char *name, uint32_t *size);
/* A symbol's bytes (NULL: unknown), its size in `size`. */
const uint8_t *mzm_rom_data(const char *name, uint32_t *size);
uint8_t mzm_rom_u8(uint32_t address);
uint16_t mzm_rom_u16(uint32_t address);
uint32_t mzm_rom_u32(uint32_t address);

/* The decompressed size of a BIOS LZ77 or run-length stream (0: not one). */
uint32_t gba_uncompressed_size(const uint8_t *src, size_t src_size);
/* LZ77UnCompWram / LZ77UnCompVram (type 0x10): the bytes written, or -1 on
 * a truncated stream or a too small destination. */
long gba_lz77_uncompress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);
/* RLUnCompWram / RLUnCompVram (type 0x30) */
long gba_rl_uncompress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size);

#endif
