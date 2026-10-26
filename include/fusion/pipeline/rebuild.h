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

typedef enum {
    FUSION_REBUILD_PATHS_FIXED,
    FUSION_REBUILD_PATHS_INVENTORY,
    FUSION_REBUILD_PATHS_CHECKLIST
} FusionRebuildPathKind;

typedef enum {
    FUSION_REBUILD_VALIDATE_NONE,
    FUSION_REBUILD_VALIDATE_EXPORT_INDEX,
    FUSION_REBUILD_VALIDATE_SPRITE_INDEX
} FusionRebuildValidation;

enum {
    FUSION_REBUILD_STAMP_ARIA = 1u << 0,
    FUSION_REBUILD_STAMP_METROID = 1u << 1
};

typedef struct {
    const char *name;
    unsigned version;
    FusionRebuildGroup group;
    FusionRebuildBackend backend;
    const char *const *dependencies;
    size_t dependency_count;
    const char *const *requirements;
    size_t requirement_count;
    const char *const *inputs;
    size_t input_count;
    const char *const *outputs;
    size_t output_count;
    FusionRebuildPathKind path_kind;
    FusionRebuildValidation validation;
    unsigned stamp_games;
} FusionRebuildTask;

typedef struct {
    const FusionRebuildTask **items;
    size_t count;
} FusionRebuildOrder;

typedef enum {
    FUSION_REBUILD_UP_TO_DATE,
    FUSION_REBUILD_WOULD_REBUILD,
    FUSION_REBUILD_REBUILT,
    FUSION_REBUILD_SKIPPED_MISSING,
    FUSION_REBUILD_SKIPPED_DEPENDENCY
} FusionRebuildPlanStatus;

typedef struct {
    const FusionRebuildTask *task;
    FusionRebuildPlanStatus status;
    char detail[512];
    char input_digest[65];
    char output_digest[65];
    size_t input_count;
    size_t output_count;
} FusionRebuildPlanEntry;

typedef struct {
    FusionRebuildPlanEntry *entries;
    size_t count;
} FusionRebuildPlan;

typedef bool (*FusionRebuildActionRunner)(const char *root,
                                          const FusionRebuildTask *task,
                                          void *user,
                                          char *error, size_t error_size);

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

/* Reads the legacy v1 state without modifying it and computes the same dry-run
 * decisions as scripts/rebuild.py. ROOT is the project root and STATE_PATH
 * may be absolute or relative to it. */
bool fusion_rebuild_plan(const char *root, const char *state_path,
                         const char *const *selected, size_t selected_count,
                         bool force, FusionRebuildPlan *plan,
                         char *error, size_t error_size);
bool fusion_rebuild_plan_tasks(const char *root, const char *state_path,
                               const FusionRebuildTask *tasks, size_t task_count,
                               const char *const *selected, size_t selected_count,
                               bool force, FusionRebuildPlan *plan,
                               char *error, size_t error_size);
/* Executes selected tasks in dependency order and persists each successful
 * result atomically. The injectable runner is intended for tests and native
 * front ends; fusion_rebuild_execute() uses the registered actions. */
bool fusion_rebuild_execute_tasks(const char *root, const char *state_path,
                                  const FusionRebuildTask *tasks, size_t task_count,
                                  const char *const *selected, size_t selected_count,
                                  bool force, FusionRebuildActionRunner runner,
                                  void *runner_user, FusionRebuildPlan *result,
                                  char *error, size_t error_size);
bool fusion_rebuild_execute(const char *root, const char *state_path,
                            const char *const *selected, size_t selected_count,
                            bool force, FusionRebuildPlan *result,
                            char *error, size_t error_size);
const char *fusion_rebuild_plan_status_name(FusionRebuildPlanStatus status);
void fusion_rebuild_plan_free(FusionRebuildPlan *plan);

#endif /* FUSION_PIPELINE_REBUILD_H */
