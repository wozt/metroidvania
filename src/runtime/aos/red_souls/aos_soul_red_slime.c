/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_soul_red_slime.h"

#include "../../aos/math/aos_math.h"
#include "../../aos/core/aos_collision.h"

AosSlime aos_slime_create(int32_t soma_x, int32_t soma_y, bool soma_facing_left,
                          uint32_t random_draw) {
    AosSlime slime = {.speed = 0x300000, .life = 0x100, .state = 0};
    slime.body.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) +
                                                   (soma_facing_left ? -0xC : 0xC)) << 16);
    slime.body.y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16);
    slime.body.mirrored = soma_facing_left;
    uint32_t angle = (random_draw & 0x3FF) + 0x1000;
    if (soma_facing_left) angle = (uint16_t)(-0x8000 - angle);
    slime.angle = (uint16_t)angle;
    return slime;
}

bool aos_slime_update(AosSlime *slime, const AosCollision *layer, bool *variant) {
    if (variant) *variant = false;
    if (slime->state == 0) slime->state = 1;
    if (slime->state == 2) return false;
    int32_t scaled = slime->speed >> 8;
    slime->body.vx = (aos_sine((int32_t)slime->angle + 0x4000) >> 8) * scaled;
    slime->body.vy = (aos_sine((int32_t)slime->angle) >> 8) * scaled;
    /* sub_0802CE20: integrate, then snap on the floor/ceiling probes the
     * native + 0x56 = 4 offset uses; the slide bookkeeping is folded. */
    slime->body.x += slime->body.vx;
    slime->body.y += slime->body.vy;
    int32_t px = slime->body.x >> 16, py = slime->body.y >> 16;
    if (slime->body.vy > 0) {
        int depth = aos_floor_depth(layer, px, py + 4, 0, false);
        if (depth) {
            slime->body.y += (int32_t)((uint32_t)depth << 16);
            slime->body.vy = 0;
        }
    } else if (slime->body.vy < 0) {
        int depth = aos_ceiling_depth(layer, px, py - 4, 0, false);
        if (depth) {
            slime->body.y += (int32_t)((uint32_t)depth << 16);
            slime->body.vy = 0;
        }
    }
    if (slime->life == 0) {
        slime->state = 2;
        return false;
    }
    --slime->life;
    if (!slime->swapped && slime->life == 0x20) {
        slime->swapped = 1;
        if (variant) *variant = true;
    }
    return true;
}

void aos_slime_trail_push(AosSlimeTrail *trail, int32_t x, int32_t y) {
    trail->hx[trail->head] = (int16_t)(x >> 16);
    trail->hy[trail->head] = (int16_t)(y >> 16);
    trail->head = (uint8_t)((trail->head + 1) % 9);
    if (trail->head == 0) trail->filled = true;
}

void aos_slime_trail_pos(const AosSlimeTrail *trail, int32_t *x, int32_t *y) {
    int idx = trail->filled ? trail->head : 0;
    *x = (int32_t)((uint32_t)(uint16_t)trail->hx[idx] << 16);
    *y = (int32_t)((uint32_t)(uint16_t)trail->hy[idx] << 16);
}

bool aos_slime_trail_update(AosSlimeTrail *trail, bool parent_alive) {
    (void)trail;
    return parent_alive;
}
