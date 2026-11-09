/* SPDX-License-Identifier: GPL-3.0-only */
/* Aria of Sorrow enemies: Mimic (enemy 0x60) and its hidden trigger.
 *
 * This family is a separate translation unit and uses the shared internal
 * enemy framework API. */

#include "../aos_enemy_internal.h"

#include <string.h>

/* sub_080992C8: the Mimic's trigger, a hidden entity at its position with
 * the hurtbox of the bytes 0x100CF8FA (-6, -8, 12, 16), no attack box;
 * its common sprite (animation 3, row 7) and its draw routine sub_08046100
 * are not drawn. */
static AosEnemy mimic_trigger(const AosEnemy *m) {
    AosEnemy t = {0};
    t.id = m->id;
    t.role = AOS_ROLE_MIMIC_TRIGGER;
    t.static_frame = -1;
    t.x = m->x;
    t.y = m->y;
    t.hidden = true;
    t.hp = 1;
    t.combat.type = AOS_TYPE_ENEMY;
    t.combat.attack_off = true;
    t.own_boxes = true;
    t.own_hurt = t.own_attack = (AosBox){-6, -8, 12, 16};
    return t;
}

/* sub_0809937C: the trigger collides until struck; then, hidden, it sends
 * flames of sub_0806D5C0 (8 x 8) every 4 frames while its timer is above
 * 30 and is deleted after 41 frames. */
AosHitReport aos_enemy_mimic_trigger_update(
    AosEnemy *t, AosSoma *soma, const AosEnemyKind *kind,
    const AosWeaponEntity *weapon, const AosWeaponFrames *weapon_frames,
    int soma_atk, int soma_def, uint32_t (*random)(void)) {
    AosHitReport report = {0};
    if (t->state == 0) {
        report = aos_enemy_collide(t, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
        aos_combat_tick(&t->combat);
        return report;
    }
    t->hidden = true;
    if (t->timer > 0x1E && (t->timer & 3) == 0)
        aos_enemy_spawn_flame_at(t->x, t->y, 0x80000, 0x80000, random,
                                 &report);
    if (--t->timer == 0xFF) t->removed = true;
    return report;
}

/* sub_080990D8: step 0 waits hidden without collisions (its trigger made
 * on the first update); woken (step 1) it shows itself and falls as a
 * closed chest (step 2: sprite frame 0, sub_080698A8(e, 1.0, -16, 4.0)
 * with + 0x54 0x2000) until it lands; step 3 sits closed until the player
 * is in (x - 16, y - 20, 32, 20), then opens (animation 0 once, step 4)
 * and plays animation 1 (step 5), hidden beyond margin entry 4. Returns
 * whether its collisions are off this frame (+ 0x18 bit 0). */
static bool mimic_act(AosEnemy *m, const AosSoma *soma, const AosCollision *layer,
                      const AosEnemyKind *kind, int cam_x, int cam_y, AosHitReport *report) {
    switch (m->step) {
    case 0:
        if (!m->dropped) {
            m->dropped = true;
            if (report->child_count < AOS_ENEMY_MAX_CHILDREN)
                report->children[report->child_count++] = mimic_trigger(m);
        }
        m->hidden = true;
        return true;
    case 1:
        m->step = 2;
        m->hidden = false;
        m->ay = 0x2000;
        /* fall through */
    case 2:
        m->static_frame = 0;
        if (aos_enemy_fall_collide(m, layer, 0x10000,
                                   (int32_t)0xFFF00000, 0x40000) & 4) {
            m->step = 3;
            m->own_boxes = true;
            m->own_hurt = m->own_attack = (AosBox){-8, -14, 16, 14};
            m->combat.attack_off = true;
            m->combat.hurt_off = false;
        }
        return true;
    case 3:
        m->static_frame = 0;
        if (aos_enemy_player_in(soma, (int16_t)((m->x >> 16) - 0x10),
                                (int16_t)((m->y >> 16) - 0x14), 0x20,
                                0x14)) {
            m->static_frame = -1;
            m->own_boxes = false;
            m->combat.attack_off = false;
            aos_enemy_play_loop(m, kind->anims, 0, false);
            m->step = 4;
        }
        return false;
    case 4:
        if (m->cycle_done) {
            aos_enemy_play(m, kind->anims, 1);
            m->step = 5;
        }
        return false;
    case 5:
        m->hidden = aos_enemy_outside_margins(
            kind, 4, (m->x >> 16) - cam_x, (m->y >> 16) - cam_y);
        return false;
    default:
        return false;
    }
}

/* EnemyMimicUpdate: inside the activity window, state 0 acts, 1 runs the
 * generic death, 2 (inert) plays animation 1 once animation 0 ends; then
 * the animation step (a closed chest holds sprite frame 0) and, unless
 * dying or with collisions off, the collision pass. */
AosHitReport aos_enemy_mimic_update(
    AosEnemy *m, AosSoma *soma, const AosCollision *layer,
    const AosEnemyKind *kind, const AosWeaponEntity *weapon,
    const AosWeaponFrames *weapon_frames, int soma_atk, int soma_def,
    int cam_x, int cam_y, uint32_t (*random)(void)) {
    AosHitReport report = {0}, spawned = {0};
    int sx = (m->x >> 16) - cam_x, sy = (m->y >> 16) - cam_y;
    if ((uint16_t)(sx + 0x80) > 0x1F0 || (uint16_t)(sy + 0x40) > 0x120) return report;
    bool quiet = false;
    switch (m->state) {
    case 0: quiet = mimic_act(m, soma, layer, kind, cam_x, cam_y, &spawned); break;
    case 1:
        aos_enemy_generic_death(m, kind, random, &spawned);
        if (m->removed) return spawned;
        break;
    case 2:
        if (m->step == 0 && m->cycle_done) {
            aos_enemy_play(m, kind->anims, 1);
            m->step = 1;
        }
        aos_enemy_step_anim(m, kind->anims);
        return report;
    default:
        break;
    }
    if (m->static_frame < 0) aos_enemy_step_anim(m, kind->anims);
    if (m->state != 1 && !quiet)
        report = aos_enemy_collide(m, soma, kind, weapon, weapon_frames,
                                   soma_atk, soma_def);
    aos_combat_tick(&m->combat);
    report.child_count = spawned.child_count;
    memcpy(report.children, spawned.children, sizeof spawned.children[0] * spawned.child_count);
    aos_enemy_take_effects(&report, &spawned);
    return report;
}
