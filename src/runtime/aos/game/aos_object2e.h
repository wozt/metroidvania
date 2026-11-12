#ifndef AOS_OBJECT2E_H
#define AOS_OBJECT2E_H
#include <stdbool.h>
#include <stdint.h>

/* Object2ECreate (0x08058A70) / Object2EUpdate (0x08058AB8),
 * dispatch index 46 (0x2E): a one-shot breakable prop. Create
 * resolves tiles 0x081CBECC, palette slot
 * sub_08068264(0x0820A100, 5), frames 0x0821137C with
 * drawFunc sub_0803B9D0 (exported as special_46), presets
 * sub_0803F2C8(0, 0, 1), steps the animation, sets + 0x5C =
 * 0x12 and marks + 0x58 bit 0x20.
 *
 * Update runs sub_08058B1C while + 0xA == 0: on the first
 * sub_0806CFFC probe hit over the (x - 0x20, y - 0x20, 0x40,
 * 0x70) rectangle it latches + 0xB = 1, spawns three children
 * through sub_080589A4 at fixed-point offsets (-0x50000, 0),
 * (0, -0x60000) and (0, 0x50000) and plays song 0x78. */
typedef struct {
    uint8_t triggered;      /* +0xB latch */
    uint8_t anim;           /* sub_0803F17C step count */
    int children;           /* sub_080589A4 spawns (3) */
    int song;               /* 0x78 when fired, -1 none */
} AosObject2E;

void aos_object2e_create(AosObject2E *o);

/* `hit` is the sub_0806CFFC rectangle probe result. */
void aos_object2e_update(AosObject2E *o, bool hit);
#endif
