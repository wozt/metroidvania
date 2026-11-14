/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared lookup tables of the Aria of Sorrow room runtime: the exported
 * sprite/art names that the object index and the metadata TSVs are parsed
 * against. Defined once in aria_tables.c; the asset loaders
 * (aria_assets.c) and the draw pass both read them, so they cannot stay
 * file-local to either (AGENTS.md rule 8). */
#ifndef ARIA_TABLES_H
#define ARIA_TABLES_H

#include "aria_runtime_types.h"

/* Other palette banks of a kind, exported as Enemy/<name>_bank<N>: the
 * (enemy kind id, bank) pairs variant_keys lists. */
typedef struct { int id, bank; } AriaVariantKey;

extern const char *const pickup_art_names[PICKUP_ARTS];
extern const char *const enemy_names[ENEMY_KINDS];
extern const char *const skill_art_names[SKILL_ARTS];
extern const char *const effect_names[AOS_EFFECT_KINDS];
extern const AriaVariantKey variant_keys[VARIANT_ARTS];

#endif
