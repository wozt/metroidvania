#ifndef AOS_OBJECT31_H
#define AOS_OBJECT31_H
#include <stdbool.h>
#include <stdint.h>

/* Object31Create (0x08059090) / Object31Update (0x080593A8),
 * dispatch index 49 (0x31): the delayed-launch spinning
 * projectile.
 *
 * Create copies four ROM tables (0x0811865C u16 x-screen,
 * 0x08118664 u16 y-screen, 0x0811866C u16 anim select,
 * 0x08118674 u32 wait) indexed by the + 0x30 parameter, loads
 * the sprite triple shared with special_47 — tiles
 * 0x081CBEEC via sub_0803B800, palette
 * sub_08068264(0x0820A1C4, 9), frames 0x08211844,
 * drawFunc sub_0803B9D0 — then sub_0803F2C8(anim, 0, 1) and
 * sub_0803F17C. It deletes when event word + 0x37E bit 2 is
 * set or the sprite load fails. Per frame of setup it rolls
 * + 0x6F = RandomNumberGenerator() % 6, sets + 0x5C = 0x1A,
 * clears + 0x59 bits 0x1D and + 0x58 bit 0x41. Parameters
 * 0 and 2 take the palette-variant path: sub_0803E654(1)
 * picks a free slot in the gUnk_030019D8 palette allocator
 * (returns -1 when full -> EntityDelete); on success the low
 * 5 bits land in + 0x58, + 0x59 gains bit 4, and sub_0803E058
 * receives the + 0x58 sign/bit7/low5 triple with a 0x10000
 * step. All variants then store + 0x42 = x_table[param],
 * + 0x46 = y_table[param] and + 0x14 = wait_table[param]
 * (60/120/240/360 frames).
 *
 * Update calls sub_080591EC only while + 0xA == 0, then
 * sub_0803F17C. The + 0xB machine has 12 cases:
 *  - 0 waits for gEwramData+0x42C bit 12, then -> 1.
 *  - 1 decrements + 0x14; on expiry params 0/2 launch into
 *    10 (+ 0x18 = + 0x1C = 0, + 0x20 = 4, + 0x50 = 0x40000,
 *    + 0x54 = 0x200, + 0xD = 0x3C) and spawn the type-0x49
 *    child via sub_0805940C at + 0xA00000 offset; other
 *    params advance to 2 (+ 0x54 = 0x400).
 *  - 2 runs sub_0806D460(0x20000) y-physics then the
 *    sub_0806D128(3) on-screen probe; a hit -> 3.
 *  - 3 frees the palette slot (sub_0803E6BC on + 0x58 low
 *    bits) and deletes for params 0/2, else -> 4.
 *  - cases 4-9 are empty (render tail only).
 *  - 10 runs x-physics sub_0806D430(0x4000) +
 *    sub_0806D460(0x2000), ticks + 0xD; on expiry -> 11 with
 *    + 0x50 = 0.
 *  - 11 accumulates + 0x1C += + 0x20 clamped to 0x60 into
 *    + 0x18, applies the sub_0803E058 rotation from the
 *    + 0x58 bits, integrates position (sub_0806D3C0),
 *    re-runs y-physics and the probe; a hit -> 3.
 *  - every frame params 0/2 re-apply sub_0803E058 with a
 *    0x20000 step before sub_0803F17C.
 *
 * Approximations: the palette allocator is reduced to a
 * variant index output, sub_0803E058 becomes an angle state,
 * the sub_0806D128(3) probe checks the dumped rect
 * (48,48) -> x in [-48,288], y in [-48,208], and physics
 * uses the shared + 0x4C/+ 0x54 gravity model of Object21. */
typedef struct {
    uint8_t state;          /* +0xB: 0 wait, 1 count, 2 drift,
                             * 3 resolve, 10 launch-wind,
                             * 11 spin, 4 parked */
    uint8_t timer;          /* +0xD: launch-wind countdown */
    int32_t wait;           /* +0x14: pre-launch frame count */
    int32_t spin_rate;      /* +0x20: +4 per launch */
    int32_t spin_acc;       /* +0x1C: clamped to 0x60 */
    int32_t spin_total;     /* +0x18: accumulated rotation */
    int32_t accel_y;        /* +0x54: 0x200 launch / 0x400 drift */
    int32_t accel_x;        /* +0x50: 0x40000 launch */
    int32_t vel_y;          /* +0x4C */
    int32_t vel_x;          /* +0x48 */
    int32_t pos_y;          /* +0x44: 16.16 */
    int32_t pos_x;          /* +0x40: 16.16 */
    int16_t pos_x_px;       /* +0x42: table init, screen px */
    int16_t pos_y_px;       /* +0x46 */
    uint8_t variant_byte;   /* +0x58: palette variant + angle */
    uint8_t flags59;        /* +0x59: bit 4 palette held */
    uint8_t spawn_idx;      /* +0x6F: RNG % 6 */
    uint8_t param;          /* +0x30: variant selector 0-3 */
    int angle;              /* sub_0803E058 approximation */
    bool deleted;           /* EntityDelete ran */
    /* Per-frame outputs, reset by update. */
    bool child_spawn;       /* sub_0805940C type-0x49 fx */
    int palette_slot;       /* sub_0803E654 result, -1 n/a */
} AosObject31;

/* Object31Create. `param` is the + 0x30 variant (0-3),
 * `event_37e` the event word bit 2 gate, `rand6` the
 * RandomNumberGenerator % 6 roll and `palette_ok` the
 * sub_0803E654(1) success approximation (false -> delete). */
void aos_object31_create(AosObject31 *o, int param,
                         uint32_t event_37e, int rand6,
                         bool palette_ok, bool *deleted);

/* Object31Update while + 0xA == 0. `sx`/`sy` are the + 0x42/
 * + 0x46 screen pixels after the sub_0803F17C equivalent and
 * `event_42c` the gEwramData+0x42C word. Returns false when
 * the entity deleted itself this frame. */
bool aos_object31_update(AosObject31 *o, int16_t sx, int16_t sy,
                         uint32_t event_42c);

#endif
