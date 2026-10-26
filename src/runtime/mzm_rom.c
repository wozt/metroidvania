/* SPDX-License-Identifier: GPL-3.0-only */
#include "mzm_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    uint32_t address, size;
} Symbol;

static uint8_t *g_bytes;
static uint32_t g_base, g_size;
static Symbol *g_symbols;
static size_t g_count, g_capacity;
static bool g_sorted;

void mzm_rom_close(void) {
    free(g_bytes);
    g_bytes = NULL;
    g_base = g_size = 0;
    for (size_t i = 0; i < g_count; ++i) free(g_symbols[i].name);
    free(g_symbols);
    g_symbols = NULL;
    g_count = g_capacity = 0;
}

bool mzm_rom_ready(void) { return g_bytes != NULL; }

bool mzm_rom_open_memory(uint32_t base, const uint8_t *bytes, uint32_t size) {
    mzm_rom_close();
    g_bytes = malloc(size ? size : 1);
    if (!g_bytes) return false;
    memcpy(g_bytes, bytes, size);
    g_base = base;
    g_size = size;
    return true;
}

bool mzm_rom_add_symbol(const char *name, uint32_t address, uint32_t size) {
    if (g_count == g_capacity) {
        size_t capacity = g_capacity ? g_capacity * 2 : 1024;
        Symbol *grown = realloc(g_symbols, capacity * sizeof *grown);
        if (!grown) return false;
        g_symbols = grown;
        g_capacity = capacity;
    }
    size_t length = strlen(name);
    char *copy = malloc(length + 1);
    if (!copy) return false;
    memcpy(copy, name, length + 1);
    g_symbols[g_count++] = (Symbol){copy, address, size};
    g_sorted = false;
    return true;
}

bool mzm_rom_open(const char *bank_path, const char *symbols_path) {
    mzm_rom_close();
    FILE *f = fopen(bank_path, "rb");
    if (!f) {
        fprintf(stderr, "No Zero Mission ROM bank: %s "
                        "(run ./build/fusion_rebuild --task metroid_rom_bank)\n",
                bank_path);
        return false;
    }
    uint8_t header[16];
    bool ok = fread(header, 1, 16, f) == 16 && !memcmp(header, "MZMROM01", 8);
    uint32_t base = 0, size = 0;
    if (ok) {
        base = (uint32_t)header[8] | (uint32_t)header[9] << 8 | (uint32_t)header[10] << 16 |
               (uint32_t)header[11] << 24;
        size = (uint32_t)header[12] | (uint32_t)header[13] << 8 | (uint32_t)header[14] << 16 |
               (uint32_t)header[15] << 24;
        g_bytes = malloc(size ? size : 1);
        ok = g_bytes && fread(g_bytes, 1, size, f) == size;
    }
    fclose(f);
    if (!ok) {
        fprintf(stderr, "Invalid Zero Mission ROM bank: %s\n", bank_path);
        mzm_rom_close();
        return false;
    }
    g_base = base;
    g_size = size;
    f = fopen(symbols_path, "r");
    if (!f) {
        fprintf(stderr, "No Zero Mission ROM symbols: %s\n", symbols_path);
        mzm_rom_close();
        return false;
    }
    char line[512];
    while (ok && fgets(line, sizeof line, f)) {
        char name[256];
        unsigned long address, length;
        if (strncmp(line, "symbol\t", 7)) continue;
        if (sscanf(line + 7, "%255s %lu %lu", name, &address, &length) != 3) ok = false;
        else ok = mzm_rom_add_symbol(name, (uint32_t)address, (uint32_t)length);
    }
    fclose(f);
    if (!ok) {
        fprintf(stderr, "Invalid Zero Mission ROM symbols: %s\n", symbols_path);
        mzm_rom_close();
    }
    return ok;
}

const uint8_t *mzm_rom_ptr(uint32_t address, uint32_t size) {
    if (!g_bytes || address < g_base || address - g_base > g_size || size > g_size - (address - g_base))
        return NULL;
    return g_bytes + (address - g_base);
}

const uint8_t *mzm_rom_span(uint32_t address, uint32_t *available) {
    if (!g_bytes || address < g_base || address - g_base >= g_size) {
        if (available) *available = 0;
        return NULL;
    }
    if (available) *available = g_size - (address - g_base);
    return g_bytes + (address - g_base);
}

static int by_name(const void *a, const void *b) {
    return strcmp(((const Symbol *)a)->name, ((const Symbol *)b)->name);
}

uint32_t mzm_rom_symbol(const char *name, uint32_t *size) {
    if (!g_sorted && g_count) {
        qsort(g_symbols, g_count, sizeof *g_symbols, by_name);
        g_sorted = true;
    }
    Symbol key = {(char *)name, 0, 0};
    const Symbol *found = g_count ? bsearch(&key, g_symbols, g_count, sizeof *g_symbols, by_name) : NULL;
    if (size) *size = found ? found->size : 0;
    return found ? found->address : 0;
}

const uint8_t *mzm_rom_data(const char *name, uint32_t *size) {
    uint32_t length;
    uint32_t address = mzm_rom_symbol(name, &length);
    if (size) *size = length;
    return address ? mzm_rom_ptr(address, length) : NULL;
}

uint8_t mzm_rom_u8(uint32_t address) {
    const uint8_t *p = mzm_rom_ptr(address, 1);
    return p ? p[0] : 0;
}

uint16_t mzm_rom_u16(uint32_t address) {
    const uint8_t *p = mzm_rom_ptr(address, 2);
    return p ? (uint16_t)(p[0] | p[1] << 8) : 0;
}

uint32_t mzm_rom_u32(uint32_t address) {
    const uint8_t *p = mzm_rom_ptr(address, 4);
    return p ? (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24 : 0;
}

uint32_t gba_uncompressed_size(const uint8_t *src, size_t src_size) {
    if (!src || src_size < 4 || (src[0] != 0x10 && src[0] != 0x30)) return 0;
    return (uint32_t)src[1] | (uint32_t)src[2] << 8 | (uint32_t)src[3] << 16;
}

long gba_lz77_uncompress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size) {
    if (!src || src_size < 4 || src[0] != 0x10) return -1;
    size_t total = gba_uncompressed_size(src, src_size), in = 4, out = 0;
    if (total > dst_size) return -1;
    while (out < total) {
        if (in >= src_size) return -1;
        uint8_t flags = src[in++];
        for (int bit = 7; bit >= 0 && out < total; --bit) {
            if (!(flags >> bit & 1)) {
                if (in >= src_size) return -1;
                dst[out++] = src[in++];
                continue;
            }
            if (in + 1 >= src_size) return -1;
            size_t length = (size_t)(src[in] >> 4) + 3;
            size_t distance = ((size_t)(src[in] & 15) << 8 | src[in + 1]) + 1;
            in += 2;
            if (distance > out) return -1;
            for (size_t i = 0; i < length && out < total; ++i, ++out) dst[out] = dst[out - distance];
        }
    }
    return (long)total;
}

long gba_rl_uncompress(const uint8_t *src, size_t src_size, uint8_t *dst, size_t dst_size) {
    if (!src || src_size < 4 || src[0] != 0x30) return -1;
    size_t total = gba_uncompressed_size(src, src_size), in = 4, out = 0;
    if (total > dst_size) return -1;
    while (out < total) {
        if (in >= src_size) return -1;
        uint8_t flag = src[in++];
        if (flag & 0x80) {
            size_t length = (size_t)(flag & 0x7F) + 3;
            if (in >= src_size) return -1;
            uint8_t value = src[in++];
            for (size_t i = 0; i < length && out < total; ++i) dst[out++] = value;
        } else {
            size_t length = (size_t)flag + 1;
            for (size_t i = 0; i < length && out < total; ++i) {
                if (in >= src_size) return -1;
                dst[out++] = src[in++];
            }
        }
    }
    return (long)total;
}
