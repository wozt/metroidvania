/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SOUL_BLUE_DEATH_H
#define AOS_SOUL_BLUE_DEATH_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_anim.h"
#include "aos_soul_red_devil.h"
#include "aos_soul.h"

/* SkillDeathUse (blue soul 24, toggled): an unseen owner 32 pixels above
 * Soma with his facing, guardian power 20 and elements 1 (0x131C2,
 * 0x131C4; sub_08042A54(2, 0x14) is not run). Eight frames after it starts
 * and then every 56 it throws a sickle (sub_08030238, a guardian slot of
 * sub_0802DD00) up to 32 pixels from it on each axis. A sickle (art
 * 0x081E2174, translucent, sound 0x96) grows by 0x2000 a frame to unit
 * scale while it spins (+ 0x2C by + 0x2E: -0x900, or +0x900 when mirrored
 * at random) through its sequence 0; then it seeks the nearest homing
 * target (sub_0802D484): state 2 turns its heading 0x555 a frame toward it
 * (sub_0802D408) at a speed growing by 0x2000 to 2.0, for 64 frames or
 * until the target dies; state 3 (no target, or after) curves by a 16th of
 * its spin. Both strike as type 6 (box 0x1010F8F8: -8, -8, 16 x 16) and
 * move with sub_0806D3D8(e, 8.0). Beyond screen margin 1 it goes. The
 * guardian's end deletes its sickles (sub_0802DD84). */
enum { AOS_DEATH_SICKLES = 7, AOS_DEATH_POWER = 20, AOS_DEATH_ELEMENTS = 1,
       AOS_DEATH_TARGETS = 4 };

typedef struct {
    bool alive;
    uint8_t state;          /* + 0x0A: 0 created .. 4 gone */
    uint8_t timer;          /* + 0x0D */
    int32_t x, y;           /* + 0x40, + 0x44 (16.16) */
    int32_t vx, vy;         /* + 0x48, + 0x4C */
    int32_t scale;          /* + 0x28 */
    int32_t speed;          /* + 0x1C */
    uint16_t angle;         /* + 0x2C */
    int16_t spin;           /* + 0x2E */
    uint16_t heading;       /* + 0x34 */
    bool mirrored;          /* + 0x58 bit 6 */
    bool attacking;         /* the type-6 block registered */
    int target;             /* the homing slot chased (+ 0x30), -1 for none */
    AosAnimState anim;
} AosDeathSickle;

typedef struct {
    int32_t x, y;           /* whole pixels */
    bool facing_left;
    uint8_t timer;          /* + 0x0D */
    AosDeathSickle sickles[AOS_DEATH_SICKLES];
} AosDeathGuardian;

/* One update: the owner, then its sickles (their art `anims`, sequence 0;
 * `targets` the homing list this frame; the camera and margin 1 in room
 * pixels). Returns false once the guardian has ended (its sickles gone). */
bool aos_death_guardian_update(AosDeathGuardian *death, AosGuardian *guardian, AosStats *stats,
                               const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                               uint16_t held, uint16_t pressed, uint32_t *soma_flags,
                               int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                               const AosSkillTarget targets[AOS_DEATH_TARGETS],
                               const AosAnimSet *anims, int cam_x, int cam_y, int margin_x,
                               int margin_y, uint32_t (*random)(void));
#endif
