/* SPDX-License-Identifier: MIT */
#ifndef PROTO_CONFIG_H
#define PROTO_CONFIG_H

#include "curve.h"
#include <stddef.h>

/* Strict JSON settings: see docs/FORMULAS.md. On failure, config is unchanged. */
bool proto_config_load(const char *path, ProtoConfig *config, char *error, size_t size);
/* Atomic JSON file replacement through a sibling .tmp file. */
bool proto_config_save(const char *path, const ProtoConfig *config, char *error, size_t size);

#endif
