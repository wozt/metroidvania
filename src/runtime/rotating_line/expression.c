/* SPDX-License-Identifier: MIT */
#include "expression.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PROTO_PI 3.14159265358979323846f
#define MAX_DEPTH 24

typedef struct {
    const char *p;
    float u, t, amp, cycles, speed;
    int depth;
    bool ok;
} Parser;

static void spaces(Parser *e) {
    while (isspace((unsigned char)*e->p)) ++e->p;
}

static bool accept(Parser *e, char ch) {
    spaces(e);
    if (*e->p == ch) { ++e->p; return true; }
    return false;
}

static float expression(Parser *e);

static float primary(Parser *e) {
    if (!e->ok || ++e->depth > MAX_DEPTH) { e->ok = false; return 0.0f; }
    spaces(e);
    float v = 0.0f;
    if (accept(e, '(')) {
        v = expression(e);
        if (!accept(e, ')')) e->ok = false;
    } else if (isdigit((unsigned char)*e->p) || *e->p == '.') {
        char *end;
        v = strtof(e->p, &end);
        if (end == e->p || !isfinite(v)) e->ok = false;
        else e->p = end;
    } else if (isalpha((unsigned char)*e->p) || *e->p == '_') {
        char name[24];
        size_t n = 0;
        while (isalnum((unsigned char)*e->p) || *e->p == '_') {
            if (n + 1 >= sizeof name) { e->ok = false; break; }
            name[n++] = *e->p++;
        }
        name[n] = '\0';
        if (!e->ok) { --e->depth; return 0.0f; }
        if (accept(e, '(')) {
            float a = expression(e), b = 0.0f;
            bool two = !strcmp(name, "min") || !strcmp(name, "max") || !strcmp(name, "pow");
            if (two) {
                if (!accept(e, ',')) e->ok = false;
                else b = expression(e);
            }
            if (!accept(e, ')')) e->ok = false;
            if (!strcmp(name, "sin") && !two) v = sinf(a);
            else if (!strcmp(name, "cos") && !two) v = cosf(a);
            else if (!strcmp(name, "abs") && !two) v = fabsf(a);
            else if (!strcmp(name, "floor") && !two) v = floorf(a);
            else if (!strcmp(name, "fract") && !two) v = a - floorf(a);
            else if (!strcmp(name, "sqrt") && !two) v = sqrtf(a);
            else if (!strcmp(name, "tri") && !two) v = 1.0f - 4.0f * fabsf(a - floorf(a) - 0.5f);
            else if (!strcmp(name, "min") && two) v = fminf(a, b);
            else if (!strcmp(name, "max") && two) v = fmaxf(a, b);
            else if (!strcmp(name, "pow") && two) v = powf(a, b);
            else e->ok = false;
        } else if (!strcmp(name, "u")) v = e->u;
        else if (!strcmp(name, "t")) v = e->t;
        else if (!strcmp(name, "amp")) v = e->amp;
        else if (!strcmp(name, "cycles")) v = e->cycles;
        else if (!strcmp(name, "speed")) v = e->speed;
        else if (!strcmp(name, "pi")) v = PROTO_PI;
        else e->ok = false;
    } else e->ok = false;
    --e->depth;
    if (!isfinite(v)) e->ok = false;
    return e->ok ? v : 0.0f;
}

static float unary(Parser *e) {
    if (accept(e, '+')) return unary(e);
    if (accept(e, '-')) return -unary(e);
    return primary(e);
}

static float power(Parser *e) {
    float left = unary(e);
    if (accept(e, '^')) left = powf(left, power(e));
    if (!isfinite(left)) e->ok = false;
    return e->ok ? left : 0.0f;
}

static float term(Parser *e) {
    float left = power(e);
    for (;;) {
        if (accept(e, '*')) left *= power(e);
        else if (accept(e, '/')) {
            float denom = power(e);
            if (fabsf(denom) < 1.0e-8f) e->ok = false;
            else left /= denom;
        } else break;
        if (!e->ok || !isfinite(left)) { e->ok = false; return 0.0f; }
    }
    return left;
}

static float expression(Parser *e) {
    float left = term(e);
    for (;;) {
        if (accept(e, '+')) left += term(e);
        else if (accept(e, '-')) left -= term(e);
        else break;
        if (!e->ok || !isfinite(left)) { e->ok = false; return 0.0f; }
    }
    return left;
}

float proto_expression_evaluate(const char *source, float u, float time,
                                float amplitude, float cycles, float speed, bool *ok) {
    Parser e = {source, u, time, amplitude, cycles, speed, 0, source != NULL};
    if (!e.ok) { if (ok) *ok = false; return 0.0f; }
    float result = expression(&e);
    spaces(&e);
    if (*e.p || !isfinite(result)) e.ok = false;
    if (ok) *ok = e.ok;
    return e.ok ? result : 0.0f;
}

bool proto_expression_valid(const char *source) {
    if (!source || !*source || strlen(source) > 159) return false;
    for (int u = 0; u <= 4; ++u) {
        bool ok = false;
        (void)proto_expression_evaluate(source, (float)u / 4.0f, 0.37f,
                                        14.0f, 3.0f, 0.13f, &ok);
        if (!ok) return false;
    }
    return true;
}
