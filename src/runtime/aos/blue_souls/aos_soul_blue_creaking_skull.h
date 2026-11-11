/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_BLUE_CREAKING_SKULL_H
#define AOS_SOUL_BLUE_CREAKING_SKULL_H
#include <stdbool.h>
#include <stdint.h>

#include "../souls/aos_soul.h"

/* SkillCreakingSkullUse (blue soul 14, toggled, 5 MP per second): the
 * guardian that parks 0x14 pixels above Soma and commands three
 * orbiting skulls. The native spawns the skulls through the shared
 * sub_0802DD00 slot table (gEwramData + 0x13144, eight slots); the
 * port keeps them embedded in the guardian instead — documented
 * divergence. Every update the upkeep sub_0802DE5C runs first and a
 * nonzero result deletes the entity outright; the pixel position
 * copies Soma's with y - 0x14 and + 0x58 bit 6 is the inverse of
 * Soma's. State 0 loads the shared skull record (tiles 0x081E1970,
 * palette 0x0820B670, frames 0x0823767C — the caller's art), sets
 * + 0x2C to the first attack record and + 0x34 to table A
 * (0x084F13DC), arms the three children and ramps nothing; state 1
 * ramps + 0x28 by 0x1000 until it passes 0xFFFF; state 2 re-arms
 * table A, enters state 3 and sets + 0xC; state 3 reads the next
 * record of + 0x34 indexed by + 0xB — a NULL entry restarts table A
 * at index 0 through state 2, a live record stores its pointer in
 * + 0x2C, its byte 0 - 1 in + 0xD, its halfword at + 2 in + 0x31 and
 * pulses + 0x30 for one frame; state 4 holds the record for + 0xD
 * frames, plays song 0x9B when table B's index-3 record ends and
 * advances + 0xB. With + 0xC set, each update re-checks the native
 * gEwramData + 0x1311C[source->+ 0x28] slot — approximated by the
 * `empowered` parameter — and a nonzero entry switches + 0x34 to
 * table B (0x084F13E8) and jumps to state 3.
 *
 * Children 0 and 1 run sub_08030754: state 0 rolls the sub_0803E654
 * variant (a negative roll deletes the child), stores the record's
 * per-child target angle in + 0x2C and enters state 1; a nonzero
 * parent + 0x30 forces state 2, which computes the angular velocity
 * (target - direction) / record halfword 0 into + 0x2E over + 0x32
 * frames; state 3 integrates and snaps, back to state 1; state 4 is
 * the fade/delete. The orbit tail puts child 0 on the parent and
 * child 1 at radius table_080E1AFC[1] * + 0x28 / 0x10000 around the
 * angle record_low - 0x8000 (negated by facing), where record_low is
 * the low halfword of the record's native address — a quirk of the
 * native + 0x2C pointer reuse, kept faithfully through
 * `native_low`. Child 2 runs sub_08030650: it registers the
 * {-16,-16,32,32} attack box once, and while the parent's + 0x31 is
 * nonzero it orbits at a radius alternating 0x30/0x10 every frame on
 * the same record_low angle base. */

enum { AOS_CREAKING_SKULL_CHILDREN = 3 };

/* One 10-byte attack record of the 0x080E1AA8 stream: byte 0 is the
 * hold count + 1, the halfword at + 2 gates child 2's orbit through
 * its low byte, and the three halfwords at + 4/+ 6/+ 8 are the
 * per-child target angles. `native_low` is the record address's low
 * halfword — the native reuses + 0x2C both as this pointer and as an
 * angle source for the orbit tail. */
typedef struct {
    uint8_t duration;    /* byte 0: state-4 hold frames + 1 */
    uint8_t pad;         /* byte 1: always 0 in the ROM stream */
    uint16_t field31;    /* halfword at + 2: low byte -> parent + 0x31 */
    uint16_t angles[3];  /* halfwords at + 4, + 6, + 8 */
    uint16_t native_low; /* address & 0xFFFF (0x1AA8, 0x1AB2, ...) */
} AosCreakingAttack;

typedef struct {
    int16_t angle;       /* + 0x2C: orbit/tween angle (turn units) */
    int16_t vel;         /* + 0x2E: angular velocity per frame */
    uint16_t frames;     /* + 0x32: tween frame count */
    uint8_t state;       /* + 0xA: 0 init, 1 idle, 2 aim, 3 turn, 4 die */
    uint8_t index;       /* + 0x31: child slot 0..2 */
    uint8_t variant;     /* + 0x58 bits 0-4: palette-variant roll */
    uint8_t tick;        /* + 0xD: child 2's radius alternation counter */
    int32_t x, y;        /* + 0x40/+ 0x44: 16.16 position */
    bool facing_left;    /* + 0x58 bit 6 copied from the parent */
    bool attacking;      /* contact box armed this frame */
    bool armed;          /* + 0x72 bit 1 (child 2's orbit gate mirror) */
    bool dead;           /* state 4 / refused roll: fade and delete */
} AosCreakingChild;

typedef struct {
    const AosCreakingAttack *record; /* + 0x2C: current attack record */
    const AosCreakingAttack *const *table; /* + 0x34: A or B table */
    uint8_t state;          /* + 0xA: 0..4 */
    uint8_t timer;          /* + 0xD: record hold frames left */
    uint8_t index;          /* + 0xB: table slot */
    uint8_t flag_c;         /* + 0xC: empowered recheck armed */
    uint8_t flag_30;        /* + 0x30: record picked this frame */
    uint8_t field31;        /* + 0x31: current record's low byte */
    uint32_t scale;         /* + 0x28: 0 -> 0x10000 ramp, then ~0x10FFF */
    int32_t x, y;           /* + 0x40/+ 0x44: 16.16, Soma pos y - 0x14 */
    bool facing_left;       /* + 0x58 bit 6: inverse of Soma's */
    bool song_9b;           /* table B index-3 record ended */
    AosCreakingChild children[AOS_CREAKING_SKULL_CHILDREN];
} AosCreakingSkullGuardian;

/* One update after Soma's. `empowered` approximates the native
 * gEwramData + 0x1311C[source->+ 0x28] slot read while + 0xC is set.
 * Returns false once the entity is deleted (upkeep drain). Dead
 * children stay in the array flagged `dead` for the caller to drop. */
bool aos_creaking_skull_guardian_update(AosCreakingSkullGuardian *skull,
                                        AosGuardian *guardian, AosStats *stats,
                                        const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                        uint16_t held, uint16_t pressed,
                                        uint32_t *soma_flags, int32_t soma_x,
                                        int32_t soma_y, bool soma_facing_left,
                                        bool empowered, uint32_t (*random)(void));
#endif
