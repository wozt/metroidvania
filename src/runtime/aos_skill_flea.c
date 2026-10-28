/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_flea.h"

AosFlea aos_flea_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left) {
    AosFlea flea = {.state = 1, .timer = AOS_FLEA_FRAMES, .frame = 1};
    flea.body.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (soma_facing_left ? -8 : 8))
                            << 16);
    flea.body.y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16);
    flea.body.ay = 0x2000;
    /* The placement copies Soma's facing bit, then it is flipped: set (an
     * enemy's mirrored, walking right) when Soma faces right. */
    flea.body.mirrored = !soma_facing_left;
    return flea;
}

bool aos_flea_update(AosFlea *flea, const AosCollision *layer, const AosProbes *probes,
                     uint32_t (*random)(void), int cam_x, int cam_y, int mx, int my,
                     bool *started_end) {
    *started_end = false;
    AosEnemy *body = &flea->body;
    if (flea->state == 1) {
        int result = aos_enemy_probe_walk(body, layer, probes, 0x18000, 8);
        flea->frame = body->vy < -0x8000 ? 0 : body->vy > 0x8000 ? 2 : 1;
        if (result & 3) body->mirrored = !body->mirrored;
        if (result & 8)
            body->vy = (random() & 3) ? -0x10000 : (int32_t)(random() & 0xFFFF) - 0x50000;
        if (flea->timer == 0) {
            flea->blinking = true;
            flea->state = 2;
            flea->timer = AOS_FLEA_END_FRAMES;
            *started_end = true;
        } else {
            flea->timer--;
        }
    } else if (flea->state == 2) {
        if (!(flea->timer & 3)) {
            (void)random();
            (void)random();
        }
        if (flea->timer == 0) return false;
        flea->timer--;
    }
    int sx = (body->x >> 16) - cam_x, sy = (body->y >> 16) - cam_y;
    return !(sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0);
}
