/* SPDX-License-Identifier: GPL-3.0-only */
/* Native entry point for the incremental reconstruction pipeline. */
#include "fusion/pipeline/rebuild.h"

#include <stdio.h>
#include <string.h>

static void usage(const char *program) {
    fprintf(stderr,
            "Usage: %s --list\n"
            "       %s --task NAME [--task NAME ...]\n"
            "       %s --all|--assets|--native|--inventory|--checklist\n",
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
        if (task->backend == FUSION_REBUILD_BACKEND_PYTHON_TEMPORARY)
            printf(" backend=python-temporary");
        putchar('\n');
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--list")) return list_tasks();
    usage(argv[0]);
    fprintf(stderr, "fusion_rebuild: task execution is not implemented yet\n");
    return argc > 1 ? 3 : 2;
}
