/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_werewolf.h"

AosWerewolf aos_werewolf_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    AosWerewolf werewolf = {.state = 1};
    werewolf.body.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) +
                                                     (soma_facing_left ? -0x14 : 0x14)) << 16);
    werewolf.body.y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0xA) << 16);
    werewolf.body.mirrored = soma_facing_left;
    return werewolf;
}

bool aos_werewolf_ground_ok(const AosWerewolf *werewolf, const AosCollision *layer) {
    int32_t px = werewolf->body.x >> 16, py = werewolf->body.y >> 16;
    for (int d = 8; d <= 0x28; d += 8)
        if (aos_floor_depth(layer, px, py + d, 0, false)) return true;
    return false;
}

void aos_werewolf_land(AosWerewolf *werewolf, const AosCollision *layer,
                        const AosProbes *probes) {
    /* sub_0802393C walks + 0x46 (the y integer part) down in 8-pixel probes
     * until sub_08001D94 reports the floor cell; the walker step then puts
     * the entity on the surface. */
    int32_t px = werewolf->body.x >> 16, py = werewolf->body.y >> 16;
    for (int d = 0; d < 0x30; ++d) {
        if (aos_floor_depth(layer, px, py + d, 0, false)) {
            werewolf->body.y = (int32_t)((uint32_t)(int16_t)(py + d) << 16);
            (void)aos_enemy_probe_walk(&werewolf->body, layer, probes, -0x4000, 4);
            return;
        }
    }
}

void aos_werewolf_update(AosWerewolf *werewolf, const AosCollision *layer,
                          const AosProbes *probes) {
    (void)aos_enemy_probe_walk(&werewolf->body, layer, probes, -0x4000, 4);
}
