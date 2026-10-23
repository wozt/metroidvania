/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/assets/gba.h"

#include <string.h>

void fusion_binary_reader_init(FusionBinaryReader *reader, const uint8_t *data,
                               size_t size) {
    if (!reader) return;
    *reader = (FusionBinaryReader){data, size, 0};
}

size_t fusion_binary_remaining(const FusionBinaryReader *reader) {
    return reader && reader->offset <= reader->size ? reader->size - reader->offset : 0;
}

bool fusion_binary_read_u8(FusionBinaryReader *reader, uint8_t *value) {
    if (!reader || !value || fusion_binary_remaining(reader) < 1) return false;
    *value = reader->data[reader->offset++];
    return true;
}

bool fusion_binary_read_u16(FusionBinaryReader *reader, uint16_t *value) {
    if (!reader || !value || fusion_binary_remaining(reader) < 2) return false;
    size_t offset = reader->offset;
    *value = (uint16_t)reader->data[offset] | (uint16_t)reader->data[offset + 1] << 8;
    reader->offset += 2;
    return true;
}

bool fusion_binary_read_u32(FusionBinaryReader *reader, uint32_t *value) {
    if (!reader || !value || fusion_binary_remaining(reader) < 4) return false;
    size_t offset = reader->offset;
    *value = (uint32_t)reader->data[offset] | (uint32_t)reader->data[offset + 1] << 8 |
             (uint32_t)reader->data[offset + 2] << 16 |
             (uint32_t)reader->data[offset + 3] << 24;
    reader->offset += 4;
    return true;
}

bool fusion_binary_slice(FusionBinaryReader *reader, size_t size, const uint8_t **data) {
    if (!reader || !data || fusion_binary_remaining(reader) < size) return false;
    *data = reader->data + reader->offset;
    reader->offset += size;
    return true;
}

uint32_t fusion_gba_uncompressed_size(const uint8_t *src, size_t src_size) {
    if (!src || src_size < 4 || (src[0] != 0x10 && src[0] != 0x30)) return 0;
    return (uint32_t)src[1] | (uint32_t)src[2] << 8 | (uint32_t)src[3] << 16;
}

static bool output_size(uint32_t total, uint8_t *dst, size_t dst_size, size_t *written) {
    if (!dst || total == 0 || total > dst_size) return false;
    if (written) *written = 0;
    return true;
}

bool fusion_gba_lz77_uncompress(const uint8_t *src, size_t src_size, uint8_t *dst,
                                size_t dst_size, size_t *written) {
    if (!src || src_size < 4 || src[0] != 0x10) return false;
    uint32_t total = fusion_gba_uncompressed_size(src, src_size);
    if (!output_size(total, dst, dst_size, written)) return false;
    size_t in = 4, out = 0;
    while (out < total) {
        if (in >= src_size) return false;
        uint8_t flags = src[in++];
        for (int bit = 7; bit >= 0 && out < total; --bit) {
            if (!(flags & (uint8_t)(1u << bit))) {
                if (in >= src_size) return false;
                dst[out++] = src[in++];
                continue;
            }
            if (src_size - in < 2) return false;
            size_t length = (size_t)(src[in] >> 4) + 3;
            size_t distance = ((size_t)(src[in] & 0x0F) << 8 | src[in + 1]) + 1;
            in += 2;
            if (distance > out || length > (size_t)total - out) return false;
            for (size_t i = 0; i < length; ++i) dst[out] = dst[out - distance], ++out;
        }
    }
    if (written) *written = out;
    return true;
}

bool fusion_gba_rl_uncompress(const uint8_t *src, size_t src_size, uint8_t *dst,
                              size_t dst_size, size_t *written) {
    if (!src || src_size < 4 || src[0] != 0x30) return false;
    uint32_t total = fusion_gba_uncompressed_size(src, src_size);
    if (!output_size(total, dst, dst_size, written)) return false;
    size_t in = 4, out = 0;
    while (out < total) {
        if (in >= src_size) return false;
        uint8_t control = src[in++];
        bool repeat = (control & 0x80) != 0;
        size_t length = repeat ? (size_t)(control & 0x7F) + 3 : (size_t)control + 1;
        if (length > (size_t)total - out) return false;
        if (repeat) {
            if (in >= src_size) return false;
            memset(dst + out, src[in++], length);
        } else {
            if (src_size - in < length) return false;
            memcpy(dst + out, src + in, length);
            in += length;
        }
        out += length;
    }
    if (written) *written = out;
    return true;
}
