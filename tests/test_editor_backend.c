/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "backend.h"

#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif

static void put_string(FusionJson *object, const char *key, const char *value) {
    assert(fusion_json_set(object, key, fusion_json_string(value)));
}

static void put_bool(FusionJson *object, const char *key, bool value) {
    assert(fusion_json_set(object, key, fusion_json_bool(value)));
}

static const FusionJson *member(const FusionJson *object, const char *key,
                                FusionJsonKind kind) {
    const FusionJson *value = fusion_json_member(object, key);
    assert(value != NULL);
    assert(value->kind == kind);
    return value;
}

static FusionJson room_scope(void) {
    FusionJson options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    put_string(&options, "width", "128");
    put_string(&options, "height", "64");
    return options;
}

static FusionJson execute_ok(const char *command, FusionJson options,
                             const char *root, bool dry_run) {
    FusionJson result = {0};
    FebError error = {0};
    bool ok = feb_execute(command, &options, root, FUSION_SOURCE_DIR, dry_run,
                          &result, &error);
    if (!ok) fprintf(stderr, "%s failed: %s\n", command, error.message);
    assert(ok);
    assert(error.message[0] == '\0');
    fusion_json_free(&options);
    return result;
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(text, 1, strlen(text), file) == strlen(text));
    assert(!fclose(file));
}

static void remove_tree(const char *path) {
    DIR *directory = opendir(path);
    if (!directory) {
        assert(!unlink(path));
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[4096];
        int length = snprintf(child, sizeof child, "%s/%s", path, entry->d_name);
        assert(length > 0 && (size_t)length < sizeof child);
        struct stat status;
        assert(!lstat(child, &status));
        if (S_ISDIR(status.st_mode) && !S_ISLNK(status.st_mode)) remove_tree(child);
        else assert(!unlink(child));
    }
    assert(!closedir(directory));
    assert(!rmdir(path));
}

static void test_command_registry(void) {
    static const char *const native[] = {
        "capabilities", "project-info", "room-create", "room-export",
        "entity-create", "collision-fill", "event-create", "door-link",
        "tile-fill", "room-audit", "project-audit",
    };
    for (size_t index = 0; index < sizeof native / sizeof native[0]; ++index) {
        bool known = false;
        assert(feb_is_native(native[index]));
        assert(feb_command_fields(native[index], &known) != NULL);
        assert(known);
    }
    assert(!feb_is_native("missing-command"));
    assert(feb_capability("audio-list") != NULL);
    assert(!strcmp(feb_capability("audio-list"),
                   "unavailable_native_inventory_pending"));
}

static void test_backend_operations(const char *root) {
    FusionJson result = execute_ok("project-info", fusion_json_object(), root, false);
    assert(!strcmp(member(&result, "gameplay_status", FUSION_JSON_STRING)->string,
                   "not_implemented"));
    fusion_json_free(&result);

    FusionJson options = fusion_json_object();
    put_string(&options, "world", "aria");
    result = execute_ok("collision-capabilities", options, root, false);
    assert(member(&result, "resolution_px", FUSION_JSON_NUMBER)->integer == 8);
    assert(member(&result, "types", FUSION_JSON_ARRAY)->count == 7);
    fusion_json_free(&result);

    options = room_scope();
    put_string(&options, "x", "0");
    put_string(&options, "y", "0");
    put_string(&options, "fill_width", "2");
    put_string(&options, "fill_height", "1");
    put_string(&options, "collision_type", "solid");
    result = execute_ok("collision-fill", options, root, false);
    assert(member(&result, "changed_cells", FUSION_JSON_NUMBER)->integer == 2);
    fusion_json_free(&result);

    options = room_scope();
    result = execute_ok("collision-list", options, root, false);
    assert(member(&result, "cells", FUSION_JSON_ARRAY)->count == 2);
    fusion_json_free(&result);

    options = room_scope();
    put_string(&options, "kind", "ENEMY");
    put_string(&options, "x", "16");
    put_string(&options, "y", "16");
    put_string(&options, "label", "Native enemy");
    put_string(&options, "native_type", "unassigned");
    result = execute_ok("entity-create", options, root, false);
    assert(member(member(&result, "entity", FUSION_JSON_OBJECT), "id",
                  FUSION_JSON_NUMBER)->integer == 1);
    fusion_json_free(&result);

    options = room_scope();
    put_string(&options, "x", "0");
    put_string(&options, "y", "0");
    put_string(&options, "door_width", "16");
    put_string(&options, "door_height", "32");
    put_string(&options, "label", "Native gate");
    put_string(&options, "door_type", "normal");
    put_string(&options, "facing", "left");
    result = execute_ok("door-create", options, root, false);
    assert(member(member(&result, "door", FUSION_JSON_OBJECT), "id",
                  FUSION_JSON_NUMBER)->integer == 1);
    fusion_json_free(&result);

    options = room_scope();
    put_string(&options, "event_kind", "TRIGGER");
    put_string(&options, "x", "16");
    put_string(&options, "y", "16");
    put_string(&options, "region_width", "16");
    put_string(&options, "region_height", "16");
    put_string(&options, "label", "Native checkpoint");
    put_string(&options, "trigger_type", "enter");
    put_string(&options, "action_type", "checkpoint");
    put_string(&options, "action_ref", "checkpoint:native_test");
    put_bool(&options, "once", true);
    result = execute_ok("event-create", options, root, false);
    assert(member(member(&result, "event", FUSION_JSON_OBJECT), "id",
                  FUSION_JSON_NUMBER)->integer == 1);
    fusion_json_free(&result);

    options = room_scope();
    result = execute_ok("event-validate", options, root, false);
    const FusionJson *events = member(&result, "events", FUSION_JSON_ARRAY);
    assert(events->count == 1);
    assert(member(&events->items[0], "valid", FUSION_JSON_BOOL)->boolean);
    fusion_json_free(&result);

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("room-export", options, root, false);
    const char *package = member(&result, "package_dir", FUSION_JSON_STRING)->string;
    struct stat status;
    assert(!stat(package, &status) && S_ISDIR(status.st_mode));
    assert(strlen(member(&result, "sha256", FUSION_JSON_STRING)->string) == 64);
    fusion_json_free(&result);
}

static int run_cli(int argc, char **argv, char *output, size_t capacity) {
    char capture[] = "/tmp/fusion-editor-output-XXXXXX";
    int descriptor = mkstemp(capture);
    assert(descriptor >= 0);
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    assert(saved >= 0);
    assert(dup2(descriptor, STDOUT_FILENO) >= 0);
    bool handled = false;
    int code = feb_cli_main(argc, argv, FUSION_SOURCE_DIR, &handled);
    fflush(stdout);
    assert(dup2(saved, STDOUT_FILENO) >= 0);
    assert(!close(saved));
    assert(handled);
    assert(lseek(descriptor, 0, SEEK_SET) == 0);
    ssize_t size = read(descriptor, output, capacity - 1);
    assert(size >= 0);
    output[size] = '\0';
    assert(!close(descriptor));
    assert(!unlink(capture));
    return code;
}

static void test_cli_and_batch(const char *root) {
    char output[16384], root_option[4096], batch_path[4096], batch_option[4096];
    assert(snprintf(root_option, sizeof root_option, "--root=%s", root) > 0);
    char *info[] = {"fusion_editor_cli", "--command=project-info", "--format=json",
                    root_option};
    assert(run_cli(4, info, output, sizeof output) == FEB_EXIT_SUCCESS);
    assert(strstr(output, "\"success\":true") != NULL);
    assert(strstr(output, "\"gameplay_status\":\"not_implemented\"") != NULL);

    char *unknown[] = {"fusion_editor_cli", "--command=project-info", "--world=aria",
                       "--format=json", root_option};
    assert(run_cli(5, unknown, output, sizeof output) == FEB_EXIT_USAGE);
    assert(strstr(output, "\"type\":\"usage\"") != NULL);

    char *unavailable[] = {"fusion_editor_cli", "--command=audio-list", "--format=json",
                           root_option};
    assert(run_cli(4, unavailable, output, sizeof output) == FEB_EXIT_UNAVAILABLE);
    assert(strstr(output, "\"type\":\"unavailable\"") != NULL);

    int length = snprintf(batch_path, sizeof batch_path, "%s/native-batch.json", root);
    assert(length > 0 && (size_t)length < sizeof batch_path);
    write_text(batch_path,
               "{\"operations\":[{\"command\":\"room-create\","
               "\"world\":\"aria\",\"area\":0,\"slug\":\"dry_room\","
               "\"name\":\"Dry room\",\"width_screens\":1,"
               "\"height_screens\":1}]}\n");
    assert(snprintf(batch_option, sizeof batch_option, "--batch=%s", batch_path) > 0);
    char *batch[] = {"fusion_editor_cli", batch_option, "--atomic=true", "--dry-run=true",
                     "--format=json", root_option};
    assert(run_cli(6, batch, output, sizeof output) == FEB_EXIT_SUCCESS);
    assert(strstr(output, "\"command\":\"room-create\"") != NULL);
    char assets[4096];
    assert(snprintf(assets, sizeof assets, "%s/assets", root) > 0);
    struct stat status;
    assert(stat(assets, &status) != 0);
}

int main(void) {
    char root_template[] = "/tmp/fusion-editor-backend-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    test_command_registry();
    test_backend_operations(root);

    char cli_template[] = "/tmp/fusion-editor-cli-XXXXXX";
    char *cli_root = mkdtemp(cli_template);
    assert(cli_root != NULL);
    test_cli_and_batch(cli_root);
    remove_tree(cli_root);
    remove_tree(root);
    return 0;
}
