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

typedef struct {
    size_t routines, declarations, data_symbols, call_edges;
} FusionInventorySummary;

/* The complete v6 inventory of both pinned trees below ``root``
 * (formerly scripts/native_inventory.py); with ``write`` it is written to
 * ``output`` and its sibling record shards. */
bool fusion_inventory_build(const char *root, const char *output, bool write,
                            FusionInventorySummary summary[2], char *error, size_t error_size);

#endif /* FUSION_PIPELINE_INVENTORY_H */
