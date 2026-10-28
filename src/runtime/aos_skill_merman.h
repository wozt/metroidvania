/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_MERMAN_H
#define AOS_SKILL_MERMAN_H
#include <stdbool.h>
#include <stdint.h>

enum {
    AOS_MERMAN_DROPS = 5,
    AOS_MERMAN_SPOUT_SEQUENCE = 0,  /* SkillMermanUse's sequence of resource 0 */
    AOS_MERMAN_DROP_SEQUENCE = 6,   /* sub_08026DB8's looping sequence */
};

/* One drop of SkillMermanUse (sub_08026DB8). */
typedef struct {
    int32_t x, y;       /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vx, vy;     /* + 0x48, + 0x4C */
    int32_t ax;         /* + 0x50 */
    int32_t gravity;    /* + 0x54 */
    bool facing_left;   /* + 0x58 bit 6 */
    uint8_t state;      /* + 0x0A: 0 created, 1 striking, 2 falling, 3 deleted next */
    uint8_t timer;      /* + 0x0D */
    uint8_t splash;     /* + 0x0B: the timer value that drops a splash */
    bool attacking;     /* + 0x72 bit 1 clear */
} AosMermanDrop;

/* SkillMermanUse's state 0 children: drop `index` (0-4) at the spout's
 * position, `index + 1` pixels a frame ahead (mirrored facing left), slowed
 * by its 128th (rounded toward zero) and with gravity 0x2000. */
AosMermanDrop aos_merman_drop_create(int index, int32_t x, int32_t y, bool facing_left);
/* One update of sub_08026DB8: state 0 arms its type-5 attack and a 16-frame
 * timer, then (same frame) state 1 moves along X only, counting down; on
 * the frame the timer reaches 0 the attack is disabled and state 2 falls
 * (sub_0806D3D8, limit 8 pixels a frame). The tail sends it to state 3
 * outside the screen margins (mx, my) of entry 3, deleted the next update
 * (false). `*splash` is set on the frame sub_0802DAD4's splash would be
 * created. */
bool aos_merman_drop_update(AosMermanDrop *drop, int cam_x, int cam_y, int mx, int my,
                            bool *splash);
#endif
