/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ASSETS_CORE_OUTPUT_H
#define FUSION_ASSETS_CORE_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef bool (*FusionAssetWriteBody)(FILE *file, const void *context);

bool fusion_asset_write_atomic(const char *path, FusionAssetWriteBody write_body,
                               const void *context,
                               char *error, size_t error_size);

#endif /* FUSION_ASSETS_CORE_OUTPUT_H */
