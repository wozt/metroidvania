/* SPDX-License-Identifier: MIT */
/* Small strict JSON reader: no nonstandard comments, no extra libraries. */
#include "config.h"
#include "expression.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSON_MAX_BYTES 65536
#define JSON_MAX_TOKENS 600
#define JSON_MAX_DEPTH 24

enum { T_OBJECT = 1, T_ARRAY, T_STRING, T_NUMBER, T_TRUE, T_FALSE, T_NULL };

typedef struct { int kind, start, end, next; } Token;
typedef struct {
    const char *s;
    int pos, total, count;
    Token tokens[JSON_MAX_TOKENS];
    bool ok;
} Json;

static void set_error(char *error, size_t size, const char *message) {
    if (error && size) snprintf(error, size, "%s", message);
}

static void ws(Json *j) {
    while (j->pos < j->total && isspace((unsigned char)j->s[j->pos])) ++j->pos;
}

static bool take(Json *j, char ch) {
    ws(j);
    if (j->pos < j->total && j->s[j->pos] == ch) { ++j->pos; return true; }
    return false;
}

static int scan_value(Json *j, int depth);

static int new_token(Json *j, int kind, int start) {
    if (!j->ok || j->count >= JSON_MAX_TOKENS) { j->ok = false; return -1; }
    int i = j->count++;
    j->tokens[i] = (Token){kind, start, start, -1};
    return i;
}

static int scan_string(Json *j) {
    if (!take(j, '"')) { j->ok = false; return -1; }
    int idx = new_token(j, T_STRING, j->pos);
    if (idx < 0) return -1;
    while (j->pos < j->total) {
        unsigned char ch = (unsigned char)j->s[j->pos++];
        if (ch == '"') {
            j->tokens[idx].end = j->pos - 1;
            j->tokens[idx].next = j->count;
            return idx;
        }
        if (ch < 32) break;
        if (ch == '\\') {
            if (j->pos >= j->total) break;
            char esc = j->s[j->pos++];
            if (strchr("\"\\/bfnrt", esc)) continue;
            if (esc != 'u' || j->total - j->pos < 4) break;
            bool valid = true;
            for (int k = 0; k < 4; ++k)
                if (!isxdigit((unsigned char)j->s[j->pos + k])) valid = false;
            if (!valid) break;
            j->pos += 4;
        }
    }
    j->ok = false;
    return -1;
}

static int scan_number(Json *j) {
    int start = j->pos;
    if (j->s[j->pos] == '-') ++j->pos;
    if (j->pos >= j->total) { j->ok = false; return -1; }
    if (j->s[j->pos] == '0') ++j->pos;
    else if (j->s[j->pos] >= '1' && j->s[j->pos] <= '9') {
        do { ++j->pos; } while (j->pos < j->total && isdigit((unsigned char)j->s[j->pos]));
    } else { j->ok = false; return -1; }
    if (j->pos < j->total && j->s[j->pos] == '.') {
        ++j->pos;
        if (j->pos >= j->total || !isdigit((unsigned char)j->s[j->pos])) j->ok = false;
        while (j->pos < j->total && isdigit((unsigned char)j->s[j->pos])) ++j->pos;
    }
    if (j->pos < j->total && (j->s[j->pos] == 'e' || j->s[j->pos] == 'E')) {
        ++j->pos;
        if (j->pos < j->total && (j->s[j->pos] == '+' || j->s[j->pos] == '-')) ++j->pos;
        if (j->pos >= j->total || !isdigit((unsigned char)j->s[j->pos])) j->ok = false;
        while (j->pos < j->total && isdigit((unsigned char)j->s[j->pos])) ++j->pos;
    }
    int idx = new_token(j, T_NUMBER, start);
    if (idx >= 0) { j->tokens[idx].end = j->pos; j->tokens[idx].next = j->count; }
    return idx;
}

static int scan_value(Json *j, int depth) {
    ws(j);
    if (!j->ok || depth > JSON_MAX_DEPTH || j->pos >= j->total) {
        j->ok = false; return -1;
    }
    char ch = j->s[j->pos];
    if (ch == '"') return scan_string(j);
    if (ch == '-' || isdigit((unsigned char)ch)) return scan_number(j);
    if (ch == '{' || ch == '[') {
        int kind = ch == '{' ? T_OBJECT : T_ARRAY;
        int idx = new_token(j, kind, j->pos);
        ++j->pos;
        if (idx < 0) return -1;
        char close = kind == T_OBJECT ? '}' : ']';
        if (!take(j, close)) {
            for (;;) {
                if (kind == T_OBJECT) {
                    if (scan_string(j) < 0 || !take(j, ':')) { j->ok = false; break; }
                }
                if (scan_value(j, depth + 1) < 0) { j->ok = false; break; }
                if (take(j, close)) break;
                if (!take(j, ',')) { j->ok = false; break; }
            }
        }
        j->tokens[idx].end = j->pos;
        j->tokens[idx].next = j->count;
        return j->ok ? idx : -1;
    }
    const char *literal = NULL;
    int kind = 0;
    if (ch == 't') { literal = "true"; kind = T_TRUE; }
    else if (ch == 'f') { literal = "false"; kind = T_FALSE; }
    else if (ch == 'n') { literal = "null"; kind = T_NULL; }
    if (!literal || (size_t)(j->total - j->pos) < strlen(literal) ||
        strncmp(j->s + j->pos, literal, strlen(literal))) {
        j->ok = false; return -1;
    }
    int idx = new_token(j, kind, j->pos);
    j->pos += (int)strlen(literal);
    if (idx >= 0) { j->tokens[idx].end = j->pos; j->tokens[idx].next = j->count; }
    return idx;
}

static bool str_token(const Json *j, int idx, char *out, size_t cap) {
    if (idx < 0 || j->tokens[idx].kind != T_STRING || !cap) return false;
    size_t n = 0;
    const Token *tok = &j->tokens[idx];
    for (int i = tok->start; i < tok->end;) {
        unsigned char c = (unsigned char)j->s[i++];
        if (c == '\\') {
            if (i >= tok->end) return false;
            char esc = j->s[i++];
            if (esc == 'u') return false; /* UTF-8 names/expressions not needed. */
            if (esc == 'n') c = '\n';
            else if (esc == 'r') c = '\r';
            else if (esc == 't') c = '\t';
            else if (esc == 'b') c = '\b';
            else if (esc == 'f') c = '\f';
            else c = (unsigned char)esc;
        }
        if (n + 1 >= cap) return false;
        out[n++] = (char)c;
    }
    out[n] = '\0';
    return true;
}

static bool key_is(const Json *j, int idx, const char *key) {
    char buffer[64];
    return str_token(j, idx, buffer, sizeof buffer) && !strcmp(buffer, key);
}

static bool scalar(const Json *j, int idx, float *out, float min, float max) {
    if (idx < 0 || j->tokens[idx].kind != T_NUMBER) return false;
    const Token *tok = &j->tokens[idx];
    if (tok->end - tok->start >= 64) return false;
    char text[64];
    memcpy(text, j->s + tok->start, (size_t)(tok->end - tok->start));
    text[tok->end - tok->start] = '\0';
    char *end = NULL;
    errno = 0;
    float v = strtof(text, &end);
    if (errno == ERANGE || *end || !isfinite(v) || v < min || v > max) return false;
    *out = v;
    return true;
}

static int hexdigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool color_token(const Json *j, int idx, unsigned char dst[4]) {
    char text[12];
    if (!str_token(j, idx, text, sizeof text)) return false;
    size_t n = strlen(text);
    if (text[0] != '#' || (n != 7 && n != 9)) return false;
    for (int k = 0; k < 4; ++k) {
        if (k == 3 && n == 7) { dst[k] = 255; continue; }
        int high = hexdigit(text[1 + 2 * k]), low = hexdigit(text[2 + 2 * k]);
        if (high < 0 || low < 0) return false;
        dst[k] = (unsigned char)((high << 4) | low);
    }
    return true;
}

static bool easing_json(const Json *j, int idx, ProtoEasing *easing) {
    if (idx < 0 || j->tokens[idx].kind != T_STRING) return false;
    char name[32];
    if (!str_token(j, idx, name, sizeof name)) return false;
    if (!strcmp(name, "linear")) { *easing = PROTO_EASE_LINEAR; return true; }
    if (!strcmp(name, "smoothstep")) { *easing = PROTO_EASE_SMOOTHSTEP; return true; }
    if (!strcmp(name, "quad_in")) { *easing = PROTO_EASE_QUAD_IN; return true; }
    if (!strcmp(name, "quad_out")) { *easing = PROTO_EASE_QUAD_OUT; return true; }
    if (!strcmp(name, "quad_in_out")) { *easing = PROTO_EASE_QUAD_IN_OUT; return true; }
    if (!strcmp(name, "cubic_in")) { *easing = PROTO_EASE_CUBIC_IN; return true; }
    if (!strcmp(name, "cubic_out")) { *easing = PROTO_EASE_CUBIC_OUT; return true; }
    if (!strcmp(name, "cubic_in_out")) { *easing = PROTO_EASE_CUBIC_IN_OUT; return true; }
    return false;
}

static bool transition_json(const Json *j, int idx, ProtoTransition *t) {
    if (idx < 0 || j->tokens[idx].kind != T_OBJECT) return false;
    unsigned seen = 0;
    t->expression[0] = '\0';  /* Clear expression by default */
    t->morph_duration = 0.55f;
    for (int key = idx + 1; key < j->tokens[idx].next;) {
        int value = key + 1;
        unsigned bit = 0;
        if (key_is(j, key, "formula")) {
            bit = 1;
            char name[PROTO_EXPRESSION_LENGTH];
            if (!str_token(j, value, name, sizeof name)) return false;
            if (proto_parse_formula(name, &t->formula)) {
                t->expression[0] = '\0';
            } else {
                const char *expr = !strncmp(name, "expr:", 5) ? name + 5 : name;
                if (!proto_expression_valid(expr)) return false;
                t->formula = PROTO_CUSTOM;
                snprintf(t->expression, sizeof t->expression, "%s", expr);
            }
        } else if (key_is(j, key, "duration")) {
            bit = 2;
            if (!scalar(j, value, &t->duration, 0.1f, 60.0f)) return false;
        } else if (key_is(j, key, "morph_duration")) {
            bit = 8;
            if (!scalar(j, value, &t->morph_duration, 0.1f, 60.0f)) return false;
        } else if (key_is(j, key, "easing")) {
            bit = 4;
            if (!easing_json(j, value, &t->easing)) return false;
        }
        if (!bit || (seen & bit)) return false;
        seen |= bit;
        key = j->tokens[value].next;
    }
    return (seen & 1) != 0 && (seen & 2) != 0;
}

static bool section_json(const Json *j, int idx, ProtoSection *s) {
    if (idx < 0 || j->tokens[idx].kind != T_OBJECT) return false;
    unsigned seen = 0;
    for (int key = idx + 1; key < j->tokens[idx].next;) {
        int value = key + 1;
        unsigned bit = 0;
        if (key_is(j, key, "formula")) {
            bit = 1;
            char name[PROTO_EXPRESSION_LENGTH];
            if (!str_token(j, value, name, sizeof name)) return false;
            if (proto_parse_formula(name, &s->formula)) s->expression[0] = '\0';
            else {
                const char *expr = !strncmp(name, "expr:", 5) ? name + 5 : name;
                if (!proto_expression_valid(expr)) return false;
                s->formula = PROTO_CUSTOM;
                snprintf(s->expression, sizeof s->expression, "%s", expr);
            }
        } else if (key_is(j, key, "amplitude")) {
            bit = 2;
            if (!scalar(j, value, &s->amplitude, 0.0f, 48.0f)) return false;
        } else if (key_is(j, key, "cycles")) {
            bit = 4;
            if (!scalar(j, value, &s->cycles, 0.25f, 12.0f)) return false;
        } else if (key_is(j, key, "phase_speed")) {
            bit = 8;
            if (!scalar(j, value, &s->phase_speed, -4.0f, 4.0f)) return false;
        } else if (key_is(j, key, "gap_pixels")) {
            bit = 128;
            if (!scalar(j, value, &s->gap_pixels, 0.0f, 5.0f)) return false;
        } else if (key_is(j, key, "repeat_visible")) {
            bit = 256;
            if (j->tokens[value].kind != T_TRUE && j->tokens[value].kind != T_FALSE) return false;
            s->repeat_visible = j->tokens[value].kind == T_TRUE;
        } else if (key_is(j, key, "ribbon_width")) {
            bit = 1024;
            if (!scalar(j, value, &s->ribbon_width, 0.5f, 12.0f)) return false;
        } else if (key_is(j, key, "spiral_scale")) {
            bit = 512;
            if (!scalar(j, value, &s->spiral_scale, 0.35f, 1.0f)) return false;
        } else if (key_is(j, key, "spiral_layout")) {
            bit = 2048;
            float value32 = 0.0f;
            if (!scalar(j, value, &value32, 1.0f, 4.0f) ||
                (float)(int)value32 != value32 || (seen & bit)) return false;
            s->spiral_layout = (int)value32;
            seen |= bit;
        } else if (key_is(j, key, "stagger_delay")) {
            bit = 4096;
            if ((seen & bit) || !scalar(j, value, &s->stagger_delay, 0.0f, 2.0f)) return false;
            seen |= bit;
        } else if (key_is(j, key, "animated")) {
            bit = 16;
            s->animated = j->tokens[value].kind == T_TRUE;
        } else if (key_is(j, key, "loop")) {
            bit = 32;
            s->loop = j->tokens[value].kind == T_TRUE;
        } else if (key_is(j, key, "transitions")) {
            bit = 64;
            if (j->tokens[value].kind != T_ARRAY) return false;
            int count = 0;
            for (int i = value + 1; i < j->tokens[value].next; i = j->tokens[i].next) {
                if (count >= PROTO_MAX_TRANSITIONS || !transition_json(j, i, &s->transitions[count])) return false;
                ++count;
            }
            s->transition_count = count;
        }
        if (!bit) return false;
        if (bit & 15) {  /* Required fields: formula, amplitude, cycles, phase_speed */
            if (seen & bit) return false;
            seen |= bit;
        }
        key = j->tokens[value].next;
    }
    return (seen & 1) != 0;
}

static bool colors_json(const Json *j, int idx, ProtoConfig *config) {
    if (idx < 0 || j->tokens[idx].kind != T_OBJECT) return false;
    unsigned seen = 0;
    for (int key = idx + 1; key < j->tokens[idx].next;) {
        int value = key + 1;
        unsigned bit = 0;
        unsigned char *dst = NULL;
        if (key_is(j, key, "violet")) { bit = 1; dst = config->violet; }
        else if (key_is(j, key, "cyan")) { bit = 2; dst = config->cyan; }
        else if (key_is(j, key, "divider")) { bit = 4; dst = config->divider_color; }
        if (!bit || (seen & bit) || !color_token(j, value, dst)) return false;
        seen |= bit;
        key = j->tokens[value].next;
    }
    return true;
}

static bool sections_json(const Json *j, int idx, ProtoConfig *config, int *length) {
    if (idx < 0 || j->tokens[idx].kind != T_ARRAY) return false;
    int count = 0;
    for (int i = idx + 1; i < j->tokens[idx].next; i = j->tokens[i].next) {
        if (count >= PROTO_MAX_SECTIONS || !section_json(j, i, &config->sections[count])) return false;
        ++count;
    }
    *length = count;
    return true;
}

bool proto_config_load(const char *path, ProtoConfig *config, char *error, size_t size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        char message[256];
        snprintf(message, sizeof message, "Cannot open %s: %s", path, strerror(errno));
        set_error(error, size, message);
        return false;
    }
    char *source = malloc(JSON_MAX_BYTES + 1);
    if (!source) { fclose(file); set_error(error, size, "Out of memory"); return false; }
    size_t n = fread(source, 1, JSON_MAX_BYTES + 1, file);
    bool read_ok = !ferror(file) && n <= JSON_MAX_BYTES;
    if (fclose(file) != 0) read_ok = false;
    if (!read_ok) {
        free(source);
        set_error(error, size, "Cannot read JSON or file exceeds 64 KiB");
        return false;
    }
    source[n] = '\0';
    Json *j = calloc(1, sizeof *j);
    if (!j) { free(source); set_error(error, size, "Out of memory"); return false; }
    j->s = source;
    j->total = (int)n;
    j->ok = true;
    int root = scan_value(j, 0);
    ws(j);
    bool valid = j->ok && root >= 0 && j->pos == j->total &&
                 j->tokens[root].kind == T_OBJECT;
    ProtoConfig candidate = *config;
    unsigned seen = 0;
    int sections_available = -1;
    if (valid) for (int key = root + 1; key < j->tokens[root].next;) {
        int value = key + 1;
        unsigned bit = 0;
        if (key_is(j, key, "section_count")) {
            bit = 1;
            float number;
            valid = scalar(j, value, &number, 1.0f, 16.0f) &&
                    proto_valid_section_count((int)number) && (float)(int)number == number;
            if (valid) candidate.section_count = (int)number;
        } else if (key_is(j, key, "rotation_speed")) {
            bit = 2;
            valid = scalar(j, value, &candidate.rotation_speed, -360.0f, 360.0f);
        } else if (key_is(j, key, "line_width")) {
            bit = 4;
            valid = scalar(j, value, &candidate.line_width, 0.0f, 4.0f);
        } else if (key_is(j, key, "show_divider")) {
            bit = 8;
            valid = j->tokens[value].kind == T_TRUE || j->tokens[value].kind == T_FALSE;
            if (valid) candidate.show_divider = j->tokens[value].kind == T_TRUE;
        } else if (key_is(j, key, "colors")) {
            bit = 16;
            valid = colors_json(j, value, &candidate);
        } else if (key_is(j, key, "sections")) {
            bit = 32;
            valid = sections_json(j, value, &candidate, &sections_available);
        }
        if (!bit || (seen & bit)) valid = false;
        if (!valid) break;
        seen |= bit;
        key = j->tokens[value].next;
    }
    if (valid && sections_available >= 0 && sections_available < candidate.section_count)
        valid = false;
    free(j);
    free(source);
    if (!valid) {
        char message[256];
        snprintf(message, sizeof message,
                 "Invalid JSON settings in %s (check fields, ranges and formulas)", path);
        set_error(error, size, message);
        return false;
    }
    *config = candidate;
    set_error(error, size, "");
    return true;
}

static int write_string(FILE *file, const char *s) {
    if (fputc('"', file) == EOF) return -1;
    for (; *s; ++s) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\') {
            if (fputc('\\', file) == EOF || fputc(ch, file) == EOF) return -1;
        } else if (ch < 32) {
            if (fprintf(file, "\\u%04x", ch) < 0) return -1;
        } else if (fputc(ch, file) == EOF) return -1;
    }
    return fputc('"', file) == EOF ? -1 : 0;
}

static const char *easing_name(ProtoEasing easing) {
    switch (easing) {
        case PROTO_EASE_LINEAR: return "linear";
        case PROTO_EASE_SMOOTHSTEP: return "smoothstep";
        case PROTO_EASE_QUAD_IN: return "quad_in";
        case PROTO_EASE_QUAD_OUT: return "quad_out";
        case PROTO_EASE_QUAD_IN_OUT: return "quad_in_out";
        case PROTO_EASE_CUBIC_IN: return "cubic_in";
        case PROTO_EASE_CUBIC_OUT: return "cubic_out";
        case PROTO_EASE_CUBIC_IN_OUT: return "cubic_in_out";
        default: return "linear";
    }
}

bool proto_config_save(const char *path, const ProtoConfig *config, char *error, size_t size) {
    if (!proto_valid_section_count(config->section_count)) {
        set_error(error, size, "Invalid section count");
        return false;
    }
    size_t len = strlen(path);
    char *temporary = malloc(len + 5);
    if (!temporary) { set_error(error, size, "Out of memory"); return false; }
    snprintf(temporary, len + 5, "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    if (!file) { free(temporary); set_error(error, size, "Cannot write JSON file"); return false; }
    char violet[10], cyan[10], divider[10];
    const unsigned char *colors[] = {config->violet, config->cyan, config->divider_color};
    char *texts[] = {violet, cyan, divider};
    for (int i = 0; i < 3; ++i) {
        const unsigned char *c = colors[i];
        if (c[3] == 255) snprintf(texts[i], 10, "#%02X%02X%02X", c[0], c[1], c[2]);
        else snprintf(texts[i], 10, "#%02X%02X%02X%02X", c[0], c[1], c[2], c[3]);
    }
    bool ok = fprintf(file, "{\n  \"section_count\": %d,\n"
                            "  \"rotation_speed\": %.4f,\n"
                            "  \"line_width\": %.3f,\n"
                            "  \"show_divider\": %s,\n"
                            "  \"colors\": {\n"
                            "    \"violet\": \"%s\",\n"
                            "    \"cyan\": \"%s\",\n"
                            "    \"divider\": \"%s\"\n"
                            "  },\n  \"sections\": [\n",
                      config->section_count, config->rotation_speed,
                      config->line_width, config->show_divider ? "true" : "false",
                      violet, cyan, divider) >= 0;
    for (int i = 0; ok && i < PROTO_MAX_SECTIONS; ++i) {
        const ProtoSection *s = &config->sections[i];
        const char *name = s->formula == PROTO_CUSTOM ? s->expression : proto_formula_name(s->formula);
        ok = fprintf(file, "    {\"formula\": ") >= 0 && write_string(file, name) == 0 &&
             fprintf(file, ", \"amplitude\": %.3f, \"cycles\": %.3f, "
                           "\"phase_speed\": %.3f",
                     s->amplitude, s->cycles, s->phase_speed) >= 0;
        if (ok) ok = fprintf(file, ", \"gap_pixels\": %.3f, \"repeat_visible\": %s",
                             s->gap_pixels, s->repeat_visible ? "true" : "false") >= 0;
        if (ok) ok = fprintf(file, ", \"spiral_scale\": %.3f",
                          s->spiral_scale > 0.0f ? s->spiral_scale : 1.0f) >= 0;
        if (ok) ok = fprintf(file, ", \"ribbon_width\": %.3f",
                             s->ribbon_width > 0.0f ? s->ribbon_width : 7.0f) >= 0;
        if (ok && s->spiral_layout > 0)
            ok = fprintf(file, ", \"spiral_layout\": %d", s->spiral_layout) >= 0;
        if (ok && s->stagger_delay > 0.0f)
            ok = fprintf(file, ", \"stagger_delay\": %.3f", s->stagger_delay) >= 0;
        if (ok && s->animated) {
            ok = fprintf(file, ", \"animated\": true") >= 0;
            if (ok && s->loop) ok = fprintf(file, ", \"loop\": true") >= 0;
            if (ok && s->transition_count > 0) {
                ok = fprintf(file, ", \"transitions\": [") >= 0;
                for (int t = 0; ok && t < s->transition_count; ++t) {
                    const ProtoTransition *trans = &s->transitions[t];
                    const char *name = trans->formula == PROTO_CUSTOM ? trans->expression : proto_formula_name(trans->formula);
                    ok = fprintf(file, "%s{\"formula\": ", t == 0 ? "" : ", ") >= 0 &&
                         write_string(file, name) == 0 &&
                         fprintf(file, ", \"duration\": %.3f, \"morph_duration\": %.3f, \"easing\": \"%s\"}",
                                 trans->duration, trans->morph_duration > 0.0f ? trans->morph_duration : 0.55f,
                                 easing_name(trans->easing)) >= 0;
                }
                if (ok) ok = fprintf(file, "]") >= 0;
            }
        }
        if (ok) ok = fprintf(file, "}%s\n", i + 1 == PROTO_MAX_SECTIONS ? "" : ",") >= 0;
    }
    if (ok) ok = fprintf(file, "  ]\n}\n") >= 0;
    if (fclose(file) != 0) ok = false;
    if (ok) ok = rename(temporary, path) == 0;
    if (!ok) remove(temporary);
    free(temporary);
    set_error(error, size, ok ? "" : "Error saving JSON configuration");
    return ok;
}
