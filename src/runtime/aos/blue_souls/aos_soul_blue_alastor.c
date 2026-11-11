/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_blue_alastor.h"

#include "../math/aos_math.h"

#include <stddef.h>

bool aos_alastor_guardian_update(AosAlastorGuardian *ala, AosGuardian *guardian,
                                 AosStats *stats,
                                 const AosBlueSoulRecord records[AOS_BLUE_SOUL_RECORDS],
                                 uint16_t held, uint16_t pressed,
                                 uint32_t *soma_flags, int32_t src_x, int32_t src_y,
                                 int32_t aim_x, int32_t aim_y,
                                 bool soma_facing_left,
                                 const AosSkillTarget targets[AOS_ALASTOR_TARGETS],
                                 uint32_t (*random)(void)) {
    ala->song_19e = false;
    /* sub_0802DF2C runs only while the state is below 6; a drain
     * enters state 6 (the shrink-out), not an immediate delete. */
    if (ala->state < 6 &&
        aos_guardian_upkeep(guardian, stats, records, held, pressed))
        ala->state = 6;
    switch (ala->state) {
    case 0: {
        /* The sub_0803E654 variant roll always passes its
         * zero-extended sign check; + 0x58 bits 0-4 take roll & 0x1F. */
        uint16_t roll = (uint16_t)(random() & 0xFFFF);
        (void)roll; /* the + 0x58 palette bits have no runtime reader */
        /* The cast copies the source's + 0x42/+ 0x46 pixel halves with
         * y - 0x10 (the aim point's - 0x28 is a different offset). */
        ala->x = src_x;
        ala->y = src_y - (0x10 << 16);
        /* + 0x2E sign reads the copied (pre-inversion) bit 6; the
         * sprite bit then inverts it. */
        ala->vel = soma_facing_left ? -0x100 : 0x100;
        ala->facing_left = !soma_facing_left;
        ala->state = 1;
        ala->song_19e = true;
        break;
    }
    case 1:
        ala->angle = (int16_t)(ala->angle + ala->vel);
        if (ala->scale <= 0xFFFF) {
            ala->scale += 0x1000;
        } else {
            ala->state = 2;
        }
        break;
    case 2:
        ala->attacking = true;
        if ((random() & 0x7F) == 0) {
            ala->state = 4;
            break;
        }
        /* sub_0806E43C(aim, 5): homing zeroes + 0x48/+ 0x4C (the move
         * itself travels 0 px natively). */
        ala->vx = ala->vy = 0;
        ala->tick++;
        /* sin(+ 0xD * 0x200) >> 9 with the native round-away add. */
        int32_t wobble = aos_sine((uint16_t)(ala->tick << 9));
        if (wobble < 0) wobble += 0x1FF;
        ala->angle = (int16_t)(ala->angle + (wobble >> 9));
        ala->state = 3;
        break;
    case 3:
        ala->attacking = true;
        if ((random() & 0x7F) <= 1) {
            ala->state = 4;
            break;
        }
        ala->tick++;
        /* + 0x4C = sin(+ 0xD * 0x100 + 0x4000) >> 2 (rounding add 3
         * on negatives); + 0x48 decays by 63/64; the shared
         * sub_0806D3D8 integrates both under the 0x20000 cap. */
        int32_t vy = aos_sine((uint16_t)((ala->tick << 8) + 0x4000));
        if (vy < 0) vy += 3;
        ala->vy = vy >> 2;
        ala->vx = ala->vx * 63 / 64;
        if (ala->vx > 0x20000) ala->vx = 0x20000;
        if (ala->vx < -0x20000) ala->vx = -0x20000;
        if (ala->vy > 0x20000) ala->vy = 0x20000;
        if (ala->vy < -0x20000) ala->vy = -0x20000;
        ala->x += ala->vx;
        ala->y += ala->vy;
        /* sub_0806E3F8's 16.16 distance to the aim point; past 48 px
         * it re-aims through state 2. The native Sqrt yields the
         * distance in 16.16 — compared against 0x300000 here by
         * integer pixels, which is exact for whole-pixel offsets and
         * within one pixel otherwise (the aim is a pixel position). */
        {
            int64_t px = (aim_x - ala->x) >> 16, py = (aim_y - ala->y) >> 16;
            if (px * px + py * py > 48 * 48) ala->state = 2;
        }
        break;
    case 4: {
        /* sub_0802D484: nearest of the four + 0x13194 slots by
         * squared pixel distance (+ 0x42/+ 0x46 halves). */
        int best = -1;
        int64_t best_d2 = -1;
        for (int k = 0; k < AOS_ALASTOR_TARGETS; ++k) {
            const AosSkillTarget *t = &targets[k];
            if (!t->present || t->hp <= 0) continue;
            int64_t ddx = (t->x >> 16) - (ala->x >> 16);
            int64_t ddy = (t->y >> 16) - (ala->y >> 16);
            int64_t d2 = ddx * ddx + ddy * ddy;
            if (best < 0 || d2 < best_d2) {
                best = k;
                best_d2 = d2;
            }
        }
        if (best < 0) {
            ala->state = 2;
            ala->tick = 0x80;
            ala->target = -1;
            break;
        }
        ala->target = (int8_t)best;
        ala->offset = -(int32_t)(random() & 0xF) << 16;
        ala->state = 5;
        break;
    }
    case 5: {
        ala->attacking = false;
        const AosSkillTarget *t = ala->target >= 0 ? &targets[ala->target] : NULL;
        if (!t || !t->present || t->hp <= 0) {
            ala->state = 2;
            break;
        }
        /* + 0xD counts down first; the frame it reaches zero skips the
         * homing and the angle gain. */
        ala->tick--;
        if (ala->tick == 0) {
            ala->state = 2;
            break;
        }
        /* sub_0806E43C toward (tx, ty + offset): zeroes the velocities
         * natively, then + 0x2C += + 0x48 / 64 + + 0x2E * 4. */
        ala->vx = ala->vy = 0;
        int32_t turn = (ala->vx >= 0 ? (ala->vx + 0x3F) : ala->vx) >> 6;
        ala->angle = (int16_t)(ala->angle + turn + (ala->vel << 2));
        ala->state = 2;
        break;
    }
    case 6:
        /* The shrink-out: fast spin while + 0x28 drains by 0x1000;
         * at zero the native runs sub_0802DD84 and EntityDelete. */
        ala->angle = (int16_t)(ala->angle + (ala->vel << 4));
        /* The native compares + 0x28 signed (bgt): a sub-0x1000
         * remainder wraps negative and deletes on the next check. */
        if ((int32_t)ala->scale > 0) {
            ala->scale -= 0x1000;
        } else {
            ala->dead = true;
            aos_guardian_end(guardian, soma_flags);
            return false;
        }
        break;
    }
    /* The state-5 rotating contact box is unreachable natively (see
     * the header): state 5 always ends in state 2 on this update. */
    return true;
}
