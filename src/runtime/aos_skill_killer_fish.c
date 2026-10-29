/* SPDX-License-Identifier: GPL-3.0-only */
#include "aos_skill_killer_fish.h"

#include "aos_audio.h"

#include <limits.h>

enum { KILLER_FISH_SPEED_LIMIT = 0x30000 };

static int32_t whole_position(int32_t position, int delta) {
    return (int32_t)((uint32_t)(int16_t)((position >> 16) + delta) << 16);
}

AosKillerFishSoul aos_killer_fish_soul_create(int32_t soma_x, int32_t soma_y,
                                               bool soma_facing_left) {
    return (AosKillerFishSoul){
        .x = whole_position(soma_x, soma_facing_left ? -20 : 20),
        .y = whole_position(soma_y, -12),
        .vx = soma_facing_left ? -0x28000 : 0x28000,
        .facing_left = !soma_facing_left,
    };
}

static int collision_kind(const AosCollision *layer, int x, int y) {
    uint8_t value = aos_collision_cell(layer, x, y);
    if (!value) return (aos_collision_cell(layer, x, y + 1) & 3) ? 0 : 1;
    return (value & 8) ? 3 : 2;
}

static int water_depth(const AosCollision *layer, int x, int y) {
    for (int depth = 0; depth < 8; ++depth)
        if (collision_kind(layer, x, y + depth) >= 2) return depth;
    return 8;
}

static void move_axis(int32_t *position, int32_t *speed, int32_t acceleration,
                      int32_t limit) {
    *speed += acceleration;
    if (acceleration >= 0) {
        if (*speed > limit) *speed = limit;
    } else if (*speed < -limit) {
        *speed = -limit;
    }
    *position += *speed;
}

/* sub_08069CF4 with half-width 8, head -7, mode 1 and speed cap 3.0.
 * The original reads the current entity collision offsets; this skill's
 * setup leaves their centre at the entity position. */
static int swim_step(AosKillerFishSoul *soul, const AosCollision *layer) {
    int result = 0;
    move_axis(&soul->x, &soul->vx, soul->ax, KILLER_FISH_SPEED_LIMIT);
    int x = soul->x >> 16, y = soul->y >> 16;
    if (soul->vx < 0) {
        int push = aos_wall_push_right(layer, x - 8, y);
        if (push) {
            soul->x += (int32_t)((uint32_t)push << 16);
            result |= 2;
        }
    } else if (soul->vx > 0) {
        int push = aos_wall_push_left(layer, x + 8, y);
        if (push) {
            soul->x += (int32_t)((uint32_t)push << 16);
            result |= 1;
        }
    }

    move_axis(&soul->y, &soul->vy, soul->ay, KILLER_FISH_SPEED_LIMIT);
    x = soul->x >> 16;
    y = soul->y >> 16;
    if (soul->vy < 0) {
        int depth = aos_ceiling_depth(layer, x, y - 7, 0, false);
        if (depth) {
            soul->y += (int32_t)((uint32_t)depth << 16);
            result |= 8;
        }
    } else if (soul->vy > 0) {
        int depth = aos_floor_depth(layer, x, y, 0, false);
        if (depth) {
            soul->y += (int32_t)((uint32_t)depth << 16);
            result |= 4;
        }
    }
    x = soul->x >> 16;
    y = soul->y >> 16;
    if (collision_kind(layer, x, y - 7) == 3) result |= 0x80;
    if (collision_kind(layer, x, y) == 3) result |= 0x40;
    return result;
}

static const AosKillerFishTarget *nearest_target(const AosKillerFishSoul *soul,
                                                  const AosKillerFishTarget *targets,
                                                  size_t count) {
    const AosKillerFishTarget *nearest = NULL;
    uint64_t nearest_distance = UINT64_MAX;
    int32_t x = soul->x >> 16, y = soul->y >> 16;
    for (size_t index = 0; targets && index < count; ++index) {
        if (!targets[index].active) continue;
        int64_t dx = (targets[index].x >> 16) - x;
        int64_t dy = (targets[index].y >> 16) - y;
        uint64_t distance = (uint64_t)(dx * dx + dy * dy);
        if (distance < nearest_distance) {
            nearest = &targets[index];
            nearest_distance = distance;
        }
    }
    return nearest;
}

static bool outside(const AosKillerFishSoul *soul, int cam_x, int cam_y,
                    int margin_x, int margin_y) {
    int x = (soul->x >> 16) - cam_x;
    int y = (soul->y >> 16) - cam_y;
    return x < -margin_x || x > margin_x + 0xF0 ||
           y < -margin_y || y > margin_y + 0xA0;
}

bool aos_killer_fish_soul_update(AosKillerFishSoul *soul,
                                 const AosCollision *layer,
                                 const AosKillerFishTarget *targets,
                                 size_t target_count, bool appearance_ended,
                                 uint32_t (*random)(void), int cam_x, int cam_y,
                                 int margin_x, int margin_y,
                                 AosKillerFishEffect *effect) {
    if (!soul || !effect || soul->state == AOS_KILLER_FISH_ENDING) return false;
    *effect = (AosKillerFishEffect){0};
    if (soul->state == 0) {
        soul->state = AOS_KILLER_FISH_APPEARING;
        aos_audio_play_song(0x19C);
    }
    if (soul->state == AOS_KILLER_FISH_APPEARING && appearance_ended) {
        int x = soul->x >> 16, y = soul->y >> 16;
        bool in_water = aos_collision_special(layer, x, y) ||
                        aos_collision_special(layer, x, y - 16);
        if (!in_water) {
            soul->vx /= 2;
            soul->vy = -0x38000;
            soul->ay = 0x8000;
            soul->vertical_flip = true;
            soul->state = AOS_KILLER_FISH_AIRBORNE;
            soul->timer = 1;
            aos_audio_play_song(0x1AD);
            aos_audio_play_song(0xDB);
        } else {
            const AosKillerFishTarget *target = nearest_target(soul, targets, target_count);
            if (target) {
                int32_t dx = target->x - soul->x;
                if (dx > 0x200000 || dx < -0x200000) {
                    soul->vx = dx / 128;
                    soul->vy = (target->y - soul->y) / 128;
                }
            }
            soul->state = AOS_KILLER_FISH_SWIMMING;
            aos_audio_play_song(0xDA);
        }
    } else if (soul->state == AOS_KILLER_FISH_AIRBORNE) {
        move_axis(&soul->x, &soul->vx, soul->ax, 0x80000);
        move_axis(&soul->y, &soul->vy, soul->ay, 0x80000);
        soul->timer++;
        if (!(soul->timer & 3) && random) {
            effect->spawn = true;
            effect->x = soul->x + (int32_t)(random() & 0x7FFFF) - 0x40000;
            effect->y = soul->y + (int32_t)(random() & 0x7FFFF) - 0x40000;
        }
    } else if (soul->state == AOS_KILLER_FISH_SWIMMING) {
        int result = swim_step(soul, layer);
        if (result & 3) {
            effect->spawn = true;
            effect->x = soul->x;
            effect->y = soul->y;
            soul->state = AOS_KILLER_FISH_ENDING;
        } else if (!(result & 0x80)) {
            int depth = water_depth(layer, soul->x >> 16, (soul->y >> 16) - 7);
            soul->y += (int32_t)((uint32_t)depth << 16);
            soul->vy = 0;
            soul->ay = 0x400;
        }
    }
    if (outside(soul, cam_x, cam_y, margin_x, margin_y))
        soul->state = AOS_KILLER_FISH_ENDING;
    if (soul->state == AOS_KILLER_FISH_ENDING) soul->hidden = true;
    return true;
}
