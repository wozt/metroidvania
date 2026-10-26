/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor command-line interface (formerly scripts/editor_cli.py): the
 * argparse-compatible options, the one-command flow and its text, JSON and
 * TSV results. */
#define _DEFAULT_SOURCE
#include "backend.h"

#include "common.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum { OPTION_STRING, OPTION_BOOL, OPTION_CHOICE, OPTION_FLAG } OptionKind;

typedef struct {
    const char *flag;
    const char *dest;
    OptionKind kind;
} Option;

/* The parser's options in declaration order (argparse lists ambiguous
 * matches in this order). */
static const Option options_table[] = {
    {"--help", "help", OPTION_FLAG},
    {"--version", "version", OPTION_FLAG},
    {"--command", "command", OPTION_STRING},
    {"--batch", "batch", OPTION_STRING},
    {"--atomic", "atomic", OPTION_BOOL},
    {"--format", "format", OPTION_CHOICE},
    {"--verbose", "verbose", OPTION_BOOL},
    {"--dry-run", "dry_run", OPTION_BOOL},
    {"--output", "output", OPTION_STRING},
    {"--root", "root", OPTION_STRING},
    {"--world", "world", OPTION_STRING},
    {"--area", "area", OPTION_STRING},
    {"--room", "room", OPTION_STRING},
    {"--source", "source", OPTION_STRING},
    {"--slug", "slug", OPTION_STRING},
    {"--name", "name", OPTION_STRING},
    {"--width-screens", "width_screens", OPTION_STRING},
    {"--height-screens", "height_screens", OPTION_STRING},
    {"--workers", "workers", OPTION_STRING},
    {"--report", "report", OPTION_STRING},
    {"--x", "x", OPTION_STRING},
    {"--y", "y", OPTION_STRING},
    {"--width", "width", OPTION_STRING},
    {"--height", "height", OPTION_STRING},
    {"--id", "id", OPTION_STRING},
    {"--kind", "kind", OPTION_STRING},
    {"--label", "label", OPTION_STRING},
    {"--native-type", "native_type", OPTION_STRING},
    {"--item-id", "item_id", OPTION_STRING},
    {"--parameter-0", "parameter_0", OPTION_STRING},
    {"--parameter-1", "parameter_1", OPTION_STRING},
    {"--flags", "flags", OPTION_STRING},
    {"--preview", "preview", OPTION_BOOL},
    {"--confirm", "confirm", OPTION_BOOL},
    {"--input", "input", OPTION_STRING},
    {"--target", "target", OPTION_STRING},
    {"--layer", "layer", OPTION_STRING},
    {"--tile-id", "tile_id", OPTION_STRING},
    {"--type", "collision_type", OPTION_STRING},
    {"--fill-width", "fill_width", OPTION_STRING},
    {"--fill-height", "fill_height", OPTION_STRING},
    {"--points", "points", OPTION_STRING},
    {"--event-kind", "event_kind", OPTION_STRING},
    {"--region-width", "region_width", OPTION_STRING},
    {"--region-height", "region_height", OPTION_STRING},
    {"--trigger-type", "trigger_type", OPTION_STRING},
    {"--action-type", "action_type", OPTION_STRING},
    {"--action-ref", "action_ref", OPTION_STRING},
    {"--once", "once", OPTION_BOOL},
    {"--condition-mode", "condition_mode", OPTION_STRING},
    {"--conditions", "conditions", OPTION_STRING},
    {"--door-width", "door_width", OPTION_STRING},
    {"--door-height", "door_height", OPTION_STRING},
    {"--door-type", "door_type", OPTION_STRING},
    {"--native-index", "native_index", OPTION_STRING},
    {"--native-variant", "native_variant", OPTION_STRING},
    {"--facing", "facing", OPTION_STRING},
    {"--source-door-id", "source_door_id", OPTION_STRING},
    {"--target-world", "target_world", OPTION_STRING},
    {"--target-area", "target_area", OPTION_STRING},
    {"--target-room", "target_room", OPTION_STRING},
    {"--target-door-id", "target_door_id", OPTION_STRING},
    {"--spawn-x", "spawn_x", OPTION_STRING},
    {"--spawn-y", "spawn_y", OPTION_STRING},
};

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

/* The options that never reach a command. */
static const char *const common_options[] = {
    "command", "batch", "atomic", "format", "verbose", "dry_run", "output", "root",
    "help", "version"};

/* The TSV_COMMANDS of the former CLI. */
static const char *const tsv_commands[] = {
    "room-list", "list-rooms", "placement-list", "entity-list", "entity-catalog",
    "entity-item-settings", "list-assets", "collision-list", "door-list",
    "door-target-list", "door-return-plan", "transition-list", "connection-list",
    "event-list", "event-validate"};

typedef struct {
    const char *values[COUNT(options_table)];
    bool present[COUNT(options_table)];
    bool help;
    bool version;
} Arguments;

static size_t option_index(const char *dest) {
    for (size_t index = 0; index < COUNT(options_table); ++index)
        if (!strcmp(options_table[index].dest, dest)) return index;
    return SIZE_MAX;
}

static bool is_negative_number(const char *text) {
    if (text[0] != '-' || !text[1]) return false;
    const char *c = text + 1;
    bool digits = false, point = false;
    for (; *c; ++c) {
        if (*c >= '0' && *c <= '9') digits = true;
        else if (*c == '.' && !point) point = true;
        else return false;
    }
    return digits;
}

typedef struct {
    size_t option;      /* SIZE_MAX: not an option */
    const char *value;  /* the explicit "=value", or NULL */
    bool unknown;       /* an unrecognized optional */
} Classified;

/* _parse_optional(): an option, its explicit value, or a positional. */
static bool classify(const char *argument, Classified *result, FebError *error) {
    *result = (Classified){SIZE_MAX, NULL, false};
    if (argument[0] != '-' || !argument[1]) return true;
    if (!strcmp(argument, "-h")) {
        result->option = option_index("help");
        return true;
    }
    char name[256];
    const char *equals = strchr(argument, '=');
    snprintf(name, sizeof name, "%.*s", equals ? (int)(equals - argument) : (int)strlen(argument),
             argument);
    for (size_t index = 0; index < COUNT(options_table); ++index) {
        if (!strcmp(options_table[index].flag, argument)) {
            result->option = index;
            return true;
        }
    }
    if (equals) {
        for (size_t index = 0; index < COUNT(options_table); ++index) {
            if (!strcmp(options_table[index].flag, name)) {
                result->option = index;
                result->value = equals + 1;
                return true;
            }
        }
    }
    if (!strncmp(argument, "--", 2)) {
        char matches[FEB_MESSAGE_CAPACITY] = "";
        size_t count = 0, match = 0;
        for (size_t index = 0; index < COUNT(options_table); ++index) {
            if (!strncmp(options_table[index].flag, name, strlen(name))) {
                size_t used = strlen(matches);
                snprintf(matches + used, sizeof matches - used, "%s%s", count ? ", " : "",
                         options_table[index].flag);
                match = index;
                ++count;
            }
        }
        if (count > 1)
            return feb_fail(error, FEB_EXIT_USAGE, "ambiguous option: %s could match %s",
                            argument, matches);
        if (count == 1) {
            result->option = match;
            result->value = equals ? equals + 1 : NULL;
            return true;
        }
    }
    if (is_negative_number(argument) || strchr(argument, ' ')) return true;
    result->unknown = true;
    return true;
}

static bool parse_arguments(int argc, char **argv, Arguments *arguments, FebError *error) {
    *arguments = (Arguments){0};
    Classified *classified = calloc((size_t)argc + 1, sizeof *classified);
    if (!classified) return feb_oom(error);
    for (int index = 1; index < argc; ++index) {
        if (!classify(argv[index], &classified[index], error)) {
            free(classified);
            return false;
        }
    }
    char extras[FEB_MESSAGE_CAPACITY] = "";
    bool ok = true;
    for (int index = 1; ok && index < argc; ++index) {
        Classified *item = &classified[index];
        if (item->option == SIZE_MAX) {
            size_t used = strlen(extras);
            snprintf(extras + used, sizeof extras - used, "%s%s", used ? " " : "", argv[index]);
            continue;
        }
        const Option *option = &options_table[item->option];
        if (option->kind == OPTION_FLAG) {
            if (item->value) {
                ok = feb_fail(error, FEB_EXIT_USAGE, "argument %s: ignored explicit argument '%s'",
                              !strcmp(option->dest, "help") ? "-h/--help" : option->flag,
                              item->value);
                break;
            }
            if (!strcmp(option->dest, "help")) arguments->help = true;
            else arguments->version = true;
            break;  /* both actions exit at once */
        }
        const char *value = item->value;
        if (!value) {
            Classified *next = index + 1 < argc ? &classified[index + 1] : NULL;
            if (!next || next->option != SIZE_MAX || next->unknown) {
                ok = feb_fail(error, FEB_EXIT_USAGE, "argument %s: expected one argument",
                              option->flag);
                break;
            }
            value = argv[++index];
        }
        if (option->kind == OPTION_BOOL && strcmp(value, "true") && strcmp(value, "false")) {
            ok = feb_fail(error, FEB_EXIT_USAGE, "argument %s: expected true or false",
                          option->flag);
            break;
        }
        if (option->kind == OPTION_CHOICE && strcmp(value, "json") && strcmp(value, "text") &&
            strcmp(value, "tsv")) {
            ok = feb_fail(error, FEB_EXIT_USAGE,
                          "argument %s: invalid choice: '%s' (choose from json, text, tsv)",
                          option->flag, value);
            break;
        }
        arguments->values[item->option] = value;
        arguments->present[item->option] = true;
    }
    if (ok && !arguments->help && !arguments->version && extras[0])
        ok = feb_fail(error, FEB_EXIT_USAGE, "unrecognized arguments: %s", extras);
    free(classified);
    return ok;
}

static const char *value_of(const Arguments *arguments, const char *dest) {
    size_t index = option_index(dest);
    return index != SIZE_MAX && arguments->present[index] ? arguments->values[index] : NULL;
}

static bool flag_of(const Arguments *arguments, const char *dest) {
    const char *value = value_of(arguments, dest);
    return value && !strcmp(value, "true");
}

/* _clean_options(): every given command option, booleans as JSON booleans. */
static bool clean_options(const Arguments *arguments, FusionJson *options) {
    *options = fusion_json_object();
    for (size_t index = 0; index < COUNT(options_table); ++index) {
        if (!arguments->present[index]) continue;
        bool common = false;
        for (size_t other = 0; other < COUNT(common_options); ++other)
            if (!strcmp(common_options[other], options_table[index].dest)) common = true;
        if (common) continue;
        FusionJson value = options_table[index].kind == OPTION_BOOL
            ? fusion_json_bool(!strcmp(arguments->values[index], "true"))
            : fusion_json_string(arguments->values[index]);
        if (!fusion_json_set(options, options_table[index].dest, value)) return false;
    }
    return true;
}

static bool is_tsv_command(const char *command) {
    for (size_t index = 0; index < COUNT(tsv_commands); ++index)
        if (!strcmp(tsv_commands[index], command)) return true;
    return false;
}

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Text;

static bool text_put(Text *text, const char *data, size_t length) {
    if (text->size + length + 1 > text->capacity) {
        size_t next = text->capacity ? text->capacity : 1024;
        while (next < text->size + length + 1) next *= 2;
        char *grown = realloc(text->data, next);
        if (!grown) return false;
        text->data = grown;
        text->capacity = next;
    }
    memcpy(text->data + text->size, data, length);
    text->size += length;
    text->data[text->size] = '\0';
    return true;
}

static bool text_puts(Text *text, const char *data) {
    return text_put(text, data, strlen(data));
}

/* _safe_tsv_field(): str(value), refusing separators and long fields. */
static bool tsv_field(Text *text, const FusionJson *value, bool first, FebError *error) {
    char buffer[64];
    const char *field = buffer;
    switch (value ? value->kind : FUSION_JSON_NULL) {
        case FUSION_JSON_STRING: field = value->string; break;
        case FUSION_JSON_BOOL: field = value->boolean ? "True" : "False"; break;
        case FUSION_JSON_NUMBER:
            if (value->integral) snprintf(buffer, sizeof buffer, "%lld", (long long)value->integer);
            else fusion_json_float_repr(value->number, buffer, sizeof buffer);
            break;
        default: field = "None"; break;
    }
    if (feb_utf8_length(field) > 4096 || strpbrk(field, "\t\n\r"))
        return feb_fail(error, FEB_EXIT_USAGE, "unsafe TSV field returned by backend");
    if ((!first && !text_puts(text, "\t")) || !text_puts(text, field)) return feb_oom(error);
    return true;
}

static const FusionJson *path(const FusionJson *value, const char *first, const char *second) {
    const FusionJson *member = fusion_json_member(value, first);
    return second ? fusion_json_member(member, second) : member;
}

static bool tsv_row(Text *text, const FusionJson *item, const char *const (*fields)[2],
                    size_t count, FebError *error) {
    for (size_t index = 0; index < count; ++index)
        if (!tsv_field(text, path(item, fields[index][0], fields[index][1]), index == 0, error))
            return false;
    return text_puts(text, "\n") || feb_oom(error);
}

/* _tsv_result() for the ported commands. */
static bool tsv_result(const char *command, const FusionJson *data, Text *text,
                       FebError *error) {
    if (!strcmp(command, "room-list") || !strcmp(command, "list-rooms")) {
        const FusionJson *source = fusion_json_member(data, "source");
        if (!source || source->kind != FUSION_JSON_STRING || strcmp(source->string, "draft"))
            return feb_fail(error, FEB_EXIT_USAGE,
                            "TSV room listing is available only for draft rooms");
        static const char *const fields[][2] = {
            {"id", NULL}, {"name", NULL}, {"geometry", "width_screens"},
            {"geometry", "height_screens"}};
        const FusionJson *rooms = fusion_json_member(data, "rooms");
        for (size_t index = 0; index < rooms->count; ++index)
            if (!tsv_row(text, &rooms->items[index], fields, COUNT(fields), error)) return false;
        return true;
    }
    if (!strcmp(command, "placement-list")) {
        static const char *const fields[][2] = {
            {"id", NULL}, {"name", NULL}, {"world", NULL}, {"area", NULL}, {"x", NULL},
            {"y", NULL}, {"width_screens", NULL}, {"height_screens", NULL}};
        const FusionJson *placements = fusion_json_member(data, "placements");
        for (size_t index = 0; index < placements->count; ++index)
            if (!tsv_row(text, &placements->items[index], fields, COUNT(fields), error))
                return false;
        return true;
    }
    return feb_fail(error, FEB_EXIT_USAGE, "TSV output is unavailable for %s", command);
}

static bool has_dotdot(const char *path_text) {
    char copy[FEB_PATH_CAPACITY];
    snprintf(copy, sizeof copy, "%s", path_text);
    for (char *part = strtok(copy, "/"); part; part = strtok(NULL, "/"))
        if (!strcmp(part, "..")) return true;
    return false;
}

/* _safe_output(): an atomic write that refuses traversal and symlinks. */
static bool safe_output(const char *target, const char *payload, size_t size,
                        FebError *error) {
    if (has_dotdot(target))
        return feb_fail(error, FEB_EXIT_USAGE, "output path traversal refused");
    char absolute[FEB_PATH_CAPACITY], current[FEB_PATH_CAPACITY] = "", copy[FEB_PATH_CAPACITY];
    if (!feb_absolute(target, absolute))
        return feb_fail(error, FEB_EXIT_OPERATION, "output path is too long");
    snprintf(copy, sizeof copy, "%s", absolute);
    for (char *part = strtok(copy, "/"); part; part = strtok(NULL, "/")) {
        if (!strcmp(part, ".")) continue;
        size_t used = strlen(current);
        snprintf(current + used, sizeof current - used, "/%s", part);
        if (feb_is_symlink(current))
            return feb_fail(error, FEB_EXIT_USAGE, "symlink output path refused: %s", current);
    }
    char parent[FEB_PATH_CAPACITY];
    snprintf(parent, sizeof parent, "%s", current);
    char *slash = strrchr(parent, '/');
    if (!slash) return feb_fail(error, FEB_EXIT_OPERATION, "invalid output path");
    *slash = '\0';
    for (char *cursor = parent + 1; ; ++cursor) {
        if (*cursor == '/' || *cursor == '\0') {
            char saved = *cursor;
            *cursor = '\0';
            if (!feb_is_directory(parent) && mkdir(parent, 0777) != 0 && errno != EEXIST)
                return feb_fail(error, FEB_EXIT_OPERATION, "[Errno %d] %s: '%s'", errno,
                                strerror(errno), parent);
            *cursor = saved;
            if (!saved) break;
        }
    }
    char temporary[2 * FEB_PATH_CAPACITY];
    snprintf(temporary, sizeof temporary, "%s/.%s.XXXXXX.tmp", parent, slash + 1);
    int descriptor = mkstemps(temporary, 4);
    if (descriptor < 0)
        return feb_fail(error, FEB_EXIT_OPERATION, "[Errno %d] %s", errno, strerror(errno));
    bool ok = write(descriptor, payload, size) == (ssize_t)size && fsync(descriptor) == 0;
    ok = close(descriptor) == 0 && ok;
    if (!ok || rename(temporary, current) != 0) {
        unlink(temporary);
        return feb_fail(error, FEB_EXIT_OPERATION, "cannot write %s", current);
    }
    return true;
}

static bool add_created(FusionJson *created, const FusionJson *value) {
    if (!value || value->kind == FUSION_JSON_NULL ||
        (value->kind == FUSION_JSON_STRING && !value->string[0]) ||
        (value->kind == FUSION_JSON_BOOL && !value->boolean))
        return true;
    FusionJson copy;
    return fusion_json_clone(value, &copy) && fusion_json_push(created, copy);
}

/* _text_result(). */
static bool text_result(const char *command, const FusionJson *data, const FebError *failure,
                        Text *text) {
    if (failure)
        return text_puts(text, command) && text_puts(text, ": ") &&
               text_puts(text, failure->message);
    char *json = NULL;
    if (!fusion_json_write(data, 2, &json, NULL)) return false;
    bool ok = text_puts(text, command) && text_puts(text, ": success\n") && text_puts(text, json);
    free(json);
    return ok;
}

static bool json_result(const char *command, FusionJson *data, const FebError *failure,
                        FusionJson *created, Text *text) {
    FusionJson result = fusion_json_object(), copy;
    bool ok = feb_put_bool(&result, "success", failure == NULL) &&
              feb_put_string(&result, "command", command);
    if (ok && failure) ok = feb_put(&result, "data", fusion_json_null());
    else if (ok) ok = fusion_json_clone(data, &copy) && feb_put(&result, "data", copy);
    ok = ok && feb_put(&result, "warnings", fusion_json_array());
    if (ok && failure) {
        FusionJson error = fusion_json_object();
        const char *type = failure->code == FEB_EXIT_USAGE ? "usage"
                         : failure->code == FEB_EXIT_UNAVAILABLE ? "unavailable" : "operation";
        ok = feb_put(&result, "created_paths", fusion_json_array()) &&
             feb_put_string(&error, "type", type) &&
             feb_put_string(&error, "message", failure->message) &&
             feb_put(&result, "error", error);
    } else if (ok) {
        ok = fusion_json_clone(created, &copy) && feb_put(&result, "created_paths", copy);
    }
    char *json = NULL;
    ok = ok && fusion_json_write(&result, FUSION_JSON_COMPACT, &json, NULL) &&
         text_puts(text, json);
    free(json);
    fusion_json_free(&result);
    return ok;
}

int feb_cli_main(int argc, char **argv, const char *repository, bool *handled) {
    *handled = true;
    FebError failure = {0};
    Arguments arguments;
    const char *format = "text", *command = "unknown";
    FusionJson options = {0}, data = {0}, created = fusion_json_array();
    Text payload = {0};
    bool ok = parse_arguments(argc, argv, &arguments, &failure);
    if (ok && arguments.help) {
        *handled = false;  /* the help text still comes from the Python parser */
        return 0;
    }
    if (ok && arguments.version) {
        puts(FEB_BACKEND_VERSION);
        return FEB_EXIT_SUCCESS;
    }
    if (ok) {
        if (value_of(&arguments, "format")) format = value_of(&arguments, "format");
        const char *requested = value_of(&arguments, "command");
        const char *batch = value_of(&arguments, "batch");
        bool has_command = requested && requested[0], has_batch = batch && batch[0];
        if (has_command == has_batch) {
            ok = feb_fail(&failure, FEB_EXIT_USAGE, "provide exactly one of --command or --batch");
        } else if (has_batch || !feb_is_native(requested)) {
            *handled = false;
            return 0;
        } else {
            command = requested;
        }
    }
    if (ok && !strcmp(format, "tsv") && !is_tsv_command(command))
        ok = feb_fail(&failure, FEB_EXIT_USAGE, "TSV output is unavailable for %s", command);
    if (ok && !clean_options(&arguments, &options)) ok = feb_oom(&failure);
    if (ok) {
        bool known = false;
        const char *const *fields = feb_command_fields(command, &known);
        char unknown[FEB_MESSAGE_CAPACITY];
        if (known && !feb_unknown_options(&options, fields, unknown, sizeof unknown))
            ok = feb_fail(&failure, FEB_EXIT_USAGE, "unknown option(s) for %s: %s", command,
                          unknown);
    }
    const char *root = value_of(&arguments, "root") ? value_of(&arguments, "root") : repository;
    if (ok)
        ok = feb_execute(command, &options, root, repository, flag_of(&arguments, "dry_run"),
                         &data, &failure);
    if (ok && data.kind == FUSION_JSON_OBJECT) {
        ok = add_created(&created, fusion_json_member(&data, "path")) &&
             add_created(&created, fusion_json_member(&data, "report_path"));
        const FusionJson *more = fusion_json_member(&data, "created_paths");
        for (size_t index = 0; ok && more && index < more->count; ++index)
            ok = add_created(&created, &more->items[index]);
        if (!ok) feb_oom(&failure);
    }
    if (ok) {
        if (!strcmp(format, "json"))
            ok = (json_result(command, &data, NULL, &created, &payload) &&
                  text_puts(&payload, "\n")) || feb_oom(&failure);
        else if (!strcmp(format, "tsv"))
            ok = tsv_result(command, &data, &payload, &failure);
        else
            ok = (text_result(command, &data, NULL, &payload) && text_puts(&payload, "\n")) ||
                 feb_oom(&failure);
    }
    const char *output = value_of(&arguments, "output");
    if (ok && output) ok = safe_output(output, payload.data ? payload.data : "", payload.size,
                                       &failure);
    int code = FEB_EXIT_SUCCESS;
    if (ok) {
        fwrite(payload.data ? payload.data : "", 1, payload.size, stdout);
    } else {
        code = failure.code ? failure.code : FEB_EXIT_OPERATION;
        fprintf(stderr, "%s\n", failure.message);
        Text text = {0};
        bool written = !strcmp(format, "json")
            ? json_result(command, NULL, &failure, NULL, &text)
            : text_result(command, NULL, &failure, &text);
        if (written) printf("%s\n", text.data);
        free(text.data);
    }
    fflush(stdout);
    fusion_json_free(&options);
    fusion_json_free(&data);
    fusion_json_free(&created);
    free(payload.data);
    return code;
}
