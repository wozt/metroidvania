/* SPDX-License-Identifier: GPL-3.0-only */
/* A bounded JSON reader for the pinned decompilation databases. */
#ifndef FUSION_ASSETS_CORE_JSON_H
#define FUSION_ASSETS_CORE_JSON_H

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
    int64_t integer;
    char *string;            /* NUL-terminated UTF-8 */
    struct FusionJson *items;
    char **keys;             /* object member names, parallel to items */
    size_t count;
} FusionJson;

bool fusion_json_parse(const char *text, size_t size, FusionJson *root,
                       char *error, size_t error_size);
void fusion_json_free(FusionJson *value);
/* The member of an object, or NULL. */
const FusionJson *fusion_json_member(const FusionJson *object, const char *key);

#endif /* FUSION_ASSETS_CORE_JSON_H */
