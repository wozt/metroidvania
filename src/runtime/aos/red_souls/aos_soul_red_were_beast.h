/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_WERE_BEAST_H
#define AOS_SOUL_RED_WERE_BEAST_H
#include <stdbool.h>
#include <stdint.h>

/* Skill_Werejaguar_Weretiger_Use's shared spectral beast: like the Killer
 * Mantle it has no movement of its own — sub_08031EA4(entity, source, 0, 0)
 * re-places it on the source every update. */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 active, 2 ending */
} AosWereBeast;

/* State 0 loads the shared 0x081D00E4 / 0x0820B164 / 0x0823199C record
 * (entry 0x60 of 0x080E13BC, variant through + 0x1E), presets attack box
 * 5, plays song 0xEB and enters state 1; state 2 clears the native
 * counters and deletes the entity. */
AosWereBeast aos_were_beast_create(void);
/* One update. source_gone stands for the native (source + 0x10 & 0x180)
 * != 0x100 check; returns false once the entity is deleted. */
bool aos_were_beast_update(AosWereBeast *beast, bool anim_finished,
                           bool source_gone);
#endif
