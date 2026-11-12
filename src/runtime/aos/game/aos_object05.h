/* Aria of Sorrow special object 5 (sUnk_084F0DF8[5] /
 * sUnk_084F0ED8[5], Object05Create 0x0804F00C / Object05Update
 * 0x0804F084): a passive proximity trigger. Create runs the shared
 * sub_0804DF3C init, writes + 0x65 = 0x7D and + 0x5A = (+ 0x5A & 0x87)
 * | 0x38 (a draw-time variant), applies the gDisplayRegisters winOut
 * (0x3F/0x2F at + 0x42/+ 0x43) and probes a 0x38 x 0x22 window around
 * Soma (soma_x - 0x1C, soma_y): inside it arms + 0x14 = 0x30, outside
 * the timer stays 0. There is no state write, no block sealing, no
 * SetPlayerInput and no parameter use — params are ignored.
 * Update only maintains the + 0x58 off-screen bit and the state-0
 * timer cycle: inside the (u16)(ex_sx - soma_sx + 0x28) <= 0x4F window
 * the timer climbs by 2 saturating at 0x30, outside it falls by 2 and
 * clears at <= 2; the 0x110 x 0xC0 probe at (-0x10, 0) plays song
 * 0x113 on the timer-0 edge entering the window and on the 0x30 edge
 * leaving it. */
#ifndef AOS_OBJECT05_H
#define AOS_OBJECT05_H
#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_collision.h"
#include "../core/aos_soma.h"

typedef struct {
    uint8_t state;          /* + 0x0A: always 0 (nothing writes it) */
    int32_t timer;          /* + 0x14: 0x30 armed, cycles +-2 */
    uint8_t param0;         /* + 0x30: unused by the native code */
    uint8_t param1;         /* + 0x32: unused by the native code */
    bool facing_right;      /* + 0x1A: screen x > 0x78 (shared init) */
    uint8_t variant;        /* + 0x65 = 0x7D write */
    uint8_t flags5a;        /* + 0x5A = (+ 0x5A & 0x87) | 0x38 */
    uint8_t flags58;        /* + 0x58 bit 0x20: off the screen window */
    /* Per-update outputs. */
    int song;               /* PlaySong id, -1 none (0x113) */
} AosObject05;

/* Object05Create; screen/soma values are screen pixels (room - camera).
 * Returns true: the entity always survives. */
bool aos_object05_create(AosObject05 *o, int32_t screen_x,
                         int32_t screen_y, int32_t soma_screen_x,
                         int32_t soma_screen_y);

/* Object05Update; `sx`/`sy` are the object's screen pixels,
 * `soma_screen_x`/`soma_screen_y` Soma's screen pixels. */
void aos_object05_update(AosObject05 *o, int32_t sx, int32_t sy,
                         int32_t soma_screen_x, int32_t soma_screen_y);
#endif
