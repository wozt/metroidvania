/* SPDX-License-Identifier: GPL-3.0-only */
/* A bounded JSON document model shared by the native tools: a reader, a
 * builder that keeps member insertion order, and a writer whose output is
 * byte-identical to Python's json.dumps(..., ensure_ascii=False) in its
 * compact (separators=(",", ":")) and indent=2 forms. */
#ifndef FUSION_CORE_JSON_H
#define FUSION_CORE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    FUSION_JSON_NULL,
    FUSION_JSON_BOOL,
    FUSION_JSON_NUMBER,
    FUSION_JSON_STRING,
    FUSION_JSON_ARRAY,
    FUSION_JSON_OBJECT,
} FusionJsonKind;

typedef struct FusionJson {
    FusionJsonKind kind;
    bool boolean;
    bool integral;           /* a number without fraction or exponent */
    int64_t integer;         /* the value of an integral number */
    double number;           /* the value of any number */
    char *string;            /* NUL-terminated UTF-8 */
    struct FusionJson *items;
    char **keys;             /* object member names, parallel to items */
    size_t count;
    size_t capacity;
} FusionJson;

enum { FUSION_JSON_COMPACT = -1 };

bool fusion_json_parse(const char *text, size_t size, FusionJson *root,
                       char *error, size_t error_size);
void fusion_json_free(FusionJson *value);
/* The member of an object, or NULL. */
const FusionJson *fusion_json_member(const FusionJson *object, const char *key);
FusionJson *fusion_json_member_mut(FusionJson *object, const char *key);

FusionJson fusion_json_null(void);
FusionJson fusion_json_bool(bool value);
FusionJson fusion_json_int(int64_t value);
FusionJson fusion_json_double(double value);
/* A copy of ``value``; a NULL string stays NULL (check before use). */
FusionJson fusion_json_string(const char *value);
FusionJson fusion_json_object(void);
FusionJson fusion_json_array(void);
/* Moves ``value`` into the object, replacing an existing member in place as
 * a Python dict assignment does. ``value`` is freed on failure. */
bool fusion_json_set(FusionJson *object, const char *key, FusionJson value);
/* Moves ``value`` to the end of the array; it is freed on failure. */
bool fusion_json_push(FusionJson *array, FusionJson value);
bool fusion_json_remove(FusionJson *object, const char *key);
bool fusion_json_clone(const FusionJson *source, FusionJson *copy);
bool fusion_json_equal(const FusionJson *left, const FusionJson *right);
/* Serializes with ``indent`` spaces per level, or FUSION_JSON_COMPACT. The
 * NUL-terminated text is returned in ``*text``, to be freed. */
bool fusion_json_write(const FusionJson *value, int indent, char **text, size_t *size);
/* Python's repr() of a double. */
void fusion_json_float_repr(double value, char *buffer, size_t size);

#endif /* FUSION_CORE_JSON_H */
