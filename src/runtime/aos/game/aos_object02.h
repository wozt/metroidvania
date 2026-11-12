/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_OBJECT02_H
#define AOS_OBJECT02_H
#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_collision.h"
#include "../core/aos_soma.h"

/* Aria of Sorrow special object 2 (the sUnk_084F0DF8[2] /
 * sUnk_084F0ED8[2] pair, Object02Create 0x0804DFAC /
 * Object02Update 0x0804E0F8): a cutscene director that seals the
 * room with three solid blocks, walks Soma with forced input, and
 * releases him when the scene ends.
 *
 * The native entity keeps its position in screen pixels (+ 0x42/+ 0x46);
 * the runtime stores the room-pixel spawn and subtracts the camera, the
 * same approximation aos_door uses. sub_08068AD4 is the unsigned
 * proximity probe (u16)(ex - ax) <= w && (u16)(ey - ay) <= h over those
 * screen pixels. sub_08002200/sub_08002248 are the solid-block override
 * (aos_collision_set_block) at the entity anchor plus the gEwramData
 * + 0xA094 camera origin, at y - 8, y - 0x18 and y - 0x28.
 * SetPlayerInput(1, keys) becomes `forced`; the + 0x42C bits 0x101 hold
 * becomes `holding`; gUnk_03002CB0 bit 15 becomes `scene_active`.
 * gDisplayRegisters winOut_L/H = 0x3F/0x2F (the window-out blend) and
 * sub_080235C8's per-room bookkeeping are not modelled. */

typedef struct {
    uint8_t state;          /* + 0x0A: 0 scene, 2/10 walk, 11 release, 20/21 edges */
    int32_t timer;          /* + 0x14 */
    uint8_t counter;        /* + 0x0D: sparkle cadence (& 3) */
    uint8_t param0;         /* + 0x30 s16: <=1 one-shot, 1 -> 0x14, 3 -> 0x63, 7 sparkle variant */
    uint8_t param1;         /* + 0x32 s16: gEwramData + 0x37E event-bit index, ==7 variant */
    bool facing_right;      /* + 0x1A: screen x > 0x78 */
    uint8_t anim;           /* + 0x1B: 6, or 9 when param1 == 7 */
    uint8_t flags58;        /* + 0x58 bit 0x20: off the screen window */
    bool deleted;           /* EntityDelete ran */
    bool holding;           /* + 0x42C bits 0x101 held while walking */
    bool scene_active;      /* gUnk_03002CB0 bit 15 */
    /* Per-update outputs. */
    int song;               /* PlaySong id, -1 none (0x113 everywhere here) */
    uint16_t forced;        /* SetPlayerInput(1, mask) */
    bool force_zero;        /* SetPlayerInput(1, 0) */
    bool zero_soma_vx;      /* the create's conditional + 0x48/+ 0x50 clear */
    bool sparkle;           /* a sub_08045CEC drift was requested */
    int32_t sparkle_x, sparkle_y; /* its room-pixel position */
} AosObject02;

/* Object02Create. `spawn` decides the branch: the native probes the
 * entity screen position against (soma screen x - 0x1C, soma screen y)
 * within 0x38 x 0x22; `near` is that result computed by the caller.
 * Returns false when the + 0x37E event bit already fired (param0 <= 1)
 * and the entity must not spawn. */
bool aos_object02_create(AosObject02 *o, int param0, int param1,
                         uint16_t special_37e, bool near,
                         int32_t screen_x, int32_t soma_screen_x,
                         int32_t soma_vx);

/* Object02Update; `layer` receives the block writes, `cam_x`/`cam_y`
 * are the BG1 whole-pixel origin (gEwramData + 0xA094 + 6/+ 0xA),
 * `soma_screen_x`/`soma_screen_y` are Soma's screen pixels, `ex`/`ey`
 * the object's room pixels (screen = room - camera). `rng` feeds the
 * param0 == 7 sparkle. */
void aos_object02_update(AosObject02 *o, AosCollision *layer,
                         int32_t cam_x, int32_t cam_y,
                         int32_t ex, int32_t ey,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         uint16_t special_37e, uint32_t (*rng)(void));
#endif
