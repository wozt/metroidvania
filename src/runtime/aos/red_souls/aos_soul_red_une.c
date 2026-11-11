/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_une.h"

AosUne aos_une_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    AosUne une = {.state = 0, .timer = 0, .speed = 0xA0000, .vy = -0x18000,
                  .shot1 = false, .shot2 = false};
    une.body.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) +
                                                (soma_facing_left ? -8 : 8)) << 16);
    une.body.y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16);
    une.body.mirrored = soma_facing_left;
    return une;
}

bool aos_une_update(AosUne *une, const AosCollision *layer,
                    const AosProbes *probes, bool hit, bool outside) {
    /* State 4 is the native EntityDelete case: entering it (hit flag or
     * margin probe) survives the frame, the next update deletes. */
    if (une->state == 4) return false;
    int walk = 0;
    /* The shared entity physics (sub_0806D430) integrates + 0x4C with
     * the + 0x54 gravity every frame; the floor snap mirrors the
     * sub_0802393C cell scan used by the other walkers. */
    une->vy += 0x2000;
    une->body.y += une->vy;
    {
        int px = une->body.x >> 16, py = une->body.y >> 16;
        for (int d = 0; d < 0x20; ++d)
            if (aos_floor_depth(layer, px, py + d, 0, false)) {
                une->body.y = (int32_t)((uint32_t)(int16_t)(py + d) << 16);
                une->vy = 0;
                break;
            }
    }
    if (une->state == 0) {
        une->state = 1;
    } else if (une->state == 1) {
        /* sub_0806CAF8(entity, -+ 0x24, probes, 0): walls negate and
         * halve the speed (the native rounding drops the low bit). */
        walk = aos_enemy_probe_walk(&une->body, layer, probes,
                                    -une->speed, 0);
        if (walk & 3) une->speed = -(une->speed >> 1);
        if (walk & 8) {
            une->shot1 = true;      /* the table-driven shot is not ported */
            une->state = 2;
            une->timer = 0x80;
        }
    } else if (une->state == 2) {
        /* + 0x0D counts down; + 0x18 (not traced) short-circuits the wait. */
        if (une->timer == 0 || --une->timer == 0) {
            une->state = 3;
            une->shot2 = true;
        }
    } else if (une->state == 3) {
        /* The native re-reads the shot table and calls sub_08042848 every
         * frame here; only the + 0x6C bit-4 hit flag ends it. */
        if (hit) une->state = 4;
    }
    /* The tail: margin probe or + 0x59 bit 4 forces state 4. */
    if (une->state != 4 && outside) une->state = 4;
    return true;
}
