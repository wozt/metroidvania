/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_RED_MINOTAUR_H
#define AOS_SOUL_RED_MINOTAUR_H
#include <stdbool.h>
#include <stdint.h>

/* SkillRedMinotaurUse's anchor and its sub_08026858 partner blade.
 * The anchor sits at sub_08031EA4(source, 0, -0x1A) and spins the
 * + 0x2C angle by the facing-signed + 0x2E step while + 0x28
 * accelerates it: state 1 ramps + 0x24 (the draw charge) to
 * 0x1FFFF, state 2 spins up for 0x50 frames of + 0x0D, state 3
 * spins down while + 0x24 drains, state 4 deletes. The blade child
 * orbits the anchor at radius 80 px (x = sin(angle) * 80,
 * y = sin(angle - 0x4000) * 80) and carries the {-32,-32,64,64}
 * attack box, enabled except while the parent sits in state 2. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44: spawn position, static */
    int32_t angle;          /* + 0x2C: 0x4000, mirrored -0x4000 */
    int32_t step;           /* + 0x2E: -0x800, +0x800 when mirrored */
    int32_t accel;          /* + 0x28: 0x40, -0x40 when mirrored */
    int32_t charge;         /* + 0x24: 0 -> 0x1FFFF -> 0 */
    bool facing_left;       /* + 0x58 bit 6 */
    uint8_t state;          /* + 0x0A: 0..4 */
    uint8_t timer;          /* + 0x0D: 0x50 spin-up frames */
    bool hidden;            /* + 0x58 bit 5: the margin/flag path */
    bool outside;           /* sub_0806D128(e, 4) / + 0x59 bit 4 */
} AosRedMinotaur;

/* The sub_08026858 blade entity: it follows the anchor's angle and
 * reports its own attack state (+ 0x72 bit 1 semantics). */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44: orbit position */
    int32_t angle;          /* the anchor's + 0x2C, for the draw spin */
    uint8_t state;          /* + 0x0A: 0 init, 1 armed, 2 parent spin */
    bool attacking;         /* + 0x72 bit 1: box enabled */
    bool alive;             /* false once the parent reaches state 4 */
} AosMinotaurBlade;

/* The anchor as the state-0 init leaves it: placed at (soma_x,
 * soma_y - 0x1A), angle/step/accel facing-signed, charge 0,
 * timer 0x50, state 0 -> 1 on the same frame, songs 0x1A0 and 0xCF
 * requested once. The native variant pick (sub_0803E654) and the
 * sub_08023530 partner registration failure paths are not ported —
 * the cast always succeeds here (documented divergence). */
AosRedMinotaur aos_red_minotaur_create(int32_t soma_x, int32_t soma_y,
                                       bool facing_left);
/* One update; false once the entity is deleted (state 4 processed). */
bool aos_red_minotaur_update(AosRedMinotaur *minotaur);
/* sub_08026858's first frames: installs the {-32,-32,64,64} box. */
void aos_minotaur_blade_init(AosMinotaurBlade *blade);
/* One blade update; follows the anchor state machine, places the
 * orbit position and returns false when the anchor is gone. */
bool aos_minotaur_blade_update(AosMinotaurBlade *blade,
                               const AosRedMinotaur *anchor);
#endif
