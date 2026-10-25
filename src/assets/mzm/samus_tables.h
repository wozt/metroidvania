/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FUSION_ASSETS_MZM_SAMUS_TABLES_H
#define FUSION_ASSETS_MZM_SAMUS_TABLES_H

#include <stddef.h>

typedef struct {
    const char *selector;
    size_t count;
    const char *symbols[2];
} FusionMzmSamusTableRow;

typedef struct {
    const char *name;
    const FusionMzmSamusTableRow *rows;
    size_t count;
} FusionMzmSamusTable;

extern const FusionMzmSamusTable fusion_mzm_samus_tables[];
extern const size_t fusion_mzm_samus_table_count;
extern const char *const fusion_mzm_projectile_frame_tables[];
extern const size_t fusion_mzm_projectile_frame_table_count;

#endif /* FUSION_ASSETS_MZM_SAMUS_TABLES_H */
