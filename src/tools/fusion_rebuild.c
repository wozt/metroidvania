/* SPDX-License-Identifier: GPL-3.0-only */
/* Native entry point for the incremental reconstruction pipeline. */
#include "fusion/pipeline/rebuild.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FUSION_SOURCE_DIR
#define FUSION_SOURCE_DIR "."
#endif

typedef enum {
    SELECT_ALL,
    SELECT_ASSETS,
    SELECT_NATIVE,
    SELECT_INVENTORY,
    SELECT_CHECKLIST,
    SELECT_TASKS
} Selection;

static void usage(FILE *file, const char *program) {
    fprintf(file,
            "Usage: %s --list\n"
            "       %s [--all|--assets|--native|--inventory|--checklist]\n"
            "          [--dry-run] [--force] [--verbose] [--state PATH]\n"
            "       %s --task NAME [--task NAME ...] [--dry-run] [--force]\n"
            "          [--verbose] [--state PATH]\n",
            program, program, program);
}

static int list_tasks(void) {
    size_t count = 0;
    const FusionRebuildTask *tasks = fusion_rebuild_tasks(&count);
    for (size_t index = 0; index < count; ++index) {
        const FusionRebuildTask *task = &tasks[index];
        printf("%s: group=%s dependencies=", task->name,
               fusion_rebuild_group_name(task->group));
        if (!task->dependency_count) printf("none");
        for (size_t dependency = 0; dependency < task->dependency_count; ++dependency)
            printf("%s%s", dependency ? ", " : "", task->dependencies[dependency]);
        if (task->requirement_count) {
            printf(" requires=");
            for (size_t requirement = 0; requirement < task->requirement_count; ++requirement)
                printf("%s%s", requirement ? ", " : "", task->requirements[requirement]);
        }
        putchar('\n');
    }
    return 0;
}

static bool set_selection(Selection *selection, bool *explicit_selection,
                          Selection value) {
    if (*explicit_selection && *selection != value) return false;
    *selection = value;
    *explicit_selection = true;
    return true;
}

int main(int argc, char **argv) {
    bool list = false, dry_run = false, force = false, verbose = false;
    bool explicit_selection = false;
    Selection selection = SELECT_ALL;
    const char *named[64];
    size_t named_count = 0;
    const char *state_path = ".cache/rebuild/state.json";
    for (int index = 1; index < argc; ++index) {
        if (!strcmp(argv[index], "--help")) {
            usage(stdout, argv[0]);
            return 0;
        } else if (!strcmp(argv[index], "--list")) list = true;
        else if (!strcmp(argv[index], "--dry-run")) dry_run = true;
        else if (!strcmp(argv[index], "--force")) force = true;
        else if (!strcmp(argv[index], "--verbose")) verbose = true;
        else if (!strcmp(argv[index], "--all")) {
            if (!set_selection(&selection, &explicit_selection, SELECT_ALL)) goto selection_error;
        } else if (!strcmp(argv[index], "--assets")) {
            if (!set_selection(&selection, &explicit_selection, SELECT_ASSETS)) goto selection_error;
        } else if (!strcmp(argv[index], "--native")) {
            if (!set_selection(&selection, &explicit_selection, SELECT_NATIVE)) goto selection_error;
        } else if (!strcmp(argv[index], "--inventory")) {
            if (!set_selection(&selection, &explicit_selection, SELECT_INVENTORY)) goto selection_error;
        } else if (!strcmp(argv[index], "--checklist")) {
            if (!set_selection(&selection, &explicit_selection, SELECT_CHECKLIST)) goto selection_error;
        } else if (!strcmp(argv[index], "--task") && index + 1 < argc) {
            if (!set_selection(&selection, &explicit_selection, SELECT_TASKS) ||
                named_count == sizeof named / sizeof named[0])
                goto selection_error;
            named[named_count++] = argv[++index];
        } else if (!strcmp(argv[index], "--state") && index + 1 < argc) {
            state_path = argv[++index];
        } else {
            usage(stderr, argv[0]);
            fprintf(stderr, "fusion_rebuild: unrecognized argument: %s\n", argv[index]);
            return 2;
        }
    }
    if (list) return list_tasks();
    size_t task_count = 0;
    const FusionRebuildTask *tasks = fusion_rebuild_tasks(&task_count);
    const char **selected = calloc(task_count, sizeof *selected);
    if (!selected) {
        fprintf(stderr, "fusion_rebuild: out of memory\n");
        return 1;
    }
    size_t selected_count = 0;
    if (selection == SELECT_TASKS) {
        for (size_t index = 0; index < named_count; ++index)
            selected[selected_count++] = named[index];
    } else {
        for (size_t index = 0; index < task_count; ++index) {
            bool include = selection == SELECT_ALL ||
                           (selection == SELECT_ASSETS &&
                            tasks[index].group == FUSION_REBUILD_GROUP_ASSETS) ||
                           (selection == SELECT_NATIVE &&
                            tasks[index].group == FUSION_REBUILD_GROUP_NATIVE) ||
                           (selection == SELECT_INVENTORY &&
                            !strcmp(tasks[index].name, "inventory")) ||
                           (selection == SELECT_CHECKLIST &&
                            !strcmp(tasks[index].name, "checklist"));
            if (include) selected[selected_count++] = tasks[index].name;
        }
    }
    char error[512] = "";
    FusionRebuildPlan plan;
    bool ok = dry_run
                  ? fusion_rebuild_plan(FUSION_SOURCE_DIR, state_path,
                                        selected, selected_count, force,
                                        &plan, error, sizeof error)
                  : fusion_rebuild_execute(FUSION_SOURCE_DIR, state_path,
                                           selected, selected_count, force,
                                           &plan, error, sizeof error);
    free(selected);
    if (!ok) {
        fprintf(stderr, "fusion_rebuild: %s\n", error);
        return 2;
    }
    for (size_t index = 0; index < plan.count; ++index) {
        const FusionRebuildPlanEntry *entry = &plan.entries[index];
        printf("%s: %s", entry->task->name,
               fusion_rebuild_plan_status_name(entry->status));
        if ((entry->status == FUSION_REBUILD_SKIPPED_MISSING ||
             entry->status == FUSION_REBUILD_SKIPPED_DEPENDENCY) && entry->detail[0])
            printf(" (%s)", entry->detail);
        putchar('\n');
        if (verbose && entry->input_digest[0])
            printf("  inputs: %zu, digest: %s\n", entry->input_count,
                   entry->input_digest);
        if (verbose && entry->output_digest[0])
            printf("  outputs: %zu, digest: %s\n", entry->output_count,
                   entry->output_digest);
    }
    fusion_rebuild_plan_free(&plan);
    return 0;

selection_error:
    usage(stderr, argv[0]);
    fprintf(stderr, "fusion_rebuild: task selections are mutually exclusive\n");
    return 2;
}
