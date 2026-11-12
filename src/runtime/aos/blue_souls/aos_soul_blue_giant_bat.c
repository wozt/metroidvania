/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_blue_giant_bat.h"

#include "../math/aos_math.h"

bool aos_giant_bat_guardian_update(AosGiantBatGuardian *bat, AosGuardian *guardian,
                                   AosStats *stats,
                                   const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                   uint16_t held, uint16_t pressed,
                                   uint32_t *soma_flags, AosSoma *soma,
                                   uint32_t (*random)(void)) {
    bat->song_8f = false;
    bat->fx = false;
    /* Every update: + 0xA = 8 and gEwramData + 0x131B8 |= 3. */
    bat->aux_bits = 3;
    /* sub_0802DE5C ends the soul -> EntityDelete (no shrink-out). */
    if (aos_guardian_upkeep(guardian, stats, records, held, pressed)) {
        bat->dead = true;
        aos_guardian_end(guardian, soma_flags);
        return false;
    }
    /* The + 0x10 mask 0x010001A0 on the target also deletes. */
    if (soma->flags & 0x010001A0u) {
        bat->dead = true;
        aos_guardian_end(guardian, soma_flags);
        return false;
    }
    if (!bat->cast) {
        /* First update: clamp both velocities to +-0x40000, kill the
         * + 0x50/+ 0x54 pair, clear + 0x10 bits 0x21, play 0x8F, zero
         * + 0xB and arm the sixteen sparkle frames. */
        if (soma->vx > 0x40000) soma->vx = 0x40000;
        if (soma->vx < -0x40000) soma->vx = -0x40000;
        if (soma->vy > 0x40000) soma->vy = 0x40000;
        if (soma->vy < -0x40000) soma->vy = -0x40000;
        soma->friction = 0;
        soma->gravity_mod = 0;
        soma->flags &= ~0x21u;
        soma->transform_step = 0;
        bat->song_8f = true;
        bat->cast = true;
        bat->timer = 0x10;
    }
    /* The shared tail: + 0xA = 8 and + 0x13260 |= 0x100 every update. */
    soma->abilities |= 0x100;
    if (bat->timer) {
        bat->timer--;
        /* angle = (rng & 0xFF) << 8; a1 = angle + 0x4000, a2 = angle.
         * Positions take sin * 4 (+-4 px), velocities sin * 2. */
        uint32_t angle = (random() & 0xFF) << 8;
        int32_t s1 = aos_sine(angle + 0x4000);
        int32_t s2 = aos_sine(angle);
        bat->fx_x = soma->x + s1 * 4;
        bat->fx_y = soma->y - (0x10 << 16) + s2 * 4;
        bat->fx_vx = s1 * 2;
        bat->fx_vy = s2 * 2;
        bat->fx = true;
    }
    return true;
}
