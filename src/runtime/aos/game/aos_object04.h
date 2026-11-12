/* Aria of Sorrow special object 4 (sUnk_084F0DF8[4] /
 * sUnk_084F0ED8[4], Object04Create 0x0804E770 / Object04Update
 * 0x0804E8C4): the directional sibling of Object02. The create one-shots
 * param0 <= 1 objects whose + 0x37E event bit (param1) already fired,
 * then probes Soma on the APPROACH side only: + 0x1A set (screen x >
 * 0x78) wants soma_x >= ex - 0x18, clear wants soma_x <= ex + 0x18.
 * Near locks the scene like Object02 (state 0xA, SetPlayerInput(1, 0),
 * the conditional + 0x48/+ 0x50 clear); far seals the same three-block
 * column and picks state 0x14/0x63/0 for param0 1/3.
 * Update: the same + 0x58 off-screen bit, the identical state-0 block
 * cycle, but case 2 forces + 0x1A ? RIGHT : LEFT with no + 0x42C hold,
 * case 10 forces the opposite and waits on the directional probe before
 * resealing, case 11 is the release countdown, case 20 the event-bit
 * wait (sub_08013F6C accent skipped while + 0x42C bit 0x40000000 is
 * set) and case 21 the timer-0x30 block clear. */
#ifndef AOS_OBJECT04_H
#define AOS_OBJECT04_H
#include <stdbool.h>
#include <stdint.h>

#include "../core/aos_collision.h"
#include "../core/aos_soma.h"

typedef struct {
    uint8_t state;          /* + 0x0A: 0 scene, 2/10 walk, 11 release, 20/21 edges */
    int32_t timer;          /* + 0x14 */
    uint8_t counter;        /* unused here (Object02's sparkle cadence) */
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
} AosObject04;

/* Object04Create: the directional approach probe runs inside (screen
 * pixels). Returns false when the + 0x37E event bit already fired
 * (param0 <= 1) and the entity must not spawn. */
bool aos_object04_create(AosObject04 *o, int param0, int param1,
                         uint16_t special_37e, int32_t screen_x,
                         int32_t soma_screen_x, int32_t soma_vx);

/* Object04Update; `layer` receives the block writes, `cam_x`/`cam_y`
 * are the BG1 whole-pixel origin (gEwramData + 0xA094 + 6/+ 0xA),
 * `soma_screen_x`/`soma_screen_y` are Soma's screen pixels, `ex`/`ey`
 * the object's room pixels (screen = room - camera). */
void aos_object04_update(AosObject04 *o, AosCollision *layer,
                         int32_t cam_x, int32_t cam_y,
                         int32_t ex, int32_t ey,
                         int32_t soma_screen_x, int32_t soma_screen_y,
                         uint16_t special_37e, uint32_t (*rng)(void));
#endif
