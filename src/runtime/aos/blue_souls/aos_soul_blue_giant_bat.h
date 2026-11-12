/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_BLUE_GIANT_BAT_H
#define AOS_SOUL_BLUE_GIANT_BAT_H
#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_soma.h"
#include "../souls/aos_soul.h"

/* SkillGiantBatUse (blue soul 2, toggled, 30 MP per second): the freeze.
 * There is no sprite — the entity only dampens Soma and emits sixteen
 * frames of orbiting sparkles. The upkeep sub_0802DE5C deletes the
 * entity outright when it ends (no state-6 shrink). While Soma's + 0x10
 * keeps any of 0x010001A0 (attacking / wall-stop / bit 8 / body special)
 * the entity also deletes itself. On its first update (+ 0xA == 0) it
 * clamps Soma's + 0x48/+ 0x4C to +-0x40000, zeroes + 0x50/+ 0x54, clears
 * + 0x10 bits 0x21, plays song 0x8F, zeroes his + 0xB and arms + 0xD =
 * 0x10. While + 0xD counts down it spawns one sub_08045B44 sparkle a
 * frame at (soma x + sin(a1) * 4, soma y - 0x10 px + sin(a2) * 4) with
 * a1 = angle + 0x4000, a2 = angle and angle = (rng & 0xFF) << 8; the
 * spawned entity's + 0x48/+ 0x4C take sin * 2. Every update forces the
 * entity's + 0xA to 8 and sets gEwramData + 0x13260 bit 0x100 and
 * + 0x131B8 bits 0-1 — the + 0x131B8 bits have no modelled reader and
 * are exposed through aux_bits. */

typedef struct {
    uint8_t timer;          /* + 0xD: sparkle frames left (0x10 at cast) */
    bool cast;              /* the + 0xA == 0 block ran */
    bool song_8f;           /* cast sound request */
    bool fx;                /* one sub_08045B44 request this update */
    int32_t fx_x, fx_y;     /* the sparkle's spawn position */
    int32_t fx_vx, fx_vy;   /* the spawned entity's + 0x48/+ 0x4C (sin * 2) */
    uint32_t aux_bits;      /* gEwramData + 0x131B8 bits 0-1, set every update */
    bool dead;              /* upkeep ended or the + 0x10 mask hit */
} AosGiantBatGuardian;

/* One update after Soma's; `soma` is the + 0x13110 target entity the
 * routine reads and writes. Returns false once the entity is deleted. */
bool aos_giant_bat_guardian_update(AosGiantBatGuardian *bat, AosGuardian *guardian,
                                   AosStats *stats,
                                   const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                   uint16_t held, uint16_t pressed,
                                   uint32_t *soma_flags, AosSoma *soma,
                                   uint32_t (*random)(void));
#endif
