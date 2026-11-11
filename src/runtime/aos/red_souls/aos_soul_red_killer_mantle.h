/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_KILLER_MANTLE_H
#define AOS_SOUL_RED_KILLER_MANTLE_H
#include <stdbool.h>
#include <stdint.h>

/* SkillKillerMantleUse's spectral knight: the entity has no movement of
 * its own — sub_08031EA4(entity, source, 0, 0) re-places it on the source
 * every update, so the runtime keeps skill->x/y glued to Soma. */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 active, 2 ending */
} AosKillerMantle;

/* State 0 loads its resources, presets attack box 5 through
 * sub_0804277C, plays song 0xD1 and counts the cast; state 1 re-places
 * on the source and ends on the animation end (sub_0803F17C == 3) or
 * the sub_0806D128 margin probe; state 2 releases the native counters
 * and deletes the entity. */
AosKillerMantle aos_killer_mantle_create(void);
/* One update. Returns false once the entity is deleted (state 2 ran). */
bool aos_killer_mantle_update(AosKillerMantle *mantle, bool anim_finished,
                              bool outside);
#endif
