/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_MILLIONE_H
#define AOS_SKILL_MILLIONE_H

#include <stdbool.h>
#include <stdint.h>

/* SkillSkullMillioneUse, 0x0802B1B8.  The parent is an invisible emitter;
 * sub_0802B028 owns each of the three visible sequence-0 children. */
enum {
    AOS_MILLIONE_ACTIVE = 1,
    AOS_MILLIONE_ENDING = 2,
    AOS_MILLIONE_REMOVED = 3,
    AOS_MILLIONE_CHILDREN = 3
};

typedef struct {
    int32_t x, y;       /* +0x40, +0x44, 16.16 room pixels */
    uint8_t state;      /* +0x0A */
    uint8_t offset;     /* +0x0B: added to the initial 20-pixel lead */
    uint8_t emitted;    /* +0x0C */
    uint8_t interval;   /* +0x0D */
    bool facing_left;   /* +0x58 bit 6 */
    bool started;
} AosMillioneSoul;

typedef struct {
    int32_t x, y;
    bool facing_left;
    bool created;
} AosMillioneSpawn;

AosMillioneSoul aos_millione_soul_create(void);

/* Advance the parent by one native update.  It follows Soma at y - 20 and
 * advances from 20 through 25 pixels in front of him, creating a sequence-0
 * child after offsets 21, 23 and 25.  The third child leaves the parent in
 * state 2; the following update removes it. */
bool aos_millione_soul_update(AosMillioneSoul *soul, int32_t soma_x,
                              int32_t soma_y, bool soma_facing_left,
                              AosMillioneSpawn *spawn);

#endif
