/* SPDX-License-Identifier: GPL-3.0-only */
/* Native reconstruction task registry and dependency graph. */
#ifndef FUSION_PIPELINE_REBUILD_H
#define FUSION_PIPELINE_REBUILD_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    FUSION_REBUILD_GROUP_NATIVE,
    FUSION_REBUILD_GROUP_ASSETS
} FusionRebuildGroup;

typedef enum {
    FUSION_REBUILD_BACKEND_C,
    FUSION_REBUILD_BACKEND_PYTHON_TEMPORARY
} FusionRebuildBackend;

typedef struct {
    const char *name;
    unsigned version;
    FusionRebuildGroup group;
    FusionRebuildBackend backend;
    const char *const *dependencies;
    size_t dependency_count;
    const char *const *requirements;
    size_t requirement_count;
} FusionRebuildTask;

typedef struct {
    const FusionRebuildTask **items;
    size_t count;
} FusionRebuildOrder;

const FusionRebuildTask *fusion_rebuild_tasks(size_t *count);
const FusionRebuildTask *fusion_rebuild_find_task(const char *name);
const char *fusion_rebuild_group_name(FusionRebuildGroup group);

/* Resolves SELECTED and every transitive dependency in deterministic order.
 * Unknown dependencies and cycles are rejected. The caller releases ITEMS
 * with fusion_rebuild_order_free(). */
bool fusion_rebuild_order_tasks(const FusionRebuildTask *tasks,
                                size_t task_count,
                                const char *const *selected,
                                size_t selected_count,
                                FusionRebuildOrder *order,
                                char *error, size_t error_size);
bool fusion_rebuild_order_registered(const char *const *selected,
                                     size_t selected_count,
                                     FusionRebuildOrder *order,
                                     char *error, size_t error_size);
void fusion_rebuild_order_free(FusionRebuildOrder *order);

#endif /* FUSION_PIPELINE_REBUILD_H */
