#ifndef AOS_OBJECT2D_H
#define AOS_OBJECT2D_H
#include <stdbool.h>
#include <stdint.h>

/* Object2DCreate (0x080588EC) / Object2DUpdate (0x08058938),
 * dispatch index 45 (0x2D): a proximity-armed attack sprite.
 * Create resolves tiles 0x081CBEBC, palette slot
 * sub_08068264(0x0820A100, 2), frames 0x082111F4 with
 * drawFunc sub_0803B9D0 (exported as special_45), presets
 * sub_0803F2C8(2, 0, 0), sets + 0x6F = 0, + 0x6E = 6,
 * + 0x5C = 0x12 and + 0xB = 0xA.
 *
 * Update runs sub_0805872C while + 0xA == 0 then the
 * sub_0803F17C step. sub_0805872C measures the fixed-point
 * delta to the + 0x13110 entity (Soma): dormant state 0xA
 * swaps the sub_0803F2C8 preset by the + 0x6D flags byte
 * (0/3/4 -> preset 1 or 0, 1 -> preset 2, gated on + 0x59
 * bits) and enters active state 0xC when |dx| <= 0x4FFFFF and
 * |dy| <= 0x5FFFFF; active state 0xC swaps presets by + 0x6D
 * (1 -> 3, 2 -> 4, 3/4 -> 0, gated on + 0x59 bit 0), returns
 * to 0xA outside the window and runs sub_0806D128(ent, 3)
 * inside it — the attack step exposed as `attack`. */
typedef struct {
    uint8_t phase;          /* +0xB: 0xA dormant, 0xC active */
    uint8_t flags;          /* +0x6D: preset selector */
    uint8_t marks;          /* +0x59 bits 0/1 preset gates */
    uint8_t anim;           /* sub_0803F17C step count */
    int preset;             /* last sub_0803F2C8 argument */
    unsigned attack;        /* sub_0806D128 calls */
} AosObject2D;

void aos_object2d_create(AosObject2D *o);

/* `dx`/`dy` are the fixed-point deltas entity->+0x40/+0x44
 * minus the + 0x13110 entity's. */
void aos_object2d_update(AosObject2D *o, int32_t dx, int32_t dy);
#endif
