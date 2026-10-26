/* SPDX-License-Identifier: GPL-3.0-only */
#include "json.h"

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

static bool fail(Parser *parser, const char *message) {
    if (parser->error && parser->error_size) {
        snprintf(parser->error, parser->error_size, "invalid JSON at byte %zu: %s",
                 parser->offset, message);
        parser->error[parser->error_size - 1] = '\0';
    }
    return false;
}

static void skip_space(Parser *parser) {
    while (parser->offset < parser->size) {
        char c = parser->text[parser->offset];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') return;
        ++parser->offset;
    }
}

static bool literal(Parser *parser, const char *word) {
    size_t length = strlen(word);
    if (parser->size - parser->offset < length ||
        memcmp(parser->text + parser->offset, word, length))
        return fail(parser, "unknown literal");
    parser->offset += length;
    return true;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool parse_string(Parser *parser, char **result) {
    ++parser->offset;  /* opening quote */
    size_t capacity = 16, length = 0;
    char *out = malloc(capacity);
    if (!out) return fail(parser, "out of memory");
    for (;;) {
        if (parser->offset >= parser->size) {
            free(out);
            return fail(parser, "unterminated string");
        }
        unsigned char c = (unsigned char)parser->text[parser->offset++];
        uint32_t code = c;
        if (c == '"') break;
        if (c < 0x20) {
            free(out);
            return fail(parser, "control character in string");
        }
        if (c == '\\') {
            if (parser->offset >= parser->size) {
                free(out);
                return fail(parser, "unterminated escape");
            }
            char escape = parser->text[parser->offset++];
            switch (escape) {
                case '"': code = '"'; break;
                case '\\': code = '\\'; break;
                case '/': code = '/'; break;
                case 'b': code = '\b'; break;
                case 'f': code = '\f'; break;
                case 'n': code = '\n'; break;
                case 'r': code = '\r'; break;
                case 't': code = '\t'; break;
                case 'u': {
                    code = 0;
                    for (int digit = 0; digit < 4; ++digit) {
                        int value = parser->offset < parser->size
                            ? hex_digit(parser->text[parser->offset++]) : -1;
                        if (value < 0) {
                            free(out);
                            return fail(parser, "invalid unicode escape");
                        }
                        code = code * 16 + (uint32_t)value;
                    }
                    if (code >= 0xD800 && code <= 0xDFFF) {
                        free(out);
                        return fail(parser, "surrogate escapes are not supported");
                    }
                    break;
                }
                default:
                    free(out);
                    return fail(parser, "invalid escape");
            }
        }
        unsigned char bytes[4];
        size_t count = 1;
        if (c == '\\' && code >= 0x80) {
            if (code < 0x800) {
                bytes[0] = (unsigned char)(0xC0 | (code >> 6));
                bytes[1] = (unsigned char)(0x80 | (code & 0x3F));
                count = 2;
            } else {
                bytes[0] = (unsigned char)(0xE0 | (code >> 12));
                bytes[1] = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
                bytes[2] = (unsigned char)(0x80 | (code & 0x3F));
                count = 3;
            }
        } else {
            bytes[0] = (unsigned char)code;
        }
        if (length + count + 1 > capacity) {
            capacity *= 2;
            char *grown = realloc(out, capacity);
            if (!grown) {
                free(out);
                return fail(parser, "out of memory");
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

static bool parse_number(Parser *parser, FusionJson *value) {
    size_t start = parser->offset;
    bool integral = true;
    if (parser->text[parser->offset] == '-') ++parser->offset;
    while (parser->offset < parser->size) {
        char c = parser->text[parser->offset];
        if (c >= '0' && c <= '9') {
            ++parser->offset;
        } else if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
            integral = false;
            ++parser->offset;
        } else {
            break;
        }
    }
    char buffer[64];
    size_t length = parser->offset - start;
    if (!length || length >= sizeof buffer) return fail(parser, "invalid number");
    memcpy(buffer, parser->text + start, length);
    buffer[length] = '\0';
    char *end = NULL;
    value->kind = FUSION_JSON_NUMBER;
    value->integral = integral;
    if (integral) {
        value->integer = strtoll(buffer, &end, 10);
    } else {
        value->integer = (int64_t)strtod(buffer, &end);
    }
    if (!end || *end) return fail(parser, "invalid number");
    return true;
}

static bool parse_value(Parser *parser, FusionJson *value, int depth);

static bool append(Parser *parser, FusionJson *container, size_t *capacity,
                   char *key) {
    if (container->count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 8;
        FusionJson *items = realloc(container->items, next * sizeof *items);
        if (!items) {
            free(key);
            return fail(parser, "out of memory");
        }
        container->items = items;
        if (container->kind == FUSION_JSON_OBJECT) {
            char **keys = realloc(container->keys, next * sizeof *keys);
            if (!keys) {
                free(key);
                return fail(parser, "out of memory");
            }
            container->keys = keys;
        }
        *capacity = next;
    }
    container->items[container->count] = (FusionJson){0};
    if (container->kind == FUSION_JSON_OBJECT) container->keys[container->count] = key;
    ++container->count;
    return true;
}

static bool parse_container(Parser *parser, FusionJson *value, int depth, bool object) {
    value->kind = object ? FUSION_JSON_OBJECT : FUSION_JSON_ARRAY;
    ++parser->offset;
    size_t capacity = 0;
    skip_space(parser);
    if (parser->offset < parser->size &&
        parser->text[parser->offset] == (object ? '}' : ']')) {
        ++parser->offset;
        return true;
    }
    for (;;) {
        char *key = NULL;
        skip_space(parser);
        if (object) {
            if (parser->offset >= parser->size || parser->text[parser->offset] != '"')
                return fail(parser, "expected a member name");
            if (!parse_string(parser, &key)) return false;
            skip_space(parser);
            if (parser->offset >= parser->size || parser->text[parser->offset] != ':') {
                free(key);
                return fail(parser, "expected ':'");
            }
            ++parser->offset;
        }
        if (!append(parser, value, &capacity, key) ||
            !parse_value(parser, &value->items[value->count - 1], depth + 1))
            return false;
        skip_space(parser);
        if (parser->offset >= parser->size) return fail(parser, "unterminated container");
        char c = parser->text[parser->offset++];
        if (c == ',') continue;
        if (c == (object ? '}' : ']')) return true;
        return fail(parser, "expected ',' or the container end");
    }
}

static bool parse_value(Parser *parser, FusionJson *value, int depth) {
    if (depth > MAX_DEPTH) return fail(parser, "nesting too deep");
    skip_space(parser);
    if (parser->offset >= parser->size) return fail(parser, "unexpected end");
    char c = parser->text[parser->offset];
    if (c == '{') return parse_container(parser, value, depth, true);
    if (c == '[') return parse_container(parser, value, depth, false);
    if (c == '"') {
        value->kind = FUSION_JSON_STRING;
        return parse_string(parser, &value->string);
    }
    if (c == 't') {
        value->kind = FUSION_JSON_BOOL;
        value->boolean = true;
        return literal(parser, "true");
    }
    if (c == 'f') {
        value->kind = FUSION_JSON_BOOL;
        return literal(parser, "false");
    }
    if (c == 'n') {
        value->kind = FUSION_JSON_NULL;
        return literal(parser, "null");
    }
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(parser, value);
    return fail(parser, "unexpected character");
}

bool fusion_json_parse(const char *text, size_t size, FusionJson *root,
                       char *error, size_t error_size) {
    if (!text || !root) return false;
    *root = (FusionJson){0};
    Parser parser = {text, size, 0, error, error_size};
    if (!parse_value(&parser, root, 0)) {
        fusion_json_free(root);
        return false;
    }
    skip_space(&parser);
    if (parser.offset != size) {
        fusion_json_free(root);
        return fail(&parser, "trailing data");
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
    if (!object || object->kind != FUSION_JSON_OBJECT) return NULL;
    for (size_t index = 0; index < object->count; ++index)
        if (!strcmp(object->keys[index], key)) return &object->items[index];
    return NULL;
}
