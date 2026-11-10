/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Gargoyle (enemy 0x57) and Gremlin (0x37, the same code).
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../../aos/core/aos_enemy_internal.h"
#include "../../aos/core/aos_audio.h"

#define accel_move aos_enemy_accel_move
#define collide aos_enemy_collide
#define generic_death aos_enemy_generic_death
#define play aos_enemy_play
#define play_loop aos_enemy_play_loop
#define soldier_face aos_enemy_soldier_face
#define step_anim aos_enemy_step_anim
#define take_effects aos_enemy_take_effects
#define target_x aos_enemy_target_x
#define target_y aos_enemy_target_y

/* The gargoyle's pursuit of one axis: beyond 8 pixels the speed moves
 * toward the target by + 0x50, up to + 0x54. */
static void gargoyle_chase(int32_t *v, int32_t delta, int32_t a, int32_t max) {
    if (delta < -0x80000) {
        *v -= a;
        if (*v <= -max) *v = -max;
    } else if (delta > 0x80000) {
        *v += a;
        if (*v >= max) *v = max;
    }
}

/* sub_08086534: step 0 faces the player at 0.5; step 1 chases the point
 * 64 pixels above him (+ 0x50 = 0x1000 up to 1.0 per axis) until, once its
 * timer runs out, he is within 8 pixels vertically and 96 horizontally:
 * animation 3; step 2 brakes (9/10 per frame) for 21 frames, then plays
 * animation 1 and dives (sound 0x87) with accelerations (+/- 0xC00,
 * 0x1800) clamped to 2.0 for 25 frames; step 4 doubles the X acceleration,
 * keeps 15/16 of vy for 61 frames (animation 2 one frame in 256 in steps 3
 * and 4); step 5 climbs (0x1000 up to 1.5) until level with or below that
 * point, then chases again after 20 frames. */
static void gargoyle_fly(AosEnemy *g, const AosSoma *soma, const AosAnimSet *anims,
                         uint32_t (*random)(void)) {
    int32_t dx, dy;
    switch (g->step) {
    case 0:
        soldier_face(g, soma);
        g->vx = g->mirrored ? 0x8000 : -0x8000;
        g->vy = 0;
        g->ax = 0x1000;
        g->ay = 0x10000;
        g->timer = 0;
        g->step = 1;
        /* fall through */
    case 1:
        dx = target_x(soma) - g->x;
        dy = target_y(soma) - (g->y + 0x400000);
        gargoyle_chase(&g->vx, dx, g->ax, g->ay);
        gargoyle_chase(&g->vy, dy, g->ax, g->ay);
        g->mirrored = g->vx >= 0;
        g->x += g->vx;
        g->y += g->vy;
        if ((int8_t)--g->timer > 0) return;
        g->timer = 0;
        if ((uint32_t)(dy + 0x7FFFF) > 0xFFFFE) return;
        if ((dx < 0 ? -dx : dx) > 0x5FFFFF) return;
        g->step = 2;
        play_loop(g, anims, 3, false);
        return;
    case 2:
        soldier_face(g, soma);
        g->vx = g->vx * 9 / 10;
        g->vy = g->vy * 9 / 10;
        g->x += g->vx;
        g->y += g->vy;
        if (g->timer++ <= 0x13) return;
        g->step = 3;
        g->timer = 0;
        play_loop(g, anims, 1, false);
        g->ax = g->mirrored ? 0xC00 : -0xC00;
        g->ay = 0x1800;
        aos_audio_play_song(0x87);
        return;
    case 3:
        if (!(random() & 0xFF)) play_loop(g, anims, 2, false);
        accel_move(g, 0x20000);
        if (g->timer++ <= 0x17) return;
        g->step = 4;
        g->timer = 0;
        g->ax *= 2;
        return;
    case 4:
        if (!(random() & 0xFF)) play_loop(g, anims, 2, false);
        /* sub_0806D430(e, 2.0), sub_0806D4A8(e, 16), sub_0806D3CC */
        g->vx += g->ax;
        if (g->ax >= 0 ? g->vx > 0x20000 : g->vx < -0x20000) g->vx = g->ax >= 0 ? 0x20000 : -0x20000;
        g->x += g->vx;
        g->vy = g->vy * 15 / 16;
        g->y += g->vy;
        if (g->timer++ <= 0x3B) return;
        g->step = 5;
        g->timer = 0;
        g->ax = 0x1000;
        g->ay = 0x18000;
        play(g, anims, 0);
        return;
    case 5:
        dx = target_x(soma) - g->x;
        dy = target_y(soma) - (g->y + 0x400000);
        gargoyle_chase(&g->vx, dx, g->ax, g->ay);
        if (dy < 0) {
            g->vy -= g->ax;
            if (g->vy <= -g->ay) g->vy = -g->ay;
        } else {
            g->step = 1;
            g->timer = 0x14;
            g->ax = 0x1000;
            g->ay = 0x10000;
        }
        g->mirrored = g->vx >= 0;
        g->x += g->vx;
        g->y += g->vy;
        return;
    default:
        return;
    }
}

/* EnemyGargoyleUpdate: inside the activity window, state 0 flies, state 1
 * dies (9/10 of vx, a fall with 0x1000 clamped to 3.0, the generic death),
 * state 2 only steps its animation; then the animation step and the
 * collision pass. Contact sub_08086964: type 0, element 0. */
AosHitReport aos_enemy_gargoyle_update(AosEnemy *g, AosSoma *soma, const AosEnemyKind *kind,
                                    const AosWeaponEntity *weapon,
                                    const AosWeaponFrames *weapon_frames, int soma_atk,
                                    int soma_def, int cam_x, int cam_y,
                                    uint32_t (*random)(void)) {
    AosHitReport report = {0}, effects = {0};
    int sx = (g->x >> 16) - cam_x, sy = (g->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    switch (g->state) {
    case 0: gargoyle_fly(g, soma, kind->anims, random); break;
    case 1:
        g->vx = g->vx * 9 / 10;
        accel_move(g, 0x30000);
        generic_death(g, kind, random, &effects);
        if (g->removed) return effects;
        break;
    case 2:
        step_anim(g, kind->anims);
        return report;
    default:
        break;
    }
    step_anim(g, kind->anims);
    sx = (g->x >> 16) - cam_x;
    sy = (g->y >> 16) - cam_y;
    if (!g->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = collide(g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
    aos_combat_tick(&g->combat);
    take_effects(&report, &effects);
    return report;
}
