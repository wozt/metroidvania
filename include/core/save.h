#ifndef FUSION_CORE_SAVE_H
#define FUSION_CORE_SAVE_H

#include <stdbool.h>
#include <stddef.h>

#include "core/types.h"

bool save_session(const char *path, const SessionState *session,
                  char *error, size_t error_size);
bool load_session(const char *path, SessionState *session,
                  char *error, size_t error_size);

#endif
