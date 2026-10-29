/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_gladiator.h"

#include "aos_audio.h"

AosGladiatorSoul aos_gladiator_soul_create(int32_t soma_x, int32_t soma_y,
                                           bool facing_left) {
    AosGladiatorSoul soul = {
        .stored_vx = facing_left ? 0x20000 : -0x20000,
        .facing_left = facing_left,
    };
    soul.body.x = (int32_t)((uint32_t)(int16_t)((soma_x >> 16) +
                                                 (facing_left ? -8 : 8)) << 16);
    soul.body.y = (int32_t)((uint32_t)(int16_t)((soma_y >> 16) - 26) << 16);
    soul.body.vy = -0x28000;
    soul.body.ay = 0x2000;
    soul.body.mirrored = facing_left;
    return soul;
}

static int free_step(AosEnemy *body, int32_t speed) {
    body->vx = speed;
    body->x += body->vx;
    body->vy += body->ay;
    if (body->vy > 0x80000) body->vy = 0x80000;
    body->y += body->vy;
    return 0;
}

bool aos_gladiator_soul_update(AosGladiatorSoul *soul, const AosCollision *layer,
                               const AosProbes *probes, uint32_t (*random)(void),
                               int cam_x, int cam_y, int margin_x, int margin_y,
                               AosGladiatorDust *dust) {
    if (!soul || !dust) return false;
    *dust = (AosGladiatorDust){0};
    if (soul->state == 0) {
        soul->state = AOS_GLADIATOR_MOVING;
        aos_audio_play_song(0xC5);
    }
    if (soul->state == AOS_GLADIATOR_MOVING) {
        int32_t old_vy = soul->body.vy;
        int result = layer && probes && probes->count
                         ? aos_enemy_probe_walk(&soul->body, layer, probes,
                                                -soul->stored_vx, 2)
                         : free_step(&soul->body, -soul->stored_vx);
        bool stopped = false;
        if (result & 0x40) {
            soul->stored_vx += soul->stored_vx > 0 ? -0x1000 : 0x1000;
        } else if (result & 0x20) {
            soul->stored_vx += soul->stored_vx > 0 ? 0x1000 : -0x1000;
        } else if (soul->stored_vx >= 0) {
            soul->stored_vx -= 0x200;
            if (soul->stored_vx <= 0) {
                soul->stored_vx = 0;
                stopped = true;
            }
        } else {
            soul->stored_vx += 0x200;
            if (soul->stored_vx >= 0) {
                soul->stored_vx = 0;
                stopped = true;
            }
        }
        if (stopped) {
            soul->state = AOS_GLADIATOR_ENDING;
            soul->timer = 0x10;
            soul->blinking = true;
        }
        soul->angle = (uint16_t)(soul->angle + (-soul->stored_vx) / 64);
        if (result & 3) {
            aos_audio_play_song(0x15F);
            soul->stored_vx = (-soul->stored_vx) / 2;
        }
        if ((result & 8) && soul->body.vy == 0 && old_vy > 0)
            aos_audio_play_song(0x15F);
    } else if (soul->state == AOS_GLADIATOR_ENDING) {
        if ((soul->timer & 3) == 0 && random) {
            dust->spawn = true;
            dust->x = soul->body.x + (int32_t)(random() & 0x1FFFFF) - 0x100000;
            dust->y = soul->body.y + (int32_t)(random() & 0x1FFFFF) - 0x100000;
        }
        if (soul->timer == 0) return false;
        soul->timer--;
    } else {
        return false;
    }
    int sx = (soul->body.x >> 16) - cam_x;
    int sy = (soul->body.y >> 16) - cam_y;
    return !(sx < -margin_x || sx > margin_x + 0xF0 ||
             sy < -margin_y || sy > margin_y + 0xA0);
}
