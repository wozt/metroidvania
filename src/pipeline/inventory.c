/* SPDX-License-Identifier: GPL-3.0-only */
#define _POSIX_C_SOURCE 200809L
#include "fusion/pipeline/inventory.h"

#include "core/json.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} Strings;

typedef struct {
    char *symbol;
    size_t offset;
    size_t line;
} AsmMarker;

typedef struct {
    AsmMarker *items;
    size_t count;
    size_t capacity;
} Markers;

static bool fail(char *error, size_t error_size, const char *format, ...) {
    if (error && error_size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
        error[error_size - 1] = '\0';
    }
    return false;
}

static char *copy_range(const char *text, size_t start, size_t end) {
    if (end < start) return NULL;
    char *copy = malloc(end - start + 1);
    if (!copy) return NULL;
    memcpy(copy, text + start, end - start);
    copy[end - start] = '\0';
    return copy;
}

static char *copy_string(const char *text) {
    return text ? copy_range(text, 0, strlen(text)) : NULL;
}

static bool strings_add(Strings *strings, const char *value) {
    for (size_t index = 0; index < strings->count; ++index)
        if (!strcmp(strings->items[index], value)) return true;
    if (strings->count == strings->capacity) {
        size_t capacity = strings->capacity ? strings->capacity * 2 : 16;
        char **items = realloc(strings->items, capacity * sizeof *items);
        if (!items) return false;
        strings->items = items;
        strings->capacity = capacity;
    }
    char *copy = copy_string(value);
    if (!copy) return false;
    strings->items[strings->count++] = copy;
    return true;
}

static int compare_strings(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

static void strings_sort(Strings *strings) {
    qsort(strings->items, strings->count, sizeof *strings->items, compare_strings);
}

static void strings_free(Strings *strings) {
    for (size_t index = 0; index < strings->count; ++index) free(strings->items[index]);
    free(strings->items);
    memset(strings, 0, sizeof *strings);
}

static bool strings_contains(const Strings *strings, const char *value) {
    for (size_t index = 0; index < strings->count; ++index)
        if (!strcmp(strings->items[index], value)) return true;
    return false;
}

static bool read_file(const char *path, char **data, size_t *size,
                      char *error, size_t error_size) {
    FILE *file = fopen(path, "rb");
    if (!file) return fail(error, error_size, "cannot open %s", path);
    if (fseek(file, 0, SEEK_END)) {
        fclose(file);
        return fail(error, error_size, "cannot seek %s", path);
    }
    long length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return fail(error, error_size, "cannot size %s", path);
    }
    char *bytes = malloc((size_t)length + 1);
    if (!bytes || fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        return fail(error, error_size, "cannot load %s", path);
    }
    fclose(file);
    bytes[length] = '\0';
    *data = bytes;
    if (size) *size = (size_t)length;
    return true;
}

static bool has_suffix(const char *path, const char *suffix) {
    size_t path_length = strlen(path), suffix_length = strlen(suffix);
    return path_length >= suffix_length &&
           !strcmp(path + path_length - suffix_length, suffix);
}

static bool collect_paths(const char *root, const char *relative, Strings *paths,
                          char *error, size_t error_size) {
    char directory[4096];
    int length = relative[0] ? snprintf(directory, sizeof directory, "%s/%s", root, relative)
                             : snprintf(directory, sizeof directory, "%s", root);
    if (length < 0 || (size_t)length >= sizeof directory)
        return fail(error, error_size, "source path is too long");
    DIR *stream = opendir(directory);
    if (!stream) return fail(error, error_size, "cannot open source directory %s", directory);
    struct dirent *entry;
    bool ok = true;
    while (ok && (entry = readdir(stream)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[4096], absolute[4096];
        length = relative[0] ? snprintf(child, sizeof child, "%s/%s", relative, entry->d_name)
                             : snprintf(child, sizeof child, "%s", entry->d_name);
        if (length < 0 || (size_t)length >= sizeof child) {
            ok = fail(error, error_size, "source path is too long");
            break;
        }
        length = snprintf(absolute, sizeof absolute, "%s/%s", root, child);
        if (length < 0 || (size_t)length >= sizeof absolute) {
            ok = fail(error, error_size, "source path is too long");
            break;
        }
        struct stat status;
        if (lstat(absolute, &status)) {
            ok = fail(error, error_size, "cannot inspect %s", absolute);
        } else if (S_ISDIR(status.st_mode)) {
            ok = collect_paths(root, child, paths, error, error_size);
        } else if (S_ISREG(status.st_mode) &&
                   (has_suffix(child, ".c") || has_suffix(child, ".s"))) {
            ok = strings_add(paths, child);
            if (!ok) fail(error, error_size, "out of memory");
        }
    }
    closedir(stream);
    return ok;
}

static char *mask_c(const char *source, size_t size) {
    char *masked = copy_range(source, 0, size);
    if (!masked) return NULL;
    enum { CODE, BLOCK_COMMENT, LINE_COMMENT, STRING, CHARACTER } state = CODE;
    for (size_t index = 0; index < size;) {
        char current = source[index];
        char following = index + 1 < size ? source[index + 1] : '\0';
        if (state == CODE && current == '/' && following == '*') {
            masked[index] = masked[index + 1] = ' ';
            state = BLOCK_COMMENT;
            index += 2;
        } else if (state == CODE && current == '/' && following == '/') {
            masked[index] = masked[index + 1] = ' ';
            state = LINE_COMMENT;
            index += 2;
        } else if (state == CODE && (current == '"' || current == '\'')) {
            masked[index] = ' ';
            state = current == '"' ? STRING : CHARACTER;
            ++index;
        } else if (state == STRING || state == CHARACTER) {
            if (current == '\\') {
                masked[index] = ' ';
                if (index + 1 < size) {
                    if (source[index + 1] != '\n') masked[index + 1] = ' ';
                    index += 2;
                } else ++index;
            } else {
                char expected = state == STRING ? '"' : '\'';
                if (current == expected) state = CODE;
                if (current != '\n') masked[index] = ' ';
                ++index;
            }
        } else if (state == BLOCK_COMMENT) {
            if (current == '*' && following == '/') {
                masked[index] = masked[index + 1] = ' ';
                state = CODE;
                index += 2;
            } else {
                if (current != '\n') masked[index] = ' ';
                ++index;
            }
        } else if (state == LINE_COMMENT) {
            if (current == '\n') state = CODE;
            else masked[index] = ' ';
            ++index;
        } else ++index;
    }
    return masked;
}

static bool identifier_start(char character) {
    return isalpha((unsigned char)character) || character == '_';
}

static bool identifier_character(char character) {
    return isalnum((unsigned char)character) || character == '_';
}

static bool is_word(const char *start, size_t length, const char *word) {
    return strlen(word) == length && !memcmp(start, word, length);
}

static bool control_word(const char *start, size_t length) {
    static const char *const words[] = {"if", "for", "while", "switch", "return", "sizeof"};
    for (size_t index = 0; index < sizeof words / sizeof words[0]; ++index)
        if (is_word(start, length, words[index])) return true;
    return false;
}

static bool contains_word(const char *text, size_t length, const char *word) {
    size_t wanted = strlen(word);
    for (size_t index = 0; index + wanted <= length; ++index) {
        if (memcmp(text + index, word, wanted)) continue;
        if (index && identifier_character(text[index - 1])) continue;
        if (index + wanted < length && identifier_character(text[index + wanted])) continue;
        return true;
    }
    return false;
}

typedef struct {
    size_t name_start;
    size_t name_end;
    bool internal;
} FunctionHeader;

static bool function_header(const char *text, size_t start, size_t end,
                            FunctionHeader *header) {
    while (end > start && isspace((unsigned char)text[end - 1])) --end;
    if (end == start || text[end - 1] != ')') return false;
    size_t cursor = end;
    int depth = 0;
    size_t opening = SIZE_MAX;
    while (cursor > start) {
        --cursor;
        if (text[cursor] == ')') ++depth;
        else if (text[cursor] == '(' && --depth == 0) {
            opening = cursor;
            break;
        }
    }
    if (opening == SIZE_MAX) return false;
    cursor = opening;
    while (cursor > start && isspace((unsigned char)text[cursor - 1])) --cursor;
    size_t name_end = cursor;
    while (cursor > start && identifier_character(text[cursor - 1])) --cursor;
    if (cursor == name_end || !identifier_start(text[cursor])) return false;
    if (control_word(text + cursor, name_end - cursor)) return false;
    for (size_t index = start; index < cursor; ++index)
        if (text[index] == '=') return false;
    if (contains_word(text + start, cursor - start, "typedef")) return false;
    header->name_start = cursor;
    header->name_end = name_end;
    header->internal = contains_word(text + start, cursor - start, "static");
    return true;
}

static size_t line_number(const char *text, size_t position) {
    size_t line = 1;
    for (size_t index = 0; index < position; ++index)
        if (text[index] == '\n') ++line;
    return line;
}

static size_t character_offset(const char *text, size_t byte_offset) {
    size_t characters = 0;
    for (size_t index = 0; index < byte_offset; ++index) {
        if (text[index] == '\r' && index + 1 < byte_offset && text[index + 1] == '\n')
            continue;
        if (((unsigned char)text[index] & 0xC0) != 0x80) ++characters;
    }
    return characters;
}

static char *source_address(const char *symbol) {
    size_t length = strlen(symbol), start = 0;
    const char *underscore = strrchr(symbol, '_');
    if (underscore) start = (size_t)(underscore - symbol) + 1;
    if (length - start == 10 && symbol[start] == '0' && symbol[start + 1] == 'x') start += 2;
    if (length - start != 8 || symbol[start] != '0' || symbol[start + 1] != '8') return NULL;
    for (size_t index = start; index < length; ++index)
        if (!isxdigit((unsigned char)symbol[index])) return NULL;
    char *address = malloc(11);
    if (!address) return NULL;
    address[0] = '0'; address[1] = 'x';
    for (size_t index = 0; index < 8; ++index)
        address[index + 2] = (char)toupper((unsigned char)symbol[start + index]);
    address[10] = '\0';
    return address;
}

static void brief_and_address(const char *source, size_t header_start,
                              char **description, char **address) {
    size_t floor = header_start > 1200 ? header_start - 1200 : 0;
    size_t comment_start = SIZE_MAX;
    for (size_t index = floor; index + 2 < header_start; ++index)
        if (source[index] == '/' && source[index + 1] == '*' && source[index + 2] == '*')
            comment_start = index;
    if (comment_start == SIZE_MAX) return;
    size_t comment_end = SIZE_MAX;
    for (size_t index = comment_start + 3; index + 1 < header_start; ++index)
        if (source[index] == '*' && source[index + 1] == '/') {
            comment_end = index;
            break;
        }
    if (comment_end == SIZE_MAX) return;
    for (size_t index = comment_end + 2; index < header_start; ++index)
        if (source[index] == ';' || source[index] == '}') return;
    const char *brief = strstr(source + comment_start, "@brief");
    if (!brief || brief >= source + comment_end) return;
    const char *cursor = brief + 6;
    while (cursor < source + comment_end && isspace((unsigned char)*cursor)) ++cursor;
    const char *hex_start = cursor;
    while (cursor < source + comment_end && isxdigit((unsigned char)*cursor)) ++cursor;
    if (cursor == hex_start) return;
    char number[32];
    size_t digits = (size_t)(cursor - hex_start);
    if (digits >= sizeof number) return;
    memcpy(number, hex_start, digits); number[digits] = '\0';
    while (cursor < source + comment_end && isspace((unsigned char)*cursor)) ++cursor;
    if (cursor >= source + comment_end || *cursor++ != '|') return;
    while (cursor < source + comment_end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    const char *description_end = cursor;
    while (description_end < source + comment_end && *description_end != '\n' &&
           *description_end != '*') ++description_end;
    while (description_end > cursor && isspace((unsigned char)description_end[-1]))
        --description_end;
    if (description_end > cursor)
        *description = copy_range(cursor, 0, (size_t)(description_end - cursor));
    uint32_t value = (uint32_t)strtoul(number, NULL, 16);
    if (value < UINT32_C(0x08000000)) value += UINT32_C(0x08000000);
    char buffer[16];
    snprintf(buffer, sizeof buffer, "0x%08X", value);
    *address = copy_string(buffer);
}

static bool discover_calls(const char *body, size_t size, Strings *calls) {
    for (size_t index = 0; index < size;) {
        if (!identifier_start(body[index])) { ++index; continue; }
        size_t start = index++;
        while (index < size && identifier_character(body[index])) ++index;
        size_t end = index, after = index;
        while (after < size && isspace((unsigned char)body[after])) ++after;
        if (after < size && body[after] == '(' && !control_word(body + start, end - start)) {
            char *name = copy_range(body, start, end);
            if (!name) return false;
            bool ok = strings_add(calls, name);
            free(name);
            if (!ok) return false;
        }
    }
    strings_sort(calls);
    return true;
}

static bool routines_add(FusionInventoryRoutines *routines,
                         FusionInventoryRoutine routine) {
    if (routines->count == routines->capacity) {
        size_t capacity = routines->capacity ? routines->capacity * 2 : 128;
        FusionInventoryRoutine *items = realloc(routines->items,
                                                capacity * sizeof *items);
        if (!items) return false;
        routines->items = items;
        routines->capacity = capacity;
    }
    routines->items[routines->count++] = routine;
    return true;
}

static void routine_free(FusionInventoryRoutine *routine) {
    free(routine->id); free(routine->symbol); free(routine->kind);
    free(routine->source); free(routine->linkage); free(routine->description);
    free(routine->address);
    for (size_t index = 0; index < routine->raw_call_count; ++index)
        free(routine->raw_calls[index]);
    free(routine->raw_calls);
    for (size_t index = 0; index < routine->source_variant_count; ++index)
        free(routine->source_variants[index]);
    free(routine->source_variants);
    memset(routine, 0, sizeof *routine);
}

void fusion_inventory_routines_free(FusionInventoryRoutines *routines) {
    if (!routines) return;
    for (size_t index = 0; index < routines->count; ++index)
        routine_free(&routines->items[index]);
    free(routines->items);
    memset(routines, 0, sizeof *routines);
}

static bool make_identity(FusionInventoryRoutine *routine, const char *game,
                          const char *flavor, const char *source, const char *symbol) {
    size_t length = strlen(game) + strlen(flavor) + strlen(source) + strlen(symbol) + 4;
    routine->id = malloc(length);
    if (!routine->id) return false;
    snprintf(routine->id, length, "%s:%s:%s:%s", game, flavor, source, symbol);
    routine->symbol = copy_string(symbol);
    routine->kind = copy_string(!strcmp(flavor, "c") ? "c_function" : "asm_function");
    routine->source = copy_string(source);
    routine->linkage = copy_string(!strcmp(flavor, "c") ? "external" : "external");
    return routine->symbol && routine->kind && routine->source && routine->linkage;
}

static bool discover_c(const char *game, const char *root, const char *relative,
                       FusionInventoryRoutines *routines,
                       char *error, size_t error_size) {
    char path[4096];
    int length = snprintf(path, sizeof path, "%s/%s", root, relative);
    if (length < 0 || (size_t)length >= sizeof path)
        return fail(error, error_size, "source path is too long");
    char *source = NULL;
    size_t size = 0;
    if (!read_file(path, &source, &size, error, error_size)) return false;
    char *masked = mask_c(source, size);
    if (!masked) { free(source); return fail(error, error_size, "out of memory"); }
    size_t statement_start = 0, body_start = 0;
    int depth = 0;
    FusionInventoryRoutine current = {0};
    bool active = false, ok = true;
    for (size_t index = 0; ok && index < size; ++index) {
        char character = masked[index];
        if (character == '{' && depth == 0) {
            FunctionHeader header;
            if (function_header(masked, statement_start, index, &header)) {
                char *symbol = copy_range(masked, header.name_start, header.name_end);
                if (!symbol || !make_identity(&current, game, "c", relative, symbol)) {
                    free(symbol); routine_free(&current);
                    ok = fail(error, error_size, "out of memory");
                    break;
                }
                free(symbol);
                if (header.internal) {
                    free(current.linkage);
                    current.linkage = copy_string("internal");
                    if (!current.linkage) { routine_free(&current); ok = false; break; }
                }
                current.line = line_number(masked, header.name_start);
                current.body_start = character_offset(source, index + 1);
                brief_and_address(source, header.name_start,
                                  &current.description, &current.address);
                if (!current.address) current.address = source_address(current.symbol);
                body_start = index + 1;
                active = true;
            }
            depth = 1;
        } else if (character == '{' && depth > 0) ++depth;
        else if (character == '}' && depth > 0) {
            --depth;
            if (depth == 0) {
                if (active) {
                    Strings calls = {0};
                    if (!discover_calls(masked + body_start, index - body_start, &calls)) {
                        strings_free(&calls); routine_free(&current);
                        ok = fail(error, error_size, "out of memory");
                        break;
                    }
                    current.raw_calls = calls.items;
                    current.raw_call_count = calls.count;
                    if (!routines_add(routines, current)) {
                        routine_free(&current);
                        ok = fail(error, error_size, "out of memory");
                        break;
                    }
                    memset(&current, 0, sizeof current);
                }
                active = false;
                statement_start = index + 1;
            }
        } else if (character == ';' && depth == 0) statement_start = index + 1;
    }
    free(masked);
    free(source);
    return ok;
}

static bool markers_add(Markers *markers, const char *symbol, size_t offset, size_t line) {
    for (size_t index = 0; index < markers->count; ++index)
        if (!strcmp(markers->items[index].symbol, symbol)) return true;
    if (markers->count == markers->capacity) {
        size_t capacity = markers->capacity ? markers->capacity * 2 : 64;
        AsmMarker *items = realloc(markers->items, capacity * sizeof *items);
        if (!items) return false;
        markers->items = items;
        markers->capacity = capacity;
    }
    char *copy = copy_string(symbol);
    if (!copy) return false;
    markers->items[markers->count++] = (AsmMarker){copy, offset, line};
    return true;
}

static void markers_free(Markers *markers) {
    for (size_t index = 0; index < markers->count; ++index) free(markers->items[index].symbol);
    free(markers->items);
}

static char *asm_marker_symbol(const char *line, size_t length) {
    while (length && isspace((unsigned char)*line)) { ++line; --length; }
    while (length && isspace((unsigned char)line[length - 1])) --length;
    static const char *const starts[] = {"thumb_func_start", "arm_func_start"};
    for (size_t index = 0; index < 2; ++index) {
        size_t prefix = strlen(starts[index]);
        if (length > prefix && !memcmp(line, starts[index], prefix) &&
            isspace((unsigned char)line[prefix])) {
            size_t start = prefix;
            while (start < length && isspace((unsigned char)line[start])) ++start;
            size_t end = start;
            while (end < length && (identifier_character(line[end]) || line[end] == '.' ||
                                    line[end] == '$')) ++end;
            return end > start ? copy_range(line, start, end) : NULL;
        }
    }
    if (length > 5 && !memcmp(line, ".type", 5) && isspace((unsigned char)line[5])) {
        size_t start = 5;
        while (start < length && isspace((unsigned char)line[start])) ++start;
        size_t end = start;
        while (end < length && line[end] != ',' && !isspace((unsigned char)line[end])) ++end;
        const char *comma = memchr(line + end, ',', length - end);
        if (!comma) return NULL;
        const char *kind = comma + 1;
        while (kind < line + length && isspace((unsigned char)*kind)) ++kind;
        if (*kind == '%') ++kind;
        if ((size_t)(line + length - kind) != 8 || memcmp(kind, "function", 8)) return NULL;
        return end > start ? copy_range(line, start, end) : NULL;
    }
    return NULL;
}

static size_t find_asm_label(const char *source, size_t start, size_t size,
                             const char *symbol) {
    size_t wanted = strlen(symbol);
    for (size_t offset = start; offset < size;) {
        size_t end = offset;
        while (end < size && source[end] != '\n') ++end;
        size_t cursor = offset;
        while (cursor < end && isspace((unsigned char)source[cursor])) ++cursor;
        if (end - cursor >= wanted + 1 && !memcmp(source + cursor, symbol, wanted) &&
            source[cursor + wanted] == ':') return end;
        offset = end < size ? end + 1 : end;
    }
    return SIZE_MAX;
}

static bool discover_asm_calls(const char *source, size_t start, size_t end,
                               Strings *calls) {
    for (size_t offset = start; offset < end;) {
        size_t line_end = offset;
        while (line_end < end && source[line_end] != '\n') ++line_end;
        size_t cursor = offset;
        while (cursor < line_end && isspace((unsigned char)source[cursor])) ++cursor;
        size_t operation_start = cursor;
        while (cursor < line_end && isalpha((unsigned char)source[cursor])) ++cursor;
        size_t operation_length = cursor - operation_start;
        if ((is_word(source + operation_start, operation_length, "bl") ||
             is_word(source + operation_start, operation_length, "blx") ||
             is_word(source + operation_start, operation_length, "b")) &&
            cursor < line_end && isspace((unsigned char)source[cursor])) {
            while (cursor < line_end && isspace((unsigned char)source[cursor])) ++cursor;
            size_t symbol_start = cursor;
            while (cursor < line_end && (identifier_character(source[cursor]) ||
                   source[cursor] == '.' || source[cursor] == '$')) ++cursor;
            if (cursor > symbol_start) {
                char *symbol = copy_range(source, symbol_start, cursor);
                if (!symbol) return false;
                bool ok = strings_add(calls, symbol);
                free(symbol);
                if (!ok) return false;
            }
        }
        offset = line_end < end ? line_end + 1 : line_end;
    }
    strings_sort(calls);
    return true;
}

static bool discover_asm(const char *game, const char *root, const char *relative,
                         FusionInventoryRoutines *routines,
                         char *error, size_t error_size) {
    char path[4096];
    int length = snprintf(path, sizeof path, "%s/%s", root, relative);
    if (length < 0 || (size_t)length >= sizeof path)
        return fail(error, error_size, "source path is too long");
    char *source = NULL;
    size_t size = 0;
    if (!read_file(path, &source, &size, error, error_size)) return false;
    Markers markers = {0};
    bool ok = true;
    size_t line = 1, blank_run = 0;
    for (size_t offset = 0; ok && offset < size;) {
        size_t end = offset;
        while (end < size && source[end] != '\n') ++end;
        char *symbol = asm_marker_symbol(source + offset, end - offset);
        if (symbol) {
            ok = markers_add(&markers, symbol, offset, line - blank_run);
            free(symbol);
        }
        bool blank = true;
        for (size_t cursor = offset; cursor < end; ++cursor)
            if (!isspace((unsigned char)source[cursor])) { blank = false; break; }
        if (blank) ++blank_run;
        else blank_run = 0;
        if (end < size) { offset = end + 1; ++line; }
        else offset = end;
    }
    for (size_t index = 0; ok && index < markers.count; ++index) {
        AsmMarker *marker = &markers.items[index];
        size_t label_end = find_asm_label(source, marker->offset, size, marker->symbol);
        if (label_end == SIZE_MAX) continue;
        size_t body_end = index + 1 < markers.count ? markers.items[index + 1].offset : size;
        FusionInventoryRoutine routine = {0};
        if (!make_identity(&routine, game, "asm", relative, marker->symbol)) {
            routine_free(&routine); ok = false; break;
        }
        routine.line = marker->line;
        routine.address = source_address(marker->symbol);
        Strings calls = {0};
        if (!discover_asm_calls(source, label_end, body_end, &calls)) {
            routine_free(&routine); strings_free(&calls); ok = false; break;
        }
        routine.raw_calls = calls.items;
        routine.raw_call_count = calls.count;
        if (!routines_add(routines, routine)) {
            routine_free(&routine); ok = false; break;
        }
    }
    if (!ok) fail(error, error_size, "out of memory");
    markers_free(&markers);
    free(source);
    return ok;
}

static bool same_stem(const char *left, const char *right) {
    size_t a = strlen(left), b = strlen(right);
    if (a < 2 || b < 2 || left[a - 2] != '.' || right[b - 2] != '.') return false;
    return a == b && !memcmp(left, right, a - 2);
}

static bool add_variant(FusionInventoryRoutine *routine, const char *source) {
    char **variants = realloc(routine->source_variants,
                              (routine->source_variant_count + 1) * sizeof *variants);
    if (!variants) return false;
    routine->source_variants = variants;
    variants[routine->source_variant_count] = copy_string(source);
    if (!variants[routine->source_variant_count]) return false;
    ++routine->source_variant_count;
    return true;
}

static bool deduplicate_assembly(FusionInventoryRoutines *routines,
                                 char *error, size_t error_size) {
    for (size_t index = 0; index < routines->count;) {
        FusionInventoryRoutine *item = &routines->items[index];
        if (strcmp(item->kind, "asm_function")) { ++index; continue; }
        FusionInventoryRoutine *match = NULL;
        size_t external_count = 0;
        bool stem_found = false;
        for (size_t candidate = 0; candidate < routines->count; ++candidate) {
            FusionInventoryRoutine *other = &routines->items[candidate];
            if (strcmp(other->kind, "c_function") ||
                strcmp(other->symbol, item->symbol)) continue;
            if (same_stem(other->source, item->source)) {
                match = other;
                stem_found = true;
            } else if (!strcmp(other->linkage, "external")) {
                if (!stem_found) match = other;
                ++external_count;
            }
        }
        if (match && (stem_found || external_count == 1)) {
            if (!add_variant(match, item->source))
                return fail(error, error_size, "out of memory");
            routine_free(item);
            memmove(item, item + 1, (routines->count - index - 1) * sizeof *item);
            --routines->count;
        } else ++index;
    }
    /* Python's list.sort is stable; duplicate native IDs occur in the pinned
     * sources and therefore retain source-discovery order. */
    for (size_t index = 1; index < routines->count; ++index) {
        FusionInventoryRoutine value = routines->items[index];
        size_t target = index;
        while (target && strcmp(routines->items[target - 1].id, value.id) > 0) {
            routines->items[target] = routines->items[target - 1];
            --target;
        }
        routines->items[target] = value;
    }
    return true;
}

bool fusion_inventory_scan_routines(const char *game, const char *source_root,
                                    FusionInventoryRoutines *routines,
                                    char *error, size_t error_size) {
    if (!game || !source_root || !routines)
        return fail(error, error_size, "invalid inventory scan arguments");
    memset(routines, 0, sizeof *routines);
    Strings paths = {0};
    if (!collect_paths(source_root, "", &paths, error, error_size)) {
        strings_free(&paths);
        return false;
    }
    strings_sort(&paths);
    bool ok = true;
    for (size_t index = 0; ok && index < paths.count; ++index) {
        if (has_suffix(paths.items[index], ".c"))
            ok = discover_c(game, source_root, paths.items[index], routines,
                            error, error_size);
        else
            ok = discover_asm(game, source_root, paths.items[index], routines,
                              error, error_size);
    }
    strings_free(&paths);
    if (ok) ok = deduplicate_assembly(routines, error, error_size);
    if (!ok) fusion_inventory_routines_free(routines);
    return ok;
}

static const FusionJson *json_member(const FusionJson *object, const char *name,
                                     FusionJsonKind kind) {
    const FusionJson *value = fusion_json_member(object, name);
    return value && value->kind == kind ? value : NULL;
}

static const char *json_string(const FusionJson *object, const char *name) {
    const FusionJson *value = json_member(object, name, FUSION_JSON_STRING);
    return value ? value->string : NULL;
}

static bool nullable_equal(const char *left, const char *right) {
    return (!left && !right) || (left && right && !strcmp(left, right));
}

static bool compare_record(const FusionInventoryRoutine *actual,
                           const FusionInventoryRoutines *all,
                           const FusionJson *expected,
                           char *error, size_t error_size) {
    const char *id = json_string(expected, "id");
    const FusionJson *line = json_member(expected, "line", FUSION_JSON_NUMBER);
    const FusionJson *body = json_member(expected, "_body_start", FUSION_JSON_NUMBER);
    const FusionJson *variants = json_member(expected, "source_variants", FUSION_JSON_ARRAY);
    if (!id || strcmp(actual->id, id))
        return fail(error, error_size, "native routine id differs at %s / %s",
                    actual->id, id ? id : "(missing)");
#define CHECK_STRING(field, member_name) \
    if (!nullable_equal(actual->field, json_string(expected, member_name))) \
        return fail(error, error_size, "native routine %s differs at %s", member_name, id)
    CHECK_STRING(symbol, "symbol");
    CHECK_STRING(kind, "kind");
    CHECK_STRING(source, "source");
    CHECK_STRING(linkage, "linkage");
    CHECK_STRING(description, "description");
    CHECK_STRING(address, "address");
#undef CHECK_STRING
    if (!line || !line->integral || actual->line != (size_t)line->integer)
        return fail(error, error_size, "native routine line differs at %s: %zu / %lld",
                    id, actual->line, line && line->integral ? (long long)line->integer : -1LL);
    if ((body && (!body->integral || actual->body_start != (size_t)body->integer)) ||
        (!body && actual->body_start))
        return fail(error, error_size,
                    "native routine body offset differs at %s: %zu / %lld", id,
                    actual->body_start,
                    body && body->integral ? (long long)body->integer : -1LL);
    size_t variant_count = variants ? variants->count : 0;
    if (variant_count != actual->source_variant_count)
        return fail(error, error_size, "native routine variants differ at %s", id);
    for (size_t index = 0; index < variant_count; ++index)
        if (variants->items[index].kind != FUSION_JSON_STRING ||
            strcmp(variants->items[index].string, actual->source_variants[index]))
            return fail(error, error_size, "native routine variants differ at %s", id);

    Strings resolved = {0}, ambiguous = {0}, indirect = {0}, tracked = {0};
    bool ok = true;
    for (size_t call = 0; ok && call < actual->raw_call_count; ++call) {
        const char *symbol = actual->raw_calls[call];
        size_t matches = 0, same_file = 0;
        const char *match_id = NULL, *same_id = NULL;
        for (size_t candidate = 0; candidate < all->count; ++candidate) {
            if (strcmp(all->items[candidate].symbol, symbol)) continue;
            ++matches;
            match_id = all->items[candidate].id;
            if (!strcmp(all->items[candidate].source, actual->source)) {
                ++same_file;
                same_id = all->items[candidate].id;
            }
        }
        if (matches == 1) ok = strings_add(&resolved, match_id);
        else if (same_file == 1) ok = strings_add(&resolved, same_id);
        else if (matches > 1) ok = strings_add(&ambiguous, symbol);
    }
    const FusionJson *indirect_calls = json_member(expected, "indirect_calls",
                                                   FUSION_JSON_ARRAY);
    if (indirect_calls) for (size_t index = 0; ok && index < indirect_calls->count; ++index) {
        const FusionJson *targets = json_member(&indirect_calls->items[index], "targets",
                                                FUSION_JSON_ARRAY);
        if (!targets) { ok = false; break; }
        for (size_t target = 0; ok && target < targets->count; ++target)
            ok = targets->items[target].kind == FUSION_JSON_STRING &&
                 strings_add(&indirect, targets->items[target].string);
    }
    const FusionJson *calls = json_member(expected, "calls", FUSION_JSON_ARRAY);
    if (calls) {
        for (size_t index = 0; ok && index < calls->count; ++index) {
            if (calls->items[index].kind != FUSION_JSON_STRING) ok = false;
            else if (!strings_contains(&indirect, calls->items[index].string))
                ok = strings_add(&tracked, calls->items[index].string);
        }
    }
    strings_sort(&resolved); strings_sort(&ambiguous); strings_sort(&tracked);
    const FusionJson *expected_ambiguous = json_member(expected, "ambiguous_calls",
                                                       FUSION_JSON_ARRAY);
    size_t ambiguous_count = expected_ambiguous ? expected_ambiguous->count : 0;
    if (!ok || resolved.count != tracked.count || ambiguous.count != ambiguous_count)
        ok = false;
    for (size_t index = 0; ok && index < resolved.count; ++index)
        ok = !strcmp(resolved.items[index], tracked.items[index]);
    for (size_t index = 0; ok && index < ambiguous.count; ++index)
        ok = expected_ambiguous->items[index].kind == FUSION_JSON_STRING &&
             !strcmp(ambiguous.items[index], expected_ambiguous->items[index].string);
    strings_free(&resolved); strings_free(&ambiguous);
    strings_free(&indirect); strings_free(&tracked);
    if (!ok) return fail(error, error_size, "native routine direct calls differ at %s", id);
    return true;
}

static bool audit_game(const char *root, const char *manifest_directory,
                       const FusionJson *manifest, const char *game,
                       const char *source_relative, size_t *count,
                       char *error, size_t error_size) {
    const FusionJson *games = json_member(manifest, "games", FUSION_JSON_OBJECT);
    const FusionJson *game_data = games ? json_member(games, game, FUSION_JSON_OBJECT) : NULL;
    const FusionJson *files = game_data ? json_member(game_data, "record_files",
                                                      FUSION_JSON_ARRAY) : NULL;
    if (!files) return fail(error, error_size, "invalid native inventory manifest");
    FusionJson expected = fusion_json_array();
    bool ok = true;
    for (size_t index = 0; ok && index < files->count; ++index) {
        const FusionJson *record = &files->items[index];
        const char *collection = json_string(record, "collection");
        if (!collection || strcmp(collection, "routines")) continue;
        const char *name = json_string(record, "path");
        if (!name || strchr(name, '/') || strchr(name, '\\')) {
            ok = fail(error, error_size, "invalid routine fragment path");
            break;
        }
        char path[4096];
        int length = snprintf(path, sizeof path, "%s/%s", manifest_directory, name);
        if (length < 0 || (size_t)length >= sizeof path) {
            ok = fail(error, error_size, "inventory fragment path is too long");
            break;
        }
        char *text = NULL; size_t size = 0; FusionJson shard = {0};
        if (!read_file(path, &text, &size, error, error_size)) { ok = false; break; }
        ok = fusion_json_parse(text, size, &shard, error, error_size);
        free(text);
        const FusionJson *records = ok ? json_member(&shard, "records", FUSION_JSON_ARRAY) : NULL;
        if (ok && (!records || strcmp(json_string(&shard, "game"), game) ||
                   strcmp(json_string(&shard, "collection"), "routines")))
            ok = fail(error, error_size, "invalid routine fragment %s", name);
        for (size_t item = 0; ok && item < records->count; ++item) {
            FusionJson copy;
            if (!fusion_json_clone(&records->items[item], &copy) ||
                !fusion_json_push(&expected, copy))
                ok = fail(error, error_size, "out of memory");
        }
        fusion_json_free(&shard);
    }
    char source_root[4096];
    int length = snprintf(source_root, sizeof source_root, "%s/%s", root, source_relative);
    if (ok && (length < 0 || (size_t)length >= sizeof source_root))
        ok = fail(error, error_size, "source root path is too long");
    FusionInventoryRoutines actual = {0};
    if (ok) ok = fusion_inventory_scan_routines(game, source_root, &actual,
                                                error, error_size);
    if (ok && actual.count != expected.count)
        ok = fail(error, error_size, "%s routine count differs: native %zu, tracked %zu",
                  game, actual.count, expected.count);
    for (size_t index = 0; ok && index < actual.count; ++index)
        ok = compare_record(&actual.items[index], &actual, &expected.items[index],
                            error, error_size);
    if (ok) *count = actual.count;
    fusion_inventory_routines_free(&actual);
    fusion_json_free(&expected);
    return ok;
}

bool fusion_inventory_audit_routines(const char *root, const char *manifest_path,
                                     size_t counts[2],
                                     char *error, size_t error_size) {
    if (!root || !manifest_path || !counts)
        return fail(error, error_size, "invalid inventory audit arguments");
    char absolute[4096];
    int length = manifest_path[0] == '/'
        ? snprintf(absolute, sizeof absolute, "%s", manifest_path)
        : snprintf(absolute, sizeof absolute, "%s/%s", root, manifest_path);
    if (length < 0 || (size_t)length >= sizeof absolute)
        return fail(error, error_size, "inventory path is too long");
    char *text = NULL; size_t size = 0; FusionJson manifest = {0};
    if (!read_file(absolute, &text, &size, error, error_size)) return false;
    bool ok = fusion_json_parse(text, size, &manifest, error, error_size);
    free(text);
    if (!ok) return false;
    if (!json_string(&manifest, "schema") ||
        strcmp(json_string(&manifest, "schema"), "metroidvania-native-inventory-v6")) {
        fusion_json_free(&manifest);
        return fail(error, error_size, "unsupported native inventory schema");
    }
    char directory[4096];
    snprintf(directory, sizeof directory, "%s", absolute);
    char *slash = strrchr(directory, '/');
    if (slash) *slash = '\0'; else snprintf(directory, sizeof directory, ".");
    ok = audit_game(root, directory, &manifest, "mzm", "third_party/mzm",
                    &counts[0], error, error_size) &&
         audit_game(root, directory, &manifest, "aos", "third_party/cvaos",
                    &counts[1], error, error_size);
    fusion_json_free(&manifest);
    return ok;
}
