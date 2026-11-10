/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Ghost (enemy 0x0E) and its controller.
 *
 * This family is a separate translation unit and uses only the prefixed
 * internal enemy-framework API. */

#include "../../aos/core/aos_enemy_internal.h"

/* sub_08089E64: after animation 0 has appeared, animation 1 attacks while
 * the Ghost accelerates toward Soma at a random height. Near that target it
 * receives a two-pixel impulse away from a point 64 pixels below Soma. */
static void ghost_controller(AosEnemy *g, const AosSoma *soma, const AosEnemyKind *kind,
                             uint32_t (*random)(void)) {
    if (g->step == 0) {
        if (!g->cycle_done) return;
        g->step = 1;
        aos_enemy_play(g, kind->anims, 1);
        g->combat.attack_off = g->combat.hurt_off = false;
        g->speed = -(int32_t)(random() & 0xFFFFF);
        return;
    }

    aos_enemy_face_player(g, soma);
    int32_t dx = aos_enemy_target_x(soma) - g->x;
    int32_t dy = aos_enemy_target_y(soma) - g->y + g->speed -
                 ((dx < 0 ? -dx : dx) <= 0x5FFFFF ? 0x100000 : 0x200000);
    g->ax = dx > 0x40000 ? 0x400 : dx < -0x40000 ? -0x400 : 0;
    g->ay = dy > 0x40000 ? 0x400 : dy < -0x40000 ? -0x400 : 0;
    g->vx = g->vx * 31 / 32;
    g->vy = g->vy * 31 / 32;
    g->vx += g->ax;
    g->vy += g->ay;
    if (g->ax >= 0 ? g->vx > 0x400000 : g->vx < -0x400000)
        g->vx = g->ax >= 0 ? 0x400000 : -0x400000;
    if (g->ay >= 0 ? g->vy > 0x400000 : g->vy < -0x400000)
        g->vy = g->ay >= 0 ? 0x400000 : -0x400000;
    g->x += g->vx;
    g->y += g->vy;

    if ((dx < 0 ? -dx : dx) > 0x3FFFF || (dy < 0 ? -dy : dy) > 0x3FFFF) return;
    int16_t away_x = (int16_t)aos_enemy_pixel_delta(
        g->x, aos_enemy_target_x(soma));
    int16_t away_y = (int16_t)aos_enemy_pixel_delta(
        g->y + 0x400000, aos_enemy_target_y(soma));
    uint16_t angle = aos_arctan2(away_x, away_y);
    angle = (uint16_t)(angle - 0x1000 + (random() & 0x1FFF));
    g->vx = aos_fixed_mul(aos_sine(angle + 0x4000u), 0x20000);
    g->vy = aos_fixed_mul(aos_sine(angle), 0x20000);
}

/* EnemyGhostUpdate: the normal attack/death states, the event-state variant
 * and state 3's hidden room producer. The producer uses raw 0..239 and
 * 16..191 room coordinates exactly as the native routine does. */
AosHitReport aos_enemy_ghost_update(
    AosEnemy *g, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (g->state == 3) {
        if (g->producer_timer++ < (uint16_t)g->param1 || (random() & 0x1F)) return report;
        g->producer_timer = 0;
        if (g->spawned_count >= g->param0) return report;
        int x = 0, y = 0;
        bool found_x = false, found_y = false;
        for (int attempt = 0; attempt < 256 && !found_x; ++attempt) {
            x = (int)(random() % 0xF0);
            int32_t distance = soma->x - AOS_FIXED(x);
            found_x = (distance < 0 ? -distance : distance) > 0x300000;
        }
        for (int attempt = 0; attempt < 256 && !found_y; ++attempt) {
            y = (int)(random() % 0xB0) + 0x10;
            int32_t distance = soma->y - AOS_FIXED(y);
            found_y = (distance < 0 ? -distance : distance) > 0x300000;
        }
        if (!found_x || !found_y) return report;
        g->spawned_count++;
        report.spawn = true;
        report.spawn_x = x;
        report.spawn_y = y;
        return report;
    }
    if (g->defeated || g->state == 1) {
        aos_enemy_generic_death(g, kind, random, &report);
        aos_combat_tick(&g->combat);
        return report;
    }
    if (g->state == 2) {
        if (g->step == 0 && g->cycle_done) {
            g->step = 1;
            aos_enemy_play(g, kind->anims, 1);
        }
        aos_enemy_step_anim(g, kind->anims);
        return report;
    }

    int sx = (g->x >> 16) - cam_x, sy = (g->y >> 16) - cam_y;
    if (aos_enemy_outside_margins(kind, 4, sx, sy)) return report;
    g->hidden = false;
    ghost_controller(g, soma, kind, random);
    aos_enemy_step_anim(g, kind->anims);
    sx = (g->x >> 16) - cam_x;
    sy = (g->y >> 16) - cam_y;
    g->hidden = aos_enemy_outside_margins(kind, 4, sx, sy);
    if ((uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0) {
        AosHitReport hit = aos_enemy_collide(
            g, soma, kind, weapon, weapon_frames, soma_atk, soma_def);
        aos_enemy_merge_hit(&report, hit);
    }
    aos_combat_tick(&g->combat);
    return report;
}
