/* SPDX-License-Identifier: GPL-3.0-only */
#define _DEFAULT_SOURCE
#include "story.h"

#include "core/toml.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const tracks[] = {"metroid", "castlevania", "shared"};
static const char *const worlds[] = {"zero_mission", "aria", "interzone"};
static const char *const step_types[] = {
    "dialogue", "move", "animation", "wait", "sound", "music", "portal", "set_flag",
    "choice", "camera"};

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

static bool in_list(const FusionJson *value, const char *const *list, size_t count) {
    if (!value || value->kind != FUSION_JSON_STRING) return false;
    for (size_t index = 0; index < count; ++index)
        if (!strcmp(list[index], value->string)) return true;
    return false;
}

/* isinstance(value, int): integers and booleans. */
static bool python_int(const FusionJson *value, int64_t *number) {
    if (!value) return false;
    if (value->kind == FUSION_JSON_BOOL) {
        *number = value->boolean;
        return true;
    }
    if (value->kind == FUSION_JSON_NUMBER && value->integral) {
        *number = value->integer;
        return true;
    }
    return false;
}

/* ID: ^[a-z][a-z0-9_.:-]{0,95}$ */
static bool story_id(const FusionJson *value) {
    if (!value || value->kind != FUSION_JSON_STRING) return false;
    const char *text = value->string;
    size_t length = strlen(text);
    if (length && text[length - 1] == '\n') --length;  /* "$" accepts a final newline */
    if (length < 1 || length > 96 || text[0] < 'a' || text[0] > 'z') return false;
    for (size_t index = 1; index < length; ++index) {
        char c = text[index];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == ':' || c == '-'))
            return false;
    }
    return true;
}

bool feb_set_contains(const FusionJson *set, const char *value) {
    for (size_t index = 0; set && index < set->count; ++index)
        if (set->items[index].kind == FUSION_JSON_STRING &&
            !strcmp(set->items[index].string, value))
            return true;
    return false;
}

static bool set_add(FusionJson *set, const char *value) {
    if (feb_set_contains(set, value)) return true;
    return fusion_json_push(set, fusion_json_string(value));
}

static const FusionJson *list_or_empty(const FusionJson *object, const char *key) {
    static const FusionJson empty = {.kind = FUSION_JSON_ARRAY};
    const FusionJson *value = fusion_json_member(object, key);
    return value ? value : &empty;
}

bool feb_validate_timeline(const FusionJson *data, int64_t *count, FebError *error) {
    const FusionJson *schema = fusion_json_member(data, "schema");
    const FusionJson *version = fusion_json_member(data, "version");
    int64_t version_number = 0;
    if (!schema || schema->kind != FUSION_JSON_STRING ||
        strcmp(schema->string, "metroid-vania-timeline") ||
        !python_int(version, &version_number) || version_number != 1 ||
        version->kind == FUSION_JSON_BOOL)
        return feb_fail(error, FEB_EXIT_OPERATION, "timeline schema/version mismatch");
    const FusionJson *events = fusion_json_member(data, "events");
    if (!events || events->kind != FUSION_JSON_ARRAY || !events->count)
        return feb_fail(error, FEB_EXIT_OPERATION, "timeline needs events");
    FusionJson ids = fusion_json_array(), flags = fusion_json_array();
    /* The producer event of each flag, in flag order. */
    size_t flag_total = 0;
    for (size_t index = 0; index < events->count; ++index)
        flag_total += list_or_empty(&events->items[index], "sets")->count;
    size_t *producer = calloc(flag_total + 1, sizeof *producer);
    bool ok = producer != NULL;
    if (!ok) feb_oom(error);
    for (size_t index = 0; ok && index < events->count; ++index) {
        const FusionJson *event = &events->items[index];
        const FusionJson *id = fusion_json_member(event, "id");
        if (event->kind != FUSION_JSON_OBJECT || !story_id(id) ||
            feb_set_contains(&ids, id->string)) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "duplicate/invalid event ID");
            break;
        }
        ok = set_add(&ids, id->string) || feb_oom(error);
        if (ok && (!in_list(fusion_json_member(event, "track"), tracks, COUNT(tracks)) ||
                   !in_list(fusion_json_member(event, "world"), worlds, COUNT(worlds))))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "%s: invalid track/world", id->string);
        const FusionJson *title = fusion_json_member(event, "title");
        if (ok && (!title || title->kind != FUSION_JSON_STRING || !title->string[0]))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "%s: missing title", id->string);
        int64_t minimum = 0, maximum = 0;
        if (ok && (!python_int(fusion_json_member(event, "min_order"), &minimum) ||
                   !python_int(fusion_json_member(event, "max_order"), &maximum) ||
                   minimum > maximum))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "%s: order range", id->string);
        const FusionJson *sets = list_or_empty(event, "sets");
        for (size_t flag = 0; ok && flag < sets->count; ++flag) {
            const FusionJson *name = &sets->items[flag];
            if (name->kind != FUSION_JSON_STRING) continue;
            if (feb_set_contains(&flags, name->string)) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "duplicate flag producer: %s",
                              name->string);
                break;
            }
            ok = set_add(&flags, name->string) || feb_oom(error);
            if (ok) producer[flags.count - 1] = index;
        }
    }
    /* deps: each event depends on the producers of its requirements. */
    size_t *dependencies = NULL, *offsets = NULL;
    if (ok) {
        offsets = calloc(events->count + 1, sizeof *offsets);
        size_t total = 0;
        for (size_t index = 0; index < events->count; ++index)
            total += list_or_empty(&events->items[index], "requires")->count;
        dependencies = calloc(total + 1, sizeof *dependencies);
        ok = offsets && dependencies;
        if (!ok) feb_oom(error);
        size_t used = 0;
        for (size_t index = 0; ok && index < events->count; ++index) {
            offsets[index] = used;
            const FusionJson *requires = list_or_empty(&events->items[index], "requires");
            for (size_t flag = 0; ok && flag < requires->count; ++flag) {
                const FusionJson *name = &requires->items[flag];
                size_t position = SIZE_MAX;
                for (size_t other = 0; name->kind == FUSION_JSON_STRING && other < flags.count;
                     ++other)
                    if (!strcmp(flags.items[other].string, name->string)) position = other;
                if (position == SIZE_MAX) {
                    ok = feb_fail(error, FEB_EXIT_OPERATION, "%s: requirement has no producer %s",
                                  fusion_json_member(&events->items[index], "id")->string,
                                  name->kind == FUSION_JSON_STRING ? name->string : "?");
                    break;
                }
                dependencies[used++] = producer[position];
            }
        }
        if (ok) offsets[events->count] = used;
    }
    /* An iterative depth-first search for dependency cycles. */
    unsigned char *state = ok ? calloc(events->count, 1) : NULL;
    size_t *stack = ok ? calloc(events->count + 1, sizeof *stack) : NULL;
    size_t *cursor = ok ? calloc(events->count, sizeof *cursor) : NULL;
    if (ok && (!state || !stack || !cursor)) ok = feb_oom(error);
    for (size_t start = 0; ok && start < events->count; ++start) {
        if (state[start]) continue;
        size_t depth = 0;
        stack[depth++] = start;
        state[start] = 1;
        cursor[start] = offsets[start];
        while (ok && depth) {
            size_t node = stack[depth - 1];
            if (cursor[node] < offsets[node + 1]) {
                size_t next = dependencies[cursor[node]++];
                if (state[next] == 1)
                    ok = feb_fail(error, FEB_EXIT_OPERATION, "timeline dependency cycle: %s",
                                  fusion_json_member(&events->items[next], "id")->string);
                else if (!state[next]) {
                    state[next] = 1;
                    cursor[next] = offsets[next];
                    stack[depth++] = next;
                }
            } else {
                state[node] = 2;
                --depth;
            }
        }
    }
    free(state);
    free(stack);
    free(cursor);
    free(dependencies);
    free(offsets);
    free(producer);
    fusion_json_free(&ids);
    fusion_json_free(&flags);
    if (ok) *count = (int64_t)events->count;
    return ok;
}

bool feb_validate_scene(const FusionJson *data, int64_t *count, FebError *error) {
    const FusionJson *schema = fusion_json_member(data, "schema");
    const FusionJson *version = fusion_json_member(data, "version");
    int64_t version_number = 0;
    if (!schema || schema->kind != FUSION_JSON_STRING ||
        strcmp(schema->string, "metroid-vania-cutscene") ||
        !python_int(version, &version_number) || version_number != 1)
        return feb_fail(error, FEB_EXIT_OPERATION, "cutscene schema/version mismatch");
    if (!story_id(fusion_json_member(data, "id")))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid cutscene ID");
    if (!in_list(fusion_json_member(data, "world"), worlds, COUNT(worlds)))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid scene target world");
    const FusionJson *actors = list_or_empty(data, "actors");
    const FusionJson *steps = fusion_json_member(data, "steps");
    if (!steps || steps->kind != FUSION_JSON_ARRAY || !steps->count)
        return feb_fail(error, FEB_EXIT_OPERATION, "cutscene requires steps");
    for (size_t index = 0; index < steps->count; ++index) {
        const FusionJson *step = &steps->items[index];
        const FusionJson *type = fusion_json_member(step, "type");
        if (step->kind != FUSION_JSON_OBJECT || !in_list(type, step_types, COUNT(step_types)))
            return feb_fail(error, FEB_EXIT_OPERATION, "step %zu: unknown action", index);
        const char *kind = type->string;
        if (!strcmp(kind, "move") || !strcmp(kind, "animation") || !strcmp(kind, "dialogue")) {
            const FusionJson *actor = fusion_json_member(step, "actor");
            bool known = false;
            for (size_t other = 0; actor && actor->kind == FUSION_JSON_STRING &&
                                   other < actors->count; ++other) {
                const FusionJson *id = fusion_json_member(&actors->items[other], "id");
                if (actors->items[other].kind == FUSION_JSON_OBJECT && id &&
                    id->kind == FUSION_JSON_STRING && !strcmp(id->string, actor->string))
                    known = true;
            }
            if (!known)
                return feb_fail(error, FEB_EXIT_OPERATION, "step %zu: unknown actor", index);
        }
        const FusionJson *text = fusion_json_member(step, "text");
        if (!strcmp(kind, "dialogue") && (!text || text->kind != FUSION_JSON_STRING))
            return feb_fail(error, FEB_EXIT_OPERATION, "step %zu: missing text", index);
        int64_t number = 0;
        if (!strcmp(kind, "move") && (!python_int(fusion_json_member(step, "x"), &number) ||
                                      !python_int(fusion_json_member(step, "y"), &number)))
            return feb_fail(error, FEB_EXIT_OPERATION, "step %zu: invalid destination", index);
        const FusionJson *flag = fusion_json_member(step, "flag");
        if (!strcmp(kind, "set_flag") && (!flag || flag->kind != FUSION_JSON_STRING))
            return feb_fail(error, FEB_EXIT_OPERATION, "step %zu: invalid flag", index);
    }
    *count = (int64_t)steps->count;
    return true;
}

bool feb_load_toml(const char *path, size_t limit, FusionJson *data, char **text,
                   FebError *error) {
    char *content = NULL;
    size_t size = 0;
    if (!feb_read_text(path, limit, &content, &size))
        return feb_fail(error, FEB_EXIT_OPERATION, "[Errno 2] No such file or directory: '%s'",
                        path);
    char detail[FEB_MESSAGE_CAPACITY - 64];
    if (!fusion_toml_parse(content, size, data, detail, sizeof detail)) {
        free(content);
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", detail);
    }
    if (text) *text = content;
    else free(content);
    return true;
}

/* _reject_reference_symlinks(): no component below the root is a symlink. */
static bool reject_symlinks(const char *root, const char *relative, FebError *error) {
    char current[FEB_PATH_CAPACITY], copy[FEB_PATH_CAPACITY];
    snprintf(current, sizeof current, "%s", root);
    snprintf(copy, sizeof copy, "%s", relative);
    for (char *part = strtok(copy, "/"); part; part = strtok(NULL, "/")) {
        size_t used = strlen(current);
        if (used + strlen(part) + 2 > sizeof current)
            return feb_fail(error, FEB_EXIT_OPERATION, "path too long");
        current[used] = '/';
        memcpy(current + used + 1, part, strlen(part) + 1);
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "symlink story reference component refused: %s", current);
    }
    return true;
}

static int compare_names(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

bool feb_story_ids(const char *root, const char *kind, FusionJson *ids, FebError *error) {
    *ids = fusion_json_array();
    char path[FEB_PATH_CAPACITY];
    long long size = 0;
    if (!strcmp(kind, "timeline")) {
        if (!reject_symlinks(root, "data/story/timeline.toml", error) ||
            !feb_join(path, root, "data/story/timeline.toml"))
            return false;
        if (!feb_is_regular(path, &size) || feb_is_symlink(path) || size > 1000000) return true;
        FusionJson data;
        int64_t count = 0;
        if (!feb_load_toml(path, 1000000, &data, NULL, error)) return false;
        bool ok = feb_validate_timeline(&data, &count, error);
        const FusionJson *events = fusion_json_member(&data, "events");
        for (size_t index = 0; ok && index < events->count; ++index)
            ok = set_add(ids, fusion_json_member(&events->items[index], "id")->string) ||
                 feb_oom(error);
        fusion_json_free(&data);
        return ok;
    }
    char folder[FEB_PATH_CAPACITY];
    if (!reject_symlinks(root, "data/cutscenes", error) ||
        !feb_join(folder, root, "data/cutscenes"))
        return false;
    if (!feb_is_directory(folder) || feb_is_symlink(folder)) return true;
    DIR *directory = opendir(folder);
    if (!directory) return true;
    char *names[4096];
    size_t count = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL && count < COUNT(names)) {
        size_t length = strlen(entry->d_name);
        if (length < 5 || strcmp(entry->d_name + length - 5, ".toml")) continue;
        names[count] = strdup(entry->d_name);
        if (names[count]) ++count;
    }
    closedir(directory);
    qsort(names, count, sizeof names[0], compare_names);
    bool ok = true;
    for (size_t index = 0; index < count; ++index) {
        char relative[FEB_PATH_CAPACITY];
        snprintf(relative, sizeof relative, "data/cutscenes/%s", names[index]);
        if (ok && (!reject_symlinks(root, relative, error) || !feb_join(path, root, relative)))
            ok = false;
        if (ok && (feb_is_symlink(path) || !feb_is_regular(path, &size) || size > 1000000))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "unsafe cutscene reference source: %s", path);
        FusionJson data = {0};
        int64_t steps = 0;
        if (ok) ok = feb_load_toml(path, 1000000, &data, NULL, error) &&
                     feb_validate_scene(&data, &steps, error);
        if (ok) {
            const char *id = fusion_json_member(&data, "id")->string;
            if (feb_set_contains(ids, id))
                ok = feb_fail(error, FEB_EXIT_OPERATION, "duplicate cutscene ID: %s", id);
            else
                ok = set_add(ids, id) || feb_oom(error);
        }
        fusion_json_free(&data);
        free(names[index]);
    }
    if (!ok) fusion_json_free(ids);
    return ok;
}

bool feb_timeline_flags(const char *root, FusionJson *flags, FebError *error) {
    *flags = fusion_json_array();
    char path[FEB_PATH_CAPACITY];
    long long size = 0;
    if (!reject_symlinks(root, "data/story/timeline.toml", error) ||
        !feb_join(path, root, "data/story/timeline.toml"))
        return false;
    if (!feb_is_regular(path, &size) || feb_is_symlink(path) || size > 1000000) return true;
    FusionJson data;
    int64_t count = 0;
    if (!feb_load_toml(path, 1000000, &data, NULL, error)) return false;
    bool ok = feb_validate_timeline(&data, &count, error);
    const FusionJson *events = fusion_json_member(&data, "events");
    for (size_t index = 0; ok && index < events->count; ++index) {
        const FusionJson *sets = list_or_empty(&events->items[index], "sets");
        for (size_t flag = 0; ok && flag < sets->count; ++flag)
            if (sets->items[flag].kind == FUSION_JSON_STRING)
                ok = set_add(flags, sets->items[flag].string) || feb_oom(error);
    }
    fusion_json_free(&data);
    return ok;
}
