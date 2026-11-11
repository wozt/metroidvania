/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_biphron.h"

AosBiphron aos_biphron_create(int32_t soma_x, int32_t soma_y,
                              bool soma_facing_left) {
    AosBiphron biphron = {.speed = 0x8000, .timer = 0x40};
    biphron.body.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) +
                                                     (soma_facing_left ? -0x14 : 0x14)) << 16);
    biphron.body.y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0xA) << 16);
    biphron.body.mirrored = soma_facing_left;
    return biphron;
}

bool aos_biphron_ground_ok(const AosBiphron *biphron, const AosCollision *layer) {
    int32_t px = biphron->body.x >> 16, py = biphron->body.y >> 16;
    for (int d = 8; d <= 0x28; d += 8)
        if (aos_floor_depth(layer, px, py + d, 0, false)) return true;
    return false;
}

void aos_biphron_land(AosBiphron *biphron, const AosCollision *layer,
                      const AosProbes *probes) {
    /* sub_0802393C: drop onto the floor cell the probes reach. */
    int32_t px = biphron->body.x >> 16, py = biphron->body.y >> 16;
    for (int d = 0; d < 0x30; ++d) {
        if (aos_floor_depth(layer, px, py + d, 0, false)) {
            biphron->body.y = (int32_t)((uint32_t)(int16_t)(py + d) << 16);
            (void)aos_enemy_probe_walk(&biphron->body, layer, probes, -0x4000, 4);
            return;
        }
    }
}

bool aos_biphron_update(AosBiphron *biphron, const AosCollision *layer,
                        const AosProbes *probes, bool *totem) {
    if (totem) *totem = false;
    if (biphron->spawned) return false;
    if (biphron->speed < 0x1FFFF) biphron->speed += 0x1000;
    int result = aos_enemy_probe_walk(&biphron->body, layer, probes,
                                      -biphron->speed, 4);
    if (result & 3) biphron->body.mirrored = !biphron->body.mirrored;
    /* + 0x0D & 7 == 0 asks for a totem (including the delete frame);
     * + 0xD == 0 deletes the entity, else it counts down. */
    if ((biphron->timer & 7) == 0 && totem) *totem = true;
    if (biphron->timer == 0) {
        biphron->spawned = true;
        return false;
    }
    --biphron->timer;
    return true;
}
