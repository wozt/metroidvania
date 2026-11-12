#ifndef AOS_OBJECT26_H
#define AOS_OBJECT26_H
#include <stdbool.h>
#include <stdint.h>

/* Object26Create (0x08057E04) / Object26Update (0x08057EE0),
 * dispatch index 38 (0x26): the chandelier hub. Create loads the
 * shared chandelier art through sub_08057F00 (tiles 0x081CBE7C,
 * palette sub_08068264(0x08209FD8, 2), frames 0x08210264 =
 * special_19, drawFunc sub_0803B9D0), sets + 0x72 |= 6,
 * sub_0803F2C8(3,0,1), steps the animation once, writes + 0x5C =
 * 0x12, clears + 0x58 bit 6, sets + 0x59 bit 1 and + 0x58 bit
 * 0x20, then + 0x1A = 0x5A, + 0x18 = 0x1E, + 0xA = 0 and spawns
 * two type-0x1B children linked through + 0x14: an Object28 orb
 * at entity (+ 0x42 - 0x15, + 0x46 + 0x44) and an Object27 orb
 * at (+ 0x42 + 0x12, + 0x46 + 0x2C).
 *
 * Update runs sub_08057F4C while + 0xA == 0, then the
 * sub_0803F17C animation step. The probe (gated on + 0xB == 0)
 * tests the 0x8C x 0xC8 box at (+ 0x42 - 0x46, + 0x46 - 0x64)
 * through sub_0806CFFC; a hit increments the + 0x18 and + 0x1A
 * u16 counters and either way sets + 0x58 bit 0x20. Each child
 * orb's state 0 reads its parent's counter as a signed s16 >
 * 0x77 and CONSUMES it (strh 0) on arming — Object27 watches
 * + 0x18, Object28 watches + 0x1A — so the orbs charge and
 * release cyclically. */
typedef struct {
    uint16_t counter_a;     /* +0x18: the Object27 child's gate */
    uint16_t counter_b;     /* +0x1A: the Object28 child's gate */
    bool done;              /* +0x58 bit 0x20 */
} AosObject26;

void aos_object26_create(AosObject26 *o);

/* `soma_inside` is the sub_0806CFFC result for the probe box this
 * frame. */
void aos_object26_update(AosObject26 *o, bool soma_inside);
#endif
