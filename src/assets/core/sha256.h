/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ASSETS_SHA256_H
#define FUSION_ASSETS_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint8_t block[64];
    size_t block_size;
    uint64_t byte_count;
} FusionSha256;

void fusion_sha256_init(FusionSha256 *context);
void fusion_sha256_update(FusionSha256 *context, const uint8_t *data, size_t size);
void fusion_sha256_final(FusionSha256 *context, uint8_t digest[32]);
void fusion_sha256_digest(const uint8_t *data, size_t size, uint8_t digest[32]);

#endif /* FUSION_ASSETS_SHA256_H */
