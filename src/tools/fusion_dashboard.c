/* SPDX-License-Identifier: GPL-3.0-only */
/* Generates the data of the development dashboard (dashboard/), formerly
 * scripts/dashboard.py. The dashboard observes the project; it never drives
 * it. The generator reads the native parity annotations, the project feature
 * registry, CMakeLists.txt, the tracked native inventory, the rebuild graph
 * and the git history; with --run-tests it also runs CTest and records its
 * results.
 *
 * Every feature carries declared facts (status, fidelity, limitations, from a
 * registry) and verified facts (its sources exist, its tests are registered
 * and, with --run-tests, passed). Percentages are shares of registered
 * features with their denominators. The outputs, dashboard/data.json and the
 * same data as a script in dashboard/data.js, never contain ROM data,
 * extracted assets or local paths.
 *
 * --check writes nothing: it rebuilds the data without running tests and
 * reports features whose registry-derived facts or status counts differ from
 * the committed output, and a data.js that does not carry data.json. */
#define _XOPEN_SOURCE 700
#include "core/json.h"
#include "fusion/assets/rom.h"
#include "fusion/pipeline/rebuild.h"
#include "../editor_backend/object_catalog.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifndef FUSION_SOURCE_DIR
#error "FUSION_SOURCE_DIR must identify the repository source directory"
#endif

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

static const char root[] = FUSION_SOURCE_DIR;
static const char schema[] = "metroidvania-dashboard-v1";
static const char all_tests[] = "@all-tests";
static const char *const registry_header[] = {
    "area", "id", "category", "status", "fidelity", "title", "local_sources", "tests",
    "dependencies", "limitations", "last_validated"};
static const char *const areas[][2] = {
    {"mzm", "Metroid: Zero Mission"}, {"aos", "Castlevania: Aria of Sorrow"},
    {"metroidvania", "Metroidvania"},  {"story", "Story and events"},
    {"tools", "Tools and editors"},    {"assets", "Asset reconstruction"},
    {"platform", "Platforms"}};
static const char *const statuses[][3] = {
    {"not_started", "Not started", "No implementation."},
    {"in_progress", "In progress", "Work started; nothing usable yet."},
    {"partial", "Partially functional", "Part of the feature works."},
    {"functional", "Functional",
     "Works for its intended use; fidelity to the original is not claimed."},
    {"faithful", "Faithful", "Matches the original game's behavior as traced."},
    {"validated", "Validated", "Faithful or complete, and covered by passing tests."},
    {"blocked", "Blocked", "Cannot progress until something else is resolved."},
    {"unknown", "Unknown", "Status not established or not verified."}};
static const char *const fidelities[] = {"n/a", "unknown", "approximate", "partial", "faithful"};
/* Native annotation statuses on the dashboard scale. */
static const char *const annotation_statuses[][2] = {
    {"missing", "not_started"}, {"partial", "partial"},   {"functional", "functional"},
    {"validated", "validated"}, {"research", "unknown"}, {"blocked", "blocked"}};
static const char *const indicators[][3] = {
    {"global", "Overall", NULL},
    {"mzm", "Zero Mission engine", "mzm"},
    {"aos", "Aria of Sorrow engine", "aos"},
    {"assets", "Asset reconstruction", "assets"},
    {"tools", "Tools and editors", "tools"},
    {"metroidvania", "Metroidvania gameplay", "metroidvania"},
    {"story", "Story and events", "story"},
    {"platform", "Platforms", "platform"}};
static const char *const annotation_columns[] = {
    "game", "id", "category", "status", "title", "native_routines", "local_sources", "tests",
    "dependencies", "divergences", "notes", "next_action", "in_game_check",
    "emulator_compared", "played_through"};

static char failure[2048];

static bool fail(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(failure, sizeof failure, format, arguments);
    va_end(arguments);
    return false;
}

static void oom(void) {
    fputs("fusion_dashboard: out of memory\n", stderr);
    exit(2);
}

static void put(FusionJson *object, const char *key, FusionJson value) {
    if (!fusion_json_set(object, key, value)) oom();
}

static void put_string(FusionJson *object, const char *key, const char *value) {
    put(object, key, fusion_json_string(value));
}

static void put_int(FusionJson *object, const char *key, int64_t value) {
    put(object, key, fusion_json_int(value));
}

static void put_bool(FusionJson *object, const char *key, bool value) {
    put(object, key, fusion_json_bool(value));
}

static void push(FusionJson *array, FusionJson value) {
    if (!fusion_json_push(array, value)) oom();
}

static FusionJson copy_of(const FusionJson *value) {
    FusionJson copy;
    if (!value) return fusion_json_null();
    if (!fusion_json_clone(value, &copy)) oom();
    return copy;
}

static const char *text_of(const FusionJson *object, const char *key) {
    const FusionJson *value = fusion_json_member(object, key);
    return value && value->kind == FUSION_JSON_STRING ? value->string : "";
}

static bool contains(const FusionJson *array, const char *value) {
    for (size_t index = 0; array && index < array->count; ++index)
        if (array->items[index].kind == FUSION_JSON_STRING &&
            !strcmp(array->items[index].string, value))
            return true;
    return false;
}

static char *read_file(const char *relative, size_t *size) {
    char path[8192];
    snprintf(path, sizeof path, "%s/%s", root, relative);
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    char *text = malloc((size_t)length + 1);
    if (!text) oom();
    size_t read = fread(text, 1, (size_t)length, file);
    fclose(file);
    text[read] = '\0';
    if (size) *size = read;
    return text;
}

static bool path_exists(const char *relative) {
    char path[8192];
    struct stat status;
    snprintf(path, sizeof path, "%s/%s", root, relative);
    return stat(path, &status) == 0;
}

/* The output of a shell command run in the repository (stdout only). */
static char *capture(const char *command, int *status) {
    char line[8192];
    snprintf(line, sizeof line, "cd '%s' && %s", root, command);
    FILE *pipe = popen(line, "r");
    if (!pipe) return NULL;
    size_t size = 0, capacity = 1 << 16;
    char *text = malloc(capacity);
    if (!text) oom();
    for (size_t got; (got = fread(text + size, 1, capacity - size - 1, pipe)) > 0;) {
        size += got;
        if (size + 1 == capacity) {
            text = realloc(text, capacity *= 2);
            if (!text) oom();
        }
    }
    text[size] = '\0';
    int code = pclose(pipe);
    if (status) *status = code;
    return text;
}

/* "a|b||c" as ["a", "b", "c"]. */
static FusionJson split_list(const char *value) {
    FusionJson list = fusion_json_array();
    const char *start = value;
    for (const char *c = value;; ++c) {
        if (*c == '|' || !*c) {
            if (c > start) {
                char *item = strndup(start, (size_t)(c - start));
                if (!item) oom();
                push(&list, fusion_json_string(item));
                free(item);
            }
            if (!*c) break;
            start = c + 1;
        }
    }
    return list;
}

/* line.split("\t") into at most ``count`` fields; the field count is returned. */
static size_t split_tabs(char *line, char **fields, size_t count) {
    size_t used = 0;
    for (char *part = line;;) {
        char *tab = strchr(part, '\t');
        if (used < count) fields[used] = part;
        ++used;
        if (!tab) break;
        *tab = '\0';
        part = tab + 1;
    }
    return used;
}

static int area_index(const char *area) {
    for (size_t index = 0; index < COUNT(areas); ++index)
        if (!strcmp(areas[index][0], area)) return (int)index;
    return -1;
}

static bool known(const char *value, const char *const *list, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (!strcmp(list[index], value)) return true;
    return false;
}

static bool is_status(const char *value) {
    for (size_t index = 0; index < COUNT(statuses); ++index)
        if (!strcmp(statuses[index][0], value)) return true;
    return false;
}

static FusionJson evidence(FusionJson tests, FusionJson checks, bool emulator, bool played) {
    FusionJson value = fusion_json_object();
    put(&value, "unit_tests", tests);
    put(&value, "in_game_check", checks);
    put_bool(&value, "emulator_compared", emulator);
    put_bool(&value, "played_through", played);
    return value;
}

static char *read_absolute(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    char *text = malloc((size_t)length + 1);
    if (!text) oom();
    size_t read = fread(text, 1, (size_t)length, file);
    fclose(file);
    text[read] = '\0';
    return text;
}

/* load_registry(): the project feature registry at ``path``, validated. */
static bool load_registry(const char *path, FusionJson *features) {
    char *text = read_absolute(path);
    if (!text) return fail("[Errno 2] No such file or directory: '%s'", path);
    char *next = NULL, *line = text;
    int number = 1;
    bool ok = true;
    FusionJson seen = fusion_json_array();
    for (; ok && line; line = next, ++number) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        size_t length = strlen(line);
        if (length && line[length - 1] == '\r') line[--length] = '\0';
        if (!next && !*line) break;
        char *fields[16];
        if (number == 1) {
            size_t count = split_tabs(line, fields, 16);
            bool header = count == COUNT(registry_header);
            for (size_t index = 0; header && index < count; ++index)
                header = !strcmp(fields[index], registry_header[index]);
            if (!header) ok = fail("%s: unexpected header", path);
            continue;
        }
        bool blank = true;
        for (const char *c = line; *c; ++c)
            if (!isspace((unsigned char)*c)) blank = false;
        if (blank) continue;
        size_t count = split_tabs(line, fields, 16);
        if (count != COUNT(registry_header)) {
            ok = fail("%s:%d: expected %zu columns", path, number, COUNT(registry_header));
            break;
        }
        const char *area = fields[0], *id = fields[1];
        if (area_index(area) < 0 || !strcmp(area, "mzm") || !strcmp(area, "aos")) {
            ok = fail("%s:%d: unknown area '%s' (native features belong in the parity "
                      "annotations)", path, number, area);
            break;
        }
        char prefix[64];
        snprintf(prefix, sizeof prefix, "%s.", !strcmp(area, "metroidvania") ? "mv" : area);
        if (strncmp(id, prefix, strlen(prefix))) {
            ok = fail("%s:%d: id '%s' does not match its area", path, number, id);
            break;
        }
        if (contains(&seen, id)) {
            ok = fail("%s:%d: duplicate id %s", path, number, id);
            break;
        }
        push(&seen, fusion_json_string(id));
        if (!is_status(fields[3])) {
            ok = fail("%s:%d: unknown status '%s'", path, number, fields[3]);
            break;
        }
        if (!known(fields[4], fidelities, COUNT(fidelities))) {
            ok = fail("%s:%d: unknown fidelity '%s'", path, number, fields[4]);
            break;
        }
        const char *date = fields[10];
        bool date_ok = !*date || (strlen(date) == 10 && date[4] == '-' && date[7] == '-');
        for (int index = 0; date_ok && *date && index < 10; ++index)
            if (index != 4 && index != 7 && !isdigit((unsigned char)date[index])) date_ok = false;
        if (!date_ok) {
            ok = fail("%s:%d: last_validated must be YYYY-MM-DD", path, number);
            break;
        }
        FusionJson feature = fusion_json_object();
        put_string(&feature, "id", id);
        put_string(&feature, "area", area);
        put_string(&feature, "category", fields[2]);
        put_string(&feature, "title", fields[5]);
        put_string(&feature, "status", fields[3]);
        put_string(&feature, "fidelity", fields[4]);
        put(&feature, "local_sources", split_list(fields[6]));
        put(&feature, "tests", split_list(fields[7]));
        put(&feature, "dependencies", split_list(fields[8]));
        put(&feature, "native_routines", fusion_json_array());
        put_string(&feature, "limitations", fields[9]);
        put_string(&feature, "next_action", "");
        put(&feature, "evidence",
            evidence(split_list(fields[7]), fusion_json_array(), false, false));
        put(&feature, "last_validated", *date ? fusion_json_string(date) : fusion_json_null());
        put_string(&feature, "source", "data/dashboard/features.tsv");
        push(features, feature);
    }
    fusion_json_free(&seen);
    free(text);
    return ok;
}

/* native_parity.load_annotations(): {game: [feature records]}. */
static bool load_annotations(FusionJson *games) {
    const char *relative = "data/native_parity/annotations.tsv";
    char *text = read_file(relative, NULL);
    char path[8192];
    snprintf(path, sizeof path, "%s/%s", root, relative);
    if (!text) return fail("cannot load %s", path);
    *games = fusion_json_object();
    char previous[512] = "";
    int number = 1;
    bool ok = true;
    for (char *line = text, *next; ok && line; line = next, ++number) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        char *fields[20];
        if (number == 1) {
            size_t count = split_tabs(line, fields, 20);
            bool header = count == COUNT(annotation_columns);
            for (size_t index = 0; header && index < count; ++index)
                header = !strcmp(fields[index], annotation_columns[index]);
            if (!header) ok = fail("%s: header must be exactly the annotation columns", path);
            continue;
        }
        if (!*line) continue;
        size_t count = split_tabs(line, fields, 20);
        if (count != COUNT(annotation_columns)) {
            ok = fail("%s:%d: expected %zu columns, got %zu", path, number,
                      COUNT(annotation_columns), count);
            break;
        }
        const char *game = fields[0], *id = fields[1];
        if (strcmp(game, "mzm") && strcmp(game, "aos")) {
            ok = fail("%s:%d: unknown game: '%s'", path, number, game);
            break;
        }
        if (strcmp(id, previous) <= 0) {
            ok = fail("%s:%d: rows must be sorted by unique id; '%s' is out of order or "
                      "duplicated", path, number, id);
            break;
        }
        snprintf(previous, sizeof previous, "%s", id);
        FusionJson feature = fusion_json_object();
        put_string(&feature, "id", id);
        put_string(&feature, "category", fields[2]);
        put_string(&feature, "status", fields[3]);
        put_string(&feature, "title", fields[4]);
        put(&feature, "native_symbols", split_list(fields[5]));
        put(&feature, "local_sources", split_list(fields[6]));
        put(&feature, "tests", split_list(fields[7]));
        put(&feature, "dependencies", split_list(fields[8]));
        put_string(&feature, "divergences", fields[9]);
        put_string(&feature, "notes", fields[10]);
        put_string(&feature, "next_action", fields[11]);
        put(&feature, "in_game_check", split_list(fields[12]));
        put_string(&feature, "emulator_compared", fields[13]);
        put_string(&feature, "played_through", fields[14]);
        FusionJson *list = fusion_json_member_mut(games, game);
        if (!list) {
            put(games, game, fusion_json_array());
            list = fusion_json_member_mut(games, game);
        }
        push(list, feature);
    }
    free(text);
    if (!ok) fusion_json_free(games);
    return ok;
}

static const char *dashboard_status(const char *annotation) {
    for (size_t index = 0; index < COUNT(annotation_statuses); ++index)
        if (!strcmp(annotation_statuses[index][0], annotation)) return annotation_statuses[index][1];
    return "unknown";
}

/* load_native_features(): both reconstructions on the dashboard scale. */
static void native_features(const FusionJson *annotations, FusionJson *features) {
    static const char *const games[] = {"mzm", "aos"};
    for (size_t game = 0; game < 2; ++game) {
        const FusionJson *list = fusion_json_member(annotations, games[game]);
        for (size_t index = 0; list && index < list->count; ++index) {
            const FusionJson *item = &list->items[index];
            const char *status = dashboard_status(text_of(item, "status"));
            FusionJson feature = fusion_json_object();
            put_string(&feature, "id", text_of(item, "id"));
            put_string(&feature, "area", games[game]);
            put_string(&feature, "category", text_of(item, "category"));
            put_string(&feature, "title", text_of(item, "title"));
            put_string(&feature, "status", status);
            put_string(&feature, "fidelity", !strcmp(status, "partial") ? "partial"
                                             : !strcmp(status, "validated") ? "faithful"
                                                                            : "unknown");
            put(&feature, "local_sources", copy_of(fusion_json_member(item, "local_sources")));
            put(&feature, "tests", copy_of(fusion_json_member(item, "tests")));
            put(&feature, "dependencies", copy_of(fusion_json_member(item, "dependencies")));
            put(&feature, "native_routines", copy_of(fusion_json_member(item, "native_symbols")));
            const char *divergences = text_of(item, "divergences"), *notes = text_of(item, "notes");
            char *limitations = malloc(strlen(divergences) + strlen(notes) + 2);
            if (!limitations) oom();
            sprintf(limitations, "%s%s%s", divergences, *divergences && *notes ? " " : "", notes);
            put_string(&feature, "limitations", limitations);
            free(limitations);
            put_string(&feature, "next_action", text_of(item, "next_action"));
            put(&feature, "evidence",
                evidence(copy_of(fusion_json_member(item, "tests")),
                         copy_of(fusion_json_member(item, "in_game_check")),
                         !strcmp(text_of(item, "emulator_compared"), "yes"),
                         !strcmp(text_of(item, "played_through"), "yes")));
            put(&feature, "last_validated", fusion_json_null());
            put_string(&feature, "source", "data/native_parity/annotations.tsv");
            push(features, feature);
        }
    }
}

static bool word(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static bool name_char(char c) {
    return word(c) || c == '-';
}

static bool path_char(char c) {
    return word(c) || c == '.' || c == '/' || c == '-';
}

/* Every tests/[\w./-]+ in [start, end) appended to ``files`` once. */
static void test_paths(const char *start, const char *end, FusionJson *files) {
    for (const char *c = start; c + 6 <= end; ++c) {
        if (strncmp(c, "tests/", 6)) continue;
        const char *finish = c + 6;
        while (finish < end && path_char(*finish)) ++finish;
        if (finish == c + 6) continue;
        char *path = strndup(c, (size_t)(finish - c));
        if (!path) oom();
        push(files, fusion_json_string(path));
        free(path);
        c = finish - 1;
    }
}

static int compare_strings(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

/* cmake_tests(): CTest names with the test sources of their executables. */
static FusionJson cmake_tests(void) {
    char *text = read_file("CMakeLists.txt", NULL);
    if (!text) text = calloc(1, 1);
    /* re.sub(r"#[^\n]*", "", text) */
    char *write = text;
    for (const char *c = text; *c;) {
        if (*c == '#') {
            while (*c && *c != '\n') ++c;
            continue;
        }
        *write++ = *c++;
    }
    *write = '\0';
    FusionJson sources = fusion_json_object(), tests = fusion_json_object();
    /* add_executable\(\s*([\w-]+)([^)]*)\) */
    for (const char *c = strstr(text, "add_executable("); c; c = strstr(c + 1, "add_executable(")) {
        const char *name = c + 15;
        while (isspace((unsigned char)*name)) ++name;
        const char *name_end = name;
        while (name_char(*name_end)) ++name_end;
        if (name_end == name) continue;
        const char *close = strchr(name_end, ')');
        if (!close) continue;
        char *target = strndup(name, (size_t)(name_end - name));
        FusionJson files = fusion_json_array();
        test_paths(name_end, close, &files);
        put(&sources, target, files);
        free(target);
    }
    /* add_test\(\s*NAME\s+([\w-]+)([^)]*)\) */
    for (const char *c = strstr(text, "add_test("); c; c = strstr(c + 1, "add_test(")) {
        const char *at = c + 9;
        while (isspace((unsigned char)*at)) ++at;
        if (strncmp(at, "NAME", 4) || !isspace((unsigned char)at[4])) continue;
        at += 4;
        while (isspace((unsigned char)*at)) ++at;
        const char *name_end = at;
        while (name_char(*name_end)) ++name_end;
        if (name_end == at) continue;
        const char *close = strchr(name_end, ')');
        if (!close) continue;
        FusionJson files = fusion_json_array();
        test_paths(name_end, close, &files);
        /* \$<TARGET_FILE:([\w-]+)>|COMMAND\s+([\w-]+) */
        for (const char *probe = name_end; probe < close; ++probe) {
            const char *target = NULL, *target_end = NULL;
            if (!strncmp(probe, "$<TARGET_FILE:", 14)) {
                target = probe + 14;
                target_end = target;
                while (target_end < close && name_char(*target_end)) ++target_end;
                if (target_end == target || *target_end != '>') continue;
            } else if (!strncmp(probe, "COMMAND", 7) && probe + 7 < close &&
                       isspace((unsigned char)probe[7])) {
                target = probe + 7;
                while (target < close && isspace((unsigned char)*target)) ++target;
                target_end = target;
                while (target_end < close && name_char(*target_end)) ++target_end;
                if (target_end == target) continue;
            } else {
                continue;
            }
            char *name = strndup(target, (size_t)(target_end - target));
            const FusionJson *more = fusion_json_member(&sources, name);
            for (size_t index = 0; more && index < more->count; ++index)
                push(&files, copy_of(&more->items[index]));
            free(name);
            probe = target_end - 1;
        }
        /* sorted(set(files)) */
        char **names = malloc((files.count + 1) * sizeof *names);
        size_t unique = 0;
        for (size_t index = 0; index < files.count; ++index) names[index] = files.items[index].string;
        qsort(names, files.count, sizeof *names, compare_strings);
        FusionJson sorted = fusion_json_array();
        for (size_t index = 0; index < files.count; ++index)
            if (!unique || strcmp(names[index], names[index - 1])) {
                push(&sorted, fusion_json_string(names[index]));
                ++unique;
            }
        free(names);
        fusion_json_free(&files);
        char *test = strndup(at, (size_t)(name_end - at));
        FusionJson entry = fusion_json_object();
        put_string(&entry, "name", test);
        put(&entry, "files", sorted);
        put(&tests, test, entry);
        free(test);
    }
    fusion_json_free(&sources);
    free(text);
    return tests;
}

/* run_ctest(): every CTest test. */
static FusionJson default_run_ctest(const char *build) {
    char command[8192];
    snprintf(command, sizeof command, "ctest --test-dir '%s' 2>/dev/null", build);
    char *output = capture(command, NULL);
    FusionJson outcomes = fusion_json_object();
    /* Test\s+#\d+:\s+([\w-]+)\s+\.+\s*(\*{0,3}\s*\w[^\n]*?)\s+[\d.]+ sec */
    for (char *line = output, *next; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        char *test = strstr(line, "Test");
        while (test) {
            char *c = test + 4;
            if (!isspace((unsigned char)*c)) {
                test = strstr(test + 1, "Test");
                continue;
            }
            while (isspace((unsigned char)*c)) ++c;
            if (*c != '#' || !isdigit((unsigned char)c[1])) {
                test = strstr(test + 1, "Test");
                continue;
            }
            ++c;
            while (isdigit((unsigned char)*c)) ++c;
            if (*c != ':') break;
            ++c;
            while (isspace((unsigned char)*c)) ++c;
            char *name = c;
            while (name_char(*c)) ++c;
            if (c == name || !isspace((unsigned char)*c)) break;
            char *name_end = c;
            while (isspace((unsigned char)*c)) ++c;
            if (*c != '.') break;
            while (*c == '.') ++c;
            char *seconds = strstr(c, " sec");
            if (!seconds) break;
            char saved = *name_end;
            *name_end = '\0';
            char *result = strndup(c, (size_t)(seconds - c));
            put_string(&outcomes, name, strstr(result, "Passed") ? "passed" : "failed");
            free(result);
            *name_end = saved;
            break;
        }
    }
    free(output);
    return outcomes;
}

/* The test runners, replaceable by the tests of this generator. */
static FusionJson (*run_ctest)(const char *build) = default_run_ctest;

/* _verify(): what this run checked about one feature. */
static FusionJson verify(const FusionJson *feature, const FusionJson *ctest,
                         const FusionJson *ctest_results) {
    FusionJson missing = fusion_json_array(), runs = fusion_json_array(), warnings = fusion_json_array();
    const FusionJson *sources = fusion_json_member(feature, "local_sources");
    for (size_t index = 0; index < sources->count; ++index)
        if (!path_exists(sources->items[index].string)) push(&missing, copy_of(&sources->items[index]));
    const FusionJson *tests = fusion_json_member(feature, "tests");
    for (size_t index = 0; index < tests->count; ++index) {
        const char *path = tests->items[index].string;
        FusionJson run = fusion_json_object();
        if (!strcmp(path, all_tests)) {
            bool failed = false;
            for (size_t other = 0; ctest_results && other < ctest_results->count; ++other)
                failed |= !strcmp(ctest_results->items[other].string, "failed");
            char name[64];
            snprintf(name, sizeof name, "%s on linux", all_tests);
            put_string(&run, "test", name);
            put_string(&run, "kind", "suite");
            put_bool(&run, "registered", true);
            put(&run, "outcome", ctest_results
                                     ? fusion_json_string(failed ? "failed" : "passed")
                                     : fusion_json_null());
            push(&runs, run);
            continue;
        }
        FusionJson names = fusion_json_array();
        for (size_t test = 0; test < ctest->count; ++test)
            if (contains(fusion_json_member(&ctest->items[test], "files"), path))
                push(&names, fusion_json_string(ctest->keys[test]));
        FusionJson outcome = fusion_json_null();
        if (ctest_results && names.count) {
            bool failed = false, all_passed = true;
            for (size_t other = 0; other < names.count; ++other) {
                const FusionJson *state = fusion_json_member(ctest_results, names.items[other].string);
                const char *value = state ? state->string : "not run";
                failed |= !strcmp(value, "failed");
                all_passed &= !strcmp(value, "passed");
            }
            outcome = fusion_json_string(failed ? "failed" : all_passed ? "passed" : "not run");
        }
        put_string(&run, "test", path);
        put_string(&run, "kind", "ctest");
        put_bool(&run, "registered", names.count > 0);
        put(&run, "ctest", names);
        put(&run, "outcome", outcome);
        put_bool(&run, "exists", path_exists(path));
        push(&runs, run);
    }
    bool any_ran = false, failed = false, all_passed = true, unregistered = false;
    for (size_t index = 0; index < runs.count; ++index) {
        const FusionJson *outcome = fusion_json_member(&runs.items[index], "outcome");
        if (!fusion_json_member(&runs.items[index], "registered")->boolean) unregistered = true;
        if (outcome->kind == FUSION_JSON_NULL) continue;
        any_ran = true;
        failed |= !strcmp(outcome->string, "failed");
        all_passed &= !strcmp(outcome->string, "passed");
    }
    const char *verdict = missing.count ? "missing_sources"
                        : !runs.count ? "untested"
                        : unregistered ? "unregistered_tests"
                        : !any_ran ? "tests_not_run"
                        : failed ? "failing"
                        : all_passed ? "passing"
                                     : "tests_not_run";
    const char *status = text_of(feature, "status");
    if ((!strcmp(status, "functional") || !strcmp(status, "validated")) &&
        strcmp(verdict, "passing")) {
        char warning[256];
        snprintf(warning, sizeof warning,
                 "declared %s, but no passing test was observed in this run", status);
        push(&warnings, fusion_json_string(warning));
    }
    if ((!strcmp(status, "functional") || !strcmp(status, "faithful") ||
         !strcmp(status, "validated")) && !sources->count)
        push(&warnings, fusion_json_string("declared working without any source file"));
    if (missing.count) {
        size_t size = 32;
        for (size_t index = 0; index < missing.count; ++index) size += strlen(missing.items[index].string) + 2;
        char *text = malloc(size);
        if (!text) oom();
        strcpy(text, "missing sources: ");
        for (size_t index = 0; index < missing.count; ++index) {
            if (index) strcat(text, ", ");
            strcat(text, missing.items[index].string);
        }
        push(&warnings, fusion_json_string(text));
        free(text);
    }
    FusionJson result = fusion_json_object();
    put_bool(&result, "sources_found", missing.count == 0);
    put(&result, "missing_sources", missing);
    put(&result, "tests", runs);
    put_string(&result, "verdict", verdict);
    put(&result, "warnings", warnings);
    return result;
}

/* round(100.0 * part / total, 1), or null for an empty total. */
static FusionJson share(int64_t part, int64_t total) {
    if (!total) return fusion_json_null();
    char text[64];
    snprintf(text, sizeof text, "%.1f", 100.0 * (double)part / (double)total);
    return fusion_json_double(strtod(text, NULL));
}

static void indicator(const FusionJson *features, const char *area, FusionJson *result) {
    int64_t counts[COUNT(statuses)] = {0}, total = 0, passing = 0;
    for (size_t index = 0; index < features->count; ++index) {
        const FusionJson *feature = &features->items[index];
        if (area && strcmp(text_of(feature, "area"), area)) continue;
        ++total;
        for (size_t status = 0; status < COUNT(statuses); ++status)
            if (!strcmp(statuses[status][0], text_of(feature, "status"))) ++counts[status];
        if (!strcmp(text_of(fusion_json_member(feature, "verification"), "verdict"), "passing"))
            ++passing;
    }
    int64_t working = counts[3] + counts[4] + counts[5];
    FusionJson by_status = fusion_json_object();
    for (size_t status = 0; status < COUNT(statuses); ++status)
        put_int(&by_status, statuses[status][0], counts[status]);
    put_int(result, "total", total);
    put(result, "by_status", by_status);
    put_int(result, "functional_or_better", working);
    put(result, "functional_or_better_percent", share(working, total));
    put_int(result, "with_passing_tests", passing);
    put(result, "with_passing_tests_percent", share(passing, total));
    put_int(result, "unknown_or_blocked", counts[7] + counts[6]);
}

static char *git(const char *arguments) {
    char command[1024];
    int status = 0;
    snprintf(command, sizeof command, "git %s 2>/dev/null", arguments);
    char *output = capture(command, &status);
    if (!output) return strdup("");
    if (status) output[0] = '\0';
    size_t length = strlen(output);
    while (length && isspace((unsigned char)output[length - 1])) output[--length] = '\0';
    char *start = output;
    while (isspace((unsigned char)*start)) ++start;
    memmove(output, start, strlen(start) + 1);
    return output;
}

static FusionJson timeline(void) {
    char *log = git("log -n80 --date=short --pretty=format:%h%x09%ad%x09%s");
    FusionJson entries = fusion_json_array();
    for (char *line = log, *next; line && *line; line = next) {
        next = strchr(line, '\n');
        if (next) *next++ = '\0';
        char *first = strchr(line, '\t'), *second = first ? strchr(first + 1, '\t') : NULL;
        if (!second) continue;
        *first = *second = '\0';
        const char *subject = second + 1;
        /* ^\w+(?:\(([\w-]+)\))?: */
        char scope[128] = "project";
        const char *c = subject;
        while (word(*c)) ++c;
        if (c > subject) {
            if (*c == '(') {
                const char *name = c + 1, *end = name;
                while (name_char(*end)) ++end;
                if (end > name && end[0] == ')' && end[1] == ':')
                    snprintf(scope, sizeof scope, "%.*s", (int)(end - name), name);
            }
        }
        FusionJson entry = fusion_json_object();
        put_string(&entry, "commit", line);
        put_string(&entry, "date", first + 1);
        put_string(&entry, "subject", subject);
        put_string(&entry, "scope", scope);
        push(&entries, entry);
    }
    free(log);
    return entries;
}

/* The routines collection of one game in the sharded native inventory. */
static bool inventory_routines(const FusionJson *summary, const char *game, FusionJson *routines) {
    *routines = fusion_json_array();
    const FusionJson *files = fusion_json_member(summary, "record_files");
    for (size_t index = 0; files && index < files->count; ++index) {
        const FusionJson *descriptor = &files->items[index];
        if (strcmp(text_of(descriptor, "collection"), "routines")) continue;
        char relative[1024];
        snprintf(relative, sizeof relative, "data/native_parity/%s", text_of(descriptor, "path"));
        size_t size = 0;
        char *payload = read_file(relative, &size);
        if (!payload) return fail("cannot load inventory shard %s/%s", root, relative);
        char digest[65];
        if (!fusion_sha256_hex((const uint8_t *)payload, size, digest) ||
            strcmp(digest, text_of(descriptor, "sha256"))) {
            free(payload);
            return fail("inventory shard checksum mismatch: %s/%s", root, relative);
        }
        FusionJson shard;
        char error[256];
        bool parsed = fusion_json_parse(payload, size, &shard, error, sizeof error);
        free(payload);
        if (!parsed) return fail("cannot load inventory shard %s/%s: %s", root, relative, error);
        if (strcmp(text_of(&shard, "game"), game)) {
            fusion_json_free(&shard);
            return fail("invalid inventory shard metadata: %s/%s", root, relative);
        }
        const FusionJson *records = fusion_json_member(&shard, "records");
        for (size_t record = 0; records && record < records->count; ++record)
            push(routines, copy_of(fusion_json_member(&records->items[record], "symbol")));
        fusion_json_free(&shard);
    }
    return true;
}

static FusionJson inventory_summary(const FusionJson *annotations) {
    FusionJson result = fusion_json_object();
    char *text = read_file("data/native_parity/inventory.json", NULL);
    FusionJson manifest;
    char error[256];
    if (!text || !fusion_json_parse(text, strlen(text), &manifest, error, sizeof error)) {
        char message[512];
        snprintf(message, sizeof message, "cannot load %s/data/native_parity/inventory.json", root);
        put_bool(&result, "available", false);
        put_string(&result, "error", message);
        free(text);
        return result;
    }
    free(text);
    FusionJson games = fusion_json_object();
    static const char *const ids[] = {"mzm", "aos"};
    for (size_t game = 0; game < 2; ++game) {
        const FusionJson *data = fusion_json_member(fusion_json_member(&manifest, "games"), ids[game]);
        FusionJson routines;
        if (!inventory_routines(data, ids[game], &routines)) {
            fusion_json_free(&games);
            fusion_json_free(&manifest);
            put_bool(&result, "available", false);
            put_string(&result, "error", failure);
            return result;
        }
        FusionJson cited = fusion_json_array();
        const FusionJson *features = fusion_json_member(annotations, ids[game]);
        for (size_t index = 0; features && index < features->count; ++index) {
            const FusionJson *symbols = fusion_json_member(&features->items[index], "native_symbols");
            for (size_t symbol = 0; symbols && symbol < symbols->count; ++symbol)
                if (!contains(&cited, symbols->items[symbol].string))
                    push(&cited, copy_of(&symbols->items[symbol]));
        }
        int64_t attached = 0;
        for (size_t index = 0; index < routines.count; ++index)
            if (routines.items[index].kind == FUSION_JSON_STRING &&
                contains(&cited, routines.items[index].string))
                ++attached;
        const FusionJson *statistics = fusion_json_member(data, "statistics");
        int64_t total = fusion_json_member(statistics, "routines")->integer;
        const FusionJson *evidence_counts = fusion_json_member(statistics, "by_category_evidence");
        const FusionJson *edges = fusion_json_member(statistics, "resolved_call_edges");
        FusionJson entry = fusion_json_object();
        put(&entry, "title", copy_of(fusion_json_member(data, "title")));
        put_int(&entry, "routines", total);
        put(&entry, "by_category", copy_of(fusion_json_member(statistics, "by_category")));
        put(&entry, "by_category_evidence",
            evidence_counts ? copy_of(evidence_counts) : fusion_json_object());
        put_int(&entry, "routines_cited_by_features", (int64_t)cited.count);
        put_int(&entry, "routines_attached_to_features", attached);
        put_int(&entry, "unassigned_routines", total - attached);
        put_int(&entry, "call_edges", edges ? edges->integer : 0);
        put(&games, ids[game], entry);
        fusion_json_free(&cited);
        fusion_json_free(&routines);
    }
    put_bool(&result, "available", true);
    put(&result, "schema", copy_of(fusion_json_member(&manifest, "schema")));
    put(&result, "games", games);
    put(&result, "not_yet_indexed",
        copy_of(fusion_json_member(
            fusion_json_member(fusion_json_member(fusion_json_member(&manifest, "games"), "aos"),
                               "coverage"),
            "not_yet_indexed")));
    fusion_json_free(&manifest);
    return result;
}

static FusionJson rebuild_tasks(void) {
    size_t count = 0;
    const FusionRebuildTask *tasks = fusion_rebuild_tasks(&count);
    FusionJson list = fusion_json_array();
    for (size_t index = 0; index < count; ++index) {
        FusionJson task = fusion_json_object(), dependencies = fusion_json_array(),
                   requires = fusion_json_array();
        for (size_t other = 0; other < tasks[index].dependency_count; ++other)
            push(&dependencies, fusion_json_string(tasks[index].dependencies[other]));
        for (size_t other = 0; other < tasks[index].requirement_count; ++other)
            push(&requires, fusion_json_string(tasks[index].requirements[other]));
        put_string(&task, "name", tasks[index].name);
        put_string(&task, "group", fusion_rebuild_group_name(tasks[index].group));
        put(&task, "dependencies", dependencies);
        put(&task, "requires", requires);
        push(&list, task);
    }
    return list;
}

/* ---- The bestiary ----------------------------------------------------- */

static const char *const mzm_drops[] = {"LARGE_ENERGY_DROP", "SMALL_ENERGY_DROP", "MISSILE_DROP",
                                        "SUPER_MISSILE_DROP", "POWER_BOMB_DROP",
                                        "MULTIPLE_LARGE_ENERGY"};
static const char *const mzm_family_annotations[][2] = {
    {"MZM_FAMILY_DROP", "mzm.items.enemy_drops"},
    {"MZM_FAMILY_TOURIAN", "mzm.enemies.tourian_defences"},
    {"MZM_FAMILY_GEEGA", "mzm.enemies.pipe_bugs"},
    {"MZM_FAMILY_ZEB", "mzm.enemies.pipe_bugs"},
    {"MZM_FAMILY_ZEBBO", "mzm.enemies.pipe_bugs"},
    {"MZM_FAMILY_GAMET", "mzm.enemies.pipe_bugs"},
    {"MZM_FAMILY_GADGET", "mzm.environment.room_gadgets"},
    {"MZM_FAMILY_CHOZO_STATUE", "mzm.items.chozo_statues"},
    {"MZM_FAMILY_ITEM_SPRITE", "mzm.items.item_sprites"},
    {"MZM_FAMILY_IMAGO_LARVA_RIGHT_SIDE", "mzm.enemies.imago_larva"}};
static const char *const aria_item_families[][2] = {
    {"money", "Money pickups"},          {"consumable", "Consumable items"},
    {"weapon", "Weapons"},               {"armor", "Armor"},
    {"accessory", "Accessories"},        {"red_soul", "Red souls (offensive)"},
    {"blue_soul", "Blue souls (guardian)"}, {"yellow_soul", "Yellow souls (passive)"},
    {"ability_soul", "Ability souls (movement)"}};
static const char *const statue_items[][2] = {
    {"LONG", "Long Beam"},     {"ICE", "Ice Beam"},         {"WAVE", "Wave Beam"},
    {"BOMB", "Bombs"},         {"SPEEDBOOSTER", "Speed Booster"}, {"HIGH_JUMP", "High Jump"},
    {"SCREW", "Screw Attack"}, {"VARIA", "Varia Suit"},     {"GRAVITY", "Gravity Suit"},
    {"SPACE_JUMP", "Space Jump"}, {"PLASMA_BEAM", "Plasma Beam"}};

/* _prettify(): "SKREE_BLUE" as "Skree Blue". */
static void prettify(const char *identifier, char *out, size_t size) {
    size_t used = 0;
    bool first = true;
    for (const char *c = identifier; *c && used + 1 < size; ++c) {
        if (*c == '_') {
            out[used++] = ' ';
            first = true;
            continue;
        }
        out[used++] = first ? (char)toupper((unsigned char)*c) : (char)tolower((unsigned char)*c);
        first = false;
    }
    out[used] = '\0';
}

static const FusionJson *feature_by_id(const FusionJson *annotations, const char *id) {
    for (size_t game = 0; game < annotations->count; ++game) {
        const FusionJson *list = &annotations->items[game];
        for (size_t index = 0; index < list->count; ++index)
            if (!strcmp(text_of(&list->items[index], "id"), id)) return &list->items[index];
    }
    return NULL;
}

static FusionJson bestiary_row(FusionJson id, const char *name, const char *annotation,
                               const char *fallback, const FusionJson *annotations) {
    const FusionJson *feature = annotation ? feature_by_id(annotations, annotation) : NULL;
    FusionJson row = fusion_json_object();
    put(&row, "id", id);
    put_string(&row, "name", name);
    put_string(&row, "status", feature ? dashboard_status(text_of(feature, "status")) : "not_started");
    put_string(&row, "note", feature ? text_of(feature, "title") : fallback);
    put(&row, "evidence",
        feature ? evidence(copy_of(fusion_json_member(feature, "tests")),
                           copy_of(fusion_json_member(feature, "in_game_check")),
                           !strcmp(text_of(feature, "emulator_compared"), "yes"),
                           !strcmp(text_of(feature, "played_through"), "yes"))
                : evidence(fusion_json_array(), fusion_json_array(), false, false));
    if (annotation) put_string(&row, "feature", annotation);
    return row;
}

/* _mzm_sprite_annotations(): each MZM_PSPRITE of mzm_sprite_family's primary
 * switch mapped to its family's parity annotation. */
static bool sprite_annotations_of(const char *source, const FusionJson *annotations,
                                  FusionJson *mapping) {
    char *engine = strdup(source);
    if (!engine) oom();
    *mapping = fusion_json_object();
    char *body = strstr(engine, "int mzm_sprite_family(");
    char *end = body ? strstr(body, "default:") : NULL;
    if (!body || !end) {
        free(engine);
        return fail("mzm_sprite_family switch not found");
    }
    *end = '\0';
    bool ok = true;
    for (char *at = strstr(body, "return MZM_FAMILY_"); ok && at; at = strstr(at + 1, "return MZM_FAMILY_")) {
        char *family = at + 7, *family_end = family;
        while (isupper((unsigned char)*family_end) || isdigit((unsigned char)*family_end) ||
               *family_end == '_')
            ++family_end;
        const char *after = family_end;
        if (*after != ';') continue;
        /* The run of "case MZM_PSPRITE_X:" before it, whitespace apart. */
        char *cursor = at;
        FusionJson sprites = fusion_json_array();
        for (;;) {
            char *scan = cursor;
            while (scan > body && isspace((unsigned char)scan[-1])) --scan;
            if (scan <= body || scan[-1] != ':') break;
            char *name_end = scan - 1, *name = name_end;
            while (name > body && (isupper((unsigned char)name[-1]) ||
                                   isdigit((unsigned char)name[-1]) || name[-1] == '_'))
                --name;
            /* "case MZM_PSPRITE_X:" with exactly one space after "case". */
            if (name - body < 5 || strncmp(name - 5, "case ", 5) ||
                strncmp(name, "MZM_PSPRITE_", 12) || name + 12 >= name_end)
                break;
            char *item = strndup(name + 12, (size_t)(name_end - name - 12));
            if (!item) oom();
            push(&sprites, fusion_json_string(item));
            free(item);
            cursor = name - 5;
        }
        if (!sprites.count) {
            fusion_json_free(&sprites);
            continue;
        }
        char family_name[128], annotation[192];
        snprintf(family_name, sizeof family_name, "%.*s", (int)(family_end - family), family);
        const char *known_annotation = NULL;
        for (size_t index = 0; index < COUNT(mzm_family_annotations); ++index)
            if (!strcmp(mzm_family_annotations[index][0], family_name))
                known_annotation = mzm_family_annotations[index][1];
        if (known_annotation) {
            snprintf(annotation, sizeof annotation, "%s", known_annotation);
        } else {
            char lower[128];
            snprintf(lower, sizeof lower, "%s", family_name + strlen("MZM_FAMILY_"));
            for (char *c = lower; *c; ++c) *c = (char)tolower((unsigned char)*c);
            snprintf(annotation, sizeof annotation, "mzm.enemies.%s", lower);
        }
        if (!feature_by_id(annotations, annotation)) {
            ok = fail("%s: unknown parity annotation %s", family_name, annotation);
        }
        /* The sprites were collected backwards: restore source order. */
        for (size_t index = sprites.count; ok && index > 0; --index)
            put_string(mapping, sprites.items[index - 1].string, annotation);
        fusion_json_free(&sprites);
    }
    free(engine);
    if (!ok) fusion_json_free(mapping);
    return ok;
}

static bool sprite_annotations(const FusionJson *annotations, FusionJson *mapping) {
    char *engine = read_file("src/runtime/mzm_sprites.c", NULL);
    if (!engine) return fail("cannot read src/runtime/mzm_sprites.c");
    bool ok = sprite_annotations_of(engine, annotations, mapping);
    free(engine);
    return ok;
}

static int64_t record_id(const FusionJson *record) {
    return fusion_json_member(record, "native_id")->integer;
}

static bool bestiary(const FusionJson *annotations, FusionJson *result) {
    FusionJson mapping, records;
    if (!sprite_annotations(annotations, &mapping)) return false;
    FebError error = {0};
    if (!feb_object_records(root, "zero_mission", &records, &error)) {
        fusion_json_free(&mapping);
        return fail("%s", error.message);
    }
    /* list-assets is display-sorted; the bestiary keeps the native enum order. */
    for (size_t index = 1; index < records.count; ++index) {
        FusionJson item = records.items[index];
        size_t position = index;
        while (position > 0 && record_id(&records.items[position - 1]) > record_id(&item)) {
            records.items[position] = records.items[position - 1];
            --position;
        }
        records.items[position] = item;
    }
    FusionJson monsters = fusion_json_array(), items = fusion_json_array();
    for (size_t index = 0; index < records.count; ++index) {
        const FusionJson *record = &records.items[index];
        const char *type = text_of(record, "native_type");
        const char *symbol = !strncmp(type, "PSPRITE_", 8) ? type + 8 : type;
        const char *category = text_of(record, "category");
        const FusionJson *annotation = fusion_json_member(&mapping, symbol);
        const char *mapped = annotation ? annotation->string : NULL;
        bool drop = known(symbol, mzm_drops, COUNT(mzm_drops));
        FusionJson id = fusion_json_int(record_id(record));
        if (!strcmp(category, "Enemy / actor") || !strcmp(symbol, "GEKITAI_MACHINE")) {
            push(&monsters, bestiary_row(id, text_of(record, "name"), mapped, "", annotations));
        } else if (!strcmp(category, "Item / pickup") || drop) {
            push(&items, bestiary_row(id, text_of(record, "name"),
                                      drop ? "mzm.items.enemy_drops" : mapped, "", annotations));
        } else if (!strncmp(symbol, "CHOZO_STATUE_", 13) &&
                   (strlen(symbol) < 5 || strcmp(symbol + strlen(symbol) - 5, "_HINT"))) {
            const char *key = symbol + 13, *item_name = NULL;
            for (size_t statue = 0; statue < COUNT(statue_items); ++statue)
                if (!strcmp(statue_items[statue][0], key)) item_name = statue_items[statue][1];
            char pretty[128], name[192];
            prettify(key, pretty, sizeof pretty);
            snprintf(name, sizeof name, "%s (Chozo statue)", item_name ? item_name : pretty);
            FusionJson row = bestiary_row(id, name, mapped, "", annotations);
            if (mapped) {
                char note[512];
                snprintf(note, sizeof note, "%s; the hint map screen is not ported",
                         text_of(&row, "note"));
                put_string(&row, "note", note);
            }
            push(&items, row);
        } else {
            fusion_json_free(&id);
        }
    }
    fusion_json_free(&records);
    fusion_json_free(&mapping);
    char *header = read_file("src/runtime/aos_enemy.h", NULL);
    if (!header) return fail("cannot read src/runtime/aos_enemy.h");
    FusionJson aos_monsters = fusion_json_array();
    bool ported[0x71] = {false};
    bool ok = true;
    /* AOS_ENEMY_([A-Z0-9_]+)\s*=\s*0x([0-9A-Fa-f]+) */
    for (char *at = strstr(header, "AOS_ENEMY_"); ok && at; at = strstr(at + 1, "AOS_ENEMY_")) {
        char *name = at + 10, *name_end = name;
        while (isupper((unsigned char)*name_end) || isdigit((unsigned char)*name_end) || *name_end == '_')
            ++name_end;
        if (name_end == name) continue;
        char *c = name_end;
        while (isspace((unsigned char)*c)) ++c;
        if (*c != '=') continue;
        ++c;
        while (isspace((unsigned char)*c)) ++c;
        if (c[0] != '0' || c[1] != 'x' || !isxdigit((unsigned char)c[2])) continue;
        long address = strtol(c + 2, NULL, 16);
        char identifier[128], annotation[192], pretty[128];
        snprintf(identifier, sizeof identifier, "%.*s", (int)(name_end - name), name);
        snprintf(annotation, sizeof annotation, "aos.enemies.%s", identifier);
        for (char *letter = annotation; *letter; ++letter) *letter = (char)tolower((unsigned char)*letter);
        if (!feature_by_id(annotations, annotation)) {
            ok = fail("AOS_ENEMY_%s: unknown parity annotation %s", identifier, annotation);
            break;
        }
        prettify(identifier, pretty, sizeof pretty);
        push(&aos_monsters, bestiary_row(fusion_json_int(address), pretty, annotation, "", annotations));
        if (address >= 0 && address < 0x71) ported[address] = true;
        at = c;
    }
    free(header);
    if (!ok) {
        fusion_json_free(&monsters);
        fusion_json_free(&items);
        fusion_json_free(&aos_monsters);
        return false;
    }
    for (int enemy = 0; enemy < 0x71; ++enemy) {
        if (ported[enemy]) continue;
        char name[32];
        snprintf(name, sizeof name, "Enemy 0x%02X", enemy);
        push(&aos_monsters, bestiary_row(fusion_json_int(enemy), name, NULL,
                                         "not identified without the ROM constructor table",
                                         annotations));
    }
    for (size_t index = 1; index < aos_monsters.count; ++index) {
        FusionJson item = aos_monsters.items[index];
        size_t position = index;
        while (position > 0 && fusion_json_member(&aos_monsters.items[position - 1], "id")->integer >
                                   fusion_json_member(&item, "id")->integer) {
            aos_monsters.items[position] = aos_monsters.items[position - 1];
            --position;
        }
        aos_monsters.items[position] = item;
    }
    FusionJson aos_items = fusion_json_array();
    for (size_t index = 0; index < COUNT(aria_item_families); ++index)
        push(&aos_items, bestiary_row(fusion_json_string(aria_item_families[index][0]),
                                      aria_item_families[index][1], NULL,
                                      "Documented pickup family; no runtime pickup behavior is ported",
                                      annotations));
    /* Tanks are Clipdata (BgClipCheckTouchingTransitionOrTank), not sprites. */
    static const char *const tanks[][2] = {{"tank_energy", "Energy Tank"},
                                           {"tank_missile", "Missile Tank"},
                                           {"tank_super_missile", "Super Missile Tank"},
                                           {"tank_power_bomb", "Power Bomb Tank"}};
    for (size_t index = 0; index < 4; ++index)
        push(&items, bestiary_row(fusion_json_string(tanks[index][0]), tanks[index][1],
                                  "mzm.items.tanks", "", annotations));
    FusionJson mzm = fusion_json_object(), aos = fusion_json_object(), games = fusion_json_object();
    put_string(&mzm, "title", "Metroid: Zero Mission");
    put(&mzm, "monsters", monsters);
    put(&mzm, "items", items);
    put_string(&mzm, "items_note",
               "Collectible Zero Mission items are the drop sprites, the item sprites of the "
               "native enum, the ability items of the Chozo statues and the four tank kinds "
               "(Clipdata, not sprites).");
    put_string(&aos, "title", "Castlevania: Aria of Sorrow");
    put(&aos, "monsters", aos_monsters);
    put(&aos, "items", aos_items);
    put(&games, "mzm", mzm);
    put(&games, "aos", aos);
    *result = fusion_json_object();
    put_string(result, "method",
               "A row's status is its parity annotation's: 'partial' means a ported AI, "
               "'functional' adds unit tests and a recorded in-game check, 'validated' requires "
               "an emulator comparison; sprites without a ported AI are not started. Names come "
               "from the pinned sources only; unidentified Aria enemies keep their native id.");
    put(result, "games", games);
    return true;
}

/* ---- build() ---------------------------------------------------------- */

static int compare_features(const void *left, const void *right) {
    const FusionJson *a = left, *b = right;
    int order = area_index(text_of(a, "area")) - area_index(text_of(b, "area"));
    return order ? order : strcmp(text_of(a, "id"), text_of(b, "id"));
}

static bool build(bool run_tests, const char *build_dir, time_t now, FusionJson *data) {
    FusionJson annotations, features = fusion_json_array();
    if (!load_annotations(&annotations)) return false;
    native_features(&annotations, &features);
    char registry[8192];
    snprintf(registry, sizeof registry, "%s/data/dashboard/features.tsv", root);
    if (!load_registry(registry, &features)) {
        fusion_json_free(&annotations);
        fusion_json_free(&features);
        return false;
    }
    for (size_t index = 0; index < features.count; ++index) {
        const FusionJson *feature = &features.items[index];
        if (strcmp(text_of(feature, "source"), "data/dashboard/features.tsv")) continue;
        const FusionJson *dependencies = fusion_json_member(feature, "dependencies");
        char unknown[2048] = "";
        size_t used = 0, missing = 0;
        for (size_t other = 0; other < dependencies->count; ++other) {
            const char *dependency = dependencies->items[other].string;
            bool found = false;
            for (size_t item = 0; item < features.count && !found; ++item)
                found = !strcmp(text_of(&features.items[item], "id"), dependency);
            if (!found)
                used += (size_t)snprintf(unknown + used, sizeof unknown - used, "%s'%s'",
                                         missing++ ? ", " : "", dependency);
        }
        if (missing) {
            fail("%s: unknown dependencies [%s]", text_of(feature, "id"), unknown);
            fusion_json_free(&annotations);
            fusion_json_free(&features);
            return false;
        }
    }
    FusionJson ctest = cmake_tests();
    FusionJson ctest_results = fusion_json_null();
    bool have_ctest = false;
    if (run_tests) {
        char testfile[8192];
        struct stat status;
        snprintf(testfile, sizeof testfile, "%s/CTestTestfile.cmake", build_dir);
        if (!stat(testfile, &status)) {
            ctest_results = run_ctest(build_dir);
            have_ctest = true;
        }
    }
    struct tm utc;
    gmtime_r(&now, &utc);
    char date[16], stamp[40];
    strftime(date, sizeof date, "%Y-%m-%d", &utc);
    strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%S+00:00", &utc);
    for (size_t index = 0; index < features.count; ++index) {
        FusionJson *feature = &features.items[index];
        FusionJson verification = verify(feature, &ctest, have_ctest ? &ctest_results : NULL);
        bool passing = !strcmp(text_of(&verification, "verdict"), "passing");
        put(&verification, "verified_on", passing ? fusion_json_string(date) : fusion_json_null());
        put(feature, "verification", verification);
    }
    /* A stable sort by (area order, id). */
    for (size_t index = 1; index < features.count; ++index) {
        FusionJson item = features.items[index];
        size_t position = index;
        while (position > 0 && compare_features(&features.items[position - 1], &item) > 0) {
            features.items[position] = features.items[position - 1];
            --position;
        }
        features.items[position] = item;
    }
    FusionJson indicator_list = fusion_json_array();
    for (size_t index = 0; index < COUNT(indicators); ++index) {
        const char *area = indicators[index][2];
        FusionJson entry = fusion_json_object(), area_list = fusion_json_array(),
                   ids = fusion_json_array();
        put_string(&entry, "id", indicators[index][0]);
        put_string(&entry, "title", indicators[index][1]);
        if (area) {
            push(&area_list, fusion_json_string(area));
        } else {
            for (size_t other = 0; other < COUNT(areas); ++other)
                push(&area_list, fusion_json_string(areas[other][0]));
        }
        put(&entry, "areas", area_list);
        indicator(&features, area, &entry);
        for (size_t other = 0; other < features.count; ++other)
            if (!area || !strcmp(text_of(&features.items[other], "area"), area))
                push(&ids, fusion_json_string(text_of(&features.items[other], "id")));
        put(&entry, "features", ids);
        push(&indicator_list, entry);
    }
    FusionJson tests = fusion_json_object(), ctest_list = fusion_json_array();
    put_bool(&tests, "ran", run_tests);
    put_string(&tests, "host", "linux");
    int64_t passed = 0, failed = 0, other_count = 0;
    for (size_t index = 0; index < ctest.count; ++index) {
        FusionJson item = copy_of(&ctest.items[index]);
        FusionJson outcome = fusion_json_null();
        if (have_ctest) {
            const FusionJson *state = fusion_json_member(&ctest_results, ctest.keys[index]);
            const char *value = state ? state->string : "not run";
            outcome = fusion_json_string(value);
            if (!strcmp(value, "passed")) ++passed;
            else if (!strcmp(value, "failed")) ++failed;
            else ++other_count;
        }
        put(&item, "outcome", outcome);
        push(&ctest_list, item);
    }
    put(&tests, "ctest", ctest_list);
    if (have_ctest) {
        FusionJson summary = fusion_json_object();
        put_int(&summary, "passed", passed);
        put_int(&summary, "failed", failed);
        put_int(&summary, "other", other_count);
        put(&tests, "summary", summary);
    }
    FusionJson beasts;
    if (!bestiary(&annotations, &beasts)) {
        fusion_json_free(&annotations);
        fusion_json_free(&features);
        fusion_json_free(&indicator_list);
        fusion_json_free(&tests);
        fusion_json_free(&ctest);
        fusion_json_free(&ctest_results);
        return false;
    }
    *data = fusion_json_object();
    put_string(data, "schema", schema);
    put_string(data, "generated_at", stamp);
    FusionJson git_state = fusion_json_object();
    char *commit = git("rev-parse --short HEAD"), *branch = git("rev-parse --abbrev-ref HEAD"),
         *dirty = git("status --porcelain --untracked-files=no");
    put_string(&git_state, "commit", commit);
    put_string(&git_state, "branch", branch);
    put_bool(&git_state, "dirty", dirty[0] != '\0');
    free(commit);
    free(branch);
    free(dirty);
    put(data, "git", git_state);
    FusionJson area_list = fusion_json_array(), status_list = fusion_json_array();
    for (size_t index = 0; index < COUNT(areas); ++index) {
        FusionJson entry = fusion_json_object();
        put_string(&entry, "id", areas[index][0]);
        put_string(&entry, "title", areas[index][1]);
        push(&area_list, entry);
    }
    for (size_t index = 0; index < COUNT(statuses); ++index) {
        FusionJson entry = fusion_json_object();
        put_string(&entry, "id", statuses[index][0]);
        put_string(&entry, "label", statuses[index][1]);
        put_string(&entry, "description", statuses[index][2]);
        push(&status_list, entry);
    }
    put(data, "areas", area_list);
    put(data, "statuses", status_list);
    put_string(data, "method",
               "Percentages are shares of registered features: 'functional or better' counts the "
               "functional, faithful and validated statuses declared in the registries; 'passing "
               "tests' counts features whose declared tests all passed when this data was "
               "generated. Unregistered work is not counted.");
    put(data, "indicators", indicator_list);
    put(data, "features", features);
    put(data, "tests", tests);
    put(data, "timeline", timeline());
    put(data, "inventory", inventory_summary(&annotations));
    put(data, "bestiary", beasts);
    put(data, "rebuild_tasks", rebuild_tasks());
    fusion_json_free(&annotations);
    fusion_json_free(&ctest);
    fusion_json_free(&ctest_results);
    return true;
}

/* ---- Writing and checking --------------------------------------------- */

static const char *const declared_fields[] = {
    "id", "area", "category", "title", "status", "fidelity", "local_sources", "tests",
    "dependencies", "native_routines", "limitations", "next_action", "last_validated",
    "source", "evidence"};

/* declared_view(): the registry- and file-derived parts of the data. */
static FusionJson declared_view(const FusionJson *data) {
    FusionJson features = fusion_json_object(), indicator_views = fusion_json_object();
    const FusionJson *list = fusion_json_member(data, "features");
    for (size_t index = 0; list && index < list->count; ++index) {
        const FusionJson *item = &list->items[index];
        FusionJson view = fusion_json_object();
        for (size_t field = 0; field < COUNT(declared_fields); ++field)
            put(&view, declared_fields[field], copy_of(fusion_json_member(item, declared_fields[field])));
        const FusionJson *verification = fusion_json_member(item, "verification");
        const FusionJson *missing = verification ? fusion_json_member(verification, "missing_sources") : NULL;
        put(&view, "missing_sources", missing ? copy_of(missing) : fusion_json_array());
        FusionJson registration = fusion_json_array();
        const FusionJson *runs = verification ? fusion_json_member(verification, "tests") : NULL;
        for (size_t run = 0; runs && run < runs->count; ++run) {
            FusionJson pair = fusion_json_array();
            push(&pair, copy_of(fusion_json_member(&runs->items[run], "test")));
            push(&pair, copy_of(fusion_json_member(&runs->items[run], "registered")));
            push(&registration, pair);
        }
        put(&view, "test_registration", registration);
        put(&features, text_of(item, "id"), view);
    }
    const FusionJson *indicator_list = fusion_json_member(data, "indicators");
    for (size_t index = 0; indicator_list && index < indicator_list->count; ++index) {
        const FusionJson *item = &indicator_list->items[index];
        FusionJson view = fusion_json_object();
        put(&view, "total", copy_of(fusion_json_member(item, "total")));
        put(&view, "by_status", copy_of(fusion_json_member(item, "by_status")));
        put(&view, "functional_or_better", copy_of(fusion_json_member(item, "functional_or_better")));
        put(&indicator_views, text_of(item, "id"), view);
    }
    FusionJson view = fusion_json_object();
    put(&view, "features", features);
    put(&view, "indicators", indicator_views);
    put(&view, "bestiary", copy_of(fusion_json_member(data, "bestiary")));
    return view;
}

/* Object equality regardless of member order, as Python's dict ==. */
static bool same(const FusionJson *left, const FusionJson *right) {
    if (!left || !right) return left == right;
    if (left->kind != right->kind) return false;
    if (left->kind == FUSION_JSON_OBJECT) {
        if (left->count != right->count) return false;
        for (size_t index = 0; index < left->count; ++index)
            if (!same(&left->items[index], fusion_json_member(right, left->keys[index]))) return false;
        return true;
    }
    if (left->kind == FUSION_JSON_ARRAY) {
        if (left->count != right->count) return false;
        for (size_t index = 0; index < left->count; ++index)
            if (!same(&left->items[index], &right->items[index])) return false;
        return true;
    }
    return fusion_json_equal(left, right);
}

/* check(): the problems that make the committed data stale, in ``problems``;
 * false when the data cannot be read or rebuilt. */
static bool check_problems(const char *output_dir, FusionJson *problems) {
    char json_path[8300], js_path[8300];
    snprintf(json_path, sizeof json_path, "%s/data.json", output_dir);
    snprintf(js_path, sizeof js_path, "%s/data.js", output_dir);
    FILE *file = fopen(json_path, "rb");
    char *json_text = NULL, *script = NULL;
    size_t size = 0;
    if (file) {
        fseek(file, 0, SEEK_END);
        size = (size_t)ftell(file);
        rewind(file);
        json_text = malloc(size + 1);
        if (!json_text) oom();
        size = fread(json_text, 1, size, file);
        json_text[size] = '\0';
        fclose(file);
    }
    file = fopen(js_path, "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        size_t length = (size_t)ftell(file);
        rewind(file);
        script = malloc(length + 1);
        if (!script) oom();
        length = fread(script, 1, length, file);
        script[length] = '\0';
        fclose(file);
    }
    FusionJson committed;
    char error[256];
    *problems = fusion_json_array();
    if (!json_text || !script || !fusion_json_parse(json_text, size, &committed, error, sizeof error)) {
        push(problems, fusion_json_string("cannot read the dashboard data"));
        free(json_text);
        free(script);
        return true;
    }
    /* data.js carries data.json: its second line onwards is the assignment. */
    const char *prefix = "window.METROIDVANIA_DASHBOARD = ";
    const char *body = strchr(script, '\n');
    body = body ? body + 1 : "";
    char *expected = NULL;
    if (!fusion_json_write(&committed, 1, &expected, NULL)) oom();
    size_t body_length = strlen(body);
    while (body_length && isspace((unsigned char)body[body_length - 1])) --body_length;
    while (body_length && body[body_length - 1] == ';') --body_length;
    bool carried = !strncmp(body, prefix, strlen(prefix)) &&
                   body_length - strlen(prefix) == strlen(expected) &&
                   !strncmp(body + strlen(prefix), expected, strlen(expected));
    if (!carried) push(problems, fusion_json_string("data.js does not carry data.json"));
    free(expected);
    FusionJson fresh;
    if (!build(false, "", time(NULL), &fresh)) {
        fusion_json_free(&committed);
        free(json_text);
        free(script);
        return false;
    }
    FusionJson old_view = declared_view(&committed), new_view = declared_view(&fresh);
    const FusionJson *old_features = fusion_json_member(&old_view, "features");
    const FusionJson *new_features = fusion_json_member(&new_view, "features");
    /* sorted(set(old) | set(new)) */
    size_t total = old_features->count + new_features->count;
    char **ids = malloc((total + 1) * sizeof *ids);
    size_t id_count = 0;
    for (size_t index = 0; index < old_features->count; ++index) ids[id_count++] = old_features->keys[index];
    for (size_t index = 0; index < new_features->count; ++index)
        if (!fusion_json_member(old_features, new_features->keys[index]))
            ids[id_count++] = new_features->keys[index];
    if (id_count) qsort(ids, id_count, sizeof *ids, compare_strings);
    for (size_t index = 0; index < id_count; ++index) {
        const FusionJson *before = fusion_json_member(old_features, ids[index]);
        const FusionJson *after = fusion_json_member(new_features, ids[index]);
        char message[4096];
        if (!before) {
            snprintf(message, sizeof message, "%s: missing from the dashboard data", ids[index]);
        } else if (!after) {
            snprintf(message, sizeof message, "%s: no longer registered", ids[index]);
        } else if (!same(before, after)) {
            char *keys[64];
            size_t key_count = 0;
            for (size_t key = 0; key < after->count && key_count < 64; ++key)
                if (!same(fusion_json_member(before, after->keys[key]), &after->items[key]))
                    keys[key_count++] = after->keys[key];
            qsort(keys, key_count, sizeof *keys, compare_strings);
            size_t used = (size_t)snprintf(message, sizeof message, "%s: stale ", ids[index]);
            for (size_t key = 0; key < key_count; ++key)
                used += (size_t)snprintf(message + used, sizeof message - used, "%s%s",
                                         key ? ", " : "", keys[key]);
        } else {
            continue;
        }
        push(problems, fusion_json_string(message));
    }
    free(ids);
    const FusionJson *old_indicators = fusion_json_member(&old_view, "indicators");
    const FusionJson *new_indicators = fusion_json_member(&new_view, "indicators");
    for (size_t index = 0; index < new_indicators->count; ++index)
        if (!same(fusion_json_member(old_indicators, new_indicators->keys[index]),
                  &new_indicators->items[index])) {
            char message[256];
            snprintf(message, sizeof message, "indicator %s: stale status counts",
                     new_indicators->keys[index]);
            push(problems, fusion_json_string(message));
        }
    if (!same(fusion_json_member(&old_view, "bestiary"), fusion_json_member(&new_view, "bestiary")))
        push(problems, fusion_json_string("bestiary: stale monsters/items data"));
    fusion_json_free(&old_view);
    fusion_json_free(&new_view);
    fusion_json_free(&fresh);
    fusion_json_free(&committed);
    free(json_text);
    free(script);
    return true;
}

#ifndef FUSION_DASHBOARD_NO_MAIN
static int check(const char *output_dir) {
    FusionJson problems;
    if (!check_problems(output_dir, &problems)) {
        fprintf(stderr, "fusion_dashboard: error: %s\n", failure);
        return 2;
    }
    for (size_t index = 0; index < problems.count; ++index)
        fprintf(stderr, "Stale dashboard data: %s\n", problems.items[index].string);
    int result = 0;
    if (problems.count) {
        fputs("Regenerate with: ./build/fusion_dashboard --run-tests\n", stderr);
        result = 1;
    } else {
        char json_path[8300];
        snprintf(json_path, sizeof json_path, "%s/data.json", output_dir);
        printf("Dashboard data checked: %s\n",
               !strncmp(json_path, root, strlen(root)) && json_path[strlen(root)] == '/'
                   ? json_path + strlen(root) + 1 : json_path);
    }
    fusion_json_free(&problems);
    return result;
}
#endif

static bool write_data(const FusionJson *data, const char *output_dir) {
    char *text = NULL;
    if (!fusion_json_write(data, 1, &text, NULL)) oom();
    if (strstr(text, "assets/extracted")) {
        free(text);
        return fail("dashboard data would contain 'assets/extracted'");
    }
    if (strstr(text, root)) {
        free(text);
        return fail("dashboard data would contain '%s'", root);
    }
    mkdir(output_dir, 0777);
    char path[8300];
    snprintf(path, sizeof path, "%s/data.json", output_dir);
    FILE *file = fopen(path, "w");
    if (!file) {
        free(text);
        return fail("cannot write %s", path);
    }
    fprintf(file, "%s\n", text);
    fclose(file);
    snprintf(path, sizeof path, "%s/data.js", output_dir);
    file = fopen(path, "w");
    if (!file) {
        free(text);
        return fail("cannot write %s", path);
    }
    fprintf(file, "// Generated by fusion_dashboard; do not edit.\n"
                  "window.METROIDVANIA_DASHBOARD = %s;\n", text);
    fclose(file);
    free(text);
    return true;
}

#ifndef FUSION_DASHBOARD_NO_MAIN
static const char *relative_to_root(const char *path) {
    size_t length = strlen(root);
    return !strncmp(path, root, length) && path[length] == '/' ? path + length + 1 : path;
}
#endif

#ifndef FUSION_DASHBOARD_NO_MAIN
int main(int argc, char **argv) {
    bool run_tests = false, check_only = false;
    char build_dir[8192], output_dir[8192];
    snprintf(build_dir, sizeof build_dir, "%s/build", root);
    snprintf(output_dir, sizeof output_dir, "%s/dashboard", root);
    time_t now = time(NULL);
    const char *usage = "usage: fusion_dashboard [-h] [--run-tests] [--build BUILD] [--output OUTPUT] "
                        "[--check]\n";
    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];
        if (!strcmp(argument, "-h") || !strcmp(argument, "--help")) {
            fputs(usage, stdout);
            puts("\nGenerate the data of the development dashboard (dashboard/).");
            return 0;
        } else if (!strcmp(argument, "--run-tests")) {
            run_tests = true;
        } else if (!strcmp(argument, "--check")) {
            check_only = true;
        } else if ((!strcmp(argument, "--build") || !strcmp(argument, "--output") ||
                    !strcmp(argument, "--now")) && index + 1 < argc) {
            const char *value = argv[++index];
            if (!strcmp(argument, "--now")) now = (time_t)strtoll(value, NULL, 10);
            else if (value[0] == '/')
                snprintf(!strcmp(argument, "--build") ? build_dir : output_dir, 8192, "%s", value);
            else
                snprintf(!strcmp(argument, "--build") ? build_dir : output_dir, 8192, "%s/%s", root,
                         value);
        } else {
            fprintf(stderr, "%sfusion_dashboard: error: unrecognized arguments: %s\n", usage, argument);
            return 2;
        }
    }
    if (check_only) {
        if (run_tests) {
            fprintf(stderr, "%sfusion_dashboard: error: --check does not run tests\n", usage);
            return 2;
        }
        return check(output_dir);
    }
    FusionJson data;
    if (!build(run_tests, build_dir, now, &data) || !write_data(&data, output_dir)) {
        fprintf(stderr, "%sfusion_dashboard: error: %s\n", usage, failure);
        return 2;
    }
    const FusionJson *overall = &fusion_json_member(&data, "indicators")->items[0];
    printf("Dashboard data: %lld features, %lld functional or better, %lld with passing tests\n",
           (long long)fusion_json_member(overall, "total")->integer,
           (long long)fusion_json_member(overall, "functional_or_better")->integer,
           (long long)fusion_json_member(overall, "with_passing_tests")->integer);
    char path[8300];
    snprintf(path, sizeof path, "%s/data.json", output_dir);
    printf("Wrote %s\n", relative_to_root(path));
    snprintf(path, sizeof path, "%s/data.js", output_dir);
    printf("Wrote %s\n", relative_to_root(path));
    int result = 0;
    const FusionJson *tests = fusion_json_member(&data, "tests");
    if (fusion_json_member(tests, "ran")->boolean) {
        const FusionJson *summary = fusion_json_member(tests, "summary");
        printf("Tests: %lld passed, %lld failed, %lld other\n",
               summary ? (long long)fusion_json_member(summary, "passed")->integer : 0LL,
               summary ? (long long)fusion_json_member(summary, "failed")->integer : 0LL,
               summary ? (long long)fusion_json_member(summary, "other")->integer : 0LL);
        fflush(stdout);
        const FusionJson *ctest = fusion_json_member(tests, "ctest");
        for (size_t index = 0; index < ctest->count; ++index)
            if (!strcmp(text_of(&ctest->items[index], "outcome"), "failed")) {
                fprintf(stderr, "Failed test: %s\n", text_of(&ctest->items[index], "name"));
                result = 1;
            }
    }
    fusion_json_free(&data);
    return result;
}
#endif
