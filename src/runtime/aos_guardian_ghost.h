/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_GUARDIAN_GHOST_H
#define AOS_GUARDIAN_GHOST_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_soul.h"

/* Skill_Witch_GiantGhost_Use (blue souls 5 and 6, toggled): the record's
 * argument (+ 0x1C) 0 is the Student Witch, 1 the Giant Ghost. It grows
 * over Soma (its affine scale + 0x28 by 0x2000 a frame to 1.0), follows
 * him with his facing, and shrinks away once its upkeep ends it. Its own
 * attack (type 7: sub_08021530(7) is 0, the 0x080E1320 entry 7 being 0)
 * only touches; each touch (its contact callback sub_0802F8CC) throws a
 * spirit (sub_0802F7E8) from the contact point. The guardian power is 30
 * with elements 0x20 (gEwramData + 0x131C2 / + 0x131C4). */
enum { AOS_GHOST_POWER = 0x1E, AOS_GHOST_ELEMENTS = 0x20, AOS_GHOST_SHOTS = 7,
       AOS_GHOST_ATTACK_TYPE = 7 };

typedef struct {
    bool alive;
    uint8_t state;          /* + 0x0A: 0 created, 1 flying, 2 gone */
    int32_t x, y;           /* + 0x40, + 0x44 */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
} AosGhostShot;

typedef struct {
    bool giant;             /* + 0x1C: the Giant Ghost (else the Student Witch) */
    int32_t x, y;           /* Soma's position (16.16) */
    bool facing_left;
    int32_t scale;          /* + 0x28: the affine scale, 0x10000 = unit */
    bool attacking;         /* + 0x72 bit 1 clear */
    AosGhostShot shots[AOS_GHOST_SHOTS];
} AosGhostGuardian;

/* One update after Soma's. State 0 sets it up (power, elements, sound 0x97
 * for the Witch or 0x8D for the Ghost); states 0-1 grow it (state 2 at
 * 1.0, its attack on), and in states 0-2 the upkeep (sub_0802DF2C) may end
 * it: state 3, its attack off, shrinking by 0x2000 to 0 when the soul is
 * cleaned up (sub_0802DD84). Its spirits update after it. Returns false
 * once ended. */
bool aos_ghost_guardian_update(AosGhostGuardian *ghost, AosGuardian *guardian, AosStats *stats,
                               const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                               uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                               int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                               int cam_x, int cam_y);
/* Its attack box (0x2830D8E8 for the Ghost: -24, -40, 48 x 40; 0x2010E010
 * for the Witch: 16, -32, 16 x 32), relative to its position, mirrored
 * with its facing. */
void aos_ghost_guardian_box(const AosGhostGuardian *ghost, int8_t box[4]);
/* sub_0802F8CC: a touch at the contact point (cx, cy) (the centre of the
 * overlap, sub_0806B658) throws a spirit at 4.0 away from 16 pixels above
 * the guardian (ArcTan2, the y off by (r & 3) - 2) into a free guardian
 * slot (sub_0802DD00). */
void aos_ghost_guardian_touch(AosGhostGuardian *ghost, int cx, int cy, uint32_t (*random)(void));
#endif
