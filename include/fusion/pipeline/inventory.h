/* SPDX-License-Identifier: GPL-3.0-only */
/* Native lexical discovery used by the reconstruction inventory. */
#ifndef FUSION_PIPELINE_INVENTORY_H
#define FUSION_PIPELINE_INVENTORY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *id;
    char *symbol;
    char *kind;
    char *source;
    size_t line;
    char *linkage;
    char *description;
    char *address;
    size_t body_start;
    char **raw_calls;
    size_t raw_call_count;
    char **source_variants;
    size_t source_variant_count;
} FusionInventoryRoutine;

typedef struct {
    FusionInventoryRoutine *items;
    size_t count;
    size_t capacity;
} FusionInventoryRoutines;

bool fusion_inventory_scan_routines(const char *game, const char *source_root,
                                    FusionInventoryRoutines *routines,
                                    char *error, size_t error_size);
void fusion_inventory_routines_free(FusionInventoryRoutines *routines);

/* Compare every discovered routine identity and source metadata field with
 * the tracked v6 routine fragments. This is deliberately an audit, not yet
 * the production inventory writer. */
bool fusion_inventory_audit_routines(const char *root, const char *manifest_path,
                                     size_t counts[2],
                                     char *error, size_t error_size);

#endif /* FUSION_PIPELINE_INVENTORY_H */
