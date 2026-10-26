/* SPDX-License-Identifier: GPL-3.0-only */
/* Validation and file helpers shared by the native editor commands. */
#ifndef FUSION_EDITOR_BACKEND_COMMON_H
#define FUSION_EDITOR_BACKEND_COMMON_H

#include "backend.h"

#include <stdint.h>

enum { FEB_WORLD_COUNT = 2 };

/* authored_rooms.AREAS: the worlds in order and their area names. */
extern const char *const feb_world_ids[FEB_WORLD_COUNT];
const char *const *feb_areas(const char *world, size_t *count);

/* _integer(): a JSON integer or a canonical decimal string in range. */
bool feb_integer(const FusionJson *value, const char *label, int64_t lower,
                 int64_t upper, int64_t *result, FebError *error);
/* _world(): zero_mission or aria. */
bool feb_world(const FusionJson *value, const char **world, FebError *error);
/* The string member of an options object, or NULL when absent. */
const FusionJson *feb_option(const FusionJson *options, const char *key);

/* Python-compatible text checks. */
size_t feb_utf8_length(const char *text);
bool feb_python_space(uint32_t code);
/* The first and last code points of a UTF-8 string. */
bool feb_utf8_edges(const char *text, uint32_t *first, uint32_t *last);

bool feb_is_symlink(const char *path);
bool feb_is_regular(const char *path, long long *size);
bool feb_is_directory(const char *path);
bool feb_join(char *out, const char *left, const char *right);
/* The path made absolute against the working directory. */
bool feb_absolute(const char *path, char *out);
/* Reads a whole file of at most ``limit`` bytes. */
bool feb_read_text(const char *path, size_t limit, char **text, size_t *size);
/* Parses a JSON file; the error message is the backend's. */
bool feb_load_json(const char *path, size_t limit, FusionJson *value, char *detail,
                   size_t detail_size);

/* Sets ``key`` of ``object``; returns false (out of memory) on failure. */
bool feb_put(FusionJson *object, const char *key, FusionJson value);
bool feb_put_string(FusionJson *object, const char *key, const char *value);
bool feb_put_int(FusionJson *object, const char *key, int64_t value);
bool feb_put_bool(FusionJson *object, const char *key, bool value);
bool feb_oom(FebError *error);

#endif /* FUSION_EDITOR_BACKEND_COMMON_H */
