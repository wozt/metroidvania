/* SPDX-License-Identifier: GPL-3.0-only */
#include "fusion/pipeline/rebuild.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_registered_graph(void) {
    size_t count = 0;
    const FusionRebuildTask *tasks = fusion_rebuild_tasks(&count);
    assert(tasks != NULL);
    assert(count == 15);
    assert(fusion_rebuild_find_task("aria_rooms") != NULL);
    assert(fusion_rebuild_find_task("missing") == NULL);

    const char *selected[] = {"raw_import", "checklist", "aria_objects"};
    FusionRebuildOrder order;
    char error[256] = "";
    assert(fusion_rebuild_order_registered(selected, 3, &order,
                                           error, sizeof error));
    const char *expected[] = {
        "aria_rom", "aria_objects", "inventory", "checklist",
        "metroid_rom", "raw_import",
    };
    assert(order.count == sizeof expected / sizeof expected[0]);
    for (size_t index = 0; index < order.count; ++index)
        assert(!strcmp(order.items[index]->name, expected[index]));
    fusion_rebuild_order_free(&order);
}

static void test_unknown_and_cycle(void) {
    char error[256] = "";
    FusionRebuildOrder order;
    const char *missing[] = {"missing"};
    assert(!fusion_rebuild_order_registered(missing, 1, &order,
                                            error, sizeof error));
    assert(strstr(error, "unknown rebuild task") != NULL);

    const char *a_dependencies[] = {"b"};
    const char *b_dependencies[] = {"a"};
    const FusionRebuildTask cyclic[] = {
        {"a", 1, FUSION_REBUILD_GROUP_NATIVE, FUSION_REBUILD_BACKEND_C,
         a_dependencies, 1, NULL, 0},
        {"b", 1, FUSION_REBUILD_GROUP_NATIVE, FUSION_REBUILD_BACKEND_C,
         b_dependencies, 1, NULL, 0},
    };
    const char *a[] = {"a"};
    error[0] = '\0';
    assert(!fusion_rebuild_order_tasks(cyclic, 2, a, 1, &order,
                                       error, sizeof error));
    assert(!strcmp(error, "rebuild task dependency cycle at a"));
}

static void test_unknown_dependency(void) {
    const char *dependencies[] = {"absent"};
    const FusionRebuildTask tasks[] = {
        {"task", 1, FUSION_REBUILD_GROUP_NATIVE, FUSION_REBUILD_BACKEND_C,
         dependencies, 1, NULL, 0},
    };
    const char *selected[] = {"task"};
    FusionRebuildOrder order;
    char error[256] = "";
    assert(!fusion_rebuild_order_tasks(tasks, 1, selected, 1, &order,
                                       error, sizeof error));
    assert(!strcmp(error, "task: unknown dependency: absent"));
}

static void test_duplicate_selection_is_stable(void) {
    const char *selected[] = {"checklist", "checklist", "inventory"};
    FusionRebuildOrder order;
    char error[256] = "";
    assert(fusion_rebuild_order_registered(selected, 3, &order,
                                           error, sizeof error));
    assert(order.count == 2);
    assert(!strcmp(order.items[0]->name, "inventory"));
    assert(!strcmp(order.items[1]->name, "checklist"));
    fusion_rebuild_order_free(&order);
}

int main(void) {
    test_registered_graph();
    test_unknown_and_cycle();
    test_unknown_dependency();
    test_duplicate_selection_is_stable();
    puts("fusion rebuild graph tests passed");
    return 0;
}
