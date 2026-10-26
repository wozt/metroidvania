/* SPDX-License-Identifier: GPL-3.0-only */
/* The native editor backend (formerly scripts/editor_backend.py): one
 * validated operation per command, returning its data as a JSON document. */
#ifndef FUSION_EDITOR_BACKEND_H
#define FUSION_EDITOR_BACKEND_H

#include "core/json.h"

#include <stdbool.h>
#include <stddef.h>

#define FEB_BACKEND_VERSION "1.7.0"

enum {
    FEB_EXIT_SUCCESS = 0,
    FEB_EXIT_USAGE = 2,
    FEB_EXIT_UNAVAILABLE = 3,
    FEB_EXIT_OPERATION = 4,
    FEB_MESSAGE_CAPACITY = 2048,
    FEB_PATH_CAPACITY = 4096,
};

typedef struct {
    int code;   /* one of the FEB_EXIT_* failures */
    char message[FEB_MESSAGE_CAPACITY];
} FebError;

typedef struct {
    char root[FEB_PATH_CAPACITY];   /* the validated, resolved project root */
    bool dry_run;
} FebContext;

/* Records a failure and returns false. */
bool feb_fail(FebError *error, int code, const char *format, ...);

/* The commands this native backend implements. */
bool feb_is_native(const char *command);
/* Whether the command exists (COMMAND_FIELDS), and its allowed options. */
const char *const *feb_command_fields(const char *command, bool *known);
/* The CAPABILITIES entry of a command, or NULL. */
const char *feb_capability(const char *command);

bool feb_is_mutating(const char *command);
/* The options not in ``fields`` (a NULL-terminated list), sorted and joined
 * with ", "; returns true when there are none. */
bool feb_unknown_options(const FusionJson *options, const char *const *fields,
                         char *unknown, size_t size);

/* execute(): validates the root and runs one command into ``data``.
 * ``repository`` is the checkout holding the decompilations and assets. */
bool feb_execute(const char *command, const FusionJson *options, const char *root,
                 const char *repository, bool dry_run, FusionJson *data, FebError *error);

/* The command-line interface; ``handled`` is false when the command must be
 * forwarded to the remaining Python backend. */
int feb_cli_main(int argc, char **argv, const char *repository, bool *handled);

#endif /* FUSION_EDITOR_BACKEND_H */
