#ifndef AOS_OBJECT27_H
#define AOS_OBJECT27_H
#include <stdbool.h>
#include <stdint.h>

/* Object27Create (0x08058258) / Object27Update (0x08058464),
 * dispatch index 39 (0x27): a spinning attack orb sharing the
 * chandelier art (tiles 0x081CBE7C, palette slot
 * sub_08068264(0x08209FD8, 2), frames 0x08210264 = special_19).
 * Create presets sub_0804277C(8, sub_080584D8, 0) — the hurt
 * callback that probes through sub_08021654 — sub_0803F2C8(2,0,1),
 * derives + 0x3C from the + 0x131FE/+ 0x1327E camera words, sets
 * + 0x5C = 0x12 and + 0x59 bit 1, and marks + 0x58 bit 0x20.
 *
 * Update runs sub_08058344 while + 0xA == 0, then the
 * sub_0803F17C animation step and the sub_080421AC attack-slot
 * read. The + 0xB machine: state 0 waits for the linked + 0x14
 * entity's + 0x18 u16 to exceed 0x77, then clears it, enters 1
 * with + 0x48 = 0xC000; state 1 ramps + 0x48 by 0x800 a frame
 * until it passes 0xFFFF, then clamps to 0x40000, enters 2 with
 * + 0xD = 0x1E and refreshes the attack preset; state 2 counts
 * + 0xD down and enters 3 after the u8 wrap; state 3 bleeds
 * + 0x48 by 0x800 until it reaches 0xC000, then resets to state
 * 0 and sets + 0x58 bit 0x20. Every active state calls
 * sub_0803E238 with + 0x48 as the angular step. */
typedef struct {
    uint8_t phase;          /* +0xB machine state 0-3 */
    uint8_t timer;          /* +0xD: 0x1E hold in state 2 */
    uint8_t variant;        /* +0x3C: camera-derived byte */
    uint32_t spin;          /* +0x48: angular velocity */
    uint32_t angle;         /* sub_0803E238 accumulator */
    bool attack_armed;      /* +0x72 bit 1 cleared at spin-up */
    bool done;              /* +0x58 bit 0x20 */
    bool preset_refreshed;  /* state-1 -> 2 sub_08042848 call */
} AosObject27;

/* `variant` is the + 0x3C byte Object27Create derives from the
 * camera words (passed in by the runtime). */
void aos_object27_create(AosObject27 *o, uint8_t variant);

/* `target_far` is the + 0x14 entity + 0x18 u16 > 0x77 test that
 * arms state 0. */
void aos_object27_update(AosObject27 *o, bool target_far);
#endif
