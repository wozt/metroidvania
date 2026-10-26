/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/pipeline/rebuild.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { UNSEEN, VISITING, ORDERED } VisitState;

typedef struct {
    const FusionRebuildTask *tasks;
    size_t task_count;
    VisitState *states;
    const FusionRebuildTask **items;
    size_t count;
    char *error;
    size_t error_size;
} Graph;

static bool fail(Graph *graph, const char *format, ...) {
    if (graph->error && graph->error_size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(graph->error, graph->error_size, format, arguments);
        va_end(arguments);
        graph->error[graph->error_size - 1] = '\0';
    }
    return false;
}

static ptrdiff_t find(const FusionRebuildTask *tasks, size_t count,
                      const char *name) {
    for (size_t index = 0; index < count; ++index)
        if (!strcmp(tasks[index].name, name)) return (ptrdiff_t)index;
    return -1;
}

static bool visit(Graph *graph, size_t index) {
    if (graph->states[index] == ORDERED) return true;
    if (graph->states[index] == VISITING)
        return fail(graph, "rebuild task dependency cycle at %s",
                    graph->tasks[index].name);
    graph->states[index] = VISITING;
    const FusionRebuildTask *task = &graph->tasks[index];
    for (size_t dependency = 0; dependency < task->dependency_count; ++dependency) {
        const char *name = task->dependencies[dependency];
        ptrdiff_t found = find(graph->tasks, graph->task_count, name);
        if (found < 0)
            return fail(graph, "%s: unknown dependency: %s", task->name, name);
        if (!visit(graph, (size_t)found)) return false;
    }
    graph->states[index] = ORDERED;
    graph->items[graph->count++] = task;
    return true;
}

static int compare_names(const void *left, const void *right) {
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}

bool fusion_rebuild_order_tasks(const FusionRebuildTask *tasks,
                                size_t task_count,
                                const char *const *selected,
                                size_t selected_count,
                                FusionRebuildOrder *order,
                                char *error, size_t error_size) {
    if (order) *order = (FusionRebuildOrder){0};
    if (!tasks || !task_count || !selected || !selected_count || !order) {
        if (error && error_size) snprintf(error, error_size, "no rebuild tasks selected");
        return false;
    }
    VisitState *states = calloc(task_count, sizeof *states);
    const FusionRebuildTask **items = malloc(task_count * sizeof *items);
    const char **names = malloc(selected_count * sizeof *names);
    if (!states || !items || !names) {
        free(states);
        free(items);
        free(names);
        if (error && error_size) snprintf(error, error_size, "out of memory");
        return false;
    }
    memcpy(names, selected, selected_count * sizeof *names);
    qsort(names, selected_count, sizeof *names, compare_names);
    Graph graph = {tasks, task_count, states, items, 0, error, error_size};
    bool ok = true;
    for (size_t selected_index = 0; selected_index < selected_count; ++selected_index) {
        ptrdiff_t index = find(tasks, task_count, names[selected_index]);
        if (index < 0) {
            ok = fail(&graph, "unknown rebuild task: %s", names[selected_index]);
            break;
        }
        if (!visit(&graph, (size_t)index)) {
            ok = false;
            break;
        }
    }
    free(names);
    free(states);
    if (!ok) {
        free(items);
        return false;
    }
    order->items = items;
    order->count = graph.count;
    return true;
}

bool fusion_rebuild_order_registered(const char *const *selected,
                                     size_t selected_count,
                                     FusionRebuildOrder *order,
                                     char *error, size_t error_size) {
    size_t count = 0;
    const FusionRebuildTask *tasks = fusion_rebuild_tasks(&count);
    return fusion_rebuild_order_tasks(tasks, count, selected, selected_count,
                                      order, error, error_size);
}

void fusion_rebuild_order_free(FusionRebuildOrder *order) {
    if (!order) return;
    free(order->items);
    *order = (FusionRebuildOrder){0};
}
