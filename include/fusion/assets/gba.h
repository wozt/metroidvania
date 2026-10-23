/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounds-checked GBA binary and BIOS compression primitives. */
#ifndef FUSION_ASSETS_GBA_H
#define FUSION_ASSETS_GBA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *data;
    size_t size;
    size_t offset;
} FusionBinaryReader;

void fusion_binary_reader_init(FusionBinaryReader *reader, const uint8_t *data,
                               size_t size);
size_t fusion_binary_remaining(const FusionBinaryReader *reader);
bool fusion_binary_read_u8(FusionBinaryReader *reader, uint8_t *value);
bool fusion_binary_read_u16(FusionBinaryReader *reader, uint16_t *value);
bool fusion_binary_read_u32(FusionBinaryReader *reader, uint32_t *value);
bool fusion_binary_slice(FusionBinaryReader *reader, size_t size,
                         const uint8_t **data);

uint32_t fusion_gba_uncompressed_size(const uint8_t *src, size_t src_size);
bool fusion_gba_lz77_uncompress(const uint8_t *src, size_t src_size, uint8_t *dst,
                                size_t dst_size, size_t *written);
bool fusion_gba_rl_uncompress(const uint8_t *src, size_t src_size, uint8_t *dst,
                              size_t dst_size, size_t *written);

#endif /* FUSION_ASSETS_GBA_H */
