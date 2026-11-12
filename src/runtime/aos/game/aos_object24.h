#ifndef AOS_OBJECT24_H
#define AOS_OBJECT24_H
#include <stdbool.h>
#include <stdint.h>

/* Object24Create (0x080578A4) / Object24Update (0x0805790C),
 * dispatch index 36 (0x24): a small ambient flickering sprite.
 * Create resolves tiles 0x0825920C through sub_0803B800, the
 * palette slot through sub_08068264(0x08209FD8, 7) and frames
 * 0x0825CED0 through sub_0803B924 with drawFunc sub_0803B9D0,
 * deletes on load failure, presets sub_0803F2C8(2, 0, 1), draws
 * + 0x6F = rng & 0xF and + 0x6E = rng % 3, steps the animation
 * once and sets + 0x5C = 0x13, + 0x5A = (+ 0x5A & ~0x19) | 2.
 *
 * Update draws one RNG byte per frame: when (rng & 0xFF) <= 0xDF
 * (~88%) it steps the animation through sub_0803F17C, then
 * + 0xD increments and + 0x58 bit 0x20 follows the OLD + 0xD
 * parity — a per-frame flicker mark. */
typedef struct {
    uint8_t variant;        /* +0x6E: rng % 3 */
    uint8_t phase;          /* +0x6F: rng & 0xF */
    uint8_t tick;           /* +0xD */
    uint8_t anim;           /* sub_0803F17C step count */
    bool hidden;            /* +0x58 bit 0x20 on old-tick parity */
    bool advanced;          /* sub_0803F17C ran this frame */
} AosObject24;

/* `r1`/`r2` are the two create-time RNG draws (phase then
 * variant). */
void aos_object24_create(AosObject24 *o, uint32_t r1, uint32_t r2);

/* `rng` is the per-frame RandomNumberGenerator draw. */
void aos_object24_update(AosObject24 *o, uint32_t rng);
#endif
