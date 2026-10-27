/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_H
#define AOS_SOUL_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_stats.h"

/* One of Soma's red soul records (0x080E1598, metadata/red_souls.tsv). */
typedef struct {
    uint32_t skill;      /* + 0x0: the skill entity's routine, 0 for none */
    uint16_t pose;       /* + 0x4: cast pose 0-3 */
    int16_t mp;          /* + 0x6: MP cost */
    uint8_t maximum;     /* + 0x8: skill entities alive at once */
    uint16_t parameter;  /* + 0xE: given to the skill entity (+ 0x1E) */
    uint16_t power;      /* + 0xA: sub_08021530(5) */
    uint16_t elements;   /* + 0xC: sub_0806B7D8's attack elements */
} AosRedSoulRecord;

enum { AOS_RED_SOUL_RECORDS = 29, AOS_RED_SOUL_SLOTS = 8 };

/* The active red soul entities: activeRedSoulEntities (0x13124),
 * nbrActiveRedSoulEntities (0x131BE) and lastUsedRedSoul (0x131BF). */
typedef struct {
    bool slots[AOS_RED_SOUL_SLOTS];
    uint8_t active;
    uint8_t last_used;
} AosRedSoulCaster;

/* The guards and bookkeeping of sub_08019478 (Soma's red soul use; the
 * 0x131B8 & 0x100 lock and gEwramData + 0x344 & 0x200000 are passed as
 * `locked`): with no skill entity alive the last used soul follows the
 * equipped one; the cast needs that soul, a skill routine, at least its MP
 * cost (Soma may spend all his MP), fewer active entities than the last
 * used record's maximum and a free slot (sub_0802D208). It then spends the
 * MP and takes the slot. Returns the slot, or -1. */
int aos_red_soul_try(AosRedSoulCaster *caster, AosStats *stats,
                     const AosRedSoulRecord records[AOS_RED_SOUL_RECORDS], bool locked);
/* The cast animation of sub_0802D1D8: 0x080E1918[pose * 2 + airborne]. */
uint8_t aos_red_soul_cast_animation(uint16_t pose, bool airborne);
/* A skill entity's end: its slot is freed and the active count drops. */
void aos_red_soul_release(AosRedSoulCaster *caster, int slot);

/* sub_08021530(5) in normal play (gEwramData + 0x4CC bit 6 clear) for the
 * last used soul's record: (STR * power / 10 + INT / 4 + power * 6) / 16,
 * scaled by 8 / 10 when gEwramData + 0xA1 bit 4 is set (`reduced`). */
int32_t aos_red_soul_power(const AosStats *stats, const AosRedSoulRecord *record, bool reduced);
/* sub_08021994's MP regeneration, run with Soma's + 0x22 tick counter
 * before it advances: accessory 0x2C keeps MP full; otherwise one MP every
 * 64 ticks (every 16 with accessory 0x29), at most the maximum. The curse
 * states of unk_13270 are not modelled. */
void aos_stats_mp_regen(AosStats *stats, uint16_t ticks);
#endif
