/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "fusion/pipeline/rebuild.h"
#include "fusion/assets/rom.h"
#include "internal.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
        {.name = "a", .version = 1, .group = FUSION_REBUILD_GROUP_NATIVE,
         .dependencies = a_dependencies, .dependency_count = 1},
        {.name = "b", .version = 1, .group = FUSION_REBUILD_GROUP_NATIVE,
         .dependencies = b_dependencies, .dependency_count = 1},
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
        {.name = "task", .version = 1, .group = FUSION_REBUILD_GROUP_NATIVE,
         .dependencies = dependencies, .dependency_count = 1},
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

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(!fclose(file));
}

static void record_entry(FusionRebuildState *state,
                         const FusionRebuildPlanEntry *entry) {
    FusionJson *tasks = fusion_json_member_mut(&state->document, "tasks");
    FusionJson record = fusion_json_object();
    assert(fusion_json_set(&record, "digest",
                           fusion_json_string(entry->input_digest)));
    assert(fusion_json_set(&record, "output_digest",
                           fusion_json_string(entry->output_digest)));
    assert(fusion_json_set(&record, "future_field", fusion_json_int(7)));
    assert(fusion_json_set(tasks, entry->task->name, record));
}

static void test_incremental_plan_and_v1_state(void) {
    char root_template[] = "/tmp/fusion-rebuild-plan-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    char cache[512], input[512], middle[512], final[512], state_path[512];
    snprintf(cache, sizeof cache, "%s/cache", root);
    snprintf(input, sizeof input, "%s/input.txt", root);
    snprintf(middle, sizeof middle, "%s/middle.txt", root);
    snprintf(final, sizeof final, "%s/final.txt", root);
    snprintf(state_path, sizeof state_path, "%s/cache/state.json", root);
    assert(!mkdir(cache, 0700));
    write_text(input, "source");
    write_text(middle, "middle");
    write_text(final, "final");

    const char *first_requirements[] = {"input.txt"};
    const char *first_inputs[] = {"input.txt"};
    const char *first_outputs[] = {"middle.txt"};
    const char *second_dependencies[] = {"first"};
    const char *second_inputs[] = {"middle.txt"};
    const char *second_outputs[] = {"final.txt"};
    const FusionRebuildTask tasks[] = {
        {.name = "first", .version = 1, .group = FUSION_REBUILD_GROUP_NATIVE,
         .requirements = first_requirements, .requirement_count = 1,
         .inputs = first_inputs, .input_count = 1,
         .outputs = first_outputs, .output_count = 1},
        {.name = "second", .version = 3, .group = FUSION_REBUILD_GROUP_NATIVE,
         .dependencies = second_dependencies, .dependency_count = 1,
         .inputs = second_inputs, .input_count = 1,
         .outputs = second_outputs, .output_count = 1},
    };
    const char *first_selection[] = {"first"};
    const char *second_selection[] = {"second"};
    FusionRebuildPlan plan;
    char error[512] = "";
    assert(fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                     first_selection, 1, false,
                                     &plan, error, sizeof error));
    assert(plan.count == 1 && plan.entries[0].status == FUSION_REBUILD_WOULD_REBUILD);
    FusionRebuildState state;
    assert(fusion_rebuild_state_load(state_path, &state, error, sizeof error));
    record_entry(&state, &plan.entries[0]);
    assert(fusion_json_set(&state.document, "future", fusion_json_string("kept")));
    assert(fusion_rebuild_state_save(state_path, &state, error, sizeof error));
    fusion_rebuild_state_free(&state);
    fusion_rebuild_plan_free(&plan);

    assert(fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                     second_selection, 1, false,
                                     &plan, error, sizeof error));
    assert(plan.count == 2);
    assert(plan.entries[0].status == FUSION_REBUILD_UP_TO_DATE);
    assert(plan.entries[1].status == FUSION_REBUILD_WOULD_REBUILD);
    assert(fusion_rebuild_state_load(state_path, &state, error, sizeof error));
    record_entry(&state, &plan.entries[1]);
    assert(fusion_rebuild_state_save(state_path, &state, error, sizeof error));
    fusion_rebuild_state_free(&state);
    fusion_rebuild_plan_free(&plan);

    assert(fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                     second_selection, 1, false,
                                     &plan, error, sizeof error));
    assert(plan.entries[0].status == FUSION_REBUILD_UP_TO_DATE);
    assert(plan.entries[1].status == FUSION_REBUILD_UP_TO_DATE);
    fusion_rebuild_plan_free(&plan);

    write_text(input, "changed");
    assert(fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                     second_selection, 1, false,
                                     &plan, error, sizeof error));
    assert(plan.entries[0].status == FUSION_REBUILD_WOULD_REBUILD);
    assert(plan.entries[1].status == FUSION_REBUILD_WOULD_REBUILD);
    fusion_rebuild_plan_free(&plan);
    write_text(input, "source");

    write_text(middle, "tampered");
    assert(fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                     second_selection, 1, false,
                                     &plan, error, sizeof error));
    assert(plan.entries[0].status == FUSION_REBUILD_WOULD_REBUILD);
    assert(plan.entries[1].status == FUSION_REBUILD_WOULD_REBUILD);
    fusion_rebuild_plan_free(&plan);
    write_text(middle, "middle");

    assert(fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                     second_selection, 1, true,
                                     &plan, error, sizeof error));
    assert(plan.entries[0].status == FUSION_REBUILD_WOULD_REBUILD);
    assert(plan.entries[1].status == FUSION_REBUILD_WOULD_REBUILD);
    fusion_rebuild_plan_free(&plan);

    assert(!unlink(input));
    assert(fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                     second_selection, 1, false,
                                     &plan, error, sizeof error));
    assert(plan.entries[0].status == FUSION_REBUILD_SKIPPED_MISSING);
    assert(plan.entries[1].status == FUSION_REBUILD_SKIPPED_DEPENDENCY);
    fusion_rebuild_plan_free(&plan);

    assert(fusion_rebuild_state_load(state_path, &state, error, sizeof error));
    const FusionJson *future = fusion_json_member(&state.document, "future");
    const FusionJson *saved_tasks = fusion_json_member(&state.document, "tasks");
    const FusionJson *first_record = fusion_json_member(saved_tasks, "first");
    assert(future && future->kind == FUSION_JSON_STRING &&
           !strcmp(future->string, "kept"));
    assert(fusion_json_member(first_record, "future_field") != NULL);
    fusion_rebuild_state_free(&state);

    write_text(state_path, "not json");
    assert(!fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                      first_selection, 1, false,
                                      &plan, error, sizeof error));
    assert(strstr(error, "cannot read rebuild state") != NULL);

    assert(!unlink(state_path));
    assert(!symlink(middle, state_path));
    assert(!fusion_rebuild_plan_tasks(root, state_path, tasks, 2,
                                      first_selection, 1, false,
                                      &plan, error, sizeof error));
    assert(strstr(error, "not a symlink") != NULL);
    assert(!unlink(state_path));
    assert(fusion_rebuild_state_load(state_path, &state, error, sizeof error));
    char temporary_state[520];
    snprintf(temporary_state, sizeof temporary_state, "%s.tmp", state_path);
    assert(!symlink(middle, temporary_state));
    assert(!fusion_rebuild_state_save(state_path, &state, error, sizeof error));
    assert(strstr(error, "temporary state output") != NULL);
    fusion_rebuild_state_free(&state);
    assert(!unlink(temporary_state));
    assert(!unlink(middle));
    assert(!unlink(final));
    assert(!rmdir(cache));
    assert(!rmdir(root));
}

static void test_export_index_validation(void) {
    char root_template[] = "/tmp/fusion-rebuild-index-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    char folder[512], payload[512], index[512], state_path[512];
    snprintf(folder, sizeof folder, "%s/export", root);
    snprintf(payload, sizeof payload, "%s/export/item.bin", root);
    snprintf(index, sizeof index, "%s/export/export_index.tsv", root);
    snprintf(state_path, sizeof state_path, "%s/state.json", root);
    assert(!mkdir(folder, 0700));
    write_text(payload, "payload");
    char hash[65];
    assert(fusion_sha256_hex((const uint8_t *)"payload", 7, hash));
    char contents[256];
    snprintf(contents, sizeof contents,
             "schema\tmetroidvania-export-index-v1\nfile\titem.bin\t%s\n", hash);
    write_text(index, contents);
    const char *outputs[] = {"export/export_index.tsv"};
    const FusionRebuildTask task = {
        .name = "export", .version = 1, .group = FUSION_REBUILD_GROUP_ASSETS,
        .outputs = outputs, .output_count = 1,
        .validation = FUSION_REBUILD_VALIDATE_EXPORT_INDEX,
    };
    const char *selected[] = {"export"};
    FusionRebuildPlan plan;
    char error[512] = "";
    assert(fusion_rebuild_plan_tasks(root, state_path, &task, 1,
                                     selected, 1, false,
                                     &plan, error, sizeof error));
    FusionRebuildState state;
    assert(fusion_rebuild_state_load(state_path, &state, error, sizeof error));
    record_entry(&state, &plan.entries[0]);
    assert(fusion_rebuild_state_save(state_path, &state, error, sizeof error));
    fusion_rebuild_state_free(&state);
    fusion_rebuild_plan_free(&plan);
    assert(fusion_rebuild_plan_tasks(root, state_path, &task, 1,
                                     selected, 1, false,
                                     &plan, error, sizeof error));
    assert(plan.entries[0].status == FUSION_REBUILD_UP_TO_DATE);
    fusion_rebuild_plan_free(&plan);
    write_text(payload, "changed");
    assert(fusion_rebuild_plan_tasks(root, state_path, &task, 1,
                                     selected, 1, false,
                                     &plan, error, sizeof error));
    assert(plan.entries[0].status == FUSION_REBUILD_WOULD_REBUILD);
    fusion_rebuild_plan_free(&plan);
    assert(!unlink(state_path));
    assert(!unlink(index));
    assert(!unlink(payload));
    assert(!rmdir(folder));
    assert(!rmdir(root));
}

typedef struct {
    size_t calls;
    const char *fail_name;
} TestRunner;

static bool test_action_runner(const char *root, const FusionRebuildTask *task,
                               void *opaque, char *error, size_t error_size) {
    TestRunner *runner = opaque;
    ++runner->calls;
    if (runner->fail_name && !strcmp(runner->fail_name, task->name)) {
        snprintf(error, error_size, "%s failed deliberately", task->name);
        return false;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/%s", root, task->outputs[0]);
    char contents[128];
    snprintf(contents, sizeof contents, "%s output %zu", task->name, runner->calls);
    write_text(path, contents);
    return true;
}

static void test_execution_and_failure(void) {
    char root_template[] = "/tmp/fusion-rebuild-execute-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    char input[512], first_output[512], second_output[512], state_path[512];
    snprintf(input, sizeof input, "%s/input.txt", root);
    snprintf(first_output, sizeof first_output, "%s/first.txt", root);
    snprintf(second_output, sizeof second_output, "%s/second.txt", root);
    snprintf(state_path, sizeof state_path, "%s/cache/state.json", root);
    write_text(input, "source");
    const char *requirements[] = {"input.txt"};
    const char *inputs[] = {"input.txt"};
    const char *first_outputs[] = {"first.txt"};
    const char *dependencies[] = {"first"};
    const char *second_inputs[] = {"first.txt"};
    const char *second_outputs[] = {"second.txt"};
    const FusionRebuildTask tasks[] = {
        {.name = "first", .version = 1, .group = FUSION_REBUILD_GROUP_NATIVE,
         .requirements = requirements, .requirement_count = 1,
         .inputs = inputs, .input_count = 1,
         .outputs = first_outputs, .output_count = 1},
        {.name = "second", .version = 1, .group = FUSION_REBUILD_GROUP_NATIVE,
         .dependencies = dependencies, .dependency_count = 1,
         .inputs = second_inputs, .input_count = 1,
         .outputs = second_outputs, .output_count = 1},
    };
    const char *selected[] = {"second"};
    char error[512] = "";
    FusionRebuildPlan result;
    TestRunner runner = {0};
    assert(fusion_rebuild_execute_tasks(root, state_path, tasks, 2, selected, 1,
                                        false, test_action_runner, &runner,
                                        &result, error, sizeof error));
    assert(runner.calls == 2 && result.count == 2);
    assert(result.entries[0].status == FUSION_REBUILD_REBUILT);
    assert(result.entries[1].status == FUSION_REBUILD_REBUILT);
    fusion_rebuild_plan_free(&result);
    assert(fusion_rebuild_execute_tasks(root, state_path, tasks, 2, selected, 1,
                                        false, test_action_runner, &runner,
                                        &result, error, sizeof error));
    assert(runner.calls == 2);
    assert(result.entries[0].status == FUSION_REBUILD_UP_TO_DATE);
    assert(result.entries[1].status == FUSION_REBUILD_UP_TO_DATE);
    fusion_rebuild_plan_free(&result);

    write_text(input, "changed");
    runner.fail_name = "second";
    assert(!fusion_rebuild_execute_tasks(root, state_path, tasks, 2, selected, 1,
                                         false, test_action_runner, &runner,
                                         &result, error, sizeof error));
    assert(strstr(error, "failed deliberately") != NULL);
    assert(runner.calls == 4);
    runner.fail_name = NULL;
    assert(fusion_rebuild_execute_tasks(root, state_path, tasks, 2, selected, 1,
                                        false, test_action_runner, &runner,
                                        &result, error, sizeof error));
    assert(result.entries[0].status == FUSION_REBUILD_UP_TO_DATE);
    assert(result.entries[1].status == FUSION_REBUILD_REBUILT);
    fusion_rebuild_plan_free(&result);

    assert(!unlink(state_path));
    char cache[512];
    snprintf(cache, sizeof cache, "%s/cache", root);
    assert(!unlink(second_output));
    assert(!unlink(first_output));
    assert(!unlink(input));
    assert(!rmdir(cache));
    assert(!rmdir(root));
}

int main(void) {
    test_registered_graph();
    test_unknown_and_cycle();
    test_unknown_dependency();
    test_duplicate_selection_is_stable();
    test_incremental_plan_and_v1_state();
    test_export_index_validation();
    test_execution_and_failure();
    puts("fusion rebuild graph and cache tests passed");
    return 0;
}
