/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_NEEDLES_H
#define AOS_SKILL_NEEDLES_H
#include <stdbool.h>
#include <stdint.h>

enum { AOS_NEEDLES_PIECES = 4, AOS_NEEDLES_FRAMES = 0x80, AOS_NEEDLES_BURST_FRAMES = 0x20 };

/* SkillNeedlesUse's urchin; after its burst, four pieces fly out. */
typedef struct {
    int32_t x, y;           /* + 0x40, + 0x44 (room pixels, 16.16) */
    int32_t vy, gravity;    /* + 0x4C, + 0x54 */
    bool facing_left;
    uint8_t state;          /* + 0x0A: 1 waiting, 2 burst, 3 deleted next */
    uint8_t timer;          /* + 0x0D */
    bool struck;            /* + 0x18: its hit callback sub_0802D9B4 or its timer */
    int16_t radius[AOS_NEEDLES_PIECES];    /* + 0x20 + 4k: 1/256 pixels */
    uint16_t angle[AOS_NEEDLES_PIECES];    /* + 0x22 + 4k */
} AosNeedles;

/* State 0: placed by sub_08031EA4 at Soma's facing and (20, -0x1A), 128 frames. */
AosNeedles aos_needles_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left);
/* One update after its animation step: waiting, its timer runs down (0
 * sets `struck`), and once struck it bursts (`burst`: the dust of
 * sub_08045CEC(x, y, 15, 7) and sound 0xD7): it jumps at -1.0 minus a
 * random fraction with gravity 0x2000 and its pieces take the angles
 * 0xF000 + k * 0x3333 plus a random 0x3FF at radius 0; burst, it falls
 * (sub_0806D460, 8.0) while each piece's radius grows by timer * 32, for
 * 32 frames, then it is deleted (false). Outside the screen margins (mx,
 * my) of entry 3 it is deleted too. */
bool aos_needles_update(AosNeedles *needles, uint32_t (*random)(void), int cam_x, int cam_y,
                        int mx, int my, bool *burst);
/* sub_08028018: whether piece `k` is drawn this frame (it alternates with
 * the timer's parity) and where (room pixels, 16.16). */
bool aos_needles_piece(const AosNeedles *needles, int k, int32_t *x, int32_t *y);
#endif
