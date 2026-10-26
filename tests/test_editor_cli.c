/* SPDX-License-Identifier: GPL-3.0-only */
/* Functional contracts of the display-independent editor CLI: exit codes and
 * error types, batches, draft placements, project entities, collision,
 * doors and transitions, event regions in both worlds, story saving, dry
 * runs and the refusal of symlinked output and batch paths. */
#define _XOPEN_SOURCE 700
#include "core/json.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif
#ifndef FUSION_BINARY_DIR
#error "FUSION_BINARY_DIR must identify the build directory"
#endif

#define CLI FUSION_BINARY_DIR "/fusion_editor_cli"

static int failures = 0;

static void fail(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    fputs("FAIL: ", stderr);
    vfprintf(stderr, format, arguments);
    fputc('\n', stderr);
    va_end(arguments);
    ++failures;
}

typedef struct {
    int code;
    char *out, *err;
} Result;

static char *slurp(int descriptor) {
    size_t size = 0, capacity = 4096;
    char *text = malloc(capacity);
    for (ssize_t got; text && (got = read(descriptor, text + size, capacity - size - 1)) > 0;) {
        size += (size_t)got;
        if (size + 1 == capacity) text = realloc(text, capacity *= 2);
    }
    if (!text) exit(2);
    text[size] = '\0';
    return text;
}

/* Runs the CLI with a NULL-terminated argument list and no display. */
static Result cli(const char *first, ...) {
    const char *arguments[64] = {CLI};
    size_t count = 1;
    va_list list;
    va_start(list, first);
    for (const char *argument = first; argument && count < 63; argument = va_arg(list, const char *))
        arguments[count++] = argument;
    va_end(list);
    arguments[count] = NULL;
    char out_path[] = "/tmp/fusion-cli-out-XXXXXX", err_path[] = "/tmp/fusion-cli-err-XXXXXX";
    int out = mkstemp(out_path), err = mkstemp(err_path);
    pid_t child = fork();
    if (!child) {
        dup2(out, STDOUT_FILENO);
        dup2(err, STDERR_FILENO);
        unsetenv("DISPLAY");
        unsetenv("WAYLAND_DISPLAY");
        if (chdir(FUSION_SOURCE_DIR)) _exit(126);
        execv(CLI, (char *const *)arguments);
        _exit(127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    lseek(out, 0, SEEK_SET);
    lseek(err, 0, SEEK_SET);
    Result result = {WIFEXITED(status) ? WEXITSTATUS(status) : -1, slurp(out), slurp(err)};
    close(out);
    close(err);
    unlink(out_path);
    unlink(err_path);
    return result;
}

static void done(Result *result) {
    free(result->out);
    free(result->err);
}

static void expect_code(Result *result, int code, const char *what) {
    if (result->code != code) fail("%s: exit %d, expected %d (%s)", what, result->code, code, result->err);
}

static void expect_output(Result *result, const char *expected, const char *what) {
    if (strcmp(result->out, expected)) fail("%s: output\n%s\nexpected\n%s", what, result->out, expected);
}

static FusionJson payload(const Result *result, const char *what) {
    FusionJson value;
    char error[256];
    if (!fusion_json_parse(result->out, strlen(result->out), &value, error, sizeof error)) {
        fail("%s: invalid JSON (%s)", what, error);
        return fusion_json_null();
    }
    return value;
}

/* The member at a slash-separated path, or NULL. */
static const FusionJson *at(const FusionJson *value, const char *path) {
    char copy[256];
    snprintf(copy, sizeof copy, "%s", path);
    for (char *key = strtok(copy, "/"); key && value; key = strtok(NULL, "/"))
        value = fusion_json_member(value, key);
    return value;
}

static void expect_text(const FusionJson *value, const char *path, const char *expected,
                        const char *what) {
    const FusionJson *member = at(value, path);
    if (!member || member->kind != FUSION_JSON_STRING || strcmp(member->string, expected))
        fail("%s: %s is not \"%s\"", what, path, expected);
}

static void expect_number(const FusionJson *value, const char *path, int64_t expected,
                          const char *what) {
    const FusionJson *member = at(value, path);
    if (!member || member->kind != FUSION_JSON_NUMBER || member->integer != expected)
        fail("%s: %s is not %lld", what, path, (long long)expected);
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    if (!file || fputs(text, file) < 0) fail("cannot write %s", path);
    if (file) fclose(file);
}

static char *temporary_root(void) {
    char *root = strdup("/tmp/fusion-cli-root-XXXXXX");
    if (!root || !mkdtemp(root)) exit(2);
    return root;
}

static void remove_root(char *root) {
    char command[512];
    snprintf(command, sizeof command, "rm -rf '%s'", root);
    if (system(command) != 0) fail("cannot remove %s", root);
    free(root);
}

static bool exists(const char *path) {
    struct stat status;
    return stat(path, &status) == 0;
}

static void test_errors(void) {
    Result result = cli("--command=project-info", "--format=json", NULL);
    expect_code(&result, 0, "project-info");
    FusionJson value = payload(&result, "project-info");
    const FusionJson *success = at(&value, "success");
    if (!success || !success->boolean) fail("project-info: not successful");
    expect_text(&value, "data/gameplay_status", "not_implemented", "project-info");
    fusion_json_free(&value);
    done(&result);

    result = cli("--command=project-info", "--world=aria", "--format=json", NULL);
    expect_code(&result, 2, "unknown option");
    value = payload(&result, "unknown option");
    expect_text(&value, "error/type", "usage", "unknown option");
    if (!strstr(result.err, "unknown option")) fail("unknown option: stderr");
    fusion_json_free(&value);
    done(&result);

    result = cli("--command=audio-list", "--format=json", NULL);
    expect_code(&result, 3, "audio-list");
    value = payload(&result, "audio-list");
    expect_text(&value, "error/type", "unavailable", "audio-list");
    const FusionJson *message = at(&value, "error/message");
    if (!message || !strstr(message->string, "pending")) fail("audio-list: message");
    fusion_json_free(&value);
    done(&result);
}

static void test_batches_and_paths(void) {
    char *root = temporary_root(), path[1024], option[1200], root_option[600];
    snprintf(path, sizeof path, "%s/batch.json", root);
    write_text(path, "{\"operations\": [{\"command\": \"room-create\", \"world\": \"aria\", "
                     "\"area\": 0, \"slug\": \"dry_room\", \"name\": \"Dry room\", "
                     "\"width_screens\": 1, \"height_screens\": 1}]}");
    snprintf(option, sizeof option, "--batch=%s", path);
    snprintf(root_option, sizeof root_option, "--root=%s", root);
    Result result = cli(option, "--atomic=true", "--dry-run=true", root_option, "--format=json", NULL);
    expect_code(&result, 0, "atomic dry-run batch");
    snprintf(path, sizeof path, "%s/assets", root);
    if (exists(path)) fail("atomic dry-run batch created %s", path);
    done(&result);

    /* Symlinked output and batch parents are refused. */
    char target[600], link[600];
    snprintf(target, sizeof target, "%s/target", root);
    snprintf(link, sizeof link, "%s/link", root);
    mkdir(target, 0777);
    if (symlink(target, link)) fail("cannot link %s", link);
    snprintf(option, sizeof option, "--output=%s/result.json", link);
    result = cli("--command=project-info", "--format=json", option, NULL);
    expect_code(&result, 2, "symlink output parent");
    done(&result);
    snprintf(path, sizeof path, "%s/batch.json", target);
    write_text(path, "{\"operations\":[{\"command\":\"project-info\"}]}");
    snprintf(option, sizeof option, "--batch=%s/batch.json", link);
    result = cli(option, "--format=json", NULL);
    expect_code(&result, 2, "symlink batch parent");
    done(&result);
    remove_root(root);
}

static void test_drafts_and_placements(void) {
    char *root = temporary_root(), root_option[600], path[600];
    snprintf(root_option, sizeof root_option, "--root=%s", root);
    Result result = cli("--command=room-create", "--world=zero_mission", "--area=0",
                        "--slug=cli_room", "--name=CLI Room", "--width-screens=1",
                        "--height-screens=1", root_option, "--format=json", NULL);
    expect_code(&result, 0, "room-create");
    done(&result);
    snprintf(path, sizeof path, "%s/assets/extracted/world_overview", root);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/assets/extracted/world_overview/mzm.tsv", root);
    write_text(path, "0|0|31|31|0|0\n");
    result = cli("--command=room-place", "--room=zero_mission:00:cli_room", "--x=2", "--y=3",
                 root_option, "--format=json", NULL);
    expect_code(&result, 0, "room-place");
    done(&result);
    result = cli("--command=placement-list", "--world=zero_mission", root_option, "--format=tsv",
                 NULL);
    expect_output(&result, "zero_mission:00:cli_room\tCLI Room\tzero_mission\t0\t2\t3\t1\t1\n",
                  "placement-list");
    done(&result);
    result = cli("--command=room-unplace", "--room=zero_mission:00:cli_room", root_option,
                 "--format=json", NULL);
    expect_code(&result, 4, "unconfirmed room-unplace");
    done(&result);
    result = cli("--command=room-unplace", "--room=zero_mission:00:cli_room", "--confirm=true",
                 root_option, "--format=json", NULL);
    expect_code(&result, 0, "room-unplace");
    done(&result);
    remove_root(root);
}

#define ZERO_SCOPE "--world=zero_mission", "--area=Brinstar", "--room=7", "--width=64", \
                   "--height=32", root_option

static void test_entities_collision_and_doors(void) {
    char *root = temporary_root(), root_option[600];
    snprintf(root_option, sizeof root_option, "--root=%s", root);
    Result result = cli("--command=entity-create", ZERO_SCOPE, "--kind=ENEMY", "--x=0", "--y=0",
                        "--label=CLI Enemy", "--format=json", NULL);
    expect_code(&result, 0, "entity-create");
    done(&result);
    result = cli("--command=entity-move", ZERO_SCOPE, "--id=1", "--x=16", "--y=0",
                 "--format=json", NULL);
    expect_code(&result, 0, "entity-move");
    done(&result);
    result = cli("--command=entity-update", ZERO_SCOPE, "--id=1", "--x=32", "--y=0",
                 "--label=Edited CLI Enemy", "--native-type=unassigned", "--format=json", NULL);
    expect_code(&result, 0, "entity-update");
    done(&result);
    result = cli("--command=entity-list", ZERO_SCOPE, "--preview=true", "--format=tsv", NULL);
    expect_output(&result, "1\tENEMY\t32\t0\tEdited CLI Enemy\tunassigned\t-1\n", "entity-list");
    done(&result);
    result = cli("--command=entity-delete", ZERO_SCOPE, "--id=1", "--format=json", NULL);
    expect_code(&result, 4, "unconfirmed entity-delete");
    done(&result);
    result = cli("--command=entity-delete", ZERO_SCOPE, "--id=1", "--confirm=true",
                 "--format=json", NULL);
    expect_code(&result, 0, "entity-delete");
    done(&result);

    result = cli("--command=collision-fill", ZERO_SCOPE, "--x=0", "--y=0", "--fill-width=2",
                 "--fill-height=1", "--type=solid", "--format=json", NULL);
    expect_code(&result, 0, "collision-fill");
    done(&result);
    result = cli("--command=collision-stroke", ZERO_SCOPE, "--points=0,0;1,0;1,1;1,1",
                 "--type=water", "--format=json", NULL);
    expect_code(&result, 0, "collision-stroke");
    FusionJson value = payload(&result, "collision-stroke");
    expect_number(&value, "data/points", 3, "collision-stroke");
    fusion_json_free(&value);
    done(&result);
    result = cli("--command=collision-list", ZERO_SCOPE, "--format=tsv", NULL);
    expect_output(&result, "0\t0\twater\t16\n1\t0\twater\t16\n1\t1\twater\t16\n", "collision-list");
    done(&result);
    result = cli("--command=door-create", ZERO_SCOPE, "--x=0", "--y=0", "--door-width=16",
                 "--door-height=32", "--label=CLI Gate", "--door-type=portal", "--facing=left",
                 "--format=json", NULL);
    expect_code(&result, 0, "door-create");
    done(&result);
    result = cli("--command=door-link", ZERO_SCOPE, "--source-door-id=1",
                 "--target-world=zero_mission", "--target-area=Brinstar", "--target-room=3",
                 "--target-door-id=0", "--spawn-x=16", "--spawn-y=16", "--format=json", NULL);
    expect_code(&result, 0, "door-link");
    done(&result);
    result = cli("--command=transition-list", ZERO_SCOPE, "--format=tsv", NULL);
    expect_output(&result, "1\t1\tmzm\tBrinstar\t3\t0\t16\t16\n", "transition-list");
    done(&result);
    result = cli("--command=door-delete", ZERO_SCOPE, "--id=1", "--confirm=true", "--format=json",
                 NULL);
    expect_code(&result, 4, "door-delete with a transition");
    done(&result);
    result = cli("--command=transition-delete", ZERO_SCOPE, "--id=1", "--confirm=true",
                 "--format=json", NULL);
    expect_code(&result, 0, "transition-delete");
    done(&result);
    result = cli("--command=door-delete", ZERO_SCOPE, "--id=1", "--confirm=true", "--format=json",
                 NULL);
    expect_code(&result, 0, "door-delete");
    done(&result);
    remove_root(root);
}

static void test_aria_item_fields(void) {
    char *root = temporary_root(), root_option[600];
    snprintf(root_option, sizeof root_option, "--root=%s", root);
    Result result = cli("--command=entity-create", "--world=aria", "--area=3", "--room=7",
                        "--width=64", "--height=32", root_option, "--kind=ITEM", "--x=8", "--y=0",
                        "--label=Aria Item", "--native-type=pickup:03", "--item-id=1",
                        "--parameter-0=2", "--parameter-1=3", "--flags=4", "--format=json", NULL);
    expect_code(&result, 0, "Aria entity-create");
    done(&result);
    result = cli("--command=entity-update", "--world=aria", "--area=3", "--room=7", "--width=64",
                 "--height=32", root_option, "--id=1", "--x=16", "--y=8",
                 "--label=Edited Aria Item", "--native-type=pickup:03", "--item-id=12",
                 "--parameter-0=513", "--parameter-1=999", "--flags=7", "--format=json", NULL);
    expect_code(&result, 0, "Aria entity-update");
    FusionJson value = payload(&result, "Aria entity-update");
    expect_number(&value, "data/entity/x", 16, "Aria entity-update");
    expect_number(&value, "data/entity/y", 8, "Aria entity-update");
    expect_text(&value, "data/entity/label", "Edited Aria Item", "Aria entity-update");
    expect_number(&value, "data/entity/settings/item_id", 12, "Aria entity-update");
    expect_number(&value, "data/entity/settings/parameter_0", 513, "Aria entity-update");
    expect_number(&value, "data/entity/settings/parameter_1", 999, "Aria entity-update");
    expect_number(&value, "data/entity/settings/flags", 7, "Aria entity-update");
    if (at(&value, "data/entity/settings") && at(&value, "data/entity/settings")->count != 4)
        fail("Aria entity-update: settings fields");
    fusion_json_free(&value);
    done(&result);
    remove_root(root);
}

static void test_collision_capabilities(void) {
    static const struct {
        const char *world;
        int resolution;
    } worlds[] = {{"zero_mission", 16}, {"aria", 8}};
    for (size_t index = 0; index < 2; ++index) {
        char option[64];
        snprintf(option, sizeof option, "--world=%s", worlds[index].world);
        Result result = cli("--command=collision-capabilities", option, "--format=json", NULL);
        expect_code(&result, 0, "collision-capabilities");
        FusionJson value = payload(&result, "collision-capabilities");
        expect_text(&value, "data/world", worlds[index].world, "collision-capabilities");
        expect_number(&value, "data/resolution_px", worlds[index].resolution,
                      "collision-capabilities");
        expect_text(&value, "data/native_encoder", "unavailable", "collision-capabilities");
        const FusionJson *types = at(&value, "data/types");
        static const char *const expected[] = {"solid", "one_way", "hazard", "slope_up",
                                               "slope_down", "water", "air"};
        if (!types || types->count != 7) fail("collision-capabilities: types");
        for (size_t type = 0; types && type < 7; ++type) {
            bool found = false;
            for (size_t entry = 0; entry < types->count; ++entry) {
                const FusionJson *item = &types->items[entry];
                if (strcmp(fusion_json_member(item, "type")->string, expected[type])) continue;
                found = true;
                if (!strcmp(expected[type], "one_way"))
                    expect_text(item, "native_reference_status", "verified_exact",
                                "collision-capabilities");
            }
            if (!found) fail("collision-capabilities: missing %s", expected[type]);
        }
        fusion_json_free(&value);
        done(&result);
    }
}

static void test_event_regions(void) {
    char *root = temporary_root(), root_option[600];
    snprintf(root_option, sizeof root_option, "--root=%s", root);
    Result result = cli("--command=capabilities", root_option, "--format=json", NULL);
    expect_code(&result, 0, "capabilities");
    FusionJson value = payload(&result, "capabilities");
    static const char *const events[] = {"event-list", "event-inspect", "event-create",
                                         "event-update", "event-delete", "event-validate"};
    for (size_t index = 0; index < 6; ++index) {
        char path[96];
        snprintf(path, sizeof path, "data/commands/%s", events[index]);
        expect_text(&value, path, "available_project_room_data_only", "capabilities");
    }
    fusion_json_free(&value);
    done(&result);
    static const struct {
        const char *world, *area;
        int step;
    } worlds[] = {{"zero_mission", "Brinstar", 16}, {"aria", "3", 8}};
    for (size_t index = 0; index < 2; ++index) {
        int step = worlds[index].step;
        char world[64], area[64], xy[32], region_width[48], region_height[48], expected[512];
        snprintf(world, sizeof world, "--world=%s", worlds[index].world);
        snprintf(area, sizeof area, "--area=%s", worlds[index].area);
        snprintf(xy, sizeof xy, "%d", step);
        char x[48], y[48];
        snprintf(x, sizeof x, "--x=%d", step);
        snprintf(y, sizeof y, "--y=%d", step);
        snprintf(region_width, sizeof region_width, "--region-width=%d", step * 2);
        snprintf(region_height, sizeof region_height, "--region-height=%d", step);
#define EVENT_SCOPE world, area, "--room=5", "--width=64", "--height=64", root_option
        result = cli("--command=event-create", EVENT_SCOPE, "--event-kind=TRIGGER", x, y,
                     region_width, region_height, "--label=Checkpoint trigger",
                     "--trigger-type=enter", "--action-type=checkpoint",
                     "--action-ref=checkpoint:room5", "--once=true", "--condition-mode=any",
                     "--conditions=[{\"type\":\"checkpoint_active\",\"ref\":\"checkpoint:alpha\","
                     "\"negated\":false},{\"type\":\"checkpoint_active\","
                     "\"ref\":\"checkpoint:beta\",\"negated\":true}]",
                     "--format=json", NULL);
        expect_code(&result, 0, "event-create");
        done(&result);
        snprintf(region_width, sizeof region_width, "--region-width=%d", step);
        result = cli("--command=event-update", EVENT_SCOPE, "--id=1", "--event-kind=EVENT",
                     "--x=0", "--y=0", region_width, region_height, "--label=Checkpoint event",
                     "--trigger-type=interact", "--action-type=checkpoint",
                     "--action-ref=checkpoint:room5_return", "--once=false",
                     "--condition-mode=all",
                     "--conditions=[{\"type\":\"checkpoint_active\",\"ref\":\"checkpoint:alpha\","
                     "\"negated\":false}]",
                     "--format=json", NULL);
        expect_code(&result, 0, "event-update");
        done(&result);
        result = cli("--command=event-validate", EVENT_SCOPE, "--format=tsv", NULL);
        expect_output(&result,
                      "1\tcheckpoint\tcheckpoint:room5_return\t1\tvalid_checkpoint_key\tall\t1\t1\t"
                      "0:valid_checkpoint_key\n",
                      "event-validate");
        done(&result);
        snprintf(expected, sizeof expected,
                 "1\tEVENT\t0\t0\t%s\t%s\tCheckpoint event\tinteract\tcheckpoint\t"
                 "checkpoint:room5_return\t0\tall\tcheckpoint_active,checkpoint:alpha,0\n",
                 xy, xy);
        Result listed = cli("--command=event-list", EVENT_SCOPE, "--format=tsv", NULL);
        expect_output(&listed, expected, "event-list");
        result = cli("--command=event-update", EVENT_SCOPE, "--id=1", "--action-type=story",
                     "--action-ref=timeline:missing", "--format=json", NULL);
        expect_code(&result, 4, "unresolved story reference");
        done(&result);
        result = cli("--command=event-update", EVENT_SCOPE, "--id=1",
                     "--conditions=[{\"type\":\"entity_present\",\"ref\":\"entity:999\","
                     "\"negated\":false}]",
                     "--format=json", NULL);
        expect_code(&result, 4, "unresolved condition");
        done(&result);
        result = cli("--command=event-list", EVENT_SCOPE, "--format=tsv", NULL);
        expect_output(&result, listed.out, "event-list unchanged");
        done(&result);
        done(&listed);
        result = cli("--command=event-delete", EVENT_SCOPE, "--id=1", "--confirm=true",
                     "--format=json", NULL);
        expect_code(&result, 0, "event-delete");
        done(&result);
#undef EVENT_SCOPE
    }
    remove_root(root);
}

static void test_story_and_dry_runs(void) {
    char *root = temporary_root(), root_option[600], path[600], input[700];
    snprintf(root_option, sizeof root_option, "--root=%s", root);
    snprintf(path, sizeof path, "%s/data", root);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/data/cutscenes", root);
    mkdir(path, 0777);
    snprintf(path, sizeof path, "%s/candidate.toml", root);
    const char *scene = "schema = \"metroid-vania-cutscene\"\nversion = 1\nid = \"cli_scene\"\n"
                        "world = \"interzone\"\n[[actors]]\nid = \"soma\"\n[[steps]]\n"
                        "type = \"dialogue\"\nactor = \"soma\"\ntext = \"Hello\"\n";
    write_text(path, scene);
    snprintf(input, sizeof input, "--input=%s", path);
    Result result = cli("--command=story-save", "--kind=cutscene", input,
                        "--target=data/cutscenes/cli_scene.toml", root_option, "--format=json",
                        NULL);
    expect_code(&result, 0, "story-save");
    done(&result);
    snprintf(path, sizeof path, "%s/data/cutscenes/cli_scene.toml", root);
    FILE *file = fopen(path, "r");
    char saved[1024] = "";
    if (file) {
        size_t read = fread(saved, 1, sizeof saved - 1, file);
        saved[read] = '\0';
        fclose(file);
    }
    if (strcmp(saved, scene)) fail("story-save: saved text differs");
    result = cli("--command=story-save", "--kind=cutscene", input, "--target=../escape.toml",
                 root_option, "--format=json", NULL);
    expect_code(&result, 4, "story-save traversal");
    done(&result);
    result = cli("--command=room-open", "--world=zero_mission", "--area=Brinstar", "--room=3",
                 "--dry-run=true", root_option, "--format=json", NULL);
    expect_code(&result, 0, "room-open dry run");
    FusionJson value = payload(&result, "room-open dry run");
    const FusionJson *persisted = at(&value, "data/persisted");
    const FusionJson *created = at(&value, "created_paths");
    if (!persisted || persisted->boolean) fail("room-open dry run: persisted");
    if (!created || created->count) fail("room-open dry run: created paths");
    fusion_json_free(&value);
    done(&result);
    remove_root(root);
}

static void test_gui_uses_the_shared_cli(void) {
    static const char *const sources[] = {"editor/room_browser.c", "editor/native_workspace.c",
                                          "editor/world_atlas.c", "editor/object_catalog.c",
                                          "editor/story_workspace.c"};
    bool program = false;
    for (size_t index = 0; index < 5; ++index) {
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", FUSION_SOURCE_DIR, sources[index]);
        FILE *file = fopen(path, "rb");
        if (!file) {
            fail("cannot read %s", sources[index]);
            continue;
        }
        char *text = slurp(fileno(file));
        fclose(file);
        program |= strstr(text, "fusion_editor_cli_program()") != NULL;
        if (strstr(text, "python3") || strstr(text, "\"scripts."))
            fail("%s runs a Python module", sources[index]);
        free(text);
    }
    if (!program) fail("the GTK editor does not run fusion_editor_cli_program()");
}

int main(void) {
    if (access(CLI, X_OK)) {
        puts("editor CLI contracts skipped: fusion_editor_cli not built");
        return 77;
    }
    test_errors();
    test_batches_and_paths();
    test_drafts_and_placements();
    test_entities_collision_and_doors();
    test_aria_item_fields();
    test_collision_capabilities();
    test_event_regions();
    test_story_and_dry_runs();
    test_gui_uses_the_shared_cli();
    if (failures) {
        fprintf(stderr, "%d editor CLI failure(s)\n", failures);
        return 1;
    }
    puts("editor CLI contracts: ok");
    return 0;
}
