/* Aria of Sorrow special object 3 (sUnk_084F0DF8[3] /
 * sUnk_084F0ED8[3], Object03Create 0x0804E4A0 / Object03Update
 * 0x0804E52C): the escort variant of the cutscene directors, used by
 * 28 exported rooms. Create ALWAYS seals the three-block column
 * (y - 8, y - 0x18, y - 0x28 at the entity anchor plus the
 * gEwramData + 0xA094 camera origin), zeroes + 0x14 and picks
 * 0x14/0x63/0 for param0 1/3 — no probe, no one-shot, no input.
 * Update keeps the + 0x58 off-screen bit (+ 0x58 bit 0x40 = right
 * half of the screen picks the forced direction) and runs:
 *  - state 0: skipped entirely while gEwramData + 0x13220 is non-zero
 *    (the escort-busy word). With + 0x4CC bit 6 (cinematic) clear it
 *    advances straight to state 2. With it set it waits for Soma to
 *    stand in the escort window — (u16)(ex_sx - soma_sx + 0x18)
 *    <= 0x2F forces the direction, (u16)(ex_sx - soma_sx + 0x13)
 *    <= 0x26 must also hold — then requires BOTH partner entities
 *    (+ 0x13114 and + 0x13118) inside their own 0x26 windows with
 *    equal screen x before arming + 0xD = 4 and advancing to state 1.
 *  - state 1: SetPlayerInput(1, dir) every frame while the + 0xD
 *    countdown runs; at 0 advances to state 2.
 *  - state 2: while the cinematic flag holds, forces the direction;
 *    at timer 0 clears the column and plays song 0x113; the timer
 *    counts to 0x30, releases + 0x42C bits 0x101 and advances to
 *    state 3 only while the cinematic flag still holds.
 *  - state 3: forces the direction unconditionally. */
#ifndef AOS_OBJECT03_H
#define AOS_OBJECT03_H
#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_collision.h"
#include "../core/aos_soma.h"

typedef struct {
    uint8_t state;          /* + 0x0A: 0 wait, 1 walk, 2 release, 3 hold */
    int32_t timer;          /* + 0x14 */
    uint8_t counter;        /* + 0xD: escort arm value / state-1 countdown */
    uint8_t param0;         /* + 0x30 s16: 1 -> 0x14, 3 -> 0x63 */
    uint8_t param1;         /* + 0x32: unused by the native code */
    bool facing_right;      /* + 0x1A: screen x > 0x78 (shared init) */
    uint8_t flags58;        /* + 0x58: bit 0x20 off-screen, bit 0x40 right half */
    bool deleted;           /* EntityDelete ran */
    bool scene_active;      /* gUnk_03002CB0 bit 15 / + 0x42C bits 0x101 */
    /* Per-update outputs. */
    int song;               /* PlaySong id, -1 none (0x113) */
    uint16_t forced;        /* SetPlayerInput(1, mask): 0x10 right, 0x20 left */
    bool force_zero;        /* SetPlayerInput(1, 0) */
} AosObject03;

/* Object03Create; `layer`/`ex`/`ey` receive the always-on block seal
 * at room pixels, `screen_x` feeds the shared-init facing. Always
 * returns true — the entity always survives. */
bool aos_object03_create(AosObject03 *o, AosCollision *layer,
                         int param0, int param1,
                         int32_t screen_x, int32_t ex, int32_t ey);

/* Object03Update; `cam_x`/`cam_y` the BG1 origin, `ex`/`ey` room
 * pixels, soma_* screen pixels, `escort_busy` the + 0x13220 word
 * (non-zero skips state 0), `cinematic` the + 0x4CC bit 6 flag,
 * `partner1_sx`/`partner2_sx` the + 0x13114/+ 0x13118 entities'
 * screen x (pass a sentinel like INT32_MIN when absent). */
void aos_object03_update(AosObject03 *o, AosCollision *layer,
                         int32_t cam_x, int32_t cam_y,
                         int32_t ex, int32_t ey,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         bool escort_busy, bool cinematic,
                         int32_t partner1_sx, int32_t partner2_sx);
#endif
