/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef AOS_SKILL_KILLER_FISH_H
#define AOS_SKILL_KILLER_FISH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aos_collision.h"

enum {
    AOS_KILLER_FISH_APPEARING = 1,
    AOS_KILLER_FISH_AIRBORNE = 2,
    AOS_KILLER_FISH_SWIMMING = 3,
    AOS_KILLER_FISH_ENDING = 4,
};

typedef struct {
    bool active;
    int32_t x, y;
} AosKillerFishTarget;

typedef struct {
    bool spawn;
    int32_t x, y;
} AosKillerFishEffect;

/* SkillKillerFishUse, 0x08027C58; red soul 7. */
typedef struct {
    int32_t x, y;       /* +0x40, +0x44, 16.16 room pixels */
    int32_t vx, vy;     /* +0x48, +0x4C */
    int32_t ax, ay;     /* +0x50, +0x54 */
    uint8_t state;      /* +0x0A */
    uint8_t timer;      /* +0x0D */
    bool facing_left;   /* +0x58 bit 6 */
    bool vertical_flip; /* +0x58 bit 7, set by the out-of-water branch */
    bool hidden;        /* +0x58 bit 5 during the one-frame ending */
} AosKillerFishSoul;

/* State 0 places animation 1 twenty pixels ahead and twelve pixels above
 * Soma. Its sprite faces back toward Soma while it starts at 2.5 pixels per
 * frame in Soma's facing direction. */
AosKillerFishSoul aos_killer_fish_soul_create(int32_t soma_x, int32_t soma_y,
                                               bool soma_facing_left);

/* SkillKillerFishUse after creation. `appearance_ended` is +0x6C bit 2 from
 * animation 1. The transition probes bit-3 collision at the fish and sixteen
 * pixels above it: outside water it jumps and falls; in water it aims once at
 * the nearest homing-list target and swims with the native three-pixel speed
 * cap. Collisions and every fourth airborne update return the animation-7
 * sub_08045CEC effect. The ending remains alive but hidden for one update,
 * matching state 4's deletion on the following frame. */
bool aos_killer_fish_soul_update(AosKillerFishSoul *soul,
                                 const AosCollision *layer,
                                 const AosKillerFishTarget *targets,
                                 size_t target_count, bool appearance_ended,
                                 uint32_t (*random)(void), int cam_x, int cam_y,
                                 int margin_x, int margin_y,
                                 AosKillerFishEffect *effect);

#endif
