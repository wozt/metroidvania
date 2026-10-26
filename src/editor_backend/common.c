/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "common.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *const feb_world_ids[FEB_WORLD_COUNT] = {"zero_mission", "aria"};

static const char *const zero_mission_areas[] = {
    "Brinstar", "Kraid", "Norfair", "Ridley", "Tourian", "Crateria", "Chozodia"};
static const char *const aria_areas[] = {
    "Castle Corridor", "Chapel", "Study", "Dance Hall", "Inner Quarters",
    "Floating Garden", "Clock Tower", "Underground", "The Arena", "Top Floor",
    "Chaotic Realm entrance", "Chaotic Realm boss"};

const char *const *feb_areas(const char *world, size_t *count) {
    if (world && !strcmp(world, "zero_mission")) {
        *count = sizeof zero_mission_areas / sizeof zero_mission_areas[0];
        return zero_mission_areas;
    }
    if (world && !strcmp(world, "aria")) {
        *count = sizeof aria_areas / sizeof aria_areas[0];
        return aria_areas;
    }
    *count = 0;
    return NULL;
}

bool feb_fail(FebError *error, int code, const char *format, ...) {
    if (!error) return false;
    error->code = code;
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error->message, sizeof error->message, format, arguments);
    va_end(arguments);
    return false;
}

bool feb_oom(FebError *error) {
    return feb_fail(error, FEB_EXIT_OPERATION, "out of memory");
}

const FusionJson *feb_option(const FusionJson *options, const char *key) {
    return fusion_json_member(options, key);
}

static bool decode_utf8(const unsigned char **cursor, uint32_t *code) {
    const unsigned char *c = *cursor;
    if (c[0] < 0x80) {
        *code = c[0];
        *cursor += 1;
    } else if ((c[0] & 0xE0) == 0xC0 && c[1]) {
        *code = ((uint32_t)(c[0] & 0x1F) << 6) | (c[1] & 0x3F);
        *cursor += 2;
    } else if ((c[0] & 0xF0) == 0xE0 && c[1] && c[2]) {
        *code = ((uint32_t)(c[0] & 0x0F) << 12) | ((uint32_t)(c[1] & 0x3F) << 6) |
                (c[2] & 0x3F);
        *cursor += 3;
    } else if ((c[0] & 0xF8) == 0xF0 && c[1] && c[2] && c[3]) {
        *code = ((uint32_t)(c[0] & 0x07) << 18) | ((uint32_t)(c[1] & 0x3F) << 12) |
                ((uint32_t)(c[2] & 0x3F) << 6) | (c[3] & 0x3F);
        *cursor += 4;
    } else {
        *code = c[0];
        *cursor += 1;
        return false;
    }
    return true;
}

size_t feb_utf8_length(const char *text) {
    size_t count = 0;
    const unsigned char *cursor = (const unsigned char *)text;
    while (*cursor) {
        uint32_t code;
        decode_utf8(&cursor, &code);
        ++count;
    }
    return count;
}

bool feb_utf8_edges(const char *text, uint32_t *first, uint32_t *last) {
    const unsigned char *cursor = (const unsigned char *)text;
    bool any = false;
    while (*cursor) {
        uint32_t code;
        decode_utf8(&cursor, &code);
        if (!any) *first = code;
        *last = code;
        any = true;
    }
    return any;
}

/* str.isspace(): the Unicode White_Space characters and the separators
 * Python counts as whitespace. */
bool feb_python_space(uint32_t code) {
    return (code >= 0x09 && code <= 0x0D) || (code >= 0x1C && code <= 0x20) ||
           code == 0x85 || code == 0xA0 || code == 0x1680 ||
           (code >= 0x2000 && code <= 0x200A) || code == 0x2028 || code == 0x2029 ||
           code == 0x202F || code == 0x205F || code == 0x3000;
}

/* int(text): Python whitespace around an optional sign and decimal digits
 * with single underscores between them. */
static bool python_int(const char *text, bool *negative, char *digits, size_t capacity) {
    const unsigned char *cursor = (const unsigned char *)text;
    const unsigned char *end = cursor + strlen(text);
    uint32_t code;
    const unsigned char *probe;
    while (cursor < end) {
        probe = cursor;
        decode_utf8(&probe, &code);
        if (!feb_python_space(code)) break;
        cursor = probe;
    }
    while (end > cursor) {
        const unsigned char *start = end - 1;
        while (start > cursor && (*start & 0xC0) == 0x80) --start;
        probe = start;
        decode_utf8(&probe, &code);
        if (!feb_python_space(code)) break;
        end = start;
    }
    *negative = false;
    if (cursor < end && (*cursor == '+' || *cursor == '-')) {
        *negative = *cursor == '-';
        ++cursor;
    }
    size_t count = 0;
    bool previous_digit = false;
    for (; cursor < end; ++cursor) {
        if (*cursor >= '0' && *cursor <= '9') {
            if (count + 1 >= capacity) return false;
            digits[count++] = (char)*cursor;
            previous_digit = true;
        } else if (*cursor == '_' && previous_digit && cursor + 1 < end &&
                   cursor[1] >= '0' && cursor[1] <= '9') {
            previous_digit = false;
        } else {
            return false;
        }
    }
    digits[count] = '\0';
    return count > 0;
}

bool feb_integer(const FusionJson *value, const char *label, int64_t lower,
                 int64_t upper, int64_t *result, FebError *error) {
    if (!value || value->kind == FUSION_JSON_BOOL || value->kind == FUSION_JSON_NULL ||
        value->kind == FUSION_JSON_ARRAY || value->kind == FUSION_JSON_OBJECT)
        return feb_fail(error, FEB_EXIT_OPERATION, "%s must be an integer", label);
    int64_t parsed = 0;
    bool overflow = false;
    if (value->kind == FUSION_JSON_NUMBER) {
        if (!value->integral) {
            /* int(1.5) is 1, whose text differs from the float's. */
            return feb_fail(error, FEB_EXIT_OPERATION, "%s must be a canonical integer",
                            label);
        }
        parsed = value->integer;
    } else {
        char digits[64];
        bool negative = false;
        if (!python_int(value->string, &negative, digits, sizeof digits))
            return feb_fail(error, FEB_EXIT_OPERATION, "%s must be an integer", label);
        const char *significant = digits;
        while (significant[0] == '0' && significant[1]) ++significant;
        char canonical[72];
        bool zero = !strcmp(significant, "0");
        snprintf(canonical, sizeof canonical, "%s%s", negative && !zero ? "-" : "",
                 significant);
        if (strcmp(canonical, value->string))
            return feb_fail(error, FEB_EXIT_OPERATION, "%s must be a canonical integer",
                            label);
        errno = 0;
        long long number = strtoll(canonical, NULL, 10);
        overflow = errno == ERANGE;
        parsed = number;
    }
    if (overflow || parsed < lower || parsed > upper)
        return feb_fail(error, FEB_EXIT_OPERATION, "%s must be in [%lld, %lld]", label,
                        (long long)lower, (long long)upper);
    *result = parsed;
    return true;
}

bool feb_world(const FusionJson *value, const char **world, FebError *error) {
    if (value && value->kind == FUSION_JSON_STRING) {
        for (size_t index = 0; index < FEB_WORLD_COUNT; ++index) {
            if (!strcmp(value->string, feb_world_ids[index])) {
                *world = feb_world_ids[index];
                return true;
            }
        }
    }
    return feb_fail(error, FEB_EXIT_OPERATION, "world must be zero_mission or aria");
}

bool feb_is_symlink(const char *path) {
    struct stat info;
    return lstat(path, &info) == 0 && S_ISLNK(info.st_mode);
}

bool feb_is_regular(const char *path, long long *size) {
    struct stat info;
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) return false;
    if (size) *size = (long long)info.st_size;
    return true;
}

bool feb_is_directory(const char *path) {
    struct stat info;
    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

bool feb_join(char *out, const char *left, const char *right) {
    int written = snprintf(out, FEB_PATH_CAPACITY, "%s/%s", left, right);
    return written > 0 && written < FEB_PATH_CAPACITY;
}

bool feb_absolute(const char *path, char *out) {
    if (path[0] == '/') {
        size_t length = strlen(path);
        if (length >= FEB_PATH_CAPACITY) return false;
        memcpy(out, path, length + 1);
        return true;
    }
    char cwd[FEB_PATH_CAPACITY];
    if (!getcwd(cwd, sizeof cwd)) return false;
    return feb_join(out, cwd, path);
}

bool feb_read_text(const char *path, size_t limit, char **text, size_t *size) {
    *text = NULL;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    char *buffer = malloc(limit + 1);
    size_t read = buffer ? fread(buffer, 1, limit + 1, file) : 0;
    bool ok = buffer && !ferror(file) && read <= limit;
    fclose(file);
    if (!ok) {
        free(buffer);
        return false;
    }
    buffer[read] = '\0';
    *text = buffer;
    if (size) *size = read;
    return true;
}

bool feb_load_json(const char *path, size_t limit, FusionJson *value, char *detail,
                   size_t detail_size) {
    char *text = NULL;
    size_t size = 0;
    if (!feb_read_text(path, limit, &text, &size)) {
        snprintf(detail, detail_size, "cannot read %s", path);
        return false;
    }
    bool ok = fusion_json_parse(text, size, value, detail, detail_size);
    free(text);
    return ok;
}

bool feb_put(FusionJson *object, const char *key, FusionJson value) {
    return fusion_json_set(object, key, value);
}

bool feb_put_string(FusionJson *object, const char *key, const char *value) {
    return fusion_json_set(object, key, value ? fusion_json_string(value) : fusion_json_null());
}

bool feb_put_int(FusionJson *object, const char *key, int64_t value) {
    return fusion_json_set(object, key, fusion_json_int(value));
}

bool feb_put_bool(FusionJson *object, const char *key, bool value) {
    return fusion_json_set(object, key, fusion_json_bool(value));
}
