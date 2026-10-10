/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow entity animation timing.
 *
 * Ports of cvaos sub_0803F2C8 (start an animation: entity + 0x6D id,
 * + 0x6E frame, + 0x6F tick, + 0x6C flags) and sub_0803EC34 (the step for
 * animation encoding 1, reached through sub_0803F17C / sub_0803EFF0 and the
 * table at 0x080E2B34). Only timing is modelled: frame ids and durations
 * come from the caller (for Soma, the exported sprite library). No SDL. */
#ifndef AOS_ANIM_H
#define AOS_ANIM_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t count;             /* frames; 0 for a missing animation */
    const uint8_t *durations;   /* count entries, in 60 Hz frames */
} AosAnimDef;

typedef struct {
    const AosAnimDef *anims;
    uint16_t count;             /* descriptor + 2 */
} AosAnimSet;

enum {
    AOS_ANIM_LOOP = 1,          /* + 0x6C bit 0 */
    AOS_ANIM_ENDED = 4,         /* + 0x6C bit 2 */
    AOS_ANIM_TABLE = 8          /* + 0x6C bit 3, set by every start */
};

typedef struct {
    uint8_t id;                 /* + 0x6D */
    uint8_t frame;              /* + 0x6E */
    uint8_t tick;               /* + 0x6F */
    uint8_t flags;              /* + 0x6C */
} AosAnimState;

/* sub_0803F2C8: false (and no change) when the id is out of range. */
bool aos_anim_start(AosAnimState *state, const AosAnimSet *set, unsigned id, bool loop);
/* sub_0803EC34: 1 = same frame, 2 = next frame, 3 = ended (held on the last
 * frame), 4 = looped to frame 0, 0 = no animation. */
int aos_anim_step(AosAnimState *state, const AosAnimSet *set);

#endif /* AOS_ANIM_H */
