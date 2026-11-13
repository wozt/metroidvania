#ifndef AOS_OBJECT33_H
#define AOS_OBJECT33_H
#include <stdbool.h>
#include <stdint.h>

/* Object33Create (0x080598B0) / Object33Update (0x08059B9C),
 * dispatch index 51 (0x33): the dual-burst ambient particle
 * emitter.
 *
 * Create runs the sub_08059C04 sprite attach — tiles
 * 0x081CBEFC through sub_0803B800, palette
 * sub_08068264(0x0820A32C, 0), frames 0x08211CB0,
 * drawFunc sub_0803B9D0 — deletes on load failure, then
 * sub_0803F2C8(param == 1, 0, 1) and sub_0803F17C. It sets
 * + 0x5A = (+ 0x5A & ~7) | 2, + 0x5C = 9, + 0x58 =
 * (+ 0x58 & 0x4A) keeping only bits 1/3/6, ORs bit 0x20 for
 * param 0 and clears + 0xA.
 *
 * Update calls sub_08059C50 while + 0xA == 0, then
 * sub_0803F17C. The tick only runs while + 0xB == 0: it
 * pre-decrements + 0xD and, when the OLD u8 was 0 or 1,
 * reloads + 0xD = (RandomNumberGenerator() & 0xF) + 0x10
 * (16-31 frames), fires sub_08059928 — the burst-A type-0x49
 * child copying the parent position with random +/-65536
 * x drift (param 2: +/-0x20000), a small negative y kick,
 * + 0x54 = 0x2800 gravity, + 0x65 = (RNG & 3) + 0xB, + 0x72
 * |= 6, + 0x58 bits 6/7 from RNG, + 0x59 |= 2, + 0xD = 0x32 —
 * and, when the + 0x30 param is nonzero, sub_08059A8C — the
 * burst-B child scattered +/-0x100000 in x with a
 * (RNG % 0xC000) - 0x6000 x velocity and a -0x4000..-0x2000
 * y velocity. Both children share the 0x081CBEFC /
 * 0x0820A32C / 0x08211CB0 triple and tick down through
 * sub_08059C90/sub_08059BBC — sub_0806D490 physics,
 * sub_0806D3D8 integration, the sub_0806D518 phase table
 * driving + 0x58 bit 5 and a + 0x59 bit-3 done mark.
 *
 * Approximations: both child kinds collapse to per-burst
 * effect outputs (their velocities, gravity, phase-table
 * anim and despawn marks are not modelled), the sprite load
 * is an always-available flag and there is no emulator
 * comparison. */
typedef struct {
    uint8_t state;          /* +0xB: emitter gate, always 0 */
    uint8_t timer;          /* +0xD: 16-31 frame cadence */
    uint8_t flags5a;        /* +0x5A: (old & ~7) | 2 */
    uint8_t byte58;         /* +0x58: kept bits 1/3/6, bit 5
                             * for param 0 */
    uint8_t param;          /* +0x30: 0 no burst B, 1 alt anim,
                             * 2 wide x drift */
    bool deleted;           /* sprite load failure */
    /* Per-frame outputs, reset by update. */
    bool burst_a;           /* sub_08059928 child fired */
    bool burst_b;           /* sub_08059A8C child fired */
} AosObject33;

/* Object33Create. `param` is the + 0x30 selector and
 * `sprite_ok` the sub_08059C04 load result (false -> delete). */
void aos_object33_create(AosObject33 *o, int param, bool sprite_ok,
                         bool *deleted);

/* Object33Update while + 0xA == 0. `rng` supplies the
 * RandomNumberGenerator rolls for the cadence and child
 * variants. */
void aos_object33_update(AosObject33 *o, uint32_t (*rng)(void));

#endif
