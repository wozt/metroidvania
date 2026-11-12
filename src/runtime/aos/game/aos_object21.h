#ifndef AOS_OBJECT21_H
#define AOS_OBJECT21_H
#include <stdbool.h>
#include <stdint.h>

/* Object21Create (0x08056550) / Object21Update (0x08056784),
 * dispatch index 33 (0x21): the cyclic lift director.
 * Create runs the sub_080567AC sprite attach — tiles
 * 0x081CBE94 through sub_0803B800, palette slot
 * sub_08068264(0x08209FD8, 1), frames 0x082107EC,
 * drawFunc sub_0803B9D0 — installs the sub_080567F8 hurt
 * callback through sub_0804277C(8, ...), the 0x143C29E2
 * collision preset through sub_08042848, + 0x72 =
 * (+ 0x72 | 4) & 0xFD, sub_0803F2C8(0, 0, 1) and
 * sub_0803F17C, derives the + 0x3C camera byte, sets
 * + 0x5C = 0x12, + 0xA = 0, places + 0x42 = 0x40 -
 * A094[6] and + 0x46 = 0x18 - A094[0xA] and seeds
 * + 0x54 = 0x2800, deleting on load failure.
 *
 * Update runs sub_08056618 while + 0xA == 0, then
 * sub_0803F17C and the sub_080421AC slot read every frame.
 * The + 0xB machine cycles 0-4: states 0 and 1 run
 * sub_0806D460 physics (+ 0x4C += + 0x54, clamped to
 * +/- 0x70000, + 0x44 += + 0x4C) against the upper bound
 * 0x7F - A094[0xA]; state 0 fires sub_0803F2C8(3, 0, 1)
 * and PlaySong 0x7F with + 0x4C = -0x14000 into state 1,
 * state 1 zeroes + 0x4C into state 2. State 2 waits the
 * + 0xD > 0x3B timeout into state 3. State 3 pins
 * + 0x4C = -0x10000 and integrates + 0x44 directly until
 * + 0x46 reaches 0x18 - A094[0xA] (anim 0, 0, 1 when
 * + 0x6D == 3) into state 4, which waits the same timeout
 * and resets to state 0 with + 0x4C = 0. */
typedef struct {
    uint8_t state;          /* +0xB: 0 fall, 1 settle, 2 hold,
                             * 3 rise, 4 rest */
    uint8_t timer;          /* +0xD: state 2/4 timeout */
    int32_t vel;            /* +0x4C: 16.16 y velocity */
    int32_t accel;          /* +0x54: 0x2800 at create */
    int32_t pos_y;          /* +0x44: 16.16 y position */
    int32_t pos_x;          /* +0x40: 16.16 x position */
    uint16_t pos_x_px;      /* +0x42 */
    int16_t pos_y_px;       /* +0x46 mirror for the bound reads */
    uint8_t flag6d;         /* +0x6D: collision flag, input */
    uint8_t cam_byte;       /* +0x3C: camera-derived byte */
    bool deleted;           /* EntityDelete ran (load fail) */
    /* Per-frame outputs, reset by update. */
    int song;               /* PlaySong id, -1 none */
    int anim_seq;           /* sub_0803F2C8 sequence, -1 none */
} AosObject21;

/* Object21Create. `room_ox` / `room_oy` are the A094 + 6 /
 * + 0xA u16 fields and `cam_byte` the + 0x3C approximation
 * (the runtime feeds param1, as Object27 does). */
void aos_object21_create(AosObject21 *o, uint16_t room_ox,
                         uint16_t room_oy, int cam_byte,
                         bool *deleted);

/* Object21Update while + 0xA == 0. `room_oy` is the A094 +
 * 0xA s16 the bound reads subtract and `flag6d` the + 0x6D
 * byte. */
void aos_object21_update(AosObject21 *o, int16_t room_oy,
                         uint8_t flag6d);
#endif
