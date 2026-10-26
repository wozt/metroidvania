/* SPDX-License-Identifier: GPL-3.0-only */
/* The deterministic native-source inventory of the pinned decompilations
 * (formerly scripts/native_inventory.py): routines, header declarations,
 * data symbols, types and constants with their lexical relations, written
 * as a manifest and checksummed record shards. */
#define _POSIX_C_SOURCE 200809L
#include "fusion/pipeline/inventory.h"

#include "core/json.h"
#include "fusion/assets/rom.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define COUNT(array) (sizeof (array) / sizeof (array)[0])

enum { MAX_SHARD_BYTES = 3000000 };

static const char inventory_schema[] = "metroidvania-native-inventory-v6";
static const char record_schema[] = "metroidvania-native-inventory-records-v6";
static const char *const collections[] = {"routines", "declarations", "data_symbols", "types",
                                          "constants"};
static const char *const control_words[] = {"if", "for", "while", "switch", "return", "sizeof"};
static const char *const type_words[] = {
    "const", "volatile", "static", "extern", "register", "signed", "unsigned", "struct",
    "union", "enum", "void", "char", "short", "int", "long", "float", "double"};
static const char *const mzm_bosses[] = {
    "acid_worm", "boss_statues", "chozo_statue", "crocomire", "deorem", "imago",
    "imago_cocoon", "kraid", "mecha_ridley", "mother_brain", "ridley", "ruins_test"};

static char *failure_text;
static size_t failure_size;

static bool fail(const char *format, ...) {
    if (failure_text && failure_size) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(failure_text, failure_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

static void oom(void) {
    fputs("fusion_inventory: out of memory\n", stderr);
    exit(2);
}

static void *grow(void *items, size_t *capacity, size_t count, size_t size) {
    if (count < *capacity) return items;
    *capacity = *capacity ? *capacity * 2 : 16;
    void *grown = realloc(items, *capacity * size);
    if (!grown) oom();
    return grown;
}

static char *copy_n(const char *text, size_t length) {
    char *copy = malloc(length + 1);
    if (!copy) oom();
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static char *copy_text(const char *text) {
    return copy_n(text, strlen(text));
}

static char *format(const char *pattern, ...) {
    va_list arguments;
    va_start(arguments, pattern);
    int length = vsnprintf(NULL, 0, pattern, arguments);
    va_end(arguments);
    char *text = malloc((size_t)length + 1);
    if (!text) oom();
    va_start(arguments, pattern);
    vsnprintf(text, (size_t)length + 1, pattern, arguments);
    va_end(arguments);
    return text;
}

/* ---- Strings and pairs ------------------------------------------------- */

typedef struct {
    char **items;
    size_t count, capacity;
} List;

typedef struct {
    char *first, *second;
} Pair;

typedef struct {
    Pair *items;
    size_t count, capacity;
} Pairs;

static void list_add(List *list, const char *value) {
    list->items = grow(list->items, &list->capacity, list->count, sizeof *list->items);
    list->items[list->count++] = copy_text(value);
}

static void list_add_n(List *list, const char *value, size_t length) {
    list->items = grow(list->items, &list->capacity, list->count, sizeof *list->items);
    list->items[list->count++] = copy_n(value, length);
}

static void list_free(List *list) {
    for (size_t index = 0; index < list->count; ++index) free(list->items[index]);
    free(list->items);
    *list = (List){0};
}

static int compare_strings(const void *left, const void *right) {
    return strcmp(*(char *const *)left, *(char *const *)right);
}

/* sorted(set(list)) in place. */
static void list_unique(List *list) {
    if (list->count) qsort(list->items, list->count, sizeof *list->items, compare_strings);
    size_t kept = 0;
    for (size_t index = 0; index < list->count; ++index) {
        if (kept && !strcmp(list->items[kept - 1], list->items[index])) {
            free(list->items[index]);
            continue;
        }
        list->items[kept++] = list->items[index];
    }
    list->count = kept;
}

static bool list_has(const List *list, const char *value) {
    for (size_t index = 0; index < list->count; ++index)
        if (!strcmp(list->items[index], value)) return true;
    return false;
}

static void pairs_add(Pairs *pairs, const char *first, size_t first_length, const char *second,
                      size_t second_length) {
    pairs->items = grow(pairs->items, &pairs->capacity, pairs->count, sizeof *pairs->items);
    pairs->items[pairs->count++] = (Pair){copy_n(first, first_length), copy_n(second, second_length)};
}

static int compare_pairs(const void *left, const void *right) {
    const Pair *a = left, *b = right;
    int order = strcmp(a->first, b->first);
    return order ? order : strcmp(a->second, b->second);
}

static void pairs_unique(Pairs *pairs) {
    if (pairs->count) qsort(pairs->items, pairs->count, sizeof *pairs->items, compare_pairs);
    size_t kept = 0;
    for (size_t index = 0; index < pairs->count; ++index) {
        if (kept && !compare_pairs(&pairs->items[kept - 1], &pairs->items[index])) {
            free(pairs->items[index].first);
            free(pairs->items[index].second);
            continue;
        }
        pairs->items[kept++] = pairs->items[index];
    }
    pairs->count = kept;
}

static void pairs_free(Pairs *pairs) {
    for (size_t index = 0; index < pairs->count; ++index) {
        free(pairs->items[index].first);
        free(pairs->items[index].second);
    }
    free(pairs->items);
    *pairs = (Pairs){0};
}

static bool in_words(const char *start, size_t length, const char *const *words, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (strlen(words[index]) == length && !memcmp(words[index], start, length)) return true;
    return false;
}

static bool control_word(const char *start, size_t length) {
    return in_words(start, length, control_words, COUNT(control_words));
}

static bool type_word(const char *start, size_t length) {
    return in_words(start, length, type_words, COUNT(type_words));
}

/* ---- A string hash map of index lists ---------------------------------- */

typedef struct {
    char *key;
    size_t *values;
    size_t count, capacity;
} Slot;

typedef struct {
    Slot *slots;
    size_t capacity, used;
} Map;

static uint64_t hash(const char *key) {
    uint64_t value = 1469598103934665603u;
    for (; *key; ++key) value = (value ^ (unsigned char)*key) * 1099511628211u;
    return value;
}

static Slot *map_slot(Map *map, const char *key, bool create) {
    if (!map->capacity) {
        if (!create) return NULL;
        map->capacity = 1024;
        map->slots = calloc(map->capacity, sizeof *map->slots);
        if (!map->slots) oom();
    }
    if (create && (map->used + 1) * 2 > map->capacity) {
        Map bigger = {calloc(map->capacity * 2, sizeof *map->slots), map->capacity * 2, 0};
        if (!bigger.slots) oom();
        for (size_t index = 0; index < map->capacity; ++index) {
            Slot *slot = &map->slots[index];
            if (!slot->key) continue;
            size_t at = hash(slot->key) & (bigger.capacity - 1);
            while (bigger.slots[at].key) at = (at + 1) & (bigger.capacity - 1);
            bigger.slots[at] = *slot;
            ++bigger.used;
        }
        free(map->slots);
        *map = bigger;
    }
    size_t at = hash(key) & (map->capacity - 1);
    while (map->slots[at].key) {
        if (!strcmp(map->slots[at].key, key)) return &map->slots[at];
        at = (at + 1) & (map->capacity - 1);
    }
    if (!create) return NULL;
    map->slots[at].key = copy_text(key);
    ++map->used;
    return &map->slots[at];
}

static void map_add(Map *map, const char *key, size_t value) {
    Slot *slot = map_slot(map, key, true);
    slot->values = grow(slot->values, &slot->capacity, slot->count, sizeof *slot->values);
    slot->values[slot->count++] = value;
}

static void map_free(Map *map) {
    for (size_t index = 0; index < map->capacity; ++index) {
        free(map->slots[index].key);
        free(map->slots[index].values);
    }
    free(map->slots);
    *map = (Map){0};
}

/* ---- Source text ------------------------------------------------------- */

typedef struct {
    char *text;   /* decoded UTF-8 with universal newlines */
    size_t size;
    char *raw;    /* the file bytes */
    size_t raw_size;
} Source;

/* read_text(encoding="utf-8", errors="replace") with newline translation. */
static bool load_source(const char *path, Source *source) {
    FILE *file = fopen(path, "rb");
    if (!file) return fail("cannot read %s", path);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);
    source->raw = malloc((size_t)length + 1);
    if (!source->raw) oom();
    source->raw_size = fread(source->raw, 1, (size_t)length, file);
    fclose(file);
    source->text = malloc(source->raw_size * 3 + 1);
    if (!source->text) oom();
    size_t used = 0;
    const unsigned char *bytes = (const unsigned char *)source->raw;
    for (size_t index = 0; index < source->raw_size;) {
        unsigned char c = bytes[index];
        if (c == '\r') {
            source->text[used++] = '\n';
            index += index + 1 < source->raw_size && bytes[index + 1] == '\n' ? 2 : 1;
            continue;
        }
        if (c < 0x80) {
            source->text[used++] = (char)c;
            ++index;
            continue;
        }
        size_t length_needed = c >= 0xF0 && c <= 0xF4 ? 4 : c >= 0xE0 ? 3 : c >= 0xC2 && c < 0xE0 ? 2 : 0;
        bool valid = length_needed && index + length_needed <= source->raw_size;
        for (size_t extra = 1; valid && extra < length_needed; ++extra)
            valid = (bytes[index + extra] & 0xC0) == 0x80;
        if (valid && length_needed == 3) {
            unsigned code = ((c & 0x0F) << 12) | ((bytes[index + 1] & 0x3F) << 6);
            valid = code >= 0x800 && (code < 0xD800 || code > 0xDFFF);
        }
        if (valid && length_needed == 4) {
            unsigned code = ((c & 0x07) << 18) | ((bytes[index + 1] & 0x3F) << 12);
            valid = code >= 0x10000 && code <= 0x10FFFF;
        }
        if (valid) {
            memcpy(source->text + used, bytes + index, length_needed);
            used += length_needed;
            index += length_needed;
        } else {
            memcpy(source->text + used, "\xEF\xBF\xBD", 3);
            used += 3;
            ++index;
        }
    }
    source->text[used] = '\0';
    source->size = used;
    return true;
}

static void source_free(Source *source) {
    free(source->text);
    free(source->raw);
    *source = (Source){0};
}

static size_t code_points(const char *text, size_t end) {
    size_t count = 0;
    for (size_t index = 0; index < end; ++index)
        if (((unsigned char)text[index] & 0xC0) != 0x80) ++count;
    return count;
}

/* The byte offset ``points`` code points before ``end``. */
static size_t back_points(const char *text, size_t end, size_t points) {
    size_t index = end;
    while (index > 0 && points) {
        --index;
        if (((unsigned char)text[index] & 0xC0) != 0x80) --points;
    }
    while (index > 0 && ((unsigned char)text[index] & 0xC0) == 0x80) --index;
    return index;
}

static size_t line_of(const char *text, size_t position) {
    size_t line = 1;
    for (size_t index = 0; index < position; ++index) line += text[index] == '\n';
    return line;
}

static bool word(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static bool ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* _mask_c(): comments and literals as spaces, offsets kept. */
static char *mask_c(const char *text, size_t size) {
    char *masked = copy_n(text, size);
    enum { CODE, BLOCK, LINE, STRING, CHARACTER } state = CODE;
    for (size_t index = 0; index < size;) {
        char current = text[index], following = index + 1 < size ? text[index + 1] : '\0';
        if (state == CODE && current == '/' && following == '*') {
            masked[index] = masked[index + 1] = ' ';
            state = BLOCK;
            index += 2;
        } else if (state == CODE && current == '/' && following == '/') {
            masked[index] = masked[index + 1] = ' ';
            state = LINE;
            index += 2;
        } else if (state == CODE && (current == '"' || current == '\'')) {
            masked[index] = ' ';
            state = current == '"' ? STRING : CHARACTER;
            ++index;
        } else if (state == STRING || state == CHARACTER) {
            if (current == '\\') {
                masked[index] = ' ';
                if (index + 1 < size) {
                    if (text[index + 1] != '\n') masked[index + 1] = ' ';
                    index += 2;
                } else {
                    ++index;
                }
                continue;
            }
            if (current == (state == STRING ? '"' : '\'')) {
                masked[index] = ' ';
                state = CODE;
            } else if (current != '\n') {
                masked[index] = ' ';
            }
            ++index;
        } else if (state == BLOCK) {
            if (current == '*' && following == '/') {
                masked[index] = masked[index + 1] = ' ';
                state = CODE;
                index += 2;
            } else {
                if (current != '\n') masked[index] = ' ';
                ++index;
            }
        } else if (state == LINE) {
            if (current == '\n') state = CODE;
            else masked[index] = ' ';
            ++index;
        } else {
            ++index;
        }
    }
    return masked;
}

/* _mask_preprocessor(): directive lines (and their continuations) blanked. */
static void mask_preprocessor(char *text, size_t size) {
    bool continued = false;
    for (size_t start = 0; start < size;) {
        size_t end = start;
        while (end < size && text[end] != '\n') ++end;
        size_t cursor = start;
        while (cursor < end && space(text[cursor])) ++cursor;
        bool directive = continued || (cursor < end && text[cursor] == '#');
        size_t last = end;
        while (last > start && text[last - 1] == '\r') --last;
        continued = directive && last > start && text[last - 1] == '\\';
        if (directive)
            for (size_t index = start; index < end; ++index)
                if (text[index] != '\r') text[index] = ' ';
        start = end < size ? end + 1 : end;
    }
}

static size_t matching_brace(const char *text, size_t size, size_t opening) {
    int depth = 0;
    for (size_t index = opening; index < size; ++index) {
        if (text[index] == '{') ++depth;
        else if (text[index] == '}' && --depth == 0) return index;
    }
    return SIZE_MAX;
}

static bool contains_word(const char *text, size_t length, const char *needle) {
    size_t wanted = strlen(needle);
    for (size_t index = 0; index + wanted <= length; ++index) {
        if (memcmp(text + index, needle, wanted)) continue;
        if (index && word(text[index - 1])) continue;
        if (index + wanted < length && word(text[index + wanted])) continue;
        return true;
    }
    return false;
}

/* _function_header() on text[start, end): the name range and prefix end. */
static bool function_header(const char *text, size_t start, size_t end, size_t *name_start,
                            size_t *name_end) {
    while (end > start && space(text[end - 1])) --end;
    if (end == start || text[end - 1] != ')') return false;
    int depth = 0;
    size_t opening = SIZE_MAX;
    for (size_t cursor = end; cursor > start;) {
        --cursor;
        if (text[cursor] == ')') ++depth;
        else if (text[cursor] == '(' && --depth == 0) {
            opening = cursor;
            break;
        }
    }
    if (opening == SIZE_MAX) return false;
    /* ([A-Za-z_]\w*)\s*$ before the parenthesis: the last identifier. */
    size_t cursor = opening;
    while (cursor > start && space(text[cursor - 1])) --cursor;
    size_t finish = cursor;
    while (cursor > start && word(text[cursor - 1])) --cursor;
    while (cursor < finish && !ident_start(text[cursor])) ++cursor;
    if (cursor == finish) return false;
    if (control_word(text + cursor, finish - cursor)) return false;
    for (size_t index = start; index < cursor; ++index)
        if (text[index] == '=') return false;
    if (contains_word(text + start, cursor - start, "typedef")) return false;
    *name_start = cursor;
    *name_end = finish;
    return true;
}

/* _source_address(): (?:^|_)(?:0x)?(08[0-9A-Fa-f]{6})$. */
static char *source_address(const char *symbol) {
    size_t length = strlen(symbol);
    if (length < 8) return NULL;
    const char *digits = symbol + length - 8;
    if (digits[0] != '0' || digits[1] != '8') return NULL;
    for (int index = 2; index < 8; ++index)
        if (!isxdigit((unsigned char)digits[index])) return NULL;
    size_t before = length - 8;
    bool ok = before == 0 || symbol[before - 1] == '_';
    if (!ok && before >= 2 && symbol[before - 2] == '0' && symbol[before - 1] == 'x')
        ok = before == 2 || symbol[before - 3] == '_';
    if (!ok) return NULL;
    char *address = format("0x%.8s", digits);
    for (char *c = address + 2; *c; ++c) *c = (char)toupper((unsigned char)*c);
    return address;
}

/* _brief_and_address() from the documentation comment before a header. */
static void brief_and_address(const char *text, size_t header, char **description, char **address) {
    *description = *address = NULL;
    size_t floor = back_points(text, header, 1200);
    size_t comment_start = SIZE_MAX;
    for (size_t index = floor; index + 3 <= header; ++index)
        if (!memcmp(text + index, "/**", 3)) comment_start = index;
    if (comment_start == SIZE_MAX) return;
    size_t comment_end = SIZE_MAX;
    for (size_t index = comment_start; index + 2 <= header; ++index)
        if (!memcmp(text + index, "*/", 2)) {
            comment_end = index;
            break;
        }
    if (comment_end == SIZE_MAX) return;
    for (size_t index = comment_end + 2; index < header; ++index)
        if (text[index] == ';' || text[index] == '}') return;
    size_t finish = comment_end + 2;
    /* @brief\s+([0-9A-Fa-f]+)\s*\|\s*([^\n*]*) */
    for (size_t at = comment_start; at + 6 <= finish; ++at) {
        if (memcmp(text + at, "@brief", 6)) continue;
        size_t cursor = at + 6;
        if (cursor >= finish || !space(text[cursor])) continue;
        while (cursor < finish && space(text[cursor])) ++cursor;
        size_t hex = cursor;
        while (cursor < finish && isxdigit((unsigned char)text[cursor])) ++cursor;
        if (cursor == hex) continue;
        size_t hex_end = cursor;
        while (cursor < finish && space(text[cursor])) ++cursor;
        if (cursor >= finish || text[cursor] != '|') continue;
        ++cursor;
        while (cursor < finish && space(text[cursor])) ++cursor;
        size_t start = cursor;
        while (cursor < finish && text[cursor] != '\n' && text[cursor] != '*') ++cursor;
        size_t end = cursor;
        while (start < end && space(text[start])) ++start;
        while (end > start && space(text[end - 1])) --end;
        if (end > start) *description = copy_n(text + start, end - start);
        char number[64];
        snprintf(number, sizeof number, "%.*s", (int)(hex_end - hex < 60 ? hex_end - hex : 60), text + hex);
        unsigned long long value = strtoull(number, NULL, 16);
        if (value < 0x08000000ull) value += 0x08000000ull;
        *address = format("0x%08llX", value);
        return;
    }
}

/* ---- Categories ------------------------------------------------------- */

static const char *category_of(const char *game, const char *relative) {
    char path[4096], stem[1024];
    snprintf(path, sizeof path, "%s", relative);
    for (char *c = path; *c; ++c) *c = (char)tolower((unsigned char)*c);
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    snprintf(stem, sizeof stem, "%.1000s", name);
    char *dot = strrchr(stem, '.');
    if (dot && dot != stem) *dot = '\0';
    if (strstr(path, "m4a") || strstr(path, "audio") || strstr(path, "music") || strstr(path, "sound"))
        return "audio";
    if (strstr(path, "save") || strstr(path, "sram")) return "saves";
    if (strstr(path, "input")) return "input";
    if (!strcmp(game, "aos")) {
        if (!strcmp(stem, "agb_print") || !strcmp(stem, "agb_multi_sio_sync")) return "technical";
        if (!strncmp(path, "src/data/", 9)) return "data";
        return "unclassified";
    }
    if (!strncmp(path, "src/menus/", 10)) return "menus";
    if (!strncmp(path, "src/cutscenes/", 14) || strstr(stem, "intro") || strstr(stem, "ending") ||
        strstr(stem, "gallery"))
        return "cutscenes";
    if (!strncmp(path, "src/sprites_ai/", 15)) {
        for (size_t index = 0; index < COUNT(mzm_bosses); ++index)
            if (!strcmp(stem, mzm_bosses[index])) return "bosses";
        return "enemies";
    }
    if (!strncmp(path, "src/data/", 9)) return "data";
    if (strstr(stem, "samus")) return "player";
    if (strstr(stem, "projectile")) return "weapons";
    if (strstr(stem, "room") || strstr(stem, "connection") || strstr(stem, "scroll") ||
        strstr(stem, "clipdata") || strstr(stem, "block"))
        return "rooms";
    if (strstr(stem, "hud") || strstr(stem, "minimap") || strstr(stem, "display") || strstr(stem, "oam"))
        return "interface";
    if (strstr(stem, "event") || strstr(stem, "escape")) return "events";
    return "technical";
}

/* ---- Records ------------------------------------------------------------ */

typedef struct {
    char *id, *symbol, *kind, *source, *linkage, *category, *description, *address, *evidence;
    size_t line, body_start;
    bool has_body;
    List raw_calls, raw_tables, raw_refs, variants;
    Pairs raw_writes;
    List calls, ambiguous, address_refs;
    FusionJson indirect, callbacks;
} Routine;

typedef struct {
    char *id, *symbol, *source, *category, *linkage;
    size_t line;
    List definitions;
} Declaration;

typedef struct {
    char *id, *symbol, *kind, *source, *linkage, *category, *address;
    size_t line;
    List raw_refs, targets, variants;
    Pairs raw_inits;
    FusionJson callbacks;
    bool pointer_table;
} Data;

typedef struct {
    char *id, *symbol, *kind, *source, *category;
    size_t line;
    bool anonymous;
    List members, fp_members;
} Type;

typedef struct {
    char *id, *symbol, *source, *category;
    size_t line;
} Constant;

typedef struct {
    Routine *routines;
    size_t routine_count, routine_capacity;
    Declaration *declarations;
    size_t declaration_count, declaration_capacity;
    Data *data;
    size_t data_count, data_capacity;
    Type *types;
    size_t type_count, type_capacity;
    Constant *constants;
    size_t constant_count, constant_capacity;
} Game;

static Routine *new_routine(Game *game) {
    game->routines = grow(game->routines, &game->routine_capacity, game->routine_count,
                          sizeof *game->routines);
    Routine *routine = &game->routines[game->routine_count++];
    *routine = (Routine){0};
    routine->indirect = fusion_json_array();
    routine->callbacks = fusion_json_array();
    return routine;
}

static Data *new_data(Game *game) {
    game->data = grow(game->data, &game->data_capacity, game->data_count, sizeof *game->data);
    Data *data = &game->data[game->data_count++];
    *data = (Data){0};
    data->callbacks = fusion_json_array();
    return data;
}

/* ---- Lexical scans ----------------------------------------------------- */

/* Every identifier matched by \b([A-Za-z_]\w*) in [start, end): its range. */
typedef bool (*IdentifierVisitor)(const char *text, size_t start, size_t end, size_t limit,
                                  void *context);

static void each_identifier(const char *text, size_t from, size_t to, IdentifierVisitor visit,
                            void *context) {
    for (size_t index = from; index < to;) {
        if (!word(text[index])) {
            ++index;
            continue;
        }
        size_t start = index;
        while (index < to && word(text[index])) ++index;
        /* \b...: a run of word characters is one identifier when it does
         * not start with a digit (a digit start never yields a match). */
        if (!ident_start(text[start])) continue;
        visit(text, start, index, to, context);
    }
}

static bool add_call(const char *text, size_t start, size_t end, size_t limit, void *context) {
    size_t after = end;
    while (after < limit && space(text[after])) ++after;
    if (after < limit && text[after] == '(' && !control_word(text + start, end - start))
        list_add_n(context, text + start, end - start);
    return true;
}

static bool add_table(const char *text, size_t start, size_t end, size_t limit, void *context) {
    /* \s*\[[^\]\n]+\]\s*\( */
    size_t cursor = end;
    while (cursor < limit && space(text[cursor])) ++cursor;
    if (cursor >= limit || text[cursor] != '[') return true;
    size_t inner = ++cursor;
    while (cursor < limit && text[cursor] != ']' && text[cursor] != '\n') ++cursor;
    if (cursor >= limit || text[cursor] != ']' || cursor == inner) return true;
    ++cursor;
    while (cursor < limit && space(text[cursor])) ++cursor;
    if (cursor < limit && text[cursor] == '(') list_add_n(context, text + start, end - start);
    return true;
}

typedef struct {
    List *names;
} RefContext;

static bool add_address_ref(const char *text, size_t start, size_t end, size_t limit,
                            void *context) {
    (void)limit;
    RefContext *refs = context;
    /* body[start] == "." or body[start - 1:start + 1] == "->", ignoring spaces. */
    long before = (long)start - 1;
    while (before >= 0 && isspace((unsigned char)text[before])) --before;
    if (before >= 0 && (text[before] == '.' || (before >= 1 && text[before - 1] == '-' &&
                                                 text[before] == '>')))
        return true;
    if (before == 0 && text[0] == '>') {
        /* body[-1:1] is empty unless the body has one character. */
    }
    size_t after = end;
    while (after < limit && isspace((unsigned char)text[after])) ++after;
    if (after < limit && text[after] == '(') return true;
    if (control_word(text + start, end - start) || type_word(text + start, end - start)) return true;
    list_add_n(refs->names, text + start, end - start);
    return true;
}

static bool add_identifier(const char *text, size_t start, size_t end, size_t limit, void *context) {
    (void)limit;
    list_add_n(context, text + start, end - start);
    return true;
}

/* MEMBER_WRITE_RE: (?:\.|->)\s*NAME\s*=\s*(?:\(\s*[A-Za-z_][\w\s\*]*\)\s*)?NAME\s*; */
static void member_writes(const char *text, size_t from, size_t to, Pairs *pairs) {
    for (size_t index = from; index < to; ++index) {
        size_t cursor;
        if (text[index] == '.') cursor = index + 1;
        else if (text[index] == '-' && index + 1 < to && text[index + 1] == '>') cursor = index + 2;
        else continue;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || !ident_start(text[cursor])) continue;
        size_t member = cursor;
        while (cursor < to && word(text[cursor])) ++cursor;
        size_t member_end = cursor;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || text[cursor] != '=') continue;
        ++cursor;
        while (cursor < to && space(text[cursor])) ++cursor;
        size_t value_from = cursor;
        if (cursor < to && text[cursor] == '(') {
            size_t cast = cursor + 1;
            while (cast < to && space(text[cast])) ++cast;
            if (cast < to && ident_start(text[cast])) {
                ++cast;
                while (cast < to && (word(text[cast]) || space(text[cast]) || text[cast] == '*')) ++cast;
                if (cast < to && text[cast] == ')') {
                    ++cast;
                    while (cast < to && space(text[cast])) ++cast;
                    value_from = cast;
                }
            }
        }
        cursor = value_from;
        if (cursor >= to || !ident_start(text[cursor])) continue;
        size_t value = cursor;
        while (cursor < to && word(text[cursor])) ++cursor;
        size_t value_end = cursor;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || text[cursor] != ';') continue;
        pairs_add(pairs, text + member, member_end - member, text + value, value_end - value);
        index = cursor;
    }
}

/* MEMBER_INIT_RE: \.\s*NAME\s*=\s*NAME (no trailing context). */
static void member_inits(const char *text, size_t from, size_t to, Pairs *pairs) {
    for (size_t index = from; index < to; ++index) {
        if (text[index] != '.') continue;
        size_t cursor = index + 1;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || !ident_start(text[cursor])) continue;
        size_t member = cursor;
        while (cursor < to && word(text[cursor])) ++cursor;
        size_t member_end = cursor;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || text[cursor] != '=') continue;
        ++cursor;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || !ident_start(text[cursor])) continue;
        size_t value = cursor;
        while (cursor < to && word(text[cursor])) ++cursor;
        pairs_add(pairs, text + member, member_end - member, text + value, cursor - value);
        index = cursor - 1;
    }
}

/* \(\s*\*\s*NAME\s*\) at or after ``from``: the name range, or false. */
static bool pointer_name(const char *text, size_t from, size_t to, size_t *start, size_t *end,
                         size_t *match_start, bool need_call) {
    for (size_t index = from; index < to; ++index) {
        if (text[index] != '(') continue;
        size_t cursor = index + 1;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || text[cursor] != '*') continue;
        ++cursor;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || !ident_start(text[cursor])) continue;
        size_t name = cursor;
        while (cursor < to && word(text[cursor])) ++cursor;
        size_t name_end = cursor;
        while (cursor < to && space(text[cursor])) ++cursor;
        if (cursor >= to || text[cursor] != ')') continue;
        if (need_call) {
            ++cursor;
            while (cursor < to && space(text[cursor])) ++cursor;
            if (cursor >= to || text[cursor] != '(') continue;
        }
        *start = name;
        *end = name_end;
        if (match_start) *match_start = index;
        return true;
    }
    return false;
}

/* _split_top_level_commas() of text[from, to): the part ranges. */
static size_t split_commas(const char *text, size_t from, size_t to, size_t (*parts)[2],
                           size_t capacity) {
    size_t count = 0, start = from;
    int depth = 0;
    for (size_t index = from; index < to; ++index) {
        char c = text[index];
        if (c == '(' || c == '{' || c == '[') ++depth;
        else if ((c == ')' || c == '}' || c == ']') && depth) --depth;
        else if (c == ',' && depth == 0) {
            if (count < capacity) {
                parts[count][0] = start;
                parts[count][1] = index;
            }
            ++count;
            start = index + 1;
        }
    }
    if (count < capacity) {
        parts[count][0] = start;
        parts[count][1] = to;
    }
    return count + 1;
}

/* The ranges of a comma split, allocated. */
static size_t (*comma_parts(const char *text, size_t from, size_t to, size_t *count))[2] {
    size_t needed = split_commas(text, from, to, NULL, 0);
    size_t (*parts)[2] = malloc(needed * sizeof *parts);
    if (!parts) oom();
    *count = split_commas(text, from, to, parts, needed);
    return parts;
}

/* re.sub(r"(?:\s*\[[^]]*\]\s*)+$", "", text) as a new end. */
static size_t strip_brackets(const char *text, size_t start, size_t end) {
    size_t finish = end;
    for (;;) {
        size_t cursor = finish;
        while (cursor > start && space(text[cursor - 1])) --cursor;
        if (cursor == start || text[cursor - 1] != ']') return finish;
        size_t close = cursor - 1, previous = start;
        for (size_t index = close; index > start; --index)
            if (text[index - 1] == ']') {
                previous = index;
                break;
            }
        size_t opening = SIZE_MAX;
        for (size_t index = previous; index < close; ++index)
            if (text[index] == '[') {
                opening = index;
                break;
            }
        if (opening == SIZE_MAX) return finish;
        size_t before = opening;
        while (before > start && space(text[before - 1])) --before;
        finish = before;
    }
}

static size_t rstrip(const char *text, size_t start, size_t end) {
    while (end > start && space(text[end - 1])) --end;
    return end;
}

static size_t lstrip(const char *text, size_t start, size_t end) {
    while (start < end && space(text[start])) ++start;
    return start;
}

/* ([A-Za-z_]\w*)\s*$ of text[start, end): the identifier range. */
static bool trailing_identifier(const char *text, size_t start, size_t end, size_t *name,
                                size_t *name_end) {
    size_t finish = rstrip(text, start, end), cursor = finish;
    while (cursor > start && word(text[cursor - 1])) --cursor;
    while (cursor < finish && !ident_start(text[cursor])) ++cursor;
    if (cursor == finish) return false;
    *name = cursor;
    *name_end = finish;
    return true;
}

static size_t find_char(const char *text, size_t start, size_t end, char wanted) {
    for (size_t index = start; index < end; ++index)
        if (text[index] == wanted) return index;
    return end;
}

/* _declarator_name(): the declared identifier of one declarator. */
static bool declarator_name(const char *text, size_t start, size_t end, size_t *name,
                            size_t *name_end) {
    if (pointer_name(text, start, end, name, name_end, NULL, false)) return true;
    size_t finish = find_char(text, start, end, '=');
    finish = rstrip(text, start, strip_brackets(text, start, finish));
    size_t begin = start;
    while (begin < finish && text[begin] == '*') ++begin;
    finish = rstrip(text, begin, finish);
    if (!trailing_identifier(text, begin, finish, name, name_end)) return false;
    return !type_word(text + *name, *name_end - *name);
}

/* ---- Discovery ---------------------------------------------------------- */

static void discover_c(const char *game_id, const char *relative, const Source *source, Game *game) {
    char *masked = mask_c(source->text, source->size);
    const char *category = category_of(game_id, relative);
    size_t statement = 0, body = 0;
    int depth = 0;
    Routine *current = NULL;
    for (size_t index = 0; index < source->size; ++index) {
        char c = masked[index];
        if (c == '{' && depth == 0) {
            size_t name, name_end;
            if (function_header(masked, statement, index, &name, &name_end)) {
                char *symbol = copy_n(masked + name, name_end - name);
                size_t position = name;
                /* masked.rfind(symbol, statement_start, index) */
                for (size_t probe = index; probe-- > statement;)
                    if (probe + (name_end - name) <= index &&
                        !memcmp(masked + probe, symbol, name_end - name)) {
                        position = probe;
                        break;
                    }
                current = new_routine(game);
                current->symbol = symbol;
                current->id = format("%s:c:%s:%s", game_id, relative, symbol);
                current->kind = copy_text("c_function");
                current->source = copy_text(relative);
                current->line = line_of(masked, position);
                current->linkage = copy_text(contains_word(masked + statement, name - statement, "static")
                                                 ? "internal" : "external");
                current->category = copy_text(category);
                brief_and_address(source->text, position, &current->description, &current->address);
                if (!current->address) current->address = source_address(symbol);
                current->body_start = code_points(source->text, index + 1);
                current->has_body = true;
                body = index + 1;
            }
            depth = 1;
        } else if (c == '{' && depth > 0) {
            ++depth;
        } else if (c == '}' && depth > 0) {
            if (--depth == 0) {
                if (current) {
                    each_identifier(masked, body, index, add_call, &current->raw_calls);
                    list_unique(&current->raw_calls);
                    each_identifier(masked, body, index, add_table, &current->raw_tables);
                    list_unique(&current->raw_tables);
                    member_writes(masked, body, index, &current->raw_writes);
                    pairs_unique(&current->raw_writes);
                    RefContext refs = {&current->raw_refs};
                    each_identifier(masked, body, index, add_address_ref, &refs);
                    list_unique(&current->raw_refs);
                }
                current = NULL;
                statement = index + 1;
            }
        } else if (c == ';' && depth == 0) {
            statement = index + 1;
        }
    }
    free(masked);
}

/* The data statements of _top_level_data_statements(). */
static size_t (*data_statements(const char *masked, size_t size, size_t *count))[2] {
    size_t (*statements)[2] = NULL, capacity = 0, start = 0;
    *count = 0;
    int depth = 0;
    for (size_t index = 0; index < size;) {
        char c = masked[index];
        if (c == '{' && depth == 0) {
            size_t name, name_end;
            if (function_header(masked, start, index, &name, &name_end)) {
                size_t closing = matching_brace(masked, size, index);
                if (closing == SIZE_MAX) break;
                start = closing + 1;
                index = closing + 1;
                continue;
            }
            depth = 1;
        } else if (c == '{' && depth) {
            ++depth;
        } else if (c == '}' && depth) {
            --depth;
        } else if (c == ';' && depth == 0) {
            statements = grow(statements, &capacity, *count, sizeof *statements);
            statements[*count][0] = start;
            statements[*count][1] = index + 1;
            ++*count;
            start = index + 1;
        }
        ++index;
    }
    return statements;
}

static void discover_c_data(const char *game_id, const char *relative, const char *masked,
                            size_t size, Game *game) {
    const char *category = category_of(game_id, relative);
    size_t count = 0;
    size_t (*statements)[2] = data_statements(masked, size, &count);
    for (size_t item = 0; item < count; ++item) {
        size_t start = statements[item][0], end = statements[item][1];
        /* statement.strip().rstrip(";").strip() */
        size_t from = lstrip(masked, start, end), to = rstrip(masked, from, end);
        while (to > from && masked[to - 1] == ';') --to;
        to = rstrip(masked, from, to);
        if (to == from || contains_word(masked + from, to - from, "typedef") ||
            contains_word(masked + from, to - from, "extern"))
            continue;
        size_t part_count = 0;
        size_t (*parts)[2] = comma_parts(masked, from, to, &part_count);
        size_t head_end = rstrip(masked, parts[0][0], find_char(masked, parts[0][0], parts[0][1], '='));
        size_t name, name_end, prefix_end;
        bool skip = function_header(masked, parts[0][0], head_end, &name, &name_end);
        size_t pointer_start = 0, pointer_end = 0, match_start = 0;
        if (!skip) {
            if (pointer_name(masked, parts[0][0], head_end, &pointer_start, &pointer_end, &match_start,
                             false)) {
                prefix_end = match_start;
            } else {
                size_t trimmed = rstrip(masked, parts[0][0], strip_brackets(masked, parts[0][0], head_end));
                while (trimmed > parts[0][0] && masked[trimmed - 1] == '*') --trimmed;
                trimmed = rstrip(masked, parts[0][0], trimmed);
                if (!trailing_identifier(masked, parts[0][0], trimmed, &name, &name_end) ||
                    type_word(masked + name, name_end - name))
                    skip = true;
                else
                    prefix_end = name;
            }
        }
        bool letter = false;
        for (size_t index = parts[0][0]; !skip && index < prefix_end; ++index)
            letter |= ident_start(masked[index]);
        if (skip || !letter) {
            free(parts);
            continue;
        }
        size_t prefix_start = parts[0][0];
        bool has_paren = find_char(masked, prefix_start, prefix_end, '(') < prefix_end;
        bool internal = contains_word(masked + prefix_start, prefix_end - prefix_start, "static");
        for (size_t part = 0; part < part_count; ++part) {
            size_t p_start = parts[part][0], p_end = parts[part][1];
            size_t equals = find_char(masked, p_start, p_end, '=');
            size_t head = rstrip(masked, p_start, equals);
            if (part && !has_paren) {
                /* [A-Za-z_]\w*\s*\([^()]*\)\s*$ */
                size_t finish = rstrip(masked, p_start, head);
                if (finish > p_start && masked[finish - 1] == ')') {
                    size_t open = finish - 1;
                    while (open > p_start && masked[open - 1] != '(' && masked[open - 1] != ')') --open;
                    if (open > p_start && masked[open - 1] == '(') {
                        size_t ident_end = rstrip(masked, p_start, open - 1), ident = ident_end;
                        while (ident > p_start && word(masked[ident - 1])) --ident;
                        while (ident < ident_end && !ident_start(masked[ident])) ++ident;
                        if (ident < ident_end) continue;
                    }
                }
            }
            size_t symbol_start, symbol_end;
            if (!declarator_name(masked, p_start, p_end, &symbol_start, &symbol_end)) continue;
            char *symbol = copy_n(masked + symbol_start, symbol_end - symbol_start);
            /* masked.find(symbol, start, end) */
            size_t position = start;
            size_t length = symbol_end - symbol_start;
            for (size_t probe = start; probe + length <= end; ++probe)
                if (!memcmp(masked + probe, symbol, length)) {
                    position = probe;
                    break;
                }
            Data *data = new_data(game);
            data->symbol = symbol;
            data->id = format("%s:data:%s:%s", game_id, relative, symbol);
            data->kind = copy_text("c_data");
            data->source = copy_text(relative);
            data->line = line_of(masked, position);
            data->linkage = copy_text(internal ? "internal" : "external");
            data->category = copy_text(category);
            data->address = source_address(symbol);
            if (equals < p_end) {
                each_identifier(masked, equals + 1, p_end, add_identifier, &data->raw_refs);
                list_unique(&data->raw_refs);
                member_inits(masked, equals + 1, p_end, &data->raw_inits);
                pairs_unique(&data->raw_inits);
            }
        }
        free(parts);
    }
    free(statements);
}

static void discover_declarations(const char *game_id, const char *relative, const char *masked,
                                  size_t size, Game *game) {
    const char *category = category_of(game_id, relative);
    size_t start = 0;
    int depth = 0;
    for (size_t index = 0; index < size; ++index) {
        char c = masked[index];
        if (c == '{') ++depth;
        else if (c == '}' && depth) --depth;
        else if (c == ';' && depth == 0) {
            size_t from = lstrip(masked, start, index), to = rstrip(masked, from, index);
            size_t name, name_end;
            if (function_header(masked, from, to, &name, &name_end)) {
                char *symbol = copy_n(masked + name, name_end - name);
                size_t position = name, length = name_end - name;
                for (size_t probe = index + 1; probe-- > start;)
                    if (probe + length <= index + 1 && !memcmp(masked + probe, symbol, length)) {
                        position = probe;
                        break;
                    }
                game->declarations = grow(game->declarations, &game->declaration_capacity,
                                          game->declaration_count, sizeof *game->declarations);
                Declaration *declaration = &game->declarations[game->declaration_count++];
                *declaration = (Declaration){0};
                declaration->symbol = symbol;
                declaration->id = format("%s:declaration:%s:%s", game_id, relative, symbol);
                declaration->source = copy_text(relative);
                declaration->line = line_of(masked, position);
                declaration->category = copy_text(category);
                declaration->linkage = copy_text(contains_word(masked + from, name - from, "static")
                                                     ? "internal" : "external");
            }
            start = index + 1;
        }
    }
}

/* _type_members() of body text[from, to). */
static void type_members(const char *kind, const char *text, size_t from, size_t to, List *members) {
    if (!strcmp(kind, "enum")) {
        size_t count = 0;
        size_t (*parts)[2] = comma_parts(text, from, to, &count);
        for (size_t part = 0; part < count; ++part) {
            size_t cursor = lstrip(text, parts[part][0], parts[part][1]);
            if (cursor < parts[part][1] && ident_start(text[cursor])) {
                size_t end = cursor;
                while (end < parts[part][1] && word(text[end])) ++end;
                list_add_n(members, text + cursor, end - cursor);
            }
        }
        free(parts);
        return;
    }
    for (size_t start = from; start <= to;) {
        size_t end = find_char(text, start, to, ';');
        size_t count = 0;
        size_t (*parts)[2] = comma_parts(text, start, end, &count);
        for (size_t part = 0; part < count; ++part) {
            size_t name, name_end;
            if (pointer_name(text, parts[part][0], parts[part][1], &name, &name_end, NULL, false)) {
                list_add_n(members, text + name, name_end - name);
                continue;
            }
            size_t limit = find_char(text, parts[part][0], parts[part][1], ':');
            List identifiers = {0};
            each_identifier(text, parts[part][0], limit, add_identifier, &identifiers);
            for (size_t index = identifiers.count; index-- > 0;)
                if (!type_word(identifiers.items[index], strlen(identifiers.items[index]))) {
                    list_add(members, identifiers.items[index]);
                    break;
                }
            list_free(&identifiers);
        }
        free(parts);
        start = end + 1;
    }
}

static void fp_members(const char *kind, const char *text, size_t from, size_t to,
                       const Map *typedefs, List *members) {
    if (!strcmp(kind, "enum")) return;
    /* FP_MEMBER_RE: \(\s*\*\s*NAME\s*\)\s*\( */
    for (size_t index = from; index < to;) {
        size_t name, name_end, match;
        if (!pointer_name(text, index, to, &name, &name_end, &match, true)) break;
        list_add_n(members, text + name, name_end - name);
        index = name_end;
    }
    for (size_t start = from; start <= to;) {
        size_t end = find_char(text, start, to, ';');
        if (find_char(text, start, end, '(') == end) {
            size_t limit = find_char(text, start, end, ':');
            List identifiers = {0}, candidates = {0};
            each_identifier(text, start, limit, add_identifier, &identifiers);
            for (size_t index = 0; index < identifiers.count; ++index)
                if (!type_word(identifiers.items[index], strlen(identifiers.items[index])))
                    list_add(&candidates, identifiers.items[index]);
            if (candidates.count >= 2 && map_slot((Map *)typedefs, candidates.items[0], false))
                list_add(members, candidates.items[candidates.count - 1]);
            list_free(&identifiers);
            list_free(&candidates);
        }
        start = end + 1;
    }
    list_unique(members);
}

static bool seen_type(const List *seen, const char *kind, const char *symbol) {
    char key[512];
    snprintf(key, sizeof key, "%s\t%s", kind, symbol);
    return list_has(seen, key);
}

static void discover_types(const char *game_id, const char *relative, const char *masked, size_t size,
                           const Map *typedefs, Game *game) {
    const char *category = category_of(game_id, relative);
    List seen = {0};
    static const char *const kinds[] = {"struct", "union", "enum"};
    /* TYPE_RE: \b(struct|union|enum)\s+NAME\s*\{ */
    for (size_t index = 0; index < size; ++index) {
        if (index && word(masked[index - 1])) continue;
        const char *kind = NULL;
        for (size_t candidate = 0; candidate < 3 && !kind; ++candidate) {
            size_t length = strlen(kinds[candidate]);
            if (!strncmp(masked + index, kinds[candidate], length) && index + length < size &&
                space(masked[index + length]))
                kind = kinds[candidate];
        }
        if (!kind) continue;
        size_t cursor = index + strlen(kind);
        while (cursor < size && space(masked[cursor])) ++cursor;
        if (cursor >= size || !ident_start(masked[cursor])) continue;
        size_t name = cursor;
        while (cursor < size && word(masked[cursor])) ++cursor;
        size_t name_end = cursor;
        while (cursor < size && space(masked[cursor])) ++cursor;
        if (cursor >= size || masked[cursor] != '{') continue;
        size_t opening = cursor, closing = matching_brace(masked, size, opening);
        char *symbol = copy_n(masked + name, name_end - name);
        if (closing == SIZE_MAX || seen_type(&seen, kind, symbol)) {
            free(symbol);
            index = opening;
            continue;
        }
        char key[512];
        snprintf(key, sizeof key, "%s\t%s", kind, symbol);
        list_add(&seen, key);
        game->types = grow(game->types, &game->type_capacity, game->type_count, sizeof *game->types);
        Type *type = &game->types[game->type_count++];
        *type = (Type){0};
        type->symbol = symbol;
        type->id = format("%s:type:%s:%s:%s", game_id, relative, kind, symbol);
        type->kind = copy_text(kind);
        type->source = copy_text(relative);
        type->line = line_of(masked, index);
        type->category = copy_text(category);
        type_members(kind, masked, opening + 1, closing, &type->members);
        fp_members(kind, masked, opening + 1, closing, typedefs, &type->fp_members);
        index = opening;
    }
    /* ANONYMOUS_TYPEDEF_RE: \btypedef\s+(struct|union|enum)\s*\{ */
    for (size_t index = 0; index + 7 < size; ++index) {
        if ((index && word(masked[index - 1])) || strncmp(masked + index, "typedef", 7) ||
            !space(masked[index + 7]))
            continue;
        size_t cursor = index + 7;
        while (cursor < size && space(masked[cursor])) ++cursor;
        const char *kind = NULL;
        for (size_t candidate = 0; candidate < 3 && !kind; ++candidate)
            if (!strncmp(masked + cursor, kinds[candidate], strlen(kinds[candidate])))
                kind = kinds[candidate];
        if (!kind) continue;
        cursor += strlen(kind);
        while (cursor < size && space(masked[cursor])) ++cursor;
        if (cursor >= size || masked[cursor] != '{') continue;
        size_t opening = cursor, closing = matching_brace(masked, size, opening);
        if (closing == SIZE_MAX) continue;
        /* \s*([A-Za-z_]\w*)\s*; after the body */
        size_t after = closing + 1;
        while (after < size && space(masked[after])) ++after;
        if (after >= size || !ident_start(masked[after])) {
            index = opening;
            continue;
        }
        size_t name = after;
        while (after < size && word(masked[after])) ++after;
        size_t name_end = after;
        while (after < size && space(masked[after])) ++after;
        if (after >= size || masked[after] != ';') {
            index = opening;
            continue;
        }
        char *symbol = copy_n(masked + name, name_end - name);
        if (seen_type(&seen, "typedef", symbol)) {
            free(symbol);
            index = opening;
            continue;
        }
        char key[512];
        snprintf(key, sizeof key, "typedef\t%s", symbol);
        list_add(&seen, key);
        game->types = grow(game->types, &game->type_capacity, game->type_count, sizeof *game->types);
        Type *type = &game->types[game->type_count++];
        *type = (Type){0};
        type->symbol = symbol;
        type->id = format("%s:type:%s:%s:%s", game_id, relative, kind, symbol);
        type->kind = copy_text(kind);
        type->source = copy_text(relative);
        type->line = line_of(masked, index);
        type->category = copy_text(category);
        type->anonymous = true;
        type_members(kind, masked, opening + 1, closing, &type->members);
        fp_members(kind, masked, opening + 1, closing, typedefs, &type->fp_members);
        index = opening;
    }
    list_free(&seen);
}

static bool blank_line(const char *text, size_t start, size_t end) {
    for (size_t index = start; index < end; ++index)
        if (!space(text[index])) return false;
    return true;
}

/* The line of a "^\s*..." MULTILINE match on line ``line`` starting at
 * ``offset``: preceding whitespace-only lines belong to the match. */
static size_t spanning_line(const char *text, size_t offset, size_t line, size_t floor) {
    while (offset > floor) {
        size_t previous_end = offset - 1, previous_start = previous_end;
        while (previous_start > floor && text[previous_start - 1] != '\n') --previous_start;
        if (previous_start == floor && floor > 0 && text[floor - 1] != '\n') break;
        if (!blank_line(text, previous_start, previous_end)) break;
        offset = previous_start;
        --line;
    }
    return line;
}

static void discover_constants(const char *game_id, const char *relative, const Source *source,
                               Game *game) {
    const char *category = category_of(game_id, relative);
    const char *text = source->text;
    size_t line = 1, floor = 0;
    for (size_t start = 0; start < source->size;) {
        size_t end = start;
        while (end < source->size && text[end] != '\n') ++end;
        size_t cursor = start;
        while (cursor < end && space(text[cursor])) ++cursor;
        if (cursor < end && text[cursor] == '#') {
            ++cursor;
            while (cursor < end && space(text[cursor])) ++cursor;
            if (!strncmp(text + cursor, "define", 6) && cursor + 6 < end && space(text[cursor + 6])) {
                cursor += 6;
                while (cursor < end && space(text[cursor])) ++cursor;
                if (cursor < end && ident_start(text[cursor])) {
                    size_t name = cursor;
                    while (cursor < end && word(text[cursor])) ++cursor;
                    size_t after = cursor;
                    while (after < end && (text[after] == ' ' || text[after] == '\t')) ++after;
                    if (after >= end || text[after] != '(') {
                        char *symbol = copy_n(text + name, cursor - name);
                        game->constants = grow(game->constants, &game->constant_capacity,
                                               game->constant_count, sizeof *game->constants);
                        Constant *constant = &game->constants[game->constant_count++];
                        constant->symbol = symbol;
                        constant->id = format("%s:constant:%s:%s", game_id, relative, symbol);
                        constant->source = copy_text(relative);
                        constant->line = spanning_line(text, start, line, floor);
                        constant->category = copy_text(category);
                        floor = cursor;
                    }
                }
            }
        }
        if (end < source->size) {
            start = end + 1;
            ++line;
        } else {
            start = end;
        }
    }
}

/* An assembly marker line: thumb/arm_func_start NAME or .type NAME, %KIND. */
static bool asm_symbol(const char *text, size_t start, size_t end, bool object, size_t *name,
                       size_t *name_end) {
    size_t cursor = start;
    while (cursor < end && space(text[cursor])) ++cursor;
    size_t finish = end;
    while (finish > cursor && space(text[finish - 1])) --finish;
    if (!object) {
        static const char *const starts[] = {"thumb_func_start", "arm_func_start"};
        for (size_t index = 0; index < 2; ++index) {
            size_t length = strlen(starts[index]);
            if (!strncmp(text + cursor, starts[index], length) && cursor + length < finish &&
                space(text[cursor + length])) {
                size_t at = cursor + length;
                while (at < finish && space(text[at])) ++at;
                if (at >= finish || !(ident_start(text[at]) || text[at] == '.' || text[at] == '$')) return false;
                size_t symbol = at++;
                while (at < finish && word(text[at])) ++at;
                if (at != finish) return false;
                *name = symbol;
                *name_end = at;
                return true;
            }
        }
    }
    if (strncmp(text + cursor, ".type", 5) || cursor + 5 >= finish || !space(text[cursor + 5])) return false;
    size_t at = cursor + 5;
    while (at < finish && space(text[at])) ++at;
    if (at >= finish || !(ident_start(text[at]) || text[at] == '.' || text[at] == '$')) return false;
    size_t symbol = at++;
    while (at < finish && word(text[at])) ++at;
    size_t symbol_end = at;
    while (at < finish && space(text[at])) ++at;
    if (at >= finish || text[at] != ',') return false;
    ++at;
    while (at < finish && space(text[at])) ++at;
    if (at < finish && text[at] == '%') ++at;
    const char *wanted = object ? "object" : "function";
    if ((size_t)(finish - at) != strlen(wanted) || strncmp(text + at, wanted, strlen(wanted))) return false;
    *name = symbol;
    *name_end = symbol_end;
    return true;
}

typedef struct {
    char *symbol;
    size_t offset, line;
} Marker;

static Marker *asm_markers(const Source *source, bool object, size_t *count) {
    Marker *markers = NULL;
    size_t capacity = 0, line = 1, blank_run = 0;
    *count = 0;
    const char *text = source->text;
    for (size_t start = 0; start < source->size;) {
        size_t end = start;
        while (end < source->size && text[end] != '\n') ++end;
        size_t name, name_end;
        if (asm_symbol(text, start, end, object, &name, &name_end)) {
            char *symbol = copy_n(text + name, name_end - name);
            bool known = false;
            for (size_t index = 0; index < *count && !known; ++index)
                known = !strcmp(markers[index].symbol, symbol);
            if (known && !object) {
                free(symbol);
            } else {
                markers = grow(markers, &capacity, *count, sizeof *markers);
                markers[(*count)++] = (Marker){symbol, start, line - blank_run};
            }
        }
        if (blank_line(text, start, end)) ++blank_run;
        else blank_run = 0;
        if (end < source->size) {
            start = end + 1;
            ++line;
        } else {
            start = end;
        }
    }
    return markers;
}

/* ^\s*SYMBOL:\s*(?:[@;].*)?$ after ``from``: the end of the label line. */
static size_t asm_label(const char *text, size_t size, size_t from, const char *symbol) {
    size_t wanted = strlen(symbol);
    for (size_t start = from; start < size;) {
        size_t end = start;
        while (end < size && text[end] != '\n') ++end;
        size_t cursor = start;
        while (cursor < end && space(text[cursor])) ++cursor;
        if (end - cursor > wanted && !strncmp(text + cursor, symbol, wanted) && text[cursor + wanted] == ':') {
            size_t after = cursor + wanted + 1;
            while (after < end && space(text[after])) ++after;
            if (after == end || text[after] == '@' || text[after] == ';') return end;
        }
        start = end < size ? end + 1 : end;
    }
    return SIZE_MAX;
}

static void asm_branches(const char *text, size_t from, size_t to, List *calls) {
    for (size_t start = from; start < to;) {
        size_t end = start;
        while (end < to && text[end] != '\n') ++end;
        size_t cursor = start;
        while (cursor < end && space(text[cursor])) ++cursor;
        static const char *const operations[] = {"bl", "blx", "b"};
        for (size_t operation = 0; operation < 3; ++operation) {
            size_t length = strlen(operations[operation]);
            if (strncmp(text + cursor, operations[operation], length) || cursor + length >= end ||
                !space(text[cursor + length]))
                continue;
            size_t at = cursor + length;
            while (at < end && space(text[at])) ++at;
            if (at < end && (ident_start(text[at]) || text[at] == '.' || text[at] == '$')) {
                size_t symbol = at++;
                while (at < end && word(text[at])) ++at;
                list_add_n(calls, text + symbol, at - symbol);
            }
            break;
        }
        start = end < to ? end + 1 : end;
    }
}

/* ASM_WORD_RE lines: [label:] .4byte|.word|.long SYMBOL [@ or ; comment]. */
static void asm_words(const char *text, size_t from, size_t to, List *refs) {
    for (size_t start = from; start < to;) {
        size_t end = start;
        while (end < to && text[end] != '\n') ++end;
        size_t cursor = start;
        while (cursor < end && space(text[cursor])) ++cursor;
        if (cursor < end && (ident_start(text[cursor]) || text[cursor] == '.' || text[cursor] == '$')) {
            size_t probe = cursor + 1;
            while (probe < end && word(text[probe])) ++probe;
            if (probe < end && text[probe] == ':') {
                cursor = probe + 1;
                while (cursor < end && space(text[cursor])) ++cursor;
            }
        }
        static const char *const directives[] = {".4byte", ".word", ".long"};
        for (size_t directive = 0; directive < 3; ++directive) {
            size_t length = strlen(directives[directive]);
            if (strncmp(text + cursor, directives[directive], length) || cursor + length >= end ||
                !space(text[cursor + length]))
                continue;
            size_t at = cursor + length;
            while (at < end && space(text[at])) ++at;
            if (at < end && (ident_start(text[at]) || text[at] == '.' || text[at] == '$')) {
                size_t symbol = at++;
                while (at < end && word(text[at])) ++at;
                size_t symbol_end = at;
                while (at < end && space(text[at])) ++at;
                if (at == end || text[at] == '@' || text[at] == ';')
                    list_add_n(refs, text + symbol, symbol_end - symbol);
            }
            break;
        }
        start = end < to ? end + 1 : end;
    }
}

static void discover_asm(const char *game_id, const char *relative, const Source *source, Game *game) {
    size_t count = 0;
    Marker *markers = asm_markers(source, false, &count);
    const char *category = category_of(game_id, relative);
    for (size_t index = 0; index < count; ++index) {
        size_t line_end = markers[index].offset;
        while (line_end < source->size && source->text[line_end] != '\n') ++line_end;
        size_t label = asm_label(source->text, source->size, line_end, markers[index].symbol);
        if (label == SIZE_MAX) continue;
        size_t body_end = index + 1 < count ? markers[index + 1].offset : source->size;
        Routine *routine = new_routine(game);
        routine->symbol = copy_text(markers[index].symbol);
        routine->id = format("%s:asm:%s:%s", game_id, relative, routine->symbol);
        routine->kind = copy_text("asm_function");
        routine->source = copy_text(relative);
        routine->line = markers[index].line;
        routine->linkage = copy_text("external");
        routine->category = copy_text(category);
        routine->address = source_address(routine->symbol);
        if (label < body_end) {
            asm_branches(source->text, label, body_end, &routine->raw_calls);
            asm_words(source->text, label, body_end, &routine->raw_refs);
        }
        list_unique(&routine->raw_calls);
        list_unique(&routine->raw_refs);
    }
    for (size_t index = 0; index < count; ++index) free(markers[index].symbol);
    free(markers);
    markers = asm_markers(source, true, &count);
    for (size_t index = 0; index < count; ++index) {
        Data *data = new_data(game);
        data->symbol = markers[index].symbol;
        data->id = format("%s:data:%s:%s", game_id, relative, data->symbol);
        data->kind = copy_text("asm_data");
        data->source = copy_text(relative);
        data->line = markers[index].line;
        data->linkage = copy_text("external");
        data->category = copy_text(category);
        data->address = source_address(data->symbol);
    }
    free(markers);
}

/* TYPEDEF_FP_RE: typedef ... (*NAME)(...); with no ; { } inside. */
static void fp_typedefs(const char *masked, size_t size, Map *typedefs) {
    for (size_t index = 0; index + 7 < size; ++index) {
        if ((index && word(masked[index - 1])) || strncmp(masked + index, "typedef", 7) ||
            !space(masked[index + 7]))
            continue;
        size_t end = index + 7;
        while (end < size && masked[end] != ';' && masked[end] != '{' && masked[end] != '}') ++end;
        if (end >= size || masked[end] != ';') continue;
        size_t finish = rstrip(masked, index, end);
        if (finish <= index || masked[finish - 1] != ')') continue;
        /* The rightmost (*NAME)( whose parameter list closes the statement. */
        size_t best_start = 0, best_end = 0;
        bool found = false;
        for (size_t probe = index + 8; probe < finish; ++probe) {
            size_t name, name_end, match;
            if (!pointer_name(masked, probe, finish, &name, &name_end, &match, true)) break;
            found = true;
            best_start = name;
            best_end = name_end;
            probe = match;
        }
        if (found) {
            char *symbol = copy_n(masked + best_start, best_end - best_start);
            if (!map_slot(typedefs, symbol, false)) map_add(typedefs, symbol, 0);
            free(symbol);
        }
        index = end;
    }
}

/* ---- Paths ---------------------------------------------------------------- */

static int compare_paths(const void *left, const void *right) {
    const char *a = *(char *const *)left, *b = *(char *const *)right;
    for (;;) {
        size_t la = strcspn(a, "/"), lb = strcspn(b, "/");
        size_t common = la < lb ? la : lb;
        int order = memcmp(a, b, common);
        if (order) return order;
        if (la != lb) return la < lb ? -1 : 1;
        if (!a[la] || !b[lb]) return a[la] ? 1 : b[lb] ? -1 : 0;
        a += la + 1;
        b += lb + 1;
    }
}

/* Path(name).suffix.lower(). */
static void suffix_of(const char *path, char *suffix, size_t size) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const char *dot = strrchr(name, '.');
    suffix[0] = '\0';
    if (!dot || dot == name || !dot[1]) return;
    size_t used = 0;
    for (; *dot && used + 1 < size; ++dot) suffix[used++] = (char)tolower((unsigned char)*dot);
    suffix[used] = '\0';
}

static bool source_suffix(const char *suffix) {
    return !strcmp(suffix, ".c") || !strcmp(suffix, ".h") || !strcmp(suffix, ".s") ||
           !strcmp(suffix, ".inc");
}

static void collect(const char *root, const char *relative, List *paths) {
    char directory[8192];
    snprintf(directory, sizeof directory, "%s%s%s", root, *relative ? "/" : "", relative);
    DIR *handle = opendir(directory);
    if (!handle) return;
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[8192], full[16384];
        snprintf(child, sizeof child, "%s%s%s", relative, *relative ? "/" : "", entry->d_name);
        snprintf(full, sizeof full, "%s/%s", root, child);
        struct stat link, status;
        if (lstat(full, &link)) continue;
        if (S_ISDIR(link.st_mode)) {
            collect(root, child, paths);
        } else if (!stat(full, &status) && S_ISREG(status.st_mode)) {
            char suffix[32];
            suffix_of(child, suffix, sizeof suffix);
            if (source_suffix(suffix)) list_add(paths, child);
        }
    }
    closedir(handle);
}

/* ---- Resolution ------------------------------------------------------------ */

static char *stem_of(const char *source) {
    const char *slash = strrchr(source, '/');
    const char *name = slash ? slash + 1 : source;
    const char *dot = strrchr(name, '.');
    return dot && dot != name ? copy_n(source, (size_t)(dot - source)) : copy_text(source);
}

/* resolve_unique(): the one routine of that symbol, or the one in that file. */
static const char *resolve_unique(const Game *game, const Map *by_symbol, const char *symbol,
                                  const char *source) {
    Slot *slot = map_slot((Map *)by_symbol, symbol, false);
    if (!slot) return NULL;
    if (slot->count == 1) return game->routines[slot->values[0]].id;
    const char *found = NULL;
    size_t same = 0;
    for (size_t index = 0; index < slot->count; ++index)
        if (!strcmp(game->routines[slot->values[index]].source, source)) {
            found = game->routines[slot->values[index]].id;
            ++same;
        }
    return same == 1 ? found : NULL;
}

static int compare_ids(const char *left, const char *right) {
    return strcmp(left, right);
}

#define STABLE_SORT(items, count, type)                                                    \
    do {                                                                                   \
        type *scratch = malloc(((count) ? (count) : 1) * sizeof(type));                     \
        if (!scratch) oom();                                                               \
        for (size_t width = 1; width < (count); width *= 2) {                              \
            for (size_t begin = 0; begin < (count); begin += 2 * width) {                  \
                size_t middle = begin + width < (count) ? begin + width : (count);         \
                size_t finish = begin + 2 * width < (count) ? begin + 2 * width : (count); \
                size_t left = begin, right = middle, out = begin;                          \
                while (left < middle && right < finish)                                    \
                    scratch[out++] = compare_ids((items)[right].id, (items)[left].id) < 0  \
                                         ? (items)[right++] : (items)[left++];             \
                while (left < middle) scratch[out++] = (items)[left++];                    \
                while (right < finish) scratch[out++] = (items)[right++];                  \
            }                                                                              \
            memcpy((items), scratch, (count) * sizeof(type));                              \
        }                                                                                  \
        free(scratch);                                                                     \
    } while (0)

static FusionJson string_array(const List *list) {
    FusionJson array = fusion_json_array();
    for (size_t index = 0; index < list->count; ++index)
        if (!fusion_json_push(&array, fusion_json_string(list->items[index]))) oom();
    return array;
}

static void put(FusionJson *object, const char *key, FusionJson value) {
    if (!fusion_json_set(object, key, value)) oom();
}

static void put_text(FusionJson *object, const char *key, const char *value) {
    put(object, key, value ? fusion_json_string(value) : fusion_json_null());
}

static void put_int(FusionJson *object, const char *key, int64_t value) {
    put(object, key, fusion_json_int(value));
}

/* NAME_CATEGORY_RULES: the category a native name proves, or NULL. */
static bool all_word(const char *start, const char *end) {
    for (const char *c = start; c < end; ++c)
        if (!word(*c)) return false;
    return true;
}

static bool ends_with(const char *text, const char *suffix) {
    size_t length = strlen(text), wanted = strlen(suffix);
    return length >= wanted && !strcmp(text + length - wanted, suffix);
}

static bool prefix_word(const char *symbol, const char *prefix, const char *const *suffixes,
                        size_t suffix_count, bool hex_middle) {
    size_t length = strlen(symbol), prefix_length = strlen(prefix);
    if (strncmp(symbol, prefix, prefix_length) || !all_word(symbol, symbol + length)) return false;
    for (size_t index = 0; index < suffix_count; ++index) {
        size_t suffix_length = strlen(suffixes[index]);
        if (length < prefix_length + suffix_length || !ends_with(symbol, suffixes[index])) continue;
        size_t middle = length - prefix_length - suffix_length;
        if (hex_middle) {
            if (middle != 2) continue;
            const char *digits = symbol + prefix_length;
            bool hex = true;
            for (int digit = 0; digit < 2; ++digit)
                hex &= isdigit((unsigned char)digits[digit]) || (digits[digit] >= 'A' && digits[digit] <= 'F');
            if (hex) return true;
        } else if (middle >= 1) {
            return true;
        }
    }
    return false;
}

static bool starts(const char *symbol, const char *prefix) {
    return !strncmp(symbol, prefix, strlen(prefix));
}

static bool exact(const char *symbol, const char *const *names, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (!strcmp(symbol, names[index])) return true;
    return false;
}

static const char *name_category(const char *symbol) {
    static const char *const create_update[] = {"Create", "Update"};
    static const char *const use_update[] = {"Use", "Update"};
    if (prefix_word(symbol, "Enemy", create_update, 2, false)) return "enemies";
    if (prefix_word(symbol, "Object", create_update, 2, true)) return "environment";
    if (prefix_word(symbol, "Skill", use_update, 2, false)) return "abilities";
    if (starts(symbol, "SoulInventory_")) return "inventory";
    if (starts(symbol, "GameModeTitleScreen") || starts(symbol, "GameModeKonamiLogo") ||
        starts(symbol, "GameModeLicensedByNintendo"))
        return "title";
    if (starts(symbol, "TitleScreen")) return "title";
    if (starts(symbol, "GameModeMainMenu") || starts(symbol, "GameModeBossRushMenu") ||
        starts(symbol, "GameModeSoundTestMenu") || starts(symbol, "GameModeSoulTradeMenu"))
        return "menus";
    if (starts(symbol, "GameModeCredits")) return "endings";
    if (starts(symbol, "GameModeGameOver")) return "death";
    if (starts(symbol, "GameModeIntroCutscene")) return "cutscenes";
    if (starts(symbol, "GameModeDebug")) return "technical";
    static const char *const startup[] = {"GameModeUpdate", "GameModeReset", "SetGameMode"};
    if (exact(symbol, startup, COUNT(startup))) return "startup";
    static const char *const technical[] = {"BgAffineSet", "ArcTan2", "Sqrt", "Div", "Mod",
                                            "CpuSet", "CpuFastSet", "RandomNumberGenerator"};
    size_t length = strlen(symbol);
    bool words = all_word(symbol, symbol + length);
    if (exact(symbol, technical, COUNT(technical)) ||
        (words && ((starts(symbol, "Entity") && length > 6) ||
                   (starts(symbol, "BgCmdBuffer_") && length > 12) || starts(symbol, "DmaFill") ||
                   starts(symbol, "DmaCopy") || starts(symbol, "Intr"))))
        return "technical";
    if (starts(symbol, "m4a") || starts(symbol, "MP2K") || starts(symbol, "MPlay") ||
        starts(symbol, "Cgb") || starts(symbol, "Midi") || starts(symbol, "Sound") ||
        starts(symbol, "ply_"))
        return "audio";
    if (starts(symbol, "Sram") || starts(symbol, "Save")) return "saves";
    static const char *const input[] = {"UpdateInput", "SetPlayerInput"};
    if (exact(symbol, input, 2)) return "input";
    if (!strcmp(symbol, "CheckRoomTransition")) return "transitions";
    static const char *const rooms[] = {"GetRoomPointer", "GetRoomFromMapPosition",
                                        "GetAreaFromMapPosition", "GetSaveRoomFlagFromMapPosition",
                                        "GetWarpRoomFlagFromMapPosition"};
    if (exact(symbol, rooms, COUNT(rooms))) return "rooms";
    return NULL;
}

/* _classify_by_evidence(): name rules, then exclusive referrers. */
static void classify(Game *game) {
    Map by_id = {0};
    for (size_t index = 0; index < game->routine_count; ++index) {
        Routine *routine = &game->routines[index];
        Slot *slot = map_slot(&by_id, routine->id, true);
        slot->count = 0;
        map_add(&by_id, routine->id, index);
        if (strcmp(routine->category, "unclassified")) {
            routine->evidence = copy_text("path");
            continue;
        }
        const char *category = name_category(routine->symbol);
        if (category) {
            free(routine->category);
            routine->category = copy_text(category);
            routine->evidence = copy_text("name");
        }
    }
    /* referrers[target] = the routines calling it or taking its address. */
    Map referrers = {0};
    for (size_t index = 0; index < game->routine_count; ++index) {
        Routine *routine = &game->routines[index];
        List targets = {0};
        for (size_t item = 0; item < routine->calls.count; ++item) list_add(&targets, routine->calls.items[item]);
        for (size_t item = 0; item < routine->address_refs.count; ++item)
            list_add(&targets, routine->address_refs.items[item]);
        for (size_t item = 0; item < routine->callbacks.count; ++item)
            list_add(&targets, fusion_json_member(&routine->callbacks.items[item], "target")->string);
        list_unique(&targets);
        for (size_t item = 0; item < targets.count; ++item) {
            if (!strcmp(targets.items[item], routine->id)) continue;
            Slot *slot = map_slot(&referrers, targets.items[item], true);
            bool present = false;
            for (size_t value = 0; value < slot->count && !present; ++value) present = slot->values[value] == index;
            if (!present) map_add(&referrers, targets.items[item], index);
        }
        list_free(&targets);
    }
    for (bool changed = true; changed;) {
        changed = false;
        for (size_t index = 0; index < game->routine_count; ++index) {
            Routine *routine = &game->routines[index];
            if (strcmp(routine->category, "unclassified")) continue;
            Slot *slot = map_slot(&referrers, routine->id, false);
            if (!slot || !slot->count) continue;
            const char *only = NULL;
            bool single = true;
            for (size_t value = 0; value < slot->count && single; ++value) {
                /* by_id: the last routine of that id. */
                const char *referrer_id = game->routines[slot->values[value]].id;
                Slot *owner = map_slot(&by_id, referrer_id, false);
                const char *category = game->routines[owner->values[owner->count - 1]].category;
                if (!only) only = category;
                else if (strcmp(only, category)) single = false;
            }
            if (single && strcmp(only, "unclassified")) {
                free(routine->category);
                routine->category = copy_text(only);
                free(routine->evidence);
                routine->evidence = copy_text("referrers");
                changed = true;
            }
        }
    }
    map_free(&by_id);
    map_free(&referrers);
}

static bool git_revision(const char *root, char *revision, size_t size) {
    char command[8192];
    snprintf(command, sizeof command, "git -C '%s' rev-parse HEAD 2>/dev/null", root);
    FILE *pipe = popen(command, "r");
    if (!pipe) return false;
    bool ok = fgets(revision, (int)size, pipe) != NULL;
    int status = pclose(pipe);
    if (!ok || status) return false;
    revision[strcspn(revision, "\r\n")] = '\0';
    return true;
}

static void member_callback(FusionJson *list, const char *member, const char *target,
                            const Map *fp_types, const List *type_ids) {
    FusionJson callback = fusion_json_object(), types = fusion_json_array();
    Slot *slot = map_slot((Map *)fp_types, member, false);
    List ids = {0};
    for (size_t index = 0; slot && index < slot->count; ++index) list_add(&ids, type_ids->items[slot->values[index]]);
    if (ids.count) qsort(ids.items, ids.count, sizeof *ids.items, compare_strings);
    for (size_t index = 0; index < ids.count; ++index)
        if (!fusion_json_push(&types, fusion_json_string(ids.items[index]))) oom();
    list_free(&ids);
    put_text(&callback, "member", member);
    put_text(&callback, "target", target);
    put(&callback, "types", types);
    if (!fusion_json_push(list, callback)) oom();
}

/* inventory_game(): one game's records, statistics and coverage. */
static bool inventory_game(const char *game_id, const char *title, const char *source_relative,
                           const char *root, FusionJson *result) {
    char source_root[8192];
    snprintf(source_root, sizeof source_root, "%s/%s", root, source_relative);
    struct stat status;
    if (stat(source_root, &status) || !S_ISDIR(status.st_mode))
        return fail("missing pinned source tree: %s", source_root);
    List paths = {0};
    collect(source_root, "", &paths);
    if (paths.count) qsort(paths.items, paths.count, sizeof *paths.items, compare_paths);
    Game game = {0};
    Map typedefs = {0};
    Source *sources = calloc(paths.count + 1, sizeof *sources);
    if (!sources) oom();
    int64_t file_counts[4] = {0, 0, 0, 0};
    for (size_t index = 0; index < paths.count; ++index) {
        char path[16384], suffix[32];
        snprintf(path, sizeof path, "%s/%s", source_root, paths.items[index]);
        if (!load_source(path, &sources[index])) {
            for (size_t other = 0; other <= index; ++other) source_free(&sources[other]);
            free(sources);
            list_free(&paths);
            return false;
        }
        suffix_of(paths.items[index], suffix, sizeof suffix);
        file_counts[!strcmp(suffix, ".c") ? 0 : !strcmp(suffix, ".h") ? 1 : !strcmp(suffix, ".s") ? 2 : 3]++;
        if (!strcmp(suffix, ".c") || !strcmp(suffix, ".h")) {
            char *masked = mask_c(sources[index].text, sources[index].size);
            mask_preprocessor(masked, sources[index].size);
            fp_typedefs(masked, sources[index].size, &typedefs);
            free(masked);
        }
    }
    for (size_t index = 0; index < paths.count; ++index) {
        const char *relative = paths.items[index];
        char suffix[32];
        suffix_of(relative, suffix, sizeof suffix);
        Source *source = &sources[index];
        if (!strcmp(suffix, ".c") || !strcmp(suffix, ".h")) {
            char *masked = mask_c(source->text, source->size);
            mask_preprocessor(masked, source->size);
            if (!strcmp(suffix, ".c")) {
                discover_c(game_id, relative, source, &game);
                discover_c_data(game_id, relative, masked, source->size, &game);
            } else {
                discover_declarations(game_id, relative, masked, source->size, &game);
            }
            discover_types(game_id, relative, masked, source->size, &typedefs, &game);
            discover_constants(game_id, relative, source, &game);
            free(masked);
        } else if (!strcmp(suffix, ".s")) {
            discover_asm(game_id, relative, source, &game);
        } else {
            discover_constants(game_id, relative, source, &game);
        }
    }
    /* Assembly routines and data that duplicate a C definition. */
    {
        Map c_definitions = {0}, c_external = {0};
        for (size_t index = 0; index < game.routine_count; ++index) {
            Routine *routine = &game.routines[index];
            if (strcmp(routine->kind, "c_function")) continue;
            char *stem = stem_of(routine->source), *key = format("%s\t%s", stem, routine->symbol);
            Slot *slot = map_slot(&c_definitions, key, true);
            slot->count = 0;
            map_add(&c_definitions, key, index);
            if (!strcmp(routine->linkage, "external")) map_add(&c_external, routine->symbol, index);
            free(stem);
            free(key);
        }
        size_t kept = 0;
        bool *drop = calloc(game.routine_count + 1, sizeof *drop);
        for (size_t index = 0; index < game.routine_count; ++index) {
            Routine *routine = &game.routines[index];
            char *stem = stem_of(routine->source), *key = format("%s\t%s", stem, routine->symbol);
            Slot *match = map_slot(&c_definitions, key, false);
            long target = match ? (long)match->values[0] : -1;
            if (target < 0 && !strcmp(routine->kind, "asm_function")) {
                Slot *external = map_slot(&c_external, routine->symbol, false);
                if (external && external->count == 1) target = (long)external->values[0];
            }
            if (!strcmp(routine->kind, "asm_function") && target >= 0) {
                list_add(&game.routines[target].variants, routine->source);
                drop[index] = true;
            }
            free(stem);
            free(key);
        }
        for (size_t index = 0; index < game.routine_count; ++index) {
            if (drop[index]) {
                Routine *routine = &game.routines[index];
                free(routine->id);
                free(routine->symbol);
                free(routine->kind);
                free(routine->source);
                free(routine->linkage);
                free(routine->category);
                free(routine->description);
                free(routine->address);
                list_free(&routine->raw_calls);
                list_free(&routine->raw_refs);
                list_free(&routine->raw_tables);
                list_free(&routine->variants);
                pairs_free(&routine->raw_writes);
                fusion_json_free(&routine->indirect);
                fusion_json_free(&routine->callbacks);
                continue;
            }
            game.routines[kept++] = game.routines[index];
        }
        game.routine_count = kept;
        free(drop);
        map_free(&c_definitions);
        map_free(&c_external);
        Map data_definitions = {0}, data_external = {0};
        for (size_t index = 0; index < game.data_count; ++index) {
            Data *data = &game.data[index];
            if (strcmp(data->kind, "c_data")) continue;
            char *stem = stem_of(data->source), *key = format("%s\t%s", stem, data->symbol);
            Slot *slot = map_slot(&data_definitions, key, true);
            slot->count = 0;
            map_add(&data_definitions, key, index);
            if (!strcmp(data->linkage, "external")) map_add(&data_external, data->symbol, index);
            free(stem);
            free(key);
        }
        kept = 0;
        drop = calloc(game.data_count + 1, sizeof *drop);
        for (size_t index = 0; index < game.data_count; ++index) {
            Data *data = &game.data[index];
            if (strcmp(data->kind, "asm_data")) continue;
            char *stem = stem_of(data->source), *key = format("%s\t%s", stem, data->symbol);
            Slot *match = map_slot(&data_definitions, key, false);
            long target = match ? (long)match->values[0] : -1;
            if (target < 0) {
                Slot *external = map_slot(&data_external, data->symbol, false);
                if (external && external->count == 1) target = (long)external->values[0];
            }
            if (target >= 0) {
                list_add(&game.data[target].variants, data->source);
                drop[index] = true;
            }
            free(stem);
            free(key);
        }
        for (size_t index = 0; index < game.data_count; ++index) {
            if (drop[index]) {
                Data *data = &game.data[index];
                free(data->id);
                free(data->symbol);
                free(data->kind);
                free(data->source);
                free(data->linkage);
                free(data->category);
                free(data->address);
                list_free(&data->raw_refs);
                pairs_free(&data->raw_inits);
                fusion_json_free(&data->callbacks);
                continue;
            }
            game.data[kept++] = game.data[index];
        }
        game.data_count = kept;
        free(drop);
        map_free(&data_definitions);
        map_free(&data_external);
    }
    Map by_symbol = {0};
    for (size_t index = 0; index < game.routine_count; ++index)
        map_add(&by_symbol, game.routines[index].symbol, index);
    /* fp_member_types: member -> the types declaring it. */
    Map fp_types = {0};
    List type_ids = {0};
    for (size_t index = 0; index < game.type_count; ++index) {
        list_add(&type_ids, game.types[index].id);
        for (size_t member = 0; member < game.types[index].fp_members.count; ++member)
            map_add(&fp_types, game.types[index].fp_members.items[member], index);
    }
    int64_t callback_edges = 0, address_edges = 0, indirect_edges = 0;
    Map data_by_symbol = {0};
    for (size_t index = 0; index < game.data_count; ++index) {
        Data *data = &game.data[index];
        for (size_t item = 0; item < data->raw_refs.count; ++item) {
            const char *resolved = resolve_unique(&game, &by_symbol, data->raw_refs.items[item], data->source);
            if (resolved) list_add(&data->targets, resolved);
        }
        list_unique(&data->targets);
        data->pointer_table = data->targets.count > 0;
        for (size_t item = 0; item < data->raw_inits.count; ++item) {
            const char *member = data->raw_inits.items[item].first;
            if (!map_slot(&fp_types, member, false)) continue;
            const char *resolved = resolve_unique(&game, &by_symbol, data->raw_inits.items[item].second,
                                                  data->source);
            if (!resolved) continue;
            ++callback_edges;
            member_callback(&data->callbacks, member, resolved, &fp_types, &type_ids);
        }
        map_add(&data_by_symbol, data->symbol, index);
    }
    for (size_t index = 0; index < game.routine_count; ++index) {
        Routine *routine = &game.routines[index];
        for (size_t item = 0; item < routine->raw_calls.count; ++item) {
            const char *symbol = routine->raw_calls.items[item];
            Slot *slot = map_slot(&by_symbol, symbol, false);
            if (!slot) continue;
            if (slot->count == 1) {
                list_add(&routine->calls, game.routines[slot->values[0]].id);
                continue;
            }
            const char *found = NULL;
            size_t same = 0;
            for (size_t value = 0; value < slot->count; ++value)
                if (!strcmp(game.routines[slot->values[value]].source, routine->source)) {
                    found = game.routines[slot->values[value]].id;
                    ++same;
                }
            if (same == 1) list_add(&routine->calls, found);
            else list_add(&routine->ambiguous, symbol);
        }
        for (size_t item = 0; item < routine->raw_tables.count; ++item) {
            Slot *slot = map_slot(&data_by_symbol, routine->raw_tables.items[item], false);
            size_t tables[2], table_count = 0, same_file = 0, same_index = 0;
            for (size_t value = 0; slot && value < slot->count; ++value) {
                Data *data = &game.data[slot->values[value]];
                if (!data->targets.count) continue;
                if (table_count < 2) tables[table_count] = slot->values[value];
                ++table_count;
                if (!strcmp(data->source, routine->source)) {
                    ++same_file;
                    same_index = slot->values[value];
                }
            }
            size_t chosen;
            if (table_count == 1) chosen = tables[0];
            else if (table_count > 1 && same_file == 1) chosen = same_index;
            else continue;
            Data *table = &game.data[chosen];
            for (size_t target = 0; target < table->targets.count; ++target)
                list_add(&routine->calls, table->targets.items[target]);
            indirect_edges += (int64_t)table->targets.count;
            FusionJson call = fusion_json_object();
            put_text(&call, "table", table->id);
            put(&call, "targets", string_array(&table->targets));
            if (!fusion_json_push(&routine->indirect, call)) oom();
        }
        list_unique(&routine->calls);
        list_unique(&routine->ambiguous);
        for (size_t item = 0; item < routine->raw_writes.count; ++item) {
            const char *member = routine->raw_writes.items[item].first;
            if (!map_slot(&fp_types, member, false)) continue;
            const char *resolved = resolve_unique(&game, &by_symbol, routine->raw_writes.items[item].second,
                                                  routine->source);
            if (!resolved) continue;
            ++callback_edges;
            member_callback(&routine->callbacks, member, resolved, &fp_types, &type_ids);
        }
        for (size_t item = 0; item < routine->raw_refs.count; ++item) {
            const char *symbol = routine->raw_refs.items[item];
            if (!strcmp(symbol, routine->symbol)) continue;
            const char *resolved = resolve_unique(&game, &by_symbol, symbol, routine->source);
            if (resolved) list_add(&routine->address_refs, resolved);
        }
        list_unique(&routine->address_refs);
        address_edges += (int64_t)routine->address_refs.count;
    }
    STABLE_SORT(game.routines, game.routine_count, Routine);
    Map definitions = {0};
    for (size_t index = 0; index < game.routine_count; ++index)
        map_add(&definitions, game.routines[index].symbol, index);
    for (size_t index = 0; index < game.declaration_count; ++index) {
        Slot *slot = map_slot(&definitions, game.declarations[index].symbol, false);
        for (size_t value = 0; slot && value < slot->count; ++value)
            list_add(&game.declarations[index].definitions, game.routines[slot->values[value]].id);
        List *list = &game.declarations[index].definitions;
        if (list->count) qsort(list->items, list->count, sizeof *list->items, compare_strings);
    }
    STABLE_SORT(game.declarations, game.declaration_count, Declaration);
    STABLE_SORT(game.data, game.data_count, Data);
    STABLE_SORT(game.types, game.type_count, Type);
    STABLE_SORT(game.constants, game.constant_count, Constant);
    classify(&game);
    /* The JSON records. */
    FusionJson routines = fusion_json_array(), declarations = fusion_json_array(),
               data_symbols = fusion_json_array(), types = fusion_json_array(),
               constants = fusion_json_array();
    FusionJson by_category = fusion_json_object(), by_evidence = fusion_json_object();
    int64_t with_address = 0, call_edges = 0;
    for (size_t index = 0; index < game.routine_count; ++index) {
        Routine *routine = &game.routines[index];
        FusionJson record = fusion_json_object();
        put_text(&record, "id", routine->id);
        put_text(&record, "symbol", routine->symbol);
        put_text(&record, "kind", routine->kind);
        put_text(&record, "source", routine->source);
        put_int(&record, "line", (int64_t)routine->line);
        put_text(&record, "linkage", routine->linkage);
        put_text(&record, "category", routine->category);
        put_text(&record, "description", routine->description);
        put_text(&record, "address", routine->address);
        if (routine->has_body) put_int(&record, "_body_start", (int64_t)routine->body_start);
        put(&record, "calls", string_array(&routine->calls));
        put(&record, "ambiguous_calls", string_array(&routine->ambiguous));
        put(&record, "indirect_calls", routine->indirect);
        routine->indirect = fusion_json_null();
        put(&record, "member_callbacks", routine->callbacks);
        routine->callbacks = fusion_json_null();
        put(&record, "address_references", string_array(&routine->address_refs));
        put(&record, "source_variants", string_array(&routine->variants));
        if (routine->evidence) put_text(&record, "category_evidence", routine->evidence);
        if (!fusion_json_push(&routines, record)) oom();
        with_address += routine->address != NULL;
        call_edges += (int64_t)routine->calls.count;
        FusionJson *count = fusion_json_member_mut(&by_category, routine->category);
        if (count) ++count->integer;
        else put_int(&by_category, routine->category, 1);
        const char *evidence = routine->evidence ? routine->evidence : "none";
        count = fusion_json_member_mut(&by_evidence, evidence);
        if (count) ++count->integer;
        else put_int(&by_evidence, evidence, 1);
    }
    int64_t without_definition = 0;
    for (size_t index = 0; index < game.declaration_count; ++index) {
        Declaration *declaration = &game.declarations[index];
        FusionJson record = fusion_json_object();
        put_text(&record, "id", declaration->id);
        put_text(&record, "symbol", declaration->symbol);
        put_text(&record, "source", declaration->source);
        put_int(&record, "line", (int64_t)declaration->line);
        put_text(&record, "category", declaration->category);
        put_text(&record, "linkage", declaration->linkage);
        put(&record, "definition_ids", string_array(&declaration->definitions));
        without_definition += declaration->definitions.count == 0;
        if (!fusion_json_push(&declarations, record)) oom();
    }
    int64_t pointer_tables = 0;
    for (size_t index = 0; index < game.data_count; ++index) {
        Data *data = &game.data[index];
        FusionJson record = fusion_json_object();
        put_text(&record, "id", data->id);
        put_text(&record, "symbol", data->symbol);
        put_text(&record, "kind", data->kind);
        put_text(&record, "source", data->source);
        put_int(&record, "line", (int64_t)data->line);
        put_text(&record, "linkage", data->linkage);
        put_text(&record, "category", data->category);
        put_text(&record, "address", data->address);
        put(&record, "pointer_targets", string_array(&data->targets));
        if (data->pointer_table) put(&record, "pointer_table", fusion_json_bool(true));
        put(&record, "member_callbacks", data->callbacks);
        data->callbacks = fusion_json_null();
        put(&record, "source_variants", string_array(&data->variants));
        pointer_tables += data->pointer_table;
        if (!fusion_json_push(&data_symbols, record)) oom();
    }
    for (size_t index = 0; index < game.type_count; ++index) {
        Type *type = &game.types[index];
        FusionJson record = fusion_json_object();
        put_text(&record, "id", type->id);
        put_text(&record, "symbol", type->symbol);
        put_text(&record, "kind", type->kind);
        put_text(&record, "source", type->source);
        put_int(&record, "line", (int64_t)type->line);
        put_text(&record, "category", type->category);
        if (type->anonymous) put(&record, "anonymous", fusion_json_bool(true));
        put(&record, "members", string_array(&type->members));
        put(&record, "function_pointer_members", string_array(&type->fp_members));
        if (!fusion_json_push(&types, record)) oom();
    }
    for (size_t index = 0; index < game.constant_count; ++index) {
        Constant *constant = &game.constants[index];
        FusionJson record = fusion_json_object();
        put_text(&record, "id", constant->id);
        put_text(&record, "symbol", constant->symbol);
        put_text(&record, "source", constant->source);
        put_int(&record, "line", (int64_t)constant->line);
        put_text(&record, "category", constant->category);
        if (!fusion_json_push(&constants, record)) oom();
    }
    /* _source_digest(): every path and its bytes. */
    size_t total = 0;
    for (size_t index = 0; index < paths.count; ++index)
        total += strlen(paths.items[index]) + sources[index].raw_size + 2;
    uint8_t *joined = malloc(total + 1);
    if (!joined) oom();
    size_t used = 0;
    for (size_t index = 0; index < paths.count; ++index) {
        size_t length = strlen(paths.items[index]);
        memcpy(joined + used, paths.items[index], length);
        used += length;
        joined[used++] = 0;
        memcpy(joined + used, sources[index].raw, sources[index].raw_size);
        used += sources[index].raw_size;
        joined[used++] = 0;
    }
    char digest[65], revision[128];
    if (!fusion_sha256_hex(joined, used, digest)) oom();
    free(joined);
    if (!git_revision(source_root, revision, sizeof revision))
        return fail("cannot read the revision of %s", source_root);
    *result = fusion_json_object();
    put_text(result, "title", title);
    put_text(result, "source_root", source_relative);
    put_text(result, "source_revision", revision);
    put_text(result, "source_digest_sha256", digest);
    FusionJson coverage = fusion_json_object(), missing = fusion_json_array();
    put_text(&coverage, "definitions", "C definitions and marked assembly functions");
    put_text(&coverage, "declarations", "header function declarations linked by symbol");
    put_text(&coverage, "data", "top-level C definitions and assembly object markers");
    put_text(&coverage, "types", "named struct, union and enum definitions with lexical members");
    put_text(&coverage, "constants", "object-like preprocessor definitions");
    put_text(&coverage, "calls", "direct lexical calls, assembly branches and proven table dispatch");
    put_text(&coverage, "member_callbacks", "lexically proven function-pointer member writes");
    put_text(&coverage, "address_references",
             "routines whose address a routine takes without calling it (C values, assembly literal "
             "pools): possible callback targets, not proven calls");
    static const char *const pending[] = {"which stored callback a dynamic dispatch actually runs",
                                          "callbacks built from computed addresses",
                                          "runtime-observed dependencies"};
    for (size_t index = 0; index < 3; ++index)
        if (!fusion_json_push(&missing, fusion_json_string(pending[index]))) oom();
    put(&coverage, "not_yet_indexed", missing);
    put(result, "coverage", coverage);
    FusionJson files = fusion_json_object();
    put_int(&files, "c", file_counts[0]);
    put_int(&files, "headers", file_counts[1]);
    put_int(&files, "assembly", file_counts[2]);
    put_int(&files, "includes", file_counts[3]);
    put(result, "files", files);
    FusionJson statistics = fusion_json_object();
    put_int(&statistics, "routines", (int64_t)game.routine_count);
    put_int(&statistics, "declarations", (int64_t)game.declaration_count);
    put_int(&statistics, "declarations_without_definition", without_definition);
    put_int(&statistics, "data_symbols", (int64_t)game.data_count);
    put_int(&statistics, "pointer_tables", pointer_tables);
    put_int(&statistics, "types", (int64_t)game.type_count);
    put_int(&statistics, "constants", (int64_t)game.constant_count);
    put_int(&statistics, "with_source_address", with_address);
    put_int(&statistics, "resolved_call_edges", call_edges);
    put_int(&statistics, "resolved_indirect_call_edges", indirect_edges);
    put_int(&statistics, "resolved_member_callback_edges", callback_edges);
    put_int(&statistics, "address_reference_edges", address_edges);
    put(&statistics, "by_category", by_category);
    put(&statistics, "by_category_evidence", by_evidence);
    put(result, "statistics", statistics);
    put(result, "routines", routines);
    put(result, "declarations", declarations);
    put(result, "data_symbols", data_symbols);
    put(result, "types", types);
    put(result, "constants", constants);
    /* Release the scan. */
    for (size_t index = 0; index < game.routine_count; ++index) {
        Routine *routine = &game.routines[index];
        free(routine->id);
        free(routine->symbol);
        free(routine->kind);
        free(routine->source);
        free(routine->linkage);
        free(routine->category);
        free(routine->description);
        free(routine->address);
        free(routine->evidence);
        list_free(&routine->raw_calls);
        list_free(&routine->raw_tables);
        list_free(&routine->raw_refs);
        list_free(&routine->variants);
        list_free(&routine->calls);
        list_free(&routine->ambiguous);
        list_free(&routine->address_refs);
        pairs_free(&routine->raw_writes);
    }
    for (size_t index = 0; index < game.declaration_count; ++index) {
        free(game.declarations[index].id);
        free(game.declarations[index].symbol);
        free(game.declarations[index].source);
        free(game.declarations[index].category);
        free(game.declarations[index].linkage);
        list_free(&game.declarations[index].definitions);
    }
    for (size_t index = 0; index < game.data_count; ++index) {
        Data *data = &game.data[index];
        free(data->id);
        free(data->symbol);
        free(data->kind);
        free(data->source);
        free(data->linkage);
        free(data->category);
        free(data->address);
        list_free(&data->raw_refs);
        list_free(&data->targets);
        list_free(&data->variants);
        pairs_free(&data->raw_inits);
    }
    for (size_t index = 0; index < game.type_count; ++index) {
        free(game.types[index].id);
        free(game.types[index].symbol);
        free(game.types[index].kind);
        free(game.types[index].source);
        free(game.types[index].category);
        list_free(&game.types[index].members);
        list_free(&game.types[index].fp_members);
    }
    for (size_t index = 0; index < game.constant_count; ++index) {
        free(game.constants[index].id);
        free(game.constants[index].symbol);
        free(game.constants[index].source);
        free(game.constants[index].category);
    }
    free(game.routines);
    free(game.declarations);
    free(game.data);
    free(game.types);
    free(game.constants);
    map_free(&by_symbol);
    map_free(&fp_types);
    map_free(&data_by_symbol);
    map_free(&definitions);
    map_free(&typedefs);
    list_free(&type_ids);
    for (size_t index = 0; index < paths.count; ++index) source_free(&sources[index]);
    free(sources);
    list_free(&paths);
    return true;
}

/* ---- Serialization ----------------------------------------------------------- */

/* _compact(): None, [] and {} members removed recursively. */
static FusionJson compacted(const FusionJson *value) {
    if (value->kind == FUSION_JSON_OBJECT) {
        FusionJson result = fusion_json_object();
        for (size_t index = 0; index < value->count; ++index) {
            FusionJson item = compacted(&value->items[index]);
            bool empty = item.kind == FUSION_JSON_NULL ||
                         ((item.kind == FUSION_JSON_ARRAY || item.kind == FUSION_JSON_OBJECT) && !item.count);
            if (empty) {
                fusion_json_free(&item);
                continue;
            }
            put(&result, value->keys[index], item);
        }
        return result;
    }
    if (value->kind == FUSION_JSON_ARRAY) {
        FusionJson result = fusion_json_array();
        for (size_t index = 0; index < value->count; ++index)
            if (!fusion_json_push(&result, compacted(&value->items[index]))) oom();
        return result;
    }
    FusionJson copy;
    if (!fusion_json_clone(value, &copy)) oom();
    return copy;
}

/* Objects with sorted keys, in place. */
static void sort_keys(FusionJson *value) {
    if (value->kind == FUSION_JSON_ARRAY) {
        for (size_t index = 0; index < value->count; ++index) sort_keys(&value->items[index]);
        return;
    }
    if (value->kind != FUSION_JSON_OBJECT) return;
    for (size_t index = 0; index < value->count; ++index) sort_keys(&value->items[index]);
    for (size_t index = 1; index < value->count; ++index) {
        char *key = value->keys[index];
        FusionJson item = value->items[index];
        size_t at = index;
        while (at > 0 && strcmp(value->keys[at - 1], key) > 0) {
            value->keys[at] = value->keys[at - 1];
            value->items[at] = value->items[at - 1];
            --at;
        }
        value->keys[at] = key;
        value->items[at] = item;
    }
}

/* json.dumps(_compact(value), sort_keys=True, separators=(",", ":")) + "\n",
 * with ensure_ascii's \uXXXX escapes. */
static char *encoded(const FusionJson *value, size_t *size) {
    FusionJson compact = compacted(value);
    sort_keys(&compact);
    char *text = NULL;
    size_t length = 0;
    if (!fusion_json_write(&compact, FUSION_JSON_COMPACT, &text, &length)) oom();
    fusion_json_free(&compact);
    size_t extra = 0;
    for (size_t index = 0; index < length; ++index) extra += (unsigned char)text[index] >= 0x80;
    char *result = malloc(length + extra * 12 + 2);
    if (!result) oom();
    size_t used = 0;
    const unsigned char *bytes = (const unsigned char *)text;
    for (size_t index = 0; index < length;) {
        unsigned char c = bytes[index];
        if (c < 0x80) {
            result[used++] = (char)c;
            ++index;
            continue;
        }
        unsigned code;
        size_t step;
        if (c >= 0xF0) {
            code = ((c & 7u) << 18) | ((bytes[index + 1] & 0x3Fu) << 12) |
                   ((bytes[index + 2] & 0x3Fu) << 6) | (bytes[index + 3] & 0x3Fu);
            step = 4;
        } else if (c >= 0xE0) {
            code = ((c & 0xFu) << 12) | ((bytes[index + 1] & 0x3Fu) << 6) | (bytes[index + 2] & 0x3Fu);
            step = 3;
        } else {
            code = ((c & 0x1Fu) << 6) | (bytes[index + 1] & 0x3Fu);
            step = 2;
        }
        if (code >= 0x10000) {
            code -= 0x10000;
            used += (size_t)sprintf(result + used, "\\u%04x\\u%04x", 0xD800 + (code >> 10),
                                    0xDC00 + (code & 0x3FF));
        } else {
            used += (size_t)sprintf(result + used, "\\u%04x", code);
        }
        index += step;
    }
    result[used++] = '\n';
    result[used] = '\0';
    free(text);
    *size = used;
    return result;
}

static bool atomic_write(const char *path, const char *data, size_t size) {
    char temporary[8300];
    snprintf(temporary, sizeof temporary, "%s.tmp", path);
    FILE *file = fopen(temporary, "wb");
    if (!file) return fail("cannot write %s", temporary);
    bool ok = fwrite(data, 1, size, file) == size;
    ok = fclose(file) == 0 && ok;
    if (!ok || rename(temporary, path)) {
        unlink(temporary);
        return fail("cannot write %s", path);
    }
    return true;
}

static bool write_inventory(const FusionJson *inventory, const char *output) {
    char directory[8192];
    snprintf(directory, sizeof directory, "%s", output);
    char *slash = strrchr(directory, '/');
    if (slash) *slash = '\0';
    else snprintf(directory, sizeof directory, ".");
    FusionJson manifest = fusion_json_object(), games = fusion_json_object();
    List expected = {0};
    for (size_t index = 0; index < inventory->count; ++index)
        if (strcmp(inventory->keys[index], "games")) {
            FusionJson copy;
            if (!fusion_json_clone(&inventory->items[index], &copy)) oom();
            put(&manifest, inventory->keys[index], copy);
        }
    const FusionJson *source_games = fusion_json_member(inventory, "games");
    bool ok = true;
    for (size_t game = 0; ok && game < source_games->count; ++game) {
        const char *name = source_games->keys[game];
        const FusionJson *data = &source_games->items[game];
        FusionJson summary = fusion_json_object(), record_files = fusion_json_array();
        for (size_t key = 0; key < data->count; ++key) {
            bool collection = false;
            for (size_t other = 0; other < COUNT(collections); ++other)
                collection |= !strcmp(collections[other], data->keys[key]);
            if (collection) continue;
            FusionJson copy;
            if (!fusion_json_clone(&data->items[key], &copy)) oom();
            put(&summary, data->keys[key], copy);
        }
        for (size_t collection = 0; ok && collection < COUNT(collections); ++collection) {
            const FusionJson *records = fusion_json_member(data, collections[collection]);
            /* _record_chunks(): at most MAX_SHARD_BYTES of encoded records. */
            size_t chunk_start = 0, chunk_size = 0, chunk_index = 0;
            for (size_t record = 0; ok && record <= records->count; ++record) {
                size_t record_size = 0;
                bool last = record == records->count;
                if (!last) {
                    char *text = encoded(&records->items[record], &record_size);
                    free(text);
                }
                bool flush = (last && record > chunk_start) ||
                             (!last && record > chunk_start && chunk_size + record_size > MAX_SHARD_BYTES);
                if (flush) {
                    FusionJson shard = fusion_json_object(), list = fusion_json_array();
                    for (size_t item = chunk_start; item < record; ++item) {
                        FusionJson copy;
                        if (!fusion_json_clone(&records->items[item], &copy) || !fusion_json_push(&list, copy)) oom();
                    }
                    put_text(&shard, "schema", record_schema);
                    put_text(&shard, "game", name);
                    put_text(&shard, "collection", collections[collection]);
                    put(&shard, "records", list);
                    size_t payload_size = 0;
                    char *payload = encoded(&shard, &payload_size);
                    char file_name[256], path[8500], digest[65];
                    snprintf(file_name, sizeof file_name, "inventory.%s.%s.%03zu.json", name,
                             collections[collection], chunk_index);
                    snprintf(path, sizeof path, "%s/%s", directory, file_name);
                    if (payload_size > 4 * 1024 * 1024)
                        ok = fail("inventory shard exceeds repository limit: %s", file_name);
                    ok = ok && atomic_write(path, payload, payload_size);
                    if (ok && !fusion_sha256_hex((const uint8_t *)payload, payload_size, digest)) oom();
                    if (ok) {
                        FusionJson descriptor = fusion_json_object();
                        put_text(&descriptor, "path", file_name);
                        put_text(&descriptor, "collection", collections[collection]);
                        put_int(&descriptor, "count", (int64_t)(record - chunk_start));
                        put_text(&descriptor, "sha256", digest);
                        if (!fusion_json_push(&record_files, descriptor)) oom();
                        list_add(&expected, file_name);
                    }
                    free(payload);
                    fusion_json_free(&shard);
                    ++chunk_index;
                    chunk_start = record;
                    chunk_size = 0;
                }
                chunk_size += record_size;
            }
        }
        put(&summary, "record_files", record_files);
        put(&games, name, summary);
    }
    put(&manifest, "games", games);
    /* Stale shards of an earlier layout are removed. */
    DIR *handle = ok ? opendir(directory) : NULL;
    struct dirent *entry;
    while (handle && (entry = readdir(handle)) != NULL) {
        const char *file = entry->d_name;
        size_t dots = 0;
        for (const char *c = file; *c; ++c) dots += *c == '.';
        if (strncmp(file, "inventory.", 10) || strlen(file) < 5 ||
            strcmp(file + strlen(file) - 5, ".json") || dots < 4 || list_has(&expected, file))
            continue;
        char path[8500];
        snprintf(path, sizeof path, "%s/%s", directory, file);
        unlink(path);
    }
    if (handle) closedir(handle);
    if (ok) {
        size_t size = 0;
        char *text = encoded(&manifest, &size);
        ok = atomic_write(output, text, size);
        free(text);
    }
    list_free(&expected);
    fusion_json_free(&manifest);
    return ok;
}

bool fusion_inventory_build(const char *root, const char *output, bool write,
                            FusionInventorySummary summary[2], char *error, size_t error_size) {
    failure_text = error;
    failure_size = error_size;
    static const char *const games[][3] = {{"mzm", "Metroid: Zero Mission", "third_party/mzm"},
                                           {"aos", "Castlevania: Aria of Sorrow", "third_party/cvaos"}};
    FusionJson inventory = fusion_json_object(), game_data = fusion_json_object();
    put_text(&inventory, "schema", inventory_schema);
    put_int(&inventory, "generator_version", 6);
    put_text(&inventory, "evidence", "automatic_static_discovery");
    bool ok = true;
    for (size_t game = 0; ok && game < 2; ++game) {
        FusionJson data;
        ok = inventory_game(games[game][0], games[game][1], games[game][2], root, &data);
        if (!ok) break;
        const FusionJson *statistics = fusion_json_member(&data, "statistics");
        summary[game].routines = (size_t)fusion_json_member(statistics, "routines")->integer;
        summary[game].declarations = (size_t)fusion_json_member(statistics, "declarations")->integer;
        summary[game].data_symbols = (size_t)fusion_json_member(statistics, "data_symbols")->integer;
        summary[game].call_edges = (size_t)fusion_json_member(statistics, "resolved_call_edges")->integer;
        put(&game_data, games[game][0], data);
    }
    put(&inventory, "games", game_data);
    ok = ok && (!write || write_inventory(&inventory, output));
    fusion_json_free(&inventory);
    failure_text = NULL;
    return ok;
}
