/* SPDX-License-Identifier: MIT */
#ifndef PROTO_EXPRESSION_H
#define PROTO_EXPRESSION_H

#include <stdbool.h>

/* A deliberately small, deterministic math-expression evaluator (no scripts). */
float proto_expression_evaluate(const char *source, float u, float time,
                                float amplitude, float cycles, float speed, bool *ok);
bool proto_expression_valid(const char *source);

#endif
