/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ASSETS_SHA256_H
#define FUSION_ASSETS_SHA256_H

#include <stddef.h>
#include <stdint.h>

void fusion_sha256_digest(const uint8_t *data, size_t size, uint8_t digest[32]);

#endif /* FUSION_ASSETS_SHA256_H */
