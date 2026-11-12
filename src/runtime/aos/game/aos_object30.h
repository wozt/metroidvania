#ifndef AOS_OBJECT30_H
#define AOS_OBJECT30_H
#include <stdbool.h>
#include <stdint.h>

#include "aos_pickup.h"

/* Object30Create (0x08058F88) / Object30Update (0x08058FD0),
 * dispatch index 48 (0x30): the ambient sparkle emitter.
 * Create runs the sub_08058FE8 sprite attach — tiles
 * 0x081CBEEC through sub_0803B800, palette slot
 * sub_08068264(0x0820A1C4, 9), frames 0x08211844, the shared
 * Object2F art exported as special_47 — then
 * sub_0803F2C8(0, 0, 1), sub_0803F17C, + 0x5C = 0x19,
 * + 0xA = 0 and + 0x58 bit 0x20, deleting on load failure.
 *
 * Update only runs while + 0xA == 0 (always: nothing writes
 * it) through sub_08059034. The + 0xB byte waits at 0 for
 * the gEwramData + 0x42C global word's bit 0x1000, latches
 * to 1 and thereafter counts + 0xD down every frame; when
 * the OLD u8 value is 0 or 1 it reloads + 0xD to
 * (RandomNumberGenerator() & 0x7F) + 0x20 and fires
 * sub_08058D7C — one type-0x49 child (callback
 * sub_08058EDC) at screen x RNG % 0xF0, variant RNG % 3,
 * the parent's + 0x5C / + 0x2C sprite state, fall speeds
 * 0xA0000 / 0x80000 / 0xC0000 by variant and a
 * (RNG & 0x3FF) - 0x200 drift on + 0x54. The child's
 * sub_08058EDC update integrates through sub_0806D3CC,
 * applies the A094[1] scroll delta to + 0x40 scaled by
 * variant (1/4, 1/8 or 3/8 — the parallax factors), probes
 * with sub_0806D128(3) and deletes on a hit. */
typedef struct {
    uint8_t gate;           /* +0xB: 0 waiting, 1 armed */
    uint8_t timer;          /* +0xD: reload 0x20..0x9F */
    int spawn_fx;           /* sub_08058D7C child this frame */
    bool deleted;           /* EntityDelete ran (load fail) */
} AosObject30;

/* Object30Create; `deleted` reports the native load-failure
 * path (the triple is shared with special_47). */
void aos_object30_create(AosObject30 *o, bool *deleted);

/* Object30Update while + 0xA == 0. `global_flags` is the
 * gEwramData + 0x42C word (bit 0x1000 arms the emitter) and
 * `random` the engine RNG. */
void aos_object30_update(AosObject30 *o, uint32_t global_flags,
                         AosRandom random);
#endif
