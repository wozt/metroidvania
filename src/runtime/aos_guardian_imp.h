/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_GUARDIAN_IMP_H
#define AOS_GUARDIAN_IMP_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_skill_devil.h"
#include "aos_soul.h"

/* SkillImpUse (soul 22, toggled, 3 MP per second): a familiar that follows
 * Soma and dives at the nearest enemy of the homing list. */
enum { AOS_IMP_POWER = 15, AOS_IMP_ELEMENTS = 0x11, AOS_IMP_TARGETS = 4 };

enum {
    AOS_IMP_ENTERING = 1, AOS_IMP_FOLLOWING = 2, AOS_IMP_HOVERING = 3, AOS_IMP_SEARCHING = 4,
    AOS_IMP_DIVING = 5, AOS_IMP_LEAVING = 6,
};

typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    bool mirrored;          /* + 0x58 bit 6 */
    uint8_t timer;          /* + 0x0D */
    bool blinking;          /* + 0x0F */
    int target;             /* the homing slot chased (+ 0x30), -1 for none */
    int32_t target_dy;      /* + 0x34: 16 to 23 pixels above the target */
} AosImpGuardian;

/* One update. State 0 (the guardian's) places it at Soma's x and 16 above
 * his feet with his facing, vy -0.25, power 15, elements 0x11 and sound
 * 0x19E (sub_08042A54(2, 0x3C) is not run). Below state 6 the upkeep
 * sub_0802DF2C runs first; its end starts state 6 (blinking, 16 frames,
 * then the cleanup). State 1 moves (sub_0806D3D8, 8.0) until + 0x0D is 0.
 * State 2 (one chance in 256 of state 4) eases toward Soma's x and his
 * y - 40 by 1/32 of the distance (sub_0806E43C) and, under a 0.5 step,
 * hovers (state 3): a sine figure from + 0x0D, back to state 2 beyond 48
 * pixels, two chances in 256 of state 4. State 4 (sub_0802D484) takes the
 * nearest present target of `targets` (whole-pixel distance), else state 2
 * with + 0x0D 0x80; state 5 dives toward it and 16 + (r & 7) above by
 * 1/16 of the distance until its HP, + 0x0D or a step under 0.25 ends the
 * dive. Facing follows vx (mirrored when vx >= 0). `targets` are the
 * homing list's four slots this frame. Returns false once ended. */
bool aos_imp_guardian_update(AosImpGuardian *imp, AosGuardian *guardian, AosStats *stats,
                             const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                             uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                             int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                             const AosSkillTarget targets[AOS_IMP_TARGETS],
                             uint32_t (*random)(void));
#endif
