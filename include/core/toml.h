/* SPDX-License-Identifier: GPL-3.0-only */
/* A bounded TOML 1.0 reader for the project's story and cutscene sources:
 * key/value pairs with bare, quoted and dotted keys, basic, literal and
 * multi-line strings, integers (decimal, hexadecimal, octal, binary),
 * floats, booleans, arrays, inline tables, [tables] and [[arrays of
 * tables]]. Dates are not supported. The document becomes a JSON object. */
#ifndef FUSION_CORE_TOML_H
#define FUSION_CORE_TOML_H

#include "core/json.h"

bool fusion_toml_parse(const char *text, size_t size, FusionJson *root,
                       char *error, size_t error_size);

#endif /* FUSION_CORE_TOML_H */
