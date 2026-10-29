/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_throw.h"

#include "aos_audio.h"

enum { AOS_THROW_LIMIT = 0x80000 };

bool aos_throw_supported(const AosThrowRecord *record) {
    return !(record->flags & ~AOS_THROW_PORTED_FLAGS);
}

AosThrow aos_throw_create(int32_t soma_x, int32_t soma_y, bool facing_left) {
    return (AosThrow){
        .x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) + (facing_left ? -8 : 8)) << 16),
        .y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 0x1A) << 16),
        .facing_left = facing_left};
}

/* sub_0806D430 / sub_0806D460 for one axis with no acceleration sign
 * flip: the speed gains the acceleration, is capped at the limit on the
 * acceleration's side, and moves the position. */
static void move_axis(int32_t *position, int32_t *speed, int32_t acceleration) {
    *speed += acceleration;
    if (acceleration >= 0) {
        if (*speed > AOS_THROW_LIMIT) *speed = AOS_THROW_LIMIT;
    } else if (*speed < -AOS_THROW_LIMIT) {
        *speed = -AOS_THROW_LIMIT;
    }
    *position += *speed;
}

bool aos_throw_update(AosThrow *entity, const AosThrowRecord *record, int cam_x, int cam_y,
                      int mx, int my) {
    return aos_throw_update_world(entity, record, NULL, NULL, cam_x, cam_y, mx, my);
}

/* Flag 4: sub_0806CAF8(e, -(+ 0x24), 0x084F1388, 0) and its landings. */
static void roll(AosThrow *entity, const AosCollision *layer, const AosProbes *probes) {
    int32_t fall = entity->vy;
    AosEnemy *body = &entity->body;
    body->x = entity->x;
    body->y = entity->y;
    body->vy = entity->vy;
    body->ay = entity->gravity;
    body->ax = 0;
    body->mirrored = entity->facing_left;
    int result = aos_enemy_probe_walk(body, layer, probes, -entity->roll_speed, 0);
    entity->x = body->x;
    entity->y = body->y;
    entity->vx = body->vx;
    entity->vy = body->vy;
    if (result & 3) entity->facing_left = !entity->facing_left;
    if (!(result & 8)) return;
    if (fall > 0x10000) {
        aos_audio_play_song(0x11B);
        entity->vy = -(fall / 2);
    } else {
        entity->roll_speed = 0;     /* r6 = flags & 2 */
    }
}

/* Flag 2: the same sub_0806CAF8 step, but wall contact stops the stored
 * speed instead of changing facing. Rock Armor (flag 0x10) bursts on its
 * first landing; Waiter Skeleton (flag 0x400) rests there until timeout. */
static void terrain(AosThrow *entity, const AosThrowRecord *record,
                    const AosCollision *layer, const AosProbes *probes) {
    AosEnemy *body = &entity->body;
    body->x = entity->x;
    body->y = entity->y;
    body->vy = entity->vy;
    body->ay = entity->gravity;
    body->ax = 0;
    body->mirrored = entity->facing_left;
    int result = aos_enemy_probe_walk(body, layer, probes, -entity->roll_speed, 0);
    entity->x = body->x;
    entity->y = body->y;
    entity->vx = body->vx;
    entity->vy = body->vy;
    if (result & 3) entity->roll_speed = 0;
    if (!(result & 8)) return;
    if (record->flags & AOS_THROW_LANDING_BURST) {
        entity->landing_burst = true;
        entity->state = 3;
        aos_audio_play_song(0xD3);
    }
    if (record->flags & AOS_THROW_TIMEOUT) entity->roll_speed = 0;
}

bool aos_throw_update_world(AosThrow *entity, const AosThrowRecord *record,
                            const AosCollision *layer, const AosProbes *probes, int cam_x,
                            int cam_y, int mx, int my) {
    entity->exploded = false;
    entity->landing_burst = false;
    entity->timeout_dust = false;
    switch (entity->state) {
    case 0:
        /* The 0x80 path takes an affine slot (sub_0803E654); the spin is
         * negated facing left. */
        if (record->flags & AOS_THROW_SPIN)
            entity->spin = (int16_t)(entity->facing_left ? -record->spin : record->spin);
        entity->roll_speed = record->vx;
        entity->vx = entity->facing_left ? -record->vx : record->vx;
        entity->vy = record->vy;
        entity->gravity = record->gravity;
        entity->attacking = !(record->flags & 0x60);
        if (record->flags & 0x60) entity->timer = AOS_THROW_FUSE_FRAMES;
        else if (record->flags & AOS_THROW_TIMEOUT) entity->timer = AOS_THROW_TIMEOUT_FRAMES;
        entity->linked = record->flags & AOS_THROW_LINK;
        if (record->song) aos_audio_play_song(record->song);
        entity->state = 1;
        break;
    case 1:
        if ((record->flags & 0x60) && --entity->timer == 0) {
            entity->exploded = true;
            entity->attacking = true;
            entity->hidden = true;
            entity->state = 2;
            entity->timer = 2;
            aos_audio_play_song(0xD7);
        } else if ((record->flags & AOS_THROW_TIMEOUT) && --entity->timer <= 0x10) {
            if (entity->timer == 0x10) entity->blinking = true;
            if (entity->timer <= 0x0F && !(entity->timer & 1)) entity->timeout_dust = true;
            if (entity->timer == 0) entity->state = 3;
        }
        if ((record->flags & AOS_THROW_TERRAIN) && layer && probes) {
            terrain(entity, record, layer, probes);
        } else if ((record->flags & AOS_THROW_ROLL) && layer && probes) {
            roll(entity, layer, probes);
        } else {
            /* Neither terrain flag (2, 4): free flight; the x acceleration
             * (+ 0x50) stays 0. */
            move_axis(&entity->x, &entity->vx, 0);
            move_axis(&entity->y, &entity->vy, entity->gravity);
        }
        break;
    case 2:
        if (entity->timer) {
            entity->timer--;
        } else {
            entity->timer = 0x10;
            entity->state = 4;
        }
        break;
    case 4:
        if (entity->timer) entity->timer--;
        else entity->state = 3;
        break;
    case 3:
        entity->linked = false;
        return false;
    default: break;
    }
    /* + 0x59 bit 2 was set by sub_08019478, so only the margins count. */
    int sx = (entity->x >> 16) - cam_x, sy = (entity->y >> 16) - cam_y;
    if (sx < -mx || sx > mx + 0xF0 || sy < -my || sy > my + 0xA0) entity->state = 3;
    if (record->flags & AOS_THROW_SPIN) entity->angle = (uint16_t)(entity->angle + entity->spin);
    return true;
}
