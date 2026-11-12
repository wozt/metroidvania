/* Aria of Sorrow special object 6 (sUnk_084F0DF8[6] /
 * sUnk_084F0ED8[6], Object06Create 0x0804EC3C / Object06Update
 * 0x0804ED30): a cinematic trigger with a particle-emitting idle
 * state. Create runs the shared sub_0804DF3C init, writes
 * + 0x5A = (+ 0x5A & 0x87) | 0x28 and the winOut blend, then takes the
 * near branch when EITHER the 0x38 x 0x22 probe around
 * (soma_x - 0x1C, soma_y) hits OR gEwramData + 0x37E bit 2 is set:
 * timer 0x30, state 0xA, SetPlayerInput(1, 0). Far seals the
 * three-block column and picks 0x14/0x63/0 for param0 1/3.
 * Update keeps the + 0x58 off-screen bit (+ 0x58 bit 0x40 = right
 * half of the screen picks the forced direction) and runs:
 *  - state 0: while + 0x37E bit 0 is clear, emits the sub_08045CEC /
 *    sub_08045B44 particle pair every frame (the + 0xD odd-frame
 *    extra drift), then probes Soma again: inside, PlaySong(0x111),
 *    the sub_0803D18C/sub_0803CDF0 fades, the + 0x131D4..+ 0x131DC
 *    snapshot writes and sub_08021248 — the globals that arm the
 *    scene. A set + 0x37E bit 0 or soma + 0x10 bit 0x80 ends the
 *    state-0 branch.
 *  - state 1: SetPlayerInput(1, dir) every frame and the + 0xD
 *    countdown; at 0 advances to state 2.
 *  - state 2: while + 0x4CC bit 6 (cinematic active) holds, forces
 *    the same direction; at timer 0 clears the column once, counts
 *    to 0x30, releases + 0x42C bits 0x101 and advances to state 3
 *    only while the cinematic flag still holds.
 *  - state 3: forces the direction unconditionally. */
#ifndef AOS_OBJECT06_H
#define AOS_OBJECT06_H
#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_collision.h"
#include "../core/aos_soma.h"

typedef struct {
    uint8_t state;          /* + 0x0A: 0 idle, 1 walk-in, 2 release, 3 hold */
    int32_t timer;          /* + 0x14 */
    uint8_t counter;        /* + 0xD: particle cadence / state-1 countdown */
    uint8_t param0;         /* + 0x30 s16: <= unused, 1 -> 0x14, 3 -> 0x63 */
    uint8_t param1;         /* + 0x32: unused by the native code */
    bool facing_right;      /* + 0x1A: screen x > 0x78 (shared init) */
    uint8_t flags58;        /* + 0x58: bit 0x20 off-screen, bit 0x40 right half */
    bool deleted;           /* EntityDelete ran */
    bool scene_active;      /* gUnk_03002CB0 bit 15 / + 0x42C bits 0x101 */
    /* Per-update outputs. */
    int song;               /* PlaySong id, -1 none (0x111) */
    uint16_t forced;        /* SetPlayerInput(1, mask): 0x10 right, 0x20 left */
    bool force_zero;        /* SetPlayerInput(1, 0) */
    bool triggered;         /* the state-0 probe fired: fades + snapshot */
    bool fx_drift;          /* a sub_08045CEC drift particle was requested */
    bool fx_burst;          /* a sub_08045B44 burst particle was requested */
    int32_t fx_x, fx_y;     /* particle room-pixel anchor */
} AosObject06;

/* Object06Create; screen/soma values are screen pixels (room - camera).
 * `layer`/`ex`/`ey` receive the far-branch block seal at room pixels.
 * Returns false when the entity must not spawn (never for Object06 —
 * kept for symmetry with the sibling directors). */
bool aos_object06_create(AosObject06 *o, AosCollision *layer,
                         int param0, int param1, uint16_t special_37e,
                         int32_t screen_x, int32_t screen_y,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         int32_t ex, int32_t ey);

/* Object06Update; `cam_x`/`cam_y` the BG1 origin, `ex`/`ey` room
 * pixels, soma_* screen pixels, `soma_flags10` Soma's + 0x10 word,
 * `special_37e` the + 0x37E bitfield, `cinematic` the + 0x4CC bit 6
 * flag and `rng` the native RandomNumberGenerator. */
void aos_object06_update(AosObject06 *o, AosCollision *layer,
                         int32_t cam_x, int32_t cam_y,
                         int32_t ex, int32_t ey,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         uint16_t soma_flags10, uint16_t special_37e,
                         bool cinematic, uint32_t (*rng)(void));
#endif
