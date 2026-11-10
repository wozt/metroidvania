/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Medusa Head (enemy 0x39) and its spawner.
 *
 * This family is a separate translation unit and uses only the prefixed
 * internal enemy-framework API. */

#include "../../aos/core/aos_enemy_internal.h"

/* sub_08093A3C: vy swings by + 0x54 (subtracted while + 0x20 is set),
 * + 0x20 flips every + 0x22 frames (+ 0x21 counts), then the move
 * (sub_0806D3AC); rising it plays animation 2, falling animation 0 or 1
 * at random. */
static void medusa_fly(AosEnemy *m, const AosAnimSet *anims, uint32_t (*random)(void)) {
    if (m->step != 0) return;
    m->vy += m->dropped ? -m->ay : m->ay;
    if (++m->volley_count >= (int8_t)m->shot) {
        m->volley_count = 0;
        m->dropped = !m->dropped;
    }
    m->x += m->vx;
    m->y += m->vy;
    if (m->vy >= 0) {
        if (m->anim.id == 2)
            aos_enemy_play(m, anims, (random() & 1) ? 0 : 1);
    } else if (m->anim.id != 2) {
        aos_enemy_play(m, anims, 2);
    }
}

/* EnemyMedusaHeadUpdate state 3: a spawner creates a head one frame in 32,
 * while fewer than parameter 0 exist (+ 0x18), at screen X 256 when Soma's
 * screen X is at most 0x67, or at most 0x88 facing right (sub_0806CEC0),
 * else at -16, and screen Y 64 + (r & 0x3F); the head takes parameter 1
 * bit 0 (gold). */
static void medusa_spawner(AosEnemy *spawner, const AosSoma *soma, int cam_x, int cam_y,
                           uint32_t (*random)(void), AosHitReport *report) {
    if (random() & 0x1F) return;
    if (spawner->spawned_count >= spawner->param0) return;
    spawner->spawned_count++;
    int px = (aos_enemy_target_x(soma) >> 16) - cam_x;
    int sx = px <= 0x67 || (px <= 0x88 && !soma->facing_left) ? 256 : -16;
    report->spawn = true;
    report->spawn_x = cam_x + sx;
    report->spawn_y = cam_y + (int32_t)(random() & 0x3F) + 0x40;
    report->spawn_param1 = (int16_t)(spawner->param1 & 1);
}

/* EnemyMedusaHeadUpdate: outside the margins of sub_0806D128(e, 4) a head
 * dies at once (state 2, timer 1); sub_0806AD24 stuns it; state 0 flies,
 * 2 is the generic death, 3 the spawner, 4 inert; then the animation step
 * and the collision pass (sub_0806E314). The contact callback sub_08093BF0
 * uses element 0x100 for gold heads (its petrification is not ported) and
 * the tracking list of sub_08021924 is not. */
AosHitReport aos_enemy_medusa_head_update(
    AosEnemy *m, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, int cam_x, int cam_y,
    uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    if (m->state != 3 &&
        aos_enemy_outside_margins(kind, 4, (m->x >> 16) - cam_x,
                                  (m->y >> 16) - cam_y)) {
        m->state = 2;
        m->timer = 1;
    }
    if (m->hp > 0 && m->hit_flash) {
        m->hit_flash--;
        if (m->state != 2)
            report = aos_enemy_collide(m, soma, kind, weapon, weapon_frames,
                                       soma_atk, soma_def);
        aos_combat_tick(&m->combat);
        return report;
    }
    switch (m->state) {
    case 0: medusa_fly(m, kind->anims, random); break;
    case 2:
        aos_enemy_generic_death(m, kind, random, &spawned);
        if (m->removed) return spawned;
        break;
    case 3:
        medusa_spawner(m, soma, cam_x, cam_y, random, &report);
        return report;
    case 4:
        aos_enemy_step_anim(m, kind->anims);
        return report;
    default:
        break;
    }
    aos_enemy_step_anim(m, kind->anims);
    int sx = (m->x >> 16) - cam_x, sy = (m->y >> 16) - cam_y;
    if (!m->defeated && (uint16_t)(sx + 0x10) <= 0x110 && (uint16_t)sy <= 0xB0)
        report = aos_enemy_collide(m, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    aos_combat_tick(&m->combat);
    aos_enemy_take_effects(&report, &spawned);
    return report;
}
