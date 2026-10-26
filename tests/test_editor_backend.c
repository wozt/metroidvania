/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "audit.h"
#include "backend.h"
#include "project_room.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
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

static const FusionJson *item_with_id(const FusionJson *array, const char *key, int64_t id) {
    assert(array->kind == FUSION_JSON_ARRAY);
    for (size_t index = 0; index < array->count; ++index) {
        const FusionJson *value = fusion_json_member(&array->items[index], key);
        if (value && value->kind == FUSION_JSON_NUMBER && value->integer == id)
            return &array->items[index];
    }
    assert(false);
    return NULL;
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

static FusionJson scoped_room(const char *room, const char *width, const char *height) {
    FusionJson options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", room);
    put_string(&options, "width", width);
    put_string(&options, "height", height);
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

static FusionJson create_door(const char *root, const char *room, const char *x,
                              const char *y, const char *facing) {
    FusionJson options = scoped_room(room, "128", "128");
    put_string(&options, "x", x);
    put_string(&options, "y", y);
    put_string(&options, "door_width", "16");
    put_string(&options, "door_height", "32");
    put_string(&options, "label", !strcmp(room, "7") ? "A" : "B");
    put_string(&options, "door_type", "normal");
    put_string(&options, "facing", facing);
    return execute_ok("door-create", options, root, false);
}

static void execute_error(const char *command, FusionJson options, const char *root,
                          const char *message) {
    FusionJson result = {0};
    FebError error = {0};
    assert(!feb_execute(command, &options, root, FUSION_SOURCE_DIR, false, &result, &error));
    assert(strstr(error.message, message) != NULL);
    fusion_json_free(&result);
    fusion_json_free(&options);
}

static char *read_text(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(!fseek(file, 0, SEEK_END));
    long size = ftell(file);
    assert(size >= 0);
    assert(!fseek(file, 0, SEEK_SET));
    char *text = malloc((size_t)size + 1);
    assert(text != NULL);
    assert(fread(text, 1, (size_t)size, file) == (size_t)size);
    text[size] = '\0';
    assert(!fclose(file));
    return text;
}

static void make_directory(const char *path) {
    char copy[4096];
    assert(strlen(path) < sizeof copy);
    strcpy(copy, path);
    for (char *part = copy + 1; *part; ++part) {
        if (*part != '/') continue;
        *part = '\0';
        assert(!mkdir(copy, 0777) || errno == EEXIST);
        *part = '/';
    }
    assert(!mkdir(copy, 0777) || errno == EEXIST);
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

static void test_audit_contract(void) {
    assert(!strcmp(feb_audit_diagnostic_status("unsupported affine"), "unsupported"));
    assert(!strcmp(feb_audit_diagnostic_status("truncated resource"), "error"));

    FusionJson result = fusion_json_object();
    put_string(&result, "status", "PARTIAL_NATIVE_BG_RENDER");
    assert(fusion_json_set(&result, "width", fusion_json_int(320)));
    assert(fusion_json_set(&result, "height", fusion_json_int(160)));
    FusionJson limitations = fusion_json_array();
    assert(fusion_json_push(&limitations, fusion_json_string("diagnostic only")));
    assert(fusion_json_set(&result, "limitations", limitations));
    FusionJson layer = fusion_json_object(), layers = fusion_json_object();
    assert(fusion_json_set(&layer, "width_blocks", fusion_json_int(20)));
    assert(fusion_json_set(&layer, "height_blocks", fusion_json_int(10)));
    assert(fusion_json_set(&layer, "unresolved_pixels_or_cells", fusion_json_int(7)));
    assert(fusion_json_set(&layers, "Bg1", layer));
    assert(fusion_json_set(&result, "layers", layers));
    FusionJson record = fusion_json_object();
    assert(feb_audit_finish_record(&record, &result, NULL));
    assert(!strcmp(member(&record, "status", FUSION_JSON_STRING)->string, "partial"));
    const FusionJson *expected = member(&record, "expected_render", FUSION_JSON_OBJECT);
    assert(member(expected, "width_px", FUSION_JSON_NUMBER)->integer == 320);
    assert(member(expected, "height_px", FUSION_JSON_NUMBER)->integer == 160);
    const FusionJson *invalid = member(&record, "invalid_graphics_references",
                                       FUSION_JSON_ARRAY);
    assert(invalid->count == 1);
    assert(member(&invalid->items[0], "unresolved_count", FUSION_JSON_NUMBER)->integer == 7);
    fusion_json_free(&record);
    fusion_json_free(&result);

    record = fusion_json_object();
    assert(feb_audit_finish_record(&record, NULL, "unsupported affine"));
    assert(!strcmp(member(&record, "status", FUSION_JSON_STRING)->string, "unsupported"));
    fusion_json_free(&record);
}

static void test_workspace_source_contract(void) {
    char path[4096];
    assert(snprintf(path, sizeof path, "%s/editor/native_workspace.c", FUSION_SOURCE_DIR) > 0);
    char *source = read_text(path);
    static const char *const terms[] = {
        "project_door_list_targets_0114(", "project_door_target_chosen_0114(",
        "List saved target doors", "--command=door-target-list", "project_door_link_0102(",
        "form->target_choice_ids", "PATCH_0115_RECIPROCAL_GTK", "Prepare return link",
        "--command=door-return-plan", "pending_reverse_0115",
        "project_reverse_apply_0115(doc);", "project_command(doc, \"door-link\", opts, NULL)",
        "First save the forward link with the source room diskette.",
    };
    for (size_t index = 0; index < sizeof terms / sizeof terms[0]; ++index)
        assert(strstr(source, terms[index]) != NULL);
    free(source);
    assert(snprintf(path, sizeof path, "%s/src/editor_backend/commands.c", FUSION_SOURCE_DIR) > 0);
    source = read_text(path);
    assert(strstr(source, "{\"door-target-list\",") != NULL);
    assert(strstr(source, "static const char *const worlds[] = {\"zero_mission\", \"aria\"}") !=
           NULL);
    assert(strstr(source, "feb_audit_write_report(repository, &report, file, path, error)") !=
           NULL);
    assert(strstr(source, "report must be a lowercase safe identifier") != NULL);
    free(source);

    assert(snprintf(path, sizeof path, "%s/editor/world_atlas.c", FUSION_SOURCE_DIR) > 0);
    source = read_text(path);
    static const char *const connection_terms[] = {
        "PATCH_0116_CONNECTION_VISUALIZATION",
        "gtk_overlay_add_overlay(GTK_OVERLAY(map_overlay), grid);",
        "gtk_widget_set_can_target(w->connection_canvas, FALSE);",
        "connection_draw_0116(", "connections_refresh_0116(",
        "All links in current map", "Show connections",
    };
    for (size_t index = 0; index < sizeof connection_terms / sizeof connection_terms[0]; ++index)
        assert(strstr(source, connection_terms[index]) != NULL);
    free(source);
}

static FusionJson adoption_options(const char *index) {
    FusionJson options = scoped_room("10", "640", "480");
    put_string(&options, "native_index", index);
    put_string(&options, "native_variant", "native");
    put_string(&options, "native_type", index);
    return options;
}

static void test_native_door_adoption(void) {
    char root_template[] = "/tmp/fusion-editor-adopt-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    execute_error("door-adopt", adoption_options("26"), root, "import original room annotations");
    char directory[4096], annotation[4096];
    assert(snprintf(directory, sizeof directory,
                    "%s/assets/extracted/rooms/metroid/annotations", root) > 0);
    make_directory(directory);
    assert(snprintf(annotation, sizeof annotation, "%s/brinstar_010.tsv", directory) > 0);
    write_text(annotation, "# private annotation\n"
                           "DOOR|26|32|80|16|64|native|26|Door 26|native original\n");
    FusionJson result = execute_ok("door-adopt", adoption_options("26"), root, false);
    const FusionJson *door = member(&result, "door", FUSION_JSON_OBJECT);
    assert(member(member(door, "native_source", FUSION_JSON_OBJECT), "index",
                  FUSION_JSON_NUMBER)->integer == 26);
    int64_t id = member(door, "id", FUSION_JSON_NUMBER)->integer;
    fusion_json_free(&result);
    result = execute_ok("door-adopt", adoption_options("26"), root, false);
    assert(member(member(&result, "door", FUSION_JSON_OBJECT), "id",
                  FUSION_JSON_NUMBER)->integer == id);
    fusion_json_free(&result);
    FusionJson options = scoped_room("10", "640", "480");
    result = execute_ok("door-list", options, root, false);
    assert(member(&result, "doors", FUSION_JSON_ARRAY)->count == 1);
    fusion_json_free(&result);
    execute_error("door-adopt", adoption_options("28"), root,
                  "source door missing or ambiguous");
    remove_tree(root);
}

static FusionJson link_options(const char *room, const char *source_door,
                               const char *target_room, const char *target_door,
                               const char *spawn_x, const char *spawn_y) {
    FusionJson options = scoped_room(room, "128", "128");
    put_string(&options, "source_door_id", source_door);
    put_string(&options, "target_world", "zero_mission");
    put_string(&options, "target_area", "Brinstar");
    put_string(&options, "target_room", target_room);
    put_string(&options, "target_door_id", target_door);
    put_string(&options, "spawn_x", spawn_x);
    put_string(&options, "spawn_y", spawn_y);
    return options;
}

static void test_target_and_reciprocal_links(void) {
    char root_template[] = "/tmp/fusion-editor-links-XXXXXX";
    char *root = mkdtemp(root_template);
    assert(root != NULL);
    FusionJson result = create_door(root, "7", "16", "32", "right");
    fusion_json_free(&result);
    result = create_door(root, "8", "48", "64", "left");
    fusion_json_free(&result);

    FusionJson options = fusion_json_object();
    put_string(&options, "target_world", "zero_mission");
    put_string(&options, "target_area", "Brinstar");
    put_string(&options, "target_room", "8");
    result = execute_ok("door-target-list", options, root, false);
    const FusionJson *doors = member(&result, "doors", FUSION_JSON_ARRAY);
    assert(doors->count == 1);
    assert(!strcmp(member(&doors->items[0], "label", FUSION_JSON_STRING)->string, "B"));
    fusion_json_free(&result);

    result = create_door(root, "7", "64", "0", "left");
    fusion_json_free(&result);
    result = create_door(root, "7", "80", "0", "left");
    fusion_json_free(&result);
    result = create_door(root, "7", "96", "0", "left");
    fusion_json_free(&result);
    result = create_door(root, "8", "80", "0", "right");
    fusion_json_free(&result);

    execute_error("door-link", link_options("7", "1", "8", "99", "48", "64"), root,
                  "not present");
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    put_string(&options, "source_door_id", "1");
    execute_error("door-return-plan", options, root, "forward destination");

    result = execute_ok("door-link", link_options("7", "1", "8", "1", "48", "64"), root,
                        false);
    fusion_json_free(&result);

    options = scoped_room("7", "128", "128");
    put_string(&options, "source_door_id", "3");
    put_string(&options, "target_world", "aria");
    put_string(&options, "target_area", "3");
    put_string(&options, "target_room", "5");
    put_string(&options, "spawn_x", "0");
    put_string(&options, "spawn_y", "0");
    result = execute_ok("door-link", options, root, false);
    fusion_json_free(&result);

    result = execute_ok("door-link", link_options("7", "4", "8", "2", "80", "0"), root,
                        false);
    fusion_json_free(&result);
    options = scoped_room("8", "128", "128");
    put_string(&options, "id", "2");
    put_bool(&options, "confirm", true);
    result = execute_ok("door-delete", options, root, false);
    fusion_json_free(&result);

    FebRoomScope source_scope = {.world = "mzm", .room = 7, .width = 128, .height = 128};
    strcpy(source_scope.area, "Brinstar");
    char source_path[4096];
    FebError path_error = {0};
    assert(feb_room_path(root, &source_scope, source_path, &path_error));
    char *source_before = read_text(source_path);
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("connection-list", options, root, false);
    const FusionJson *connections = member(&result, "connections", FUSION_JSON_ARRAY);
    assert(connections->count == 4);
    assert(!strcmp(member(item_with_id(connections, "door_id", 1), "state",
                          FUSION_JSON_STRING)->string, "simple"));
    assert(!strcmp(member(item_with_id(connections, "door_id", 2), "state",
                          FUSION_JSON_STRING)->string, "missing"));
    assert(!strcmp(member(item_with_id(connections, "door_id", 3), "state",
                          FUSION_JSON_STRING)->string, "interworld"));
    assert(!strcmp(member(item_with_id(connections, "door_id", 4), "state",
                          FUSION_JSON_STRING)->string, "invalid"));
    fusion_json_free(&result);
    char *source_after = read_text(source_path);
    assert(!strcmp(source_before, source_after));
    free(source_before);
    free(source_after);

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    put_string(&options, "source_door_id", "1");
    result = execute_ok("door-return-plan", options, root, false);
    assert(member(&result, "source_room", FUSION_JSON_NUMBER)->integer == 8);
    assert(member(&result, "source_door_id", FUSION_JSON_NUMBER)->integer == 1);
    assert(member(&result, "target_room", FUSION_JSON_NUMBER)->integer == 7);
    assert(member(&result, "target_door_id", FUSION_JSON_NUMBER)->integer == 1);
    assert(member(&result, "spawn_x", FUSION_JSON_NUMBER)->integer == 16);
    assert(member(&result, "spawn_y", FUSION_JSON_NUMBER)->integer == 32);
    fusion_json_free(&result);

    assert(!setenv("MV_EDITOR_ROOM_STAGE", "00000000-0000-4000-8000-000000000115", 1));
    result = execute_ok("door-link", link_options("8", "1", "7", "1", "16", "32"), root,
                        false);
    assert(member(member(&result, "transition", FUSION_JSON_OBJECT), "target_door_id",
                  FUSION_JSON_NUMBER)->integer == 1);
    fusion_json_free(&result);
    assert(!unsetenv("MV_EDITOR_ROOM_STAGE"));
    options = scoped_room("8", "128", "128");
    result = execute_ok("transition-list", options, root, false);
    assert(member(&result, "transitions", FUSION_JSON_ARRAY)->count == 0);
    fusion_json_free(&result);

    result = execute_ok("door-link", link_options("8", "1", "7", "1", "16", "32"), root,
                        false);
    fusion_json_free(&result);
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("connection-list", options, root, false);
    connections = member(&result, "connections", FUSION_JSON_ARRAY);
    const FusionJson *reciprocal = item_with_id(connections, "door_id", 1);
    assert(!strcmp(member(reciprocal, "state", FUSION_JSON_STRING)->string, "reciprocal"));
    assert(member(reciprocal, "reciprocal", FUSION_JSON_BOOL)->boolean);
    fusion_json_free(&result);
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    put_string(&options, "source_door_id", "1");
    execute_error("door-return-plan", options, root, "already has a transition");
    remove_tree(root);
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

    FebRoomScope source_scope = {.world = "mzm", .room = 7, .width = 128, .height = 64};
    strcpy(source_scope.area, "Brinstar");
    char source_path[4096];
    struct stat status;
    FebError path_error = {0};
    assert(feb_room_path(root, &source_scope, source_path, &path_error));
    char *source_before = read_text(source_path);

    assert(!setenv("MV_EDITOR_ROOM_STAGE", "00000000-0000-4000-8000-000000000124", 1));
    options = room_scope();
    put_string(&options, "x", "2");
    put_string(&options, "y", "2");
    put_string(&options, "fill_width", "1");
    put_string(&options, "fill_height", "1");
    put_string(&options, "collision_type", "hazard");
    result = execute_ok("collision-fill", options, root, false);
    fusion_json_free(&result);
    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("room-export", options, root, true);
    assert(!member(&result, "persisted", FUSION_JSON_BOOL)->boolean);
    fusion_json_free(&result);
    char export_root[4096];
    assert(snprintf(export_root, sizeof export_root, "%s/assets/extracted/exports", root) > 0);
    assert(stat(export_root, &status) != 0);

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("room-export", options, root, false);
    assert(member(member(&result, "counts", FUSION_JSON_OBJECT), "collision",
                  FUSION_JSON_NUMBER)->integer == 2);
    const char *package = member(&result, "package_dir", FUSION_JSON_STRING)->string;
    assert(!stat(package, &status) && S_ISDIR(status.st_mode));
    const char *digest_value = member(&result, "sha256", FUSION_JSON_STRING)->string;
    assert(strlen(digest_value) == 64);
    char digest[65], package_path[4096], preview_path[4096];
    strcpy(digest, digest_value);
    assert(strlen(package) < sizeof package_path);
    strcpy(package_path, package);
    assert(snprintf(preview_path, sizeof preview_path, "%s/preview.tsv", package) > 0);
    char *preview = read_text(preview_path);
    assert(strstr(preview, "MVROOM-PREVIEW\t1\tmzm\tBrinstar\t7\t128\t64\t16\n"));
    assert(strstr(preview, "C\t0\t0\t16\t16\t1\n"));
    assert(strstr(preview, "C\t16\t0\t16\t16\t1\n"));
    assert(strstr(preview, "C\t32\t32\t16\t16\t3\n") == NULL);
    assert(strstr(preview, "D\t0\t0\t16\t32\t0\n"));
    assert(strstr(preview, "E\t16\t16\t16\t16\t0\n"));
    assert(strstr(preview, "V\t16\t16\t16\t16\t0\n"));
    assert(strlen(preview) >= 4 && !strcmp(preview + strlen(preview) - 4, "END\n"));
    free(preview);
    fusion_json_free(&result);
    assert(!unsetenv("MV_EDITOR_ROOM_STAGE"));

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "7");
    result = execute_ok("room-export", options, root, false);
    assert(!strcmp(member(&result, "sha256", FUSION_JSON_STRING)->string, digest));
    assert(!strcmp(member(&result, "package_dir", FUSION_JSON_STRING)->string, package_path));
    fusion_json_free(&result);
    char *source_after = read_text(source_path);
    assert(!strcmp(source_before, source_after));
    free(source_before);
    free(source_after);

    options = fusion_json_object();
    put_string(&options, "world", "zero_mission");
    put_string(&options, "area", "Brinstar");
    put_string(&options, "room", "9");
    execute_error("room-export", options, root, "saved project room");
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
    test_audit_contract();
    test_workspace_source_contract();
    test_native_door_adoption();
    test_target_and_reciprocal_links();
    test_backend_operations(root);

    char cli_template[] = "/tmp/fusion-editor-cli-XXXXXX";
    char *cli_root = mkdtemp(cli_template);
    assert(cli_root != NULL);
    test_cli_and_batch(cli_root);
    remove_tree(cli_root);
    remove_tree(root);
    return 0;
}
