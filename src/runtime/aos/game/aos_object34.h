#ifndef AOS_OBJECT34_H
#define AOS_OBJECT34_H
#include <stdbool.h>
#include <stdint.h>

/* Object34Create (0x08059CE0) / Object34Update (0x08059D7C) /
 * sub_08059D98, dispatch index 52 (0x34): the waterfall
 * splash. Create loads the sprite through sub_0805A120
 * (tiles 0x081CBF14, palette slot sub_08068264(0x0820A350,
 * 2), frames 0x08211DC4 = special_52), sets + 0x72 bits 1-2,
 * + 0x65 = 3, + 0x5C = 0x12, + 0x18 = 0x4000 and + 0x1C =
 * the + 0x32 u16 param. The + 0x30 s16 param picks the
 * direction: == 0 gives + 0xB = 0 with + 0x1A = 0x100,
 * otherwise + 0xB = 1 with + 0x1A = 0xFF00 (-256). Positions
 * live in the bgInfo[1] scroll space like Object2A: + 0x10/
 * + 0x14 keep the world copy, + 0x24/+ 0x28 and + 0x40/+ 0x44
 * the screen copies. Three sub_08059E4C children (type-0x49
 * particles on frames 0x08211DC4, layer ids 2/1/0) spawn at
 * create; the layer-2 child also gets the attack preset.
 *
 * Update runs sub_08059D98 while + 0xA == 0 — no animation
 * step, no attack read: it refreshes the screen copies from
 * the world position minus the current bgInfo[1] scroll,
 * then ping-pongs + 0x18: state 0 accelerates + 0x1A down by
 * + 0x1C (floored at -0x100) and adds it to + 0x18 until the
 * 16.16 product exceeds 0x3FFF0000, flipping to state 1;
 * state 1 accelerates + 0x1A up by + 0x1C (ceiled at 0x100)
 * until the product exceeds 0x40000000, flipping back. The
 * epilogue writes + 0x40/+ 0x44 = world - scroll. */
typedef struct {
    uint8_t phase;          /* +0xB ping-pong direction 0/1 */
    uint16_t offset;        /* +0x18 oscillating value */
    int16_t vel;            /* +0x1A per-frame delta */
    uint16_t accel;         /* +0x1C: the +0x32 u16 param */
    int32_t world_x;        /* +0x10: screen pos + scroll x */
    int32_t world_y;        /* +0x14: screen pos + scroll y */
    int32_t screen_x;       /* +0x24/+0x40: world - scroll x */
    int32_t screen_y;       /* +0x28/+0x44: world - scroll y */
    int child_count;        /* the three sub_08059E4C children */
    bool deleted;           /* EntityDelete ran (sprite load fail) */
} AosObject34;

/* `param0` is the entity + 0x30 s16 direction pick, `param1`
 * the + 0x32 u16 acceleration, `scroll_x`/`scroll_y` the
 * bgInfo[1] scroll words and `x`/`y` the entity position in
 * the same screen space (16.16). */
void aos_object34_create(AosObject34 *o, int16_t param0,
                         uint16_t param1, int32_t scroll_x,
                         int32_t scroll_y, int32_t x, int32_t y);
void aos_object34_update(AosObject34 *o, int32_t scroll_x,
                         int32_t scroll_y);
#endif
