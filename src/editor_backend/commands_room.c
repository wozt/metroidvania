/* SPDX-License-Identifier: GPL-3.0-only */
/* The project-room, story and project-validation commands of the native
 * editor backend (formerly scripts/editor_backend.py). */
#define _DEFAULT_SOURCE
#include "commands_room.h"

#include "catalog.h"
#include "drafts.h"
#include "project_room.h"
#include "story.h"

#include "fusion/assets/rom.h"

#include <dirent.h>
#include <stdarg.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

enum { ROOM_LIMIT_BYTES = 4000000 };

static const char *const room_commands[] = {
    "collision-capabilities", "collision-list", "collision-get", "collision-set",
    "collision-fill", "collision-stroke", "collision-clear", "collision-validate",
    "event-list", "event-inspect", "event-validate", "event-create", "event-update",
    "event-delete", "door-list", "door-target-list", "connection-list", "door-return-plan",
    "door-inspect", "door-create", "door-adopt", "door-update", "door-delete", "door-link",
    "transition-list", "transition-create", "transition-update", "transition-delete",
    "transition-validate", "entity-list", "entity-inspect", "entity-create", "entity-move",
    "entity-update", "entity-delete", "entity-assign", "entity-item-settings", "story-validate", "story-save", "project-validate",
    "room-export",
};

bool feb_is_room_command(const char *command) {
    for (size_t index = 0; index < COUNT(room_commands); ++index)
        if (!strcmp(room_commands[index], command)) return true;
    return false;
}

static int64_t integer(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    return value ? value->integer : 0;
}

static const char *string(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    return value && value->kind == FUSION_JSON_STRING ? value->string : NULL;
}

static bool is_decimal(const FusionJson *value) {
    /* str(value).isdecimal() */
    if (!value) return false;
    if (value->kind == FUSION_JSON_NUMBER) return value->integral && value->integer >= 0;
    if (value->kind != FUSION_JSON_STRING || !value->string[0]) return false;
    for (const char *c = value->string; *c; ++c)
        if (*c < '0' || *c > '9') return false;
    return true;
}

static int lower_compare(const char *left, const char *right) {
    for (;; ++left, ++right) {
        int a = *left >= 'A' && *left <= 'Z' ? *left - 'A' + 'a' : *left;
        int b = *right >= 'A' && *right <= 'Z' ? *right - 'A' + 'a' : *right;
        if (a != b || !a) return a - b;
    }
}

/* A Zero Mission area given by index or name, or an Aria area index. */
static bool scope_area(const char *world, const FusionJson *value, const char *label,
                       const char *unknown, char *area, FebError *error) {
    int64_t index = 0;
    if (!strcmp(world, "mzm")) {
        if (!value || value->kind == FUSION_JSON_NULL)
            return feb_fail(error, FEB_EXIT_OPERATION, "%s is required", label);
        if (is_decimal(value)) {
            if (!feb_integer(value, label, 0, 6, &index, error)) return false;
            snprintf(area, 32, "%s", feb_mzm_areas[index]);
            return true;
        }
        for (size_t candidate = 0; candidate < 7; ++candidate) {
            if (value->kind == FUSION_JSON_STRING &&
                !lower_compare(feb_mzm_areas[candidate], value->string)) {
                snprintf(area, 32, "%s", feb_mzm_areas[candidate]);
                return true;
            }
        }
        return feb_fail(error, FEB_EXIT_OPERATION, "%s", unknown);
    }
    if (!feb_integer(value, label, 0, 11, &index, error)) return false;
    snprintf(area, 32, "%lld", (long long)index);
    return true;
}

/* _entity_scope(). */
static bool entity_scope(const FusionJson *options, FebRoomScope *scope, FebError *error) {
    const char *public_world = NULL;
    if (!feb_world(feb_option(options, "world"), &public_world, error)) return false;
    scope->world = !strcmp(public_world, "zero_mission") ? "mzm" : "aria";
    return scope_area(scope->world, feb_option(options, "area"), "area",
                      "unknown Zero Mission area", scope->area, error) &&
           feb_integer(feb_option(options, "room"), "room", 0, 999, &scope->room, error) &&
           feb_integer(feb_option(options, "width"), "width", 16, 16384, &scope->width, error) &&
           feb_integer(feb_option(options, "height"), "height", 16, 16384, &scope->height, error);
}

static const char *public_world(const char *world) {
    return !strcmp(world, "mzm") ? "zero_mission" : "aria";
}

/* _filter_native_rooms(public world, area) matches for one room. */
static bool native_room_count(const char *repository, const char *world, const char *area,
                              int64_t room, size_t *matches, FebError *error) {
    FusionJson area_value = fusion_json_string(area), rooms;
    bool ok = area_value.string &&
              feb_filter_native_rooms(repository, public_world(world), &area_value, &rooms, error);
    fusion_json_free(&area_value);
    if (!ok) return false;
    *matches = 0;
    for (size_t index = 0; index < rooms.count; ++index)
        if (integer(&rooms.items[index], "room") == room) ++*matches;
    fusion_json_free(&rooms);
    return true;
}

/* _saved_target_doors(): the doors of the saved destination, by id. */
static bool saved_target_doors(const char *root, const char *world, const char *area,
                               int64_t room, FusionJson *doors, FebError *error) {
    static const char *const messages[4] = {
        "invalid saved destination room document", "invalid saved destination room document",
        "invalid saved destination room document", "invalid saved destination room geometry"};
    FusionJson document;
    *doors = fusion_json_array();
    if (!feb_room_saved(root, world, area, room, false, messages, &document, error)) return false;
    if (document.kind != FUSION_JSON_OBJECT) return true;
    FusionJson *list = fusion_json_member_mut(&document, "doors");
    /* sorted(..., key=id) is stable. */
    for (size_t index = 1; index < list->count; ++index) {
        FusionJson item = list->items[index];
        size_t position = index;
        while (position > 0 && integer(&list->items[position - 1], "id") > integer(&item, "id")) {
            list->items[position] = list->items[position - 1];
            --position;
        }
        list->items[position] = item;
    }
    *doors = *list;
    *list = fusion_json_null();
    fusion_json_free(&document);
    return true;
}

typedef struct {
    const char *world;
    char area[32];
    int64_t room, door, spawn_x, spawn_y;
} Target;

static bool target_json(const Target *target, FusionJson *object) {
    *object = fusion_json_object();
    return feb_put_string(object, "target_world", target->world) &&
           feb_put_string(object, "target_area", target->area) &&
           feb_put_int(object, "target_room", target->room) &&
           feb_put_int(object, "target_door_id", target->door) &&
           feb_put_int(object, "spawn_x", target->spawn_x) &&
           feb_put_int(object, "spawn_y", target->spawn_y);
}

/* _transition_target(). */
static bool transition_target(const char *root, const char *repository,
                              const FusionJson *world_value, const FusionJson *area_value,
                              const FusionJson *room_value, const FusionJson *door_value,
                              const FusionJson *spawn_x, const FusionJson *spawn_y,
                              Target *target, FebError *error) {
    const char *world = NULL;
    if (!feb_world(world_value, &world, error)) return false;
    target->world = !strcmp(world, "zero_mission") ? "mzm" : "aria";
    FusionJson zero = fusion_json_int(0);
    if (!scope_area(target->world, area_value, "target_area", "unknown Zero Mission target area",
                    target->area, error) ||
        !feb_integer(room_value, "target_room", 0, 999, &target->room, error))
        return false;
    size_t matches = 0;
    if (!native_room_count(repository, target->world, target->area, target->room, &matches, error))
        return false;
    if (matches != 1)
        return feb_fail(error, FEB_EXIT_OPERATION, "transition target room is missing or ambiguous");
    if (!feb_integer(door_value ? door_value : &zero, "target_door_id", 0, 999999, &target->door,
                     error) ||
        !feb_integer(spawn_x ? spawn_x : &zero, "spawn_x", 0, 16383, &target->spawn_x, error) ||
        !feb_integer(spawn_y ? spawn_y : &zero, "spawn_y", 0, 16383, &target->spawn_y, error))
        return false;
    if (target->door) {
        FusionJson doors;
        if (!saved_target_doors(root, target->world, target->area, target->room, &doors, error))
            return false;
        bool found = false;
        for (size_t index = 0; index < doors.count; ++index)
            if (integer(&doors.items[index], "id") == target->door) found = true;
        fusion_json_free(&doors);
        if (!found)
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "target project door ID is not present in the saved target room");
    }
    return true;
}

static bool options_target(const char *root, const char *repository, const FusionJson *options,
                           Target *target, FebError *error) {
    return transition_target(root, repository, feb_option(options, "target_world"),
                             feb_option(options, "target_area"),
                             feb_option(options, "target_room"),
                             feb_option(options, "target_door_id"), feb_option(options, "spawn_x"),
                             feb_option(options, "spawn_y"), target, error);
}

/* _transition_validation(). */
static bool transition_validation(const char *root, const char *repository,
                                  const FusionJson *document, FusionJson *records,
                                  FebError *error) {
    *records = fusion_json_array();
    const FusionJson *transitions = fusion_json_member(document, "transitions");
    for (size_t index = 0; index < transitions->count; ++index) {
        const FusionJson *transition = &transitions->items[index];
        const char *world = string(transition, "target_world");
        const char *area = string(transition, "target_area");
        int64_t room = integer(transition, "target_room"), door = integer(transition, "target_door_id");
        size_t matches = 0;
        if (!native_room_count(repository, world, area, room, &matches, error)) {
            fusion_json_free(records);
            return false;
        }
        bool found = door == 0;
        if (!found && matches == 1) {
            FusionJson doors;
            if (!saved_target_doors(root, world, area, room, &doors, error)) {
                fusion_json_free(records);
                return false;
            }
            for (size_t other = 0; other < doors.count; ++other)
                if (integer(&doors.items[other], "id") == door) found = true;
            fusion_json_free(&doors);
        }
        char label[96];
        snprintf(label, sizeof label, "%s:%s:%lld", public_world(world), area, (long long)room);
        FusionJson record = fusion_json_object();
        bool ok = feb_put_int(&record, "id", integer(transition, "id")) &&
                  feb_put_int(&record, "source_door_id", integer(transition, "source_door_id")) &&
                  feb_put_bool(&record, "target_exists", matches == 1) &&
                  feb_put_string(&record, "target", label) &&
                  feb_put_string(&record, "target_door_status",
                                 door == 0 ? "unspecified"
                                 : found ? "saved_project_door_verified"
                                         : "missing_target_project_door") &&
                  feb_put_string(&record, "engine_adapter", "unavailable") &&
                  fusion_json_push(records, record);
        if (!ok) {
            fusion_json_free(records);
            return feb_oom(error);
        }
    }
    return true;
}

/* A canonical decimal reference target: isdecimal() and str(int(x)) == x. */
static bool canonical_id(const char *target, int64_t *value) {
    if (!*target) return false;
    for (const char *c = target; *c; ++c)
        if (*c < '0' || *c > '9') return false;
    if (target[0] == '0' && target[1]) return false;
    if (strlen(target) > 18) return false;
    *value = strtoll(target, NULL, 10);
    return true;
}

static bool has_item(const FusionJson *document, const char *list_key, int64_t id) {
    const FusionJson *list = fusion_json_member(document, list_key);
    for (size_t index = 0; index < list->count; ++index)
        if (integer(&list->items[index], "id") == id) return true;
    return false;
}

static bool checkpoint_key(const char *target) {
    size_t length = strlen(target);
    if (length < 1 || length > 64 || target[0] < 'a' || target[0] > 'z') return false;
    for (size_t index = 1; index < length; ++index) {
        char c = target[index];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == '-'))
            return false;
    }
    return true;
}

typedef struct {
    FusionJson timeline, cutscene, flags;
    bool has_timeline, has_cutscene, has_flags;
} StoryIds;

static void free_story(StoryIds *story) {
    fusion_json_free(&story->timeline);
    fusion_json_free(&story->cutscene);
    fusion_json_free(&story->flags);
}

/* _event_reference_validation() and _condition_reference_validation(). */
static bool event_record(const char *root, const FusionJson *document, const FusionJson *event,
                         StoryIds *story, FusionJson *record, FebError *error) {
    const char *reference = string(event, "action_ref");
    const char *action = string(event, "action_type");
    const char *colon = strchr(reference, ':');
    char prefix[96] = "";
    const char *target = colon ? colon + 1 : "";
    if (colon) snprintf(prefix, sizeof prefix, "%.*s", (int)(colon - reference), reference);
    const char *status = "invalid_reference_syntax";
    char expected[64];
    bool valid = false;
    int64_t id = 0;
    if (colon && *target) {
        if (!strcmp(action, "story") && (!strcmp(prefix, "timeline") || !strcmp(prefix, "cutscene"))) {
            bool timeline = !strcmp(prefix, "timeline");
            FusionJson *ids = timeline ? &story->timeline : &story->cutscene;
            bool *loaded = timeline ? &story->has_timeline : &story->has_cutscene;
            if (!*loaded) {
                if (!feb_story_ids(root, prefix, ids, error)) return false;
                *loaded = true;
            }
            valid = feb_set_contains(ids, target);
            status = timeline ? (valid ? "verified_timeline" : "missing_timeline")
                              : (valid ? "verified_cutscene" : "missing_cutscene");
        } else if (!strcmp(action, "spawn") && !strcmp(prefix, "entity")) {
            valid = canonical_id(target, &id) && has_item(document, "entities", id);
            status = valid ? "verified_project_entity" : "missing_project_entity";
        } else if (!strcmp(action, "toggle") && !strcmp(prefix, "event")) {
            valid = canonical_id(target, &id) && id != integer(event, "id") &&
                    has_item(document, "events", id);
            status = valid ? "verified_project_event" : "missing_or_self_project_event";
        } else if (!strcmp(action, "transition") && !strcmp(prefix, "transition")) {
            valid = canonical_id(target, &id) && has_item(document, "transitions", id);
            status = valid ? "verified_project_transition" : "missing_project_transition";
        } else if (!strcmp(action, "checkpoint") && !strcmp(prefix, "checkpoint")) {
            valid = checkpoint_key(target);
            status = valid ? "valid_checkpoint_key" : "invalid_checkpoint_key";
        } else {
            snprintf(expected, sizeof expected, "expected_%s_reference", action);
            status = expected;
        }
    }
    const FusionJson *conditions = fusion_json_member(event, "conditions");
    FusionJson records = fusion_json_array();
    bool conditions_valid = true;
    for (size_t index = 0; index < conditions->count; ++index) {
        const FusionJson *condition = &conditions->items[index];
        const char *ref = string(condition, "ref"), *type = string(condition, "type");
        const char *separator = strchr(ref, ':');
        char head[96] = "", wanted[64];
        const char *tail = separator ? separator + 1 : "";
        if (separator) snprintf(head, sizeof head, "%.*s", (int)(separator - ref), ref);
        const char *condition_status = "invalid_reference_syntax";
        bool condition_valid = false;
        if (separator && *tail) {
            if (!strcmp(type, "story_flag") && !strcmp(head, "flag")) {
                if (!story->has_flags) {
                    if (!feb_timeline_flags(root, &story->flags, error)) {
                        fusion_json_free(&records);
                        return false;
                    }
                    story->has_flags = true;
                }
                condition_valid = feb_set_contains(&story->flags, tail);
                condition_status = condition_valid ? "verified_story_flag" : "missing_story_flag";
            } else if (!strcmp(type, "event_complete") && !strcmp(head, "event")) {
                condition_valid = canonical_id(tail, &id) && id != integer(event, "id") &&
                                  has_item(document, "events", id);
                condition_status = condition_valid ? "verified_project_event"
                                                   : "missing_or_self_project_event";
            } else if (!strcmp(type, "entity_present") && !strcmp(head, "entity")) {
                condition_valid = canonical_id(tail, &id) && has_item(document, "entities", id);
                condition_status = condition_valid ? "verified_project_entity"
                                                   : "missing_project_entity";
            } else if (!strcmp(type, "transition_ready") && !strcmp(head, "transition")) {
                condition_valid = canonical_id(tail, &id) && has_item(document, "transitions", id);
                condition_status = condition_valid ? "verified_project_transition"
                                                   : "missing_project_transition";
            } else if (!strcmp(type, "checkpoint_active") && !strcmp(head, "checkpoint")) {
                condition_valid = checkpoint_key(tail);
                condition_status = condition_valid ? "valid_checkpoint_key"
                                                   : "invalid_checkpoint_key";
            } else {
                snprintf(wanted, sizeof wanted, "expected_%s_reference", type);
                condition_status = wanted;
            }
        }
        conditions_valid = conditions_valid && condition_valid;
        FusionJson item = fusion_json_object();
        bool ok = feb_put_int(&item, "index", (int64_t)index) && feb_put_string(&item, "type", type) &&
                  feb_put_string(&item, "ref", ref) &&
                  feb_put_bool(&item, "negated", fusion_json_member(condition, "negated")->boolean) &&
                  feb_put_bool(&item, "valid", condition_valid) &&
                  feb_put_string(&item, "reference_status", condition_status) &&
                  fusion_json_push(&records, item);
        if (!ok) {
            fusion_json_free(&records);
            return feb_oom(error);
        }
    }
    *record = fusion_json_object();
    bool ok = feb_put_int(record, "id", integer(event, "id")) &&
              feb_put_string(record, "action_type", action) &&
              feb_put_string(record, "action_ref", reference) &&
              feb_put_bool(record, "valid", valid && conditions_valid) &&
              feb_put_string(record, "reference_status", status) &&
              feb_put_string(record, "engine_adapter", "unavailable") &&
              feb_put_bool(record, "action_valid", valid) &&
              feb_put_string(record, "condition_mode", string(event, "condition_mode")) &&
              feb_put_int(record, "condition_count", (int64_t)records.count) &&
              feb_put_bool(record, "conditions_valid", conditions_valid) &&
              feb_put(record, "conditions", records);
    if (!ok) {
        fusion_json_free(record);
        return feb_oom(error);
    }
    return true;
}

/* _event_validation(): story sources are read only when an event needs them. */
static bool event_validation(const char *root, const FusionJson *document, FusionJson *records,
                             FebError *error) {
    StoryIds story = {fusion_json_array(), fusion_json_array(), fusion_json_array(), false, false,
                      false};
    const FusionJson *events = fusion_json_member(document, "events");
    /* The former backend loaded every needed source first. */
    bool need_timeline = false, need_cutscene = false, need_flags = false;
    for (size_t index = 0; index < events->count; ++index) {
        const FusionJson *event = &events->items[index];
        const char *reference = string(event, "action_ref");
        if (!strcmp(string(event, "action_type"), "story")) {
            if (!strncmp(reference, "timeline:", 9) || !strcmp(reference, "timeline"))
                need_timeline = true;
            if (!strncmp(reference, "cutscene:", 9) || !strcmp(reference, "cutscene"))
                need_cutscene = true;
        }
        const FusionJson *conditions = fusion_json_member(event, "conditions");
        for (size_t entry = 0; entry < conditions->count; ++entry)
            if (!strcmp(string(&conditions->items[entry], "type"), "story_flag")) need_flags = true;
    }
    bool ok = true;
    if (need_cutscene) {
        ok = feb_story_ids(root, "cutscene", &story.cutscene, error);
        story.has_cutscene = ok;
    }
    if (ok && need_timeline) {
        ok = feb_story_ids(root, "timeline", &story.timeline, error);
        story.has_timeline = ok;
    }
    if (ok && need_flags) {
        ok = feb_timeline_flags(root, &story.flags, error);
        story.has_flags = ok;
    }
    story.has_flags = true;  /* an unneeded flag set stays empty */
    *records = fusion_json_array();
    for (size_t index = 0; ok && index < events->count; ++index) {
        FusionJson record;
        ok = event_record(root, document, &events->items[index], &story, &record, error) &&
             (fusion_json_push(records, record) || feb_oom(error));
    }
    free_story(&story);
    if (!ok) fusion_json_free(records);
    return ok;
}

/* _event_conditions(): the bounded JSON wire form of the conditions. */
static bool event_conditions(const FusionJson *value, FusionJson *conditions, FebError *error) {
    if (!value || value->kind == FUSION_JSON_NULL) {
        *conditions = fusion_json_array();
        return true;
    }
    if (value->kind == FUSION_JSON_STRING) {
        size_t length = feb_utf8_length(value->string);
        if (length < 2 || length > 4096)
            return feb_fail(error, FEB_EXIT_OPERATION, "conditions must be a bounded JSON array");
        char detail[256];
        if (!fusion_json_parse(value->string, strlen(value->string), conditions, detail,
                               sizeof detail))
            return feb_fail(error, FEB_EXIT_OPERATION, "conditions must be valid JSON");
    } else if (value->kind == FUSION_JSON_ARRAY) {
        char *text = NULL;
        size_t size = 0;
        if (!fusion_json_write(value, FUSION_JSON_COMPACT, &text, &size)) return feb_oom(error);
        free(text);
        if (size > 4096)
            return feb_fail(error, FEB_EXIT_OPERATION, "conditions must be a bounded JSON array");
        if (!fusion_json_clone(value, conditions)) return feb_oom(error);
    } else {
        return feb_fail(error, FEB_EXIT_OPERATION, "conditions must be a JSON array");
    }
    if (conditions->kind != FUSION_JSON_ARRAY || conditions->count > FEB_MAX_EVENT_CONDITIONS) {
        fusion_json_free(conditions);
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid project event condition count");
    }
    for (size_t index = 0; index < conditions->count; ++index) {
        const FusionJson *condition = &conditions->items[index];
        const FusionJson *type = fusion_json_member(condition, "type");
        const FusionJson *ref = fusion_json_member(condition, "ref");
        const FusionJson *negated = fusion_json_member(condition, "negated");
        bool known = false;
        for (size_t candidate = 0; type && type->kind == FUSION_JSON_STRING && candidate < 5;
             ++candidate)
            if (!strcmp(type->string, feb_condition_types[candidate])) known = true;
        bool reference = ref && ref->kind == FUSION_JSON_STRING && ref->string[0] &&
                         strlen(ref->string) <= 80;
        for (const char *c = reference ? ref->string : ""; *c; ++c)
            if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') ||
                  (*c >= '0' && *c <= '9') || strchr("_.:-", *c)))
                reference = false;
        if (condition->kind != FUSION_JSON_OBJECT || condition->count != 3 || !type || !ref ||
            !negated || !known || !reference || negated->kind != FUSION_JSON_BOOL) {
            fusion_json_free(conditions);
            return feb_fail(error, FEB_EXIT_OPERATION, "invalid project event condition");
        }
    }
    return true;
}

/* _collision_points(): "x,y;x,y". */
static bool collision_points(const FusionJson *value, int64_t (**points)[2], size_t *count,
                             FebError *error) {
    if (!value || value->kind != FUSION_JSON_STRING || !value->string[0] ||
        feb_utf8_length(value->string) > 49151)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "collision points must be a non-empty bounded string");
    size_t total = 1;
    for (const char *c = value->string; *c; ++c) total += *c == ';';
    if (total > 4096)
        return feb_fail(error, FEB_EXIT_OPERATION, "collision stroke must contain 1..4096 points");
    *points = calloc(total, sizeof **points);
    if (!*points) return feb_oom(error);
    *count = 0;
    char *copy = strdup(value->string);
    if (!copy) {
        free(*points);
        return feb_oom(error);
    }
    bool ok = true;
    char *cursor = copy;
    for (size_t index = 0; ok && index < total; ++index) {
        char *end = strchr(cursor, ';');
        if (end) *end = '\0';
        char *comma = strchr(cursor, ',');
        if (!comma || strchr(comma + 1, ',')) {
            ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid collision stroke point");
            break;
        }
        *comma = '\0';
        FusionJson x = fusion_json_string(cursor), y = fusion_json_string(comma + 1);
        ok = x.string && y.string &&
             feb_integer(&x, "collision x", 0, 2047, &(*points)[*count][0], error) &&
             feb_integer(&y, "collision y", 0, 2047, &(*points)[*count][1], error);
        fusion_json_free(&x);
        fusion_json_free(&y);
        if (ok) ++*count;
        cursor = end ? end + 1 : cursor + strlen(cursor);
    }
    free(copy);
    if (!ok) free(*points);
    return ok;
}

/* The unique stroke points in their first order. */
static size_t unique_points(int64_t (*points)[2], size_t count) {
    size_t kept = 0;
    for (size_t index = 0; index < count; ++index) {
        bool seen = false;
        for (size_t other = 0; other < kept && !seen; ++other)
            seen = points[other][0] == points[index][0] && points[other][1] == points[index][1];
        if (!seen) {
            points[kept][0] = points[index][0];
            points[kept][1] = points[index][1];
            ++kept;
        }
    }
    return kept;
}

static bool copy_option(FusionJson *object, const char *key, const FusionJson *value) {
    FusionJson copy;
    if (!value) return fusion_json_set(object, key, fusion_json_null());
    return fusion_json_clone(value, &copy) && fusion_json_set(object, key, copy);
}

static bool persisted(const char *root, FusionJson *document, FusionJson *data, bool dry_run,
                      bool write, FebError *error) {
    if (dry_run || !write) return true;
    char path[FEB_PATH_CAPACITY];
    if (!feb_room_save(root, document, path, error)) return false;
    return (feb_put_bool(data, "persisted", true) && feb_put_string(data, "path", path)) ||
           feb_oom(error);
}

static bool command_collision(const char *command, const char *root, const FusionJson *options,
                              FusionJson *document, bool dry_run, FusionJson *data,
                              FebError *error) {
    int64_t resolution = integer(fusion_json_member(document, "collision"), "resolution_px");
    const FusionJson *cells = fusion_json_member(fusion_json_member(document, "collision"), "cells");
    if (!strcmp(command, "collision-list")) {
        FusionJson copy;
        return (fusion_json_clone(cells, &copy) && feb_put_int(data, "resolution_px", resolution) &&
                feb_put_int(data, "grid_width", integer(document, "width_px") / resolution) &&
                feb_put_int(data, "grid_height", integer(document, "height_px") / resolution) &&
                feb_put_int(data, "count", (int64_t)cells->count) && feb_put(data, "cells", copy) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    if (!strcmp(command, "collision-validate")) {
        FusionJson types = fusion_json_array();
        bool ok = true;
        for (size_t index = 0; ok && index < 7; ++index)
            ok = fusion_json_push(&types, fusion_json_string(feb_collision_types[index]));
        return (ok && feb_put_string(data, "status", "valid_project_data_not_playable") &&
                feb_put_int(data, "resolution_px", resolution) &&
                feb_put_int(data, "cell_count", (int64_t)cells->count) &&
                feb_put(data, "accepted_types", types) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    int64_t changed = 0;
    if (!strcmp(command, "collision-stroke")) {
        int64_t (*points)[2] = NULL;
        size_t count = 0;
        const FusionJson *type = feb_option(options, "collision_type");
        if (!collision_points(feb_option(options, "points"), &points, &count, error)) return false;
        size_t unique = unique_points(points, count);
        bool ok = feb_collision_stroke(document, (const int64_t (*)[2])points, unique, type,
                                       &changed, error);
        free(points);
        if (!ok) return false;
        ok = feb_put_int(data, "points", (int64_t)unique) && copy_option(data, "type", type) &&
             feb_put_int(data, "changed_cells", changed) && feb_put_bool(data, "persisted", false) &&
             feb_put_string(data, "engine_adapter", "unavailable");
        if (!ok) return feb_oom(error);
        return persisted(root, document, data, dry_run, changed != 0, error);
    }
    int64_t x = 0, y = 0;
    if (!feb_integer(feb_option(options, "x"), "x", 0, 2047, &x, error) ||
        !feb_integer(feb_option(options, "y"), "y", 0, 2047, &y, error))
        return false;
    if (!strcmp(command, "collision-get")) {
        const char *type = NULL;
        if (!feb_collision_get(document, x, y, &type, error)) return false;
        return (feb_put_int(data, "x", x) && feb_put_int(data, "y", y) &&
                feb_put_string(data, "type", type) &&
                feb_put_int(data, "resolution_px", resolution)) || feb_oom(error);
    }
    FusionJson one = fusion_json_int(1);
    const FusionJson *fill_width = feb_option(options, "fill_width");
    const FusionJson *fill_height = feb_option(options, "fill_height");
    bool fill = !strcmp(command, "collision-fill");
    int64_t width = 0, height = 0;
    if (!feb_integer(fill_width ? fill_width : fill ? NULL : &one, "fill_width", 1, 2048, &width,
                     error) ||
        !feb_integer(fill_height ? fill_height : fill ? NULL : &one, "fill_height", 1, 2048,
                     &height, error))
        return false;
    FusionJson type_value;
    if (!strcmp(command, "collision-clear")) {
        const FusionJson *confirm = feb_option(options, "confirm");
        if (!confirm || confirm->kind != FUSION_JSON_BOOL || !confirm->boolean)
            return feb_fail(error, FEB_EXIT_OPERATION, "collision-clear requires --confirm=true");
        if (!feb_collision_clear(document, x, y, width, height, &changed, error)) return false;
        type_value = fusion_json_string("empty");
    } else {
        const FusionJson *type = feb_option(options, "collision_type");
        if (!feb_collision_fill(document, x, y, width, height, type, &changed, error)) return false;
        if (!fusion_json_clone(type, &type_value)) return feb_oom(error);
    }
    bool ok = feb_put_int(data, "x", x) && feb_put_int(data, "y", y) &&
              feb_put_int(data, "width", width) && feb_put_int(data, "height", height) &&
              feb_put(data, "type", type_value) && feb_put_int(data, "changed_cells", changed) &&
              feb_put_bool(data, "persisted", false) &&
              feb_put_string(data, "engine_adapter", "unavailable");
    if (!ok) return feb_oom(error);
    return persisted(root, document, data, dry_run, changed != 0, error);
}

static bool confirmed(const FusionJson *options) {
    const FusionJson *confirm = feb_option(options, "confirm");
    return confirm && confirm->kind == FUSION_JSON_BOOL && confirm->boolean;
}

static bool deleted(int64_t id, FusionJson *changed) {
    *changed = fusion_json_object();
    return feb_put_int(changed, "id", id) && feb_put_bool(changed, "deleted", true);
}

/* The event fields of event-create, or the given ones of event-update. */
static bool event_fields(const FusionJson *options, bool update, FusionJson *fields,
                         FebError *error) {
    *fields = fusion_json_object();
    static const char *const direct[][2] = {
        {"event_kind", "kind"}, {"label", "label"}, {"trigger_type", "trigger_type"},
        {"action_type", "action_type"}, {"action_ref", "action_ref"}, {"once", "once"},
        {"condition_mode", "condition_mode"}};
    static const char *const numeric[][2] = {
        {"x", "x"}, {"y", "y"}, {"region_width", "width"}, {"region_height", "height"}};
    bool ok = true;
    if (!update) {
        int64_t values[4];
        for (size_t index = 0; ok && index < 4; ++index) {
            bool position = index < 2;
            ok = feb_integer(feb_option(options, numeric[index][0]), numeric[index][0],
                             position ? 0 : 1, position ? 16383 : 16384, &values[index], error);
        }
        FusionJson conditions;
        ok = ok && event_conditions(feb_option(options, "conditions"), &conditions, error);
        if (!ok) {
            fusion_json_free(fields);
            return false;
        }
        const FusionJson *mode = feb_option(options, "condition_mode");
        FusionJson default_mode = fusion_json_string("all");
        ok = copy_option(fields, "kind", feb_option(options, "event_kind")) &&
             feb_put_int(fields, "x", values[0]) && feb_put_int(fields, "y", values[1]) &&
             feb_put_int(fields, "width", values[2]) && feb_put_int(fields, "height", values[3]) &&
             copy_option(fields, "label", feb_option(options, "label")) &&
             copy_option(fields, "trigger_type", feb_option(options, "trigger_type")) &&
             copy_option(fields, "action_type", feb_option(options, "action_type")) &&
             copy_option(fields, "action_ref", feb_option(options, "action_ref")) &&
             copy_option(fields, "once", feb_option(options, "once")) &&
             copy_option(fields, "condition_mode", mode ? mode : &default_mode) &&
             feb_put(fields, "conditions", conditions);
        fusion_json_free(&default_mode);
        if (!ok) {
            fusion_json_free(fields);
            return feb_oom(error);
        }
        return true;
    }
    for (size_t index = 0; ok && index < COUNT(direct); ++index)
        if (feb_option(options, direct[index][0]))
            ok = copy_option(fields, direct[index][1], feb_option(options, direct[index][0])) ||
                 feb_oom(error);
    for (size_t index = 0; ok && index < COUNT(numeric); ++index) {
        if (!feb_option(options, numeric[index][0])) continue;
        bool position = index < 2;
        int64_t value = 0;
        ok = feb_integer(feb_option(options, numeric[index][0]), numeric[index][0],
                         position ? 0 : 1, position ? 16383 : 16384, &value, error) &&
             (feb_put_int(fields, numeric[index][1], value) || feb_oom(error));
    }
    if (ok && feb_option(options, "conditions")) {
        FusionJson conditions;
        ok = event_conditions(feb_option(options, "conditions"), &conditions, error) &&
             (feb_put(fields, "conditions", conditions) || feb_oom(error));
    }
    if (!ok) fusion_json_free(fields);
    return ok;
}

static bool check_event(const char *root, FusionJson *document, const FusionJson *event,
                        FebError *error) {
    StoryIds story = {fusion_json_array(), fusion_json_array(), fusion_json_array(), false, false,
                      false};
    FusionJson record;
    bool ok = event_record(root, document, event, &story, &record, error);
    free_story(&story);
    if (!ok) return false;
    if (fusion_json_member(&record, "valid")->boolean) {
        fusion_json_free(&record);
        return true;
    }
    if (!fusion_json_member(&record, "action_valid")->boolean) {
        feb_fail(error, FEB_EXIT_OPERATION, "invalid %s action reference %s: %s",
                 string(event, "action_type"), string(event, "action_ref"),
                 string(&record, "reference_status"));
        fusion_json_free(&record);
        return false;
    }
    char details[FEB_MESSAGE_CAPACITY] = "";
    const FusionJson *conditions = fusion_json_member(&record, "conditions");
    for (size_t index = 0; index < conditions->count; ++index) {
        const FusionJson *condition = &conditions->items[index];
        if (fusion_json_member(condition, "valid")->boolean) continue;
        size_t used = strlen(details);
        snprintf(details + used, sizeof details - used, "%s%lld:%s", used ? ", " : "",
                 (long long)integer(condition, "index"), string(condition, "reference_status"));
    }
    fusion_json_free(&record);
    return feb_fail(error, FEB_EXIT_OPERATION, "invalid event condition reference(s): %s", details);
}

static bool command_event(const char *command, const char *root, const FusionJson *options,
                          FusionJson *document, bool dry_run, FusionJson *data, FebError *error) {
    const FusionJson *events = fusion_json_member(document, "events");
    if (!strcmp(command, "event-list")) {
        FusionJson copy;
        return (fusion_json_clone(events, &copy) &&
                feb_put_int(data, "count", (int64_t)events->count) && feb_put(data, "events", copy) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    if (!strcmp(command, "event-validate")) {
        FusionJson records;
        if (!event_validation(root, document, &records, error)) return false;
        int64_t valid = 0;
        for (size_t index = 0; index < records.count; ++index)
            valid += fusion_json_member(&records.items[index], "valid")->boolean;
        int64_t count = (int64_t)records.count;
        return (feb_put_int(data, "count", count) && feb_put_int(data, "valid_count", valid) &&
                feb_put_int(data, "invalid_count", count - valid) &&
                feb_put(data, "events", records) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    int64_t id = 0;
    if (strcmp(command, "event-create") &&
        !feb_integer(feb_option(options, "id"), "id", 1, 999999, &id, error))
        return false;
    FusionJson changed;
    if (!strcmp(command, "event-inspect")) {
        const FusionJson *current = NULL;
        for (size_t index = 0; index < events->count; ++index)
            if (integer(&events->items[index], "id") == id) current = &events->items[index];
        if (!current) return feb_fail(error, FEB_EXIT_OPERATION, "unknown project event id");
        return (fusion_json_clone(current, &changed) && feb_put(data, "event", changed) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    if (!strcmp(command, "event-delete")) {
        if (!confirmed(options))
            return feb_fail(error, FEB_EXIT_OPERATION, "event-delete requires --confirm=true");
        if (!has_item(document, "events", id))
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown project event id");
        char ref[64];
        snprintf(ref, sizeof ref, "event:%lld", (long long)id);
        for (size_t index = 0; index < events->count; ++index) {
            const FusionJson *event = &events->items[index];
            if (integer(event, "id") == id) continue;
            if (!strcmp(string(event, "action_type"), "toggle") &&
                !strcmp(string(event, "action_ref"), ref))
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "event is referenced by another project event");
        }
        for (size_t index = 0; index < events->count; ++index) {
            const FusionJson *event = &events->items[index];
            if (integer(event, "id") == id) continue;
            const FusionJson *conditions = fusion_json_member(event, "conditions");
            for (size_t entry = 0; entry < conditions->count; ++entry)
                if (!strcmp(string(&conditions->items[entry], "type"), "event_complete") &&
                    !strcmp(string(&conditions->items[entry], "ref"), ref))
                    return feb_fail(error, FEB_EXIT_OPERATION,
                                    "event is referenced by another project event condition");
        }
        if (!feb_room_remove(document, "events", id, "unknown project event id", error) ||
            !deleted(id, &changed))
            return false;
    } else {
        FusionJson fields;
        bool update = !strcmp(command, "event-update");
        if (!event_fields(options, update, &fields, error)) return false;
        static const char *const editable[] = {
            "kind", "x", "y", "width", "height", "label", "trigger_type", "action_type",
            "action_ref", "once", "condition_mode", "conditions", NULL};
        bool ok = update
            ? feb_room_update(document, "events", id, &fields, editable,
                              "event update requires only editable event fields",
                              "unknown project event id", &changed, error)
            : feb_room_append(document, "events", "next_event_id", FEB_MAX_EVENTS,
                              "project event limit reached", fields, &changed, error);
        if (update) fusion_json_free(&fields);
        if (!ok) return false;
        if (!check_event(root, document, &changed, error)) {
            fusion_json_free(&changed);
            return false;
        }
    }
    if (!feb_put(data, "event", changed) || !feb_put_bool(data, "persisted", false))
        return feb_oom(error);
    if (!dry_run) {
        char path[FEB_PATH_CAPACITY];
        if (!feb_room_save(root, document, path, error)) return false;
        if (!feb_put_bool(data, "persisted", true) || !feb_put_string(data, "path", path))
            return feb_oom(error);
    }
    return feb_put_string(data, "engine_adapter", "unavailable") || feb_oom(error);
}

static bool door_adopt(const char *root, const FusionJson *options, FusionJson *document,
                       const FebRoomScope *scope, bool dry_run, FusionJson *data,
                       FebError *error) {
    int64_t index = 0;
    if (!feb_integer(feb_option(options, "native_index"), "native_index", 0, 999999, &index,
                     error))
        return false;
    const FusionJson *variant = feb_option(options, "native_variant");
    const FusionJson *type = feb_option(options, "native_type");
    bool valid = variant && variant->kind == FUSION_JSON_STRING && type &&
                 type->kind == FUSION_JSON_STRING;
    size_t length = valid ? strlen(variant->string) : 0;
    valid = valid && length >= 1 && length <= 47 && strlen(type->string) >= 1 &&
            strlen(type->string) <= 64;
    for (const char *c = valid ? variant->string : ""; *c; ++c)
        if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
              *c == '_' || *c == '-'))
            valid = false;
    for (const char *c = valid ? type->string : ""; *c; ++c)
        if (!((*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') ||
              strchr("_.:-", *c)))
            valid = false;
    if (!valid) return feb_fail(error, FEB_EXIT_OPERATION, "invalid original door identity");
    char path[FEB_PATH_CAPACITY];
    if (!strcmp(scope->world, "mzm")) {
        char lower[32];
        snprintf(lower, sizeof lower, "%s", scope->area);
        for (char *c = lower; *c; ++c)
            if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
        snprintf(path, sizeof path, "%s/assets/extracted/rooms/metroid/annotations/%s_%03lld.tsv",
                 root, lower, (long long)scope->room);
    } else {
        snprintf(path, sizeof path,
                 "%s/assets/extracted/rooms/aria/annotations/area_%02d_room_%03lld.tsv", root,
                 atoi(scope->area), (long long)scope->room);
    }
    long long size = 0;
    char *text = NULL;
    if (feb_is_symlink(path) || !feb_is_regular(path, &size) || size > 2000000 ||
        !feb_read_text(path, 2000000, &text, NULL))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "import original room annotations before editing native doors");
    int64_t geometry[4] = {0};
    size_t matches = 0, lines = 0;
    bool ok = true;
    for (char *line = text; ok && line && *line && lines < 4096; ++lines) {
        char *end = strchr(line, '\n');
        if (end) *end = '\0';
        char *fields[10];
        size_t count = 0;
        fields[count++] = line;
        for (char *c = line; *c && count < 10; ++c)
            if (*c == '|') {
                *c = '\0';
                fields[count++] = c + 1;
            }
        char number[32];
        snprintf(number, sizeof number, "%lld", (long long)index);
        if (count == 10 && !strcmp(fields[0], "DOOR") && !strcmp(fields[1], number) &&
            !strcmp(fields[6], variant->string) && !strcmp(fields[7], type->string)) {
            for (int field = 0; field < 4; ++field) {
                char *tail = NULL;
                geometry[field] = strtoll(fields[2 + field], &tail, 10);
                if (!fields[2 + field][0] || *tail) {
                    ok = feb_fail(error, FEB_EXIT_OPERATION, "invalid original door geometry");
                    break;
                }
            }
            ++matches;
        }
        line = end ? end + 1 : NULL;
    }
    free(text);
    if (!ok) return false;
    if (matches != 1)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "source door missing or ambiguous in private original annotations");
    FusionJson source = fusion_json_object(), door;
    if (!feb_put_int(&source, "index", index) || !copy_option(&source, "variant", variant) ||
        !copy_option(&source, "native_type", type)) {
        fusion_json_free(&source);
        return feb_oom(error);
    }
    if (!feb_door_adopt(document, source, geometry[0], geometry[1], geometry[2], geometry[3],
                        &door, error))
        return false;
    if (!feb_put(data, "door", door) || !feb_put_bool(data, "persisted", false) ||
        !feb_put_string(data, "engine_adapter", "unavailable"))
        return feb_oom(error);
    return persisted(root, document, data, dry_run, true, error);
}

static bool command_door(const char *command, const char *root, const FusionJson *options,
                         FusionJson *document, const FebRoomScope *scope, bool dry_run,
                         FusionJson *data, FebError *error) {
    const FusionJson *doors = fusion_json_member(document, "doors");
    if (!strcmp(command, "door-adopt"))
        return door_adopt(root, options, document, scope, dry_run, data, error);
    if (!strcmp(command, "door-list")) {
        FusionJson copy;
        return (fusion_json_clone(doors, &copy) && feb_put_int(data, "count", (int64_t)doors->count) &&
                feb_put(data, "doors", copy) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    int64_t id = 0;
    if (strcmp(command, "door-create") &&
        !feb_integer(feb_option(options, "id"), "id", 1, 999999, &id, error))
        return false;
    FusionJson changed;
    if (!strcmp(command, "door-inspect")) {
        const FusionJson *door = NULL, *transition = NULL;
        for (size_t index = 0; index < doors->count; ++index)
            if (integer(&doors->items[index], "id") == id) door = &doors->items[index];
        if (!door) return feb_fail(error, FEB_EXIT_OPERATION, "unknown project door id");
        const FusionJson *transitions = fusion_json_member(document, "transitions");
        for (size_t index = 0; index < transitions->count && !transition; ++index)
            if (integer(&transitions->items[index], "source_door_id") == id)
                transition = &transitions->items[index];
        FusionJson link = fusion_json_null();
        return (fusion_json_clone(door, &changed) && feb_put(data, "door", changed) &&
                (!transition || fusion_json_clone(transition, &link)) &&
                feb_put(data, "transition", link) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    if (!strcmp(command, "door-delete")) {
        if (!confirmed(options))
            return feb_fail(error, FEB_EXIT_OPERATION, "door-delete requires --confirm=true");
        const FusionJson *transitions = fusion_json_member(document, "transitions");
        for (size_t index = 0; index < transitions->count; ++index)
            if (integer(&transitions->items[index], "source_door_id") == id)
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "delete the linked transition before deleting its door");
        if (!feb_room_remove(document, "doors", id, "unknown project door id", error) ||
            !deleted(id, &changed))
            return false;
    } else {
        int64_t values[4];
        static const char *const options_names[] = {"x", "y", "door_width", "door_height"};
        static const int64_t lows[] = {0, 0, 8, 8}, highs[] = {16383, 16383, 16384, 16384};
        for (size_t index = 0; index < 4; ++index)
            if (!feb_integer(feb_option(options, options_names[index]), options_names[index],
                             lows[index], highs[index], &values[index], error))
                return false;
        FusionJson fields = fusion_json_object();
        bool ok = feb_put_int(&fields, "x", values[0]) && feb_put_int(&fields, "y", values[1]) &&
                  feb_put_int(&fields, "width", values[2]) &&
                  feb_put_int(&fields, "height", values[3]) &&
                  copy_option(&fields, "label", feb_option(options, "label")) &&
                  copy_option(&fields, "door_type", feb_option(options, "door_type")) &&
                  copy_option(&fields, "facing", feb_option(options, "facing"));
        if (!ok) {
            fusion_json_free(&fields);
            return feb_oom(error);
        }
        static const char *const editable[] = {
            "x", "y", "width", "height", "label", "door_type", "facing", NULL};
        if (!strcmp(command, "door-create")) {
            ok = feb_room_append(document, "doors", "next_door_id", FEB_MAX_DOORS,
                                 "project door limit reached", fields, &changed, error);
        } else {
            ok = feb_room_update(document, "doors", id, &fields, editable,
                                 "door update requires only editable door fields",
                                 "unknown project door id", &changed, error);
            fusion_json_free(&fields);
        }
        if (!ok) return false;
    }
    if (!feb_put(data, "door", changed) || !feb_put_bool(data, "persisted", false) ||
        !feb_put_string(data, "engine_adapter", "unavailable"))
        return feb_oom(error);
    return persisted(root, document, data, dry_run, true, error);
}

static bool command_transition(const char *command, const char *root, const char *repository,
                               const FusionJson *options, FusionJson *document, bool dry_run,
                               FusionJson *data, FebError *error) {
    const FusionJson *transitions = fusion_json_member(document, "transitions");
    if (!strcmp(command, "transition-list")) {
        FusionJson copy;
        return (fusion_json_clone(transitions, &copy) &&
                feb_put_int(data, "count", (int64_t)transitions->count) &&
                feb_put(data, "transitions", copy) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    if (!strcmp(command, "transition-validate")) {
        FusionJson records;
        if (!transition_validation(root, repository, document, &records, error)) return false;
        bool valid = true;
        for (size_t index = 0; index < records.count; ++index)
            if (!fusion_json_member(&records.items[index], "target_exists")->boolean ||
                !strcmp(string(&records.items[index], "target_door_status"),
                        "missing_target_project_door"))
                valid = false;
        int64_t count = (int64_t)records.count;
        return (feb_put_string(data, "status",
                               valid ? "valid_project_data_not_playable" : "invalid_target") &&
                feb_put_int(data, "count", count) && feb_put(data, "transitions", records) &&
                feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    }
    bool create = !strcmp(command, "transition-create") || !strcmp(command, "door-link");
    int64_t id = 0;
    if (!create && !feb_integer(feb_option(options, "id"), "id", 1, 999999, &id, error))
        return false;
    FusionJson changed;
    if (!strcmp(command, "transition-delete")) {
        if (!confirmed(options))
            return feb_fail(error, FEB_EXIT_OPERATION, "transition-delete requires --confirm=true");
        if (!has_item(document, "transitions", id))
            return feb_fail(error, FEB_EXIT_OPERATION, "unknown project transition id");
        char ref[64];
        snprintf(ref, sizeof ref, "transition:%lld", (long long)id);
        const FusionJson *events = fusion_json_member(document, "events");
        for (size_t index = 0; index < events->count; ++index)
            if (!strcmp(string(&events->items[index], "action_type"), "transition") &&
                !strcmp(string(&events->items[index], "action_ref"), ref))
                return feb_fail(error, FEB_EXIT_OPERATION,
                                "transition is referenced by a project event");
        for (size_t index = 0; index < events->count; ++index) {
            const FusionJson *conditions = fusion_json_member(&events->items[index], "conditions");
            for (size_t entry = 0; entry < conditions->count; ++entry)
                if (!strcmp(string(&conditions->items[entry], "type"), "transition_ready") &&
                    !strcmp(string(&conditions->items[entry], "ref"), ref))
                    return feb_fail(error, FEB_EXIT_OPERATION,
                                    "transition is referenced by a project event condition");
        }
        if (!feb_room_remove(document, "transitions", id, "unknown project transition id", error) ||
            !deleted(id, &changed))
            return false;
    } else {
        int64_t source = 0;
        Target target;
        FusionJson fields;
        if (!feb_integer(feb_option(options, "source_door_id"), "source_door_id", 1, 999999,
                         &source, error) ||
            !options_target(root, repository, options, &target, error))
            return false;
        FusionJson rest;
        if (!target_json(&target, &rest)) {
            fusion_json_free(&rest);
            return feb_oom(error);
        }
        fields = fusion_json_object();
        bool ok = feb_put_int(&fields, "source_door_id", source);
        for (size_t index = 0; ok && index < rest.count; ++index) {
            ok = fusion_json_set(&fields, rest.keys[index], rest.items[index]);
            rest.items[index] = fusion_json_null();
        }
        fusion_json_free(&rest);
        if (!ok) {
            fusion_json_free(&fields);
            return feb_oom(error);
        }
        static const char *const editable[] = {
            "source_door_id", "target_world", "target_area", "target_room", "target_door_id",
            "spawn_x", "spawn_y", NULL};
        if (create) {
            ok = feb_room_append(document, "transitions", "next_transition_id",
                                 FEB_MAX_TRANSITIONS, "project transition limit reached", fields,
                                 &changed, error);
        } else {
            ok = feb_room_update(document, "transitions", id, &fields, editable,
                                 "transition update requires only editable transition fields",
                                 "unknown project transition id", &changed, error);
            fusion_json_free(&fields);
        }
        if (!ok) return false;
    }
    if (!feb_put(data, "transition", changed) || !feb_put_bool(data, "persisted", false) ||
        !feb_put_string(data, "engine_adapter", "unavailable"))
        return feb_oom(error);
    return persisted(root, document, data, dry_run, true, error);
}

static const FusionJson *given(const FusionJson *options, const char *key) {
    const FusionJson *value = feb_option(options, key);
    return value && value->kind != FUSION_JSON_NULL ? value : NULL;
}

/* _entity_settings(): NULL settings when no item field is given. */
static bool entity_settings(const FusionJson *options, const FusionJson *native_type,
                            FusionJson *settings, bool *present, FebError *error) {
    static const char *const keys[] = {"item_id", "parameter_0", "parameter_1", "flags"};
    static const int64_t limits[] = {65535, 65535, 65535, 255};
    size_t count = 0;
    for (size_t index = 0; index < COUNT(keys); ++index)
        if (given(options, keys[index])) ++count;
    *present = count != 0;
    if (!count) return true;
    if (count != COUNT(keys))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "item settings require item_id, parameter_0, parameter_1 and flags");
    int64_t values[4];
    for (size_t index = 0; index < COUNT(keys); ++index)
        if (!feb_integer(given(options, keys[index]), keys[index], 0, limits[index], &values[index],
                         error))
            return false;
    return feb_item_settings(native_type, values[0], values[1], values[2], values[3], settings,
                             error);
}

static const FusionJson *entity_by_id(const FusionJson *document, int64_t id) {
    const FusionJson *list = fusion_json_member(document, "entities");
    for (size_t index = 0; index < list->count; ++index)
        if (integer(&list->items[index], "id") == id) return &list->items[index];
    return NULL;
}

/* entity-create, entity-update and entity-assign. */
static bool command_entity_catalog(const char *command, const char *root, const char *repository,
                                   const FusionJson *options, FusionJson *document, bool dry_run,
                                   FusionJson *data, FebError *error) {
    bool create = !strcmp(command, "entity-create");
    int64_t id = 0, x = 0, y = 0;
    if (!create && !feb_integer(feb_option(options, "id"), "id", 1, 999999, &id, error))
        return false;
    FusionJson unassigned = fusion_json_string("unassigned");
    const FusionJson *native_type = create ? feb_option(options, "native_type")
                                           : given(options, "native_type");
    if (create && !native_type) native_type = &unassigned;
    FusionJson settings = fusion_json_null(), changed = fusion_json_null();
    bool present = false, ok = entity_settings(options, native_type, &settings, &present, error);
    if (ok && strcmp(command, "entity-assign"))
        ok = feb_integer(feb_option(options, "x"), "x", 0, 16383, &x, error) &&
             feb_integer(feb_option(options, "y"), "y", 0, 16383, &y, error);
    if (ok && create) {
        ok = feb_entity_create(document, repository, given(options, "kind"), x, y,
                               given(options, "label"), native_type, present ? &settings : NULL,
                               &changed, error);
    } else if (ok) {
        ok = !strcmp(command, "entity-update")
            ? feb_entity_update(document, repository, id, x, y, given(options, "label"),
                                native_type, present ? &settings : NULL, error)
            : feb_entity_assign(document, repository, id, native_type, present ? &settings : NULL,
                                error);
        ok = ok && (fusion_json_clone(entity_by_id(document, id), &changed) || feb_oom(error));
    }
    fusion_json_free(&settings);
    fusion_json_free(&unassigned);
    if (!ok) return false;
    if (!feb_put(data, "entity", changed) || !feb_put_bool(data, "persisted", false))
        return feb_oom(error);
    return persisted(root, document, data, dry_run, true, error);
}

static bool command_entity(const char *command, const char *root, const FusionJson *options,
                           FusionJson *document, const FebRoomScope *scope, bool dry_run,
                           FusionJson *data, FebError *error) {
    const FusionJson *entities = fusion_json_member(document, "entities");
    if (!strcmp(command, "entity-list")) {
        const FusionJson *preview = feb_option(options, "preview");
        FusionJson copy;
        return (fusion_json_clone(entities, &copy) &&
                feb_put_string(data, "world", public_world(scope->world)) &&
                feb_put_string(data, "area", scope->area) && feb_put_int(data, "room", scope->room) &&
                feb_put_bool(data, "preview",
                             preview && preview->kind == FUSION_JSON_BOOL && preview->boolean) &&
                feb_put_int(data, "count", (int64_t)entities->count) &&
                feb_put(data, "entities", copy)) || feb_oom(error);
    }
    int64_t id = 0;
    if (!feb_integer(feb_option(options, "id"), "id", 1, 999999, &id, error)) return false;
    const FusionJson *current = NULL;
    for (size_t index = 0; index < entities->count; ++index)
        if (integer(&entities->items[index], "id") == id) current = &entities->items[index];
    FusionJson changed;
    if (!strcmp(command, "entity-inspect") || !strcmp(command, "entity-item-settings")) {
        if (!current) return feb_fail(error, FEB_EXIT_OPERATION, "unknown project entity id");
        if (!strcmp(command, "entity-item-settings") &&
            (strcmp(scope->world, "aria") || strcmp(string(current, "kind"), "ITEM")))
            return feb_fail(error, FEB_EXIT_OPERATION, "entity is not an Aria project item");
        const FusionJson *settings = fusion_json_member(current, "settings");
        FusionJson settings_copy = fusion_json_object();
        if (settings) {
            fusion_json_free(&settings_copy);
            if (!fusion_json_clone(settings, &settings_copy)) return feb_oom(error);
        }
        return (fusion_json_clone(current, &changed) && feb_put(data, "entity", changed) &&
                feb_put(data, "settings", settings_copy)) || feb_oom(error);
    }
    if (!strcmp(command, "entity-move")) {
        int64_t x = 0, y = 0;
        if (!feb_integer(feb_option(options, "x"), "x", 0, 16383, &x, error) ||
            !feb_integer(feb_option(options, "y"), "y", 0, 16383, &y, error) ||
            !feb_entity_move(document, id, x, y, error))
            return false;
        const FusionJson *moved = NULL;
        const FusionJson *list = fusion_json_member(document, "entities");
        for (size_t index = 0; index < list->count; ++index)
            if (integer(&list->items[index], "id") == id) moved = &list->items[index];
        if (!fusion_json_clone(moved, &changed)) return feb_oom(error);
    } else {
        if (!confirmed(options))
            return feb_fail(error, FEB_EXIT_OPERATION, "entity-delete requires --confirm=true");
        if (!feb_entity_delete(document, id, error) || !deleted(id, &changed)) return false;
    }
    if (!feb_put(data, "entity", changed) || !feb_put_bool(data, "persisted", false))
        return feb_oom(error);
    return persisted(root, document, data, dry_run, true, error);
}

static bool saved_room_0115(const char *root, const Target *target, FusionJson *document,
                            FebError *error) {
    static const char *const messages[4] = {
        "save both project doors before preparing the return link", "invalid saved room document",
        "invalid saved room document", "invalid saved room geometry"};
    return feb_room_saved(root, target->world, target->area, target->room, true, messages,
                          document, error);
}

/* _reciprocal_plan_0115(): a read-only plan of the return transition. */
static bool return_plan(const char *root, const char *repository, const FusionJson *options,
                        FusionJson *data, FebError *error) {
    Target original, target;
    if (!transition_target(root, repository, feb_option(options, "world"),
                           feb_option(options, "area"), feb_option(options, "room"),
                           feb_option(options, "source_door_id"), NULL, NULL, &original, error))
        return false;
    if (!original.door)
        return feb_fail(error, FEB_EXIT_OPERATION, "a saved source project door is required");
    FusionJson origin, destination;
    if (!saved_room_0115(root, &original, &origin, error)) return false;
    const FusionJson *source_door = NULL, *forward = NULL;
    const FusionJson *doors = fusion_json_member(&origin, "doors");
    for (size_t index = 0; index < doors->count; ++index)
        if (integer(&doors->items[index], "id") == original.door) source_door = &doors->items[index];
    bool ok = source_door || feb_fail(error, FEB_EXIT_OPERATION, "source project door is missing");
    size_t forwards = 0;
    const FusionJson *links = fusion_json_member(&origin, "transitions");
    for (size_t index = 0; ok && index < links->count; ++index)
        if (integer(&links->items[index], "source_door_id") == original.door) {
            forward = &links->items[index];
            ++forwards;
        }
    if (ok && forwards != 1)
        ok = feb_fail(error, FEB_EXIT_OPERATION,
                      "save the forward destination and its source room first");
    if (ok && !integer(forward, "target_door_id"))
        ok = feb_fail(error, FEB_EXIT_OPERATION,
                      "choose and save an exact destination project door first");
    if (ok) {
        FusionJson world = fusion_json_string(public_world(string(forward, "target_world")));
        FusionJson area, room = fusion_json_int(integer(forward, "target_room"));
        FusionJson door = fusion_json_int(integer(forward, "target_door_id"));
        ok = fusion_json_clone(fusion_json_member(forward, "target_area"), &area) &&
             transition_target(root, repository, &world, &area, &room, &door, NULL, NULL, &target,
                               error);
        fusion_json_free(&world);
        fusion_json_free(&area);
    }
    if (ok) ok = saved_room_0115(root, &target, &destination, error);
    if (ok) {
        const FusionJson *back = fusion_json_member(&destination, "transitions");
        for (size_t index = 0; ok && index < back->count; ++index)
            if (integer(&back->items[index], "source_door_id") == target.door)
                ok = feb_fail(error, FEB_EXIT_OPERATION,
                              "destination project door already has a transition; no overwrite");
        fusion_json_free(&destination);
    }
    if (ok)
        ok = (feb_put_string(data, "source_world", public_world(target.world)) &&
              feb_put_string(data, "source_area", target.area) &&
              feb_put_int(data, "source_room", target.room) &&
              feb_put_int(data, "source_door_id", target.door) &&
              feb_put_string(data, "target_world", public_world(original.world)) &&
              feb_put_string(data, "target_area", original.area) &&
              feb_put_int(data, "target_room", original.room) &&
              feb_put_int(data, "target_door_id", original.door) &&
              feb_put_int(data, "spawn_x", integer(source_door, "x")) &&
              feb_put_int(data, "spawn_y", integer(source_door, "y")) &&
              feb_put_string(data, "status", "prepared_only_not_persisted") &&
              feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
    fusion_json_free(&origin);
    return ok;
}

static bool connection_list(const char *root, const FusionJson *options, FusionJson *data,
                            FebError *error) {
    const char *world = NULL;
    if (!feb_world(feb_option(options, "world"), &world, error)) return false;
    const char *source_world = !strcmp(world, "aria") ? "aria" : "mzm";
    const FusionJson *area_option = feb_option(options, "area");
    char area[32];
    const char *source_area = NULL;
    if (area_option) {
        int64_t index = 0;
        if (!strcmp(source_world, "aria")) {
            if (!feb_integer(area_option, "area", 0, 11, &index, error)) return false;
            snprintf(area, sizeof area, "%lld", (long long)index);
        } else if (is_decimal(area_option)) {
            if (!feb_integer(area_option, "area", 0, 6, &index, error)) return false;
            snprintf(area, sizeof area, "%s", feb_mzm_areas[index]);
        } else if (!scope_area("mzm", area_option, "area", "unknown Zero Mission area", area,
                               error)) {
            return false;
        }
        source_area = area;
    }
    const FusionJson *room_option = feb_option(options, "room");
    int64_t room = 0;
    if (room_option && !feb_integer(room_option, "room", 0, 999, &room, error)) return false;
    FusionJson connections;
    if (!feb_room_connections(root, source_world, source_area, room_option ? &room : NULL,
                              &connections, error))
        return false;
    int64_t count = (int64_t)connections.count;
    return (feb_put(data, "connections", connections) && feb_put_int(data, "count", count) &&
            feb_put_string(data, "source", "saved_project_rooms_only") &&
            feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
}

static bool door_target_list(const char *root, const char *repository, const FusionJson *options,
                             FusionJson *data, FebError *error) {
    Target target;
    FusionJson target_value, doors;
    if (!options_target(root, repository, options, &target, error)) return false;
    if (!target_json(&target, &target_value)) {
        fusion_json_free(&target_value);
        return feb_oom(error);
    }
    if (!saved_target_doors(root, target.world, target.area, target.room, &doors, error)) {
        fusion_json_free(&target_value);
        return false;
    }
    return (feb_put(data, "target", target_value) && feb_put(data, "doors", doors) &&
            feb_put_string(data, "engine_adapter", "unavailable")) || feb_oom(error);
}

/* _input_toml(): a bounded, non-symlink TOML input. */
static bool input_toml(const FusionJson *value, FusionJson *parsed, char **text,
                       FebError *error) {
    if (!value || value->kind != FUSION_JSON_STRING || !value->string[0])
        return feb_fail(error, FEB_EXIT_OPERATION, "input TOML path is required");
    char absolute[FEB_PATH_CAPACITY], current[FEB_PATH_CAPACITY] = "", copy[FEB_PATH_CAPACITY];
    if (!feb_absolute(value->string, absolute))
        return feb_fail(error, FEB_EXIT_OPERATION, "input TOML path is too long");
    snprintf(copy, sizeof copy, "%s", absolute);
    for (char *part = strtok(copy, "/"); part; part = strtok(NULL, "/")) {
        if (!strcmp(part, ".")) continue;
        size_t used = strlen(current);
        if (used + strlen(part) + 2 > sizeof current) break;
        current[used] = '/';
        memcpy(current + used + 1, part, strlen(part) + 1);
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION, "symlink input TOML component refused: %s",
                            current);
    }
    long long size = 0;
    if (!feb_is_regular(absolute, &size) || size > 2000000)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "input TOML must be a bounded non-symlink file");
    return feb_load_toml(absolute, 2000000, parsed, text, error);
}

static bool story_target(const char *root, const char *kind, const FusionJson *value,
                         char *target, FebError *error) {
    if (!value || value->kind != FUSION_JSON_STRING || strchr(value->string, '\\'))
        return feb_fail(error, FEB_EXIT_OPERATION, "unsafe story target");
    char copy[FEB_PATH_CAPACITY];
    snprintf(copy, sizeof copy, "%s", value->string);
    for (char *part = strtok(copy, "/"); part; part = strtok(NULL, "/"))
        if (!strcmp(part, "..")) return feb_fail(error, FEB_EXIT_OPERATION, "unsafe story target");
    if (!strcmp(kind, "timeline") && strcmp(value->string, "data/story/timeline.toml"))
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "timeline target must be data/story/timeline.toml");
    if (!strcmp(kind, "cutscene")) {
        const char *name = value->string + strlen("data/cutscenes/");
        size_t length = strlen(value->string);
        bool valid = !strncmp(value->string, "data/cutscenes/", 15) && length > 20 &&
                     !strcmp(value->string + length - 5, ".toml") && name[0] >= 'a' &&
                     name[0] <= 'z' && (size_t)(value->string + length - 5 - name) <= 80;
        for (const char *c = name; valid && c < value->string + length - 5; ++c)
            if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_'))
                valid = false;
        if (!valid)
            return feb_fail(error, FEB_EXIT_OPERATION,
                            "cutscene target must be a safe data/cutscenes TOML path");
    }
    if (!feb_join(target, root, value->string))
        return feb_fail(error, FEB_EXIT_OPERATION, "unsafe story target");
    char current[FEB_PATH_CAPACITY];
    snprintf(current, sizeof current, "%s", root);
    snprintf(copy, sizeof copy, "%s", value->string);
    for (char *part = strtok(copy, "/"); part; part = strtok(NULL, "/")) {
        size_t used = strlen(current);
        if (used + strlen(part) + 2 > sizeof current) break;
        current[used] = '/';
        memcpy(current + used + 1, part, strlen(part) + 1);
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_OPERATION, "symlink story target refused: %s", current);
    }
    char parent[FEB_PATH_CAPACITY];
    snprintf(parent, sizeof parent, "%s", target);
    *strrchr(parent, '/') = '\0';
    if (!feb_is_directory(parent))
        return feb_fail(error, FEB_EXIT_OPERATION, "story target parent is missing");
    return true;
}

static bool atomic_text(const char *path, const char *text, FebError *error) {
    char parent[FEB_PATH_CAPACITY], temporary[2 * FEB_PATH_CAPACITY];
    snprintf(parent, sizeof parent, "%s", path);
    char *slash = strrchr(parent, '/');
    *slash = '\0';
    snprintf(temporary, sizeof temporary, "%s/.%s.XXXXXX.tmp", parent, slash + 1);
    int descriptor = mkstemps(temporary, 4);
    if (descriptor < 0) return feb_fail(error, FEB_EXIT_OPERATION, "cannot write %s", path);
    size_t size = strlen(text);
    bool ok = write(descriptor, text, size) == (ssize_t)size && fsync(descriptor) == 0;
    ok = close(descriptor) == 0 && ok;
    if (ok && feb_is_symlink(path)) {
        unlink(temporary);
        return feb_fail(error, FEB_EXIT_OPERATION, "story target became a symlink");
    }
    if (!ok || rename(temporary, path) != 0) {
        unlink(temporary);
        return feb_fail(error, FEB_EXIT_OPERATION, "cannot write %s", path);
    }
    return true;
}

static bool command_story(const char *command, const char *root, const FusionJson *options,
                          bool dry_run, FusionJson *data, FebError *error) {
    const FusionJson *kind = feb_option(options, "kind");
    FusionJson parsed;
    char *text = NULL;
    if (!input_toml(feb_option(options, "input"), &parsed, &text, error)) return false;
    int64_t count = 0;
    bool ok;
    if (kind && kind->kind == FUSION_JSON_STRING && !strcmp(kind->string, "timeline"))
        ok = feb_validate_timeline(&parsed, &count, error);
    else if (kind && kind->kind == FUSION_JSON_STRING && !strcmp(kind->string, "cutscene"))
        ok = feb_validate_scene(&parsed, &count, error);
    else
        ok = feb_fail(error, FEB_EXIT_OPERATION, "kind must be timeline or cutscene");
    fusion_json_free(&parsed);
    ok = ok && ((copy_option(data, "kind", kind) && feb_put_int(data, "record_count", count) &&
                 feb_put_bool(data, "persisted", false)) || feb_oom(error));
    if (ok && !strcmp(command, "story-save")) {
        char target[FEB_PATH_CAPACITY];
        ok = story_target(root, kind->string, feb_option(options, "target"), target, error);
        if (ok && !dry_run)
            ok = atomic_text(target, text, error) &&
                 ((feb_put_bool(data, "persisted", true) && feb_put_string(data, "path", target)) ||
                  feb_oom(error));
        else if (ok)
            ok = feb_put_string(data, "target", target) || feb_oom(error);
    }
    free(text);
    return ok;
}

static int compare_names(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

/* sorted(folder.glob(pattern)) for one directory and a suffix. */
static size_t list_files(const char *folder, const char *suffix, char **names, size_t capacity) {
    DIR *directory = opendir(folder);
    if (!directory) return 0;
    size_t count = 0, length = strlen(suffix);
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL && count < capacity) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        size_t name_length = strlen(entry->d_name);
        if (name_length < length || strcmp(entry->d_name + name_length - length, suffix)) continue;
        names[count] = strdup(entry->d_name);
        if (names[count]) ++count;
    }
    closedir(directory);
    qsort(names, count, sizeof *names, compare_names);
    return count;
}

/* _project_validation(). */
bool feb_project_validation(const char *root, FusionJson *data, FebError *error) {
    char path[FEB_PATH_CAPACITY];
    FusionJson timeline;
    int64_t count = 0;
    if (!feb_join(path, root, "data/story/timeline.toml") ||
        !feb_load_toml(path, 64 * 1024 * 1024, &timeline, NULL, error))
        return false;
    bool ok = feb_validate_timeline(&timeline, &count, error);
    fusion_json_free(&timeline);
    if (!ok) return false;
    FusionJson scenes = fusion_json_array();
    char folder[FEB_PATH_CAPACITY];
    char *names[4096];
    size_t total = feb_join(folder, root, "data/cutscenes") ? list_files(folder, ".toml", names, 4096) : 0;
    for (size_t index = 0; index < total; ++index) {
        char file[FEB_PATH_CAPACITY + 256];
        snprintf(file, sizeof file, "%s/%s", folder, names[index]);
        if (ok && feb_is_symlink(file))
            ok = feb_fail(error, FEB_EXIT_OPERATION, "symlink cutscene refused: %s", file);
        FusionJson scene = {0};
        int64_t steps = 0;
        if (ok) ok = feb_load_toml(file, 64 * 1024 * 1024, &scene, NULL, error) &&
                     feb_validate_scene(&scene, &steps, error);
        fusion_json_free(&scene);
        if (ok) {
            char relative[512];
            snprintf(relative, sizeof relative, "data/cutscenes/%s", names[index]);
            FusionJson record = fusion_json_object();
            ok = (feb_put_string(&record, "path", relative) && feb_put_int(&record, "step_count", steps) &&
                  fusion_json_push(&scenes, record)) || feb_oom(error);
        }
        free(names[index]);
    }
    FusionJson drafts = {0}, placements = {0}, rooms = {0}, documents = fusion_json_array();
    ok = ok && feb_list_drafts(root, NULL, &drafts, error) &&
         feb_load_placements(root, &placements, &rooms, error);
    FusionJson draft_ids = fusion_json_array();
    for (size_t index = 0; ok && index < drafts.count; ++index)
        ok = fusion_json_push(&draft_ids, fusion_json_string(fusion_json_member(&drafts.items[index], "id")->string)) ||
             feb_oom(error);
    char overrides[FEB_PATH_CAPACITY];
    if (ok && feb_join(overrides, root, "assets/extracted/overrides") && feb_is_symlink(overrides))
        ok = feb_fail(error, FEB_EXIT_OPERATION, "project entity root symlink refused");
    /* glob("*" "/entities/" "*.json"), sorted by path parts. */
    char *groups[256];
    size_t group_count = ok ? list_files(overrides, "", groups, 256) : 0;
    for (size_t group = 0; group < group_count; ++group) {
        char entities[FEB_PATH_CAPACITY + 256];
        snprintf(entities, sizeof entities, "%s/%s/entities", overrides, groups[group]);
        char *files[4096];
        size_t file_count = ok && feb_is_directory(entities) ? list_files(entities, ".json", files, 4096) : 0;
        for (size_t index = 0; index < file_count; ++index) {
            char file[FEB_PATH_CAPACITY + 512];
            snprintf(file, sizeof file, "%s/%s", entities, files[index]);
            long long size = 0;
            char relative[1024];
            snprintf(relative, sizeof relative, "assets/extracted/overrides/%s/entities/%s",
                     groups[group], files[index]);
            if (ok && (feb_is_symlink(file) || (feb_is_regular(file, &size) && size > ROOM_LIMIT_BYTES)))
                ok = feb_fail(error, FEB_EXIT_OPERATION, "unsafe project room document: %s", file);
            FusionJson document = {0};
            char detail[FEB_MESSAGE_CAPACITY - 64];
            if (ok && !feb_load_json(file, ROOM_LIMIT_BYTES, &document, detail, sizeof detail))
                ok = feb_fail(error, FEB_EXIT_OPERATION, "%s", detail);
            ok = ok && feb_room_migrate(&document, error) &&
                 feb_room_validate(&document, fusion_json_member(&document, "world"),
                                   fusion_json_member(&document, "area"),
                                   fusion_json_member(&document, "room"),
                                   fusion_json_member(&document, "width_px"),
                                   fusion_json_member(&document, "height_px"), error);
            FusionJson references = {0};
            ok = ok && event_validation(root, &document, &references, error);
            if (ok) {
                char invalid[FEB_MESSAGE_CAPACITY] = "";
                for (size_t record = 0; record < references.count; ++record) {
                    if (fusion_json_member(&references.items[record], "valid")->boolean) continue;
                    size_t used = strlen(invalid);
                    snprintf(invalid + used, sizeof invalid - used, "%s%lld", used ? ", " : "",
                             (long long)integer(&references.items[record], "id"));
                }
                if (invalid[0])
                    ok = feb_fail(error, FEB_EXIT_OPERATION,
                                  "%s: invalid event action reference(s): %s", relative, invalid);
            }
            if (ok) {
                FusionJson record = fusion_json_object();
                ok = (feb_put_string(&record, "path", relative) &&
                      feb_put_int(&record, "entity_count",
                                  (int64_t)fusion_json_member(&document, "entities")->count) &&
                      feb_put_int(&record, "collision_cell_count",
                                  (int64_t)fusion_json_member(fusion_json_member(&document, "collision"),
                                                              "cells")->count) &&
                      feb_put_int(&record, "door_count",
                                  (int64_t)fusion_json_member(&document, "doors")->count) &&
                      feb_put_int(&record, "transition_count",
                                  (int64_t)fusion_json_member(&document, "transitions")->count) &&
                      feb_put_int(&record, "event_count",
                                  (int64_t)fusion_json_member(&document, "events")->count) &&
                      feb_put_int(&record, "validated_event_reference_count",
                                  (int64_t)references.count) &&
                      fusion_json_push(&documents, record)) || feb_oom(error);
            }
            fusion_json_free(&references);
            fusion_json_free(&document);
            free(files[index]);
        }
        free(groups[group]);
    }
    FusionJson adapters = fusion_json_object(), copy;
    ok = ok && ((feb_put_int(data, "timeline_events", count) && feb_put(data, "cutscenes", scenes) &&
                 feb_put(data, "draft_rooms", draft_ids) && feb_put(data, "draft_placements", placements) &&
                 fusion_json_clone(&documents, &copy) &&
                 feb_put(data, "project_room_documents", copy) &&
                 feb_put(data, "project_entity_documents", documents) &&
                 feb_put_string(&adapters, "zero_mission", "unavailable") &&
                 feb_put_string(&adapters, "aria", "unavailable") &&
                 feb_put(data, "engine_adapters", adapters)) || feb_oom(error));
    if (!ok) {
        fusion_json_free(&scenes);
        fusion_json_free(&draft_ids);
        fusion_json_free(&placements);
        fusion_json_free(&documents);
        fusion_json_free(&adapters);
    }
    fusion_json_free(&drafts);
    fusion_json_free(&rooms);
    return ok;
}

/* project_room_package.read_saved(): only saved data, never the editor's
 * unsaved staging file. */
static bool read_saved(const char *root, const FebRoomScope *scope, FusionJson *document,
                       FebError *error) {
    char path[FEB_PATH_CAPACITY];
    FebRoomScope fixed = *scope;
    fixed.width = fixed.height = 16;
    if (!feb_room_path(root, &fixed, path, error) || !feb_room_check_path(root, path, false, error))
        return false;
    long long size = 0;
    if (!feb_is_regular(path, &size) || size > ROOM_LIMIT_BYTES)
        return feb_fail(error, FEB_EXIT_OPERATION,
                        "room-export requires a saved project room document");
    char detail[256];
    if (!feb_load_json(path, ROOM_LIMIT_BYTES, document, detail, sizeof detail))
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid saved project room document");
    if (document->kind != FUSION_JSON_OBJECT) {
        fusion_json_free(document);
        return feb_fail(error, FEB_EXIT_OPERATION, "invalid saved project room document");
    }
    FusionJson world = fusion_json_string(scope->world), area = fusion_json_string(scope->area),
               room = fusion_json_int(scope->room);
    bool ok = feb_room_migrate(document, error) &&
              feb_room_validate(document, &world, &area, &room,
                                fusion_json_member(document, "width_px"),
                                fusion_json_member(document, "height_px"), error);
    fusion_json_free(&world);
    fusion_json_free(&area);
    if (!ok) fusion_json_free(document);
    return ok;
}

/* A copy with every object's keys sorted (json.dumps(sort_keys=True)). */
static bool sorted_copy(const FusionJson *value, FusionJson *copy) {
    if (value->kind == FUSION_JSON_ARRAY) {
        *copy = fusion_json_array();
        for (size_t index = 0; index < value->count; ++index) {
            FusionJson item;
            if (!sorted_copy(&value->items[index], &item) || !fusion_json_push(copy, item)) {
                fusion_json_free(copy);
                return false;
            }
        }
        return true;
    }
    if (value->kind != FUSION_JSON_OBJECT) return fusion_json_clone(value, copy);
    size_t *order = malloc((value->count ? value->count : 1) * sizeof *order);
    if (!order) return false;
    for (size_t index = 0; index < value->count; ++index) order[index] = index;
    for (size_t index = 1; index < value->count; ++index)
        for (size_t at = index; at > 0 && strcmp(value->keys[order[at - 1]],
                                                 value->keys[order[at]]) > 0; --at) {
            size_t swap = order[at];
            order[at] = order[at - 1];
            order[at - 1] = swap;
        }
    *copy = fusion_json_object();
    bool ok = true;
    for (size_t index = 0; ok && index < value->count; ++index) {
        FusionJson item;
        ok = sorted_copy(&value->items[order[index]], &item) &&
             fusion_json_set(copy, value->keys[order[index]], item);
    }
    free(order);
    if (!ok) fusion_json_free(copy);
    return ok;
}

/* The items of a list sorted by one or two integer keys (stable). */
static const FusionJson **sorted_items(const FusionJson *list, const char *first,
                                       const char *second) {
    const FusionJson **items = malloc((list->count ? list->count : 1) * sizeof *items);
    if (!items) return NULL;
    for (size_t index = 0; index < list->count; ++index) items[index] = &list->items[index];
    for (size_t index = 1; index < list->count; ++index)
        for (size_t at = index; at > 0; --at) {
            int64_t a = integer(items[at - 1], first), b = integer(items[at], first);
            if (a == b && second) {
                a = integer(items[at - 1], second);
                b = integer(items[at], second);
            }
            if (a <= b) break;
            const FusionJson *swap = items[at];
            items[at] = items[at - 1];
            items[at - 1] = swap;
        }
    return items;
}

static bool append_text(char **text, size_t *size, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    char line[512];
    int length = vsnprintf(line, sizeof line, format, arguments);
    va_end(arguments);
    if (length < 0 || (size_t)length >= sizeof line) return false;
    char *grown = realloc(*text, *size + (size_t)length + 1);
    if (!grown) return false;
    memcpy(grown + *size, line, (size_t)length + 1);
    *text = grown;
    *size += (size_t)length;
    return true;
}

/* preview_bytes(): the bounded, non-gameplay preview of the package. */
static bool preview_text(const FusionJson *document, char **text, size_t *size) {
    static const char *const ids[][2] = {{"solid", "1"}, {"one_way", "2"}, {"hazard", "3"},
                                         {"slope_up", "4"}, {"slope_down", "5"}, {"water", "6"},
                                         {"air", "7"}};
    const FusionJson *collision = fusion_json_member(document, "collision");
    int64_t step = integer(collision, "resolution_px");
    *text = NULL;
    *size = 0;
    bool ok = append_text(text, size, "MVROOM-PREVIEW\t1\t%s\t%s\t%lld\t%lld\t%lld\t%lld\n",
                          string(document, "world"), string(document, "area"),
                          (long long)integer(document, "room"),
                          (long long)integer(document, "width_px"),
                          (long long)integer(document, "height_px"), (long long)step);
    const FusionJson *cells = fusion_json_member(collision, "cells");
    const FusionJson **items = sorted_items(cells, "y", "x");
    ok = ok && items;
    for (size_t index = 0; ok && index < cells->count; ++index) {
        const char *type = string(items[index], "type"), *id = "0";
        for (size_t kind = 0; kind < COUNT(ids); ++kind)
            if (!strcmp(ids[kind][0], type)) id = ids[kind][1];
        ok = append_text(text, size, "C\t%lld\t%lld\t%lld\t%lld\t%s\n",
                         (long long)(integer(items[index], "x") * step),
                         (long long)(integer(items[index], "y") * step), (long long)step,
                         (long long)step, id);
    }
    free(items);
    static const char *const lists[][2] = {{"doors", "D"}, {"entities", "E"}, {"events", "V"}};
    for (size_t list = 0; ok && list < COUNT(lists); ++list) {
        const FusionJson *entries = fusion_json_member(document, lists[list][0]);
        items = sorted_items(entries, "id", NULL);
        ok = items != NULL;
        for (size_t index = 0; ok && index < entries->count; ++index) {
            bool entity = list == 1;
            ok = append_text(text, size, "%s\t%lld\t%lld\t%lld\t%lld\t0\n", lists[list][1],
                             (long long)integer(items[index], "x"),
                             (long long)integer(items[index], "y"),
                             (long long)(entity ? 16 : integer(items[index], "width")),
                             (long long)(entity ? 16 : integer(items[index], "height")));
        }
        free(items);
    }
    ok = ok && append_text(text, size, "END\n");
    if (!ok) {
        free(*text);
        *text = NULL;
    }
    return ok;
}

static bool same_file(const char *path, const char *data, size_t size) {
    char *current = NULL;
    size_t current_size = 0;
    bool same = !feb_is_symlink(path) && feb_read_text(path, size + 1, &current, &current_size) &&
                current_size == size && !memcmp(current, data, size);
    free(current);
    return same;
}

static bool write_new(const char *path, const char *data, size_t size) {
    int descriptor = open(path, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (descriptor < 0) return false;
    bool ok = write(descriptor, data, size) == (ssize_t)size && fsync(descriptor) == 0;
    return close(descriptor) == 0 && ok;
}

static void remove_tree(const char *path) {
    char file[FEB_PATH_CAPACITY + 32];
    static const char *const names[] = {"room.json", "preview.tsv"};
    for (size_t index = 0; index < COUNT(names); ++index) {
        snprintf(file, sizeof file, "%s/%s", path, names[index]);
        unlink(file);
    }
    rmdir(path);
}

/* room-export: the immutable, content-addressed project room package. */
static bool room_export(const char *root, const char *repository, const FusionJson *options,
                        bool dry_run, FusionJson *data, FebError *error) {
    FusionJson scoped;
    if (!fusion_json_clone(options, &scoped)) return feb_oom(error);
    FebRoomScope scope;
    bool ok = (feb_put_int(&scoped, "width", 16) && feb_put_int(&scoped, "height", 16)) ||
              feb_oom(error);
    ok = ok && entity_scope(&scoped, &scope, error);
    fusion_json_free(&scoped);
    FusionJson document;
    if (!ok || !read_saved(root, &scope, &document, error)) return false;
    FusionJson records;
    ok = event_validation(root, &document, &records, error);
    if (ok) {
        char invalid[FEB_MESSAGE_CAPACITY] = "[";
        size_t used = 1, count = 0;
        for (size_t index = 0; index < records.count; ++index) {
            if (fusion_json_member(&records.items[index], "valid")->boolean) continue;
            used += (size_t)snprintf(invalid + used, sizeof invalid - used, "%s%lld",
                                     count++ ? ", " : "",
                                     (long long)integer(&records.items[index], "id"));
        }
        snprintf(invalid + used, sizeof invalid - used, "]");
        fusion_json_free(&records);
        if (count)
            ok = feb_fail(error, FEB_EXIT_OPERATION,
                          "room-export unresolved event references: %s", invalid);
    }
    if (ok && fusion_json_member(&document, "transitions")->count) {
        ok = transition_validation(root, repository, &document, &records, error);
        for (size_t index = 0; ok && index < records.count; ++index)
            if (!fusion_json_member(&records.items[index], "target_exists")->boolean ||
                !strcmp(string(&records.items[index], "target_door_status"),
                        "missing_target_project_door"))
                ok = feb_fail(error, FEB_EXIT_OPERATION,
                              "room-export has invalid transition destinations");
        if (records.kind == FUSION_JSON_ARRAY) fusion_json_free(&records);
    }
    FusionJson world = fusion_json_null(), area = fusion_json_null(), room = fusion_json_null();
    ok = ok && fusion_json_clone(fusion_json_member(&document, "world"), &world) &&
         fusion_json_clone(fusion_json_member(&document, "area"), &area) &&
         fusion_json_clone(fusion_json_member(&document, "room"), &room) &&
         feb_room_validate(&document, &world, &area, &room,
                           fusion_json_member(&document, "width_px"),
                           fusion_json_member(&document, "height_px"), error);
    fusion_json_free(&world);
    fusion_json_free(&area);
    fusion_json_free(&room);
    char *full = NULL, *preview = NULL;
    size_t full_size = 0, preview_size = 0;
    if (ok) {
        FusionJson payload = fusion_json_object(), copy, sorted;
        ok = (fusion_json_clone(&document, &copy) &&
              feb_put_string(&payload, "schema", "metroidvania.project-room-package") &&
              feb_put_int(&payload, "version", 1) &&
              feb_put_string(&payload, "engine_adapter", "unavailable") &&
              feb_put_bool(&payload, "native_assets_included", false) &&
              feb_put(&payload, "room", copy) && sorted_copy(&payload, &sorted)) ||
             feb_oom(error);
        fusion_json_free(&payload);
        if (ok) {
            ok = fusion_json_write(&sorted, FUSION_JSON_COMPACT, &full, &full_size) ||
                 feb_oom(error);
            fusion_json_free(&sorted);
        }
        char *line = ok ? realloc(full, full_size + 2) : NULL;
        if (ok && !line) ok = feb_oom(error);
        if (ok) {
            full = line;
            full[full_size++] = '\n';
            full[full_size] = '\0';
        }
        ok = ok && (preview_text(&document, &preview, &preview_size) || feb_oom(error));
    }
    char digest[65] = "";
    if (ok) {
        uint8_t *joined = malloc(full_size + 1 + preview_size);
        if (!joined) ok = feb_oom(error);
        if (ok) {
            memcpy(joined, full, full_size);
            joined[full_size] = 0;
            memcpy(joined + full_size + 1, preview, preview_size);
            ok = fusion_sha256_hex(joined, full_size + 1 + preview_size, digest) || feb_oom(error);
        }
        free(joined);
    }
    const char *world_name = string(&document, "world"), *area_name = string(&document, "area");
    int64_t room_number = integer(&document, "room");
    char parent[FEB_PATH_CAPACITY], destination[FEB_PATH_CAPACITY + 160], lower[32];
    snprintf(lower, sizeof lower, "%s", ok ? area_name : "");
    for (char *c = lower; *c; ++c)
        if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
    snprintf(parent, sizeof parent, "%s/assets/extracted/exports/%s", root, ok ? world_name : "");
    snprintf(destination, sizeof destination, "%s/%s_%03lld_%s", parent, lower,
             (long long)room_number, digest);
    if (ok) {
        char identity[96];
        snprintf(identity, sizeof identity, "%s:%s:%lld", world_name, area_name,
                 (long long)room_number);
        FusionJson counts = fusion_json_object();
        ok = (feb_put_string(data, "format", "metroidvania.project-room-package") &&
              feb_put_int(data, "version", 1) && feb_put_string(data, "room", identity) &&
              feb_put_string(data, "sha256", digest) &&
              feb_put_string(data, "package_dir", destination) &&
              feb_put_bool(data, "native_assets_included", false) &&
              feb_put_string(data, "engine_adapter", "unavailable") &&
              feb_put_int(&counts, "collision",
                          (int64_t)fusion_json_member(fusion_json_member(&document, "collision"),
                                                      "cells")->count) &&
              feb_put_int(&counts, "doors", (int64_t)fusion_json_member(&document, "doors")->count) &&
              feb_put_int(&counts, "entities",
                          (int64_t)fusion_json_member(&document, "entities")->count) &&
              feb_put_int(&counts, "events", (int64_t)fusion_json_member(&document, "events")->count) &&
              feb_put_int(&counts, "transitions",
                          (int64_t)fusion_json_member(&document, "transitions")->count) &&
              feb_put(data, "counts", counts) && feb_put_bool(data, "persisted", !dry_run)) ||
             feb_oom(error);
    }
    if (ok && !dry_run) {
        /* Refuse symlink components before making the ignored export folders. */
        char current[FEB_PATH_CAPACITY];
        static const char *const parts[] = {"assets", "extracted", "exports"};
        snprintf(current, sizeof current, "%s", root);
        for (size_t part = 0; ok && part < 4; ++part) {
            size_t used = strlen(current);
            snprintf(current + used, sizeof current - used, "/%s",
                     part < 3 ? parts[part] : world_name);
            if (feb_is_symlink(current))
                ok = feb_fail(error, FEB_EXIT_OPERATION, "room-export parent symlink refused");
            else
                mkdir(current, 0777);
        }
        char room_file[FEB_PATH_CAPACITY + 192], preview_file[FEB_PATH_CAPACITY + 192];
        snprintf(room_file, sizeof room_file, "%s/room.json", destination);
        snprintf(preview_file, sizeof preview_file, "%s/preview.tsv", destination);
        struct stat status;
        if (ok && (lstat(destination, &status) == 0)) {
            if (!S_ISDIR(status.st_mode) || !same_file(room_file, full, full_size) ||
                !same_file(preview_file, preview, preview_size))
                ok = feb_fail(error, FEB_EXIT_OPERATION,
                              "room-export content-addressed package collision");
        } else if (ok) {
            char temporary[FEB_PATH_CAPACITY + 32];
            snprintf(temporary, sizeof temporary, "%s/.room-package-XXXXXX", parent);
            if (!mkdtemp(temporary)) {
                ok = feb_fail(error, FEB_EXIT_OPERATION, "cannot create the room package");
            } else {
                char temporary_room[FEB_PATH_CAPACITY + 64], temporary_preview[FEB_PATH_CAPACITY + 64];
                snprintf(temporary_room, sizeof temporary_room, "%s/room.json", temporary);
                snprintf(temporary_preview, sizeof temporary_preview, "%s/preview.tsv", temporary);
                ok = (write_new(temporary_room, full, full_size) &&
                      write_new(temporary_preview, preview, preview_size)) ||
                     feb_fail(error, FEB_EXIT_OPERATION, "cannot write the room package");
                if (ok && rename(temporary, destination) != 0 &&
                    (!same_file(room_file, full, full_size) ||
                     !same_file(preview_file, preview, preview_size)))
                    ok = feb_fail(error, FEB_EXIT_OPERATION,
                                  "room-export content-addressed package collision");
                remove_tree(temporary);
            }
        }
        FusionJson created = fusion_json_array();
        ok = ok && ((fusion_json_push(&created, fusion_json_string(room_file)) &&
                     fusion_json_push(&created, fusion_json_string(preview_file)) &&
                     feb_put(data, "created_paths", created)) || feb_oom(error));
    }
    free(full);
    free(preview);
    fusion_json_free(&document);
    return ok;
}

bool feb_room_command(const char *command, const FusionJson *options, const char *root,
                      const char *repository, bool dry_run, FusionJson *data, FebError *error) {
    if (!strcmp(command, "collision-capabilities")) {
        const char *world = NULL;
        FusionJson result;
        if (!feb_world(feb_option(options, "world"), &world, error)) return false;
        if (!feb_collision_capabilities(!strcmp(world, "zero_mission") ? "mzm" : "aria", &result))
            return feb_oom(error);
        if (!feb_put_string(&result, "world", world)) {
            fusion_json_free(&result);
            return feb_oom(error);
        }
        fusion_json_free(data);
        *data = result;
        return true;
    }
    if (!strcmp(command, "door-return-plan")) return return_plan(root, repository, options, data, error);
    if (!strcmp(command, "connection-list")) return connection_list(root, options, data, error);
    if (!strcmp(command, "door-target-list"))
        return door_target_list(root, repository, options, data, error);
    if (!strcmp(command, "story-validate") || !strcmp(command, "story-save"))
        return command_story(command, root, options, dry_run, data, error);
    if (!strcmp(command, "project-validate")) return feb_project_validation(root, data, error);
    if (!strcmp(command, "room-export")) return room_export(root, repository, options, dry_run, data, error);
    FebRoomScope scope;
    FusionJson document;
    if (!entity_scope(options, &scope, error) || !feb_room_load(root, &scope, &document, error))
        return false;
    bool ok;
    if (!strncmp(command, "collision-", 10))
        ok = command_collision(command, root, options, &document, dry_run, data, error);
    else if (!strncmp(command, "event-", 6))
        ok = command_event(command, root, options, &document, dry_run, data, error);
    else if (!strncmp(command, "door-", 5) && strcmp(command, "door-link"))
        ok = command_door(command, root, options, &document, &scope, dry_run, data, error);
    else if (!strncmp(command, "transition-", 11) || !strcmp(command, "door-link"))
        ok = command_transition(command, root, repository, options, &document, dry_run, data,
                                error);
    else if (!strcmp(command, "entity-create") || !strcmp(command, "entity-update") ||
             !strcmp(command, "entity-assign"))
        ok = command_entity_catalog(command, root, repository, options, &document, dry_run, data,
                                    error);
    else
        ok = command_entity(command, root, options, &document, &scope, dry_run, data, error);
    fusion_json_free(&document);
    return ok;
}
