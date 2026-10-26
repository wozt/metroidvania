/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/json.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_DEPTH = 64 };

typedef struct {
    const char *text;
    size_t size;
    size_t offset;
    char *error;
    size_t error_size;
} Parser;

/* A json.JSONDecodeError message: the position is counted in characters,
 * with its line and column. */
static bool fail_at(Parser *parser, const char *message, size_t position) {
    if (parser->error && parser->error_size) {
        size_t characters = 0, line = 1, line_start = 0;
        for (size_t index = 0; index < position && index < parser->size; ++index) {
            unsigned char c = (unsigned char)parser->text[index];
            if ((c & 0xC0) == 0x80) continue;
            if (c == '\n') {
                ++line;
                line_start = characters + 1;
            }
            ++characters;
        }
        snprintf(parser->error, parser->error_size, "%s: line %zu column %zu (char %zu)",
                 message, line, characters - line_start + 1, characters);
        parser->error[parser->error_size - 1] = '\0';
    }
    return false;
}

static bool out_of_memory(Parser *parser) {
    if (parser->error && parser->error_size)
        snprintf(parser->error, parser->error_size, "out of memory");
    return false;
}

static void skip_space(Parser *parser) {
    while (parser->offset < parser->size) {
        char c = parser->text[parser->offset];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return;
        ++parser->offset;
    }
}

static bool starts(const Parser *parser, const char *word) {
    size_t length = strlen(word);
    return parser->size - parser->offset >= length &&
           !memcmp(parser->text + parser->offset, word, length);
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Four hexadecimal digits at ``offset``. */
static bool hex4(const Parser *parser, size_t offset, uint32_t *code) {
    if (parser->size < 4 || offset > parser->size - 4) return false;
    *code = 0;
    for (size_t digit = 0; digit < 4; ++digit) {
        int value = hex_digit(parser->text[offset + digit]);
        if (value < 0) return false;
        *code = *code * 16 + (uint32_t)value;
    }
    return true;
}

static size_t encode_utf8(uint32_t code, unsigned char bytes[4]) {
    if (code < 0x80) {
        bytes[0] = (unsigned char)code;
        return 1;
    }
    if (code < 0x800) {
        bytes[0] = (unsigned char)(0xC0 | (code >> 6));
        bytes[1] = (unsigned char)(0x80 | (code & 0x3F));
        return 2;
    }
    if (code < 0x10000) {
        bytes[0] = (unsigned char)(0xE0 | (code >> 12));
        bytes[1] = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
        bytes[2] = (unsigned char)(0x80 | (code & 0x3F));
        return 3;
    }
    bytes[0] = (unsigned char)(0xF0 | (code >> 18));
    bytes[1] = (unsigned char)(0x80 | ((code >> 12) & 0x3F));
    bytes[2] = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
    bytes[3] = (unsigned char)(0x80 | (code & 0x3F));
    return 4;
}

/* scanstring(): the string whose opening quote is at the offset. */
static bool parse_string(Parser *parser, char **result) {
    size_t start = parser->offset++;
    size_t capacity = 16, length = 0;
    char *out = malloc(capacity);
    if (!out) return out_of_memory(parser);
    for (;;) {
        if (parser->offset >= parser->size) {
            free(out);
            return fail_at(parser, "Unterminated string starting at", start);
        }
        size_t position = parser->offset;
        unsigned char c = (unsigned char)parser->text[parser->offset++];
        unsigned char bytes[4];
        size_t count = 1;
        bytes[0] = c;
        if (c == '"') break;
        if (c < 0x20) {
            free(out);
            return fail_at(parser, "Invalid control character at", position);
        }
        if (c == '\\') {
            if (parser->offset >= parser->size) {
                free(out);
                return fail_at(parser, "Unterminated string starting at", start);
            }
            char escape = parser->text[parser->offset++];
            uint32_t code = 0;
            switch (escape) {
                case '"': code = '"'; break;
                case '\\': code = '\\'; break;
                case '/': code = '/'; break;
                case 'b': code = '\b'; break;
                case 'f': code = '\f'; break;
                case 'n': code = '\n'; break;
                case 'r': code = '\r'; break;
                case 't': code = '\t'; break;
                case 'u':
                    if (!hex4(parser, parser->offset, &code)) {
                        free(out);
                        return fail_at(parser, "Invalid \\uXXXX escape", parser->offset - 1);
                    }
                    parser->offset += 4;
                    /* A high surrogate followed by \u must pair with it. */
                    if (code >= 0xD800 && code <= 0xDBFF && starts(parser, "\\u")) {
                        uint32_t low = 0;
                        if (!hex4(parser, parser->offset + 2, &low)) {
                            free(out);
                            return fail_at(parser, "Invalid \\uXXXX escape", parser->offset + 1);
                        }
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                            parser->offset += 6;
                        }
                    }
                    break;
                default:
                    free(out);
                    return fail_at(parser, "Invalid \\escape", position);
            }
            count = encode_utf8(code, bytes);
        }
        if (length + count + 1 > capacity) {
            capacity *= 2;
            char *grown = realloc(out, capacity);
            if (!grown) {
                free(out);
                return out_of_memory(parser);
            }
            out = grown;
        }
        memcpy(out + length, bytes, count);
        length += count;
    }
    out[length] = '\0';
    *result = out;
    return true;
}

static bool is_digit(const Parser *parser, size_t offset) {
    return offset < parser->size && parser->text[offset] >= '0' && parser->text[offset] <= '9';
}

/* The number grammar of the scanner: -?(0|[1-9]\d*)(\.\d+)?([eE][-+]?\d+)? */
static bool parse_number(Parser *parser, FusionJson *value) {
    size_t start = parser->offset, cursor = start;
    if (cursor < parser->size && parser->text[cursor] == '-') ++cursor;
    if (!is_digit(parser, cursor)) return fail_at(parser, "Expecting value", start);
    if (parser->text[cursor] == '0') ++cursor;
    else while (is_digit(parser, cursor)) ++cursor;
    bool integral = true;
    if (cursor < parser->size && parser->text[cursor] == '.' && is_digit(parser, cursor + 1)) {
        integral = false;
        cursor += 1;
        while (is_digit(parser, cursor)) ++cursor;
    }
    if (cursor < parser->size && (parser->text[cursor] == 'e' || parser->text[cursor] == 'E')) {
        size_t exponent = cursor + 1;
        if (exponent < parser->size &&
            (parser->text[exponent] == '+' || parser->text[exponent] == '-'))
            ++exponent;
        if (is_digit(parser, exponent)) {
            integral = false;
            cursor = exponent;
            while (is_digit(parser, cursor)) ++cursor;
        }
    }
    char buffer[400];
    size_t length = cursor - start;
    if (length >= sizeof buffer) return fail_at(parser, "Expecting value", start);
    memcpy(buffer, parser->text + start, length);
    buffer[length] = '\0';
    parser->offset = cursor;
    value->kind = FUSION_JSON_NUMBER;
    value->integral = integral;
    if (integral) {
        value->integer = strtoll(buffer, NULL, 10);
        value->number = (double)value->integer;
    } else {
        value->number = strtod(buffer, NULL);
        value->integer = (int64_t)value->number;
    }
    return true;
}

static bool parse_value(Parser *parser, FusionJson *value, int depth);

static bool reserve(FusionJson *container) {
    if (container->count < container->capacity) return true;
    size_t next = container->capacity ? container->capacity * 2 : 8;
    FusionJson *items = realloc(container->items, next * sizeof *items);
    if (!items) return false;
    container->items = items;
    if (container->kind == FUSION_JSON_OBJECT) {
        char **keys = realloc(container->keys, next * sizeof *keys);
        if (!keys) return false;
        container->keys = keys;
    }
    container->capacity = next;
    return true;
}

static bool append(Parser *parser, FusionJson *container, char *key, FusionJson item) {
    if (key) {
        /* A repeated member keeps its first position and the last value, as
         * Python's json.loads does. */
        FusionJson *existing = fusion_json_member_mut(container, key);
        if (existing) {
            fusion_json_free(existing);
            *existing = item;
            free(key);
            return true;
        }
    }
    if (!reserve(container)) {
        free(key);
        fusion_json_free(&item);
        return out_of_memory(parser);
    }
    if (container->kind == FUSION_JSON_OBJECT) container->keys[container->count] = key;
    container->items[container->count++] = item;
    return true;
}

static bool parse_object(Parser *parser, FusionJson *value, int depth) {
    value->kind = FUSION_JSON_OBJECT;
    ++parser->offset;
    skip_space(parser);
    if (parser->offset < parser->size && parser->text[parser->offset] == '}') {
        ++parser->offset;
        return true;
    }
    for (;;) {
        if (parser->offset >= parser->size || parser->text[parser->offset] != '"')
            return fail_at(parser, "Expecting property name enclosed in double quotes",
                           parser->offset);
        char *key = NULL;
        if (!parse_string(parser, &key)) return false;
        skip_space(parser);
        if (parser->offset >= parser->size || parser->text[parser->offset] != ':') {
            free(key);
            return fail_at(parser, "Expecting ':' delimiter", parser->offset);
        }
        ++parser->offset;
        skip_space(parser);
        FusionJson item = {0};
        if (!parse_value(parser, &item, depth + 1)) {
            fusion_json_free(&item);
            free(key);
            return false;
        }
        if (!append(parser, value, key, item)) return false;
        skip_space(parser);
        if (parser->offset < parser->size && parser->text[parser->offset] == '}') {
            ++parser->offset;
            return true;
        }
        if (parser->offset >= parser->size || parser->text[parser->offset] != ',')
            return fail_at(parser, "Expecting ',' delimiter", parser->offset);
        size_t comma = parser->offset++;
        skip_space(parser);
        if (parser->offset < parser->size && parser->text[parser->offset] == '}')
            return fail_at(parser, "Illegal trailing comma before end of object", comma);
    }
}

static bool parse_array(Parser *parser, FusionJson *value, int depth) {
    value->kind = FUSION_JSON_ARRAY;
    ++parser->offset;
    skip_space(parser);
    if (parser->offset < parser->size && parser->text[parser->offset] == ']') {
        ++parser->offset;
        return true;
    }
    for (;;) {
        FusionJson item = {0};
        if (!parse_value(parser, &item, depth + 1)) {
            fusion_json_free(&item);
            return false;
        }
        if (!append(parser, value, NULL, item)) return false;
        skip_space(parser);
        if (parser->offset < parser->size && parser->text[parser->offset] == ']') {
            ++parser->offset;
            return true;
        }
        if (parser->offset >= parser->size || parser->text[parser->offset] != ',')
            return fail_at(parser, "Expecting ',' delimiter", parser->offset);
        size_t comma = parser->offset++;
        skip_space(parser);
        if (parser->offset < parser->size && parser->text[parser->offset] == ']')
            return fail_at(parser, "Illegal trailing comma before end of array", comma);
    }
}

static bool parse_value(Parser *parser, FusionJson *value, int depth) {
    if (depth > MAX_DEPTH) return fail_at(parser, "Expecting value", parser->offset);
    skip_space(parser);
    if (parser->offset >= parser->size) return fail_at(parser, "Expecting value", parser->offset);
    char c = parser->text[parser->offset];
    if (c == '{') return parse_object(parser, value, depth);
    if (c == '[') return parse_array(parser, value, depth);
    if (c == '"') {
        value->kind = FUSION_JSON_STRING;
        return parse_string(parser, &value->string);
    }
    static const struct {
        const char *word;
        FusionJsonKind kind;
        bool boolean;
        double number;
    } literals[] = {
        {"true", FUSION_JSON_BOOL, true, 0}, {"false", FUSION_JSON_BOOL, false, 0},
        {"null", FUSION_JSON_NULL, false, 0}, {"NaN", FUSION_JSON_NUMBER, false, NAN},
        {"Infinity", FUSION_JSON_NUMBER, false, INFINITY},
        {"-Infinity", FUSION_JSON_NUMBER, false, -INFINITY},
    };
    for (size_t index = 0; index < sizeof literals / sizeof literals[0]; ++index) {
        if (!starts(parser, literals[index].word)) continue;
        parser->offset += strlen(literals[index].word);
        value->kind = literals[index].kind;
        value->boolean = literals[index].boolean;
        value->number = literals[index].number;
        return true;
    }
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(parser, value);
    return fail_at(parser, "Expecting value", parser->offset);
}

bool fusion_json_parse(const char *text, size_t size, FusionJson *root,
                       char *error, size_t error_size) {
    if (!text || !root) return false;
    *root = (FusionJson){0};
    Parser parser = {text, size, 0, error, error_size};
    if (size >= 3 && !memcmp(text, "\xEF\xBB\xBF", 3))
        return fail_at(&parser, "Unexpected UTF-8 BOM (decode using utf-8-sig)", 0);
    if (!parse_value(&parser, root, 0)) {
        fusion_json_free(root);
        return false;
    }
    skip_space(&parser);
    if (parser.offset != size) {
        fusion_json_free(root);
        return fail_at(&parser, "Extra data", parser.offset);
    }
    return true;
}

void fusion_json_free(FusionJson *value) {
    if (!value) return;
    for (size_t index = 0; index < value->count; ++index) {
        fusion_json_free(&value->items[index]);
        if (value->keys) free(value->keys[index]);
    }
    free(value->items);
    free(value->keys);
    free(value->string);
    *value = (FusionJson){0};
}

const FusionJson *fusion_json_member(const FusionJson *object, const char *key) {
    if (!object || object->kind != FUSION_JSON_OBJECT || !key) return NULL;
    for (size_t index = 0; index < object->count; ++index)
        if (!strcmp(object->keys[index], key)) return &object->items[index];
    return NULL;
}

FusionJson *fusion_json_member_mut(FusionJson *object, const char *key) {
    return (FusionJson *)fusion_json_member(object, key);
}

FusionJson fusion_json_null(void) {
    return (FusionJson){.kind = FUSION_JSON_NULL};
}

FusionJson fusion_json_bool(bool value) {
    return (FusionJson){.kind = FUSION_JSON_BOOL, .boolean = value};
}

FusionJson fusion_json_int(int64_t value) {
    return (FusionJson){.kind = FUSION_JSON_NUMBER, .integral = true, .integer = value,
                        .number = (double)value};
}

FusionJson fusion_json_double(double value) {
    return (FusionJson){.kind = FUSION_JSON_NUMBER, .number = value,
                        .integer = (int64_t)value};
}

FusionJson fusion_json_string(const char *value) {
    FusionJson result = {.kind = FUSION_JSON_STRING};
    if (value) {
        size_t length = strlen(value);
        result.string = malloc(length + 1);
        if (result.string) memcpy(result.string, value, length + 1);
    }
    return result;
}

FusionJson fusion_json_object(void) {
    return (FusionJson){.kind = FUSION_JSON_OBJECT};
}

FusionJson fusion_json_array(void) {
    return (FusionJson){.kind = FUSION_JSON_ARRAY};
}

bool fusion_json_set(FusionJson *object, const char *key, FusionJson value) {
    if (!object || object->kind != FUSION_JSON_OBJECT || !key ||
        (value.kind == FUSION_JSON_STRING && !value.string)) {
        fusion_json_free(&value);
        return false;
    }
    FusionJson *existing = fusion_json_member_mut(object, key);
    if (existing) {
        fusion_json_free(existing);
        *existing = value;
        return true;
    }
    size_t length = strlen(key);
    char *name = malloc(length + 1);
    if (!name || !reserve(object)) {
        free(name);
        fusion_json_free(&value);
        return false;
    }
    memcpy(name, key, length + 1);
    object->keys[object->count] = name;
    object->items[object->count++] = value;
    return true;
}

bool fusion_json_push(FusionJson *array, FusionJson value) {
    if (!array || array->kind != FUSION_JSON_ARRAY ||
        (value.kind == FUSION_JSON_STRING && !value.string) || !reserve(array)) {
        fusion_json_free(&value);
        return false;
    }
    array->items[array->count++] = value;
    return true;
}

bool fusion_json_remove(FusionJson *object, const char *key) {
    if (!object || object->kind != FUSION_JSON_OBJECT) return false;
    for (size_t index = 0; index < object->count; ++index) {
        if (strcmp(object->keys[index], key)) continue;
        fusion_json_free(&object->items[index]);
        free(object->keys[index]);
        memmove(object->items + index, object->items + index + 1,
                (object->count - index - 1) * sizeof *object->items);
        memmove(object->keys + index, object->keys + index + 1,
                (object->count - index - 1) * sizeof *object->keys);
        --object->count;
        return true;
    }
    return false;
}

bool fusion_json_clone(const FusionJson *source, FusionJson *copy) {
    *copy = (FusionJson){0};
    if (!source) return false;
    switch (source->kind) {
        case FUSION_JSON_STRING:
            *copy = fusion_json_string(source->string);
            return copy->string != NULL;
        case FUSION_JSON_ARRAY:
        case FUSION_JSON_OBJECT: {
            copy->kind = source->kind;
            for (size_t index = 0; index < source->count; ++index) {
                FusionJson item;
                if (!fusion_json_clone(&source->items[index], &item) ||
                    !(source->kind == FUSION_JSON_OBJECT
                          ? fusion_json_set(copy, source->keys[index], item)
                          : fusion_json_push(copy, item))) {
                    fusion_json_free(copy);
                    return false;
                }
            }
            return true;
        }
        default:
            *copy = *source;
            copy->string = NULL;
            copy->items = NULL;
            copy->keys = NULL;
            copy->count = copy->capacity = 0;
            return true;
    }
}

/* Python equality: objects ignore member order, and booleans compare as the
 * integers 0 and 1. */
bool fusion_json_equal(const FusionJson *left, const FusionJson *right) {
    bool left_numeric = left->kind == FUSION_JSON_NUMBER || left->kind == FUSION_JSON_BOOL;
    bool right_numeric = right->kind == FUSION_JSON_NUMBER || right->kind == FUSION_JSON_BOOL;
    if (left_numeric && right_numeric) {
        double a = left->kind == FUSION_JSON_BOOL ? left->boolean : left->number;
        double b = right->kind == FUSION_JSON_BOOL ? right->boolean : right->number;
        if (left->kind == FUSION_JSON_NUMBER && right->kind == FUSION_JSON_NUMBER &&
            left->integral && right->integral)
            return left->integer == right->integer;
        return a == b;
    }
    if (left->kind != right->kind) return false;
    switch (left->kind) {
        case FUSION_JSON_NULL: return true;
        case FUSION_JSON_STRING: return !strcmp(left->string, right->string);
        case FUSION_JSON_ARRAY:
            if (left->count != right->count) return false;
            for (size_t index = 0; index < left->count; ++index)
                if (!fusion_json_equal(&left->items[index], &right->items[index]))
                    return false;
            return true;
        case FUSION_JSON_OBJECT:
            if (left->count != right->count) return false;
            for (size_t index = 0; index < left->count; ++index) {
                const FusionJson *other = fusion_json_member(right, left->keys[index]);
                if (!other || !fusion_json_equal(&left->items[index], other)) return false;
            }
            return true;
        default:
            return false;
    }
}

void fusion_json_float_repr(double value, char *buffer, size_t size) {
    if (isnan(value)) {
        snprintf(buffer, size, "NaN");
        return;
    }
    if (isinf(value)) {
        snprintf(buffer, size, value < 0 ? "-Infinity" : "Infinity");
        return;
    }
    /* The shortest scientific form that reads back exactly. */
    char scientific[64];
    for (int precision = 0; precision < 17; ++precision) {
        snprintf(scientific, sizeof scientific, "%.*e", precision, value);
        if (strtod(scientific, NULL) == value) break;
    }
    const char *cursor = scientific;
    bool negative = *cursor == '-';
    if (negative) ++cursor;
    char digits[32];
    size_t count = 0;
    for (; *cursor && *cursor != 'e'; ++cursor)
        if (*cursor != '.') digits[count++] = *cursor;
    int exponent = atoi(cursor + 1);
    while (count > 1 && digits[count - 1] == '0') --count;
    digits[count] = '\0';
    int point = exponent + 1;
    char out[96];
    size_t used = 0;
    if (negative) out[used++] = '-';
    if (point > -4 && point <= 16) {
        if (point <= 0) {
            used += (size_t)snprintf(out + used, sizeof out - used, "0.");
            for (int zero = 0; zero < -point; ++zero) out[used++] = '0';
            used += (size_t)snprintf(out + used, sizeof out - used, "%s", digits);
        } else if ((size_t)point >= count) {
            used += (size_t)snprintf(out + used, sizeof out - used, "%s", digits);
            for (size_t zero = count; zero < (size_t)point; ++zero) out[used++] = '0';
            used += (size_t)snprintf(out + used, sizeof out - used, ".0");
        } else {
            used += (size_t)snprintf(out + used, sizeof out - used, "%.*s.%s", point, digits,
                                     digits + point);
        }
    } else {
        out[used++] = digits[0];
        if (count > 1) used += (size_t)snprintf(out + used, sizeof out - used, ".%s", digits + 1);
        used += (size_t)snprintf(out + used, sizeof out - used, "e%c%02d",
                                 exponent < 0 ? '-' : '+', abs(exponent));
    }
    out[used] = '\0';
    snprintf(buffer, size, "%s", out);
}

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
    bool failed;
} Writer;

static void put(Writer *writer, const char *data, size_t length) {
    if (writer->failed) return;
    if (writer->size + length + 1 > writer->capacity) {
        size_t next = writer->capacity ? writer->capacity : 256;
        while (next < writer->size + length + 1) next *= 2;
        char *grown = realloc(writer->data, next);
        if (!grown) {
            writer->failed = true;
            return;
        }
        writer->data = grown;
        writer->capacity = next;
    }
    memcpy(writer->data + writer->size, data, length);
    writer->size += length;
    writer->data[writer->size] = '\0';
}

static void put_text(Writer *writer, const char *text) {
    put(writer, text, strlen(text));
}

static void put_string(Writer *writer, const char *text) {
    put(writer, "\"", 1);
    for (const unsigned char *c = (const unsigned char *)text; *c; ++c) {
        char escape[8];
        switch (*c) {
            case '"': put_text(writer, "\\\""); break;
            case '\\': put_text(writer, "\\\\"); break;
            case '\n': put_text(writer, "\\n"); break;
            case '\r': put_text(writer, "\\r"); break;
            case '\t': put_text(writer, "\\t"); break;
            case '\b': put_text(writer, "\\b"); break;
            case '\f': put_text(writer, "\\f"); break;
            default:
                if (*c < 0x20) {
                    snprintf(escape, sizeof escape, "\\u%04x", *c);
                    put_text(writer, escape);
                } else {
                    put(writer, (const char *)c, 1);
                }
        }
    }
    put(writer, "\"", 1);
}

static void newline(Writer *writer, int indent, int depth) {
    put(writer, "\n", 1);
    for (int space = 0; space < indent * depth; ++space) put(writer, " ", 1);
}

static void write_value(Writer *writer, const FusionJson *value, int indent, int depth) {
    char number[64];
    switch (value->kind) {
        case FUSION_JSON_NULL: put_text(writer, "null"); return;
        case FUSION_JSON_BOOL: put_text(writer, value->boolean ? "true" : "false"); return;
        case FUSION_JSON_NUMBER:
            if (value->integral)
                snprintf(number, sizeof number, "%lld", (long long)value->integer);
            else
                fusion_json_float_repr(value->number, number, sizeof number);
            put_text(writer, number);
            return;
        case FUSION_JSON_STRING: put_string(writer, value->string); return;
        case FUSION_JSON_ARRAY:
        case FUSION_JSON_OBJECT: {
            bool object = value->kind == FUSION_JSON_OBJECT;
            put(writer, object ? "{" : "[", 1);
            if (!value->count) {
                put(writer, object ? "}" : "]", 1);
                return;
            }
            for (size_t index = 0; index < value->count; ++index) {
                if (index) put(writer, ",", 1);
                if (indent >= 0) newline(writer, indent, depth + 1);
                if (object) {
                    put_string(writer, value->keys[index]);
                    put_text(writer, indent >= 0 ? ": " : ":");
                }
                write_value(writer, &value->items[index], indent, depth + 1);
            }
            if (indent >= 0) newline(writer, indent, depth);
            put(writer, object ? "}" : "]", 1);
            return;
        }
    }
}

bool fusion_json_write(const FusionJson *value, int indent, char **text, size_t *size) {
    Writer writer = {0};
    write_value(&writer, value, indent, 0);
    if (writer.failed || !writer.data) {
        free(writer.data);
        return false;
    }
    *text = writer.data;
    if (size) *size = writer.size;
    return true;
}
