#ifndef AOS_OBJECT29_H
#define AOS_OBJECT29_H
#include <stdbool.h>
#include <stdint.h>

/* Object29Create (0x08057080) / Object29Update (0x08057380) /
 * sub_08057180, dispatch index 41 (0x29): the ping-pong
 * moving platform.
 *
 * Create memcpy's the four 4-byte collision boxes at
 * 0x08118630, runs the sub_080573B0 sprite attach — the
 * 0x085264D0 record table (stride 16, indexed by the + 0x32
 * param: tiles/palette/frames pointers, palette bank at +
 * 0xD, anim seq at + 0xC, dwell u16 at + 0xE) resolving to
 * the special_42_v00-v03 triples — deletes on load failure,
 * installs the sub_0804277C(8) hurt slot and the param-indexed
 * box through sub_080428B4/sub_08042848, sets + 0x72 |= 6,
 * sub_0803F2C8(record seq, 0, 1), + 0x5C = 0x12, + 0xA = 0,
 * + 0x18 = + 0x44 + A094[8] (bottom edge), + 0x1C =
 * param0 << 16 (far threshold), + 0x10/+ 0x14 = entity pos +
 * A094[4]/[8] (world origin), + 0x24/+ 0x28 = screen pos,
 * + 0x59 &= ~3 and + 0x20 = (+ 0x18 > + 0x1C).
 *
 * Update calls sub_08021278(1) every frame, runs sub_08057180
 * while + 0xA == 0, then sub_0803F17C and the sub_080421AC
 * slot read. The tail rewrites + 0x40/+ 0x44 from + 0x24/+ 0x28
 * — the platform carries whatever the collision layer sticks
 * to it.
 *
 * The + 0xB machine ping-pongs between the + 0x1C threshold
 * (states 0/1) and the + 0x18 bottom (states 2/3) with a
 * spring velocity + 0x4C: far from target (> 0xFFFFF) the
 * velocity accelerates by +/-0x800 per frame, near target it
 * is SET to +/-(dist_px * 0x1000 + 0x1000) — a critically
 * damped approach. Reaching the target enters the dwell state
 * (+ 0xD = record dwell, 2 frames in the dumped table) and
 * toggles the + 0x20 direction on the way out, so the
 * platform alternates threshold <-> bottom forever.
 *
 * Approximations: the sprite resolves through the exported
 * special_42_v00-v03 variant art (param1 & 3), the collision
 * boxes and hurt slot stay under special_sprites, rider
 * carry is not modelled and there is no emulator comparison. */
typedef struct {
    uint8_t state;          /* +0xB: 0 to-threshold, 1 dwell,
                             * 2 to-bottom, 3 dwell */
    uint8_t timer;          /* +0xD: dwell countdown */
    uint8_t dir;            /* +0x20: approach direction, toggled
                             * on each dwell exit */
    int32_t vel_y;          /* +0x4C: spring velocity */
    int32_t pos_x;          /* +0x10: world origin x */
    int32_t pos_y;          /* +0x14: world origin y, 16.16 */
    int32_t scr_x;          /* +0x24 */
    int32_t scr_y;          /* +0x28 */
    int32_t world_x;        /* +0x40 mirror */
    int32_t world_y;        /* +0x44 mirror */
    int32_t threshold;      /* +0x1C: param0 << 16 */
    int32_t bottom;         /* +0x18: + 0x44 + A094[8] */
    int16_t param0;         /* +0x30: threshold source */
    int16_t param1;         /* +0x32: record/box selector */
    bool deleted;           /* sprite load failure */
} AosObject29;

/* Object29Create. `param0` is the + 0x30 threshold source,
 * `param1` the + 0x32 record selector, `room_x`/`room_y` the
 * A094 + 4 / + 8 world-origin fields and `sprite_ok` the
 * sub_080573B0 load result. */
void aos_object29_create(AosObject29 *o, int16_t param0,
                         int16_t param1, int32_t room_x,
                         int32_t room_y, int32_t ent_x,
                         int32_t ent_y, bool sprite_ok,
                         bool *deleted);

/* Object29Update while + 0xA == 0. `room_x`/`room_y` are the
 * A094 + 4 / + 8 fields subtracted for the screen mirror. */
void aos_object29_update(AosObject29 *o, int32_t room_x,
                         int32_t room_y);

#endif
