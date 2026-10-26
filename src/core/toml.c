/* SPDX-License-Identifier: GPL-3.0-only */
#include "core/toml.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_DEPTH = 32 };

typedef struct {
    const char *text;
    size_t size;
    size_t offset;
    char *error;
    size_t error_size;
} Parser;

static bool fail(Parser *parser, const char *format, ...) {
    if (parser->error && parser->error_size) {
        size_t line = 1, column = 1;
        for (size_t index = 0; index < parser->offset && index < parser->size; ++index) {
            if (parser->text[index] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        char message[512];
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(message, sizeof message, format, arguments);
        va_end(arguments);
        snprintf(parser->error, parser->error_size, "%s (at line %zu, column %zu)", message,
                 line, column);
    }
    return false;
}

static char peek(const Parser *parser) {
    return parser->offset < parser->size ? parser->text[parser->offset] : '\0';
}

static bool at_end(const Parser *parser) {
    return parser->offset >= parser->size;
}

static void skip_blank(Parser *parser) {
    while (!at_end(parser) && (peek(parser) == ' ' || peek(parser) == '\t')) ++parser->offset;
}

static void skip_comment(Parser *parser) {
    if (peek(parser) == '#')
        while (!at_end(parser) && peek(parser) != '\n') ++parser->offset;
}

/* Blank space, comments and newlines (inside arrays). */
static void skip_space_lines(Parser *parser) {
    for (;;) {
        skip_blank(parser);
        skip_comment(parser);
        if (peek(parser) == '\n') ++parser->offset;
        else if (peek(parser) == '\r' && parser->offset + 1 < parser->size &&
                 parser->text[parser->offset + 1] == '\n') parser->offset += 2;
        else return;
    }
}

/* The end of a line: optional blank space and comment, then a newline. */
static bool end_of_line(Parser *parser) {
    skip_blank(parser);
    skip_comment(parser);
    if (at_end(parser)) return true;
    if (peek(parser) == '\n') {
        ++parser->offset;
        return true;
    }
    if (peek(parser) == '\r' && parser->offset + 1 < parser->size &&
        parser->text[parser->offset + 1] == '\n') {
        parser->offset += 2;
        return true;
    }
    return fail(parser, "Expected newline or end of document after a statement");
}

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} Buffer;

static bool put(Buffer *buffer, const char *data, size_t length) {
    if (buffer->size + length + 1 > buffer->capacity) {
        size_t next = buffer->capacity ? buffer->capacity * 2 : 32;
        while (next < buffer->size + length + 1) next *= 2;
        char *grown = realloc(buffer->data, next);
        if (!grown) return false;
        buffer->data = grown;
        buffer->capacity = next;
    }
    memcpy(buffer->data + buffer->size, data, length);
    buffer->size += length;
    buffer->data[buffer->size] = '\0';
    return true;
}

static bool put_code(Buffer *buffer, uint32_t code) {
    char bytes[4];
    size_t count;
    if (code < 0x80) {
        bytes[0] = (char)code;
        count = 1;
    } else if (code < 0x800) {
        bytes[0] = (char)(0xC0 | (code >> 6));
        bytes[1] = (char)(0x80 | (code & 0x3F));
        count = 2;
    } else if (code < 0x10000) {
        bytes[0] = (char)(0xE0 | (code >> 12));
        bytes[1] = (char)(0x80 | ((code >> 6) & 0x3F));
        bytes[2] = (char)(0x80 | (code & 0x3F));
        count = 3;
    } else {
        bytes[0] = (char)(0xF0 | (code >> 18));
        bytes[1] = (char)(0x80 | ((code >> 12) & 0x3F));
        bytes[2] = (char)(0x80 | ((code >> 6) & 0x3F));
        bytes[3] = (char)(0x80 | (code & 0x3F));
        count = 4;
    }
    return put(buffer, bytes, count);
}

static bool starts(const Parser *parser, const char *word) {
    size_t length = strlen(word);
    return parser->size - parser->offset >= length &&
           !memcmp(parser->text + parser->offset, word, length);
}

static bool parse_escape(Parser *parser, Buffer *buffer) {
    char escape = peek(parser);
    ++parser->offset;
    const char *simple = NULL;
    switch (escape) {
        case 'b': simple = "\b"; break;
        case 't': simple = "\t"; break;
        case 'n': simple = "\n"; break;
        case 'f': simple = "\f"; break;
        case 'r': simple = "\r"; break;
        case '"': simple = "\""; break;
        case '\\': simple = "\\"; break;
        case 'e': simple = "\x1b"; break;
        case 'u':
        case 'U': {
            size_t digits = escape == 'u' ? 4 : 8;
            uint32_t code = 0;
            for (size_t index = 0; index < digits; ++index) {
                char c = peek(parser);
                int value = c >= '0' && c <= '9' ? c - '0'
                          : c >= 'a' && c <= 'f' ? c - 'a' + 10
                          : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
                if (value < 0) return fail(parser, "Invalid hex value");
                code = code * 16 + (uint32_t)value;
                ++parser->offset;
            }
            if ((code >= 0xD800 && code <= 0xDFFF) || code > 0x10FFFF)
                return fail(parser, "Escaped character is not a Unicode scalar value");
            return put_code(buffer, code) || fail(parser, "out of memory");
        }
        default:
            return fail(parser, "Unescaped '\\' in a string");
    }
    return put(buffer, simple, 1) || fail(parser, "out of memory");
}

static bool parse_string(Parser *parser, char **result) {
    Buffer buffer = {0};
    if (!put(&buffer, "", 0)) return fail(parser, "out of memory");
    bool literal = peek(parser) == '\'';
    char quote = literal ? '\'' : '"';
    bool multiline = starts(parser, literal ? "'''" : "\"\"\"");
    parser->offset += multiline ? 3 : 1;
    if (multiline) {
        /* A newline right after the opening delimiter is trimmed. */
        if (peek(parser) == '\n') ++parser->offset;
        else if (starts(parser, "\r\n")) parser->offset += 2;
    }
    for (;;) {
        if (at_end(parser)) {
            free(buffer.data);
            return fail(parser, "Unterminated string");
        }
        char c = peek(parser);
        if (multiline && starts(parser, literal ? "'''" : "\"\"\"")) {
            /* Up to two quotes may close the content before the delimiter. */
            size_t run = 0;
            while (parser->offset + run < parser->size && parser->text[parser->offset + run] == quote)
                ++run;
            if (run > 5) {
                free(buffer.data);
                return fail(parser, "Unterminated string");
            }
            if (!put(&buffer, parser->text + parser->offset, run - 3)) {
                free(buffer.data);
                return fail(parser, "out of memory");
            }
            parser->offset += run;
            break;
        }
        if (!multiline && c == quote) {
            ++parser->offset;
            break;
        }
        if (c == '\n' && !multiline) {
            free(buffer.data);
            return fail(parser, "Unterminated string");
        }
        if ((unsigned char)c < 0x20 && c != '\t' && c != '\n' && c != '\r') {
            free(buffer.data);
            return fail(parser, "Illegal character in a string");
        }
        if (c == 0x7F) {
            free(buffer.data);
            return fail(parser, "Illegal character in a string");
        }
        if (c == '\\' && !literal) {
            ++parser->offset;
            if (multiline) {
                /* A line-ending backslash trims the following whitespace. */
                size_t probe = parser->offset;
                while (probe < parser->size &&
                       (parser->text[probe] == ' ' || parser->text[probe] == '\t'))
                    ++probe;
                if (probe < parser->size &&
                    (parser->text[probe] == '\n' || parser->text[probe] == '\r')) {
                    parser->offset = probe;
                    while (!at_end(parser) && (peek(parser) == ' ' || peek(parser) == '\t' ||
                                               peek(parser) == '\n' || peek(parser) == '\r'))
                        ++parser->offset;
                    continue;
                }
            }
            if (!parse_escape(parser, &buffer)) {
                free(buffer.data);
                return false;
            }
            continue;
        }
        if (!put(&buffer, &c, 1)) {
            free(buffer.data);
            return fail(parser, "out of memory");
        }
        ++parser->offset;
    }
    *result = buffer.data;
    return true;
}

static bool is_bare(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '-';
}

/* One key part: bare or quoted. */
static bool parse_key_part(Parser *parser, char **key) {
    char c = peek(parser);
    if (c == '"' || c == '\'') {
        if (starts(parser, "\"\"\"") || starts(parser, "'''"))
            return fail(parser, "Multi-line strings are not keys");
        return parse_string(parser, key);
    }
    size_t start = parser->offset;
    while (!at_end(parser) && is_bare(peek(parser))) ++parser->offset;
    if (parser->offset == start) return fail(parser, "Invalid initial character for a key part");
    size_t length = parser->offset - start;
    *key = malloc(length + 1);
    if (!*key) return fail(parser, "out of memory");
    memcpy(*key, parser->text + start, length);
    (*key)[length] = '\0';
    return true;
}

typedef struct {
    char *parts[MAX_DEPTH];
    size_t count;
} Key;

static void free_key(Key *key) {
    for (size_t index = 0; index < key->count; ++index) free(key->parts[index]);
    key->count = 0;
}

static bool parse_key(Parser *parser, Key *key) {
    key->count = 0;
    for (;;) {
        skip_blank(parser);
        if (key->count == MAX_DEPTH) {
            free_key(key);
            return fail(parser, "Key is too deep");
        }
        if (!parse_key_part(parser, &key->parts[key->count])) {
            free_key(key);
            return false;
        }
        ++key->count;
        skip_blank(parser);
        if (peek(parser) != '.') return true;
        ++parser->offset;
    }
}

static bool parse_value(Parser *parser, FusionJson *value, int depth);

static bool digit_run(const char *text, size_t length, int base, bool *ok) {
    bool previous_digit = false;
    for (size_t index = 0; index < length; ++index) {
        char c = text[index];
        int value = c >= '0' && c <= '9' ? c - '0'
                  : c >= 'a' && c <= 'f' ? c - 'a' + 10
                  : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (c == '_') {
            if (!previous_digit || index + 1 == length) return *ok = false;
            previous_digit = false;
            continue;
        }
        if (value < 0 || value >= base) return *ok = false;
        previous_digit = true;
    }
    return *ok = length > 0;
}

static bool parse_number(Parser *parser, FusionJson *value) {
    size_t start = parser->offset;
    while (!at_end(parser) &&
           (is_bare(peek(parser)) || peek(parser) == '+' || peek(parser) == '.' ||
            peek(parser) == ':'))
        ++parser->offset;
    size_t length = parser->offset - start;
    char text[128];
    if (!length || length >= sizeof text) return fail(parser, "Invalid value");
    memcpy(text, parser->text + start, length);
    text[length] = '\0';
    const char *body = text;
    bool negative = false;
    if (*body == '+' || *body == '-') {
        negative = *body == '-';
        ++body;
    }
    if (!strcmp(body, "inf") || !strcmp(body, "nan")) {
        *value = fusion_json_double(!strcmp(body, "nan") ? NAN : negative ? -INFINITY : INFINITY);
        return true;
    }
    int base = 10;
    if (body[0] == '0' && (body[1] == 'x' || body[1] == 'o' || body[1] == 'b') &&
        body == text) {
        base = body[1] == 'x' ? 16 : body[1] == 'o' ? 8 : 2;
        body += 2;
    }
    bool ok = false;
    char clean[128];
    size_t used = 0;
    if (base != 10) {
        if (!digit_run(body, strlen(body), base, &ok)) return fail(parser, "Invalid value");
        for (const char *c = body; *c; ++c) if (*c != '_') clean[used++] = *c;
        clean[used] = '\0';
        errno = 0;
        unsigned long long number = strtoull(clean, NULL, base);
        if (errno || number > INT64_MAX) return fail(parser, "Integer out of range");
        *value = fusion_json_int((int64_t)number);
        return true;
    }
    /* Decimal integer or float: digits with single underscores, an optional
     * fraction and exponent, and no leading zero. */
    const char *cursor = body;
    const char *integer_start = cursor;
    while (*cursor && *cursor != '.' && *cursor != 'e' && *cursor != 'E') ++cursor;
    if (!digit_run(integer_start, (size_t)(cursor - integer_start), 10, &ok))
        return fail(parser, "Invalid value");
    if (integer_start[0] == '0' && cursor - integer_start > 1)
        return fail(parser, "Leading zeros are not allowed");
    bool integral = true;
    if (*cursor == '.') {
        integral = false;
        const char *fraction = ++cursor;
        while (*cursor && *cursor != 'e' && *cursor != 'E') ++cursor;
        if (!digit_run(fraction, (size_t)(cursor - fraction), 10, &ok))
            return fail(parser, "Invalid value");
    }
    if (*cursor == 'e' || *cursor == 'E') {
        integral = false;
        ++cursor;
        if (*cursor == '+' || *cursor == '-') ++cursor;
        if (!digit_run(cursor, strlen(cursor), 10, &ok)) return fail(parser, "Invalid value");
        cursor += strlen(cursor);
    }
    if (*cursor) return fail(parser, "Invalid value");
    for (const char *c = text; *c; ++c) if (*c != '_') clean[used++] = *c;
    clean[used] = '\0';
    if (integral) {
        errno = 0;
        long long number = strtoll(clean, NULL, 10);
        if (errno) return fail(parser, "Integer out of range");
        *value = fusion_json_int(number);
    } else {
        *value = fusion_json_double(strtod(clean, NULL));
    }
    return true;
}

static bool parse_array(Parser *parser, FusionJson *value, int depth) {
    ++parser->offset;
    *value = fusion_json_array();
    for (;;) {
        skip_space_lines(parser);
        if (peek(parser) == ']') {
            ++parser->offset;
            return true;
        }
        FusionJson item = {0};
        if (!parse_value(parser, &item, depth + 1)) return false;
        if (!fusion_json_push(value, item)) return fail(parser, "out of memory");
        skip_space_lines(parser);
        if (peek(parser) == ',') {
            ++parser->offset;
            continue;
        }
        if (peek(parser) == ']') {
            ++parser->offset;
            return true;
        }
        return fail(parser, "Unclosed array");
    }
}

static bool set_path(Parser *parser, FusionJson *table, const Key *key, FusionJson value,
                     size_t from);

static bool parse_inline_table(Parser *parser, FusionJson *value, int depth) {
    ++parser->offset;
    *value = fusion_json_object();
    skip_blank(parser);
    if (peek(parser) == '}') {
        ++parser->offset;
        return true;
    }
    for (;;) {
        Key key;
        if (!parse_key(parser, &key)) return false;
        if (peek(parser) != '=') {
            free_key(&key);
            return fail(parser, "Expected '=' after a key in a key/value pair");
        }
        ++parser->offset;
        skip_blank(parser);
        FusionJson item = {0};
        bool ok = parse_value(parser, &item, depth + 1) && set_path(parser, value, &key, item, 0);
        free_key(&key);
        if (!ok) return false;
        skip_blank(parser);
        if (peek(parser) == '}') {
            ++parser->offset;
            return true;
        }
        if (peek(parser) != ',') return fail(parser, "Unclosed inline table");
        ++parser->offset;
        skip_blank(parser);
    }
}

static bool parse_value(Parser *parser, FusionJson *value, int depth) {
    if (depth > MAX_DEPTH) return fail(parser, "Nesting is too deep");
    char c = peek(parser);
    if (c == '"' || c == '\'') {
        char *text = NULL;
        if (!parse_string(parser, &text)) return false;
        *value = (FusionJson){.kind = FUSION_JSON_STRING, .string = text};
        return true;
    }
    if (c == '[') return parse_array(parser, value, depth);
    if (c == '{') return parse_inline_table(parser, value, depth);
    if (starts(parser, "true") && !is_bare(parser->offset + 4 < parser->size
                                               ? parser->text[parser->offset + 4] : ' ')) {
        parser->offset += 4;
        *value = fusion_json_bool(true);
        return true;
    }
    if (starts(parser, "false") && !is_bare(parser->offset + 5 < parser->size
                                                ? parser->text[parser->offset + 5] : ' ')) {
        parser->offset += 5;
        *value = fusion_json_bool(false);
        return true;
    }
    if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == 'i' || c == 'n')
        return parse_number(parser, value);
    return fail(parser, "Invalid value");
}

/* Sets key[from..] below ``table``, creating intermediate tables. */
static bool set_path(Parser *parser, FusionJson *table, const Key *key, FusionJson value,
                     size_t from) {
    FusionJson *current = table;
    for (size_t index = from; index + 1 < key->count; ++index) {
        FusionJson *next = fusion_json_member_mut(current, key->parts[index]);
        if (!next) {
            if (!fusion_json_set(current, key->parts[index], fusion_json_object())) {
                fusion_json_free(&value);
                return fail(parser, "out of memory");
            }
            next = fusion_json_member_mut(current, key->parts[index]);
        }
        if (next->kind != FUSION_JSON_OBJECT) {
            fusion_json_free(&value);
            return fail(parser, "Cannot overwrite a value");
        }
        current = next;
    }
    if (fusion_json_member(current, key->parts[key->count - 1])) {
        fusion_json_free(&value);
        return fail(parser, "Cannot overwrite a value");
    }
    if (!fusion_json_set(current, key->parts[key->count - 1], value))
        return fail(parser, "out of memory");
    return true;
}

/* The table a [header] or [[header]] selects. */
static bool open_table(Parser *parser, FusionJson *root, const Key *key, bool array,
                       FusionJson **table) {
    FusionJson *current = root;
    for (size_t index = 0; index < key->count; ++index) {
        bool last = index + 1 == key->count;
        FusionJson *next = fusion_json_member_mut(current, key->parts[index]);
        if (!next) {
            FusionJson created = last && array ? fusion_json_array() : fusion_json_object();
            if (!fusion_json_set(current, key->parts[index], created))
                return fail(parser, "out of memory");
            next = fusion_json_member_mut(current, key->parts[index]);
        }
        if (last && array) {
            if (next->kind != FUSION_JSON_ARRAY) return fail(parser, "Cannot overwrite a value");
            if (!fusion_json_push(next, fusion_json_object())) return fail(parser, "out of memory");
            next = &next->items[next->count - 1];
        } else if (next->kind == FUSION_JSON_ARRAY && next->count &&
                   next->items[next->count - 1].kind == FUSION_JSON_OBJECT) {
            next = &next->items[next->count - 1];
        } else if (next->kind != FUSION_JSON_OBJECT) {
            return fail(parser, "Cannot overwrite a value");
        }
        current = next;
    }
    *table = current;
    return true;
}

bool fusion_toml_parse(const char *text, size_t size, FusionJson *root,
                       char *error, size_t error_size) {
    *root = fusion_json_object();
    Parser parser = {text, size, 0, error, error_size};
    FusionJson *table = root;
    bool ok = true;
    while (ok) {
        skip_space_lines(&parser);
        if (at_end(&parser)) break;
        if (peek(&parser) == '[') {
            bool array = starts(&parser, "[[");
            parser.offset += array ? 2 : 1;
            Key key;
            ok = parse_key(&parser, &key);
            if (!ok) break;
            if (!(array ? starts(&parser, "]]") : peek(&parser) == ']')) {
                free_key(&key);
                ok = fail(&parser, "Expected ']' at the end of a table declaration");
                break;
            }
            parser.offset += array ? 2 : 1;
            ok = open_table(&parser, root, &key, array, &table) && end_of_line(&parser);
            free_key(&key);
            continue;
        }
        Key key;
        ok = parse_key(&parser, &key);
        if (!ok) break;
        if (peek(&parser) != '=') {
            free_key(&key);
            ok = fail(&parser, "Expected '=' after a key in a key/value pair");
            break;
        }
        ++parser.offset;
        skip_blank(&parser);
        FusionJson value = {0};
        ok = parse_value(&parser, &value, 0) && set_path(&parser, table, &key, value, 0) &&
             end_of_line(&parser);
        free_key(&key);
    }
    if (!ok) fusion_json_free(root);
    return ok;
}
