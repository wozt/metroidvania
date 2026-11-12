/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_blue_final_guard.h"

/* (+ 0x10 & 0x001001A2) == 0x00100000: grounded, with airborne,
 * attacking, stop-at-wall and body-special all clear. */
#define FINAL_GUARD_MASK 0x001001A2u
#define FINAL_GUARD_OK   0x00100000u

static bool grounded_ok(const AosSoma *soma) {
    return (soma->flags & FINAL_GUARD_MASK) == FINAL_GUARD_OK;
}

bool aos_final_guard_guardian_update(AosFinalGuardGuardian *guard,
                                     AosGuardian *guardian, AosStats *stats,
                                     const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                     uint16_t held, uint16_t pressed,
                                     uint32_t *soma_flags, AosSoma *soma,
                                     uint32_t (*random)(void)) {
    (void)random;
    guard->song_1a7 = false;
    guard->aux_bits = 0;
    /* sub_0802DE5C ends the held soul -> EntityDelete. */
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed)) {
        guard->dead = true;
        aos_guardian_end(guardian, soma_flags);
        return false;
    }
    if (guard->phase == 0) {
        /* The source must be idle (its + 0xA == 0) and pass the mask. */
        if (soma->state != 0 || !grounded_ok(soma)) {
            guard->dead = true;
            aos_guardian_end(guardian, soma_flags);
            return false;
        }
        /* The native writes the source's + 0xA = 0xA, requests the
         * 0x35 sequence and clears + 0x10 bits 0x400/0x1000000. */
        soma->state = 0xA;
        soma->anim_request = 0x35;
        soma->flags &= ~0x1000400u;
        guard->song_1a7 = true;
        guard->phase = 1;
        return true;
    }
    if (guard->phase == 1) {
        guard->phase = 2;
        return true;
    }
    /* State 2 on: the mask is re-checked every update. */
    if (!grounded_ok(soma)) {
        guard->dead = true;
        aos_guardian_end(guardian, soma_flags);
        return false;
    }
    /* + 0x13260 |= 0x4000 lands on the source's abilities field;
     * + 0x131B8 |= 0x820 is exposed through aux_bits. */
    soma->abilities |= 0x4000;
    guard->aux_bits = 0x820;
    /* The + 0x42/+ 0x46 pixel halves copy with y + 4. */
    guard->x = ((soma->x >> 16) << 16);
    guard->y = (((soma->y >> 16) + 4) << 16);
    guard->facing_left = soma->facing_left;
    return true;
}
