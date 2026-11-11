/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_KYOMA_DEMON_H
#define AOS_SOUL_RED_KYOMA_DEMON_H
#include <stdbool.h>
#include <stdint.h>

/* SkillKyomaDemonUse's glued familiar: no movement of its own —
 * sub_08031EA4(entity, source, 0, 0) re-places it on the source every
 * update, and it lives a fixed 0x20 + 0xB4 frames (no animation-end or
 * source-flag check). */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 wobble, 2 lingering */
    uint8_t wobble;     /* + 0x0D: counts 0x20 down in state 1 */
    uint16_t linger;    /* + 0x24 halfword: counts 0xB4 down in state 2 */
} AosKyomaDemon;

/* State 0 would pick a palette variant through sub_0803E654 and delete
 * on the 0x8000 sentinel; the variant bookkeeping is not tracked, so the
 * cast always proceeds. It presets the + 0x5A/+ 0x59 flags, plays song
 * 0x97 and enters state 1 with + 0x0D = 0x20. */
AosKyomaDemon aos_kyoma_demon_create(void);
/* One update. Returns false once the entity is deleted (state 2's
 * + 0x24 counter reaching 0). */
bool aos_kyoma_demon_update(AosKyomaDemon *demon);
#endif
