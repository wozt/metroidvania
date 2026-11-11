/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_CHRONOMAGE_H
#define AOS_SOUL_RED_CHRONOMAGE_H
#include <stdbool.h>
#include <stdint.h>

/* SkillChronomageUse's time-stop field: state 1 ramps + 0x20 by 0x1000
 * and + 0x24 by 0x2000 (both capped at 0x10000) while the global + 0x4BE
 * countdown ticks song 0x16E every sixteenth frame; at zero it releases
 * over 0x20 frames (+ 0x20 - 0x1000, + 0x24 - 0x800 clamped at 0) and
 * deletes. The tail publishes + 0x28 as the global + 0x12FE4 time value
 * every frame. The sub_0803E654 variant pick, the sub_0803E058 visual
 * and the gEwramData + 0x42C/+ 0x254CC/+ 0x254CE conditional songs are
 * not ported. */
typedef struct {
    uint8_t state;      /* + 0x0A: 0 init, 1 charge, 2 release */
    uint8_t timer;      /* + 0x0D: 0x20 release counter */
    int32_t charge;     /* + 0x20: + 0x1000/frame to 0x10000, then the
                         * published time value (+ 0x28 = + 0x20) */
    int32_t charge2;    /* + 0x24: + 0x2000/frame to 0x10000 */
    uint16_t field;     /* gEwramData + 0x4BE: the hold countdown copied
                         * from the source + 0x1E at init */
    bool tick;          /* sub_08013DA8(0x16E) request this frame */
    bool release;       /* the state 1 -> 2 transition happened */
} AosChronomage;

/* State 0 loads the entry-0x6 record (the Ripper art), sequence 0,
 * + 0x5C = 0xD, + 0x58 = variant | kept bits, + 0x59 |= 0x12, copies
 * the source + 0x1E into the global + 0x4BE field, plays songs
 * 0x1030, 0x19D and 0xEC and enters state 1. `field` is that copied
 * duration. */
AosChronomage aos_chronomage_create(uint16_t field);
/* One update. Returns false once deleted (state 2's timer reaching 0
 * deletes on the same frame, like the native EntityDelete case). */
bool aos_chronomage_update(AosChronomage *chronomage);
#endif
