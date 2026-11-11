/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_BLUE_ALASTOR_H
#define AOS_SOUL_BLUE_ALASTOR_H
#include <stdbool.h>
#include <stdint.h>

#include "../red_souls/aos_soul_red_devil.h"
#include "../souls/aos_soul.h"

/* SkillAlastorUse (blue soul 23, toggled, 3 MP per second): the spinning
 * Alastor that parks where it was cast and whirls toward the nearest
 * enemies. The native position never moves after the cast — the
 * sub_0806E43C homing calls travel sub_08068CDC with a distance of
 * 2, which shifts 2 >> 8 = 0 pixels, so + 0x48/+ 0x4C stay zero and
 * only the + 0x2C angle evolves under the + 0x2E spin. Upkeep
 * sub_0802DF2C runs while the state is below 6; its end enters
 * state 6, the shrink-out (+ 0x2C += + 0x2E << 4, + 0x28 -= 0x1000
 * until it drains, then sub_0802DD84 and EntityDelete).
 *
 * State 0 rolls the sub_0803E654 variant into + 0x58 bits 0-4 (the
 * native sign check is on the zero-extended halfword, so it always
 * passes), loads the record (tiles 0x081E3980, palette 0x0820B700,
 * frames 0x08237AFC), copies Soma's pixel position with y - 0x10,
 * sets + 0x2E = -0x100 when Soma faces the + 0x58 bit-6 way (then
 * inverts the bit for the sprite), registers the type-6 contact and
 * plays song 0x19E; power 0x19, elements 1. State 1 adds + 0x2E to
 * + 0x2C and ramps + 0x28 by 0x1000 until it passes 0xFFFF. State 2
 * re-arms + 0x72 bit 1, and on (rng & 0x7F) == 0 jumps to state 4 —
 * else it homes once, counts + 0xD and adds the sin(+ 0xD * 0x200)
 * wobble to + 0x2C before state 3. State 3 re-arms, exits to state 4
 * on (rng & 0x7F) <= 1, else counts + 0xD, writes the small
 * sin(+ 0xD * 0x100 + 0x4000) >> 2 wobble to + 0x4C, decays + 0x48
 * by 63/64 and integrates — leaving for state 2 when the aim point
 * is more than 0x300000 (48 px) away. State 4 picks the nearest of
 * the four + 0x13194 homing slots through sub_0802D484 (none ->
 * state 2 with + 0xD = 0x80) and stores -(rng & 0xF) << 16 in
 * + 0x34. State 5 disarms + 0x72 bit 1 and, while the target's
 * + 0x34 stays positive, counts + 0xD down — zero -> state 2 —
 * homing once toward (target x, target y + + 0x34), then adds
 * + 0x48 / 64 + + 0x2E * 4 to + 0x2C and falls back to state 2.
 * State 5 always lands in state 2 on the same update: sub_0806E43C
 * returns the constant 2, so both distance gates fail and the
 * rotating contact box the tail registers for state 5 is unreachable
 * natively — the guardian's damage comes from the type-6 contact
 * armed in states 2-4 (+ 0x72 bit 1). */

enum { AOS_ALASTOR_TARGETS = 4, AOS_ALASTOR_POWER = 0x19, AOS_ALASTOR_ELEMENTS = 1 };

typedef struct {
    int16_t angle;          /* + 0x2C: whirl angle (turn units) */
    int16_t vel;            /* + 0x2E: spin velocity (+-0x100) */
    int32_t vx, vy;         /* + 0x48/+ 0x4C: zeroed by each homing */
    uint32_t scale;         /* + 0x28: 0 -> ~0x10FFF ramp, shrink-out */
    uint8_t state;          /* + 0xA: 0..6 */
    uint8_t tick;           /* + 0xD: shared wobble/hold counter */
    int8_t target;          /* + 0x30: homing slot, -1 while none */
    int32_t offset;         /* + 0x34: -(rng & 0xF) << 16 aim offset */
    int32_t x, y;           /* + 0x40/+ 0x44: 16.16 cast position */
    bool facing_left;       /* + 0x58 bit 6: inverse of Soma's */
    bool attacking;         /* + 0x72 bit 1: armed in states 2-4 */
    bool song_19e;          /* cast sound request */
    bool dead;              /* scale drained: entity deleted */
} AosAlastorGuardian;

/* One update after Soma's. `src_x`/`src_y` are the source's 16.16
 * position (the cast copies its pixel halves, y - 0x10);
 * `aim_x`/`aim_y` are the sub_0806CD38 / sub_0806CD78 focus point
 * (16.16, already y - 0x280000) — approximated by the caller.
 * `targets` is the + 0x13194 homing list. Returns false once the
 * entity is deleted (state 6's scale drained). */
bool aos_alastor_guardian_update(AosAlastorGuardian *ala, AosGuardian *guardian,
                                 AosStats *stats,
                                 const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                 uint16_t held, uint16_t pressed,
                                 uint32_t *soma_flags, int32_t src_x, int32_t src_y,
                                 int32_t aim_x, int32_t aim_y,
                                 bool soma_facing_left,
                                 const AosSkillTarget targets[AOS_ALASTOR_TARGETS],
                                 uint32_t (*random)(void));
#endif
